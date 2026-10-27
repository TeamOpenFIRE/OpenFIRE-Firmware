/*!
 * @file OpenFIREprefs.cpp
 * @brief OpenFIRE file system loading/saving and presets access.
 *
 * @copyright Mike Lynch & That One Seong, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @date 2025
 */

#include "OpenFIREprefs.h"

int OF_Prefs::InitFS()
{
    if(LittleFS.begin())
        return Error_Success;
    else return Error_NoData;
}

void OF_Prefs::Load()
{
    LoadToggles();
    if(toggles[OF_Const::customPins]) LoadPins();
    LoadSettings();
    LoadUSBID();
    LoadButtons();
    for(int i = 0; i < ButtonCount; ++i)
        memcpy(&LightgunButtons::ButtonDesc[i].reportType, OF_Prefs::backupButtonDesc[i], sizeof(OF_Prefs::backupButtonDesc[i]));
}

int OF_Prefs::LoadProfiles()
{
    File prefsFile = LittleFS.open("/profiles.conf", "r");
    if(prefsFile) {
        int profileNum = 0;
        // Room for a name of up to 31 characters and the terminator.
        char buf[32];
        size_t bWritten = 0;
        size_t readSize = 0;
        while(prefsFile.available()) {
            bWritten = prefsFile.readBytesUntil('\0', buf, sizeof(buf) - 1);
            // readBytesUntil discards the terminator, so plop one at the end
            buf[bWritten++] = '\0';
            if(bWritten && OFPresets.profSettingTypes_Strings.count(buf)) {
                switch(OFPresets.profSettingTypes_Strings.at(buf)) {
                  case OF_Const::profCurrent:
                      currentProfile = prefsFile.read();
                      if(currentProfile >= PROFILE_COUNT) currentProfile = 0;
                      break;
                  default:
                      profileNum = prefsFile.read();
                      readSize = prefsFile.read();
                      // A setting is loaded into a profile that exists, from a record of the setting's own size (the name
                      // is 16 bytes, everything else a 32-bit number): anything else would write outside the profile.
                      if(profileNum < PROFILE_COUNT && readSize == (OFPresets.profSettingTypes_Strings.at(buf) == OF_Const::profName ? sizeof(ProfileData_t::name) : sizeof(uint32_t)))
                          prefsFile.readBytes((char*)&profiles[profileNum] + (sizeof(uint32_t) * OFPresets.profSettingTypes_Strings.at(buf)), readSize);
                      else prefsFile.seek(readSize, fs::SeekCur);
                      break;
                }
            } else {
                prefsFile.seek(1, fs::SeekCur);
                readSize = prefsFile.read();
                prefsFile.seek(readSize, fs::SeekCur);
            }
        }

        prefsFile.close();
        return Error_Success;
    } else return Error_Read;
}

int OF_Prefs::SaveProfiles()
{
    File prefsFile = LittleFS.open("/profiles.conf", "w");
    if(prefsFile) {
        bool currentProfLogged = false;
        for(size_t i = 0; i < PROFILE_COUNT; ++i) {
            for(auto &pair : OFPresets.profSettingTypes_Strings) {
                if(pair.second == OF_Const::profCurrent) {
                    if(!currentProfLogged) {
                        // only write string and profile num
                        prefsFile.write((const uint8_t*)pair.first.data(), pair.first.length()+1);
                        prefsFile.write((uint8_t)currentProfile);
                        currentProfLogged = true;
                    }
                } else {
                    // write data type:
                    prefsFile.write((const uint8_t*)pair.first.data(), pair.first.length()+1);
                    // Append profile number:
                    prefsFile.write((uint8_t*)&i, 1);

                    // data type:
                    switch(pair.second) {
                    // 16-bytes profile name
                    case OF_Const::profName:
                        prefsFile.write(sizeof(ProfileData_t::name));
                        prefsFile.write((uint8_t*)profiles[i].name, sizeof(ProfileData_t::name));
                        break;
                    // everything else is generic 32-bit data
                    default:
                        prefsFile.write(sizeof(int));
                        prefsFile.write((uint8_t*)&profiles[i] + (sizeof(int)*pair.second), sizeof(int));
                        break;
                    }
                }
            }
        }

        prefsFile.close();
        return Error_Success;
    } else return Error_Write;
}

int OF_Prefs::SaveToPtr(File prefsFile, void *dataPtr, const std::unordered_map<std::string_view, int> &mapPtr, const size_t &dataSize)
{
    if(prefsFile) {
        for(auto &pair : mapPtr) {
            if((pair.second >= 0 && dataPtr != backupButtonDesc) || dataPtr == backupButtonDesc && pair.second >= 0 && pair.second < ButtonCount) {
                prefsFile.write((const uint8_t*)pair.first.data(), pair.first.length()+1);
                prefsFile.write((uint8_t)dataSize);
                prefsFile.write((uint8_t*)dataPtr + (dataSize * pair.second), dataSize);
            }
        }
        
        prefsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadToPtr(File prefsFile, void *dataPtr, const std::unordered_map<std::string_view, int> &mapPtr, const size_t &elementSize, const size_t &elementCount)
{
    if(prefsFile) {
        // Room for a name of up to 31 characters and the terminator.
        char buf[32];
        size_t bWritten = 0;
        size_t dataSize = 0;
        while(prefsFile.available()) {
            bWritten = prefsFile.readBytesUntil('\0', buf, sizeof(buf) - 1);
            // readBytesUntil discards the terminator, so plop one at the end
            buf[bWritten++] = '\0';
            dataSize = prefsFile.read();
            // Only a setting that exists, with a record of its own size, is loaded: the name and the size come from the
            // file (or from an upload by the app), and an index from the map can be -1 ("Unmapped") or past the data.
            if(bWritten && mapPtr.count(buf) && dataSize == elementSize &&
               mapPtr.at(buf) >= 0 && (size_t)mapPtr.at(buf) < elementCount)
                prefsFile.readBytes((char*)dataPtr + (dataSize * mapPtr.at(buf)), dataSize);
            else prefsFile.seek(dataSize, fs::SeekCur);
        }

        prefsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadUSBID()
{
    File idFile = LittleFS.open("/USB.conf", "r");
    if(idFile) {
        idFile.readBytes((char*)&usb, sizeof(USBMap_t));

        idFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SaveUSBID()
{
    File idFile = LittleFS.open("/USB.conf", "w");
    if(idFile) {
        idFile.write((uint8_t*)&usb, sizeof(USBMap_t));

        idFile.close();
        return Error_Success;
    } else return Error_NoData;
}

void OF_Prefs::ResetPreferences()
{
    LittleFS.format();
}

void OF_Prefs::LoadPresets()
{
    memset(pins, -1, sizeof(OF_Prefs::pins));

    if(OFPresets.boardsPresetsMap.count(OPENFIRE_BOARD)) {
        for(int i = 0; i < OFPresets.boardsPresetsMap.at(OPENFIRE_BOARD).size(); ++i)
            if(OFPresets.boardsPresetsMap.at(OPENFIRE_BOARD).at(i) > -1)
                pins[OFPresets.boardsPresetsMap.at(OPENFIRE_BOARD).at(i)] = i;
    }

    // save buttons map to backup descriptor (important for initializing backupDesc for saving)
    // couldn't find a better place for this tbh lol
    for(int i = 0; i < ButtonCount; ++i)
        memcpy(OF_Prefs::backupButtonDesc[i], &LightgunButtons::ButtonDesc[i].reportType, sizeof(OF_Prefs::backupButtonDesc[i]));
}
