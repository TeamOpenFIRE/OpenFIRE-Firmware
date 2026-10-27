// Tests for OpenFIREcommon: the setup of the pins, the camera and the display, the gun modes, the profile and run mode
// selection, saving to flash, and the button bindings. The loops that need a player (calibration, aiming) are in
// test_firmware_aiming.cpp.
#include "framework.h"

#include <cstring>
#include <string>

#include <hardware/pwm.h>
#include <pico/stdlib.h>

#include "world.h"
#include "OpenFIREFeedback.h"
#include "OpenFIREserial.h"

#define private public
#include "OpenFIRElights.h"
#undef private

using world::bootGun;

namespace {

bool printed(const char* text) { return Serial.output.find(text) != std::string::npos; }

} // namespace

TEST(Common, FeedbackSetUsesThePinsThatAreMapped) {
    bootGun();
    firmware::restorePowerUpState();
    OF_Prefs::LoadPresets();
    OF_Prefs::pins[OF_Const::rumbleSwitch] = 3;
    OF_Prefs::pins[OF_Const::solenoidSwitch] = 4;
    OF_Prefs::pins[OF_Const::autofireSwitch] = 5;
    for (int pin = 0; pin < 64; pin++) testPinMode[pin] = -1;
    FW_Common::FeedbackSet();

    CHECK_EQ(testPinMode[OF_Prefs::pins[OF_Const::rumblePin]], OUTPUT);
    CHECK_EQ(testPinMode[OF_Prefs::pins[OF_Const::solenoidPin]], OUTPUT);
    CHECK_EQ(testPinMode[3], INPUT_PULLUP);
    CHECK_EQ(testPinMode[4], INPUT_PULLUP);
    CHECK_EQ(testPinMode[5], INPUT_PULLUP);
    CHECK(OF_Prefs::toggles[OF_Const::rumble]);
    CHECK(OF_Prefs::toggles[OF_Const::solenoid]);
}

TEST(Common, FeedbackSetSwitchesOffWhatHasNoPin) {
    bootGun();
    OF_Prefs::pins[OF_Const::rumblePin] = -1;
    OF_Prefs::pins[OF_Const::solenoidPin] = -1;
    OF_Prefs::toggles[OF_Const::rumble] = true;
    OF_Prefs::toggles[OF_Const::solenoid] = true;
    FW_Common::FeedbackSet();
    CHECK(!OF_Prefs::toggles[OF_Const::rumble]);
    CHECK(!OF_Prefs::toggles[OF_Const::solenoid]);
}

TEST(Common, TheAnalogStickNeedsTwoDifferentPinsThatAreNotTheTemperatureSensor) {
    bootGun();
    struct Case { int x, y, temp; bool valid; };
    const Case cases[] = {
        {26, 27, 28, true}, {26, 27, -1, true}, {-1, 27, 28, false}, {26, -1, 28, false}, {26, 26, 28, false},
        {28, 27, 28, false}, {26, 28, 28, false},
    };
    for (const Case& test : cases) {
        OF_Prefs::pins[OF_Const::analogX] = test.x;
        OF_Prefs::pins[OF_Const::analogY] = test.y;
        OF_Prefs::pins[OF_Const::tempPin] = test.temp;
        FW_Common::FeedbackSet();
        INFO("X %d, Y %d, temperature %d", test.x, test.y, test.temp);
        CHECK_EQ(FW_Common::analogIsValid, test.valid);
    }
}

TEST(Common, TheFourPinLedNeedsAllThreePins) {
    bootGun();
    FW_Common::FeedbackSet();
    CHECK(FW_Common::ledIsValid);
    CHECK_EQ(testPinMode[OF_Prefs::pins[OF_Const::ledR]], OUTPUT);
    CHECK_EQ(testPinMode[OF_Prefs::pins[OF_Const::ledG]], OUTPUT);
    CHECK_EQ(testPinMode[OF_Prefs::pins[OF_Const::ledB]], OUTPUT);

    OF_Prefs::pins[OF_Const::ledG] = -1;
    FW_Common::FeedbackSet();
    CHECK(!FW_Common::ledIsValid);
}

TEST(Common, AnExternalNeoPixelStrandIsStartedWhenItsPinIsMapped) {
    bootGun();
    OF_Prefs::pins[OF_Const::neoPixel] = 22;
    OF_Prefs::settings[OF_Const::customLEDcount] = 6;
    FW_Common::FeedbackSet();
    REQUIRE(OF_RGB::externPixel != nullptr);
    CHECK_EQ(OF_RGB::externPixel->numPixels(), 6);
    CHECK_EQ(OF_RGB::externPixel->pin, 22);
}

TEST(Common, TheDisplayStartsOnlyOnAnotherBusThanTheCamera) {
    bootGun();
    OF_Prefs::toggles[OF_Const::i2cOLED] = true;
    FW_Common::FeedbackSet();
    CHECK(FW_Common::OLED.display != nullptr);              // the Pico's presets: camera on I2C0, peripherals on I2C1
    FW_Common::OLED.Stop();

    // Both on the same bus: the display would take the camera's line.
    OF_Prefs::pins[OF_Const::periphSDA] = OF_Prefs::pins[OF_Const::camSDA];
    OF_Prefs::pins[OF_Const::periphSCL] = OF_Prefs::pins[OF_Const::camSCL];
    FW_Common::FeedbackSet();
    CHECK(FW_Common::OLED.display == nullptr);
}

TEST(Common, TheDisplayIsLeftOffWhenItsToggleIsOffOrItDoesNotAnswer) {
    bootGun();
    OF_Prefs::toggles[OF_Const::i2cOLED] = false;
    FW_Common::FeedbackSet();
    CHECK(FW_Common::OLED.display == nullptr);

    OF_Prefs::toggles[OF_Const::i2cOLED] = true;
    Adafruit_SSD1306::beginSucceedsByDefault = false;
    FW_Common::FeedbackSet();
    CHECK(FW_Common::OLED.display == nullptr);
    Adafruit_SSD1306::beginSucceedsByDefault = true;
}

TEST(Common, PinsResetGivesTheBoardBackItsPins) {
    bootGun();
    OF_Prefs::pins[OF_Const::neoPixel] = 22;
    OF_Prefs::toggles[OF_Const::i2cOLED] = true;
    FW_Common::FeedbackSet();
    REQUIRE(FW_Common::dfrIRPos != nullptr);
    REQUIRE(OF_RGB::externPixel != nullptr);
    REQUIRE(FW_Common::OLED.display != nullptr);

    OF_Prefs::pins[OF_Const::rumbleSwitch] = 3;
    OF_Prefs::pins[OF_Const::solenoidSwitch] = 4;
    OF_Prefs::pins[OF_Const::autofireSwitch] = 5;
    FW_Common::PinsReset();

    CHECK(FW_Common::dfrIRPos == nullptr);
    CHECK(OF_RGB::externPixel == nullptr);
    CHECK(FW_Common::OLED.display == nullptr);
    for (int function : {OF_Const::rumblePin, OF_Const::solenoidPin, OF_Const::rumbleSwitch, OF_Const::solenoidSwitch,
                         OF_Const::autofireSwitch, OF_Const::ledR, OF_Const::ledG, OF_Const::ledB}) {
        INFO("function %d", function);
        CHECK_EQ(testPinMode[OF_Prefs::pins[function]], INPUT);
    }
    CHECK_EQ(OF_RGB::neopixel.shown[0], 0);                 // the lights are off
}

TEST(Common, TheCameraIsStartedOnTheBusOfItsPins) {
    bootGun();
    REQUIRE(FW_Common::dfrIRPos != nullptr);
    CHECK_EQ(Wire.sdaPin, OF_Prefs::pins[OF_Const::camSDA]);        // 20 and 21: I2C0
    CHECK_EQ(Wire.sclPin, OF_Prefs::pins[OF_Const::camSCL]);
    CHECK(!FW_Common::camNotAvailable);

    // The camera is started with the profile's sensitivity, at the board's clock.
    OF_Prefs::profiles[OF_Prefs::currentProfile].irSens = DFRobotIRPositionEx::Sensitivity_Max;
    Wire1 = TwoWire();
    OF_Prefs::pins[OF_Const::camSDA] = 2;
    OF_Prefs::pins[OF_Const::camSCL] = 3;
    FW_Common::PinsReset();
    FW_Common::CameraSet();
    REQUIRE(FW_Common::dfrIRPos != nullptr);
    CHECK_EQ(Wire1.sdaPin, 2);                                      // the other bus
    CHECK_EQ(Wire1.sclPin, 3);
    CHECK_EQ(Wire1.events[0].value, DFROBOT_IR_IIC_CLOCK);
}

TEST(Common, WithoutCameraPinsOnABusThereIsNoCamera) {
    bootGun();
    FW_Common::PinsReset();
    for (const auto& pins : {std::pair<int, int>{-1, -1}, {20, 20}, {3, 2}, {1, 18}, {18, 1}}) {
        OF_Prefs::pins[OF_Const::camSDA] = pins.first;
        OF_Prefs::pins[OF_Const::camSCL] = pins.second;
        Serial.clear();
        testMillis += 4000;                 // the message waits for three seconds after the last one (or the start)
        FW_Common::camWarningTimestamp = 0;
        FW_Common::CameraSet();
        INFO("SDA %d, SCL %d", pins.first, pins.second);
        CHECK(FW_Common::dfrIRPos == nullptr);
        CHECK(FW_Common::camNotAvailable);
        CHECK(printed("CAMERROR"));
    }
}

TEST(Common, ACameraThatDoesNotAnswerIsDropped) {
    bootGun();
    FW_Common::PinsReset();
    Wire.transmissionResults = {2};     // the first message to the camera is not acknowledged
    Serial.clear();
    testMillis += 4000;
    FW_Common::camWarningTimestamp = 0;
    FW_Common::CameraSet();
    CHECK(FW_Common::dfrIRPos == nullptr);
    CHECK(FW_Common::camNotAvailable);
    CHECK(printed("CAMERROR: Not available"));
}

TEST(Common, TheCameraErrorIsOnlyPrintedEveryThreeSeconds) {
    bootGun();
    testMillis += 4000;
    FW_Common::camWarningTimestamp = 0;
    Serial.clear();
    FW_Common::PrintIrError();
    const size_t first = Serial.output.size();
    CHECK(first > 0);
    testMillis += 1000;
    FW_Common::PrintIrError();
    CHECK_EQ(Serial.output.size(), first);
    testMillis += 2100;
    FW_Common::PrintIrError();
    CHECK(Serial.output.size() > first);
}

TEST(Common, WhileSavingWhenDockedTheCameraErrorIsAMessageForTheApp) {
    bootGun();
    FW_Common::dockedSaving = true;
    Serial.clear();
    FW_Common::PrintIrError();
    FW_Common::dockedSaving = false;
    CHECK_EQ(Serial.output.size(), 2);
    CHECK_EQ((uint8_t)Serial.output[0], OF_Const::sError);
    CHECK_EQ((uint8_t)Serial.output[1], OF_Const::sErrCam);
}

TEST(Common, TheCameraClockGeneratorIsStartedOrStopped) {
    bootGun();
    FW_Common::PinsReset();
    OF_Prefs::pins[OF_Const::wiiClockGen] = 6;
    FW_Common::CameraSet();
    CHECK_EQ(testSysClockKhz, 125000);
    CHECK_EQ(testGpioFunction[6], GPIO_FUNC_PWM);
    const PwmSlice& slice = testPwmSlice[pwm_gpio_to_slice_num(6)];
    CHECK(slice.enabled);
    CHECK_EQ(slice.wrap, 4);
    CHECK_EQ(slice.level[pwm_gpio_to_channel(6)], 2);       // a 50% duty: 31.25 MHz for the camera from 125 MHz
    CHECK_NEAR(slice.divider, 1.0, 0);
}

TEST(Common, WithoutAClockGeneratorTheSystemClockIsTheOtherOne) {
    bootGun();
    FW_Common::PinsReset();
    OF_Prefs::pins[OF_Const::wiiClockGen] = -1;
    FW_Common::CameraSet();
    CHECK_EQ(testSysClockKhz, 133000);
}

TEST(Common, WithoutAClockGeneratorNoPinIsTouched) {
    // With no pin for the clock generator, nothing is set up for one: the SDK would take the pin -1 as 4294967295 and write to
    // memory beyond its tables.
    bootGun();
    FW_Common::PinsReset();
    OF_Prefs::pins[OF_Const::wiiClockGen] = -1;
    testGpioInvalidCalls = 0;
    FW_Common::CameraSet();
    CHECK_EQ(testGpioInvalidCalls, 0);
}

TEST(Common, SetModeEnteringRunStartsWithAllLedsSeenAndShowsTheProfile) {
    bootGun();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    CHECK(FW_Common::gunMode == FW_Const::GunMode_Run);
    CHECK_EQ(FW_Common::lastSeen, 0x0F);
}

TEST(Common, SetModeLeavingRunReleasesEverythingAndFlagsThePreferences) {
    bootGun();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::buttons.ReportEnable();
    FW_Common::stateFlags = 0;
    OF_FFB::triggerHeld = true;
    AbsMouse5.press(MOUSE_LEFT);

    FW_Common::SetMode(FW_Const::GunMode_Pause);
    CHECK_EQ(AbsMouse5.buttons, 0);
    CHECK(!OF_FFB::triggerHeld);
    CHECK_EQ(FW_Common::buttons.report, 0);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_PrintPreferences);
}

TEST(Common, SetModePauseIsOneChanceToSaveAndShowsTheMenu) {
    bootGun();
    OF_Prefs::toggles[OF_Const::i2cOLED] = true;
    FW_Common::FeedbackSet();
    REQUIRE(FW_Common::OLED.display != nullptr);
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::stateFlags = 0;
    FW_Common::SetMode(FW_Const::GunMode_Pause);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_PrintSelectedProfile);
    CHECK_EQ(FW_Common::pauseModeSelection, FW_Const::PauseMode_Calibrate);
    CHECK(FW_Common::OLED.display->text.find(" A > Profile A") != std::string::npos);

    OF_Prefs::toggles[OF_Const::simplePause] = true;
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::SetMode(FW_Const::GunMode_Pause);
    CHECK(FW_Common::OLED.display->text.find("Calibrate") != std::string::npos);
    FW_Common::OLED.Stop();
}

TEST(Common, SetModeToTheSameModeDoesNothing) {
    bootGun();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    FW_Common::stateFlags = 0;
    OF_Prefs::toggles[OF_Const::i2cOLED] = false;
    FW_Common::SetMode(FW_Const::GunMode_Run);
    CHECK_EQ(FW_Common::stateFlags, 0);
}

TEST(Common, SetModeDockedAllowsSavingAndUndockingSaysSo) {
    bootGun();
    FW_Common::stateFlags = 0;
    FW_Common::SetMode(FW_Const::GunMode_Docked);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);
    Serial.clear();
    FW_Common::SetMode(FW_Const::GunMode_Run);
    CHECK(printed("Undocking."));

    // Going from Docked to calibration is not an undocking.
    FW_Common::SetMode(FW_Const::GunMode_Docked);
    Serial.clear();
    FW_Common::SetMode(FW_Const::GunMode_Calibration);
    CHECK(!printed("Undocking."));
}

TEST(Common, TheLedShowsTheMode) {
    bootGun();
    FW_Common::SetMode(FW_Const::GunMode_Calibration);
    CHECK_EQ(OF_RGB::neopixel.shown[0], OF_RGB::CalModeColor);

    OF_Prefs::profiles[0].color = 0x336699;
    FW_Common::SetMode(FW_Const::GunMode_Pause);
    CHECK_EQ(OF_RGB::neopixel.shown[0], 0x336699);

    FW_Common::SetMode(FW_Const::GunMode_Run);
    CHECK_EQ(OF_RGB::neopixel.shown[0], 0);                 // all LEDs seen at the start of run mode
    FW_Common::lastSeen = 0;
    FW_Common::SetLedColorFromMode();
    CHECK_EQ(OF_RGB::neopixel.shown[0], OF_RGB::IRSeen0Color);
}

TEST(Common, SetRunModeSetsTheModeAndRemembersItInTheProfile) {
    bootGun();
    FW_Common::stateFlags = 0;
    FW_Common::SetRunMode(FW_Const::RunMode_Average2);
    CHECK(FW_Common::runMode == FW_Const::RunMode_Average2);
    CHECK_EQ(OF_Prefs::profiles[OF_Prefs::currentProfile].runMode, FW_Const::RunMode_Average2);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);

    // The test mode is used, but not stored.
    FW_Common::stateFlags = 0;
    FW_Common::SetRunMode(FW_Const::RunMode_Processing);
    CHECK(FW_Common::runMode == FW_Const::RunMode_Processing);
    CHECK_EQ(OF_Prefs::profiles[OF_Prefs::currentProfile].runMode, FW_Const::RunMode_Average2);
    CHECK_EQ(FW_Common::stateFlags, 0);

    // Setting the stored mode again changes nothing to save.
    FW_Common::SetRunMode(FW_Const::RunMode_Average2);
    CHECK_EQ(FW_Common::stateFlags, 0);

    // A mode that doesn't exist is refused.
    FW_Common::SetRunMode((FW_Const::RunMode_e)FW_Const::RunMode_Count);
    CHECK(FW_Common::runMode == FW_Const::RunMode_Average2);
}

TEST(Common, SelectingAProfileAppliesItsSettings) {
    bootGun();
    OF_Prefs::profiles[2].irSens = DFRobotIRPositionEx::Sensitivity_High;
    OF_Prefs::profiles[2].runMode = FW_Const::RunMode_OneEuro;
    OF_Prefs::profiles[2].adjX = 2222;
    OF_Prefs::profiles[2].adjY = 1111;
    FW_Common::stateFlags = 0;
    Wire.events.clear();

    CHECK(FW_Common::SelectCalProfile(2));
    CHECK_EQ(OF_Prefs::currentProfile, 2);
    CHECK(FW_Common::runMode == FW_Const::RunMode_OneEuro);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_PrintSelectedProfile);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);

    // The camera was told its sensitivity (registers 0x06, 0x08 and 0x1A for "high").
    bool wrotePrimary = false;
    for (const TwoWire::Event& event : Wire.events) {
        if (event.kind == TwoWire::Event::Transmission && event.bytes.size() == 2 && event.bytes[0] == 0x08 && event.bytes[1] == 0x41) wrotePrimary = true;
    }
    CHECK(wrotePrimary);
}

TEST(Common, AProfileThatDoesntExistIsRefused) {
    bootGun();
    CHECK(!FW_Common::SelectCalProfile(PROFILE_COUNT));
    CHECK(!FW_Common::SelectCalProfile(200));
    CHECK_EQ(OF_Prefs::currentProfile, 0);
}

TEST(Common, AProfileWithAnUnknownSensitivityOrRunModeKeepsTheCurrentOnes) {
    bootGun();
    OF_Prefs::profiles[1].irSens = 9;
    OF_Prefs::profiles[1].runMode = FW_Const::RunMode_Processing;      // not allowed in a profile
    FW_Common::SetRunMode(FW_Const::RunMode_Average);
    CHECK(FW_Common::SelectCalProfile(1));
    CHECK(FW_Common::runMode == FW_Const::RunMode_Average);
}

TEST(Common, SelectingTheSameProfileDoesntAskToPrintItAgain) {
    bootGun();
    FW_Common::stateFlags = 0;
    FW_Common::SelectCalProfile(0);
    CHECK_EQ(FW_Common::stateFlags & FW_Const::StateFlag_PrintSelectedProfile, 0);
}

TEST(Common, SensitivityIsChangedInTheCameraAndTheProfile) {
    bootGun();
    FW_Common::stateFlags = 0;
    Wire.events.clear();
    FW_Common::SetIrSensitivity(DFRobotIRPositionEx::Sensitivity_Max);
    CHECK_EQ(OF_Prefs::profiles[0].irSens, DFRobotIRPositionEx::Sensitivity_Max);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);
    CHECK(!Wire.events.empty());

    FW_Common::stateFlags = 0;
    FW_Common::SetIrSensitivity(DFRobotIRPositionEx::Sensitivity_Max);
    CHECK_EQ(FW_Common::stateFlags, 0);                     // nothing changed to save

    FW_Common::SetIrSensitivity(DFRobotIRPositionEx::Sensitivity_Max + 1);
    CHECK_EQ(OF_Prefs::profiles[0].irSens, DFRobotIRPositionEx::Sensitivity_Max);
}

TEST(Common, SensitivityNeedsNoCamera) {
    bootGun();
    FW_Common::PinsReset();
    FW_Common::SetIrSensitivity(DFRobotIRPositionEx::Sensitivity_High);
    CHECK_EQ(OF_Prefs::profiles[0].irSens, DFRobotIRPositionEx::Sensitivity_High);
}

TEST(Common, TheLayoutIsChangedInTheProfile) {
    bootGun();
    FW_Common::stateFlags = 0;
    FW_Common::SetIrLayout(OF_Const::layoutDiamond);
    CHECK_EQ(OF_Prefs::profiles[0].irLayout, OF_Const::layoutDiamond);
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);
    FW_Common::stateFlags = 0;
    FW_Common::SetIrLayout(OF_Const::layoutDiamond);
    CHECK_EQ(FW_Common::stateFlags, 0);
    FW_Common::SetIrLayout(OF_Const::layoutTypes);          // not a layout
    CHECK_EQ(OF_Prefs::profiles[0].irLayout, OF_Const::layoutDiamond);
}

namespace {

// Saves with the given gun mode and the save enabled, and returns what SavePreferences() returned.
int savePreferencesIn(FW_Const::GunMode_e mode) {
    FW_Common::gunMode = mode;
    FW_Common::stateFlags = FW_Const::StateFlag_SavePreferencesEn;
    return FW_Common::SavePreferences();
}

} // namespace

TEST(Common, SavingWritesEveryFileAndSaysSo) {
    bootGun();
    Serial.clear();
    CHECK_EQ(savePreferencesIn(FW_Const::GunMode_Pause), OF_Prefs::Error_Success);
    for (const char* file : {"/profiles.conf", "/toggles.conf", "/settings.conf", "/btns.conf", "/USB.conf"}) {
        INFO("file %s", file);
        CHECK(LittleFS.exists(file));
    }
    CHECK(!LittleFS.exists("/pins.conf"));              // the pins are only saved when custom ones are on
    CHECK(printed("Settings saved to Flash"));
    CHECK(!(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn));
}

TEST(Common, TheCustomPinsAreSavedWhenTheyAreOn) {
    bootGun();
    OF_Prefs::toggles[OF_Const::customPins] = true;
    CHECK_EQ(savePreferencesIn(FW_Const::GunMode_Pause), OF_Prefs::Error_Success);
    CHECK(LittleFS.exists("/pins.conf"));
}

TEST(Common, SavingIsOnlyDoneOncePerPauseUnlessSomethingChanged) {
    bootGun();
    CHECK_EQ(savePreferencesIn(FW_Const::GunMode_Pause), OF_Prefs::Error_Success);
    LittleFS.reset();
    CHECK_EQ(FW_Common::SavePreferences(), OF_Prefs::Error_Success);       // the flag was cleared by the first save
    CHECK(!LittleFS.exists("/profiles.conf"));              // so nothing was written
}

TEST(Common, DockedSavingAlwaysSavesAndTellsTheApp) {
    bootGun();
    FW_Common::stateFlags = 0;
    FW_Common::gunMode = FW_Const::GunMode_Docked;
    Serial.clear();
    CHECK_EQ(FW_Common::SavePreferences(), OF_Prefs::Error_Success);
    CHECK(LittleFS.exists("/profiles.conf"));
    CHECK((uint8_t)Serial.output[0] == OF_Const::sSave);
    CHECK(printed("Successfully saved"));
}

TEST(Common, AFailedSaveReportsTheFirstErrorAndAllowsAnotherTry) {
    bootGun();
    LittleFS.refuseWrites = true;
    Serial.clear();
    CHECK_EQ(savePreferencesIn(FW_Const::GunMode_Pause), OF_Prefs::Error_Write);        // the profiles come first
    CHECK(printed("Error saving Preferences to Flash."));
    CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);

    LittleFS.refuseWrites = false;
    CHECK_EQ(FW_Common::SavePreferences(), OF_Prefs::Error_Success);
}

TEST(Common, ASaveThatFailsPartwayIsAFailure) {
    // The volume fills up after some files: whichever file is the first that can't be written is the failure, and the save
    // can be tried again. The files are the profiles, toggles, settings, buttons and the USB id, in that order.
    const int expectedError[5] = {OF_Prefs::Error_Write, OF_Prefs::Error_NoData, OF_Prefs::Error_NoData, OF_Prefs::Error_NoData, OF_Prefs::Error_NoData};
    for (int filesThatFit = 0; filesThatFit < 5; filesThatFit++) {
        bootGun();
        LittleFS.writesAllowed = filesThatFit;
        INFO("%d files fit", filesThatFit);
        CHECK_EQ(savePreferencesIn(FW_Const::GunMode_Pause), expectedError[filesThatFit]);
        CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);
    }

    // Any one file alone failing makes the save fail, whichever it is. The pins are one more file when they are custom.
    for (bool customPins : {false, true}) {
        const int files = customPins ? 6 : 5;
        for (int failing = 0; failing < files; failing++) {
            bootGun();
            OF_Prefs::toggles[OF_Const::customPins] = customPins;
            LittleFS.failWriteNumber = failing;
            INFO("custom pins %d, file %d fails", customPins, failing);
            CHECK(savePreferencesIn(FW_Const::GunMode_Pause) != OF_Prefs::Error_Success);
            CHECK(FW_Common::stateFlags & FW_Const::StateFlag_SavePreferencesEn);
        }
    }
}

TEST(Common, SavingShowsItsProgressOnTheDisplay) {
    bootGun();
    OF_Prefs::toggles[OF_Const::i2cOLED] = true;
    FW_Common::FeedbackSet();
    REQUIRE(FW_Common::OLED.display != nullptr);
    savePreferencesIn(FW_Const::GunMode_Pause);
    // The success screen was shown, and then the pause screen came back.
    CHECK(FW_Common::OLED.display->displayed > 3);
    FW_Common::OLED.Stop();
}

TEST(Common, TheLedBlinksAfterSaving) {
    bootGun();
    const unsigned long before = testMillis;
    savePreferencesIn(FW_Const::GunMode_Pause);
    CHECK(testMillis - before >= 3 * (55 + 40));            // three blinks of the saved colour

    LittleFS.refuseWrites = true;
    const unsigned long beforeFailing = testMillis;
    savePreferencesIn(FW_Const::GunMode_Pause);
    CHECK(testMillis - beforeFailing >= 2 * (145 + 60));    // and two longer ones for a failure
}

TEST(Common, TheButtonsTakeTheirPinsFromThePinsMapWhenDockedOrNew) {
    bootGun();
    OF_Prefs::pins[OF_Const::btnTrigger] = 11;
    OF_Prefs::pins[OF_Const::btnHome] = 12;
    FW_Common::gunMode = FW_Const::GunMode_Docked;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].pin, 11);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Home].pin, 12);

    // In other modes the pins stay.
    OF_Prefs::pins[OF_Const::btnTrigger] = 13;
    FW_Common::gunMode = FW_Const::GunMode_Pause;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].pin, 11);
}

TEST(Common, UpdatingTheBindingsInRunModeReleasesEveryButton) {
    bootGun();
    FW_Common::gunMode = FW_Const::GunMode_Run;
    const int releases = FW_Common::buttons.releaseAllCount;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(FW_Common::buttons.releaseAllCount, releases + 1);
}

TEST(Common, StartAndSelectFollowThePlayerNumber) {
    bootGun();
    struct Case { int player; char start, select; };
    const Case cases[] = {{1, '1', '5'}, {2, '2', '6'}, {3, '3', '7'}, {4, '4', '8'}, {0, '1', '5'}, {5, '1', '5'}};
    for (const Case& test : cases) {
        OF_Prefs::usb.devicePID = test.player;
        FW_Common::gunMode = FW_Const::GunMode_Docked;
        FW_Common::UpdateBindings(true);
        INFO("player %d", test.player);
        CHECK_EQ(FW_Common::playerStartBtn, test.start);
        CHECK_EQ(FW_Common::playerSelectBtn, test.select);
    }
}

TEST(Common, TheStartAndSelectButtonsReportThePlayersKeys) {
    bootGun();
    OF_Prefs::usb.devicePID = 3;
    FW_Common::gunMode = FW_Const::GunMode_Docked;
    FW_Common::UpdateBindings(true);
    // The table has 0xFF and 0xFE in the place of the keys; they become '3' and '7' for player 3.
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].reportCode, '3');
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].reportCode2, '3');
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Select].reportCode, '7');

    // And for another player, the old keys are not left behind: the table is rebuilt from the saved one.
    OF_Prefs::usb.devicePID = 2;
    FW_Common::UpdateBindings(true);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].reportCode, '2');
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Select].reportCode, '6');
}

TEST(Common, LowButtonsModePutsStartAndSelectOnAAndBWhenOffScreen) {
    bootGun();
    OF_Prefs::usb.devicePID = 1;
    OF_Prefs::toggles[OF_Const::lowButtonsMode] = true;
    FW_Common::gunMode = FW_Const::GunMode_Docked;
    FW_Common::UpdateBindings(true);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportType2, LightgunButtons::ReportType_Keyboard);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode2, '1');
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportCode2, '5');
}

TEST(Common, SerialMappingsOfMameHookerChangeTheBindings) {
    bootGun();
    OF_Prefs::usb.devicePID = 1;
    FW_Common::gunMode = FW_Const::GunMode_Docked;

    // The trigger shoots with button A's code when off screen.
    OF_Serial::serialMappingsOffscreenShot = true;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportCode2, LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode);
    OF_Serial::serialMappingsOffscreenShot = false;

    // The pedal does what A does, or what B does.
    OF_Serial::serialMappingsPedalMode = 1;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].reportCode, LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode);
    OF_Serial::serialMappingsPedalMode = 2;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].reportCode, LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportCode);
    OF_Serial::serialMappingsPedalMode = 3;                // not a mode: the pedal is as saved
    FW_Common::UpdateBindings(false);
    CHECK_EQ(memcmp(&LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].reportType, OF_Prefs::backupButtonDesc[FW_Const::BtnIdx_Pedal], 6), 0);
    OF_Serial::serialMappingsPedalMode = 0;

    // With one button, A becomes the reload.
    OF_Prefs::toggles[OF_Const::lowButtonsMode] = true;
    OF_Serial::serialMappingsOffscreenShot = true;
    FW_Common::UpdateBindings(false);
    CHECK_EQ(LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode, OF_Prefs::backupButtonDesc[FW_Const::BtnIdx_Reload][1]);
    OF_Serial::serialMappingsOffscreenShot = false;
}

TEST(Common, RedrawingTheDisplayShowsTheDockedOrPauseScreen) {
    bootGun();
    OF_Prefs::toggles[OF_Const::i2cOLED] = true;
    FW_Common::FeedbackSet();
    REQUIRE(FW_Common::OLED.display != nullptr);

    FW_Common::gunMode = FW_Const::GunMode_Docked;
    FW_Common::OLED.display->clearDisplay();
    FW_Common::RedrawDisplay();
    CHECK_EQ(FW_Common::OLED.display->bitmaps, 2);          // the splash

    FW_Common::gunMode = FW_Const::GunMode_Pause;
    FW_Common::OLED.display->clearDisplay();
    FW_Common::RedrawDisplay();
    CHECK(FW_Common::OLED.display->text.find(" A > Profile A") != std::string::npos);

    OF_Prefs::toggles[OF_Const::simplePause] = true;
    FW_Common::OLED.display->clearDisplay();
    FW_Common::RedrawDisplay();
    CHECK(FW_Common::OLED.display->text.find("Save Gun Settings") != std::string::npos);

    // In run mode nothing is redrawn.
    FW_Common::gunMode = FW_Const::GunMode_Run;
    FW_Common::OLED.display->clearDisplay();
    FW_Common::RedrawDisplay();
    CHECK(FW_Common::OLED.display->text.empty());
    FW_Common::OLED.Stop();
}

TEST(Common, TheLedTurnsOffWhenLedsAreSeenAndAmberWhenTheyAreLost) {
    for (int layout : {OF_Const::layoutSquare, OF_Const::layoutDiamond}) {
        bootGun(layout == OF_Const::layoutDiamond);
        FW_Common::gunMode = FW_Const::GunMode_Run;
        OF_Serial::serialMode = false;
        // The tracker has seen nothing yet: the state "nothing seen".
        FW_Common::lastSeen = 0;
        OF_RGB::SetLedPackedColor(OF_RGB::IRSeen0Color);

        // Four LEDs come into view.
        const int ledX[4] = {300, 700, 300, 700}, ledY[4] = {200, 200, 600, 600};
        if (layout == OF_Const::layoutDiamond) {
            FW_Common::OpenFIREdiamond.begin(ledX, ledY, 0x0F);
        } else {
            FW_Common::OpenFIREsquare.begin(ledX, ledY, 0x0F);
        }
        FW_Common::UpdateLastSeen();
        INFO("layout %d", layout);
        CHECK_EQ(FW_Common::lastSeen, 0x0F);
        CHECK_EQ(OF_RGB::neopixel.shown[0], 0);

        // Nothing in view any more.
        if (layout == OF_Const::layoutDiamond) {
            FW_Common::OpenFIREdiamond.begin(ledX, ledY, 0);
        } else {
            FW_Common::OpenFIREsquare.begin(ledX, ledY, 0);
        }
        FW_Common::UpdateLastSeen();
        CHECK_EQ(FW_Common::lastSeen, 0);
        CHECK_EQ(OF_RGB::neopixel.shown[0], OF_RGB::IRSeen0Color);
    }
}

TEST(Common, WithMameHookerInControlTheLedIsLeftToIt) {
    bootGun();
    FW_Common::gunMode = FW_Const::GunMode_Run;
    OF_Serial::serialMode = true;
    OF_RGB::SetLedPackedColor(0x123456);
    FW_Common::lastSeen = 0x0F;
    const int ledX[4] = {300, 700, 300, 700}, ledY[4] = {200, 200, 600, 600};
    FW_Common::OpenFIREsquare.begin(ledX, ledY, 0);
    FW_Common::UpdateLastSeen();
    CHECK_EQ(OF_RGB::neopixel.shown[0], 0x123456);
    OF_Serial::serialMode = false;
}
