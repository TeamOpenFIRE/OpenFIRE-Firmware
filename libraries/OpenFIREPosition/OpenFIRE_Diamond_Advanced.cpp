/*!
 * @file OpenFIRE_Diamond_Advanced.cpp
 * @brief Advanced Light Gun library for 4 LED setup (Diamond layout)
 * @n CPP file for Samco Light Gun 4 LED setup
 *
 *
 * Modified from the OpenFIRE-Firmware-ESP32 port, 2026-10-24:
 *  - Ported to the RP2350, with the DFRobot camera values fixed as constants
 *    instead of per-camera profiles.
 *  - Renamed variables and methods, replaced magic numbers with named constants,
 *    and added explanatory comments.
 *
 * Based on the ESP32 port by:
 * @copyright alessandro-satanassi, https://github.com/alessandro-satanassi, 2026
 * @copyright GNU Lesser General Public License
 *
 * @author [Alessandro Satanassi](alessandro@cittini.it)
 * @version V2.0
 * @date 2026
 *
 * Based on the original code:
 *
 * @copyright Samco, https://github.com/samuelballantyne, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @version V1.0
 * @date 2024
 */

#include "OpenFIRETracking.h"

#ifdef ADVANCED_TRACKING

#include <Arduino.h>

#include <cfloat>

#include "OpenFIRE_Diamond_Advanced.h"

namespace {

constexpr uint8_t LED_COUNT = 4;

// The camera runs at 209 Hz; time-based tuning below was done at that rate and is scaled for any other.
constexpr float FRAME_RATE_SCALE = 209.0f / (float)CamFps;

// Frame counters stop at this value instead of wrapping around to 0, which would look like a fresh detection.
constexpr uint8_t MAX_DARK_FRAME_COUNT = 255;

// After this many frames without a detection, the tracker no longer trusts that the gun is still where it
// was, and identifies the LEDs from scratch instead of continuing from the last geometry.
constexpr uint8_t MAX_RESUME_DARK_FRAMES = 8;

// Expected ratio between the horizontal and the vertical diagonal of the diamond. It follows the shape
// of the monitor (wider than tall), and tells the cold start which diagonal is which.
constexpr float EXPECTED_DIAGONAL_RATIO =
    (float)(DiamondDefaultRightX - DiamondDefaultLeftX) / (float)(DiamondDefaultBottomY - DiamondDefaultTopY);

// The expected diagonal ratio only decides the cold start if the diamond is clearly wider or taller than
// it is square. Close to square the two diagonals can't be told apart reliably.
constexpr float WIDE_DIAMOND_RATIO = 1.15f;
constexpr float TALL_DIAMOND_RATIO = 0.87f;

// The measured diagonals must also differ clearly (by 15%, so 1.15^2 on their squared lengths) before
// they are trusted. Perspective can easily make one diagonal look 10% shorter than it is.
constexpr float CLEAR_DIAGONAL_DIFFERENCE_SQUARED = 1.3225f;

// The most the roll implied by continuity may exceed the roll of the cold start rule: 80 degrees in radians.
// A wrong lock is usually off by 90 degrees (a quarter turn), which this catches.
constexpr float MAX_EXTRA_ROLL_RADIANS = 80.0f * 0.0174533f;

// A resume after a short dark needs the closest rotation to be near the last geometry: the summed squared
// distance of the four LEDs must stay under 1/16 of the shortest side squared (a quarter of the side
// per LED on average).
constexpr float RESUME_COST_FRACTION = 0.0625f;

// Smallest squared distance between two LEDs for them to define a scale and rotation reliably, and the
// range of squared scale change that is plausible within a single frame (a factor of 0.5 to 2 in size).
constexpr float MIN_REFERENCE_DISTANCE_SQUARED = 64.0f;
constexpr float MIN_PLAUSIBLE_SCALE_SQUARED = 0.25f;
constexpr float MAX_PLAUSIBLE_SCALE_SQUARED = 4.0f;

// Smallest determinant for the three known LEDs to span a plane: below it they are almost collinear
// and the affine map from the previous geometry can't be solved reliably.
constexpr float MIN_AFFINE_DETERMINANT = 1.0f;

// A rebuilt LED may well lie outside the camera's view, but a coordinate beyond this can only come from
// an almost degenerate previous geometry. It keeps all later arithmetic in a safe numeric range.
constexpr float COORDINATE_LIMIT = 16383.0f;

// How the spring is released (see the spring section of begin()):
// the share of the offset removed per frame is clamped between these, and grows with the movement.
constexpr float MIN_RETURN_RATE = 0.03125f;
constexpr float MAX_RETURN_RATE = 0.5f;
// Scales the filtered movement (in mouse units per frame) before comparing it with the diamond's size.
constexpr float MOTION_GAIN = 32.0f;
// Share of the movement that is consumed from the offset each frame, so that a moving gun hides the correction.
constexpr float MOVEMENT_CONSUMPTION_FACTOR = 0.25f;

// Slot indices wrap around the diamond: after slot 3 comes slot 0 again.
inline uint8_t slotInCycle(uint8_t index) {
    return index & (LED_COUNT - 1);
}

// True if the four points, taken in slot order, form a convex quadrilateral: every turn goes the same way.
// A projected diamond is always convex, so a non-convex result means the LEDs were identified wrongly,
// or reconstructed from a degenerate geometry, and the frame is better thrown away.
bool isConvexCycle(const float* pointsX, const float* pointsY) {
    float firstTurn = 0.0f;
    for (uint8_t current = 0; current < LED_COUNT; current++) {
        const uint8_t next = slotInCycle(current + 1);
        const uint8_t afterNext = slotInCycle(current + 2);

        // The sign of this cross product tells whether the path current -> next -> afterNext turns
        // left or right. A zero means three points on a line, which also isn't a valid diamond.
        const float turn = (pointsX[next] - pointsX[current]) * (pointsY[afterNext] - pointsY[next]) -
                           (pointsY[next] - pointsY[current]) * (pointsX[afterNext] - pointsX[next]);
        if (turn == 0.0f || (current && ((turn > 0.0f) != (firstTurn > 0.0f)))) return false;
        firstTurn = turn;
    }
    return true;
}

}  // namespace

void OpenFIRE_Diamond::configure() {
    publishedX[0] = DiamondDefaultTopX * CamToMouseMult;
    publishedX[1] = DiamondDefaultRightX * CamToMouseMult;
    publishedX[2] = DiamondDefaultBottomX * CamToMouseMult;
    publishedX[3] = DiamondDefaultLeftX * CamToMouseMult;
    publishedY[0] = DiamondDefaultTopY * CamToMouseMult;
    publishedY[1] = DiamondDefaultRightY * CamToMouseMult;
    publishedY[2] = DiamondDefaultBottomY * CamToMouseMult;
    publishedY[3] = DiamondDefaultLeftY * CamToMouseMult;

    centerX = MouseMaxX / 2;
    centerY = MouseMaxY / 2;

    // Forget everything learned from the camera, so the next frames start from scratch.
    trackingStarted = false;
    previousSeenCount = 0;
    darkFrameCount = MAX_DARK_FRAME_COUNT;
    previousMeasuredMask = 0;
    for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
        previousGeometryX[slot] = (float)publishedX[slot];
        previousGeometryY[slot] = (float)publishedY[slot];
        springOffsetX[slot] = springOffsetY[slot] = 0.0f;
        filteredMotionX[slot] = filteredMotionY[slot] = 0.0f;
        seenHistory[slot] = 0;
    }
    springReleasesFast = false;
    rollAngle = 0;
    verticalDiagonalLength = horizontalDiagonalLength = 0;
}

void OpenFIRE_Diamond::begin(const int* cameraXPositions, const int* cameraYPositions, unsigned int ledSeenMask) {
    seenFlags = ledSeenMask;

    // Gather the real detections into compact arrays. The camera reports blobs in arbitrary slots and marks
    // the valid ones in the mask, so from here on the order of seenX/seenY carries no meaning about
    // which LED is which: that is what the rest of the function works out. Coordinates are shifted into
    // mouse space, which has extra precision bits compared to the camera's pixels.
    float seenX[LED_COUNT], seenY[LED_COUNT];
    uint8_t seenCount = 0;
    for (uint8_t blob = 0; blob < LED_COUNT; blob++) {
        if (ledSeenMask & (1u << blob)) {
            // A camera position outside the sensor can only be a bad read, and shifting a negative value would be
            // undefined behaviour: drop the whole frame and keep the last geometry, as the Square tracker does.
            if (cameraXPositions[blob] < 0 || cameraXPositions[blob] > CamMaxX ||
                cameraYPositions[blob] < 0 || cameraYPositions[blob] > CamMaxY)
                return;
            seenX[seenCount] = (float)(cameraXPositions[blob] << CamToMouseShift);
            seenY[seenCount] = (float)(cameraYPositions[blob] << CamToMouseShift);
            seenCount++;
        }
    }

    // Identifying which LED is which from fewer than 4 needs a trustworthy geometry to compare against.
    // That only exists once all 4 have been seen together at least once.
    if (!trackingStarted && seenCount != 4) return;

    // Nothing seen: keep showing the last position, and count the dark frames so that a later
    // detection knows how stale the last geometry is.
    if (seenCount == 0) {
        previousSeenCount = 0;
        if (darkFrameCount < MAX_DARK_FRAME_COUNT) darkFrameCount++;
        return;
    }

    // Coming back from the dark with only 1 or 2 LEDs, there isn't enough to tell which ones they are:
    // the gun may have moved or rolled in the meantime. Wait for at least 3.
    if (previousSeenCount == 0 && seenCount < 3) {
        if (darkFrameCount < MAX_DARK_FRAME_COUNT) darkFrameCount++;
        return;
    }

    // The geometry of this frame, a position for each of the 4 slots. Measured LEDs are copied in; the
    // ones that are hidden are rebuilt. measuredMask records which slots are real measurements.
    float geometryX[LED_COUNT], geometryY[LED_COUNT];
    uint8_t measuredMask = 0;

    if (seenCount == 4) {
        // Sort the four points by their angle around their centre. The angle grows in the same direction
        // as the slot numbers (top -> max X -> bottom -> min X, with Y pointing down), whatever the
        // distance or perspective, so the sorted order is the slot order up to a rotation. Only the
        // starting point is unknown: which of the four is the top one.
        const float centroidX = (seenX[0] + seenX[1] + seenX[2] + seenX[3]) * 0.25f;
        const float centroidY = (seenY[0] + seenY[1] + seenY[2] + seenY[3]) * 0.25f;
        uint8_t seenIndexByAngle[LED_COUNT] = {0, 1, 2, 3};
        float angleAroundCentroid[LED_COUNT];
        for (uint8_t i = 0; i < LED_COUNT; i++)
            angleAroundCentroid[i] = atan2f(seenY[i] - centroidY, seenX[i] - centroidX);

        // Insertion sort: with 4 elements it is smaller and faster than anything fancier.
        for (uint8_t i = 1; i < LED_COUNT; i++) {
            for (uint8_t j = i;
                 j > 0 && angleAroundCentroid[seenIndexByAngle[j]] < angleAroundCentroid[seenIndexByAngle[j - 1]];
                 j--) {
                const uint8_t swapped = seenIndexByAngle[j];
                seenIndexByAngle[j] = seenIndexByAngle[j - 1];
                seenIndexByAngle[j - 1] = swapped;
            }
        }

        // The cold start rule picks the top LED without using any history. The diamond is wider than tall
        // (it follows the monitor), so its longer diagonal is the horizontal one however much the gun is
        // rolled, because rotating doesn't change lengths. The top LED is then the end of the shorter,
        // vertical diagonal that has the lower Y in the image. If the two diagonals are too similar to
        // tell (nearly square diamond, or strong perspective) the LED with the lowest Y is taken instead.
        uint8_t coldStartRotation = 0;
        bool coldStartDecidedByDiagonals = false;
        {
            const float firstDiagonalX = seenX[seenIndexByAngle[0]] - seenX[seenIndexByAngle[2]];
            const float firstDiagonalY = seenY[seenIndexByAngle[0]] - seenY[seenIndexByAngle[2]];
            const float secondDiagonalX = seenX[seenIndexByAngle[1]] - seenX[seenIndexByAngle[3]];
            const float secondDiagonalY = seenY[seenIndexByAngle[1]] - seenY[seenIndexByAngle[3]];
            const float firstDiagonalSquared = firstDiagonalX * firstDiagonalX + firstDiagonalY * firstDiagonalY;
            const float secondDiagonalSquared = secondDiagonalX * secondDiagonalX + secondDiagonalY * secondDiagonalY;

            const bool isWideDiamond = EXPECTED_DIAGONAL_RATIO > WIDE_DIAMOND_RATIO;
            const bool isTallDiamond = EXPECTED_DIAGONAL_RATIO < TALL_DIAMOND_RATIO;
            const bool diagonalsClearlyDiffer =
                firstDiagonalSquared > CLEAR_DIAGONAL_DIFFERENCE_SQUARED * secondDiagonalSquared ||
                secondDiagonalSquared > CLEAR_DIAGONAL_DIFFERENCE_SQUARED * firstDiagonalSquared;

            if ((isWideDiamond || isTallDiamond) && diagonalsClearlyDiffer) {
                // The vertical diagonal is the shorter one of a wide diamond, or the longer one of a tall one.
                const bool firstDiagonalIsVertical = isWideDiamond ? (firstDiagonalSquared < secondDiagonalSquared)
                                                                   : (firstDiagonalSquared > secondDiagonalSquared);
                const uint8_t verticalDiagonalStart = firstDiagonalIsVertical ? 0 : 1;
                // The top end is the one higher up in the image (lower Y).
                coldStartRotation = (seenY[seenIndexByAngle[verticalDiagonalStart]] <=
                                     seenY[seenIndexByAngle[verticalDiagonalStart + 2]])
                                        ? verticalDiagonalStart
                                        : (uint8_t)(verticalDiagonalStart + 2);
                coldStartDecidedByDiagonals = true;
            } else {
                for (uint8_t rotation = 1; rotation < LED_COUNT; rotation++) {
                    if (seenY[seenIndexByAngle[rotation]] < seenY[seenIndexByAngle[coldStartRotation]])
                        coldStartRotation = rotation;
                }
            }
        }

        // Which rotation of the angle-sorted points is the top LED first. Unless the continuity below
        // overrides it, this is the cold start choice.
        uint8_t chosenRotation = coldStartRotation;

        // After a short dark, resume from where the geometry was instead of identifying the LEDs again:
        // if the gun hasn't moved much in the meantime, the closest rotation to the last shown
        // position is still the right one, even if the gun is rolled so far that the cold start
        // rule would pick differently. If the closest rotation is far away the gun has moved, and the
        // cold start rule is the safer choice.
        bool resumeAfterShortDark = false;
        if (previousSeenCount == 0 && trackingStarted && darkFrameCount <= MAX_RESUME_DARK_FRAMES) {
            float bestCost = FLT_MAX;
            uint8_t closestRotation = 0;
            for (uint8_t rotation = 0; rotation < LED_COUNT; rotation++) {
                // Total squared distance between the last shown geometry and this way of assigning slots.
                float cost = 0.0f;
                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    const float deltaX =
                        seenX[seenIndexByAngle[slotInCycle(rotation + slot)]] - (float)publishedX[slot];
                    const float deltaY =
                        seenY[seenIndexByAngle[slotInCycle(rotation + slot)]] - (float)publishedY[slot];
                    cost += deltaX * deltaX + deltaY * deltaY;
                }
                if (cost < bestCost) {
                    bestCost = cost;
                    closestRotation = rotation;
                }
            }

            // How close is close: compare against the shortest side of the diamond, so that the
            // allowed movement scales with the distance from the screen.
            float shortestSideSquared = FLT_MAX;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                const float deltaX = seenX[seenIndexByAngle[slotInCycle(closestRotation + slot)]] -
                                     seenX[seenIndexByAngle[slotInCycle(closestRotation + slot + 1)]];
                const float deltaY = seenY[seenIndexByAngle[slotInCycle(closestRotation + slot)]] -
                                     seenY[seenIndexByAngle[slotInCycle(closestRotation + slot + 1)]];
                shortestSideSquared = fminf(shortestSideSquared, deltaX * deltaX + deltaY * deltaY);
            }
            resumeAfterShortDark = bestCost < shortestSideSquared * RESUME_COST_FRACTION;
        }

        if (previousSeenCount != 0 || resumeAfterShortDark) {
            // Continuity: keep every LED close to where it was last shown. That is what keeps the
            // identities from jumping when the gun is rolled a long way around, which the cold start
            // rule alone can't follow because it only looks at one frame.
            float bestCost = FLT_MAX;
            for (uint8_t rotation = 0; rotation < LED_COUNT; rotation++) {
                float cost = 0.0f;
                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    const float deltaX =
                        seenX[seenIndexByAngle[slotInCycle(rotation + slot)]] - (float)publishedX[slot];
                    const float deltaY =
                        seenY[seenIndexByAngle[slotInCycle(rotation + slot)]] - (float)publishedY[slot];
                    cost += deltaX * deltaX + deltaY * deltaY;
                }
                if (cost < bestCost) {
                    bestCost = cost;
                    chosenRotation = rotation;
                }
            }

            // Continuity can also lock onto a wrong answer, for example if tracking started with the gun
            // strongly rolled and every later frame stays consistent with that mistake. The cold start
            // rule is the safety net: if continuity says the gun is rolled more than 80 degrees
            // beyond what the cold start rule says, trust the cold start rule and recover.
            if (chosenRotation != coldStartRotation) {
                // How far the vertical diagonal (from slot `rotation` to its opposite) is from vertical.
                // With perspective the horizontal diagonal tilts a lot when the gun is far to the side, while
                // the vertical one barely does, so the vertical one is the reliable measure of roll.
                auto apparentRollOf = [&](uint8_t rotation) {
                    const float diagonalX =
                        seenX[seenIndexByAngle[slotInCycle(rotation + 2)]] - seenX[seenIndexByAngle[rotation]];
                    const float diagonalY =
                        seenY[seenIndexByAngle[slotInCycle(rotation + 2)]] - seenY[seenIndexByAngle[rotation]];
                    return fabsf(atan2f(-diagonalX, diagonalY));
                };
                if (apparentRollOf(chosenRotation) >= apparentRollOf(coldStartRotation) + MAX_EXTRA_ROLL_RADIANS)
                    chosenRotation = coldStartRotation;
            }

            // If the diagonals were clearly told apart, a rotation of an odd number of steps away from the
            // cold start answer would have the horizontal and vertical diagonals swapped. That is
            // a lock that is off by 90 degrees, however the gun is tilted, so go back to the cold start answer.
            if (coldStartDecidedByDiagonals && ((chosenRotation ^ coldStartRotation) & 1))
                chosenRotation = coldStartRotation;
        }

        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            geometryX[slot] = seenX[seenIndexByAngle[slotInCycle(chosenRotation + slot)]];
            geometryY[slot] = seenY[seenIndexByAngle[slotInCycle(chosenRotation + slot)]];
        }

        // A real diamond seen by a camera is always convex. If this isn't, the frame is bad data
        // (a reflection taken for an LED, say) and the last good geometry is kept.
        if (!isConvexCycle(geometryX, geometryY)) return;
        measuredMask = 0x0F;
    } else {
        // With 1 to 3 LEDs, first decide which slot each seen point belongs to, and then rebuild
        // the hidden slots from the previous geometry.
        //
        // The assignment is found by trying every way to give each seen point a different slot, and
        // keeping the one that moves the points the least from where those slots were last shown.
        // With 3 points two refinements apply:
        //  - assignments that would mirror the diamond are skipped, since a camera can't produce those;
        //  - right after a dark, the gun may have moved a long way, so only the shape of the three
        //    points (their positions relative to their own centre) is compared, not where they are.
        uint8_t slotOfSeenPoint[3] = {0, 1, 2};
        float bestCost = FLT_MAX;
        const bool compareShapeOnly = (seenCount == 3 && previousSeenCount == 0);
        float seenCentroidX = 0.0f, seenCentroidY = 0.0f;
        if (compareShapeOnly) {
            seenCentroidX = (seenX[0] + seenX[1] + seenX[2]) / 3.0f;
            seenCentroidY = (seenY[0] + seenY[1] + seenY[2]) / 3.0f;
        }

        // The three nested loops try (up to) three slots for the (up to) three seen points. For fewer
        // points the inner loops stop after one pass, so that only the needed slots are tried.
        for (uint8_t firstPointSlot = 0; firstPointSlot < LED_COUNT; firstPointSlot++) {
            for (uint8_t secondPointSlot = 0; secondPointSlot < LED_COUNT; secondPointSlot++) {
                if (seenCount >= 2 && secondPointSlot == firstPointSlot) continue;

                for (uint8_t thirdPointSlot = 0; thirdPointSlot < LED_COUNT; thirdPointSlot++) {
                    if (seenCount >= 3 && (thirdPointSlot == firstPointSlot || thirdPointSlot == secondPointSlot))
                        continue;

                    const uint8_t candidateSlots[3] = {firstPointSlot, secondPointSlot, thirdPointSlot};

                    if (seenCount == 3) {
                        // Order the three points by their slot number. Walking them in that order must
                        // turn the same way as the diamond does when its LEDs are walked in slot order
                        // (the order the angle sort gives with 4 LEDs); the opposite turn would be a mirror image.
                        uint8_t pointsBySlot[3] = {0, 1, 2};
                        for (uint8_t i = 1; i < 3; i++) {
                            for (uint8_t j = i;
                                 j > 0 && candidateSlots[pointsBySlot[j]] < candidateSlots[pointsBySlot[j - 1]]; j--) {
                                const uint8_t swapped = pointsBySlot[j];
                                pointsBySlot[j] = pointsBySlot[j - 1];
                                pointsBySlot[j - 1] = swapped;
                            }
                        }
                        const float turnDirection = (seenX[pointsBySlot[1]] - seenX[pointsBySlot[0]]) *
                                                        (seenY[pointsBySlot[2]] - seenY[pointsBySlot[1]]) -
                                                    (seenY[pointsBySlot[1]] - seenY[pointsBySlot[0]]) *
                                                        (seenX[pointsBySlot[2]] - seenX[pointsBySlot[1]]);
                        if (turnDirection <= 0.0f) continue;
                    }

                    // When only the shape is compared, the candidate slots' last positions are shifted
                    // to their own centre too.
                    float candidateCentroidX = 0.0f, candidateCentroidY = 0.0f;
                    if (compareShapeOnly) {
                        candidateCentroidX = ((float)publishedX[firstPointSlot] + (float)publishedX[secondPointSlot] +
                                              (float)publishedX[thirdPointSlot]) /
                                             3.0f;
                        candidateCentroidY = ((float)publishedY[firstPointSlot] + (float)publishedY[secondPointSlot] +
                                              (float)publishedY[thirdPointSlot]) /
                                             3.0f;
                    }

                    float cost = 0.0f;
                    for (uint8_t point = 0; point < seenCount; point++) {
                        const float deltaX = (seenX[point] - seenCentroidX) -
                                             ((float)publishedX[candidateSlots[point]] - candidateCentroidX);
                        const float deltaY = (seenY[point] - seenCentroidY) -
                                             ((float)publishedY[candidateSlots[point]] - candidateCentroidY);
                        cost += deltaX * deltaX + deltaY * deltaY;
                    }
                    if (cost < bestCost) {
                        bestCost = cost;
                        slotOfSeenPoint[0] = firstPointSlot;
                        slotOfSeenPoint[1] = secondPointSlot;
                        slotOfSeenPoint[2] = thirdPointSlot;
                    }

                    if (seenCount < 3) break;
                }
                if (seenCount < 2) break;
            }
        }

        // No assignment survived: possible only with 3 points on a line, which no diamond produces.
        if (bestCost == FLT_MAX) return;

        // Start from the previous geometry for the hidden slots, and put the measured points in theirs.
        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            geometryX[slot] = previousGeometryX[slot];
            geometryY[slot] = previousGeometryY[slot];
        }
        for (uint8_t point = 0; point < seenCount; point++) {
            geometryX[slotOfSeenPoint[point]] = seenX[point];
            geometryY[slotOfSeenPoint[point]] = seenY[point];
            measuredMask |= 1u << slotOfSeenPoint[point];
        }

        if (seenCount == 3) {
            // One slot is hidden. Express its previous position relative to the other three, as
            // "first known slot + cu * (second - first) + cv * (third - first)", then apply the
            // same coefficients to where those three are now. This is exact for any camera motion that
            // keeps lines straight (perspective included for small changes), unlike assuming
            // the diamond is a parallelogram.
            uint8_t missingSlot = 0;
            while (measuredMask & (1u << missingSlot)) missingSlot++;
            const uint8_t firstKnown = slotInCycle(missingSlot + 1);
            const uint8_t secondKnown = slotInCycle(missingSlot + 2);
            const uint8_t thirdKnown = slotInCycle(missingSlot + 3);

            const float firstEdgeX = previousGeometryX[secondKnown] - previousGeometryX[firstKnown];
            const float firstEdgeY = previousGeometryY[secondKnown] - previousGeometryY[firstKnown];
            const float secondEdgeX = previousGeometryX[thirdKnown] - previousGeometryX[firstKnown];
            const float secondEdgeY = previousGeometryY[thirdKnown] - previousGeometryY[firstKnown];
            const float determinant = firstEdgeX * secondEdgeY - firstEdgeY * secondEdgeX;

            if (fabsf(determinant) > MIN_AFFINE_DETERMINANT) {
                const float missingFromFirstX = previousGeometryX[missingSlot] - previousGeometryX[firstKnown];
                const float missingFromFirstY = previousGeometryY[missingSlot] - previousGeometryY[firstKnown];
                // Solve missingFromFirst = coefficientFirst * firstEdge + coefficientSecond * secondEdge
                // (Cramer's rule, with the determinant computed above).
                const float coefficientFirst =
                    (missingFromFirstX * secondEdgeY - missingFromFirstY * secondEdgeX) / determinant;
                const float coefficientSecond =
                    (firstEdgeX * missingFromFirstY - firstEdgeY * missingFromFirstX) / determinant;
                geometryX[missingSlot] = geometryX[firstKnown] +
                                         coefficientFirst * (geometryX[secondKnown] - geometryX[firstKnown]) +
                                         coefficientSecond * (geometryX[thirdKnown] - geometryX[firstKnown]);
                geometryY[missingSlot] = geometryY[firstKnown] +
                                         coefficientFirst * (geometryY[secondKnown] - geometryY[firstKnown]) +
                                         coefficientSecond * (geometryY[thirdKnown] - geometryY[firstKnown]);
            } else {
                // The previous geometry was degenerate, so the known slots can't span a plane. Complete
                // the shape as a parallelogram, the best guess left.
                geometryX[missingSlot] = geometryX[firstKnown] + geometryX[thirdKnown] - geometryX[secondKnown];
                geometryY[missingSlot] = geometryY[firstKnown] + geometryY[thirdKnown] - geometryY[secondKnown];
            }
        } else if (seenCount == 2) {
            // Two slots are hidden. Find the rotation, uniform scale and translation that takes the two
            // measured slots from their previous positions to their new ones, and apply it to the hidden
            // ones too. Rotation and scale are handled as a complex number: new edge / old edge.
            const uint8_t firstSlot = slotOfSeenPoint[0];
            const uint8_t secondSlot = slotOfSeenPoint[1];
            const float previousEdgeX = previousGeometryX[secondSlot] - previousGeometryX[firstSlot];
            const float previousEdgeY = previousGeometryY[secondSlot] - previousGeometryY[firstSlot];
            const float newEdgeX = geometryX[secondSlot] - geometryX[firstSlot];
            const float newEdgeY = geometryY[secondSlot] - geometryY[firstSlot];
            const float previousEdgeSquared = previousEdgeX * previousEdgeX + previousEdgeY * previousEdgeY;

            // Two LEDs this close couldn't define a stable rotation and scale.
            if (previousEdgeSquared < MIN_REFERENCE_DISTANCE_SQUARED) return;

            // scaleCosine and scaleSine are the real and imaginary parts of (new edge / previous edge),
            // so the transformation multiplies by (scaleCosine + i * scaleSine) around the first slot.
            const float scaleCosine = (newEdgeX * previousEdgeX + newEdgeY * previousEdgeY) / previousEdgeSquared;
            const float scaleSine = (previousEdgeX * newEdgeY - previousEdgeY * newEdgeX) / previousEdgeSquared;
            const float scaleSquared = scaleCosine * scaleCosine + scaleSine * scaleSine;

            // The distance between the LEDs can't plausibly halve or double within one frame.
            // A change that big means the two points are not the pair they were taken for.
            if (scaleSquared < MIN_PLAUSIBLE_SCALE_SQUARED || scaleSquared > MAX_PLAUSIBLE_SCALE_SQUARED) return;

            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                if (slot == firstSlot || slot == secondSlot) continue;
                const float fromFirstX = previousGeometryX[slot] - previousGeometryX[firstSlot];
                const float fromFirstY = previousGeometryY[slot] - previousGeometryY[firstSlot];
                geometryX[slot] = geometryX[firstSlot] + scaleCosine * fromFirstX - scaleSine * fromFirstY;
                geometryY[slot] = geometryY[firstSlot] + scaleSine * fromFirstX + scaleCosine * fromFirstY;
            }
        } else {
            // One LED is seen: the best assumption is that the whole diamond translated with it, and
            // neither rolled nor changed size, since there is nothing to measure those from.
            const uint8_t seenSlot = slotOfSeenPoint[0];
            const float translationX = geometryX[seenSlot] - previousGeometryX[seenSlot];
            const float translationY = geometryY[seenSlot] - previousGeometryY[seenSlot];
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                if (slot == seenSlot) continue;
                geometryX[slot] = previousGeometryX[slot] + translationX;
                geometryY[slot] = previousGeometryY[slot] + translationY;
            }
        }

        // As with 4 LEDs, a result that isn't a convex diamond means something went wrong: keep
        // the last good geometry.
        if (!isConvexCycle(geometryX, geometryY)) return;

        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            if (!(fabsf(geometryX[slot]) <= COORDINATE_LIMIT && fabsf(geometryY[slot]) <= COORDINATE_LIMIT)) return;
        }
    }

    // Spring: when the set of measured LEDs changes, the rebuilt geometry rarely agrees with what
    // was being shown, and showing it at once would make the cursor jump. Instead, the shown geometry
    // keeps following the movement of the LEDs that stayed measured, and the difference to the rebuilt
    // geometry (the offset) is removed gradually over the next frames.
    if (trackingStarted && previousSeenCount > 0) {
        // Average movement of the slots that were measured both in this frame and in the previous one.
        // Only measured slots count: rebuilt ones just repeat the earlier movement.
        const uint8_t commonMask = measuredMask & previousMeasuredMask;
        float meanMovementX = 0.0f, meanMovementY = 0.0f;
        uint8_t commonCount = 0;
        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            if (commonMask & (1u << slot)) {
                meanMovementX += geometryX[slot] - previousGeometryX[slot];
                meanMovementY += geometryY[slot] - previousGeometryY[slot];
                commonCount++;
            }
        }
        if (commonCount) {
            meanMovementX /= commonCount;
            meanMovementY /= commonCount;
        }

        // The set of measured LEDs changed. Take the geometry that was shown, moved by the current
        // movement, as the position to continue from, and keep the difference to the new geometry
        // as the offset that will be released over time.
        if (measuredMask != previousMeasuredMask) {
            springReleasesFast = seenCount > previousSeenCount;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                springOffsetX[slot] = (float)publishedX[slot] + meanMovementX - geometryX[slot];
                springOffsetY[slot] = (float)publishedY[slot] + meanMovementY - geometryY[slot];
            }
        }

        float largestOffset = 0.0f;
        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            largestOffset = fmaxf(largestOffset, fabsf(springOffsetX[slot]));
            largestOffset = fmaxf(largestOffset, fabsf(springOffsetY[slot]));
        }

        if (largestOffset > 0.0f) {
            // Filtered movement of each slot, as a running average of its signed frame-to-frame step.
            // Averaging the signed values before taking magnitudes cancels jitter (which changes
            // direction) while keeping real motion such as rotation or zoom. It restarts from zero when
            // the number of measured LEDs changes, so that old speed doesn't release a new offset too fast.
            float motionSum = 0.0f;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                if (previousSeenCount != seenCount) filteredMotionX[slot] = filteredMotionY[slot] = 0.0f;
                if (commonMask & (1u << slot)) {
                    filteredMotionX[slot] = (filteredMotionX[slot] + geometryX[slot] - previousGeometryX[slot]) * 0.5f;
                    filteredMotionY[slot] = (filteredMotionY[slot] + geometryY[slot] - previousGeometryY[slot]) * 0.5f;
                    motionSum += fabsf(filteredMotionX[slot]) + fabsf(filteredMotionY[slot]);
                }
            }
            const float movement = commonCount ? motionSum / (float)commonCount : 0.0f;

            // How much of the offset to remove this frame. The idea: while the gun moves, the player can't
            // see a correction, so it is released quickly; while it is still, any visible shift would
            // be noticed, so it is released slowly.
            //  - With 4 LEDs a share of the offset is removed per frame, and that share grows with the
            //    movement, between a slow minimum and a fast maximum.
            //  - With 1 to 3 LEDs only a small constant amount (plus a share of the movement) is removed
            //    per frame, so a large offset lasts long: the shown position shouldn't quickly follow a
            //    geometry that is only an estimate.
            //  - Right after LEDs became measured again (fewer than 4 though), the offset is released as a share
            //    at the minimum rate, since the old geometry was the estimate there and the new one is real.
            // Movement is compared against the diamond's vertical size, so this works at any distance.
            const float verticalDiagonalX = geometryX[2] - geometryX[0];
            const float verticalDiagonalY = geometryY[2] - geometryY[0];
            const float normalizedDiagonalLength =
                fmaxf(sqrtf(verticalDiagonalX * verticalDiagonalX + verticalDiagonalY * verticalDiagonalY), 1.0f) *
                FRAME_RATE_SCALE;
            const float scaledMotion = MOTION_GAIN * movement;

            float returnRate;
            if (seenCount != 4 || scaledMotion <= normalizedDiagonalLength * MIN_RETURN_RATE)
                returnRate = MIN_RETURN_RATE;
            else if (scaledMotion >= normalizedDiagonalLength * MAX_RETURN_RATE)
                returnRate = MAX_RETURN_RATE;
            else
                returnRate = scaledMotion / normalizedDiagonalLength;

            // The amount removed from the largest offset this frame: at least a small constant, at least
            // a share of the movement, and (with 4 LEDs or after LEDs came back) the proportional release above.
            const float consumption =
                fmaxf(0.5f * FRAME_RATE_SCALE,
                      fmaxf(movement * MOVEMENT_CONSUMPTION_FACTOR,
                            (seenCount == 4 || springReleasesFast) ? largestOffset * returnRate : 0.0f));

            // All offsets shrink by the same factor, not by the same amount, so the shape of the
            // correction is preserved. Subtracting a fixed amount from each would zero the small ones
            // first and deform the diamond while it settles.
            const float shrinkFactor = largestOffset > consumption ? 1.0f - consumption / largestOffset : 0.0f;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                springOffsetX[slot] *= shrinkFactor;
                springOffsetY[slot] *= shrinkFactor;
            }
        }
    } else {
        // Nothing to compare with (first frame, or the detection just came back from the dark): show
        // the geometry as it is.
        for (uint8_t slot = 0; slot < LED_COUNT; slot++) springOffsetX[slot] = springOffsetY[slot] = 0.0f;
    }

    // Publish the geometry with the remaining spring offset applied, and keep the offset-free geometry as
    // the starting point for the next frame.
    for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
        publishedX[slot] = (int)lroundf(geometryX[slot] + springOffsetX[slot]);
        publishedY[slot] = (int)lroundf(geometryY[slot] + springOffsetY[slot]);
        previousGeometryX[slot] = geometryX[slot];
        previousGeometryY[slot] = geometryY[slot];
        seenHistory[slot] = (seenHistory[slot] << 1) | ((measuredMask >> slot) & 1);
    }
    previousMeasuredMask = measuredMask;
    previousSeenCount = seenCount;
    darkFrameCount = 0;
    if (seenCount == 4) trackingStarted = true;

    centerX = (publishedX[0] + publishedX[1] + publishedX[2] + publishedX[3]) / 4;
    centerY = (publishedY[0] + publishedY[1] + publishedY[2] + publishedY[3]) / 4;
    verticalDiagonalLength = (float)(publishedY[2] - publishedY[0]);
    horizontalDiagonalLength = (float)(publishedX[1] - publishedX[3]);

    // Roll is the angle of the horizontal diagonal (right LED relative to the left LED). This is the same
    // quantity the classic tracker reports, and calibration uses it to rotate the aim point back upright.
    rollAngle = atan2f((float)(publishedY[1] - publishedY[3]), (float)(publishedX[1] - publishedX[3]));
}

#endif  // ADVANCED_TRACKING
