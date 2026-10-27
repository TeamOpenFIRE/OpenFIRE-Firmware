/*!
 * @file OpenFIRE_Diamond_Advanced.h
 * @brief Advanced Light Gun library for 4 LED setup (Diamond layout)
 * @n Replaces OpenFIRE_Diamond on FPU-equipped chips, see OpenFIRETracking.h
 *
 * Same public interface as OpenFIRE_Diamond, so OpenFIREcommon doesn't change.
 * Uses the same techniques as OpenFIRE_Square_Advanced: roll tracking by continuity,
 * rebuilding of hidden LEDs, and a spring that smooths changes in the visible LED count.
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

#ifndef _OpenFIRE_Diamond_Advanced_h_
#define _OpenFIRE_Diamond_Advanced_h_

#include <stdint.h>

#include "OpenFIREConst.h"

/// @details The four physical LEDs are tracked by "slot", a fixed identity that is independent of the order
///          the camera reports them in. Slots are numbered around the diamond, in the camera's raw space
///          (X not mirrored): 0 = top, 1 = maximum X, 2 = bottom, 3 = minimum X.
///          Going 0 -> 1 -> 2 -> 3 always walks around the diamond in the same direction, which is what
///          lets the tracker keep the identities straight while the gun is rolled.
///
///          The class name and the public methods are shared with the classic OpenFIRE_Diamond, so that
///          the rest of the firmware doesn't care which of the two is built.
class OpenFIRE_Diamond {
    public:
        /// @brief Resets the tracker to the default geometry for the DFRobot camera.
        /// @note  Run when the camera is (re-)initialized, so no history of the previous tracking survives.
        void configure();

        /// @brief Updates the tracked geometry from the camera's blobs of the current frame.
        /// @param cameraXPositions X of each blob the camera reported, in camera pixels (4 entries)
        /// @param cameraYPositions Y of each blob the camera reported, in camera pixels (4 entries)
        /// @param ledSeenMask      Bit i is set if the camera's blob i is a real detection. Blobs are in
        ///                         arbitrary order, so they can't be assumed to match the diamond's slots.
        void begin(const int* cameraXPositions, const int* cameraYPositions, unsigned int ledSeenMask);

        /// @brief X of an LED slot (0 = top, 1 = max X, 2 = bottom, 3 = min X), in mouse space
        int X(int index) const {
            return publishedX[index];
        }

        /// @brief Y of an LED slot, in mouse space
        int Y(int index) const {
            return publishedY[index];
        }

        /// @brief Recent visibility of an LED slot, one bit per frame with the newest in bit 0
        unsigned int testSee(int index) const {
            return seenHistory[index];
        }

        /// @brief X of the diamond's centre, in mouse space
        int testMedianX() const {
            return centerX;
        }

        /// @brief Y of the diamond's centre, in mouse space
        int testMedianY() const {
            return centerY;
        }

        /// @brief Height of the diamond (distance between the top and bottom LEDs)
        float H() const {
            return verticalDiagonalLength;
        }

        /// @brief Width of the diamond (distance between the left and right LEDs)
        float W() const {
            return horizontalDiagonalLength;
        }

        /// @brief Roll of the diamond, as the angle of its horizontal diagonal
        float Ang() const {
            return rollAngle;
        }

        /// @brief Bit mask of the camera blobs that were real detections in the last frame
        unsigned int seen() const {
            return seenFlags;
        }

    private:
        // What the rest of the firmware reads: the tracked geometry with the spring correction applied.

        /// One bit per frame for each slot, 1 if the slot was measured (not rebuilt) in that frame
        unsigned int seenHistory[4] = {0, 0, 0, 0};

        int centerX = MouseMaxX / 2;
        int centerY = MouseMaxY / 2;

        /// Position of each slot as shown to the user, in mouse space. It starts at the default
        /// geometry, so the tracker is usable before the first full detection.
        int publishedX[4] = {DiamondDefaultTopX * CamToMouseMult, DiamondDefaultRightX * CamToMouseMult,
                             DiamondDefaultBottomX * CamToMouseMult, DiamondDefaultLeftX * CamToMouseMult};
        int publishedY[4] = {DiamondDefaultTopY * CamToMouseMult, DiamondDefaultRightY * CamToMouseMult,
                             DiamondDefaultBottomY * CamToMouseMult, DiamondDefaultLeftY * CamToMouseMult};

        float rollAngle = 0.0f;
        float verticalDiagonalLength = 0.0f;
        float horizontalDiagonalLength = 0.0f;

        /// Set once all 4 LEDs have been seen together. Until then the tracker stays on its defaults,
        /// as identifying LEDs from fewer than 4 needs a trustworthy geometry to compare against.
        bool trackingStarted = false;

        /// Bit mask of the camera blobs that were real detections in the last frame
        unsigned int seenFlags = 0;

        // Tracking state carried from one frame to the next.

        /// Geometry of the previous accepted frame before the spring was applied: measured LEDs where
        /// there were any, rebuilt ones elsewhere. Rebuilding hidden LEDs and the spring both work
        /// from this, so it is the tracker's real belief about where the diamond is.
        float previousGeometryX[4] = {publishedX[0] + 0.0f, publishedX[1] + 0.0f, publishedX[2] + 0.0f,
                                      publishedX[3] + 0.0f};
        float previousGeometryY[4] = {publishedY[0] + 0.0f, publishedY[1] + 0.0f, publishedY[2] + 0.0f,
                                      publishedY[3] + 0.0f};

        /// How many LEDs were measured in the previous accepted frame (0 = the camera saw nothing)
        uint8_t previousSeenCount = 0;

        /// Frames spent without an accepted detection since the last one, saturating at 255. A short
        /// gap (a flickering LED, a hand passing by) lets the tracker resume where it was.
        uint8_t darkFrameCount = 255;

        /// Which slots were measured in the previous accepted frame (bit i = slot i)
        uint8_t previousMeasuredMask = 0;

        // Spring: smooths the jump that happens when the set of measured LEDs changes.

        /// Difference between where a slot is shown and where the tracker now believes it is. It is
        /// created when the set of measured LEDs changes and then shrinks a little every frame.
        float springOffsetX[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float springOffsetY[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        /// true if the current offset was created by LEDs becoming measured again. The old geometry
        /// was only an estimate then, so the offset is released faster than after LEDs were lost.
        bool springReleasesFast = false;

        /// Smoothed frame-to-frame movement of each slot, which sets how fast the spring is released
        float filteredMotionX[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float filteredMotionY[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

#endif  // _OpenFIRE_Diamond_Advanced_h_
#endif  // ADVANCED_TRACKING
