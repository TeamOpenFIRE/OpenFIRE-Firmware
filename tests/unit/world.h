// A simulated gun, screen and camera for the tests of the firmware's own code: it plays out the world around the
// firmware's loops, which would otherwise wait for ever for a button or a camera frame.
//
// The screen is a plane in front of the gun, in the firmware's screen units (res_x by res_y). The LEDs are on it, in the
// square layout (the corners of a rectangle as tall as the screen) or the diamond layout (the middles of its sides).
// The gun has a pinhole camera that looks along the gun, which aims at a point of the screen; the gun can be rolled. The
// camera reports what the DFRobot camera reports: the points mirrored left to right, in its own 1024 x 768 coordinates.
//
// A test gives a script, a function from the time (in milliseconds since the start) to what the player does: where the gun
// aims, how it is rolled, and which buttons are held. The world applies it to the pins and the camera whenever the
// firmware looks at the clock, and sets the flag the board's timer would set, 200 times a second, for the camera to be read.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "firmware_support.h"
#include "OpenFIREcommon.h"

namespace world {

// What the player does at some moment.
struct Pose {
    double aimX = res_x / 2.0, aimY = res_y / 2.0;      // where on the screen the gun points, in screen units
    double rollDegrees = 0;
    uint32_t heldButtons = 0;                            // bit i: button i of the ButtonDesc table is held
    unsigned hiddenLeds = 0;                             // bit i: LED i is covered (in the order of the LEDs below)
    double jitterPixels = 0;                             // the camera's points wobble by up to this much (each frame, each point)
};

struct Setup {
    bool diamond = false;
    double ledWidth = 0.711 * res_y;                     // square layout: the width of the LED rectangle (its height is the screen's)
    double ledTop = 0, ledBottom = res_y;                // square layout: the rows of the LEDs; the screen's edges are 0 and res_y
    double gunX = res_x / 2.0, gunY = res_y / 2.0;       // where the gun is, in front of the screen plane
    double distance = 30000;                             // from the screen plane, in screen units
    double focalLength = 1728;                           // of the camera, in its pixels
    bool shuffleBlobs = false;                           // the camera reports its points in another order than the LEDs'
};

// Where LED `led` is on the screen's plane.
inline void ledPlace(const Setup& setup, int led, double& x, double& y) {
    if (setup.diamond) {
        // top, left, right, bottom
        const double places[4][2] = {{res_x / 2.0, 0}, {0, res_y / 2.0}, {(double)res_x, res_y / 2.0}, {res_x / 2.0, (double)res_y}};
        x = places[led][0];
        y = places[led][1];
    } else {
        // top left, top right, bottom left, bottom right
        const double left = res_x / 2.0 - setup.ledWidth / 2, right = res_x / 2.0 + setup.ledWidth / 2;
        const double places[4][2] = {{left, setup.ledTop}, {right, setup.ledTop}, {left, setup.ledBottom}, {right, setup.ledBottom}};
        x = places[led][0];
        y = places[led][1];
    }
}

// Where the camera sees LED `led`, in its own coordinates, or false if it is outside its image.
inline bool cameraPoint(const Setup& setup, const Pose& pose, int led, int& rawX, int& rawY) {
    double ledX, ledY;
    ledPlace(setup, led, ledX, ledY);

    // The direction of the gun, and the camera's axes: right, down, forward (the camera sits still and the screen moves).
    double forward[3] = {pose.aimX - setup.gunX, pose.aimY - setup.gunY, setup.distance};
    const double length = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    for (double& component : forward) component /= length;
    // right = down(screen) x forward, normalized; down = forward x right
    double right[3] = {forward[2], 0, -forward[0]};       // (0,1,0) x forward
    const double rightLength = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    for (double& component : right) component /= rightLength;
    double down[3] = {forward[1] * right[2] - forward[2] * right[1], forward[2] * right[0] - forward[0] * right[2], forward[0] * right[1] - forward[1] * right[0]};

    // The roll turns the camera about its axis (positive is clockwise as seen from behind the gun).
    const double roll = pose.rollDegrees * M_PI / 180.0;
    double rolledRight[3], rolledDown[3];
    for (int i = 0; i < 3; i++) {
        rolledRight[i] = right[i] * std::cos(roll) + down[i] * std::sin(roll);
        rolledDown[i] = -right[i] * std::sin(roll) + down[i] * std::cos(roll);
    }

    const double toLed[3] = {ledX - setup.gunX, ledY - setup.gunY, setup.distance};
    const double cameraX = toLed[0] * rolledRight[0] + toLed[1] * rolledRight[1] + toLed[2] * rolledRight[2];
    const double cameraY = toLed[0] * rolledDown[0] + toLed[1] * rolledDown[1] + toLed[2] * rolledDown[2];
    const double cameraZ = toLed[0] * forward[0] + toLed[1] * forward[1] + toLed[2] * forward[2];
    if (cameraZ <= 0) return false;

    const double naturalX = 512 + setup.focalLength * cameraX / cameraZ;
    const double naturalY = 384 + setup.focalLength * cameraY / cameraZ;
    rawX = (int)std::lround(1023 - naturalX);               // the sensor's image is mirrored
    rawY = (int)std::lround(naturalY);
    return rawX >= 0 && rawX <= 1023 && rawY >= 0 && rawY <= 767;
}

// The camera's answer in the basic format (see DFRobotIRPositionEx): a header byte and two frames of two points.
// A number between -1 and 1 that depends on the three given, the same every time (the camera's noise).
inline double noiseOf(unsigned frame, int blob, int axis) {
    unsigned hash = frame * 2654435761u ^ (unsigned)(blob * 40503 + axis * 9973 + 1) * 2246822519u;
    hash ^= hash >> 15; hash *= 2246822519u; hash ^= hash >> 13;
    return (hash % 20001) / 10000.0 - 1.0;
}

inline std::vector<uint8_t> basicFrame(const Setup& setup, const Pose& pose, unsigned frame = 0) {
    int x[4], y[4];
    for (int blob = 0; blob < 4; blob++) {
        static const int shuffled[4] = {2, 0, 3, 1};
        const int led = setup.shuffleBlobs ? shuffled[blob] : blob;
        int rawX, rawY;
        if (!(pose.hiddenLeds & (1u << led)) && cameraPoint(setup, pose, led, rawX, rawY)) {
            x[blob] = std::clamp((int)std::lround(rawX + pose.jitterPixels * noiseOf(frame, blob, 0)), 0, 1023);
            y[blob] = std::clamp((int)std::lround(rawY + pose.jitterPixels * noiseOf(frame, blob, 1)), 0, 767);
        } else {
            x[blob] = 1023;         // an empty point
            y[blob] = 1023;
        }
    }
    std::vector<uint8_t> bytes = {0};
    for (int frame = 0; frame < 2; frame++) {
        const int first = frame * 2, second = frame * 2 + 1;
        bytes.push_back(x[first] & 0xFF);
        bytes.push_back(y[first] & 0xFF);
        bytes.push_back(((y[first] >> 8) & 3) << 6 | ((x[first] >> 8) & 3) << 4 | ((y[second] >> 8) & 3) << 2 | ((x[second] >> 8) & 3));
        bytes.push_back(x[second] & 0xFF);
        bytes.push_back(y[second] & 0xFF);
    }
    return bytes;
}

class World {
public:
    Setup setup;
    std::function<Pose(unsigned long)> script;           // the player, by milliseconds since the start
    Pose pose;                                           // what the player is doing now

    // Starts the world: from now on, every look at the clock moves it on by a millisecond and plays the script.
    void start() {
        current() = this;
        startedAt = testMillis;
        testTickHook = &World::tick;
        Wire.responder = [this] { return basicFrame(setup, pose, frame); };
        apply();
    }
    void stop() {
        testTickHook = nullptr;
        Wire.responder = nullptr;
        current() = nullptr;
    }
    unsigned long elapsed() const { return testMillis - startedAt; }

    // Lets time go by with the script playing (outside of the firmware's own loops).
    void run(unsigned long milliseconds, const std::function<void()>& everyFrame = nullptr) {
        const unsigned long end = testMillis + milliseconds;
        while (testMillis < end) {
            tick();
            if (everyFrame && FW_Common::irPosUpdateTick) everyFrame();
        }
    }

private:
    unsigned long startedAt = 0;
    unsigned frame = 0;                                  // counts the camera frames, for the noise

    static World*& current() { static World* world = nullptr; return world; }
    static void tick() {
        World* world = current();
        if (!world) return;
        testMillis++;
        world->apply();
    }
    void apply() {
        pose = script ? script(elapsed()) : Pose();
        for (unsigned button = 0; button < ButtonCount; button++) {
            const int pin = LightgunButtons::ButtonDesc[button].pin;
            if (pin >= 0 && pin < 64) testPinLevel[pin] = (pose.heldButtons & (1u << button)) ? 0 : 1;
        }
        if (elapsed() % 5 == 0) {
            FW_Common::irPosUpdateTick = 1;      // the camera is read about 200 times a second
            frame++;
        }
    }
};

} // namespace world

namespace world {

// Powers the gun up as the firmware's setup() does, as far as the tests need: the board's presets, the camera started, the
// buttons on their pins, the trackers and the warp with no history. Run mode starts as Normal.
inline void bootGun(bool diamond = false) {
    firmware::restorePowerUpState();
    OF_Prefs::LoadPresets();
    OF_Prefs::currentProfile = 0;
    for (int profile = 0; profile < PROFILE_COUNT; profile++) {
        OF_Prefs::profiles[profile].irLayout = diamond ? OF_Const::layoutDiamond : OF_Const::layoutSquare;
    }
    Wire = TwoWire();
    Wire1 = TwoWire();
    Serial.clear();
    resetUsbDevices();

    FW_Common::gunMode = FW_Const::GunMode_Init;
    FW_Common::runMode = FW_Const::RunMode_Normal;
    FW_Common::stateFlags = FW_Const::StateFlagsDtrReset;
    FW_Common::justBooted = false;
    FW_Common::lastSeen = 0;
    FW_Common::mouseX = FW_Common::mouseY = 0;
    FW_Common::irPosUpdateTick = 0;
    FW_Common::camNotAvailable = false;
    FW_Common::camWarningTimestamp = 0;
    FW_Common::moveIndex = 0;
    for (int i = 0; i < 3; i++) FW_Common::moveXAxisArr[i] = FW_Common::moveYAxisArr[i] = 0;
    FW_Common::PinsReset();
    FW_Common::OpenFIREsquare = OpenFIRE_Square{};
    FW_Common::OpenFIREdiamond = OpenFIRE_Diamond{};
    FW_Common::OpenFIREper = OpenFIRE_Perspective{};
    FW_Common::OpenFIREoef.reset();
    FW_Common::UpdateBindings(true);
    FW_Common::buttons.Unset();
    FW_Common::buttons.offScreen = false;
    FW_Common::buttons.analogOutput = false;
    FW_Common::buttons.Begin();
    FW_Common::buttons.ReportEnable();
    FW_Common::FeedbackSet();
    FW_Common::CameraSet();
    FW_Common::SelectCalProfile(0);
}

} // namespace world
