// Tests for OpenFIREFeedback: the solenoid (single shots, autofire, bursts, temperature protection) and the rumble motor,
// driven by the fake clock and pins. The class keeps its timers private; the tests open them to start each test clean.
#include "framework.h"

#include "firmware_support.h"

#define private public
#include "OpenFIREFeedback.h"
#undef private

namespace {

constexpr int SOLENOID_PIN = 16, RUMBLE_PIN = 17, TEMP_PIN = 28;

// A gun with a solenoid and a rumble motor on their pins, everything in the FFB class at its start, and the clock at 1000 ms.
void setUp(bool withTemperatureSensor = false) {
    firmware::restorePowerUpState();
    OF_Prefs::pins[OF_Const::solenoidPin] = SOLENOID_PIN;
    OF_Prefs::pins[OF_Const::rumblePin] = RUMBLE_PIN;
    OF_Prefs::pins[OF_Const::tempPin] = withTemperatureSensor ? TEMP_PIN : -1;
    OF_Prefs::toggles[OF_Const::solenoid] = true;
    OF_Prefs::toggles[OF_Const::rumble] = true;
    OF_Prefs::toggles[OF_Const::autofire] = false;
    OF_Prefs::toggles[OF_Const::rumbleFF] = false;
    testPinLevel[SOLENOID_PIN] = 0;     // outputs start low
    testPinLevel[RUMBLE_PIN] = 0;

    OF_FFB::rumbleHappening = false;
    OF_FFB::triggerHeld = false;
    OF_FFB::burstFireActive = false;
    OF_FFB::burstFiring = false;
    OF_FFB::autofireDoubleLengthWait = false;
    OF_FFB::temperatureCurrent = 25;
    OF_FFB::tempStatus = OF_FFB::Temp_Safe;
    OF_FFB::solenoidFirstShot = false;
    OF_FFB::rumbleHappened = false;
    OF_FFB::previousMillisSol = 0;
    OF_FFB::currentMillis = 0;
    OF_FFB::previousMillisTemp = 0;
    OF_FFB::tempGraphIndex = 0;
    OF_FFB::burstFireCount = 0;
    OF_FFB::burstFireCountLast = 0;
    OF_FFB::previousMillisRumble = 0;
}

bool solenoidOn() { return testPinLevel[SOLENOID_PIN] != 0; }

// Calls what the main loop calls with the trigger held and the gun aimed at the screen, once per millisecond.
void holdOnScreen(int milliseconds) {
    for (int i = 0; i < milliseconds; i++) {
        testMillis++;
        OF_FFB::FFBOnScreen();
    }
}

// Counts the times the solenoid is switched on while the trigger is held on screen.
int pulsesWhileHeld(int milliseconds) {
    int pulses = 0;
    bool wasOn = solenoidOn();
    for (int i = 0; i < milliseconds; i++) {
        testMillis++;
        OF_FFB::FFBOnScreen();
        if (solenoidOn() && !wasOn) pulses++;
        wasOn = solenoidOn();
    }
    return pulses;
}

// Presses the trigger on screen: the first call, with the trigger not held yet, and then it is.
void pullTriggerOnScreen() {
    OF_FFB::triggerHeld = false;
    OF_FFB::FFBOnScreen();
    OF_FFB::triggerHeld = true;
}

// A reading of the temperature sensor for a given temperature in Celsius (12 bits, 3.3 V, 10 mV per degree from 0.5 V).
int readingFor(double celsius) {
    return (int)((celsius / 100.0 + 0.5) * 4096.0 / 3.3 + 0.5);
}

// Makes the temperature sensor read a temperature, long enough for the four samples that make a reading.
void readTemperature(double celsius) {
    testAnalogIn[TEMP_PIN] = readingFor(celsius);
    for (int sample = 0; sample < 4; sample++) {
        testMillis += TEMP_UPDATE_INTERVAL + 1;
        OF_FFB::TemperatureUpdate();
    }
}

} // namespace

TEST(Feedback, SingleShotFiresAtOnceAndLetsGoAfterItsOnTime) {
    setUp();
    pullTriggerOnScreen();
    CHECK(solenoidOn());                         // the first shot is immediate

    holdOnScreen(44);
    CHECK(solenoidOn());                         // still inside the 45 ms
    holdOnScreen(2);
    CHECK(!solenoidOn());
}

TEST(Feedback, HoldingTheTriggerWithoutAutofireWaitsBeforeRepeating) {
    setUp();
    pullTriggerOnScreen();
    holdOnScreen(100);
    CHECK(!solenoidOn());
    // Nothing more for the hold time of 500 ms after the first shot; then it repeats at the on/off rhythm.
    const int firedBeforeTheWait = pulsesWhileHeld(300);
    CHECK_EQ(firedBeforeTheWait, 0);
    const int firedAfter = pulsesWhileHeld(1000);
    CHECK(firedAfter >= 5);
}

TEST(Feedback, AutofireRepeatsAtTheOnAndOffLengths) {
    setUp();
    OF_Prefs::toggles[OF_Const::autofire] = true;
    pullTriggerOnScreen();
    CHECK(solenoidOn());
    // One cycle is 45 ms on and 80 ms off: about eight pulses a second.
    const int pulses = pulsesWhileHeld(1000);
    CHECK(pulses >= 7 && pulses <= 9);
}

TEST(Feedback, AutofireOnTimeAndOffTimeFollowTheSettings) {
    setUp();
    OF_Prefs::toggles[OF_Const::autofire] = true;
    OF_Prefs::settings[OF_Const::solenoidOnLength] = 20;
    OF_Prefs::settings[OF_Const::solenoidOffLength] = 30;
    pullTriggerOnScreen();
    const int pulses = pulsesWhileHeld(1000);
    CHECK(pulses >= 18 && pulses <= 21);        // 50 ms a cycle
}

TEST(Feedback, TheSolenoidDoesNothingWhenItIsSwitchedOff) {
    setUp();
    OF_Prefs::toggles[OF_Const::solenoid] = false;
    pullTriggerOnScreen();
    holdOnScreen(200);
    CHECK(!solenoidOn());
    CHECK_EQ(testPinWrites[SOLENOID_PIN], 0);
}

TEST(Feedback, BurstFireIsThreeShots) {
    setUp();
    OF_FFB::burstFireActive = true;
    pullTriggerOnScreen();
    CHECK(OF_FFB::burstFiring);
    CHECK(solenoidOn());

    // Count the pulses of the whole burst, including the first.
    int pulses = 1;
    bool wasOn = true;
    for (int i = 0; i < 1000; i++) {
        testMillis++;
        OF_FFB::FFBOnScreen();
        if (solenoidOn() && !wasOn) pulses++;
        wasOn = solenoidOn();
    }
    CHECK_EQ(pulses, 3);
    CHECK(!OF_FFB::burstFiring);
    CHECK(!solenoidOn());
}

TEST(Feedback, ABurstFinishesEvenWhenTheTriggerIsLetGo) {
    setUp();
    OF_FFB::burstFireActive = true;
    pullTriggerOnScreen();
    OF_FFB::triggerHeld = false;
    int pulses = 1;
    bool wasOn = true;
    for (int i = 0; i < 1000; i++) {
        testMillis++;
        OF_FFB::FFBRelease();
        if (solenoidOn() && !wasOn) pulses++;
        wasOn = solenoidOn();
    }
    CHECK_EQ(pulses, 3);
    CHECK(!OF_FFB::burstFiring);
    CHECK(!solenoidOn());
}

TEST(Feedback, ABurstDoesNotRestartWhileTheTriggerStaysHeld) {
    setUp();
    OF_FFB::burstFireActive = true;
    pullTriggerOnScreen();
    holdOnScreen(1000);
    CHECK(!OF_FFB::burstFiring);
    const int writes = testPinWrites[SOLENOID_PIN];
    holdOnScreen(1000);
    CHECK_EQ(testPinWrites[SOLENOID_PIN], writes);      // a held trigger gives one burst
}

TEST(Feedback, ReleasingTheTriggerSwitchesTheSolenoidOffAfterItsOnTime) {
    setUp();
    OF_Prefs::toggles[OF_Const::autofire] = true;
    pullTriggerOnScreen();
    CHECK(solenoidOn());
    OF_FFB::triggerHeld = false;
    testMillis += 10;
    OF_FFB::FFBRelease();
    CHECK(solenoidOn());                  // too early: the pulse is shorter than its on time
    testMillis += 40;
    OF_FFB::FFBRelease();
    CHECK(!solenoidOn());
}

TEST(Feedback, ShootingOffScreenEndsASolenoidPulse) {
    setUp();
    OF_Prefs::toggles[OF_Const::autofire] = true;
    pullTriggerOnScreen();
    CHECK(solenoidOn());
    testMillis += 100;
    OF_FFB::FFBOffScreen();
    CHECK(!solenoidOn());
}

TEST(Feedback, ASingleShotThatWentOffScreenIsNotRestarted) {
    setUp();
    pullTriggerOnScreen();
    testMillis += 10;
    OF_FFB::FFBOffScreen();
    CHECK(solenoidOn());                  // its first pulse goes on for the on time
    testMillis += 40;
    OF_FFB::FFBOffScreen();
    CHECK(!solenoidOn());
    const int writes = testPinWrites[SOLENOID_PIN];
    testMillis += 200;
    OF_FFB::FFBOffScreen();
    CHECK_EQ(testPinWrites[SOLENOID_PIN], writes);
}

TEST(Feedback, ShutdownSwitchesEverythingOffAndForgetsTheState) {
    setUp();
    OF_Prefs::toggles[OF_Const::autofire] = true;
    OF_FFB::burstFireActive = true;
    pullTriggerOnScreen();
    OF_FFB::RumbleActivation();
    CHECK(solenoidOn() && OF_FFB::rumbleHappening && OF_FFB::burstFiring);

    OF_FFB::FFBShutdown();
    CHECK(!solenoidOn());
    CHECK_EQ(testPinLevel[RUMBLE_PIN], 0);
    CHECK(!OF_FFB::rumbleHappening);
    CHECK(!OF_FFB::triggerHeld);
    CHECK(!OF_FFB::burstFiring);
    CHECK(!OF_FFB::solenoidFirstShot);
}

TEST(Feedback, RumbleRunsAtTheConfiguredStrengthForTheConfiguredTime) {
    setUp();
    OF_Prefs::settings[OF_Const::rumbleStrength] = 200;
    OF_Prefs::settings[OF_Const::rumbleInterval] = 150;
    OF_FFB::FFBOffScreen();             // an off screen shot with the trigger just pulled rumbles
    CHECK(OF_FFB::rumbleHappening);
    CHECK_EQ(testAnalogOut[RUMBLE_PIN], 200);

    OF_FFB::triggerHeld = true;
    testMillis += 149;
    OF_FFB::FFBOffScreen();
    CHECK(OF_FFB::rumbleHappening);
    testMillis += 2;
    OF_FFB::FFBOffScreen();
    CHECK(!OF_FFB::rumbleHappening);
    CHECK_EQ(testPinLevel[RUMBLE_PIN], 0);
}

TEST(Feedback, AHeldTriggerRumblesOnlyOnce) {
    setUp();
    OF_FFB::FFBOffScreen();
    OF_FFB::triggerHeld = true;
    testMillis += 500;
    OF_FFB::FFBOffScreen();
    CHECK(!OF_FFB::rumbleHappening);
    const int strength = testAnalogOut[RUMBLE_PIN];
    testAnalogOut[RUMBLE_PIN] = 0;
    testMillis += 500;
    OF_FFB::FFBOffScreen();
    CHECK_EQ(testAnalogOut[RUMBLE_PIN], 0);     // not started again
    (void)strength;

    // Letting go and pulling again rumbles again.
    OF_FFB::triggerHeld = false;
    OF_FFB::FFBRelease();
    OF_FFB::FFBOffScreen();
    CHECK(OF_FFB::rumbleHappening);
}

TEST(Feedback, ATriggerAlreadyHeldDoesNotStartARumble) {
    setUp();
    OF_FFB::triggerHeld = true;
    OF_FFB::FFBOffScreen();
    CHECK(!OF_FFB::rumbleHappening);
}

TEST(Feedback, RumbleIsOffWhenSwitchedOff) {
    setUp();
    OF_Prefs::toggles[OF_Const::rumble] = false;
    OF_FFB::FFBOffScreen();
    CHECK(!OF_FFB::rumbleHappening);
    CHECK_EQ(testAnalogOut[RUMBLE_PIN], 0);
}

TEST(Feedback, ARumbleStartedOnScreenKeepsGoingWhenTheGunMovesOffScreen) {
    setUp();
    OF_Prefs::settings[OF_Const::rumbleInterval] = 150;
    OF_FFB::RumbleActivation();
    OF_FFB::triggerHeld = true;
    testMillis += 200;
    OF_FFB::FFBOffScreen();           // continues the command to its end, and ends it
    CHECK(!OF_FFB::rumbleHappening);
}

TEST(Feedback, ForceFeedbackRumbleStandsInForTheSolenoid) {
    setUp();
    OF_Prefs::toggles[OF_Const::solenoid] = false;
    OF_Prefs::toggles[OF_Const::rumbleFF] = true;
    OF_Prefs::settings[OF_Const::rumbleInterval] = 150;
    OF_FFB::FFBOnScreen();             // the on screen shot rumbles
    CHECK(OF_FFB::rumbleHappening);

    // In this mode the pulse is half the interval.
    OF_FFB::triggerHeld = true;
    testMillis += 74;
    OF_FFB::FFBOnScreen();
    CHECK(OF_FFB::rumbleHappening);
    testMillis += 2;
    OF_FFB::FFBOnScreen();
    CHECK(!OF_FFB::rumbleHappening);
}

TEST(Feedback, ForceFeedbackRumbleWithAutofireStopsWhenTheTriggerIsReleased) {
    setUp();
    OF_Prefs::toggles[OF_Const::solenoid] = false;
    OF_Prefs::toggles[OF_Const::rumbleFF] = true;
    OF_Prefs::toggles[OF_Const::autofire] = true;
    OF_FFB::FFBOnScreen();
    OF_FFB::triggerHeld = true;
    testMillis += 1000;
    OF_FFB::FFBOnScreen();
    CHECK(OF_FFB::rumbleHappening);       // with autofire it goes on for as long as the trigger is held

    OF_FFB::triggerHeld = false;
    OF_FFB::FFBRelease();
    CHECK(!OF_FFB::rumbleHappening);
    CHECK_EQ(testPinLevel[RUMBLE_PIN], 0);
}

TEST(Feedback, RumbleAfterAReleaseIsAllowedAgain) {
    setUp();
    OF_FFB::rumbleHappened = true;
    OF_FFB::FFBRelease();
    CHECK(!OF_FFB::rumbleHappened);
}

TEST(Feedback, TemperatureIsTheAverageOfFourSamples) {
    setUp(true);
    testAnalogIn[TEMP_PIN] = readingFor(30);
    for (int sample = 0; sample < 3; sample++) {
        testMillis += 251;
        OF_FFB::TemperatureUpdate();
    }
    CHECK_EQ(OF_FFB::temperatureCurrent, 25);       // no reading yet after three samples
    testMillis += 251;
    OF_FFB::TemperatureUpdate();
    CHECK_NEAR(OF_FFB::temperatureCurrent, 30, 1);
}

TEST(Feedback, TemperatureIsOnlyReadEveryQuarterSecond) {
    setUp(true);
    testAnalogIn[TEMP_PIN] = readingFor(30);
    testMillis += 251;
    OF_FFB::TemperatureUpdate();
    CHECK_EQ(OF_FFB::tempGraphIndex, 1);
    testMillis += 100;
    OF_FFB::TemperatureUpdate();
    CHECK_EQ(OF_FFB::tempGraphIndex, 1);        // too soon
    testMillis += 160;
    OF_FFB::TemperatureUpdate();
    CHECK_EQ(OF_FFB::tempGraphIndex, 2);
}

TEST(Feedback, TemperatureStatusFollowsWarningAndShutdownWithHysteresis) {
    setUp(true);
    OF_Prefs::settings[OF_Const::tempWarning] = 38;
    OF_Prefs::settings[OF_Const::tempShutdown] = 45;

    readTemperature(30);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Safe);
    readTemperature(40);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Warning);
    readTemperature(36);                          // below the warning level, but not 5 degrees below
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Warning);
    readTemperature(33.3);                        // exactly 5 degrees below the level of 38 is still warm
    CHECK_EQ(OF_FFB::temperatureCurrent, 33);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Warning);
    readTemperature(32.3);
    CHECK_EQ(OF_FFB::temperatureCurrent, 32);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Safe);

    readTemperature(50);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Fatal);
    readTemperature(42);                          // below the shutdown level, but not 5 degrees below
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Fatal);
    readTemperature(40.3);                        // exactly 5 below the shutdown level of 45
    CHECK_EQ(OF_FFB::temperatureCurrent, 40);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Fatal);
    readTemperature(38);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Warning);
}

TEST(Feedback, ASensorOutOfRangeIsFatal) {
    setUp(true);
    for (int reading : {0, 99, 2501, 4095}) {
        OF_FFB::tempStatus = OF_FFB::Temp_Safe;
        OF_FFB::previousMillisTemp = 0;
        testAnalogIn[TEMP_PIN] = reading;
        testMillis += 251;
        OF_FFB::TemperatureUpdate();
        INFO("reading %d", reading);
        CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Fatal);
        CHECK_EQ(OF_FFB::temperatureCurrent, OF_Const::TEMPERATURE_SENSOR_ERROR_VALUE);
    }

    // And 100 and 2500 are still readings.
    for (int reading : {100, 2500}) {
        OF_FFB::tempStatus = OF_FFB::Temp_Safe;
        OF_FFB::tempGraphIndex = 0;
        testAnalogIn[TEMP_PIN] = reading;
        testMillis += 251;
        OF_FFB::TemperatureUpdate();
        INFO("reading %d", reading);
        CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Safe);
        CHECK_EQ(OF_FFB::tempGraphIndex, 1);
    }
}

TEST(Feedback, AnAbsurdlyHighAverageIsCappedAtTheErrorValue) {
    setUp(true);
    // Sensors that read 2500 give 190 degrees: capped to the error value, which is above the shutdown level.
    readTemperature(150);
    CHECK_EQ(OF_FFB::temperatureCurrent, OF_Const::TEMPERATURE_SENSOR_ERROR_VALUE);
    CHECK(OF_FFB::tempStatus == OF_FFB::Temp_Fatal);
}

TEST(Feedback, TooHotMeansNoShot) {
    setUp(true);
    OF_FFB::tempStatus = OF_FFB::Temp_Fatal;
    pullTriggerOnScreen();
    CHECK(!solenoidOn());                           // not even the first shot

    // Nor the repeats.
    OF_Prefs::toggles[OF_Const::autofire] = true;
    holdOnScreen(1000);
    CHECK(!solenoidOn());
}

TEST(Feedback, WithoutASensorTheTemperatureIsNotLookedAt) {
    setUp(false);
    OF_FFB::tempStatus = OF_FFB::Temp_Fatal;       // as if something had set it
    pullTriggerOnScreen();
    CHECK(solenoidOn());
}

TEST(Feedback, WarmSolenoidRestsLongerBetweenShots) {
    setUp(true);
    OF_Prefs::toggles[OF_Const::autofire] = true;
    testAnalogIn[TEMP_PIN] = readingFor(30);

    auto pulsesInTwoSeconds = [](OF_FFB::TempStatuses_e status) {
        setUp(true);
        OF_Prefs::toggles[OF_Const::autofire] = true;
        testAnalogIn[TEMP_PIN] = readingFor(25);
        OF_FFB::tempStatus = status;
        // Keep the status the test chose: the temperature thresholds are far from the reading's.
        OF_Prefs::settings[OF_Const::tempWarning] = status == OF_FFB::Temp_Warning ? 20 : 38;
        pullTriggerOnScreen();
        int pulses = 0;
        bool wasOn = solenoidOn();
        for (int i = 0; i < 2000; i++) {
            testMillis++;
            OF_FFB::FFBOnScreen();
            if (solenoidOn() && !wasOn) pulses++;
            wasOn = solenoidOn();
        }
        return pulses;
    };
    const int safe = pulsesInTwoSeconds(OF_FFB::Temp_Safe);
    const int warm = pulsesInTwoSeconds(OF_FFB::Temp_Warning);
    CHECK(safe > warm);
    CHECK(warm > 0);
}

TEST(Feedback, ASolenoidThatGetsTooHotIsSwitchedOffAtOnce) {
    setUp(true);
    OF_Prefs::toggles[OF_Const::autofire] = true;
    testAnalogIn[TEMP_PIN] = readingFor(30);
    pullTriggerOnScreen();
    CHECK(solenoidOn());
    OF_FFB::tempStatus = OF_FFB::Temp_Fatal;
    holdOnScreen(300);
    CHECK(!solenoidOn());
}

TEST(Feedback, ABurstEndsAtOnceWhenItGetsTooHot) {
    setUp(true);
    OF_FFB::burstFireActive = true;
    testAnalogIn[TEMP_PIN] = readingFor(30);
    pullTriggerOnScreen();
    CHECK(OF_FFB::burstFiring);
    OF_FFB::tempStatus = OF_FFB::Temp_Fatal;
    holdOnScreen(5);
    CHECK(!OF_FFB::burstFiring);
    CHECK(!solenoidOn());
    OF_FFB::tempStatus = OF_FFB::Temp_Safe;     // cooled down: the old burst does not resume
    const int writes = testPinWrites[SOLENOID_PIN];
    holdOnScreen(1000);
    CHECK_EQ(testPinWrites[SOLENOID_PIN], writes);
}
