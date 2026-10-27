// Shared by the tests of the firmware's own code (OpenFIREmain): puts the firmware's global state, which is all static,
// back to what it is at power-up before each test, and the fake file system with it.
#pragma once

#include <cstring>

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>
#include <TinyUSB_Devices.h>

#include "OpenFIREprefs.h"

namespace firmware {

// What OF_Prefs holds at power-up, taken the first time a test asks for a clean state.
struct PowerUpState {
    OF_Prefs::ProfileData_t profiles[PROFILE_COUNT];
    uint currentProfile;
    bool toggles[OF_Const::boolTypesCount];
    int8_t pins[OF_Const::boardInputsCount];
    uint32_t settings[OF_Const::settingsTypesCount];
    OF_Prefs::USBMap_t usb;
    uint8_t backupButtonDesc[ButtonCount][6];
    LightgunButtons::Desc_t buttonDesc[ButtonCount];
};

inline const PowerUpState& powerUp() {
    static PowerUpState state = [] {
        PowerUpState captured;
        memcpy(captured.profiles, OF_Prefs::profiles, sizeof(captured.profiles));
        captured.currentProfile = OF_Prefs::currentProfile;
        memcpy(captured.toggles, OF_Prefs::toggles, sizeof(captured.toggles));
        memcpy(captured.pins, OF_Prefs::pins, sizeof(captured.pins));
        memcpy(captured.settings, OF_Prefs::settings, sizeof(captured.settings));
        captured.usb = OF_Prefs::usb;
        memcpy(captured.backupButtonDesc, OF_Prefs::backupButtonDesc, sizeof(captured.backupButtonDesc));
        memcpy(captured.buttonDesc, LightgunButtons::ButtonDesc, sizeof(captured.buttonDesc));
        return captured;
    }();
    return state;
}

// Call at the start of a test.
inline void restorePowerUpState() {
    const PowerUpState& state = powerUp();
    memcpy(OF_Prefs::profiles, state.profiles, sizeof(state.profiles));
    OF_Prefs::currentProfile = state.currentProfile;
    memcpy(OF_Prefs::toggles, state.toggles, sizeof(state.toggles));
    memcpy(OF_Prefs::pins, state.pins, sizeof(state.pins));
    memcpy(OF_Prefs::settings, state.settings, sizeof(state.settings));
    OF_Prefs::usb = state.usb;
    memcpy(OF_Prefs::backupButtonDesc, state.backupButtonDesc, sizeof(state.backupButtonDesc));
    memcpy(LightgunButtons::ButtonDesc, state.buttonDesc, sizeof(state.buttonDesc));
    LittleFS.reset();

    // The fakes of the board go back to their start too: a screen that answers, and a gun on USB.
    Adafruit_SSD1306::beginSucceedsByDefault = true;
    TinyUSBDevices.onBattery = false;
}

} // namespace firmware
