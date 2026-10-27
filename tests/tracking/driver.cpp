// Test driver for the tracking (OpenFIRE_*_Advanced and the 1€ filter).
//
// It simulates a light gun moving in front of the screen: a pinhole camera at a random-walking position, aiming
// at a random-walking point, rolling (up to a full turn, with sudden wrist flips), and losing LEDs in every
// pattern: single LEDs flickering, pairs hidden for a while, and complete blackouts. Each frame is fed to the
// trackers, the 1€ filter and the perspective warp, and every output is printed.
//
// The output is fully deterministic (fixed random seed, simulated clock), so run.sh can compare it byte for byte
// with what was recorded earlier. See README.md.
//
// Arguments: layout (0 square, 1 diamond), wide (square only), frames, seed, calibration frames,
//            maximum roll speed in degrees per frame, average frames between blackouts.

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <Arduino.h>
unsigned long testMicros = 0;

#include <OpenFIRE_Square_Advanced.h>
#include <OpenFIRE_Diamond_Advanced.h>
#include <OpenFIRE_Perspective_Advanced.h>

#include <OpenFIRE_OneEuro_Filter.h>

// xorshift64: small, fast, and identical on every platform, unlike rand().
static uint64_t randomState = 88172645463325252ull;
static uint32_t randomNumber() {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 7;
    randomState ^= randomState << 17;
    return (uint32_t)(randomState >> 11);
}
static float randomUnit() { return (randomNumber() & 0xFFFFFF) / 16777216.0f; }
static float randomSigned(float amplitude) { return (randomUnit() * 2.0f - 1.0f) * amplitude; }

struct Vector3 { double x, y, z; };
static Vector3 subtract(Vector3 a, Vector3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static double dot(Vector3 a, Vector3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static Vector3 cross(Vector3 a, Vector3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static Vector3 normalize(Vector3 a) { const double length = std::sqrt(dot(a, a)); return {a.x / length, a.y / length, a.z / length}; }

int main(int argc, char** argv) {
    const int layout = argc > 1 ? atoi(argv[1]) : 0;
    const int wide = argc > 2 ? atoi(argv[2]) : 0;
    const int frames = argc > 3 ? atoi(argv[3]) : 60000;
    const int seed = argc > 4 ? atoi(argv[4]) : 1;
    const int calibrationFrames = argc > 5 ? atoi(argv[5]) : 0;
    const double maxRollSpeed = argc > 6 ? atof(argv[6]) : 0.4;
    const int framesPerBlackout = argc > 7 ? atoi(argv[7]) : 1500;
    randomState ^= (uint64_t)seed * 0x9E3779B97F4A7C15ull;
    for (int i = 0; i < 10; i++) randomNumber();

    // Static storage, like the firmware's objects: zero-initialized before any constructor runs.
    static OpenFIRE_Square squareTracker;
    static OpenFIRE_Diamond diamondTracker;
    static OpenFIRE_Perspective rawWarp, filteredWarp;
    static OpenFIRE_OneEuro_Filter filter;
    squareTracker.configure();
    diamondTracker.configure();
    if (wide) squareTracker.setWideLayout(true);
    squareTracker.setCalibrationMode(calibrationFrames > 0);

    // LED positions in the screen plane, with the screen 1 unit tall.
    Vector3 leds[4];
    if (layout == 0) {
        const double height = wide ? 0.55 : 1.0, width = wide ? 1.9 : 0.711;
        leds[0] = {-width / 2, +height / 2, 0}; leds[1] = {+width / 2, +height / 2, 0};
        leds[2] = {-width / 2, -height / 2, 0}; leds[3] = {+width / 2, -height / 2, 0};
    } else {
        const double height = 1.0, width = 1.78;
        leds[0] = {0, +height / 2, 0};  leds[1] = {+width / 2, 0, 0};   // top, right
        leds[2] = {0, -height / 2, 0};  leds[3] = {-width / 2, 0, 0};   // bottom, left
    }
    // Focal length of the DFRobot in camera pixels: half the 1024 pixel width over tan(33 deg / 2).
    const double focalPixels = 512.0 / std::tan(16.5 * M_PI / 180.0);

    double gunX = 0, gunY = 0, gunZ = 2.2, aimX = 0, aimY = 0, roll = 0, rollSpeed = 0;
    int hiddenFrames[4] = {0, 0, 0, 0};
    int blackoutFrames = 0;
    int blobSlotOrder[4] = {0, 1, 2, 3};
    const int ledsLeftX = 500 << 2, ledsRightX = 1420 << 2, screenWidth = 1920 << 2, screenHeight = 1080 << 2;
    rawWarp.source(512 << 2, 384 << 2); filteredWarp.source(512 << 2, 384 << 2);
    rawWarp.deinit(0); filteredWarp.deinit(0);

    for (int frame = 0; frame < frames; frame++) {
        if (calibrationFrames && frame == calibrationFrames) squareTracker.setCalibrationMode(false);
        testMicros += 4785 + (randomNumber() % 7 == 0 ? (randomNumber() % 3000) : 0);
        if (randomNumber() % 4000 == 0) testMicros += 150000;   // a stall long enough for the filter to restart

        // The gun's position, aim and roll take a random walk, kept within a living room.
        gunX += randomSigned(0.01f); gunY += randomSigned(0.006f); gunZ += randomSigned(0.012f);
        gunX = std::fmax(-1.8, std::fmin(1.8, gunX)); gunY = std::fmax(-0.6, std::fmin(0.6, gunY));
        gunZ = std::fmax(1.0, std::fmin(4.0, gunZ));
        aimX += randomSigned(0.03f); aimY += randomSigned(0.02f);
        aimX = std::fmax(-1.1, std::fmin(1.1, aimX)); aimY = std::fmax(-0.7, std::fmin(0.7, aimY));
        if (randomNumber() % 900 == 0) rollSpeed = randomSigned((float)maxRollSpeed);
        rollSpeed += randomSigned(0.05f);
        rollSpeed = std::fmax(-maxRollSpeed, std::fmin(maxRollSpeed, rollSpeed));
        roll += rollSpeed;
        if (randomNumber() % 6000 == 0) roll += (randomNumber() & 1 ? 90 : -90) + randomSigned(20.0f);   // sudden wrist flip
        if (roll > 180) roll -= 360;
        if (roll < -180) roll += 360;

        // Camera basis: forward towards the aim point, then rolled around it.
        const Vector3 position = {gunX, gunY, gunZ}, target = {aimX, aimY, 0};
        const Vector3 forward = normalize(subtract(target, position));
        const Vector3 right = normalize(cross(forward, {0, 1, 0}));
        const Vector3 up = cross(right, forward);
        const double rollCos = std::cos(roll * M_PI / 180), rollSin = std::sin(roll * M_PI / 180);
        const Vector3 rolledRight = {right.x * rollCos + up.x * rollSin, right.y * rollCos + up.y * rollSin, right.z * rollCos + up.z * rollSin};
        const Vector3 rolledUp = {up.x * rollCos - right.x * rollSin, up.y * rollCos - right.y * rollSin, up.z * rollCos - right.z * rollSin};

        // Dropouts: blackouts, single LEDs, and pairs of LEDs.
        if (blackoutFrames > 0) blackoutFrames--;
        else if (randomNumber() % framesPerBlackout == 0) blackoutFrames = 1 + randomNumber() % 14;
        for (int led = 0; led < 4; led++) {
            if (hiddenFrames[led] > 0) hiddenFrames[led]--;
            else if (randomNumber() % 700 == 0) hiddenFrames[led] = 1 + randomNumber() % 40;
        }
        if (randomNumber() % 1200 == 0) {
            const int first = randomNumber() % 4, second = randomNumber() % 4, length = 3 + randomNumber() % 30;
            hiddenFrames[first] = length;
            hiddenFrames[second] = length;
        }

        // Project the LEDs into the camera image, with half a pixel of noise.
        int imageX[4], imageY[4];
        bool visible[4];
        for (int led = 0; led < 4; led++) {
            const Vector3 toLed = subtract(leds[led], position);
            const double depth = dot(toLed, forward);
            visible[led] = false;
            imageX[led] = imageY[led] = 0;
            if (depth > 0.05 && !hiddenFrames[led] && !blackoutFrames) {
                const double u = dot(toLed, rolledRight) / depth * focalPixels + 511.5 + randomSigned(0.6f);
                const double v = -dot(toLed, rolledUp) / depth * focalPixels + 383.5 + randomSigned(0.6f);
                const int pixelX = (int)std::lround(u), pixelY = (int)std::lround(v);
                if (pixelX >= 0 && pixelX <= 1023 && pixelY >= 0 && pixelY <= 767) {
                    visible[led] = true;
                    // The camera image is mirrored relative to the screen; the Square tracker mirrors it back.
                    imageX[led] = layout == 0 ? 1023 - pixelX : pixelX;
                    imageY[led] = pixelY;
                }
            }
        }

        // The camera reports its blobs in arbitrary slots: shuffle them from time to time.
        if (randomNumber() % 12 == 0) {
            for (int i = 3; i > 0; i--) {
                const int j = randomNumber() % (i + 1);
                const int swapped = blobSlotOrder[i]; blobSlotOrder[i] = blobSlotOrder[j]; blobSlotOrder[j] = swapped;
            }
        }
        int blobX[4] = {1023, 1023, 1023, 1023}, blobY[4] = {1023, 1023, 1023, 1023};
        unsigned seenMask = 0;
        int blobCount = 0;
        for (int slot = 0; slot < 4; slot++) {
            const int led = blobSlotOrder[slot];
            if (visible[led]) {
                blobX[blobCount] = imageX[led];
                blobY[blobCount] = imageY[led];
                seenMask |= 1u << blobCount;
                blobCount++;
            }
        }

        int trackedX[4], trackedY[4];
        if (layout == 0) {
            squareTracker.begin(blobX, blobY, seenMask);
            printf("%d S %u", frame, squareTracker.seen());
            for (int i = 0; i < 4; i++) printf(" %d,%d,%u", squareTracker.X(i), squareTracker.Y(i), squareTracker.testSee(i));
            printf(" m%d,%d H%.9g W%.9g A%.9g\n", squareTracker.testMedianX(), squareTracker.testMedianY(),
                   squareTracker.H(), squareTracker.W(), squareTracker.Ang());
            for (int i = 0; i < 4; i++) { trackedX[i] = squareTracker.X(i); trackedY[i] = squareTracker.Y(i); }
        } else {
            diamondTracker.begin(blobX, blobY, seenMask);
            printf("%d D %u", frame, diamondTracker.seen());
            for (int i = 0; i < 4; i++) printf(" %d,%d,%u", diamondTracker.X(i), diamondTracker.Y(i), diamondTracker.testSee(i));
            printf(" m%d,%d H%.9g W%.9g A%.9g\n", diamondTracker.testMedianX(), diamondTracker.testMedianY(),
                   diamondTracker.H(), diamondTracker.W(), diamondTracker.Ang());
            for (int i = 0; i < 4; i++) { trackedX[i] = diamondTracker.X(i); trackedY[i] = diamondTracker.Y(i); }
        }

        float filteredX[4], filteredY[4];
        filter.process(trackedX, trackedY, filteredX, filteredY);

        // The same calls as OpenFIREcommon's GetPosition(): the Square's points as given, the Diamond's in the
        // order the advanced perspective expects (top, left, right, bottom).
        if (layout == 0) {
            rawWarp.warp(trackedX[0], trackedY[0], trackedX[1], trackedY[1], trackedX[2], trackedY[2], trackedX[3], trackedY[3],
                         ledsLeftX, 0, ledsRightX, 0, ledsLeftX, screenHeight, ledsRightX, screenHeight);
            filteredWarp.warp(filteredX[0], filteredY[0], filteredX[1], filteredY[1], filteredX[2], filteredY[2], filteredX[3], filteredY[3],
                              ledsLeftX, 0, ledsRightX, 0, ledsLeftX, screenHeight, ledsRightX, screenHeight);
        } else {
            rawWarp.warp(trackedX[0], trackedY[0], trackedX[1], trackedY[1], trackedX[3], trackedY[3], trackedX[2], trackedY[2],
                         screenWidth / 2, 0, 0, screenHeight / 2, screenWidth, screenHeight / 2, screenWidth / 2, screenHeight);
            filteredWarp.warp(filteredX[0], filteredY[0], filteredX[1], filteredY[1], filteredX[3], filteredY[3], filteredX[2], filteredY[2],
                              screenWidth / 2, 0, 0, screenHeight / 2, screenWidth, screenHeight / 2, screenWidth / 2, screenHeight);
        }
        printf("%d P %d %d | %d %d |", frame, rawWarp.getX(), rawWarp.getY(), filteredWarp.getX(), filteredWarp.getY());
        for (int i = 0; i < 4; i++) printf(" %.9g,%.9g", filteredX[i], filteredY[i]);
        printf("\n");
    }
    return 0;
}
