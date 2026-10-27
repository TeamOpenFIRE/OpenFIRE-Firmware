/*!
 * @file OpenFIRE_OneEuro_Filter.h
 * @brief 1€ filter for the 4 LED positions
 * @n Smooths the LED positions before the perspective warp, so the quadrilateral moves as a
 *    rigid body: a single smoothing strength per axis, driven by the fastest LED.
 *
 * Adaptive low-pass filter (Casiez et al., "1€ filter", https://github.com/casiez/OneEuroFilter): strong smoothing when
 * the LEDs are still or slow (where jitter is visible), almost none when they move fast (where lag would be visible).
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
 */

#ifndef _OpenFIRE_OneEuro_Filter_h_
#define _OpenFIRE_OneEuro_Filter_h_

#include <Arduino.h>

#include "OpenFIREConst.h"

/// @details How the filter works, in short.
///
///          A plain low-pass filter has a fixed cutoff frequency, which forces a choice: a low cutoff removes
///          the camera's jitter but makes the aim lag behind fast movements, a high cutoff follows fast
///          movements but lets the jitter through. The 1€ filter avoids the choice by moving the cutoff with
///          the speed: the faster the LEDs move, the higher the cutoff, so slow aiming is smooth and a fast
///          flick isn't delayed.
///
///          This version differs from the textbook one in a few ways, all to suit four LEDs seen by a camera:
///           - The four LEDs are one rigid shape, so all of them are filtered with the same cutoff,
///             set by the fastest one. Smoothing them differently would bend the shape, and the perspective
///             calculation downstream turns a small bend of the shape into a visible aim error.
///           - The speed estimate is itself smoothed, but quickly again when the speed changes abruptly,
///             so that sudden starts and stops are noticed at once instead of one second later.
///           - LEDs near the edge of the camera image are noisier, so the filter becomes less eager to
///             follow them there.
class OpenFIRE_OneEuro_Filter {
    public:
        /// @brief Smooths the positions of the four LEDs for one camera frame.
        /// @param inputX,inputY   Positions of the 4 LEDs from the tracker, in mouse space
        /// @param outputX,outputY Filtered positions of the 4 LEDs, with sub-pixel precision
        void process(const int* inputX, const int* inputY, float* outputX, float* outputY);

        /// @brief Forgets the history: the next frame passes through unfiltered.
        /// @note  Needed when the meaning of the four LED slots changes (e.g. Square <-> Diamond), since the
        ///        stored positions and speeds would then belong to different LEDs.
        void reset() {
            this->initialized = false;
        }

    private:
        static constexpr int LED_COUNT = 4;

        /// Everything the filter remembers about one LED. Kept together so that one LED's state sits in one place in memory.
        struct FilterState {
                float previousInputX;  ///< raw X of the previous frame, to measure how far the LED moved
                float filteredX;       ///< smoothed X, which is the filter's output
                float previousInputY;
                float filteredY;
                float filteredVelocityX;  ///< smoothed speed in X, in mouse units per second
                float filteredVelocityY;
        };

        FilterState states[LED_COUNT] = {};

        /// Time of the previous call, from micros(), to measure how long a frame really took.
        unsigned long lastMicros = 0;

        /// false until a first frame has been taken as the starting point
        bool initialized = false;

        // Tuning of the filter. The values were found by trying them on a DFRobot camera, and are
        // inherited from the ESP32 port of this firmware; the derivation of the numbers beyond
        // that tuning isn't known.

        /// Lowest cutoff frequency in Hz, used when the LEDs are still: the strongest smoothing.
        /// Lower is steadier but laggier. 0.1 was found to be the balance: 0.2 felt slightly slippery
        /// and 0.05 too stiff.
        static constexpr float MIN_CUTOFF = 0.1f;

        /// Highest cutoff frequency in Hz, so that even the fastest movement still gets a little smoothing
        /// and the filter never becomes a plain pass-through.
        static constexpr float MAX_CUTOFF = 30.0f;

        /// Cutoff frequency in Hz of the speed estimate. It is low in normal conditions (DERIVATIVE_CUTOFF_BASE),
        /// since the speed is computed from the difference of two noisy positions and would otherwise be
        /// mostly noise. It is high (DERIVATIVE_CUTOFF_SNAP) when the speed changes abruptly, so that
        /// flicks and sudden stops are followed immediately.
        static constexpr float DERIVATIVE_CUTOFF_BASE = 1.0f;
        static constexpr float DERIVATIVE_CUTOFF_SNAP = 25.0f;

        /// Size of one pixel of the DFRobot's sensor in mouse units: the sensor is 128 pixels wide
        /// (and 96 tall) and the mouse space 4096 wide (and 3072 tall), so 32 on both axes. The values
        /// below are written in sensor pixels so they stay meaningful if the mouse space is rescaled.
        static constexpr float SENSOR_PIXEL_SIZE = (float)MouseResX / 128.0f;

        /// How much the speed must change from one frame to the next, in mouse units per second, before
        /// the speed estimate stops being smoothed slowly and "snaps" to the new speed. SNAP_BASE applies at the
        /// centre of the image; SNAP_EDGE is added towards the edge, where noise makes the measured speed
        /// jumpier and a larger change is needed to count as a real one.
        static constexpr float SNAP_BASE = 31.25f * SENSOR_PIXEL_SIZE;
        static constexpr float SNAP_EDGE = 62.5f * SENSOR_PIXEL_SIZE;

        /// How strongly the cutoff grows with speed (the "beta" of the 1€ filter), at the centre of the
        /// image. It is reduced near the edges, see process(). Divided by the pixel size because speeds are
        /// in mouse units, so that the product with a speed doesn't depend on the mouse space's scale.
        static constexpr float BETA_BASE = 0.2464f / SENSOR_PIXEL_SIZE;

        /// The input is made of whole numbers, and a smoothed value only approaches a constant input
        /// gradually, never quite reaching it. When the input hasn't changed and the smoothed value is
        /// within this distance of it (half a unit, the rounding step of the input), it is set to
        /// the input exactly. Otherwise the output would keep creeping by tiny amounts for a long time.
        static constexpr float MICRO_SNAP_THRESHOLD = 0.5f;

        /// Lowest value the edge attenuation can have (see process()), so that even the most
        /// peripheral LEDs keep some responsiveness.
        static constexpr float MIN_EDGE_ATTENUATION = 0.20f;

        /// How fast the edge attenuation falls with the squared distance from the image centre. With 0.8
        /// it reaches the minimum 0.2 at the middle of an image edge, so every point further out
        /// (towards the corners) is held at that minimum.
        static constexpr float EDGE_ATTENUATION_SLOPE = 0.8f;

        /// The speed estimate blends from slow to fast smoothing over this range of speed change, as a
        /// fraction of the snap threshold: fully slow below 0.8 times it, fully fast above 1.2 times it, a smooth
        /// transition in between. A hard switch at one threshold would make the aim stutter when the speed
        /// change hovers around it. SNAP_BLEND_WIDTH is the width of the range, 1.2 - 0.8.
        static constexpr float SNAP_BLEND_LOWER = 0.8f;
        static constexpr float SNAP_BLEND_UPPER = 1.2f;
        static constexpr float SNAP_BLEND_WIDTH = 0.4f;

        /// Longest time in seconds between two frames for the history to still be meaningful. After a
        /// longer pause (tracking lost, another run mode in use, the pause menu) the old positions
        /// would turn into an absurd speed, so the filter restarts instead.
        static constexpr float MAX_FRAME_GAP = 0.1f;

        static constexpr float MICROSECONDS_TO_SECONDS = 0.000001f;

        /// Frame time to assume if the clock gives a zero or negative one, which would break divisions.
        static constexpr float FALLBACK_DELTA_TIME = 1.0f / (float)CamFps;

        /// Reciprocals of half of the mouse space's size: dividing the distance from the centre by them gives
        /// -1 to 1 across the image, without a division per LED and frame.
        static constexpr float INVERSE_HALF_WIDTH = 1.0f / ((float)MouseMaxX * 0.5f);
        static constexpr float INVERSE_HALF_HEIGHT = 1.0f / ((float)MouseMaxY * 0.5f);

        /// Radians in one full cycle (2 * pi), to turn a frequency in Hz into an angular frequency. (Not named TWO_PI,
        /// which Arduino already defines as a macro.)
        static constexpr float RADIANS_PER_CYCLE = 6.28318530718f;

        /// The weight given to a new sample by an exponential moving average, for a given cutoff frequency
        /// and frame time. Close to 1 follows the input almost directly, close to 0 smooths heavily.
        /// It comes from the first-order low-pass filter: with time constant tau = 1 / (2 * pi * cutoff),
        /// the weight is frame time / (frame time + tau), which is rewritten below to avoid dividing twice.
        /// @param cutoffFrequency   Cutoff in Hz
        /// @param deltaTimeTwoPi    Frame time in seconds multiplied by 2 * pi, which the caller computes once per frame
        static inline float smoothingFactor(float cutoffFrequency, float deltaTimeTwoPi) {
            const float cutoffTimesDeltaTime = cutoffFrequency * deltaTimeTwoPi;
            return cutoffTimesDeltaTime / (cutoffTimesDeltaTime + 1.0f);
        }

        /// The weight given to a new speed sample when updating the speed estimate of one axis of one LED:
        /// the slow weight when the speed is steady, the fast one when it changes abruptly, and a linear
        /// blend between the two while the change is within the blend range.
        /// @param speedChange       How far the new raw speed is from the current estimate, in mouse units per second
        /// @param lowerBound        Speed change below which only the slow weight is used
        /// @param upperBound        Speed change above which only the fast weight is used
        /// @param inverseBlendRange 1 / (upperBound - lowerBound), computed once per frame
        static inline float velocitySmoothingFactor(float speedChange, float lowerBound, float upperBound, float inverseBlendRange,
                                                    float slowFactor, float fastFactor) {
            if (speedChange <= lowerBound) {
                return slowFactor;
            }
            if (speedChange >= upperBound) {
                return fastFactor;
            }
            return slowFactor + ((speedChange - lowerBound) * inverseBlendRange) * (fastFactor - slowFactor);
        }
};

#endif  // _OpenFIRE_OneEuro_Filter_h_
