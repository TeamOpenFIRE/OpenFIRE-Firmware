/*!
 * @file OpenFIRE_Square_Advanced.h
 * @brief Advanced Light Gun library for 4 LED setup (Square layout)
 * @n Replaces OpenFIRE_Square on FPU-equipped chips, see OpenFIRETracking.h
 *
 * Differences from the classic tracking:
 *  - LED identification follows the previous frame (roll tracking), so tilting the gun
 *    doesn't swap LEDs, and wrong starting locks fix themselves.
 *  - Hidden LEDs are rebuilt from the last frame with all 4 LEDs (including perspective),
 *    not from stored side lengths and angles.
 *  - Switching between 1-4 visible LEDs is absorbed by a spring instead of jumping.
 *  - Candidate geometry is validated before it is published.
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

#ifndef _OpenFIRE_Square_Advanced_h_
#define _OpenFIRE_Square_Advanced_h_

#include <stdint.h>
#include "OpenFIREConst.h"

/// @details The four LEDs sit at the corners of a rectangle around the screen. They are tracked by "slot",
///          a fixed identity that doesn't depend on the order the camera reports them in:
///
///              slot 0 ---- slot 1        top left,    top right
///                |            |
///                |            |
///              slot 2 ---- slot 3        bottom left, bottom right
///
///          Normally the rectangle is taller than wide (its base is shorter than its height); in the "wide"
///          layout, for LEDs at the corners of the screen, the base is longer than the height.
///          The slots refer to the screen as the player sees it: X is mirrored from the camera's raw space.
///
///          The class name and the public methods are shared with the classic OpenFIRE_Square, so that
///          the rest of the firmware doesn't care which of the two is built.
class OpenFIRE_Square {
public:
    /// @brief Resets the tracker to the default geometry for the DFRobot camera.
    /// @note  Run when the camera is (re-)initialized, so no old history survives.
    void configure();

    /// @brief Updates the tracked geometry from the camera's blobs of the current frame.
    /// @param cameraXPositions X of each blob the camera reported, in camera pixels (4 entries)
    /// @param cameraYPositions Y of each blob the camera reported, in camera pixels (4 entries)
    /// @param ledSeenMask      Bit i is set if the camera's blob i is a real detection. Blobs are in
    ///                         arbitrary order, so they can't be assumed to match the slots.
    void begin(const int* cameraXPositions, const int* cameraYPositions, unsigned int ledSeenMask);

    /// @brief X of a slot (0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right), in mouse space
    int X(int index) const { return publishedX[index]; }

    /// @brief Y of a slot, in mouse space
    int Y(int index) const { return publishedY[index]; }

    /// @brief Recent visibility of a slot, one bit per frame with the newest in bit 0
    unsigned int testSee(int index) const { return seenHistory[index]; }

    /// @brief X of the rectangle's centre, in mouse space
    int testMedianX() const { return centerX; }

    /// @brief Y of the rectangle's centre, in mouse space
    int testMedianY() const { return centerY; }

    /// @brief Height of the rectangle: the mean length of its left and right sides
    float H() const { return rectangleHeight; }

    /// @brief Width of the rectangle: the mean length of its top and bottom sides
    float W() const { return rectangleWidth; }

    /// @brief Roll of the rectangle: the mean angle of its top and bottom sides
    float Ang() const { return rollAngle; }

    /// @brief Bit mask of the camera blobs that were real detections in the last frame
    unsigned int seen() const { return seenFlags; }

    /// @brief Calibration happens facing the screen with close to no roll, so the LEDs are simply sorted by height.
    void setCalibrationMode(bool enabled) { calibrationMode = enabled; }

    /// @brief Wide layout: the LEDs' base is longer than their height (e.g. LEDs at the screen corners).
    /// @note  Changing it drops the tracking history, since the LEDs are told apart differently in each layout.
    void setWideLayout(bool enabled) {
        if (wideLayout != enabled) {
            wideLayout = enabled;
            resetTracking();
        }
    }

private:
    // What the rest of the firmware reads: the tracked geometry with the spring correction applied.

    /// Position of each slot as shown to the user, in mouse space. Integers, since they are the
    /// virtual mouse pixels. They start at the default geometry, so the tracker is usable before
    /// the first full detection.
    int publishedX[4] = {SquareDefaultTLX * CamToMouseMult, SquareDefaultTRX * CamToMouseMult,
                         SquareDefaultTLX * CamToMouseMult, SquareDefaultTRX * CamToMouseMult};
    int publishedY[4] = {SquareDefaultTY * CamToMouseMult, SquareDefaultTY * CamToMouseMult,
                         SquareDefaultBY * CamToMouseMult, SquareDefaultBY * CamToMouseMult};

    int centerX = MouseMaxX / 2;
    int centerY = MouseMaxY / 2;

    /// One bit per frame for each slot, 1 if the slot was measured (not rebuilt) in that frame
    unsigned int seenHistory[4] = {0, 0, 0, 0};

    float rectangleHeight = (float)((SquareDefaultBY - SquareDefaultTY) * CamToMouseMult);
    float rectangleWidth = (float)((SquareDefaultTRX - SquareDefaultTLX) * CamToMouseMult);
    float rollAngle = 0.0f;

    /// Set once all 4 LEDs have been seen together. Until then the tracker stays on its defaults,
    /// as its first real measurements must come from a complete view.
    bool trackingStarted = false;

    /// Bit mask of the camera blobs that were real detections in the last frame
    unsigned int seenFlags = 0;

    // Tracking state carried from one frame to the next.

    /// false after a frame in which the tracker had lost its footing (nothing seen, or too little to
    /// start from). While false, fewer than 3 LEDs are not accepted, since from so little the tracker
    /// would be inventing the position.
    bool trackingStable = false;

    /// Last known real proportion of the rectangle (width / height). With only 2 LEDs visible it
    /// tells how far apart the hidden ones must be, and what the camera is seeing of the shape.
    float idealAspectRatio = IRSquareBaseHeightRatio;

    /// How many LEDs were measured in the previous accepted frame
    uint8_t previousSeenCount = 0;

    /// Which slots were measured in the previous accepted frame (see slotMaskBit() in the .cpp)
    uint8_t previousMeasuredMask = 0;

    /// Centre of the rectangle two frames ago. Together with the previous centre, it gives the last
    /// movement of the whole shape when none of the LEDs stayed measured to measure it from.
    int centerBeforePreviousX = 0;
    int centerBeforePreviousY = 0;

    /// Geometry of the previous accepted frame before the spring was applied: measured LEDs where
    /// there were any, rebuilt ones elsewhere.
    int previousGeometryX[4] = {0, 0, 0, 0};
    int previousGeometryY[4] = {0, 0, 0, 0};

    /// The last geometry in which all 4 LEDs were measured. It records the real shape of the
    /// rectangle as the camera sees it, perspective included, so LEDs that later become hidden can be
    /// rebuilt in that shape. Cleared when the camera sees nothing, since after that the gun may have moved.
    int anchorX[4] = {0, 0, 0, 0};
    int anchorY[4] = {0, 0, 0, 0};
    bool anchorAvailable = false;

    /// Frames since the labels were last taken from the heuristic (at the start, or on a resume with 3-4
    /// LEDs), counting up to 255. It tells how fresh and how uncertain the current lock still is.
    uint8_t framesSinceColdStart = 0;

    /// Consecutive frames without an accepted detection of 3-4 LEDs, since the last one. 255 means
    /// a long dark, or that the last accepted frame had only 1-2 LEDs. After a short dark the labels
    /// resume by continuity.
    uint8_t darkFrameCount = 255;

    // Spring: smooths the jump that happens when the set of measured LEDs changes.

    /// Difference between where a slot is shown and where the tracker now believes it is. It is
    /// created when the set of measured LEDs changes and then shrinks every frame. Float, so that
    /// amounts below one pixel are not rounded away before they have faded.
    float springOffsetX[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float springOffsetY[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    /// true if the current offset was created by the number of measured LEDs going up. The old geometry
    /// was only an estimate then, so the offset is released faster than after LEDs were lost.
    bool springReleasesFast = false;

    /// Smoothed frame-to-frame movement of each slot in whole mouse units, which sets how fast the
    /// spring is released while 4 LEDs are measured.
    int32_t filteredMotionX[4] = {0, 0, 0, 0};
    int32_t filteredMotionY[4] = {0, 0, 0, 0};

    bool calibrationMode = false;
    bool wideLayout = false;

    void resetTracking();
};

#endif // _OpenFIRE_Square_Advanced_h_
#endif // ADVANCED_TRACKING
