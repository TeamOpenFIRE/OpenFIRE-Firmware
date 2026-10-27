/*!
 * @file OpenFIRE_Square_Advanced.cpp
 * @brief Advanced Light Gun library for 4 LED setup (Square layout)
 * @n CPP file for Samco Light Gun 4 LED setup
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
#include "OpenFIRE_Square_Advanced.h"

namespace {

// Slot indices, matching the layout drawn in the header.
constexpr uint8_t TOP_LEFT = 0;
constexpr uint8_t TOP_RIGHT = 1;
constexpr uint8_t BOTTOM_LEFT = 2;
constexpr uint8_t BOTTOM_RIGHT = 3;
constexpr uint8_t LED_COUNT = 4;

// The camera runs at 209 Hz; time-based tuning below was done at that rate and is scaled for any other.
constexpr float FRAME_RATE_SCALE = 209.0f / (float)CamFps;

// Frame counters stop at this value instead of wrapping around to 0, which would look like a fresh detection.
constexpr uint8_t MAX_FRAME_COUNT = 255;

// After this many frames without a detection of 3-4 LEDs, the tracker no longer trusts that the gun is still
// where it was, and identifies the LEDs from scratch instead of continuing from the last geometry.
constexpr uint8_t MAX_RESUME_DARK_FRAMES = 8;

// A lock made with only 3 LEDs counts as uncertain for this many frames after a (re)start, about the first
// second at 209 Hz. During that time a wrong lock may still be corrected by the shape and roll checks.
constexpr uint8_t UNCERTAIN_LOCK_FRAMES = 200;

// Nominal focal length of the DFRobot lens in mouse units: (MouseResX / 2) / tan(33 deg / 2),
// 33 deg being the nominal horizontal field of view. An approximate value is enough: it only
// shapes the perspective of the rectangle estimates (see trueAspectScoreOf, reconstructFromAnchor).
constexpr float FOCAL_LENGTH = MouseResX * 0.5f * (1.0f / 0.29621f);

// Centre of the image. Perspective calculations are done relative to it, because the lens axis passes through it.
constexpr float IMAGE_CENTER_X = MouseResX * 0.5f;
constexpr float IMAGE_CENTER_Y = MouseResY * 0.5f;

// Numerical limit, not the camera field of view: predicted LEDs may be off-screen.
// With |coordinate| <= 16383, every squared 2-D distance used by the ordering
// fits int32_t: 2 * (2 * 16383)^2 < INT32_MAX. Nonnegative costs use uint32_t
// with bounded additions; no 64-bit arithmetic is needed.
constexpr int COORDINATE_LIMIT = 16383;
// The largest value in the 32-bit arithmetic is a difference (or sum) of two products of coordinate differences, each
// of which spans up to 2 * COORDINATE_LIMIT. It must fit int32_t, and does with little margin (2,147,221,512).
static_assert(2LL * (2LL * COORDINATE_LIMIT) * (2LL * COORDINATE_LIMIT) <= INT32_MAX,
              "COORDINATE_LIMIT too large for the 32-bit cross products and squared distances");

// Largest distance whose square still fits int32_t (floor of the square root of INT32_MAX).
constexpr int MAX_SQUARABLE_DISTANCE = 46340;
static_assert((long long)MAX_SQUARABLE_DISTANCE * MAX_SQUARABLE_DISTANCE <= INT32_MAX,
              "MAX_SQUARABLE_DISTANCE squared must fit int32_t");

// Cost added to a pairing of 2 visible LEDs with slots that weren't measured in the previous frame. It is a
// wall against jumpy labelling: it is far larger than any plausible distance, so the pairing of the previous
// frame wins unless the geometry strongly disagrees with it.
constexpr int32_t HISTORY_PENALTY = 1000000;

// Size of the "level" zone used to decide whether the gun is upright (see the labelling in begin()): two points
// count as level if they are within 5/128 of the image size of each other (30 pixels of 768, tested on the
// DFRobot). The zone is also limited to 1/8 of the rectangle's own extent, so that distant, small
// rectangles still get a proportionate zone.
constexpr int ZONE_NUMERATOR = 5;
constexpr int ZONE_DENOMINATOR = 128;
constexpr int ZONE_EXTENT_DIVISOR = 8;

// The most the roll implied by continuity may exceed the roll of the heuristic: 80 degrees in radians.
// A wrong lock is usually off by a quarter turn or more, which this lets the shape checks correct.
constexpr float MAX_EXTRA_ROLL_RADIANS = 80.0f * 0.0174533f;

// A resume after a short dark needs the closest rotation to be near the last geometry: the summed squared
// distance of the four LEDs must stay under 1/16 of the shortest side squared.
constexpr float RESUME_COST_FRACTION = 0.0625f;

// Aspect scores range from -1 to 1. Beyond this magnitude the shape counts as clear evidence for or against
// a labelling; closer to 0 the rectangle looks too square to tell.
constexpr float CLEAR_ASPECT_SCORE = 0.1f;

// Continuity overrides the heuristic only if its total squared distance to the previous geometry is below 1/4
// of the heuristic's (clearly better, not merely better).
constexpr uint32_t CLEARLY_BETTER_COST_DIVISOR = 4;

// How the spring is released: the share of the offset removed per frame is clamped between these, and
// grows with the movement.
constexpr float MIN_RETURN_RATE = 0.03125f;
constexpr float MAX_RETURN_RATE = 0.5f;
// Scales the filtered movement (in mouse units per frame) before comparing it with the rectangle's size.
constexpr float MOTION_GAIN = 32.0f;
// Share of the movement that is consumed from the offset each frame, so that a moving gun hides the correction.
constexpr float MOVEMENT_CONSUMPTION_FACTOR = 0.25f;

// Slot indices wrap around the rectangle's perimeter: after the last comes the first again.
inline uint8_t slotInCycle(uint8_t index)
{
    return index & (LED_COUNT - 1);
}

// Bit of a slot in the measured masks. The top left is the highest of the four bits (0b1000) and the bottom
// right the lowest (0b0001), so that a mask reads like the layout drawn in the header.
inline uint8_t slotMaskBit(uint8_t slot)
{
    return 1 << (3 - slot);
}

// Rounds a float to the nearest integer, without the call overhead of round().
inline int fastRound(float value)
{
    return (int)(value + (value > 0.0f ? 0.5f : -0.5f));
}

// Fills `order` with the indices of the four values, from the smallest value to the largest.
// A fixed sorting network of 5 compare-and-swaps is the fastest way to order 4 elements; it has none of the
// branching of a general sorting loop, which matters as this runs on every camera frame.
void sortIndicesAscending(const int* values, uint8_t* order)
{
    order[0] = 0;
    order[1] = 1;
    order[2] = 2;
    order[3] = 3;

    auto orderPair = [&](uint8_t first, uint8_t second) {
        if (values[order[first]] > values[order[second]]) {
            const uint8_t swapped = order[first];
            order[first] = order[second];
            order[second] = swapped;
        }
    };

    orderPair(0, 1);
    orderPair(2, 3);
    orderPair(0, 2);
    orderPair(1, 3);
    orderPair(1, 2);
}

bool areCoordinatesValid(const int* x, const int* y)
{
    for (uint8_t slot = 0; slot < LED_COUNT; ++slot) {
        if (x[slot] < -COORDINATE_LIMIT || x[slot] > COORDINATE_LIMIT ||
            y[slot] < -COORDINATE_LIMIT || y[slot] > COORDINATE_LIMIT)
            return false;
    }
    return true;
}

// A projected rectangle must remain strictly convex. Also rejects duplicate
// vertices and any three collinear vertices, without imposing a minimum angle
// or aspect ratio that would unnecessarily limit oblique views.
bool isGeometryValid(const int* x, const int* y)
{
    if (!areCoordinatesValid(x, y))
        return false;

    // Walk the perimeter in order (top left, top right, bottom right, bottom left).
    const uint8_t perimeter[4] = {TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT};
    int32_t firstTurn = 0;
    for (uint8_t i = 0; i < LED_COUNT; ++i) {
        const uint8_t current = perimeter[i];
        const uint8_t next = perimeter[slotInCycle(i + 1)];
        const uint8_t afterNext = perimeter[slotInCycle(i + 2)];
        // The sign of this cross product tells whether the path current -> next -> afterNext turns left or right.
        // In a convex shape every turn goes the same way; zero means three points on a line.
        const int32_t turn = (x[next] - x[current]) * (y[afterNext] - y[next]) -
                             (y[next] - y[current]) * (x[afterNext] - x[next]);
        if (turn == 0 || (i != 0 && (turn > 0) != (firstTurn > 0)))
            return false;
        firstTurn = turn;
    }
    return true;
}

// A virtual camera used to rebuild hidden LEDs from the last geometry seen with all 4.
//
// Perspective deforms the rectangle, so comparing two views of it directly doesn't work well. The trick: look
// at each view from a virtual camera that is turned (by the smallest possible rotation) to face the mean
// direction of the LEDs visible in that view. If the gun only rotated between the two views, the
// virtual views then differ by a plain 2D transformation, and the reconstruction is exact.
// A rough focal length is enough; with a focal length of 0 it works directly on the image.
struct VirtualView {
    float focal;
    float centerX, centerY;                          // image centre, the point the lens axis passes through
    float rotation[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // rotation of the virtual camera, row by row

    VirtualView(float lensFocal, float originX, float originY)
        : focal(lensFocal), centerX(originX), centerY(originY) {}

    // Turns the virtual camera towards the mean direction of the given image points.
    // Returns false if that direction is not in front of the camera.
    bool aimAt(const float* pointsX, const float* pointsY, uint8_t pointCount) {
        if (focal <= 0.0f) return true;

        // Each image point corresponds to a direction in 3D: (x, y, focal) from the lens. Adding up the
        // unit vectors of those directions gives their mean direction.
        float meanX = 0.0f, meanY = 0.0f, meanZ = 0.0f;
        for (uint8_t i = 0; i < pointCount; ++i) {
            const float offsetX = pointsX[i] - centerX, offsetY = pointsY[i] - centerY;
            const float inverseLength = 1.0f / sqrtf(offsetX * offsetX + offsetY * offsetY + focal * focal);
            meanX += offsetX * inverseLength;
            meanY += offsetY * inverseLength;
            meanZ += focal * inverseLength;
        }
        const float inverseMeanLength = 1.0f / sqrtf(meanX * meanX + meanY * meanY + meanZ * meanZ);
        meanX *= inverseMeanLength;
        meanY *= inverseMeanLength;
        meanZ *= inverseMeanLength;
        if (!(meanZ > 0.0f)) return false;

        // The smallest rotation that takes the mean direction to the lens axis (0, 0, 1) turns around
        // the axis (meanY, -meanX, 0), which is perpendicular to both. This is Rodrigues' rotation formula
        // for that axis, written out with the cosine and sine of the angle already known from meanZ.
        const float axisX = meanY, axisY = -meanX, oneOverOnePlusCosine = 1.0f / (1.0f + meanZ);
        rotation[0] = 1.0f - axisY * axisY * oneOverOnePlusCosine;
        rotation[1] = axisX * axisY * oneOverOnePlusCosine;
        rotation[2] = axisY;
        rotation[3] = axisX * axisY * oneOverOnePlusCosine;
        rotation[4] = 1.0f - axisX * axisX * oneOverOnePlusCosine;
        rotation[5] = -axisX;
        rotation[6] = -axisY;
        rotation[7] = axisX;
        rotation[8] = 1.0f - (axisX * axisX + axisY * axisY) * oneOverOnePlusCosine;
        return true;
    }

    // Maps an image point onto the virtual camera's image plane (in mouse units). Returns false if the
    // point would end up at or behind the virtual camera, where the projection is meaningless.
    bool toVirtualPlane(float x, float y, float& virtualX, float& virtualY) const {
        const float offsetX = x - centerX, offsetY = y - centerY;
        if (focal <= 0.0f) {
            virtualX = offsetX;
            virtualY = offsetY;
            return true;
        }
        const float depth = rotation[6] * offsetX + rotation[7] * offsetY + rotation[8] * focal;
        if (!(depth > 1e-3f * focal)) return false;
        const float projection = focal / depth;
        virtualX = (rotation[0] * offsetX + rotation[1] * offsetY + rotation[2] * focal) * projection;
        virtualY = (rotation[3] * offsetX + rotation[4] * offsetY + rotation[5] * focal) * projection;
        return true;
    }

    // The inverse of toVirtualPlane(): from the virtual image plane back to the real image.
    bool fromVirtualPlane(float virtualX, float virtualY, float& x, float& y) const {
        if (focal <= 0.0f) {
            x = virtualX + centerX;
            y = virtualY + centerY;
            return true;
        }
        const float depth = rotation[2] * virtualX + rotation[5] * virtualY + rotation[8] * focal;
        if (!(depth > 1e-3f * focal)) return false;
        const float projection = focal / depth;
        x = centerX + (rotation[0] * virtualX + rotation[3] * virtualY + rotation[6] * focal) * projection;
        y = centerY + (rotation[1] * virtualX + rotation[4] * virtualY + rotation[7] * focal) * projection;
        return true;
    }
};

// Rebuilds hidden LEDs from the last geometry seen with all 4 LEDs, which keeps the real shape
// (perspective included).
//
// The measured LEDs were at the `anchor` positions in that geometry and are at the `current` positions now. A
// transformation that takes the first to the second is worked out and applied to the hidden LEDs:
//  - with 2 measured LEDs it is a similarity (rotation, uniform scale and translation), which is all that two
//    points determine;
//  - with 3 measured LEDs it is an affine transformation, which three points determine exactly.
// Both are worked out on the virtual views described above, so that perspective doesn't spoil them.
//
// @param measuredCount     Number of measured LEDs: 2 or 3
// @param anchorX,anchorY   Where the measured LEDs were in the anchor geometry
// @param currentX,currentY Where the measured LEDs are now
// @param hiddenCount       Number of hidden LEDs to rebuild
// @param hiddenX,hiddenY   In: where the hidden LEDs were in the anchor geometry. Out: where they are now
//                          (only valid if this returns true)
// @return false if no reliable transformation could be found, or a rebuilt LED is outside the numerical limits
bool reconstructFromAnchor(uint8_t measuredCount,
                           const float* anchorX, const float* anchorY, const float* currentX, const float* currentY,
                           uint8_t hiddenCount, float* hiddenX, float* hiddenY)
{
    VirtualView anchorView(FOCAL_LENGTH, IMAGE_CENTER_X, IMAGE_CENTER_Y);
    VirtualView currentView(FOCAL_LENGTH, IMAGE_CENTER_X, IMAGE_CENTER_Y);
    if (!anchorView.aimAt(anchorX, anchorY, measuredCount) || !currentView.aimAt(currentX, currentY, measuredCount))
        return false;

    // Zeroed only to keep the compiler from warning: with 2 measured LEDs the third entries are never read.
    float anchorVirtualX[3] = {}, anchorVirtualY[3] = {}, currentVirtualX[3] = {}, currentVirtualY[3] = {};
    for (uint8_t i = 0; i < measuredCount; ++i) {
        if (!anchorView.toVirtualPlane(anchorX[i], anchorY[i], anchorVirtualX[i], anchorVirtualY[i]) ||
            !currentView.toVirtualPlane(currentX[i], currentY[i], currentVirtualX[i], currentVirtualY[i]))
            return false;
    }

    // Two basis vectors from the first measured LED, in the anchor view and in the current view. Any point can be
    // written as the first LED plus a combination of the two vectors, and the same combination in the current
    // view gives where that point is now. With 2 measured LEDs there's only one real vector; the second is the
    // first rotated by 90 degrees, which makes the transformation a similarity (scale and rotation only).
    const float anchorFirstX = anchorVirtualX[1] - anchorVirtualX[0], anchorFirstY = anchorVirtualY[1] - anchorVirtualY[0];
    const float currentFirstX = currentVirtualX[1] - currentVirtualX[0], currentFirstY = currentVirtualY[1] - currentVirtualY[0];
    const float anchorSecondX = measuredCount == 2 ? -anchorFirstY : anchorVirtualX[2] - anchorVirtualX[0];
    const float anchorSecondY = measuredCount == 2 ? anchorFirstX : anchorVirtualY[2] - anchorVirtualY[0];
    const float currentSecondX = measuredCount == 2 ? -currentFirstY : currentVirtualX[2] - currentVirtualX[0];
    const float currentSecondY = measuredCount == 2 ? currentFirstX : currentVirtualY[2] - currentVirtualY[0];

    // The basis must not be degenerate (the vectors almost parallel, or too short), or the combination
    // cannot be solved reliably.
    const float determinant = anchorFirstX * anchorSecondY - anchorFirstY * anchorSecondX;
    if (!(fabsf(determinant) >= 4.0f)) return false;
    const float inverseDeterminant = 1.0f / determinant;

    if (measuredCount == 3) {
        // The linear map from the anchor basis to the current one is the 2x2 matrix [[p, q], [r, t]]. It
        // can be split into a part that is scale and rotation (which real movement of the gun causes),
        // and a shear part (which it doesn't). A strong shear means the LEDs were given wrong labels.
        const float p = (currentFirstX * anchorSecondY - currentSecondX * anchorFirstY) * inverseDeterminant;
        const float q = (currentSecondX * anchorFirstX - currentFirstX * anchorSecondX) * inverseDeterminant;
        const float r = (currentFirstY * anchorSecondY - currentSecondY * anchorFirstY) * inverseDeterminant;
        const float t = (currentSecondY * anchorFirstX - currentFirstY * anchorSecondX) * inverseDeterminant;
        const float scaleRotationPart = (p + t) * (p + t) + (r - q) * (r - q);
        const float shearPart = (p - t) * (p - t) + (q + r) * (q + r);
        // The shear may be up to 1/4 of the scale-and-rotation part (as amplitudes: 1/16 as squares).
        if (!(shearPart <= 0.0625f * scaleRotationPart)) return false;
    }

    for (uint8_t i = 0; i < hiddenCount; ++i) {
        float virtualX, virtualY;
        if (!anchorView.toVirtualPlane(hiddenX[i], hiddenY[i], virtualX, virtualY)) return false;

        // Write the hidden LED as the first measured LED plus coefficientFirst * firstVector + coefficientSecond *
        // secondVector in the anchor view (solving the 2x2 system), and apply those coefficients to the
        // current view's vectors.
        const float fromFirstX = virtualX - anchorVirtualX[0], fromFirstY = virtualY - anchorVirtualY[0];
        const float coefficientFirst = (fromFirstX * anchorSecondY - fromFirstY * anchorSecondX) * inverseDeterminant;
        const float coefficientSecond = (anchorFirstX * fromFirstY - anchorFirstY * fromFirstX) * inverseDeterminant;
        if (!currentView.fromVirtualPlane(currentVirtualX[0] + coefficientFirst * currentFirstX + coefficientSecond * currentSecondX,
                                          currentVirtualY[0] + coefficientFirst * currentFirstY + coefficientSecond * currentSecondY,
                                          hiddenX[i], hiddenY[i]))
            return false;
        if (!(fabsf(hiddenX[i]) <= COORDINATE_LIMIT && fabsf(hiddenY[i]) <= COORDINATE_LIMIT)) return false;
    }
    return true;
}

} // namespace

void OpenFIRE_Square::resetTracking()
{
    trackingStarted = false;
    trackingStable = false;
    previousSeenCount = 0;
    previousMeasuredMask = 0;
    for (uint8_t slot = 0; slot < LED_COUNT; ++slot) {
        filteredMotionX[slot] = filteredMotionY[slot] = 0;
        springOffsetX[slot] = springOffsetY[slot] = 0.0f;
        seenHistory[slot] = 0;
    }
    springReleasesFast = false;
    // After a reset there must be no resume from a short dark with the labels of the previous layout.
    darkFrameCount = MAX_FRAME_COUNT;
    anchorAvailable = false;
}

void OpenFIRE_Square::configure()
{
    // Clean restart (e.g. after saving, which restarts the camera): the history of the previous
    // tracking must not survive the new default geometry.
    resetTracking();

    publishedX[TOP_LEFT] = publishedX[BOTTOM_LEFT] = SquareDefaultTLX * CamToMouseMult;
    publishedX[TOP_RIGHT] = publishedX[BOTTOM_RIGHT] = SquareDefaultTRX * CamToMouseMult;
    publishedY[TOP_LEFT] = publishedY[TOP_RIGHT] = SquareDefaultTY * CamToMouseMult;
    publishedY[BOTTOM_LEFT] = publishedY[BOTTOM_RIGHT] = SquareDefaultBY * CamToMouseMult;

    centerX = MouseMaxX / 2;
    centerY = MouseMaxY / 2;
    centerBeforePreviousX = centerX;
    centerBeforePreviousY = centerY;
    rectangleHeight = (float)(publishedY[BOTTOM_LEFT] - publishedY[TOP_LEFT]);
    rectangleWidth = (float)(publishedX[TOP_RIGHT] - publishedX[TOP_LEFT]);
    idealAspectRatio = IRSquareBaseHeightRatio;
}

void OpenFIRE_Square::begin(const int* cameraXPositions, const int* cameraYPositions, unsigned int ledSeenMask)
{
    seenFlags = ledSeenMask;

    // All 4 LEDs must be seen together once to start. That way the first geometry comes from real data
    // (idealAspectRatio included), and not from the default one.
    if (seenFlags != 0x0F && !trackingStarted)
        return;

    // Working copy of the points, in mouse space. The measured ones come first; a reconstruction below
    // fills in the hidden ones so that all 4 are always present for the labelling.
    uint8_t seenCount = 0;
    int pointX[LED_COUNT];
    int pointY[LED_COUNT];

    // The measured positions, untouched by any later prediction. At the end they are compared with the
    // final labelled points to tell which of those were really measured and which were invented.
    int measuredX[LED_COUNT];
    int measuredY[LED_COUNT];

    for (uint8_t blob = 0; blob < LED_COUNT; ++blob) {
        if ((seenFlags >> blob) & 0x01) {
            // Validate the camera coordinates before they are shifted: shifting a negative number or one that
            // is too large would be undefined behaviour.
            if (cameraXPositions[blob] < 0 || cameraXPositions[blob] > CamMaxX ||
                cameraYPositions[blob] < 0 || cameraYPositions[blob] > CamMaxY) {
                return;
            }
            // The camera sees the scene mirrored, so X is flipped here to match what the player sees. The shift
            // moves the camera's pixels into mouse space, which has extra precision bits.
            const int mirroredX = (CamMaxX - cameraXPositions[blob]) << CamToMouseShift;
            const int shiftedY = cameraYPositions[blob] << CamToMouseShift;
            if (mirroredX > COORDINATE_LIMIT || shiftedY > COORDINATE_LIMIT) {
                return;
            }

            pointX[seenCount] = mirroredX;
            pointY[seenCount] = shiftedY;
            measuredX[seenCount] = mirroredX;
            measuredY[seenCount] = shiftedY;

            seenCount++;
        }
    }

    if (seenCount >= 1) {
        // Safety gate: after the tracker lost its footing (total or partial blindness), don't guess the
        // position from fewer than 3 points, or the aim would jump wildly.
        if (!trackingStable && seenCount < 3) {
            previousSeenCount = 0;
            if (darkFrameCount < MAX_FRAME_COUNT) darkFrameCount++;
            return;
        }

        int32_t deltaX, deltaY;

        if (seenCount == 1) {
            // With a single LED nothing can be measured about the rectangle's roll or size. By pure inertia
            // the gun is assumed to have moved without rolling, and the whole rectangle is moved with the LED.
            if (previousSeenCount >= 1) {
                const int seenPointX = pointX[0];
                const int seenPointY = pointY[0];

                // Which of the 4 corners is this LED? The nearest one to where the corners were shown.
                // Squared distances are enough to compare, and spare a square root.
                uint8_t nearestSlot = 0;
                int32_t minDistanceSquared = INT32_MAX;

                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    const int slotDeltaX = seenPointX - publishedX[slot];
                    const int slotDeltaY = seenPointY - publishedY[slot];
                    const int32_t distanceSquared = (int32_t)slotDeltaX * slotDeltaX + (int32_t)slotDeltaY * slotDeltaY;

                    if (distanceSquared < minDistanceSquared) {
                        minDistanceSquared = distanceSquared;
                        nearestSlot = slot;
                    }
                }

                // Anti-teleport: a corner can't have moved more than a good part of the rectangle's size within
                // one frame. If the nearest corner is farther than that, the LED isn't one of ours (a reflection,
                // say), or the tracking is lost; either way it is dropped. The limit is three quarters of the
                // rectangle's shorter side, scaled to the frame rate, and capped so its square fits int32_t.
                const float jumpLimit = ((((int)(wideLayout ? rectangleHeight : rectangleWidth) * 3) / 4) * FRAME_RATE_SCALE);
                const int cappedJumpLimit = jumpLimit < (float)MAX_SQUARABLE_DISTANCE ? (int)jumpLimit : MAX_SQUARABLE_DISTANCE;
                const int32_t maxAllowedDistanceSquared = jumpLimit < (float)MAX_SQUARABLE_DISTANCE ?
                    (int32_t)cappedJumpLimit * cappedJumpLimit : INT32_MAX;

                if (minDistanceSquared > maxAllowedDistanceSquared) {
                    previousSeenCount = 0;
                    return;
                }

                // Move all 4 corners by the movement of the one that was seen.
                const int movementX = seenPointX - publishedX[nearestSlot];
                const int movementY = seenPointY - publishedY[nearestSlot];

                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    pointX[slot] = publishedX[slot] + movementX;
                    pointY[slot] = publishedY[slot] + movementY;
                }
            } else {
                // Nothing known to move from.
                previousSeenCount = 0;
                return;
            }
        }
        else if (seenCount == 2) {
            // Two LEDs are ambiguous: they can be any of the 6 pairs of corners (two on the top, bottom, left
            // or right side, or on a diagonal). The pair is chosen by how well it explains what is seen, and
            // then the other two corners are built from it.
            if (!(idealAspectRatio > 0.0f && idealAspectRatio < FLT_MAX)) {
                return;
            }
            const int firstPointX = pointX[0], firstPointY = pointY[0];
            const int secondPointX = pointX[1], secondPointY = pointY[1];

            uint8_t bestSlotForFirst = 0, bestSlotForSecond = 1;
            uint32_t minTotalCost = UINT32_MAX;

            // The 6 possible pairs of corners:           top    bottom  left   right  main diag  other diag
            //                                            (0,1)  (2,3)   (0,2)  (1,3)  (0,3)      (1,2)
            const uint8_t slotPairs[6][2] = { {0,1}, {2,3}, {0,2}, {1,3}, {0,3}, {1,2} };

            // The measured distance between the two LEDs.
            const int32_t measuredDeltaX = secondPointX - firstPointX;
            const int32_t measuredDeltaY = secondPointY - firstPointY;
            const uint32_t measuredDistanceSquared = measuredDeltaX * measuredDeltaX + measuredDeltaY * measuredDeltaY;

            // What that distance would be for each kind of pair: the width for the top and bottom pairs, the
            // height for the left and right ones, the diagonal for the two diagonals. The truncation to an
            // integer is kept as in the original implementation, as the costs below are compared in integers.
            const uint32_t widthSquared = (int32_t)(rectangleWidth * rectangleWidth);
            const uint32_t heightSquared = (int32_t)(rectangleHeight * rectangleHeight);
            const uint32_t diagonalSquared = widthSquared + heightSquared;
            const uint32_t expectedDistanceSquared[6] = {widthSquared, widthSquared, heightSquared, heightSquared, diagonalSquared, diagonalSquared};

            // For each corner: how far each of the two LEDs is from where the corner was shown, and the penalty for
            // corners that weren't measured in the previous frame. Each of these is used by three of the pairs,
            // so it is computed once. A single squared distance fits int32_t, and the sum of two fits uint32_t.
            uint32_t firstPointDistanceSquared[LED_COUNT], secondPointDistanceSquared[LED_COUNT], slotHistoryPenalty[LED_COUNT];
            const bool useHistory = previousSeenCount >= 1 && previousMeasuredMask != 0;
            for (uint8_t slot = 0; slot < LED_COUNT; ++slot) {
                const int32_t firstDeltaX = firstPointX - publishedX[slot], firstDeltaY = firstPointY - publishedY[slot];
                const int32_t secondDeltaX = secondPointX - publishedX[slot], secondDeltaY = secondPointY - publishedY[slot];
                firstPointDistanceSquared[slot] = firstDeltaX * firstDeltaX + firstDeltaY * firstDeltaY;
                secondPointDistanceSquared[slot] = secondDeltaX * secondDeltaX + secondDeltaY * secondDeltaY;
                // A corner that was hidden in the previous frame has no recent measurement behind its position,
                // so a pairing that relies on it is made much less attractive than one that continues the last frame.
                slotHistoryPenalty[slot] = useHistory && !(previousMeasuredMask & slotMaskBit(slot)) ? HISTORY_PENALTY : 0;
            }

            // Pick the pair, and which LED is which corner of it, with the lowest total cost: the pair's size
            // against what is seen, plus how far the LEDs are from where those corners were, plus the history penalty.
            for (uint8_t pair = 0; pair < 6; ++pair) {
                const uint8_t firstSlot = slotPairs[pair][0], secondSlot = slotPairs[pair][1];
                uint32_t penalty = slotHistoryPenalty[firstSlot] + slotHistoryPenalty[secondSlot];
                const uint32_t expected = expectedDistanceSquared[pair];
                const uint32_t distanceError = measuredDistanceSquared >= expected ?
                                               measuredDistanceSquared - expected : expected - measuredDistanceSquared;

                // All terms are nonnegative. The remaining budget is tested BEFORE adding, so that a cost that
                // would overflow can never become a false winner.
                if (distanceError >= minTotalCost || penalty >= minTotalCost - distanceError)
                    continue;
                penalty += distanceError;

                // The two LEDs can be assigned to the pair's two corners in two ways; try both.
                const uint32_t costDirect = firstPointDistanceSquared[firstSlot] + secondPointDistanceSquared[secondSlot];
                if (costDirect < minTotalCost - penalty) {
                    minTotalCost = costDirect + penalty;
                    bestSlotForFirst = firstSlot;
                    bestSlotForSecond = secondSlot;
                }

                const uint32_t costSwapped = firstPointDistanceSquared[secondSlot] + secondPointDistanceSquared[firstSlot];
                if (costSwapped < minTotalCost - penalty) {
                    minTotalCost = costSwapped + penalty;
                    bestSlotForFirst = secondSlot;
                    bestSlotForSecond = firstSlot;
                }
            }

            // No cost could be represented: keep the last valid geometry.
            if (minTotalCost == UINT32_MAX)
                return;

            // Start from the corners as they were shown, and put the two measured LEDs in their corners.
            float reconstructedX[LED_COUNT], reconstructedY[LED_COUNT];
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                reconstructedX[slot] = (float)publishedX[slot];
                reconstructedY[slot] = (float)publishedY[slot];
            }
            reconstructedX[bestSlotForFirst] = (float)firstPointX;
            reconstructedY[bestSlotForFirst] = (float)firstPointY;
            reconstructedX[bestSlotForSecond] = (float)secondPointX;
            reconstructedY[bestSlotForSecond] = (float)secondPointY;

            const uint8_t firstSlot = bestSlotForFirst, secondSlot = bestSlotForSecond;
            const bool pairIsTop = (firstSlot == TOP_LEFT && secondSlot == TOP_RIGHT) || (firstSlot == TOP_RIGHT && secondSlot == TOP_LEFT);
            const bool pairIsBottom = (firstSlot == BOTTOM_LEFT && secondSlot == BOTTOM_RIGHT) || (firstSlot == BOTTOM_RIGHT && secondSlot == BOTTOM_LEFT);
            const bool pairIsLeft = (firstSlot == TOP_LEFT && secondSlot == BOTTOM_LEFT) || (firstSlot == BOTTOM_LEFT && secondSlot == TOP_LEFT);
            const bool pairIsRight = (firstSlot == TOP_RIGHT && secondSlot == BOTTOM_RIGHT) || (firstSlot == BOTTOM_RIGHT && secondSlot == TOP_RIGHT);

            // If the last geometry with all 4 LEDs is known, the two hidden corners are rebuilt from it, which
            // keeps the shape as the camera really sees it. Without it the best that can be done is a flat rectangle.
            bool rebuiltFromAnchor = false;
            if (anchorAvailable) {
                uint8_t hiddenSlots[2], hiddenSlotCount = 0;
                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    if (slot != firstSlot && slot != secondSlot) hiddenSlots[hiddenSlotCount++] = slot;
                }
                const float anchorMeasuredX[2] = {(float)anchorX[firstSlot], (float)anchorX[secondSlot]};
                const float anchorMeasuredY[2] = {(float)anchorY[firstSlot], (float)anchorY[secondSlot]};
                const float currentMeasuredX[2] = {(float)firstPointX, (float)secondPointX};
                const float currentMeasuredY[2] = {(float)firstPointY, (float)secondPointY};
                float hiddenX[2] = {(float)anchorX[hiddenSlots[0]], (float)anchorX[hiddenSlots[1]]};
                float hiddenY[2] = {(float)anchorY[hiddenSlots[0]], (float)anchorY[hiddenSlots[1]]};
                rebuiltFromAnchor = reconstructFromAnchor(2, anchorMeasuredX, anchorMeasuredY, currentMeasuredX, currentMeasuredY, 2, hiddenX, hiddenY);
                if (rebuiltFromAnchor) {
                    for (uint8_t i = 0; i < 2; i++) {
                        reconstructedX[hiddenSlots[i]] = hiddenX[i];
                        reconstructedY[hiddenSlots[i]] = hiddenY[i];
                    }
                }
            }

            // Without an anchor, a flat rectangle is built on the measured side. The two known corners are put in
            // left-to-right (or top-to-bottom) order first, and the hidden ones are placed next to them, one
            // rectangle height (or width) away, perpendicular to the known side and on the correct side of it.
            // The order matters: with the wrong one the perpendicular points the wrong way and the rectangle
            // would come out crossed over like an hourglass.
            if (rebuiltFromAnchor) {
                // Both hidden corners are already rebuilt.
            } else if (pairIsTop || pairIsBottom) {
                // The known side is horizontal. firstKnown/secondKnown are the left and right known corners,
                // firstHidden/secondHidden the corners below (for the top pair) or above (for the bottom pair) them.
                uint8_t firstKnown = pairIsTop ? TOP_LEFT : BOTTOM_LEFT, secondKnown = pairIsTop ? TOP_RIGHT : BOTTOM_RIGHT;
                uint8_t firstHidden = pairIsTop ? BOTTOM_LEFT : TOP_LEFT, secondHidden = pairIsTop ? BOTTOM_RIGHT : TOP_RIGHT;
                if (reconstructedX[firstKnown] > reconstructedX[secondKnown]) {
                    // The LEDs are mirrored by the roll: the "left" corner is on the right. Swap the roles.
                    uint8_t swapped = firstKnown; firstKnown = secondKnown; secondKnown = swapped;
                    swapped = firstHidden; firstHidden = secondHidden; secondHidden = swapped;
                }
                const float knownSideX = reconstructedX[secondKnown] - reconstructedX[firstKnown];
                const float knownSideY = reconstructedY[secondKnown] - reconstructedY[firstKnown];
                const float baseLength = sqrtf(knownSideX * knownSideX + knownSideY * knownSideY);

                // Below 2 mouse units the two LEDs are the same point and the direction is meaningless.
                if (baseLength > 2.0f) {
                    // The height follows from the base and the known proportion, but never less than half of the
                    // current height: a degenerate proportion must not squash the rectangle flat.
                    const float newHeight = fmaxf(baseLength / idealAspectRatio, rectangleHeight * 0.5f);
                    const float inverseBaseLength = 1.0f / baseLength;
                    // The known side rotated by 90 degrees, scaled to the new height: the "downward" direction of the rectangle.
                    const float perpendicularX = -knownSideY * inverseBaseLength * newHeight;
                    const float perpendicularY = knownSideX * inverseBaseLength * newHeight;
                    if (pairIsTop) {
                        reconstructedX[firstHidden] = reconstructedX[firstKnown] + perpendicularX;
                        reconstructedY[firstHidden] = reconstructedY[firstKnown] + perpendicularY;
                        reconstructedX[secondHidden] = reconstructedX[secondKnown] + perpendicularX;
                        reconstructedY[secondHidden] = reconstructedY[secondKnown] + perpendicularY;
                    } else {
                        reconstructedX[firstHidden] = reconstructedX[firstKnown] - perpendicularX;
                        reconstructedY[firstHidden] = reconstructedY[firstKnown] - perpendicularY;
                        reconstructedX[secondHidden] = reconstructedX[secondKnown] - perpendicularX;
                        reconstructedY[secondHidden] = reconstructedY[secondKnown] - perpendicularY;
                    }
                }
            } else if (pairIsLeft || pairIsRight) {
                // The known side is vertical. firstKnown/secondKnown are the upper and lower known corners,
                // firstHidden/secondHidden the corners to the right (for the left pair) or left (for the right pair) of them.
                uint8_t firstKnown = pairIsLeft ? TOP_LEFT : TOP_RIGHT, secondKnown = pairIsLeft ? BOTTOM_LEFT : BOTTOM_RIGHT;
                uint8_t firstHidden = pairIsLeft ? TOP_RIGHT : TOP_LEFT, secondHidden = pairIsLeft ? BOTTOM_RIGHT : BOTTOM_LEFT;
                if (reconstructedY[firstKnown] > reconstructedY[secondKnown]) {
                    uint8_t swapped = firstKnown; firstKnown = secondKnown; secondKnown = swapped;
                    swapped = firstHidden; firstHidden = secondHidden; secondHidden = swapped;
                }
                const float knownSideX = reconstructedX[secondKnown] - reconstructedX[firstKnown];
                const float knownSideY = reconstructedY[secondKnown] - reconstructedY[firstKnown];
                const float sideLength = sqrtf(knownSideX * knownSideX + knownSideY * knownSideY);
                if (sideLength > 2.0f) {
                    // The width follows from the measured height and the known proportion.
                    const float newWidth = sideLength * idealAspectRatio;
                    const float inverseSideLength = 1.0f / sideLength;
                    const float perpendicularX = -knownSideY * inverseSideLength * newWidth;
                    const float perpendicularY = knownSideX * inverseSideLength * newWidth;
                    if (pairIsLeft) {
                        reconstructedX[firstHidden] = reconstructedX[firstKnown] - perpendicularX;
                        reconstructedY[firstHidden] = reconstructedY[firstKnown] - perpendicularY;
                        reconstructedX[secondHidden] = reconstructedX[secondKnown] - perpendicularX;
                        reconstructedY[secondHidden] = reconstructedY[secondKnown] - perpendicularY;
                    } else {
                        reconstructedX[firstHidden] = reconstructedX[firstKnown] + perpendicularX;
                        reconstructedY[firstHidden] = reconstructedY[firstKnown] + perpendicularY;
                        reconstructedX[secondHidden] = reconstructedX[secondKnown] + perpendicularX;
                        reconstructedY[secondHidden] = reconstructedY[secondKnown] + perpendicularY;
                    }
                }
            } else {
                // The known corners are on a diagonal, which fixes the size and rotation of the whole rectangle
                // once the proportion is known: the diagonal's length gives the height and width, and its
                // direction gives the rotation.
                const bool pairIsMainDiagonal = (firstSlot == TOP_LEFT && secondSlot == BOTTOM_RIGHT) || (firstSlot == BOTTOM_RIGHT && secondSlot == TOP_LEFT);
                const float diagonalStartX = pairIsMainDiagonal ? reconstructedX[TOP_LEFT] : reconstructedX[TOP_RIGHT];
                const float diagonalStartY = pairIsMainDiagonal ? reconstructedY[TOP_LEFT] : reconstructedY[TOP_RIGHT];
                const float diagonalEndX = pairIsMainDiagonal ? reconstructedX[BOTTOM_RIGHT] : reconstructedX[BOTTOM_LEFT];
                const float diagonalEndY = pairIsMainDiagonal ? reconstructedY[BOTTOM_RIGHT] : reconstructedY[BOTTOM_LEFT];
                const float diagonalVectorX = diagonalEndX - diagonalStartX, diagonalVectorY = diagonalEndY - diagonalStartY;
                const float diagonalLength = sqrtf(diagonalVectorX * diagonalVectorX + diagonalVectorY * diagonalVectorY);
                if (diagonalLength > 2.0f) {
                    // diagonal^2 = width^2 + height^2 and width = height * proportion.
                    const float heightFromDiagonal = diagonalLength / sqrtf(idealAspectRatio * idealAspectRatio + 1.0f);
                    const float widthFromDiagonal = heightFromDiagonal * idealAspectRatio;
                    // Rotation: the diagonal of an upright rectangle points at a known angle (down-right for the
                    // main diagonal, down-left for the other one); the measured diagonal's angle minus that is the roll.
                    const float measuredAngle = atan2f(diagonalVectorY, diagonalVectorX);
                    const float uprightAngle = pairIsMainDiagonal ? atan2f(heightFromDiagonal, widthFromDiagonal) : atan2f(heightFromDiagonal, -widthFromDiagonal);
                    const float rollAngleOfPair = measuredAngle - uprightAngle;
                    const float rollCosine = cosf(rollAngleOfPair), rollSine = sinf(rollAngleOfPair);
                    // The rectangle's width and height sides, rotated by the roll.
                    const float widthVectorX = rollCosine * widthFromDiagonal, widthVectorY = rollSine * widthFromDiagonal;
                    const float heightVectorX = -rollSine * heightFromDiagonal, heightVectorY = rollCosine * heightFromDiagonal;
                    if (pairIsMainDiagonal) {
                        reconstructedX[TOP_RIGHT] = reconstructedX[TOP_LEFT] + widthVectorX;
                        reconstructedY[TOP_RIGHT] = reconstructedY[TOP_LEFT] + widthVectorY;
                        reconstructedX[BOTTOM_LEFT] = reconstructedX[TOP_LEFT] + heightVectorX;
                        reconstructedY[BOTTOM_LEFT] = reconstructedY[TOP_LEFT] + heightVectorY;
                    } else {
                        reconstructedX[TOP_LEFT] = reconstructedX[TOP_RIGHT] - widthVectorX;
                        reconstructedY[TOP_LEFT] = reconstructedY[TOP_RIGHT] - widthVectorY;
                        reconstructedX[BOTTOM_RIGHT] = reconstructedX[TOP_RIGHT] + heightVectorX;
                        reconstructedY[BOTTOM_RIGHT] = reconstructedY[TOP_RIGHT] + heightVectorY;
                    }
                }
            }

            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                // Ordered comparisons reject NaN and infinity as well, before the conversion to int.
                if (!(reconstructedX[slot] >= -COORDINATE_LIMIT && reconstructedX[slot] <= COORDINATE_LIMIT &&
                      reconstructedY[slot] >= -COORDINATE_LIMIT && reconstructedY[slot] <= COORDINATE_LIMIT)) {
                    return;
                }
                pointX[slot] = fastRound(reconstructedX[slot]);
                pointY[slot] = fastRound(reconstructedY[slot]);
            }
        }
        else if (seenCount == 3) {
            // Three LEDs are three corners of a rectangle, and the fourth follows from the parallelogram rule:
            // it is the corner opposite the one between the two shorter sides. In the triangle of the three
            // LEDs, the longest side is a diagonal of the rectangle, and the remaining point is the corner
            // between the two other sides. With perspective the rectangle is not exactly a parallelogram, so this
            // is only a first guess that is refined further below.
            deltaX = pointX[0] - pointX[1]; deltaY = pointY[0] - pointY[1];
            const int32_t distanceSquared01 = deltaX * deltaX + deltaY * deltaY;

            deltaX = pointX[1] - pointX[2]; deltaY = pointY[1] - pointY[2];
            const int32_t distanceSquared12 = deltaX * deltaX + deltaY * deltaY;

            deltaX = pointX[0] - pointX[2]; deltaY = pointY[0] - pointY[2];
            const int32_t distanceSquared02 = deltaX * deltaX + deltaY * deltaY;

            uint8_t diagonalEndA;  // one end of the longest side, which is a diagonal of the rectangle
            uint8_t cornerPoint;   // the point between the two shorter sides
            uint8_t diagonalEndB;  // the other end of the longest side

            if (distanceSquared01 >= distanceSquared12 && distanceSquared01 >= distanceSquared02) {
                // 0-1 is the longest side, so 2 is the corner between the others
                diagonalEndA = 0; diagonalEndB = 1; cornerPoint = 2;
            }
            else if (distanceSquared12 >= distanceSquared02) {
                // 1-2 is the longest side, so 0 is the corner
                diagonalEndA = 1; diagonalEndB = 2; cornerPoint = 0;
            }
            else {
                // 0-2 is the longest side, so 1 is the corner
                diagonalEndA = 0; diagonalEndB = 2; cornerPoint = 1;
            }

            // The fourth corner is the one opposite the corner point: sum of the diagonal's ends minus the
            // corner point. Its place in the rectangle is not known yet; the labelling below sorts that out.
            pointX[3] = pointX[diagonalEndA] + pointX[diagonalEndB] - pointX[cornerPoint];
            pointY[3] = pointY[diagonalEndA] + pointY[diagonalEndB] - pointY[cornerPoint];
        }

        // From here on there are always 4 points (measured, or reconstructed above), and they have to be labelled
        // with the corners of the rectangle: top left, top right, bottom left, bottom right.
        if (!areCoordinatesValid(pointX, pointY)) {
            return;
        }

        uint8_t indicesSortedByX[LED_COUNT], indicesSortedByY[LED_COUNT];
        uint8_t topLeftPoint, topRightPoint, bottomLeftPoint, bottomRightPoint;
        sortIndicesAscending(pointX, indicesSortedByX);
        sortIndicesAscending(pointY, indicesSortedByY);

        if (calibrationMode) {
            // In calibration the gun faces the screen with close to no roll, so the two highest points are the top
            // ones. The final left/right correction below sorts out the order within each pair.
            topLeftPoint = indicesSortedByY[0]; topRightPoint = indicesSortedByY[1];
            bottomLeftPoint = indicesSortedByY[2]; bottomRightPoint = indicesSortedByY[3];
        }
        else if (wideLayout) {
            // Wide layout: the rectangle is wider than tall, so it is the left and right pairs of points that stay
            // together, and the roles of the two axes are the reverse of the normal layout below.
            //
            // The zone: if the two rightmost points are within it of each other in X, the right side is
            // (nearly) vertical, so the gun is (nearly) upright. The zone is a few percent of the image
            // width, but never more than 1/8 of the points' own extent so that distant rectangles work too.
            const int cameraZone = (MouseResX * ZONE_NUMERATOR) / ZONE_DENOMINATOR;
            const int geometryZone = (pointX[indicesSortedByX[3]] - pointX[indicesSortedByX[0]]) / ZONE_EXTENT_DIVISOR;
            const int criticalZone = geometryZone < cameraZone ? geometryZone : cameraZone;

            // How close the highest point is to the rightmost one, and to the leftmost one.
            deltaX = pointX[indicesSortedByX[3]] - pointX[indicesSortedByY[0]];
            deltaY = pointY[indicesSortedByX[3]] - pointY[indicesSortedByY[0]];
            const int32_t distanceToRightmostSquared = (deltaX * deltaX) + (deltaY * deltaY);

            deltaX = pointX[indicesSortedByX[0]] - pointX[indicesSortedByY[0]];
            deltaY = pointY[indicesSortedByX[0]] - pointY[indicesSortedByY[0]];
            const int32_t distanceToLeftmostSquared = (deltaX * deltaX) + (deltaY * deltaY);

            if ((pointX[indicesSortedByX[3]] - pointX[indicesSortedByX[2]]) > criticalZone) {
                // Rolled: the corners are told apart by which extreme points sit next to each other.
                if (distanceToRightmostSquared < distanceToLeftmostSquared) {
                    // The highest point is nearer to the rightmost point than to the leftmost one, so it belongs to
                    // the right side: it is the top right. The lowest point, opposite to it, is the bottom left.
                    topRightPoint = indicesSortedByY[0];
                    bottomLeftPoint = indicesSortedByY[3];

                    // Of the two remaining points, the second highest is the top left if it is also the leftmost
                    // point of all; otherwise it is the bottom right.
                    if (indicesSortedByY[1] == indicesSortedByX[0]) {
                        topLeftPoint = indicesSortedByY[1];
                        bottomRightPoint = indicesSortedByY[2];
                    } else {
                        bottomRightPoint = indicesSortedByY[1];
                        topLeftPoint = indicesSortedByY[2];
                    }
                } else {
                    // The highest point is nearer to the leftmost point: it belongs to the left side, so it is the
                    // top left, and the lowest point, opposite to it, is the bottom right. The two remaining points
                    // are told apart the same way as above.
                    topLeftPoint = indicesSortedByY[0];
                    bottomRightPoint = indicesSortedByY[3];

                    if (indicesSortedByY[1] == indicesSortedByX[0]) {
                        bottomLeftPoint = indicesSortedByY[1];
                        topRightPoint = indicesSortedByY[2];
                    } else {
                        topRightPoint = indicesSortedByY[1];
                        bottomLeftPoint = indicesSortedByY[2];
                    }
                }
            } else {
                // Upright: the two leftmost points are the left side and the two rightmost the right side. Which of
                // each pair is the top is decided by the final correction below, from their heights.
                topLeftPoint = indicesSortedByX[1];
                topRightPoint = indicesSortedByX[3];
                bottomLeftPoint = indicesSortedByX[0];
                bottomRightPoint = indicesSortedByX[2];
            }
        }
        else {
            // Normal layout: the rectangle is taller than wide.
            //
            // How close the highest point is to the leftmost one, and the lowest point to the leftmost one.
            // For a rolled rectangle, the leftmost corner is either the top left (then it is near the highest
            // point) or the bottom left (then it is near the lowest point).
            deltaX = pointX[indicesSortedByY[0]] - pointX[indicesSortedByX[0]];
            deltaY = pointY[indicesSortedByY[0]] - pointY[indicesSortedByX[0]];
            const int32_t distanceHighestToLeftmostSquared = (deltaX * deltaX) + (deltaY * deltaY);

            deltaX = pointX[indicesSortedByY[3]] - pointX[indicesSortedByX[0]];
            deltaY = pointY[indicesSortedByY[3]] - pointY[indicesSortedByX[0]];
            const int32_t distanceLowestToLeftmostSquared = (deltaX * deltaX) + (deltaY * deltaY);

            // The zone: if the two highest points are within it of each other in Y, the top side is
            // (nearly) horizontal, so the gun is (nearly) upright. It is a few percent of the image height
            // (historically 30 pixels of 768, tested on the DFRobot), scaled to the camera's resolution and
            // never more than 1/8 of the points' own extent, so that distant rectangles work too.
            const int cameraZone = (MouseResY * ZONE_NUMERATOR) / ZONE_DENOMINATOR;
            const int geometryZone = (pointY[indicesSortedByY[3]] - pointY[indicesSortedByY[0]]) / ZONE_EXTENT_DIVISOR;
            const int criticalZone = geometryZone < cameraZone ? geometryZone : cameraZone;

            if ((pointY[indicesSortedByY[1]] - pointY[indicesSortedByY[0]]) > criticalZone) {
                // Rolled: the corners are told apart by which extreme points sit next to each other.
                if (distanceHighestToLeftmostSquared < distanceLowestToLeftmostSquared) {
                    // The leftmost point is next to the highest: it is the top left, and the rightmost
                    // is the bottom right.
                    topLeftPoint = indicesSortedByX[0];
                    bottomRightPoint = indicesSortedByX[3];

                    // Of the two middle points, the one that is also the lowest of all is taken to be the bottom left.
                    if (indicesSortedByX[1] == indicesSortedByY[3]) {
                        bottomLeftPoint = indicesSortedByX[1];
                        topRightPoint = indicesSortedByX[2];
                    } else {
                        topRightPoint = indicesSortedByX[1];
                        bottomLeftPoint = indicesSortedByX[2];
                    }
                } else {
                    // The leftmost point is next to the lowest: it is the bottom left, and the rightmost
                    // is the top right.
                    bottomLeftPoint = indicesSortedByX[0];
                    topRightPoint = indicesSortedByX[3];

                    if (indicesSortedByX[1] == indicesSortedByY[3]) {
                        bottomRightPoint = indicesSortedByX[1];
                        topLeftPoint = indicesSortedByX[2];
                    } else {
                        topLeftPoint = indicesSortedByX[1];
                        bottomRightPoint = indicesSortedByX[2];
                    }
                }
            } else {
                // Upright: the two highest points are the top side and the two lowest the bottom side. Which
                // of each pair is the left one is decided by the final correction below, from their X.
                topLeftPoint = indicesSortedByY[0];
                topRightPoint = indicesSortedByY[1];
                bottomLeftPoint = indicesSortedByY[2];
                bottomRightPoint = indicesSortedByY[3];
            }
        }

        // Final correction, which enforces the naming (top left, top right, bottom left, bottom right) whatever the
        // branches above gave: within each pair, the lower one is the bottom, and the more leftward one is the left.
        // Only a few of the branches really need it, but it costs next to nothing, so it always runs
        // as a safety net for borderline cases.
        {
            uint8_t swapped;
            if (pointY[topLeftPoint] > pointY[bottomLeftPoint]) { swapped = topLeftPoint; topLeftPoint = bottomLeftPoint; bottomLeftPoint = swapped; }
            if (pointY[topRightPoint] > pointY[bottomRightPoint]) { swapped = topRightPoint; topRightPoint = bottomRightPoint; bottomRightPoint = swapped; }
            if (pointX[topLeftPoint] > pointX[topRightPoint]) { swapped = topLeftPoint; topLeftPoint = topRightPoint; topRightPoint = swapped; }
            if (pointX[bottomLeftPoint] > pointX[bottomRightPoint]) { swapped = bottomLeftPoint; bottomLeftPoint = bottomRightPoint; bottomRightPoint = swapped; }
        }

        // With 1 or 2 LEDs the points were built directly in slot order (point 0 is the top left, and so on) by
        // the reconstruction above, so that order is kept: re-sorting a perspective-shaped rectangle by
        // position could swap the labels.
        if (seenCount <= 2 && !calibrationMode) {
            topLeftPoint = TOP_LEFT; topRightPoint = TOP_RIGHT; bottomLeftPoint = BOTTOM_LEFT; bottomRightPoint = BOTTOM_RIGHT;
        }

        // Continuity of the labelling (3-4 points).
        //
        // With strong perspective and roll, the position-based labelling above can change from one frame to the
        // next, or even come out mirrored (which a camera can't produce), mostly with 3 LEDs, where the
        // fourth point is a reconstruction. The perimeter (top left, top right, bottom right, bottom left)
        // follows from the angle of each point around the centroid, which is always oriented correctly. That leaves
        // only one unknown: which of the four points is the top left. The answer is chosen as follows.
        //
        //  - While tracking: the rotation closest to the previous geometry (looking at measured LEDs only).
        //    It replaces the position-based labelling if that is mirrored, or if it is clearly better
        //    (less than 1/4 of the cost) and the shape or the roll agree:
        //      * the shape (base against height, as the layout expects it) confirms it, or
        //      * the shape can't tell, and the rotation doesn't mean a gun roll more than 80 degrees above
        //        that of the position-based labelling.
        //    This also fixes a wrong lock, for example one made at the start with the gun strongly rotated.
        //    The shape can also back the position-based labelling: with 4 LEDs when the true shape of the
        //    rectangle, corrected for perspective, confirms it (seen from far to the side, or above or below the
        //    screen, the apparent shape can invert); with 3 LEDs only in the first second or so after a
        //    (re)start, while the lock is uncertain, and if continuity would mean a more rotated gun.
        //  - After a short dark: the same, if the 4 LEDs are close to the last geometry.
        //  - At the start: only if the position-based labelling is mirrored, the rotation with the straightest gun.
        //
        // Not used in calibration, where the gun faces the screen and the position-based labelling is reliable.
        if (seenCount >= 3 && !calibrationMode) {
            // The position-based labelling as a perimeter: top left, top right, bottom right, bottom left.
            const uint8_t heuristicPerimeter[4] = {topLeftPoint, topRightPoint, bottomRightPoint, bottomLeftPoint};

            // Sort the four points by their angle around their centroid. The result walks round the perimeter,
            // in the same direction as the perimeter above; only the starting point is unknown.
            float centroidX = 0.0f, centroidY = 0.0f;
            for (uint8_t i = 0; i < LED_COUNT; i++) {
                centroidX += pointX[i];
                centroidY += pointY[i];
            }
            centroidX *= 0.25f;
            centroidY *= 0.25f;
            float angleAroundCentroid[LED_COUNT];
            uint8_t pointIndexByAngle[LED_COUNT] = {0, 1, 2, 3};
            for (uint8_t i = 0; i < LED_COUNT; i++)
                angleAroundCentroid[i] = atan2f(pointY[i] - centroidY, pointX[i] - centroidX);
            for (uint8_t i = 1; i < LED_COUNT; i++) {
                for (uint8_t j = i; j > 0 && angleAroundCentroid[pointIndexByAngle[j]] < angleAroundCentroid[pointIndexByAngle[j - 1]]; j--) {
                    const uint8_t swapped = pointIndexByAngle[j];
                    pointIndexByAngle[j] = pointIndexByAngle[j - 1];
                    pointIndexByAngle[j - 1] = swapped;
                }
            }

            // Twice the signed area of the position-based perimeter. Negative means that it runs the wrong
            // way round, i.e. a mirror image, which a camera can't see.
            const float heuristicSignedArea =
                (float)(pointX[bottomRightPoint] - pointX[topLeftPoint]) * (float)(pointY[bottomLeftPoint] - pointY[topRightPoint]) -
                (float)(pointY[bottomRightPoint] - pointY[topLeftPoint]) * (float)(pointX[bottomLeftPoint] - pointX[topRightPoint]);

            // The apparent roll of a labelling: how far the left and right sides (top left to bottom left, and
            // top right to bottom right) are from pointing straight down in the image, from 0 (upright) to pi
            // (upside down). With perspective, those two sides are not parallel in the image but converge towards
            // a vanishing point, and the direction from the image centre to that vanishing point is the gun's
            // roll around its barrel, however it is aimed. With parallel sides the vanishing point is at infinity
            // and the direction is simply that of the sides.
            // It is exact only for an ideal lens. With lens distortion and a reconstructed point (3 LEDs) it is
            // approximate, which the 80 degree margin of the checks tolerates.
            auto apparentRollOf = [&](const uint8_t* perimeter) {
                // Corners relative to the image centre, so that the vanishing point direction from the centre is simply its coordinates.
                const float topLeftRelX = pointX[perimeter[0]] - IMAGE_CENTER_X, topLeftRelY = pointY[perimeter[0]] - IMAGE_CENTER_Y;
                const float topRightRelX = pointX[perimeter[1]] - IMAGE_CENTER_X, topRightRelY = pointY[perimeter[1]] - IMAGE_CENTER_Y;
                const float bottomRightRelX = pointX[perimeter[2]] - IMAGE_CENTER_X, bottomRightRelY = pointY[perimeter[2]] - IMAGE_CENTER_Y;
                const float bottomLeftRelX = pointX[perimeter[3]] - IMAGE_CENTER_X, bottomLeftRelY = pointY[perimeter[3]] - IMAGE_CENTER_Y;

                // Each side as a line in homogeneous coordinates (the line through two points is their cross product),
                // and the vanishing point as the intersection of the two lines (again a cross product).
                const float leftLineX = topLeftRelY - bottomLeftRelY, leftLineY = bottomLeftRelX - topLeftRelX,
                            leftLineW = topLeftRelX * bottomLeftRelY - topLeftRelY * bottomLeftRelX;
                const float rightLineX = topRightRelY - bottomRightRelY, rightLineY = bottomRightRelX - topRightRelX,
                            rightLineW = topRightRelX * bottomRightRelY - topRightRelY * bottomRightRelX;
                float vanishingX = leftLineY * rightLineW - leftLineW * rightLineY;
                float vanishingY = leftLineW * rightLineX - leftLineX * rightLineW;

                // A homogeneous point has a sign ambiguity: the direction to the vanishing point could be reversed.
                // The sign is fixed by pointing it from the top of the rectangle towards its bottom.
                const float topToBottomX = (bottomLeftRelX - topLeftRelX) + (bottomRightRelX - topRightRelX);
                const float topToBottomY = (bottomLeftRelY - topLeftRelY) + (bottomRightRelY - topRightRelY);
                if (vanishingX * topToBottomX + vanishingY * topToBottomY < 0.0f) {
                    vanishingX = -vanishingX;
                    vanishingY = -vanishingY;
                }
                // If the lines are the same or the vanishing point is at the centre, its direction is meaningless:
                // use the direction of the sides instead.
                if (fabsf(vanishingX) + fabsf(vanishingY) < 1.0f) {
                    vanishingX = topToBottomX;
                    vanishingY = topToBottomY;
                }
                return fabsf(atan2f(-vanishingX, vanishingY));
            };

            // The slots, in perimeter order, that the entries of a perimeter stand for.
            const uint8_t perimeterSlots[4] = {TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT};

            // How far a labelling is from the previous geometry: the summed squared distance of its measured points
            // to the corners they would be. Reconstructed points (3 LEDs only) are skipped, as their position
            // says nothing about the labelling. Saturates instead of overflowing.
            auto distanceToPreviousGeometry = [&](const uint8_t* perimeter) {
                uint32_t totalCost = 0;
                for (uint8_t i = 0; i < LED_COUNT; ++i) {
                    if (perimeter[i] >= seenCount) continue;
                    const int32_t costDeltaX = pointX[perimeter[i]] - previousGeometryX[perimeterSlots[i]];
                    const int32_t costDeltaY = pointY[perimeter[i]] - previousGeometryY[perimeterSlots[i]];
                    const uint32_t cost = (uint32_t)(costDeltaX * costDeltaX + costDeltaY * costDeltaY);
                    totalCost = (totalCost > UINT32_MAX - cost) ? UINT32_MAX : totalCost + cost;
                }
                return totalCost;
            };

            // Short dark (a few frames, such as a flickering LED or a hand passing by): with all 4 LEDs visible
            // again, continue from the last geometry if the closest rotation is close to it (summed
            // squared distance under 1/16 of the shortest side squared). Otherwise start over as at the start.
            bool resumeAfterShortDark = false;
            if (!trackingStable && seenCount == 4 && darkFrameCount <= MAX_RESUME_DARK_FRAMES) {
                uint32_t bestCost = UINT32_MAX;
                uint8_t bestRotation = 0;
                for (uint8_t rotation = 0; rotation < LED_COUNT; ++rotation) {
                    const uint8_t candidatePerimeter[4] = {pointIndexByAngle[rotation], pointIndexByAngle[slotInCycle(rotation + 1)],
                                                           pointIndexByAngle[slotInCycle(rotation + 2)], pointIndexByAngle[slotInCycle(rotation + 3)]};
                    const uint32_t cost = distanceToPreviousGeometry(candidatePerimeter);
                    if (cost < bestCost) {
                        bestCost = cost;
                        bestRotation = rotation;
                    }
                }
                // How close is close: relative to the shortest side, so the allowed movement scales with distance.
                float shortestSideSquared = FLT_MAX;
                for (uint8_t side = 0; side < LED_COUNT; ++side) {
                    const float sideDeltaX = (float)(pointX[pointIndexByAngle[slotInCycle(bestRotation + side)]] - pointX[pointIndexByAngle[slotInCycle(bestRotation + side + 1)]]);
                    const float sideDeltaY = (float)(pointY[pointIndexByAngle[slotInCycle(bestRotation + side)]] - pointY[pointIndexByAngle[slotInCycle(bestRotation + side + 1)]]);
                    shortestSideSquared = fminf(shortestSideSquared, sideDeltaX * sideDeltaX + sideDeltaY * sideDeltaY);
                }
                resumeAfterShortDark = (float)bestCost < shortestSideSquared * RESUME_COST_FRACTION;
            }

            int chosenRotation = -1;  // -1 keeps the position-based labelling
            const bool useContinuity = (trackingStable && previousSeenCount >= 3) || resumeAfterShortDark;
            if (!useContinuity) framesSinceColdStart = 0;
            else if (framesSinceColdStart < MAX_FRAME_COUNT) framesSinceColdStart++;

            if (useContinuity) {
                // The rotation that keeps the measured LEDs closest to where they were.
                uint32_t bestCost = UINT32_MAX;
                uint8_t bestRotation = 0;
                for (uint8_t rotation = 0; rotation < LED_COUNT; ++rotation) {
                    const uint8_t candidatePerimeter[4] = {pointIndexByAngle[rotation], pointIndexByAngle[slotInCycle(rotation + 1)],
                                                           pointIndexByAngle[slotInCycle(rotation + 2)], pointIndexByAngle[slotInCycle(rotation + 3)]};
                    const uint32_t cost = distanceToPreviousGeometry(candidatePerimeter);
                    if (cost < bestCost) {
                        bestCost = cost;
                        bestRotation = rotation;
                    }
                }
                const uint8_t candidatePerimeter[4] = {pointIndexByAngle[bestRotation], pointIndexByAngle[slotInCycle(bestRotation + 1)],
                                                       pointIndexByAngle[slotInCycle(bestRotation + 2)], pointIndexByAngle[slotInCycle(bestRotation + 3)]};
                const bool sameAsHeuristic = candidatePerimeter[0] == heuristicPerimeter[0] && candidatePerimeter[1] == heuristicPerimeter[1] &&
                                             candidatePerimeter[2] == heuristicPerimeter[2] && candidatePerimeter[3] == heuristicPerimeter[3];

                if (!sameAsHeuristic) {
                    if (heuristicSignedArea <= 0.0f) {
                        // The position-based labelling is a mirror image, which can't be right.
                        chosenRotation = bestRotation;
                    } else if (bestCost < distanceToPreviousGeometry(heuristicPerimeter) / CLEARLY_BETTER_COST_DIVISOR) {
                        // Continuity is clearly better. Before trusting it, look at the shape: it can disagree.
                        //
                        // The aspect score of a labelling tells how well its shape fits the layout: positive if
                        // its base and height stand as the layout expects (the base longer than the height in the wide
                        // layout, shorter otherwise), negative if they are swapped, as they would be for a lock that is
                        // off by a quarter turn. The shape is judged first, if it is clear, so a gun tilted by
                        // 85-90 degrees doesn't swap the LEDs, and a wrong lock is fixed even with the gun tilted
                        // by 20-45 degrees. The score is (AB^2 + CD^2 - AC^2 - BD^2) / their sum, where A, B, C, D are
                        // the top left, top right, bottom left, bottom right.
                        auto aspectScoreOf = [&](const uint8_t* perimeter) {
                            auto distanceSquaredBetween = [&](uint8_t firstPoint, uint8_t secondPoint) {
                                const float aspectDeltaX = (float)(pointX[firstPoint] - pointX[secondPoint]);
                                const float aspectDeltaY = (float)(pointY[firstPoint] - pointY[secondPoint]);
                                return aspectDeltaX * aspectDeltaX + aspectDeltaY * aspectDeltaY;
                            };
                            const float horizontalSides = distanceSquaredBetween(perimeter[1], perimeter[0]) + distanceSquaredBetween(perimeter[2], perimeter[3]);
                            const float verticalSides = distanceSquaredBetween(perimeter[3], perimeter[0]) + distanceSquaredBetween(perimeter[2], perimeter[1]);
                            const float score = (horizontalSides - verticalSides) / fmaxf(horizontalSides + verticalSides, 1.0f);
                            return wideLayout ? score : -score;
                        };

                        // Seen from far to the side, or from far above or below the screen, perspective shortens one
                        // side so much that the apparent shape inverts, and aspectScoreOf() is fooled. With 4 real
                        // LEDs the true shape of the rectangle can be recovered from the perspective of the 4 points
                        // and the focal length:
                        // in space the fourth corner is D = A + (B - A) + (C - A), which relates the depths of B and C
                        // (distance from the camera) to the depth of A. Those depth ratios come from ratios of areas of
                        // triangles in the image. Knowing them, the 3D sides A-B and A-C follow, and with them the
                        // true proportion. The score uses the same scale and sign as aspectScoreOf(), and is 0 if the
                        // quadrilateral is degenerate. It is only needed to judge continuity, where the position-based
                        // order can be crossed and so can't be used.
                        auto trueAspectScoreOf = [&](const uint8_t* perimeter) {
                            // Corners relative to the image centre. In perimeter order: A = top left, B = top right,
                            // D = bottom right, C = bottom left.
                            const float aX = pointX[perimeter[0]] - IMAGE_CENTER_X, aY = pointY[perimeter[0]] - IMAGE_CENTER_Y;
                            const float bX = pointX[perimeter[1]] - IMAGE_CENTER_X, bY = pointY[perimeter[1]] - IMAGE_CENTER_Y;
                            const float dX = pointX[perimeter[2]] - IMAGE_CENTER_X, dY = pointY[perimeter[2]] - IMAGE_CENTER_Y;
                            const float cX = pointX[perimeter[3]] - IMAGE_CENTER_X, cY = pointY[perimeter[3]] - IMAGE_CENTER_Y;

                            // Twice the signed area of the triangle of three image points.
                            auto doubleSignedArea = [](float x0, float y0, float x1, float y1, float x2, float y2) {
                                return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
                            };
                            const float areaForB = doubleSignedArea(bX, bY, dX, dY, cX, cY);
                            const float areaForC = doubleSignedArea(cX, cY, dX, dY, bX, bY);
                            if (fabsf(areaForB) < 1.0f || fabsf(areaForC) < 1.0f) return 0.0f;
                            const float depthRatioB = doubleSignedArea(aX, aY, dX, dY, cX, cY) / areaForB;  // depth of B / depth of A
                            const float depthRatioC = doubleSignedArea(aX, aY, dX, dY, bX, bY) / areaForC;  // depth of C / depth of A
                            // A point can't be behind the camera.
                            if (!(depthRatioB > 0.0f && depthRatioC > 0.0f)) return 0.0f;

                            // The 3D side vectors A-B and A-C (in units of A's depth / focal length).
                            const float sideABx = depthRatioB * bX - aX, sideABy = depthRatioB * bY - aY, sideABz = FOCAL_LENGTH * (depthRatioB - 1.0f);
                            const float sideACx = depthRatioC * cX - aX, sideACy = depthRatioC * cY - aY, sideACz = FOCAL_LENGTH * (depthRatioC - 1.0f);
                            const float sideABSquared = sideABx * sideABx + sideABy * sideABy + sideABz * sideABz;
                            const float sideACSquared = sideACx * sideACx + sideACy * sideACy + sideACz * sideACz;
                            const float score = (sideABSquared - sideACSquared) / fmaxf(sideABSquared + sideACSquared, 1.0f);
                            return wideLayout ? score : -score;
                        };

                        const float candidateAspect = aspectScoreOf(candidatePerimeter);
                        const float heuristicAspect = aspectScoreOf(heuristicPerimeter);
                        bool acceptContinuity;
                        if (candidateAspect > CLEAR_ASPECT_SCORE && heuristicAspect < -CLEAR_ASPECT_SCORE) {
                            // The shape confirms the continuity answer, and contradicts the position-based one.
                            acceptContinuity = true;
                        } else if (candidateAspect < -CLEAR_ASPECT_SCORE && heuristicAspect > CLEAR_ASPECT_SCORE &&
                                   (seenCount == 4 ? trueAspectScoreOf(candidatePerimeter) < -CLEAR_ASPECT_SCORE
                                                   : (framesSinceColdStart < UNCERTAIN_LOCK_FRAMES &&
                                                      apparentRollOf(candidatePerimeter) > apparentRollOf(heuristicPerimeter)))) {
                            // The shape confirms the position-based answer. With 4 LEDs the perspective-corrected shape
                            // has to agree; with 3 the apparent shape alone is less trustworthy, so it only counts while
                            // the lock is uncertain and when continuity would mean a more rolled gun.
                            acceptContinuity = false;
                        } else {
                            // The shape can't tell: accept continuity unless it implies a far more rolled gun than
                            // the position-based answer, which is the sign of a lock that went wrong.
                            acceptContinuity = apparentRollOf(candidatePerimeter) < apparentRollOf(heuristicPerimeter) + MAX_EXTRA_ROLL_RADIANS;
                        }
                        if (acceptContinuity) chosenRotation = bestRotation;
                    }
                }
            } else if (heuristicSignedArea < 0.0f) {
                // Nothing to continue from, and the position-based labelling is a mirror image: take the rotation
                // that makes the gun the most upright.
                float lowestRoll = 10.0f;  // more than pi, the largest possible roll
                for (uint8_t rotation = 0; rotation < LED_COUNT; ++rotation) {
                    const uint8_t candidatePerimeter[4] = {pointIndexByAngle[rotation], pointIndexByAngle[slotInCycle(rotation + 1)],
                                                           pointIndexByAngle[slotInCycle(rotation + 2)], pointIndexByAngle[slotInCycle(rotation + 3)]};
                    const float roll = apparentRollOf(candidatePerimeter);
                    if (roll < lowestRoll) {
                        lowestRoll = roll;
                        chosenRotation = rotation;
                    }
                }
            }

            if (chosenRotation >= 0) {
                topLeftPoint = pointIndexByAngle[chosenRotation];
                topRightPoint = pointIndexByAngle[slotInCycle(chosenRotation + 1)];
                bottomRightPoint = pointIndexByAngle[slotInCycle(chosenRotation + 2)];
                bottomLeftPoint = pointIndexByAngle[slotInCycle(chosenRotation + 3)];
            }
        }

        // Which of the labelled points were really measured, and which were reconstructed. Compares the
        // labelled points with the untouched measurements. The spring needs to know, as only a change in the set
        // of measured corners makes a jump to smooth out.
        uint8_t measuredMask;
        if (seenCount == 4) {
            measuredMask = 0b1111;
        } else {
            measuredMask = 0;

            for (uint8_t i = 0; i < seenCount; i++) {
                const int measuredPointX = measuredX[i];
                const int measuredPointY = measuredY[i];

                if (pointX[topLeftPoint] == measuredPointX && pointY[topLeftPoint] == measuredPointY) measuredMask |= slotMaskBit(TOP_LEFT);
                else if (pointX[topRightPoint] == measuredPointX && pointY[topRightPoint] == measuredPointY) measuredMask |= slotMaskBit(TOP_RIGHT);
                else if (pointX[bottomLeftPoint] == measuredPointX && pointY[bottomLeftPoint] == measuredPointY) measuredMask |= slotMaskBit(BOTTOM_LEFT);
                else if (pointX[bottomRightPoint] == measuredPointX && pointY[bottomRightPoint] == measuredPointY) measuredMask |= slotMaskBit(BOTTOM_RIGHT);
            }
        }

        // A better position for the fourth point when only 3 are seen. With the last geometry of all 4 LEDs, the
        // missing corner keeps its real shape; otherwise the historical method just below is used.
        bool hiddenRebuiltFromAnchor = false;
        if (seenCount == 3 && anchorAvailable) {
            const uint8_t missingMask = (~measuredMask) & 0x0F;
            // Working-point index of each slot's labelled point.
            const uint8_t pointOfSlot[4] = {topLeftPoint, topRightPoint, bottomLeftPoint, bottomRightPoint};
            uint8_t hiddenSlot = 0, measuredSlots[3], measuredSlotCount = 0, hiddenSlotCount = 0;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                if (missingMask & slotMaskBit(slot)) { hiddenSlot = slot; hiddenSlotCount++; }
                else if (measuredSlotCount < 3) measuredSlots[measuredSlotCount++] = slot;
            }
            // Exactly one slot must be hidden for this to apply.
            if (hiddenSlotCount == 1) {
                float anchorMeasuredX[3], anchorMeasuredY[3], currentMeasuredX[3], currentMeasuredY[3];
                for (uint8_t i = 0; i < 3; i++) {
                    anchorMeasuredX[i] = (float)anchorX[measuredSlots[i]];
                    anchorMeasuredY[i] = (float)anchorY[measuredSlots[i]];
                    currentMeasuredX[i] = (float)pointX[pointOfSlot[measuredSlots[i]]];
                    currentMeasuredY[i] = (float)pointY[pointOfSlot[measuredSlots[i]]];
                }
                float hiddenX = (float)anchorX[hiddenSlot], hiddenY = (float)anchorY[hiddenSlot];
                if (reconstructFromAnchor(3, anchorMeasuredX, anchorMeasuredY, currentMeasuredX, currentMeasuredY, 1, &hiddenX, &hiddenY)) {
                    pointX[pointOfSlot[hiddenSlot]] = fastRound(hiddenX);
                    pointY[pointOfSlot[hiddenSlot]] = fastRound(hiddenY);
                    hiddenRebuiltFromAnchor = true;
                }
            }
        }

        // The historical method, for 3 LEDs without a usable anchor: perspective makes the rectangle differ from
        // a parallelogram, so the parallelogram guess for the missing corner (the sum of its two neighbours
        // minus the opposite corner) is off by some amount. That amount is assumed to be the same as it
        // was in the previous frame, and is added to the guess. It costs only a few additions.
        if (!hiddenRebuiltFromAnchor && seenCount == 3 && previousSeenCount >= 3) {
            const uint8_t missingMask = (~measuredMask) & 0x0F;

            if (missingMask == slotMaskBit(BOTTOM_RIGHT)) {
                // The bottom right is missing; the certain points are the top left, top right and bottom left.
                const int parallelogramX = pointX[topRightPoint] + pointX[bottomLeftPoint] - pointX[topLeftPoint];
                const int parallelogramY = pointY[topRightPoint] + pointY[bottomLeftPoint] - pointY[topLeftPoint];
                const int previousParallelogramX = previousGeometryX[TOP_RIGHT] + previousGeometryX[BOTTOM_LEFT] - previousGeometryX[TOP_LEFT];
                const int previousParallelogramY = previousGeometryY[TOP_RIGHT] + previousGeometryY[BOTTOM_LEFT] - previousGeometryY[TOP_LEFT];
                pointX[bottomRightPoint] = parallelogramX + (previousGeometryX[BOTTOM_RIGHT] - previousParallelogramX);
                pointY[bottomRightPoint] = parallelogramY + (previousGeometryY[BOTTOM_RIGHT] - previousParallelogramY);
            }
            else if (missingMask == slotMaskBit(BOTTOM_LEFT)) {
                // The bottom left is missing; the certain points are the top left, top right and bottom right.
                const int parallelogramX = pointX[topLeftPoint] + pointX[bottomRightPoint] - pointX[topRightPoint];
                const int parallelogramY = pointY[topLeftPoint] + pointY[bottomRightPoint] - pointY[topRightPoint];
                const int previousParallelogramX = previousGeometryX[TOP_LEFT] + previousGeometryX[BOTTOM_RIGHT] - previousGeometryX[TOP_RIGHT];
                const int previousParallelogramY = previousGeometryY[TOP_LEFT] + previousGeometryY[BOTTOM_RIGHT] - previousGeometryY[TOP_RIGHT];
                pointX[bottomLeftPoint] = parallelogramX + (previousGeometryX[BOTTOM_LEFT] - previousParallelogramX);
                pointY[bottomLeftPoint] = parallelogramY + (previousGeometryY[BOTTOM_LEFT] - previousParallelogramY);
            }
            else if (missingMask == slotMaskBit(TOP_RIGHT)) {
                // The top right is missing; the certain points are the top left, bottom left and bottom right.
                const int parallelogramX = pointX[topLeftPoint] + pointX[bottomRightPoint] - pointX[bottomLeftPoint];
                const int parallelogramY = pointY[topLeftPoint] + pointY[bottomRightPoint] - pointY[bottomLeftPoint];
                const int previousParallelogramX = previousGeometryX[TOP_LEFT] + previousGeometryX[BOTTOM_RIGHT] - previousGeometryX[BOTTOM_LEFT];
                const int previousParallelogramY = previousGeometryY[TOP_LEFT] + previousGeometryY[BOTTOM_RIGHT] - previousGeometryY[BOTTOM_LEFT];
                pointX[topRightPoint] = parallelogramX + (previousGeometryX[TOP_RIGHT] - previousParallelogramX);
                pointY[topRightPoint] = parallelogramY + (previousGeometryY[TOP_RIGHT] - previousParallelogramY);
            }
            else if (missingMask == slotMaskBit(TOP_LEFT)) {
                // The top left is missing; the certain points are the top right, bottom left and bottom right.
                const int parallelogramX = pointX[topRightPoint] + pointX[bottomLeftPoint] - pointX[bottomRightPoint];
                const int parallelogramY = pointY[topRightPoint] + pointY[bottomLeftPoint] - pointY[bottomRightPoint];
                const int previousParallelogramX = previousGeometryX[TOP_RIGHT] + previousGeometryX[BOTTOM_LEFT] - previousGeometryX[BOTTOM_RIGHT];
                const int previousParallelogramY = previousGeometryY[TOP_RIGHT] + previousGeometryY[BOTTOM_LEFT] - previousGeometryY[BOTTOM_RIGHT];
                pointX[topLeftPoint] = parallelogramX + (previousGeometryX[TOP_LEFT] - previousParallelogramX);
                pointY[topLeftPoint] = parallelogramY + (previousGeometryY[TOP_LEFT] - previousParallelogramY);
            }
        }

        // The geometry of this frame by slot, before the spring. The spring will absorb the jump between it
        // and what was shown, if the set of measured LEDs changed.
        int geometryX[LED_COUNT], geometryY[LED_COUNT];
        geometryX[TOP_LEFT] = pointX[topLeftPoint]; geometryY[TOP_LEFT] = pointY[topLeftPoint];
        geometryX[TOP_RIGHT] = pointX[topRightPoint]; geometryY[TOP_RIGHT] = pointY[topRightPoint];
        geometryX[BOTTOM_LEFT] = pointX[bottomLeftPoint]; geometryY[BOTTOM_LEFT] = pointY[bottomLeftPoint];
        geometryX[BOTTOM_RIGHT] = pointX[bottomRightPoint]; geometryY[BOTTOM_RIGHT] = pointY[bottomRightPoint];

        if (!isGeometryValid(geometryX, geometryY)) {
            // The labelling produced a shape that isn't a convex rectangle. With 4 measured LEDs, one more
            // attempt is made with the simplest labelling there is: sort by height, and left to right
            // within each pair. Partial tracking and calibration are simply rejected instead.
            if (seenCount != 4 || calibrationMode)
                return;

            topLeftPoint = indicesSortedByY[0]; topRightPoint = indicesSortedByY[1];
            bottomLeftPoint = indicesSortedByY[2]; bottomRightPoint = indicesSortedByY[3];
            if (pointX[topLeftPoint] > pointX[topRightPoint]) { const uint8_t swapped = topLeftPoint; topLeftPoint = topRightPoint; topRightPoint = swapped; }
            if (pointX[bottomLeftPoint] > pointX[bottomRightPoint]) { const uint8_t swapped = bottomLeftPoint; bottomLeftPoint = bottomRightPoint; bottomRightPoint = swapped; }

            // Accept the fallback only if its axes agree with the layout: in a normal layout the rectangle is
            // taller than wide, in the wide one the opposite. The sign of (AB^2 + CD^2) - (AC^2 + BD^2) tells,
            // and it equals twice (B - C) dot (D - A), which avoids computing four lengths. The coordinate limit
            // keeps this signed 32-bit product safe.
            const int32_t topRightToBottomLeftX = pointX[topRightPoint] - pointX[bottomLeftPoint];
            const int32_t topRightToBottomLeftY = pointY[topRightPoint] - pointY[bottomLeftPoint];
            const int32_t topLeftToBottomRightX = pointX[bottomRightPoint] - pointX[topLeftPoint];
            const int32_t topLeftToBottomRightY = pointY[bottomRightPoint] - pointY[topLeftPoint];
            const int32_t axisScore = topRightToBottomLeftX * topLeftToBottomRightX + topRightToBottomLeftY * topLeftToBottomRightY;
            if (axisScore == 0 || (axisScore > 0) != wideLayout)
                return;

            geometryX[TOP_LEFT] = pointX[topLeftPoint]; geometryY[TOP_LEFT] = pointY[topLeftPoint];
            geometryX[TOP_RIGHT] = pointX[topRightPoint]; geometryY[TOP_RIGHT] = pointY[topRightPoint];
            geometryX[BOTTOM_LEFT] = pointX[bottomLeftPoint]; geometryY[BOTTOM_LEFT] = pointY[bottomLeftPoint];
            geometryX[BOTTOM_RIGHT] = pointX[bottomRightPoint]; geometryY[BOTTOM_RIGHT] = pointY[bottomRightPoint];
            if (!isGeometryValid(geometryX, geometryY))
                return;
        }

        // From here on, every path ends by publishing a valid geometry, smoothed or not. No rejection
        // may come after this point, or it would leave the spring state half updated.

        // Spring: when the set of measured LEDs changes, the new geometry rarely agrees with what was
        // being shown, and showing it at once would make the cursor jump. Instead, the shown geometry keeps
        // following the movement of the LEDs that stayed measured, and the difference to the new geometry
        // (the offset) is removed gradually over the next frames.
        if (seenCount >= 2 && previousSeenCount >= 1) {
            // Mean movement of the corners that were measured both in this frame and in the previous one.
            int totalMovementX = 0, totalMovementY = 0;
            uint8_t commonCount = 0;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                if ((previousMeasuredMask & slotMaskBit(slot)) && (measuredMask & slotMaskBit(slot))) {
                    totalMovementX += (geometryX[slot] - previousGeometryX[slot]);
                    totalMovementY += (geometryY[slot] - previousGeometryY[slot]);
                    commonCount++;
                }
            }

            int meanMovementX = 0, meanMovementY = 0;
            if (commonCount > 0) {
                meanMovementX = totalMovementX / commonCount;
                meanMovementY = totalMovementY / commonCount;
            } else if (previousSeenCount > 0) {
                // No corner stayed measured. Fall back on how the centre of the whole shape moved last time.
                meanMovementX = centerX - centerBeforePreviousX;
                meanMovementY = centerY - centerBeforePreviousY;
            }

            // A corner whose state changed (measured to rebuilt or back), and every rebuilt corner when the
            // number of measured LEDs changes, gets an offset: the position that continues the shown geometry
            // (shown position plus the mean movement), minus where the new geometry puts it.
            const bool seenCountChanged = (seenCount != previousSeenCount);
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                const bool wasMeasured = (previousMeasuredMask & slotMaskBit(slot)) != 0;
                const bool isMeasured = (measuredMask & slotMaskBit(slot)) != 0;

                if (isMeasured != wasMeasured || (!isMeasured && seenCountChanged)) {
                    const int continuedX = publishedX[slot] + meanMovementX;
                    const int continuedY = publishedY[slot] + meanMovementY;

                    springOffsetX[slot] = (float)(continuedX - geometryX[slot]);
                    springOffsetY[slot] = (float)(continuedY - geometryY[slot]);
                    springReleasesFast = seenCount > previousSeenCount;
                }
            }

            const int movementMagnitude = abs(meanMovementX) + abs(meanMovementY);
            // How much of the offset is removed this frame. By default: a small constant, or a share of the
            // movement if that is more (a moving gun hides the correction). Four-LED recovery below
            // replaces this with a better estimate of the movement.
            float consumption = fmaxf(FRAME_RATE_SCALE, (float)movementMagnitude * MOVEMENT_CONSUMPTION_FACTOR);

            // With 4 measured LEDs the offset is removed as a share of itself that grows with the movement,
            // from a slow minimum to a fast maximum: invisible while moving, gentle while still. With 1-3 LEDs
            // it stays at the default linear removal above (slow for large offsets), so that the shown
            // position doesn't chase a geometry that is only an estimate. The exception is right after LEDs were
            // added back: then the offset is removed as a share at the slowest rate, because there the old geometry was
            // the weaker estimate, and the new one is based on a real measurement.
            if (seenCount == 4 || springReleasesFast) {
                float largestOffset = 0.0f;
                for (uint8_t slot = 0; slot < LED_COUNT; ++slot) {
                    if (springOffsetX[slot] != 0.0f)
                        largestOffset = fmaxf(largestOffset, fabsf(springOffsetX[slot]));
                    if (springOffsetY[slot] != 0.0f)
                        largestOffset = fmaxf(largestOffset, fabsf(springOffsetY[slot]));
                }

                if (largestOffset > 0.0f) {
                    // Filtered movement of each corner, as a running average of its signed step. Averaging
                    // the signed steps before taking magnitudes cancels jitter (which changes direction) while
                    // keeping real motion such as rotation or zoom. It restarts from zero when the number of
                    // measured LEDs changes, so that old speed doesn't speed up a new, slow release.
                    int32_t motionSum = 0;
                    for (uint8_t slot = 0; slot < LED_COUNT; ++slot) {
                        if (previousSeenCount != seenCount)
                            filteredMotionX[slot] = filteredMotionY[slot] = 0;
                        if (previousMeasuredMask & slotMaskBit(slot)) {
                            const int32_t stepX = geometryX[slot] - previousGeometryX[slot];
                            const int32_t stepY = geometryY[slot] - previousGeometryY[slot];
                            filteredMotionX[slot] = (filteredMotionX[slot] + stepX) / 2;
                            filteredMotionY[slot] = (filteredMotionY[slot] + stepY) / 2;
                            motionSum += abs(filteredMotionX[slot]) + abs(filteredMotionY[slot]);
                        }
                    }

                    // Integer rounding here only affects this movement estimate, not the measured geometry
                    // or the fractional spring offsets.
                    const float movement = commonCount ?
                        (float)motionSum / (float)commonCount : (float)movementMagnitude;
                    const float scaledMotion = MOTION_GAIN * movement;
                    // Movement is compared with the rectangle's height, so this works at any distance.
                    const float normalizedHeight = fmaxf(rectangleHeight, 1.0f) * FRAME_RATE_SCALE;

                    float returnRate;
                    if (seenCount != 4 || scaledMotion <= normalizedHeight * MIN_RETURN_RATE)
                        returnRate = MIN_RETURN_RATE;
                    else if (scaledMotion >= normalizedHeight * MAX_RETURN_RATE)
                        returnRate = MAX_RETURN_RATE;
                    else
                        returnRate = scaledMotion / normalizedHeight;

                    consumption = fmaxf(0.5f * FRAME_RATE_SCALE,
                        fmaxf(movement * MOVEMENT_CONSUMPTION_FACTOR, largestOffset * returnRate));
                }
            }

            // All offsets shrink by the same factor, not by the same amount, so the shape of the correction is
            // preserved. Subtracting a fixed amount from each would zero the small ones first and deform the
            // rectangle while it settles, making the cursor jump.
            {
                float largestAbsoluteOffset = 0.0f;
                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    largestAbsoluteOffset = fmaxf(largestAbsoluteOffset, fabsf(springOffsetX[slot]));
                    largestAbsoluteOffset = fmaxf(largestAbsoluteOffset, fabsf(springOffsetY[slot]));
                }
                const float shrinkFactor = largestAbsoluteOffset > consumption ? 1.0f - consumption / largestAbsoluteOffset : 0.0f;
                for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                    springOffsetX[slot] *= shrinkFactor;
                    springOffsetY[slot] *= shrinkFactor;
                }
            }
        } else {
            // Nothing to compare with (first frame, or the detection just came back from the dark): show
            // the geometry as it is.
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                springOffsetX[slot] = 0.0f;
                springOffsetY[slot] = 0.0f;
            }
        }

        // Apply the remaining offset to this frame's geometry to get the geometry to publish.
        int candidatePublishedX[LED_COUNT], candidatePublishedY[LED_COUNT];
        bool geometryChanged = false;
        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            // The offsets are bounded combinations of coordinates that were validated above.
            const int roundedOffsetX = fastRound(springOffsetX[slot]);
            const int roundedOffsetY = fastRound(springOffsetY[slot]);
            candidatePublishedX[slot] = geometryX[slot] + roundedOffsetX;
            candidatePublishedY[slot] = geometryY[slot] + roundedOffsetY;
            geometryChanged |= roundedOffsetX != 0 || roundedOffsetY != 0;
        }
        // The geometry itself was validated. Only if the offsets changed it, it has to be checked again.
        if (geometryChanged && !isGeometryValid(candidatePublishedX, candidatePublishedY)) {
            // The measured or reconstructed geometry is valid, only the offsets made it unusable. Publish it without
            // the offsets this once. Keeping the old offsets instead could reject the same valid
            // 4-LED frame over and over, for as long as they stay.
            for (uint8_t slot = 0; slot < LED_COUNT; ++slot) {
                springOffsetX[slot] = 0.0f;
                springOffsetY[slot] = 0.0f;
                candidatePublishedX[slot] = geometryX[slot];
                candidatePublishedY[slot] = geometryY[slot];
            }
        }

        // Publish only now that the whole candidate geometry has passed validation, and remember this
        // frame's geometry (without the offsets) as the starting point for the next frame.
        for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
            publishedX[slot] = candidatePublishedX[slot];
            publishedY[slot] = candidatePublishedY[slot];

            previousGeometryX[slot] = geometryX[slot];
            previousGeometryY[slot] = geometryY[slot];
        }

        // The visibility history is only updated for accepted frames.
        for (uint8_t slot = 0; slot < LED_COUNT; slot++)
            seenHistory[slot] = (seenHistory[slot] << 1) | ((measuredMask & slotMaskBit(slot)) ? 1 : 0);

        // A geometry with all 4 LEDs measured is the best knowledge there is of the rectangle's shape, so it
        // becomes the anchor for rebuilding hidden LEDs later.
        if (seenCount == 4) {
            trackingStarted = true;
            for (uint8_t slot = 0; slot < LED_COUNT; slot++) {
                anchorX[slot] = geometryX[slot];
                anchorY[slot] = geometryY[slot];
            }
            anchorAvailable = true;
        }

        centerBeforePreviousX = centerX;
        centerBeforePreviousY = centerY;

        // Centre of the rectangle. The +2 before dividing by 4 rounds to the nearest integer instead of truncating.
        centerX = (publishedX[TOP_LEFT] + publishedX[TOP_RIGHT] + publishedX[BOTTOM_LEFT] + publishedX[BOTTOM_RIGHT] + 2) / 4;
        centerY = (publishedY[TOP_LEFT] + publishedY[TOP_RIGHT] + publishedY[BOTTOM_LEFT] + publishedY[BOTTOM_RIGHT] + 2) / 4;

        // The four side lengths, averaged into the rectangle's height and width. These are what calibration reads
        // (H() and W()), and what the 1- and 2-LED reconstructions use as the rectangle's size.
        const float leftDeltaX = (float)(publishedX[TOP_LEFT] - publishedX[BOTTOM_LEFT]);
        const float leftDeltaY = (float)(publishedY[TOP_LEFT] - publishedY[BOTTOM_LEFT]);
        const float leftSideLength = sqrtf(leftDeltaX * leftDeltaX + leftDeltaY * leftDeltaY);

        const float rightDeltaX = (float)(publishedX[TOP_RIGHT] - publishedX[BOTTOM_RIGHT]);
        const float rightDeltaY = (float)(publishedY[TOP_RIGHT] - publishedY[BOTTOM_RIGHT]);
        const float rightSideLength = sqrtf(rightDeltaX * rightDeltaX + rightDeltaY * rightDeltaY);

        const float topDeltaX = (float)(publishedX[TOP_LEFT] - publishedX[TOP_RIGHT]);
        const float topDeltaY = (float)(publishedY[TOP_LEFT] - publishedY[TOP_RIGHT]);
        const float topSideLength = sqrtf(topDeltaX * topDeltaX + topDeltaY * topDeltaY);

        const float bottomDeltaX = (float)(publishedX[BOTTOM_LEFT] - publishedX[BOTTOM_RIGHT]);
        const float bottomDeltaY = (float)(publishedY[BOTTOM_LEFT] - publishedY[BOTTOM_RIGHT]);
        const float bottomSideLength = sqrtf(bottomDeltaX * bottomDeltaX + bottomDeltaY * bottomDeltaY);

        rectangleHeight = (leftSideLength + rightSideLength) * 0.5f;
        rectangleWidth = (topSideLength + bottomSideLength) * 0.5f;

        // The measured proportion of the rectangle is only updated when all 4 LEDs are measured, as only
        // then it isn't influenced by a reconstruction.
        if (seenCount == 4) {
            if (rectangleHeight > 1e-6f) { idealAspectRatio = rectangleWidth / rectangleHeight; }
        }

        // Roll: the mean angle of the top and the bottom sides. Y points down in the image, hence the order of the
        // Y subtractions: it makes the angle follow the usual mathematical direction, positive when the right
        // side of the rectangle is higher than the left.
        rollAngle = (atan2f((float)(publishedY[TOP_LEFT] - publishedY[TOP_RIGHT]), (float)(publishedX[TOP_RIGHT] - publishedX[TOP_LEFT])) +
                     atan2f((float)(publishedY[BOTTOM_LEFT] - publishedY[BOTTOM_RIGHT]), (float)(publishedX[BOTTOM_RIGHT] - publishedX[BOTTOM_LEFT]))) * 0.5f;

        trackingStable = true;
        previousMeasuredMask = measuredMask;
        // After a frame with at least 3 LEDs, the dark counter restarts from zero. After one with 1-2 LEDs it is
        // set to "long dark", as a resume by continuity needs a firm frame to continue from.
        darkFrameCount = seenCount >= 3 ? 0 : MAX_FRAME_COUNT;
    }
    else {
        // Nothing seen. The tracker loses its footing, and counts the dark frames.
        trackingStable = false;
        previousMeasuredMask = 0;
        if (darkFrameCount < MAX_FRAME_COUNT) darkFrameCount++;
        // After the dark, the position of the gun relative to the screen may have changed, so the anchor
        // geometry can't be used to rebuild LEDs any more.
        anchorAvailable = false;
    }
    previousSeenCount = seenCount;
}

#endif // ADVANCED_TRACKING
