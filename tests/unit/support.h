// Helpers to build test scenes: LEDs at known places, and what the DFRobot camera would report for them.
//
// The camera model is flat (no perspective, no lens): the LEDs are given directly in camera pixels. That is
// enough to test how the trackers label, follow and rebuild LEDs, since all of that is decided by the points
// the camera reports. Perspective and lens behaviour belong in the perspective warp's tests.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <OpenFIREConst.h>

namespace scene {

struct Point {
    double x, y;
};

using Leds = std::array<Point, 4>;

// The camera's size in pixels, as the library sees it (CamMaxX is the last column).
constexpr double CAMERA_CENTER_X = (CamMaxX + 1) / 2.0;
constexpr double CAMERA_CENTER_Y = (CamMaxY + 1) / 2.0;

// Where a point ends up when the shape is rolled around the camera's centre or its own: positive is clockwise on
// the screen (image Y points down).
inline Point place(double centerX, double centerY, double offsetX, double offsetY, double rollDegrees) {
    const double radians = rollDegrees * M_PI / 180.0;
    return {centerX + offsetX * std::cos(radians) - offsetY * std::sin(radians),
            centerY + offsetX * std::sin(radians) + offsetY * std::cos(radians)};
}

// The four corners of a rectangle, in the order top left, top right, bottom left, bottom right (as they are
// before the roll: after it, "top left" is still the same physical LED).
inline Leds rectangle(double centerX, double centerY, double width, double height, double rollDegrees) {
    return {place(centerX, centerY, -width / 2, -height / 2, rollDegrees),
            place(centerX, centerY, +width / 2, -height / 2, rollDegrees),
            place(centerX, centerY, -width / 2, +height / 2, rollDegrees),
            place(centerX, centerY, +width / 2, +height / 2, rollDegrees)};
}

// The four corners of a rectangle seen by a pinhole camera, in the order top left, top right, bottom left, bottom
// right. Unlike rectangle(), which is flat, this has perspective: the rectangle is turned about the vertical axis
// by yawDegrees and about the horizontal one by pitchDegrees (a gun aimed from the side, from above or below),
// then the gun is rolled about the lens axis.
//  apparentWidth, apparentHeight: the size in camera pixels it would have seen head on (with no yaw or pitch)
inline Leds viewedRectangle(double centerX, double centerY, double apparentWidth, double apparentHeight,
                            double yawDegrees, double pitchDegrees, double rollDegrees) {
    // The camera's focal length in pixels is the library's (a field of view of 33 degrees over the image width);
    // the rectangle is placed at a distance where it has the apparent size, whatever that is.
    const double focalLength = 4096 * 0.5 / 0.29621 / CamToMouseMult;
    const double distance = 1000.0;
    const double width = apparentWidth * distance / focalLength, height = apparentHeight * distance / focalLength;
    const double yaw = yawDegrees * M_PI / 180.0, pitch = pitchDegrees * M_PI / 180.0;

    Leds corners;
    const double offsets[4][2] = {{-0.5, -0.5}, {0.5, -0.5}, {-0.5, 0.5}, {0.5, 0.5}};
    for (int i = 0; i < 4; i++) {
        double x = offsets[i][0] * width, y = offsets[i][1] * height, z = 0;
        // Turn about the vertical axis, then about the horizontal one.
        const double yawedX = x * std::cos(yaw) + z * std::sin(yaw), yawedZ = -x * std::sin(yaw) + z * std::cos(yaw);
        const double pitchedY = y * std::cos(pitch) - yawedZ * std::sin(pitch), pitchedZ = y * std::sin(pitch) + yawedZ * std::cos(pitch);
        const double depth = distance + pitchedZ;
        const double imageX = focalLength * yawedX / depth, imageY = focalLength * pitchedY / depth;
        corners[i] = place(centerX, centerY, imageX, imageY, rollDegrees);
    }
    return corners;
}

// The four LEDs of a diamond, in the order top, right, bottom, left.
inline Leds diamond(double centerX, double centerY, double width, double height, double rollDegrees) {
    return {place(centerX, centerY, 0, -height / 2, rollDegrees),
            place(centerX, centerY, +width / 2, 0, rollDegrees),
            place(centerX, centerY, 0, +height / 2, rollDegrees),
            place(centerX, centerY, -width / 2, 0, rollDegrees)};
}

// What a tracker is given each frame.
struct Blobs {
    int x[4];
    int y[4];
    unsigned mask;   // bit k is set if blob k holds a detection
};

constexpr unsigned ALL_LEDS = 0x0F;
constexpr int IN_ORDER[4] = {0, 1, 2, 3};

// What the camera reports for the LEDs.
//  visibleLeds: bit i is set if LED i is visible
//  ledOfBlob:   the LED that blob k holds, for the visible ones in turn: the camera reports its blobs in an order
//               that has nothing to do with the LEDs' places in the layout
//  mirrorX:     the Square tracker expects the camera's raw image, mirrored left to right; the Diamond doesn't
// Positions are rounded to whole camera pixels, as a real camera reports them.
inline Blobs camera(const Leds& leds, unsigned visibleLeds = ALL_LEDS, const int* ledOfBlob = IN_ORDER, bool mirrorX = false) {
    Blobs blobs;
    for (int k = 0; k < 4; k++) {
        blobs.x[k] = CamMaxX;   // an empty blob, as the camera reports it
        blobs.y[k] = CamMaxY;
    }
    blobs.mask = 0;
    int used = 0;
    for (int k = 0; k < 4; k++) {
        const int led = ledOfBlob[k];
        if (!(visibleLeds & (1u << led))) continue;
        const int pixelX = (int)std::lround(leds[led].x);
        blobs.x[used] = mirrorX ? CamMaxX - pixelX : pixelX;
        blobs.y[used] = (int)std::lround(leds[led].y);
        blobs.mask |= 1u << used;
        used++;
    }
    return blobs;
}

// The position in mouse space of a camera pixel: the unit of the trackers' output.
inline int mouse(double cameraPixel) {
    return (int)std::lround(cameraPixel) * CamToMouseMult;
}

// Calls the function with every order in which the camera could report four blobs (all 24 of them).
template <class Function>
void forEachBlobOrder(Function function) {
    int order[4] = {0, 1, 2, 3};
    do {
        function(order);
    } while (std::next_permutation(order, order + 4));
}

// The checks below read a tracker's output, which has the same interface for the Square and the Diamond:
// X(slot) and Y(slot) in mouse space. LED i of a scene is expected in slot i unless said otherwise.

// The largest distance, over the four slots and both axes, between where the tracker has a slot and where
// the scene has its LED, in mouse units.
template <class Tracker>
int worstError(const Tracker& tracker, const Leds& leds) {
    int worst = 0;
    for (int slot = 0; slot < 4; slot++) {
        worst = std::max(worst, std::abs(tracker.X(slot) - mouse(leds[slot].x)));
        worst = std::max(worst, std::abs(tracker.Y(slot) - mouse(leds[slot].y)));
    }
    return worst;
}

// Whether every slot holds its LED, within the tolerance (mouse units).
template <class Tracker>
bool slotsHoldTheirLeds(const Tracker& tracker, const Leds& leds, int tolerance = 0) {
    return worstError(tracker, leds) <= tolerance;
}

// Whether the tracker's four points are exactly the scene's four LEDs, in whatever slots.
template <class Tracker>
bool publishesTheLeds(const Tracker& tracker, const Leds& leds) {
    bool taken[4] = {false, false, false, false};
    for (int slot = 0; slot < 4; slot++) {
        bool found = false;
        for (int led = 0; led < 4 && !found; led++) {
            if (!taken[led] && tracker.X(slot) == mouse(leds[led].x) && tracker.Y(slot) == mouse(leds[led].y)) {
                taken[led] = true;
                found = true;
            }
        }
        if (!found) return false;
    }
    return true;
}

// Whether walking the four slots in the given order goes round the shape clockwise on the screen, turning the same
// way at every corner. A camera can't produce a mirror image, so a tracker must never publish one.
template <class Tracker>
bool perimeterIsClockwise(const Tracker& tracker, const int (&perimeter)[4]) {
    for (int i = 0; i < 4; i++) {
        const int a = perimeter[i], b = perimeter[(i + 1) % 4], c = perimeter[(i + 2) % 4];
        const double turn = (double)(tracker.X(b) - tracker.X(a)) * (tracker.Y(c) - tracker.Y(b)) -
                            (double)(tracker.Y(b) - tracker.Y(a)) * (tracker.X(c) - tracker.X(b));
        if (!(turn > 0)) return false;
    }
    return true;
}

// The order of the slots around the shape: the Square's slots are numbered row by row, the Diamond's round the shape.
constexpr int SQUARE_PERIMETER[4] = {0, 1, 3, 2};
constexpr int DIAMOND_PERIMETER[4] = {0, 1, 2, 3};

} // namespace scene
