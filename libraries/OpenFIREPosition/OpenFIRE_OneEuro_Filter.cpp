/*!
 * @file OpenFIRE_OneEuro_Filter.cpp
 * @brief 1€ filter for the 4 LED positions
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

#include "OpenFIRE_OneEuro_Filter.h"

void OpenFIRE_OneEuro_Filter::process(const int* inputX, const int* inputY, float* outputX, float* outputY) {
    // The filter's maths depends on how much time really passed between two frames, which varies a little
    // (USB and I2C traffic delay the camera reads), so it is measured instead of assumed to be 1 / 209 s.
    // Subtracting as unsigned numbers gives the right answer even when micros() has wrapped around
    // since the last frame (it does so about every 71 minutes).
    const unsigned long currentMicros = micros();
    float deltaTime = ((float)(currentMicros - this->lastMicros)) * MICROSECONDS_TO_SECONDS;
    this->lastMicros = currentMicros;

    // A zero or negative time would turn the divisions below into infinities, which would then
    // spread through the filter. Assume a regular frame instead.
    if (deltaTime <= 0.0f) {
        deltaTime = FALLBACK_DELTA_TIME;
    }

    // After a long pause the stored positions are stale: the speed between them and the new ones
    // would be enormous and meaningless. Start over instead of computing across the gap.
    if (deltaTime > MAX_FRAME_GAP) {
        this->initialized = false;
    }

    // The first frame has no history to filter with, so it becomes the starting point and is
    // passed through unchanged. Speeds start at zero because nothing is known about them yet.
    if (!this->initialized) {
        for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
            states[ledIndex].previousInputX = states[ledIndex].filteredX = (float)inputX[ledIndex];
            states[ledIndex].previousInputY = states[ledIndex].filteredY = (float)inputY[ledIndex];
            states[ledIndex].filteredVelocityX = states[ledIndex].filteredVelocityY = 0.0f;
            outputX[ledIndex] = states[ledIndex].filteredX;
            outputY[ledIndex] = states[ledIndex].filteredY;
        }
        this->initialized = true;
        return;
    }

    // The smoothing factors depend on the cutoff and on the frame time only through their product with
    // 2 * pi, so that product is computed once here for all of them.
    const float deltaTimeTwoPi = deltaTime * RADIANS_PER_CYCLE;

    // Weights for the speed estimate: slow smoothing while the speed is steady, fast smoothing when it
    // changes abruptly (see the header). Which one to use is decided per LED and axis below.
    const float slowVelocityFactor = smoothingFactor(DERIVATIVE_CUTOFF_BASE, deltaTimeTwoPi);
    const float fastVelocityFactor = smoothingFactor(DERIVATIVE_CUTOFF_SNAP, deltaTimeTwoPi);

    // Speed is a distance divided by the frame time. Multiplying by the inverse is faster than dividing
    // for each of the eight values (four LEDs, two axes) computed below.
    const float inverseDeltaTime = 1.0f / deltaTime;

    // How much to trust the camera's measurements this frame, from the LED that is the most reliable.
    // LEDs near the edge of the image are treated as less reliable, since that is where the lens distorts
    // the most. The four LEDs move as one rigid shape, so a single reliable LED is enough to tell real
    // movement from noise, and the one nearest to the image centre decides.
    // 1 means "fully reliable" (at the centre), down to MIN_EDGE_ATTENUATION (at the edge).
    float bestEdgeAttenuation = MIN_EDGE_ATTENUATION;

    for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
        // LEDs outside the image are not measured but reconstructed by the tracker, so their distance
        // from the centre says nothing about measurement noise: they are left out of the choice.
        if (inputX[ledIndex] >= 0 && inputX[ledIndex] <= MouseMaxX && inputY[ledIndex] >= 0 && inputY[ledIndex] <= MouseMaxY) {
            // Distance from the image centre on each axis, scaled so that the image edge is at 1.
            const float normalizedDistanceX = fabsf((float)inputX[ledIndex] - ((float)MouseMaxX * 0.5f)) * INVERSE_HALF_WIDTH;
            const float normalizedDistanceY = fabsf((float)inputY[ledIndex] - ((float)MouseMaxY * 0.5f)) * INVERSE_HALF_HEIGHT;

            // Using the squared distance needs no square root, and makes the attenuation fall slowly
            // near the centre and faster towards the edge.
            const float squaredDistanceFromCenter =
                (normalizedDistanceX * normalizedDistanceX) + (normalizedDistanceY * normalizedDistanceY);
            float edgeAttenuation = 1.0f - (squaredDistanceFromCenter * EDGE_ATTENUATION_SLOPE);

            if (edgeAttenuation < MIN_EDGE_ATTENUATION) {
                edgeAttenuation = MIN_EDGE_ATTENUATION;
            }

            if (edgeAttenuation > bestEdgeAttenuation) {
                bestEdgeAttenuation = edgeAttenuation;
            }
        }
    }

    // Near the edges the filter is made less eager, in two ways:
    //  - beta is scaled down, so a given speed raises the cutoff less (more smoothing);
    //  - the speed-change threshold for "snapping" is raised, since the speed estimate is noisier there
    //    and bigger jumps in it happen without any real movement.
    const float adaptiveBeta = BETA_BASE * bestEdgeAttenuation;

    const float edgeFactor = 1.0f - bestEdgeAttenuation;
    const float dynamicSnap = SNAP_BASE + (SNAP_EDGE * edgeFactor);

    // Range of speed change over which the speed estimate blends from slow to fast smoothing, and the
    // inverse of its width, so that the blend needs no division per LED.
    const float lowerBound = dynamicSnap * SNAP_BLEND_LOWER;
    const float upperBound = dynamicSnap * SNAP_BLEND_UPPER;
    const float inverseBlendRange = 1.0f / (dynamicSnap * SNAP_BLEND_WIDTH);

    // Update every LED's speed estimate, and find the fastest speed on each axis. That fastest speed sets the cutoff
    // for all LEDs below, which keeps the four of them smoothed identically (see the header).
    float maxAbsoluteVelocityX = 0.0f;
    float maxAbsoluteVelocityY = 0.0f;

    for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
        // Raw speed: how far the LED moved since the last frame, per second. It is noisy, as
        // it is the difference of two measurements, which is why it is smoothed before use.
        const float rawVelocityX = ((float)inputX[ledIndex] - states[ledIndex].previousInputX) * inverseDeltaTime;

        // How different the new raw speed is from the smoothed one tells whether this is noise (small
        // difference: keep smoothing slowly) or a real, abrupt change of movement (large: follow it quickly).
        const float velocityChangeX = fabsf(rawVelocityX - states[ledIndex].filteredVelocityX);
        const float velocityFactorX = velocitySmoothingFactor(velocityChangeX, lowerBound, upperBound, inverseBlendRange,
                                                              slowVelocityFactor, fastVelocityFactor);
        states[ledIndex].filteredVelocityX += velocityFactorX * (rawVelocityX - states[ledIndex].filteredVelocityX);

        const float absoluteVelocityX = fabsf(states[ledIndex].filteredVelocityX);
        if (absoluteVelocityX > maxAbsoluteVelocityX) {
            maxAbsoluteVelocityX = absoluteVelocityX;
        }

        // Same for the Y axis.
        const float rawVelocityY = ((float)inputY[ledIndex] - states[ledIndex].previousInputY) * inverseDeltaTime;
        const float velocityChangeY = fabsf(rawVelocityY - states[ledIndex].filteredVelocityY);
        const float velocityFactorY = velocitySmoothingFactor(velocityChangeY, lowerBound, upperBound, inverseBlendRange,
                                                              slowVelocityFactor, fastVelocityFactor);
        states[ledIndex].filteredVelocityY += velocityFactorY * (rawVelocityY - states[ledIndex].filteredVelocityY);

        const float absoluteVelocityY = fabsf(states[ledIndex].filteredVelocityY);
        if (absoluteVelocityY > maxAbsoluteVelocityY) {
            maxAbsoluteVelocityY = absoluteVelocityY;
        }
    }

    // The core rule of the 1€ filter: the cutoff rises linearly with the speed, from MIN_CUTOFF when the LEDs are
    // still, up to MAX_CUTOFF. A higher cutoff means a larger weight for the newest measurement.
    float cutoffFrequencyX = MIN_CUTOFF + adaptiveBeta * maxAbsoluteVelocityX;
    if (cutoffFrequencyX > MAX_CUTOFF) {
        cutoffFrequencyX = MAX_CUTOFF;
    }
    const float positionFactorX = smoothingFactor(cutoffFrequencyX, deltaTimeTwoPi);

    float cutoffFrequencyY = MIN_CUTOFF + adaptiveBeta * maxAbsoluteVelocityY;
    if (cutoffFrequencyY > MAX_CUTOFF) {
        cutoffFrequencyY = MAX_CUTOFF;
    }
    const float positionFactorY = smoothingFactor(cutoffFrequencyY, deltaTimeTwoPi);

    // Smooth all four LEDs with the same two weights.
    for (int ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
        // Whether the raw position is exactly the one of the last frame. That has to be read before
        // previousInput is overwritten below, and decides if the micro-snap may be used.
        const bool inputXUnchanged = ((float)inputX[ledIndex] == states[ledIndex].previousInputX);
        const bool inputYUnchanged = ((float)inputY[ledIndex] == states[ledIndex].previousInputY);

        // Exponential moving average: move the smoothed position a share of the way to the new measurement.
        states[ledIndex].filteredX += positionFactorX * ((float)inputX[ledIndex] - states[ledIndex].filteredX);
        states[ledIndex].previousInputX = (float)inputX[ledIndex];

        states[ledIndex].filteredY += positionFactorY * ((float)inputY[ledIndex] - states[ledIndex].filteredY);
        states[ledIndex].previousInputY = (float)inputY[ledIndex];

        // Micro-snap: once the input has stopped changing, finish the approach to it. The average alone only
        // gets closer and closer without arriving, so the output would keep moving by tiny amounts that
        // then show up as a drifting cursor. Only done when the input is unchanged: a moving input must
        // stay smoothed.
        if (inputXUnchanged && fabsf((float)inputX[ledIndex] - states[ledIndex].filteredX) < MICRO_SNAP_THRESHOLD) {
            states[ledIndex].filteredX = (float)inputX[ledIndex];
        }
        if (inputYUnchanged && fabsf((float)inputY[ledIndex] - states[ledIndex].filteredY) < MICRO_SNAP_THRESHOLD) {
            states[ledIndex].filteredY = (float)inputY[ledIndex];
        }

        // Safety net. A not-a-number value never goes away on its own, since every later calculation
        // with it gives not-a-number again, and the cursor would be stuck for good. Restart this LED
        // from the measurement instead.
        if (isnan(states[ledIndex].filteredX) || isnan(states[ledIndex].filteredY)) {
            states[ledIndex].filteredX = (float)inputX[ledIndex];
            states[ledIndex].filteredY = (float)inputY[ledIndex];
            states[ledIndex].filteredVelocityX = 0.0f;
            states[ledIndex].filteredVelocityY = 0.0f;
        }

        outputX[ledIndex] = states[ledIndex].filteredX;
        outputY[ledIndex] = states[ledIndex].filteredY;
    }
}
