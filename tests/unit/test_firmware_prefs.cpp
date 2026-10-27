// Tests for OpenFIREprefs: what the firmware keeps in flash (profiles, toggles, pins, settings, USB identity,
// button map), its file formats, and the board presets. The flash is the in-memory fake in ../stub/LittleFS.h.
#include "framework.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "firmware_support.h"

using firmware::restorePowerUpState;

namespace {

// The bytes of a record in the settings-style files: the name, a zero, the size of the data, and the data.
std::vector<uint8_t> record(const std::string& name, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> bytes(name.begin(), name.end());
    bytes.push_back(0);
    bytes.push_back((uint8_t)data.size());
    bytes.insert(bytes.end(), data.begin(), data.end());
    return bytes;
}

void append(std::vector<uint8_t>& file, const std::vector<uint8_t>& bytes) {
    file.insert(file.end(), bytes.begin(), bytes.end());
}

} // namespace

TEST(Prefs, TheProfileStructureMatchesTheOrderOfTheSharedProfileSettings) {
    // Profiles are stored and loaded by offset: setting number times four bytes. The shared constants and the structure must
    // agree, or a setting is written into its neighbour.
    using Profile = OF_Prefs::ProfileData_t;
    CHECK_EQ(offsetof(Profile, topOffset), 4 * OF_Const::profTopOffset);
    CHECK_EQ(offsetof(Profile, bottomOffset), 4 * OF_Const::profBottomOffset);
    CHECK_EQ(offsetof(Profile, leftOffset), 4 * OF_Const::profLeftOffset);
    CHECK_EQ(offsetof(Profile, rightOffset), 4 * OF_Const::profRightOffset);
    CHECK_EQ(offsetof(Profile, TLled), 4 * OF_Const::profTLled);
    CHECK_EQ(offsetof(Profile, TRled), 4 * OF_Const::profTRled);
    CHECK_EQ(offsetof(Profile, adjX), 4 * OF_Const::profAdjX);
    CHECK_EQ(offsetof(Profile, adjY), 4 * OF_Const::profAdjY);
    CHECK_EQ(offsetof(Profile, irSens), 4 * OF_Const::profIrSens);
    CHECK_EQ(offsetof(Profile, runMode), 4 * OF_Const::profRunMode);
    CHECK_EQ(offsetof(Profile, irLayout), 4 * OF_Const::profIrLayout);
    CHECK_EQ(offsetof(Profile, aspectRatio), 4 * OF_Const::profAR);
    CHECK_EQ(offsetof(Profile, color), 4 * OF_Const::profColor);
    CHECK_EQ(offsetof(Profile, name), 4 * OF_Const::profName);
    CHECK_EQ(OF_Const::profName + 1, OF_Const::profDataTypes - 0);
}

TEST(Prefs, EveryNameInAnInputMapIsDistinct) {
    // Names are the keys of the file format: two settings with the same name would overwrite one another.
    OF_Const constants;
    CHECK_EQ(constants.boolTypes_Strings.size(), OF_Const::boolTypesCount);
    CHECK_EQ(constants.settingsTypes_Strings.size(), OF_Const::settingsTypesCount);
    CHECK_EQ(constants.profSettingTypes_Strings.size(), OF_Const::profDataTypes + 1);   // the settings, and the current profile
    CHECK_EQ(constants.boardInputs_Strings.size(), OF_Const::boardInputsCount + 1);     // the inputs, and "Unmapped"
}

TEST(Prefs, InitFsMountsTheVolume) {
    restorePowerUpState();
    CHECK_EQ(OF_Prefs::InitFS(), OF_Prefs::Error_Success);
    LittleFS.mounts = false;
    CHECK_EQ(OF_Prefs::InitFS(), OF_Prefs::Error_NoData);
}

TEST(Prefs, TogglesAreSavedByNameWithASizeOfOne) {
    restorePowerUpState();
    OF_Prefs::toggles[OF_Const::autofire] = true;
    OF_Prefs::toggles[OF_Const::rumble] = false;
    CHECK_EQ(OF_Prefs::SaveToggles(), OF_Prefs::Error_Success);

    // The record of one toggle, as the Desktop App also reads it.
    const std::vector<uint8_t>& file = LittleFS.bytesOf("/toggles.conf");
    const std::vector<uint8_t> autofireOn = record("Autofire", {1});
    CHECK(std::search(file.begin(), file.end(), autofireOn.begin(), autofireOn.end()) != file.end());
    const std::vector<uint8_t> rumbleOff = record("Rumble", {0});
    CHECK(std::search(file.begin(), file.end(), rumbleOff.begin(), rumbleOff.end()) != file.end());

    // One record per toggle.
    size_t records = 0;
    for (size_t at = 0; at < file.size(); at += strlen((const char*)&file[at]) + 3) records++;
    CHECK_EQ(records, OF_Const::boolTypesCount);
}

TEST(Prefs, TogglesPinsAndSettingsSurvivePowerCycle) {
    restorePowerUpState();
    OF_Prefs::toggles[OF_Const::holdToPause] = true;
    OF_Prefs::toggles[OF_Const::commonAnode] = false;
    OF_Prefs::pins[OF_Const::btnTrigger] = 17;
    OF_Prefs::pins[OF_Const::rumblePin] = 3;
    OF_Prefs::pins[OF_Const::tempPin] = 28;
    OF_Prefs::settings[OF_Const::rumbleStrength] = 123;
    OF_Prefs::settings[OF_Const::customLEDcolor2] = 0x123456;
    OF_Prefs::settings[OF_Const::tempShutdown] = 61;
    CHECK_EQ(OF_Prefs::SaveToggles(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::SavePins(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::SaveSettings(), OF_Prefs::Error_Success);

    // Power cycle: the RAM goes back to its defaults, the flash stays.
    std::map<std::string, std::shared_ptr<fs::FileData>> flash = LittleFS.files;
    restorePowerUpState();
    LittleFS.files = flash;
    CHECK(!OF_Prefs::toggles[OF_Const::holdToPause]);
    CHECK_EQ(OF_Prefs::LoadToggles(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::LoadPins(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::LoadSettings(), OF_Prefs::Error_Success);

    CHECK(OF_Prefs::toggles[OF_Const::holdToPause]);
    CHECK(!OF_Prefs::toggles[OF_Const::commonAnode]);
    CHECK(OF_Prefs::toggles[OF_Const::solenoid]);   // untouched toggles come back as saved too
    CHECK_EQ(OF_Prefs::pins[OF_Const::btnTrigger], 17);
    CHECK_EQ(OF_Prefs::pins[OF_Const::rumblePin], 3);
    CHECK_EQ(OF_Prefs::pins[OF_Const::tempPin], 28);
    CHECK_EQ(OF_Prefs::settings[OF_Const::rumbleStrength], 123);
    CHECK_EQ(OF_Prefs::settings[OF_Const::customLEDcolor2], 0x123456);
    CHECK_EQ(OF_Prefs::settings[OF_Const::tempShutdown], 61);
}

TEST(Prefs, LoadingFromAMissingFileReportsNoData) {
    restorePowerUpState();
    CHECK_EQ(OF_Prefs::LoadToggles(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::LoadPins(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::LoadSettings(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::LoadButtons(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::LoadUSBID(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::LoadProfiles(), OF_Prefs::Error_Read);

    // Nothing changed.
    CHECK_EQ(OF_Prefs::settings[OF_Const::rumbleStrength], 255);
    CHECK(OF_Prefs::toggles[OF_Const::rumble]);
}

TEST(Prefs, SavingToAFullOrDamagedVolumeReportsAnError) {
    restorePowerUpState();
    LittleFS.refuseWrites = true;
    CHECK_EQ(OF_Prefs::SaveToggles(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::SavePins(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::SaveSettings(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::SaveButtons(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::SaveUSBID(), OF_Prefs::Error_NoData);
    CHECK_EQ(OF_Prefs::SaveProfiles(), OF_Prefs::Error_Write);
}

TEST(Prefs, UnknownRecordsAreSkippedByTheirSize) {
    restorePowerUpState();
    // A file from a newer firmware: a setting this one doesn't know, between two it does.
    std::vector<uint8_t>& file = LittleFS.bytesOf("/settings.conf");
    append(file, record("RumbPwr", {77, 0, 0, 0}));
    append(file, record("FutureSetting", {1, 2, 3, 4, 5, 6, 7}));
    append(file, record("SolOn", {99, 0, 0, 0}));
    CHECK_EQ(OF_Prefs::LoadSettings(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::settings[OF_Const::rumbleStrength], 77);
    CHECK_EQ(OF_Prefs::settings[OF_Const::solenoidOnLength], 99);
    CHECK_EQ(OF_Prefs::settings[OF_Const::solenoidOffLength], 80);     // not in the file: unchanged
}

TEST(Prefs, ATruncatedFileLoadsWhatIsThere) {
    restorePowerUpState();
    std::vector<uint8_t>& file = LittleFS.bytesOf("/settings.conf");
    append(file, record("RumbPwr", {77, 0, 0, 0}));
    append(file, record("RumbTime", {200, 0, 0, 0}));
    file.resize(file.size() - 2);       // the last record loses its end
    CHECK_EQ(OF_Prefs::LoadSettings(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::settings[OF_Const::rumbleStrength], 77);
}

TEST(Prefs, ARecordLargerThanItsSettingIsNotWrittenPastIt) {
    // The size in a record comes from the file, and from the Desktop App's upload. A record whose size is not the setting's
    // is skipped, not copied over whatever follows the setting in RAM.
    restorePowerUpState();
    uint8_t memory[16];
    memset(memory, 0xAA, sizeof(memory));
    const std::unordered_map<std::string_view, int> oneByteSetting = {{"X", 0}};
    std::vector<uint8_t>& file = LittleFS.bytesOf("/x.conf");
    append(file, record("X", std::vector<uint8_t>(8, 0x55)));
    OF_Prefs::LoadToPtr(LittleFS.open("/x.conf", "r"), memory, oneByteSetting, 1, 1);     // a setting of one byte
    for (int i = 0; i < 16; i++) CHECK_EQ(memory[i], 0xAA);       // and the setting itself was not loaded from it
}

TEST(Prefs, ARecordIsOnlyLoadedForASettingThatExists) {
    restorePowerUpState();
    uint8_t memory[8];
    memset(memory, 0xAA, sizeof(memory));
    // "Unmapped" is -1 in the pins' map, and "Far" is beyond the three settings the block has.
    const std::unordered_map<std::string_view, int> settings = {{"First", 0}, {"Unmapped", -1}, {"Far", 5}};
    std::vector<uint8_t>& file = LittleFS.bytesOf("/x.conf");
    append(file, record("Unmapped", {1}));
    append(file, record("Far", {2}));
    append(file, record("First", {3}));
    OF_Prefs::LoadToPtr(LittleFS.open("/x.conf", "r"), memory, settings, 1, 3);
    CHECK_EQ(memory[0], 3);
    for (int i = 1; i < 8; i++) CHECK_EQ(memory[i], 0xAA);
}

TEST(Prefs, ANameWithoutTheEndOfItIsNotReadPastTheBuffer) {
    restorePowerUpState();
    // 40 characters and no end in a file: the name is cut at 31 and the rest is read as records, nothing is written outside.
    std::vector<uint8_t>& file = LittleFS.bytesOf("/settings.conf");
    file.insert(file.end(), 40, 'A');
    append(file, record("RumbPwr", {77, 0, 0, 0}));
    CHECK_EQ(OF_Prefs::LoadSettings(), OF_Prefs::Error_Success);
}

TEST(Prefs, AProfileRecordOfTheWrongSizeIsSkipped) {
    restorePowerUpState();
    std::vector<uint8_t>& file = LittleFS.bytesOf("/profiles.conf");
    auto add = [&](const std::string& name, uint8_t profile, const std::vector<uint8_t>& data) {
        file.insert(file.end(), name.begin(), name.end());
        file.push_back(0);
        file.push_back(profile);
        file.push_back((uint8_t)data.size());
        file.insert(file.end(), data.begin(), data.end());
    };
    add("TopOffset", 1, std::vector<uint8_t>(60, 0x77));      // a number of 60 bytes
    add("Name", 1, std::vector<uint8_t>(40, 'z'));            // a name of 40 bytes
    add("IrSens", 1, {2, 0, 0, 0});
    CHECK_EQ(OF_Prefs::LoadProfiles(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::profiles[1].topOffset, 0);
    CHECK_STREQ(std::string(OF_Prefs::profiles[1].name), "Profile B");
    CHECK_EQ(OF_Prefs::profiles[1].irSens, 2);
    CHECK_EQ(OF_Prefs::profiles[1].runMode, 1);               // the neighbours are intact
    CHECK_EQ(OF_Prefs::profiles[2].topOffset, 0);
}

TEST(Prefs, SettingNamesAreSavedWholeAndTerminated) {
    restorePowerUpState();
    OF_Prefs::SaveSettings();
    const std::vector<uint8_t>& file = LittleFS.bytesOf("/settings.conf");
    // Records follow one another: name, 0, size 4, four bytes.
    size_t at = 0, records = 0;
    while (at < file.size()) {
        const size_t nameLength = strlen((const char*)&file[at]);
        REQUIRE(at + nameLength + 2 <= file.size());
        CHECK_EQ(file[at + nameLength + 1], 4);
        at += nameLength + 2 + 4;
        records++;
    }
    CHECK_EQ(at, file.size());
    CHECK_EQ(records, OF_Const::settingsTypesCount);
}

TEST(Prefs, ProfilesSurvivePowerCycle) {
    restorePowerUpState();
    OF_Prefs::currentProfile = 2;
    OF_Prefs::profiles[1].topOffset = -123;
    OF_Prefs::profiles[1].leftOffset = 456;
    OF_Prefs::profiles[1].adjX = 1234.5f;
    OF_Prefs::profiles[1].irSens = 2;
    OF_Prefs::profiles[1].runMode = FW_Const::RunMode_OneEuro;
    OF_Prefs::profiles[1].irLayout = OF_Const::layoutDiamond;
    OF_Prefs::profiles[1].color = 0xABCDEF;
    strcpy(OF_Prefs::profiles[1].name, "My profile");
    OF_Prefs::profiles[3].TLled = 777.25f;
    CHECK_EQ(OF_Prefs::SaveProfiles(), OF_Prefs::Error_Success);

    std::map<std::string, std::shared_ptr<fs::FileData>> flash = LittleFS.files;
    restorePowerUpState();
    LittleFS.files = flash;
    CHECK_EQ(OF_Prefs::LoadProfiles(), OF_Prefs::Error_Success);

    CHECK_EQ(OF_Prefs::currentProfile, 2);
    CHECK_EQ(OF_Prefs::profiles[1].topOffset, -123);
    CHECK_EQ(OF_Prefs::profiles[1].leftOffset, 456);
    CHECK_NEAR(OF_Prefs::profiles[1].adjX, 1234.5f, 0);
    CHECK_EQ(OF_Prefs::profiles[1].irSens, 2);
    CHECK_EQ(OF_Prefs::profiles[1].runMode, FW_Const::RunMode_OneEuro);
    CHECK_EQ(OF_Prefs::profiles[1].irLayout, OF_Const::layoutDiamond);
    CHECK_EQ(OF_Prefs::profiles[1].color, 0xABCDEF);
    CHECK_STREQ(std::string(OF_Prefs::profiles[1].name), "My profile");
    CHECK_NEAR(OF_Prefs::profiles[3].TLled, 777.25f, 0);

    // The others are as they were.
    CHECK_STREQ(std::string(OF_Prefs::profiles[0].name), "Profile A");
    CHECK_EQ(OF_Prefs::profiles[0].topOffset, 0);
    CHECK_EQ(OF_Prefs::profiles[2].runMode, 1);
}

TEST(Prefs, ProfileNamesKeepAllSixteenBytes) {
    restorePowerUpState();
    memset(OF_Prefs::profiles[0].name, 'x', sizeof(OF_Prefs::profiles[0].name));
    OF_Prefs::SaveProfiles();
    memset(OF_Prefs::profiles[0].name, 0, sizeof(OF_Prefs::profiles[0].name));
    OF_Prefs::LoadProfiles();
    for (char letter : OF_Prefs::profiles[0].name) CHECK_EQ(letter, 'x');
}

TEST(Prefs, ACurrentProfileBeyondTheLastIsReplacedByTheFirst) {
    restorePowerUpState();
    OF_Prefs::SaveProfiles();
    // Change the saved current profile to 9.
    std::vector<uint8_t>& file = LittleFS.bytesOf("/profiles.conf");
    const std::string name = "CurrentProf";
    auto at = std::search(file.begin(), file.end(), name.begin(), name.end());
    REQUIRE(at != file.end());
    *(at + name.size() + 1) = 9;
    OF_Prefs::currentProfile = 3;
    OF_Prefs::LoadProfiles();
    CHECK_EQ(OF_Prefs::currentProfile, 0);
}

TEST(Prefs, ARecordForAProfileThatDoesntExistIsSkipped) {
    restorePowerUpState();
    std::vector<uint8_t>& file = LittleFS.bytesOf("/profiles.conf");
    // "TopOffset" for profiles 4 (the first that doesn't exist) and 7: four bytes of data, which must not land anywhere.
    const std::string name = "TopOffset";
    for (uint8_t missing : {4, 7}) {
        file.insert(file.end(), name.begin(), name.end());
        file.push_back(0);
        file.push_back(missing);
        file.push_back(4);
        file.insert(file.end(), {0x11, 0x22, 0x33, 0x44});
    }
    // And then a valid one for profile 2.
    file.insert(file.end(), name.begin(), name.end());
    file.push_back(0);
    file.push_back(2);
    file.push_back(4);
    file.insert(file.end(), {0x05, 0, 0, 0});
    CHECK_EQ(OF_Prefs::LoadProfiles(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::profiles[2].topOffset, 5);
    for (int profile = 0; profile < PROFILE_COUNT; profile++) {
        if (profile != 2) CHECK_EQ(OF_Prefs::profiles[profile].topOffset, 0);
    }
    // Nothing next to the profiles in memory was written to either.
    const firmware::PowerUpState& defaults = firmware::powerUp();
    CHECK_EQ(OF_Prefs::currentProfile, defaults.currentProfile);
    CHECK_EQ(memcmp(OF_Prefs::toggles, defaults.toggles, sizeof(defaults.toggles)), 0);
    CHECK_EQ(memcmp(OF_Prefs::pins, defaults.pins, sizeof(defaults.pins)), 0);
    CHECK_EQ(memcmp(OF_Prefs::settings, defaults.settings, sizeof(defaults.settings)), 0);
    CHECK_EQ(memcmp(&OF_Prefs::usb, &defaults.usb, sizeof(defaults.usb)), 0);
}

TEST(Prefs, UnknownProfileRecordsAreSkipped) {
    restorePowerUpState();
    std::vector<uint8_t>& file = LittleFS.bytesOf("/profiles.conf");
    const std::string unknown = "NewThing";
    file.insert(file.end(), unknown.begin(), unknown.end());
    file.push_back(0);
    file.push_back(1);          // profile number
    file.push_back(3);          // size
    file.insert(file.end(), {9, 9, 9});
    const std::string known = "IrSens";
    file.insert(file.end(), known.begin(), known.end());
    file.push_back(0);
    file.push_back(0);
    file.push_back(4);
    file.insert(file.end(), {2, 0, 0, 0});
    CHECK_EQ(OF_Prefs::LoadProfiles(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::profiles[0].irSens, 2);
}

TEST(Prefs, UsbIdentitySurvivesPowerCycle) {
    restorePowerUpState();
    CHECK_EQ(OF_Prefs::usb.devicePID, 1);
    OF_Prefs::usb.devicePID = 3;
    strcpy(OF_Prefs::usb.deviceName, "FIRECon P3");
    CHECK_EQ(OF_Prefs::SaveUSBID(), OF_Prefs::Error_Success);
    CHECK_EQ(LittleFS.bytesOf("/USB.conf").size(), sizeof(OF_Prefs::USBMap_t));

    OF_Prefs::usb.devicePID = 1;
    strcpy(OF_Prefs::usb.deviceName, "other");
    CHECK_EQ(OF_Prefs::LoadUSBID(), OF_Prefs::Error_Success);
    CHECK_EQ(OF_Prefs::usb.devicePID, 3);
    CHECK_STREQ(std::string(OF_Prefs::usb.deviceName), "FIRECon P3");
}

TEST(Prefs, ResetFormatsTheVolume) {
    restorePowerUpState();
    OF_Prefs::SaveToggles();
    OF_Prefs::SaveProfiles();
    CHECK(LittleFS.exists("/toggles.conf"));
    OF_Prefs::ResetPreferences();
    CHECK_EQ(LittleFS.formatCount, 1);
    CHECK(!LittleFS.exists("/toggles.conf"));
    CHECK(!LittleFS.exists("/profiles.conf"));
}

TEST(Prefs, PresetsMapTheBoardsPinsToTheirFunctions) {
    restorePowerUpState();
    OF_Prefs::LoadPresets();
    // This test is built as a Raspberry Pi Pico: GPIO 0 is button A, 15 the trigger, 16 the solenoid, 17 the rumble motor.
    CHECK_STREQ(std::string(OPENFIRE_BOARD), "rpipico");
    CHECK_EQ(OF_Prefs::pins[OF_Const::btnGunA], 0);
    CHECK_EQ(OF_Prefs::pins[OF_Const::btnTrigger], 15);
    CHECK_EQ(OF_Prefs::pins[OF_Const::solenoidPin], 16);
    CHECK_EQ(OF_Prefs::pins[OF_Const::rumblePin], 17);
    CHECK_EQ(OF_Prefs::pins[OF_Const::camSDA], 20);
    CHECK_EQ(OF_Prefs::pins[OF_Const::tempPin], 28);

    // Functions the board has no pin for stay unmapped.
    CHECK_EQ(OF_Prefs::pins[OF_Const::analogX], -1);
    CHECK_EQ(OF_Prefs::pins[OF_Const::neoPixel], -1);
    CHECK_EQ(OF_Prefs::pins[OF_Const::wiiClockGen], -1);
}

TEST(Prefs, PresetsReplaceWhateverWasMappedBefore) {
    restorePowerUpState();
    OF_Prefs::pins[OF_Const::analogX] = 5;
    OF_Prefs::pins[OF_Const::btnGunA] = 12;
    OF_Prefs::LoadPresets();
    CHECK_EQ(OF_Prefs::pins[OF_Const::analogX], -1);
    CHECK_EQ(OF_Prefs::pins[OF_Const::btnGunA], 0);
}

TEST(Prefs, EveryBoardPresetNamesEachFunctionAtMostOnce) {
    // A function on two pins would only get the last one; the presets of every board are checked.
    OF_Const constants;
    for (const auto& board : constants.boardsPresetsMap) {
        int uses[OF_Const::boardInputsCount] = {};
        for (int function : board.second) {
            if (function >= 0) uses[function]++;
        }
        for (int function = 0; function < OF_Const::boardInputsCount; function++) {
            INFO("board %s, function %d", std::string(board.first).c_str(), function);
            CHECK(uses[function] <= 1);
        }
    }
}

TEST(Prefs, ThePresetsOfTheRpiPicoSeeAllTheButtons) {
    OF_Const constants;
    const std::vector<int>& pico = constants.boardsPresetsMap.at("rpipico");
    for (int button : {OF_Const::btnTrigger, OF_Const::btnGunA, OF_Const::btnGunB, OF_Const::btnGunC, OF_Const::btnStart, OF_Const::btnSelect,
                       OF_Const::btnGunUp, OF_Const::btnGunDown, OF_Const::btnGunLeft, OF_Const::btnGunRight, OF_Const::btnHome}) {
        CHECK(std::find(pico.begin(), pico.end(), button) != pico.end());
    }
}

TEST(Prefs, LoadPresetsKeepsTheButtonMapForSaving) {
    restorePowerUpState();
    OF_Prefs::LoadPresets();
    for (unsigned button = 0; button < ButtonCount; button++) {
        CHECK_EQ(memcmp(OF_Prefs::backupButtonDesc[button], &LightgunButtons::ButtonDesc[button].reportType, 6), 0);
    }
}

TEST(Prefs, TheButtonMapIsSavedByButtonAndLoadedBack) {
    restorePowerUpState();
    OF_Prefs::LoadPresets();
    CHECK_EQ(OF_Prefs::SaveButtons(), OF_Prefs::Error_Success);

    // One record per button, of six bytes.
    const std::vector<uint8_t>& file = LittleFS.bytesOf("/btns.conf");
    size_t at = 0, records = 0;
    while (at < file.size()) {
        const size_t nameLength = strlen((const char*)&file[at]);
        CHECK_EQ(file[at + nameLength + 1], 6);
        at += nameLength + 2 + 6;
        records++;
    }
    CHECK_EQ(records, ButtonCount);

    // Change the trigger's mouse button in the saved copy, load, and the descriptor follows.
    const std::string name = "Trigger";
    auto trigger = std::search(LittleFS.bytesOf("/btns.conf").begin(), LittleFS.bytesOf("/btns.conf").end(), name.begin(), name.end());
    REQUIRE(trigger != LittleFS.bytesOf("/btns.conf").end());
    *(trigger + name.size() + 2 + 1) = MOUSE_BUTTON5;      // the report code follows the report type
    OF_Prefs::toggles[OF_Const::customPins] = false;
    const uint8_t before = LightgunButtons::ButtonDesc[OF_Const::btnTrigger].reportCode;
    OF_Prefs::Load();
    CHECK_EQ(LightgunButtons::ButtonDesc[OF_Const::btnTrigger].reportCode, MOUSE_BUTTON5);
    CHECK(before != MOUSE_BUTTON5);
    LightgunButtons::ButtonDesc[OF_Const::btnTrigger].reportCode = before;      // leave the table as it was
}

TEST(Prefs, LoadReadsTheCustomPinsOnlyWhenTheyAreSwitchedOn) {
    restorePowerUpState();
    OF_Prefs::LoadPresets();
    OF_Prefs::pins[OF_Const::btnTrigger] = 20;
    OF_Prefs::SavePins();
    OF_Prefs::SaveToggles();
    OF_Prefs::LoadPresets();

    OF_Prefs::Load();           // the custom pins toggle is off
    CHECK_EQ(OF_Prefs::pins[OF_Const::btnTrigger], 15);

    OF_Prefs::toggles[OF_Const::customPins] = true;
    OF_Prefs::SaveToggles();
    OF_Prefs::LoadPresets();
    OF_Prefs::Load();
    CHECK_EQ(OF_Prefs::pins[OF_Const::btnTrigger], 20);
}

TEST(Prefs, PowerUpDefaultsAreSane) {
    restorePowerUpState();
    for (int profile = 0; profile < PROFILE_COUNT; profile++) {
        INFO("profile %d", profile);
        CHECK(FW_Const::IsProfileRunMode(OF_Prefs::profiles[profile].runMode));
        CHECK_EQ(OF_Prefs::profiles[profile].runMode, 1);
        CHECK_EQ(OF_Prefs::profiles[profile].irLayout, OF_Const::layoutSquare);
        CHECK(OF_Prefs::profiles[profile].irSens >= DFRobotIRPositionEx::Sensitivity_Min && OF_Prefs::profiles[profile].irSens <= DFRobotIRPositionEx::Sensitivity_Max);
        CHECK(OF_Prefs::profiles[profile].aspectRatio < OF_Const::aspectRatiosCount);
        CHECK(strlen(OF_Prefs::profiles[profile].name) < sizeof(OF_Prefs::profiles[profile].name));
    }
    CHECK(OF_Prefs::currentProfile < PROFILE_COUNT);
    CHECK(OF_Prefs::settings[OF_Const::tempWarning] < OF_Prefs::settings[OF_Const::tempShutdown]);
    CHECK_EQ(OF_Prefs::settings[OF_Const::analogMode], OF_Const::analogModeStick);
}
