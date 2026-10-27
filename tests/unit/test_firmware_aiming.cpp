// End-to-end tests of the firmware's aiming: a simulated player (unit/world.h) calibrates the gun with the trigger, as in the
// calibration mode, and then aims at points of the screen. The camera, the trackers, the warp, the calibration offsets and the
// mouse output all run as in the firmware; only the board is a fake.
#include "framework.h"

#include <cstring>

#include "OpenFIREserial.h"
#include "world.h"

#define private public
#include "OpenFIRElights.h"
#undef private

using world::Pose;
using world::World;

namespace {

constexpr double CENTER_X = res_x / 2.0, CENTER_Y = res_y / 2.0;

// The calibration as a player does it: the trigger is pulled at seven moments, aimed at the screen's center, top, bottom,
// left and right edge, and the center twice. The cursor takes about half a second to move to each next target, during which
// the trigger is ignored, so the pulls are 1.5 seconds apart.
Pose calibrationScript(unsigned long milliseconds, double rollDegrees = 0) {
    Pose pose;
    pose.rollDegrees = rollDegrees;
    const unsigned long firstPull = 300, interval = 1500, pullLength = 60;
    const double aims[7][2] = {{CENTER_X, CENTER_Y}, {CENTER_X, 0}, {CENTER_X, res_y}, {0, CENTER_Y}, {res_x, CENTER_Y}, {CENTER_X, CENTER_Y}, {CENTER_X, CENTER_Y}};
    for (int pull = 0; pull < 7; pull++) {
        const unsigned long at = firstPull + interval * pull;
        if (milliseconds + 400 >= at && milliseconds < at + pullLength + 400) {      // aims at the target before and after the pull
            pose.aimX = aims[pull][0];
            pose.aimY = aims[pull][1];
        }
        if (milliseconds >= at && milliseconds < at + pullLength) pose.heldButtons = FW_Const::BtnMask_Trigger;
    }
    return pose;
}

// Where the firmware says the gun points, after the player has kept it aimed at (aimX, aimY) for a moment.
struct Reading {
    int x, y;
};

Reading aimAt(World& gun, double aimX, double aimY, double rollDegrees = 0, unsigned hiddenLeds = 0, unsigned long milliseconds = 400) {
    // The player moves the gun there over 200 ms, from where it was (a hand doesn't jump), and then holds it.
    const Pose from = gun.pose;
    const unsigned long start = gun.elapsed();
    gun.script = [=](unsigned long t) {
        const double progress = std::min(1.0, (double)(t - start) / 200.0);
        Pose pose;
        pose.aimX = from.aimX + (aimX - from.aimX) * progress;
        pose.aimY = from.aimY + (aimY - from.aimY) * progress;
        pose.rollDegrees = from.rollDegrees + (rollDegrees - from.rollDegrees) * progress;
        pose.hiddenLeds = hiddenLeds;
        return pose;
    };
    gun.run(milliseconds, [] {
        FW_Common::irPosUpdateTick = 0;
        FW_Common::GetPosition();
    });
    return {FW_Common::mouseX, FW_Common::mouseY};
}

// Boots a gun, has it calibrated by the player of the script above, and leaves it in run mode with the world still on.
void calibratedGun(World& gun, bool diamond = false) {
    world::bootGun(diamond);
    gun.setup.diamond = diamond;
    gun.script = [](unsigned long t) { return calibrationScript(t); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::ExecCalMode();
}

// How far a reading is from where the gun was aimed, as a share of the screen's size (the larger of the two axes).
double errorShare(const Reading& reading, double aimX, double aimY) {
    return std::max(std::fabs(reading.x - aimX) / res_x, std::fabs(reading.y - aimY) / res_y);
}

} // namespace

TEST(Aiming, CalibrationWithTheTriggerFindsTheScreenAndTheLeds) {
    world::bootGun();
    World gun;
    gun.script = [](unsigned long t) { return calibrationScript(t); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::ExecCalMode();
    gun.stop();

    const auto& profile = OF_Prefs::profiles[OF_Prefs::currentProfile];
    CHECK(FW_Common::gunMode == FW_Const::GunMode_Run);
    INFO("offsets %d %d %d %d, LEDs %.0f %.0f", profile.topOffset, profile.bottomOffset, profile.leftOffset, profile.rightOffset, profile.TLled, profile.TRled);
    // The LEDs are as tall as the screen and 0.711 of its height wide, in the middle.
    CHECK_NEAR(profile.TLled, CENTER_X - 0.711 * res_y / 2, 0.01 * res_x);
    CHECK_NEAR(profile.TRled, CENTER_X + 0.711 * res_y / 2, 0.01 * res_x);
    // The LEDs frame the screen exactly, so the edges are read where they are.
    CHECK_NEAR(profile.topOffset, 0, 0.01 * res_y);
    CHECK_NEAR(profile.bottomOffset, 0, 0.01 * res_y);
    CHECK_NEAR(profile.leftOffset, 0, 0.01 * res_x);
    CHECK_NEAR(profile.rightOffset, 0, 0.01 * res_x);
    CHECK_NEAR(profile.adjX, 512 << 2, 20);
    CHECK_NEAR(profile.adjY, 384 << 2, 20);
}

TEST(Aiming, AfterCalibrationTheGunPointsWhereItIsAimed) {
    World gun;
    calibratedGun(gun);
    // A grid over the screen, and a little beyond its edges.
    for (double fx : {0.05, 0.25, 0.5, 0.75, 0.95}) {
        for (double fy : {0.05, 0.25, 0.5, 0.75, 0.95}) {
            const double aimX = fx * res_x, aimY = fy * res_y;
            const Reading reading = aimAt(gun, aimX, aimY);
            INFO("aimed at %.0f, %.0f: read %d, %d", aimX, aimY, reading.x, reading.y);
            CHECK(errorShare(reading, aimX, aimY) < 0.01);
        }
    }
    gun.stop();
}

TEST(Aiming, TheMouseFollowsTheAim) {
    World gun;
    calibratedGun(gun);
    aimAt(gun, 0.25 * res_x, 0.75 * res_y);
    // The mouse is told 0..32767 across the screen.
    CHECK_NEAR(AbsMouse5.x, 0.25 * 32767, 0.01 * 32767);
    CHECK_NEAR(AbsMouse5.y, 0.75 * 32767, 0.01 * 32767);
    gun.stop();
}

TEST(Aiming, RollingTheGunDoesNotMoveTheAim) {
    World gun;
    calibratedGun(gun);
    // The classic trackers (RP2040) follow a roll of up to about 35 degrees; the advanced ones (RP2350) go on to about 120.
#ifdef ADVANCED_TRACKING
    const std::vector<double> rolls = {-100.0, -40.0, -15.0, 15.0, 40.0, 100.0};
#else
    const std::vector<double> rolls = {-30.0, -15.0, 15.0, 30.0};
#endif
    for (double roll : rolls) {
        for (double fx : {0.2, 0.8}) {
            const double aimX = fx * res_x, aimY = 0.4 * res_y;
            const Reading reading = aimAt(gun, aimX, aimY, roll);
            INFO("roll %.0f, aimed at %.0f, %.0f: read %d, %d", roll, aimX, aimY, reading.x, reading.y);
            CHECK(errorShare(reading, aimX, aimY) < 0.015);
        }
    }
    gun.stop();
}

TEST(Aiming, AimingFromTheSideAndFromAnotherDistanceStillWorks) {
    World gun;
    calibratedGun(gun);
    struct Place { double gunX, gunY, distance; };
    const Place places[] = {{CENTER_X - 0.25 * res_x, CENTER_Y, 30000}, {CENTER_X + 0.25 * res_x, CENTER_Y + 0.1 * res_y, 30000}, {CENTER_X, CENTER_Y, 20000}, {CENTER_X, CENTER_Y, 45000}};
    for (const Place& place : places) {
        gun.setup.gunX = place.gunX;
        gun.setup.gunY = place.gunY;
        gun.setup.distance = place.distance;
        const double aimX = 0.6 * res_x, aimY = 0.3 * res_y;
        const Reading reading = aimAt(gun, aimX, aimY);
        INFO("gun at %.0f, %.0f, %.0f: read %d, %d", place.gunX, place.gunY, place.distance, reading.x, reading.y);
        CHECK(errorShare(reading, aimX, aimY) < 0.02);
    }
    gun.stop();
}

TEST(Aiming, TheDiamondLayoutCalibratesAndAimsToo) {
    World gun;
    calibratedGun(gun, true);
    CHECK(FW_Common::gunMode == FW_Const::GunMode_Run);
    for (double fx : {0.1, 0.5, 0.9}) {
        for (double fy : {0.15, 0.5, 0.85}) {
            const double aimX = fx * res_x, aimY = fy * res_y;
            const Reading reading = aimAt(gun, aimX, aimY);
            INFO("aimed at %.0f, %.0f: read %d, %d", aimX, aimY, reading.x, reading.y);
            CHECK(errorShare(reading, aimX, aimY) < 0.015);
        }
    }
    gun.stop();
}

TEST(Aiming, CalibrationMakesUpForLedsThatAreLargerThanTheScreen) {
    // The LEDs are 10% taller than the screen: the screen's edges are read inside the LEDs' rectangle, and the offsets
    // taken at the edges are what stretches the output to the whole screen.
    World gun;
    gun.setup.ledTop = -0.05 * res_y;
    gun.setup.ledBottom = 1.05 * res_y;
    calibratedGun(gun);

    const auto& profile = OF_Prefs::profiles[OF_Prefs::currentProfile];
    CHECK_NEAR(profile.topOffset, 0.05 / 1.1 * res_y, 0.01 * res_y);
    CHECK_NEAR(profile.bottomOffset, 0.05 / 1.1 * res_y, 0.01 * res_y);
    for (double fy : {0.0, 0.3, 1.0}) {
        const double aimY = fy * res_y;
        const Reading reading = aimAt(gun, CENTER_X, aimY);
        INFO("aimed at y %.0f: read %d", aimY, reading.y);
        CHECK_NEAR(reading.y, aimY, 0.015 * res_y);
    }
    gun.stop();
}

TEST(Aiming, AimingPastTheScreenEdgesIsOffScreen) {
    World gun;
    calibratedGun(gun);
    aimAt(gun, 0.5 * res_x, 0.5 * res_y);
    CHECK(!FW_Common::buttons.offScreen);

    aimAt(gun, 1.1 * res_x, 0.5 * res_y);
    CHECK(FW_Common::buttons.offScreen);
    CHECK_EQ(AbsMouse5.x, 32767);               // the cursor stays at the edge

    aimAt(gun, 0.5 * res_x, -0.1 * res_y);
    CHECK(FW_Common::buttons.offScreen);
    CHECK_EQ(AbsMouse5.y, 0);

    aimAt(gun, 0.5 * res_x, 0.5 * res_y);
    CHECK(!FW_Common::buttons.offScreen);
    gun.stop();
}

TEST(Aiming, AnalogOutputMovesTheGamepadCameraInstead) {
    World gun;
    calibratedGun(gun);
    FW_Common::buttons.analogOutput = true;
    AbsMouse5.move(1, 1);
    aimAt(gun, 0.25 * res_x, 0.5 * res_y);
    CHECK_NEAR(Gamepad16.camX, 0.25 * 32767, 0.015 * 32767);
    CHECK_NEAR(Gamepad16.camY, 0.5 * 32767, 0.015 * 32767);
    CHECK_EQ(AbsMouse5.x, 1);                   // the mouse stayed where it was
    gun.stop();
}

TEST(Aiming, WithALedCoveredTheAimStaysWhereItIsWhileTheGunIsStill) {
    World gun;
    calibratedGun(gun);
    const double aimX = 0.4 * res_x, aimY = 0.6 * res_y;
    aimAt(gun, aimX, aimY);
    for (int hidden = 0; hidden < 4; hidden++) {
        const Reading reading = aimAt(gun, aimX, aimY, 0, 1u << hidden);
        INFO("LED %d covered: read %d, %d", hidden, reading.x, reading.y);
        CHECK(errorShare(reading, aimX, aimY) < 0.02);
    }
    gun.stop();
}

TEST(Aiming, TheCameraReportingItsPointsInAnotherOrderChangesNothing) {
    World gun;
    gun.setup.shuffleBlobs = true;
    calibratedGun(gun);
    const double aimX = 0.7 * res_x, aimY = 0.2 * res_y;
    const Reading reading = aimAt(gun, aimX, aimY);
    CHECK(errorShare(reading, aimX, aimY) < 0.01);
    gun.stop();
}

TEST(Aiming, TheLedColorFollowsWhetherLedsAreSeen) {
    World gun;
    calibratedGun(gun);
    OF_RGB::LedOff();
    aimAt(gun, CENTER_X, CENTER_Y);
    CHECK_EQ(FW_Common::lastSeen, 15);

    // Everything covered: the light turns to the "no LEDs" colour.
    aimAt(gun, CENTER_X, CENTER_Y, 0, 0x0F);
    CHECK_EQ(FW_Common::lastSeen, 0);
    CHECK_EQ(OF_RGB::neopixel.shown[0], WikiColor::Amber);

    aimAt(gun, CENTER_X, CENTER_Y);
    CHECK_EQ(FW_Common::lastSeen, 15);
    CHECK_EQ(OF_RGB::neopixel.shown[0], 0);
    gun.stop();
}

namespace {

// One press of some buttons at some moment, with the gun aimed where the player had it at the time.
struct Press {
    unsigned long at, length;
    uint32_t buttons;
    double aimX = CENTER_X, aimY = CENTER_Y;
};

// The pose of a player who does the presses in order: the gun is aimed at the target of the press that is next or going
// on, and stays there until the one after.
Pose playerDoing(unsigned long t, const std::vector<Press>& presses) {
    Pose pose;
    const Press* current = &presses.front();
    for (const Press& press : presses) {
        if (t + 400 >= press.at) current = &press;
        if (t >= press.at && t < press.at + press.length) pose.heldButtons |= press.buttons;
    }
    pose.aimX = current->aimX;
    pose.aimY = current->aimY;
    return pose;
}

std::vector<Press> trigger(unsigned long at, double aimX, double aimY) {
    return {{at, 60, FW_Const::BtnMask_Trigger, aimX, aimY}};
}

void joined(std::vector<Press>& all, const std::vector<Press>& more) { all.insert(all.end(), more.begin(), more.end()); }

} // namespace

TEST(Aiming, CancelingCalibrationWithHomeRestoresTheOldValuesAndGoesBackToRun) {
    world::bootGun();
    auto& profile = OF_Prefs::profiles[OF_Prefs::currentProfile];
    profile.topOffset = 11; profile.bottomOffset = 12; profile.leftOffset = 13; profile.rightOffset = 14;
    profile.TLled = 2500; profile.TRled = 5200; profile.adjX = 2100; profile.adjY = 1600;

    std::vector<Press> presses = trigger(300, CENTER_X, CENTER_Y);
    joined(presses, trigger(1800, CENTER_X, 0));
    joined(presses, {{3500, 60, FW_Const::BtnMask_Home}});       // changes of mind: Home
    World gun;
    gun.script = [presses](unsigned long t) { return playerDoing(t, presses); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    Serial.clear();
    FW_Common::ExecCalMode();
    gun.stop();

    CHECK(FW_Common::gunMode == FW_Const::GunMode_Run);
    CHECK_EQ(profile.topOffset, 11);
    CHECK_EQ(profile.bottomOffset, 12);
    CHECK_EQ(profile.leftOffset, 13);
    CHECK_EQ(profile.rightOffset, 14);
    CHECK_NEAR(profile.TLled, 2500, 0);
    CHECK_NEAR(profile.TRled, 5200, 0);
    CHECK_NEAR(profile.adjX, 2100, 0);
    CHECK_NEAR(profile.adjY, 1600, 0);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_PrintSelectedProfile);
}

TEST(Aiming, TheAppCancelsCalibrationWithTheTerminatorAndGoesBackToDocked) {
    world::bootGun();
    OF_Prefs::profiles[0].topOffset = 21;
    World gun;
    std::vector<Press> presses = trigger(300, CENTER_X, CENTER_Y);
    gun.script = [presses](unsigned long t) { return playerDoing(t, presses); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Docked);
    // The terminator arrives while the loop runs.
    gun.run(1);
    Serial.input = std::string(1, (char)OF_Const::serialTerminator);
    FW_Common::ExecCalMode(true);
    gun.stop();

    CHECK(FW_Common::gunMode == FW_Const::GunMode_Docked);
    CHECK_EQ(OF_Prefs::profiles[0].topOffset, 21);
}

TEST(Aiming, CalibrationFromTheAppEndsDockedAndDoesNotMoveTheMouse) {
    world::bootGun();
    World gun;
    gun.script = [](unsigned long t) { return calibrationScript(t); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Docked);
    AbsMouse5.move(1234, 4321);
    FW_Common::ExecCalMode(true);
    gun.stop();
    CHECK(FW_Common::gunMode == FW_Const::GunMode_Docked);
    CHECK_NEAR(OF_Prefs::profiles[0].TLled, CENTER_X - 0.711 * res_y / 2, 0.01 * res_x);
}

TEST(Aiming, TheFirstCalibrationAfterPowerUpIsSavedAtOnce) {
    world::bootGun();
    FW_Common::justBooted = true;
    World gun;
    gun.script = [](unsigned long t) { return calibrationScript(t); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::ExecCalMode();
    gun.stop();
    CHECK(LittleFS.exists("/profiles.conf"));
    FW_Common::justBooted = false;
}

TEST(Aiming, CalibrationEndsWithTheRumbleBuzzIfItIsOn) {
    world::bootGun();
    OF_Prefs::toggles[OF_Const::rumble] = true;
    World gun;
    gun.script = [](unsigned long t) { return calibrationScript(t); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::ExecCalMode();
    gun.stop();
    CHECK_EQ(testAnalogOut[OF_Prefs::pins[OF_Const::rumblePin]], OF_Prefs::settings[OF_Const::rumbleStrength]);
    CHECK_EQ(testPinOut[OF_Prefs::pins[OF_Const::rumblePin]], LOW);
}

TEST(Aiming, AOrBInTheVerificationStartsCalibrationOver) {
    world::bootGun();
    // Seven pulls of the trigger lead to the verification (the sixth enters it); there A restarts the calibration. Then the
    // whole calibration is done again, and finishes.
    std::vector<Press> presses;
    const double aims[6][2] = {{CENTER_X, CENTER_Y}, {CENTER_X, 0}, {CENTER_X, res_y}, {0, CENTER_Y}, {res_x, CENTER_Y}, {CENTER_X, CENTER_Y}};
    for (int pull = 0; pull < 6; pull++) joined(presses, trigger(300 + 1500 * pull, aims[pull][0], aims[pull][1]));
    joined(presses, {{9800, 60, FW_Const::BtnMask_A, CENTER_X, CENTER_Y}});
    for (int pull = 0; pull < 6; pull++) joined(presses, trigger(10800 + 1500 * pull, aims[pull][0], aims[pull][1]));
    joined(presses, trigger(10800 + 1500 * 6, CENTER_X, CENTER_Y));

    World gun;
    gun.script = [presses](unsigned long t) { return playerDoing(t, presses); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    Serial.clear();
    FW_Common::ExecCalMode();
    gun.stop();
    CHECK(FW_Common::gunMode == FW_Const::GunMode_Run);
    CHECK_NEAR(OF_Prefs::profiles[0].TLled, CENTER_X - 0.711 * res_y / 2, 0.01 * res_x);
    CHECK_NEAR(OF_Prefs::profiles[0].topOffset, 0, 0.01 * res_y);
}

TEST(Aiming, HomeInTheVerificationLeavesWithoutKeepingTheNewCalibration) {
    world::bootGun();
    auto& profile = OF_Prefs::profiles[OF_Prefs::currentProfile];
    profile.TLled = 2500; profile.TRled = 5200;
    std::vector<Press> presses;
    const double aims[6][2] = {{CENTER_X, CENTER_Y}, {CENTER_X, 0}, {CENTER_X, res_y}, {0, CENTER_Y}, {res_x, CENTER_Y}, {CENTER_X, CENTER_Y}};
    for (int pull = 0; pull < 6; pull++) joined(presses, trigger(300 + 1500 * pull, aims[pull][0], aims[pull][1]));
    joined(presses, {{9800, 60, FW_Const::BtnMask_Home, CENTER_X, CENTER_Y}});

    World gun;
    gun.script = [presses](unsigned long t) { return playerDoing(t, presses); };
    gun.start();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::ExecCalMode();
    gun.stop();
    CHECK(FW_Common::gunMode == FW_Const::GunMode_Run);
    CHECK_NEAR(profile.TLled, 2500, 0);
    CHECK_NEAR(profile.TRled, 5200, 0);
}

namespace {

// The spread (standard deviation) of the cursor's X while the gun is held aimed at one point and the camera's points wobble.
double wobbleOfTheCursor(World& gun, double aimX, double aimY, double jitterPixels, int frames = 400) {
    // Settle first, so that the filters have what to start from.
    aimAt(gun, aimX, aimY, 0, 0, 300);
    gun.script = [=](unsigned long) {
        Pose pose;
        pose.aimX = aimX;
        pose.aimY = aimY;
        pose.jitterPixels = jitterPixels;
        return pose;
    };
    std::vector<double> samples;
    gun.run(frames * 5ul, [&] {
        FW_Common::irPosUpdateTick = 0;
        FW_Common::GetPosition();
        samples.push_back(FW_Common::mouseX);
    });
    double mean = 0;
    for (double sample : samples) mean += sample;
    mean /= samples.size();
    double variance = 0;
    for (double sample : samples) variance += (sample - mean) * (sample - mean);
    return std::sqrt(variance / samples.size());
}

} // namespace

TEST(Aiming, TheAveragingRunModesCalmTheCursorDown) {
    World gun;
    calibratedGun(gun);
    FW_Common::SetRunMode(FW_Const::RunMode_Normal);
    const double normal = wobbleOfTheCursor(gun, 0.4 * res_x, 0.4 * res_y, 1.5);
    FW_Common::SetRunMode(FW_Const::RunMode_Average);
    const double average = wobbleOfTheCursor(gun, 0.4 * res_x, 0.4 * res_y, 1.5);
    FW_Common::SetRunMode(FW_Const::RunMode_Average2);
    const double average2 = wobbleOfTheCursor(gun, 0.4 * res_x, 0.4 * res_y, 1.5);
    INFO("normal %.1f, average %.1f, average2 %.1f", normal, average, average2);
    CHECK(normal > 5);                     // there is something to calm down
    CHECK(average < normal * 0.85);
    CHECK(average2 < average);
    gun.stop();
}

TEST(Aiming, TheOneEuroFilterCalmsTheCursorDownWhenStill) {
    World gun;
    calibratedGun(gun);
    FW_Common::SetRunMode(FW_Const::RunMode_Normal);
    const double normal = wobbleOfTheCursor(gun, 0.4 * res_x, 0.4 * res_y, 1.5);
    FW_Common::SetRunMode(FW_Const::RunMode_OneEuro);
    const double filtered = wobbleOfTheCursor(gun, 0.4 * res_x, 0.4 * res_y, 1.5);
    INFO("normal %.1f, one euro %.1f", normal, filtered);
    CHECK(filtered < normal * 0.5);
    gun.stop();
}

TEST(Aiming, TheOneEuroFilterFollowsAFastMoveWithoutLagging) {
    World gun;
    calibratedGun(gun);
    FW_Common::SetRunMode(FW_Const::RunMode_OneEuro);
    aimAt(gun, 0.3 * res_x, 0.5 * res_y);
    // A flick of the gun across a third of the screen in 100 ms, and then about 40 ms later the cursor is there.
    const Pose from = gun.pose;
    const unsigned long start = gun.elapsed();
    gun.script = [=](unsigned long t) {
        const double progress = std::min(1.0, (double)(t - start) / 100.0);
        Pose pose = from;
        pose.aimX = from.aimX + (0.6 * res_x - from.aimX) * progress;
        return pose;
    };
    gun.run(140, [] { FW_Common::irPosUpdateTick = 0; FW_Common::GetPosition(); });
    CHECK_NEAR(FW_Common::mouseX, 0.6 * res_x, 0.03 * res_x);
    gun.stop();
}

TEST(Aiming, AspectRatioCorrectionStretchesTheChosenRatioOverTheWholeMouseRange) {
    // In the 4:3 correction mode, the 16:9 picture is in the middle of the screen's width: its edges are the mouse's.
    struct Case { int ratio; double leftEdge, rightEdge; bool wide; };
    const Case cases[] = {
        {OF_Const::ar16_9, 966, 6720, true}, {OF_Const::ar16_10, 655, 7048, true}, {OF_Const::ar3_2, 438, 7264, true},
        {OF_Const::ar5_4, 148, 4182, false}, {OF_Const::ar4_3, 0, res_x, true},
    };
    for (const Case& test : cases) {
        World gun;
        calibratedGun(gun);
        OF_Serial::serialARcorrection = true;
        OF_Prefs::profiles[OF_Prefs::currentProfile].aspectRatio = test.ratio;
        INFO("ratio %d", test.ratio);
        if (test.wide) {
            aimAt(gun, test.leftEdge + 5, CENTER_Y);
            CHECK(AbsMouse5.x < 0.01 * 32767);
            aimAt(gun, test.rightEdge - 5, CENTER_Y);
            CHECK(AbsMouse5.x > 0.99 * 32767);
        } else {
            aimAt(gun, CENTER_X, test.leftEdge + 5);
            CHECK(AbsMouse5.y < 0.01 * 32767);
            aimAt(gun, CENTER_X, test.rightEdge - 5);
            CHECK(AbsMouse5.y > 0.99 * 32767);
        }
        OF_Serial::serialARcorrection = false;
        gun.stop();
    }
}

TEST(Aiming, ProcessingModeSendsTheCoordinatesToTheApp) {
    for (bool diamond : {false, true}) {
        World gun;
        calibratedGun(gun, diamond);
        FW_Common::SetRunMode(FW_Const::RunMode_Processing);
        FW_Common::SetMode(FW_Const::GunMode_Docked);
        Serial.clear();
        aimAt(gun, 0.5 * res_x, 0.5 * res_y, 0, 0, 200);
        INFO("diamond %d", diamond);
        REQUIRE(Serial.output.size() >= 49);
        CHECK_EQ((uint8_t)Serial.output[0], OF_Const::sTestCoords);
        CHECK_EQ(Serial.output.size() % 49, 0);

        // The packet holds four LED positions, then the mouse, then the center, as 32-bit numbers.
        int mouseX;
        memcpy(&mouseX, &Serial.output[33], sizeof(int));
        CHECK_NEAR(mouseX, 0.5 * res_x / 4, 0.02 * res_x / 4);
        gun.stop();
        FW_Common::SetRunMode(FW_Const::RunMode_Normal);
    }
}

TEST(Aiming, NothingIsSentWhenTheRunModeIsNotProcessing) {
    World gun;
    calibratedGun(gun);
    FW_Common::SetMode(FW_Const::GunMode_Docked);
    Serial.clear();
    aimAt(gun, 0.5 * res_x, 0.5 * res_y, 0, 0, 200);
    CHECK_EQ(Serial.output.size(), 0);
    gun.stop();
}

TEST(Aiming, TheVerificationModeMovesTheMouseToo) {
    World gun;
    calibratedGun(gun);
    FW_Common::SetMode(FW_Const::GunMode_Verification);
    AbsMouse5.move(1, 1);
    aimAt(gun, 0.25 * res_x, 0.75 * res_y);
    CHECK_NEAR(AbsMouse5.x, 0.25 * 32767, 0.015 * 32767);
    CHECK_NEAR(AbsMouse5.y, 0.75 * 32767, 0.015 * 32767);
    gun.stop();
}

TEST(Aiming, ACameraThatStopsAnsweringIsReportedAndTheCursorStays) {
    World gun;
    calibratedGun(gun);
    aimAt(gun, 0.25 * res_x, 0.75 * res_y);
    const int x = AbsMouse5.x;
    Wire.responder = [] { return std::vector<uint8_t>(); };       // no answer from the camera
    Serial.clear();
    testMillis += 4000;
    FW_Common::camWarningTimestamp = 0;
    FW_Common::irPosUpdateTick = 0;
    FW_Common::GetPosition();
    CHECK(Serial.output.find("CAMERROR") != std::string::npos);
    CHECK(FW_Common::camNotAvailable);
    CHECK_EQ(AbsMouse5.x, x);
    gun.stop();
}

TEST(Aiming, WithoutACameraThereIsNothingToRead) {
    world::bootGun();
    FW_Common::PinsReset();
    testMillis += 4000;
    FW_Common::camWarningTimestamp = 0;
    Serial.clear();
    FW_Common::GetPosition();
    CHECK(Serial.output.find("CAMERROR") != std::string::npos);
}
