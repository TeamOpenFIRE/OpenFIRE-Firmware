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
    if(OFPresets == nullptr) OFPresets = new OF_Const();
    LoadToggles();
    if(toggles[OF_Const::customPins]) LoadPins();
    if(pins[OF_Const::periphSDA] >= 0 && pins[OF_Const::periphSCL] >= 0) LoadPeriphs();
    LoadSettings();
    LoadUSBID();
    if(OFPresets != nullptr) {
        delete OFPresets;
        OFPresets = nullptr;
    }
}

int OF_Prefs::LoadProfiles()
{
    File prefsFile = LittleFS.open("/profiles.conf", "r");
    if(prefsFile) {
        int profileNum = 0;
        char buf[32];
        size_t bWritten = 0;
        size_t readSize = 0;
        if(OFPresets == nullptr) OFPresets = new OF_Const();
        while(prefsFile.available()) {
            bWritten = prefsFile.readBytesUntil('\0', buf, 32);
            // readBytesUntil discards the terminator, so plop one at the end
            buf[bWritten++] = '\0';
            if(bWritten && OFPresets->profSettingTypes_Strings.count(buf)) {
                switch(OFPresets->profSettingTypes_Strings.at(buf)) {
                  case OF_Const::profCurrent:
                      currentProfile = prefsFile.read();
                      if(currentProfile >= PROFILE_COUNT) currentProfile = 0;
                      break;
                  case OF_Const::profName:
                      profileNum = prefsFile.read();
                      readSize = prefsFile.read();
                      profileNum < PROFILE_COUNT ? prefsFile.readBytes(profiles[profileNum].name, readSize) : prefsFile.seek(readSize, fs::SeekCur);
                      break;
                  default:
                      profileNum = prefsFile.read();
                      readSize = prefsFile.read();
                      profileNum < PROFILE_COUNT ? prefsFile.readBytes((char*)&profiles[profileNum] + (sizeof(int)*OFPresets->profSettingTypes_Strings.at(buf)), readSize) : prefsFile.seek(readSize, fs::SeekCur);
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
        for(int i = 0; i < PROFILE_COUNT; ++i) {
            for(auto &pair : OFPresets->profSettingTypes_Strings) {
                if(pair.second == OF_Const::profCurrent) {
                    if(!currentProfLogged) {
                        // only write string and profile num
                        prefsFile.write(pair.first.c_str(), pair.first.length()+1);
                        prefsFile.write((uint8_t)currentProfile);
                        currentProfLogged = true;
                    }
                } else {
                    // write data type:
                    prefsFile.write(pair.first.c_str(), pair.first.length()+1);
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

int OF_Prefs::LoadPeriphs()
{
    File periphsFile = LittleFS.open("/i2cperiphs.conf", "r");
    if(periphsFile) {
        char buf[32];
        size_t bWritten = 0;
        size_t readSize = 0;
        while(periphsFile.available()) {
            bWritten = periphsFile.readBytesUntil('\0', buf, 32);
            // readBytesUntil discards the terminator, so plop one at the end
            buf[bWritten++] = '\0';
            
            if(bWritten && OFPresets->i2cDevicesTypes_Strings.count(buf)) {
                switch(OFPresets->i2cDevicesTypes_Strings.at(buf)) {
                case OF_Const::i2cDevicesEnabled:
                    while(periphsFile.peek() != OF_Const::serialTerminator) {
                        bWritten = periphsFile.readBytesUntil('\0', buf, 32);
                        buf[bWritten++] = '\0';
                        if(bWritten && OFPresets->i2cDevicesTypes_Strings.count(buf))
                            i2cPeriphs[OFPresets->i2cDevicesTypes_Strings.at(buf)] = periphsFile.read();
                        else periphsFile.seek(1, fs::SeekCur);
                    }
                    periphsFile.seek(1, fs::SeekCur);
                    break;
                case OF_Const::i2cOLED:
                    bWritten = periphsFile.readBytesUntil('\0', buf, 32);
                    buf[bWritten++] = '\0';
                    readSize = periphsFile.read();
                    if(bWritten && OFPresets->i2cOledTypes_Strings.count(buf))
                        periphsFile.readBytes((char*)&oledPrefs[OFPresets->i2cOledTypes_Strings.at(buf)], readSize);
                    else periphsFile.seek(readSize, fs::SeekCur);
                    break;
                }
            } else {
                // Note: technically this could be an issue with the devices types stream (due to its contiguous layout and not having a read length byte),
                // but since that section's a basic requirement anyways (and handled in the looping switch case above), it's unlikely to be a reproducible issue.
                periphsFile.readBytesUntil('\0', buf, 32);
                readSize = periphsFile.read();
                periphsFile.seek(readSize, fs::SeekCur);
            }
        }

        periphsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SavePeriphs()
{
    File periphsFile = LittleFS.open("/i2cperiphs.conf", "w");
    if(periphsFile) {
        for(auto &pair : OFPresets->i2cDevicesTypes_Strings) {
            // Devices enabled array - this should always be recognized
            if(pair.second == OF_Const::i2cDevicesEnabled) {
                periphsFile.write(pair.first.c_str(), pair.first.length()+1);
                for(auto &subPair : OFPresets->i2cDevicesTypes_Strings)
                    if(subPair.second != OF_Const::i2cDevicesEnabled) {
                        periphsFile.write(subPair.first.c_str(), subPair.first.length()+1);
                        periphsFile.write((uint8_t)i2cPeriphs[subPair.second]);
                    }
                periphsFile.write(OF_Const::serialTerminator);
            } else {
                switch(pair.second) {
                // OLED prefs
                case OF_Const::i2cOLED:
                    for(auto &oledPair : OFPresets->i2cOledTypes_Strings) {
                        // peripheral data always starts with device category->device setting name
                        periphsFile.write(pair.first.c_str(), pair.first.length()+1);
                        periphsFile.write(oledPair.first.c_str(), oledPair.first.length()+1);
                        // size of data to read
                        periphsFile.write(sizeof(uint32_t));
                        // pref data
                        periphsFile.write((uint8_t*)&oledPrefs[oledPair.second], sizeof(uint32_t));
                    }
                    break;
                default: break;
                }
            }
        }
        
        periphsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SaveToPtr(File prefsFile, void *dataPtr, const std::unordered_map<std::string, int> &mapPtr, const size_t &dataSize)
{
    if(prefsFile) {
        for(auto &pair : mapPtr) {
            if(pair.second >= 0) {
                prefsFile.write(pair.first.c_str(), pair.first.length()+1);
                prefsFile.write((uint8_t)dataSize);
                prefsFile.write((uint8_t*)dataPtr + (dataSize * pair.second), dataSize);
            }
        }
        
        prefsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadToPtr(File prefsFile, void *dataPtr, const std::unordered_map<std::string, int> &mapPtr)
{
    if(prefsFile) {
        char buf[32];
        size_t bWritten = 0;
        size_t dataSize = 0;
        while(prefsFile.available()) {
            bWritten = prefsFile.readBytesUntil('\0', buf, 32);
            // readBytesUntil discards the terminator, so plop one at the end
            buf[bWritten++] = '\0';
            dataSize = prefsFile.read();
            if(bWritten && mapPtr.count(buf))
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
        char buf[sizeof(USBMap_t::deviceName)];
        size_t bWritten = 0;
        while(idFile.available()) {
            // TODO: maybe just shove this into settings instead?
            switch(idFile.read()) {
            case 0:
              bWritten = idFile.readBytes(buf, sizeof(USBMap_t::devicePID));
              if(bWritten > 0) memcpy(&usb.devicePID, buf, sizeof(USBMap_t::devicePID));
              break;
            case 1:
              bWritten = idFile.readBytes(buf, sizeof(USBMap_t::deviceName));
              if(bWritten > 0) {
                  memset(usb.deviceName, '\0', sizeof(USBMap_t::deviceName));
                  strcpy(usb.deviceName, buf);
              }
              break;
            case 2:
            default:
              idFile.seek(sizeof(uint32_t));
              break;
            }
        }

        idFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SaveUSBID()
{
    File idFile = LittleFS.open("/USB.conf", "w");
    if(idFile) {
        idFile.write((uint8_t)0), idFile.write((uint8_t*)&usb.devicePID, sizeof(USBMap_t::devicePID));
        idFile.write((uint8_t)1), idFile.write((uint8_t*)usb.deviceName, sizeof(USBMap_t::deviceName));

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

    if(OFPresets == nullptr) OFPresets = new OF_Const();

    if(OFPresets->boardsPresetsMap.count(OPENFIRE_BOARD)) {
        for(int i = 0; i < OFPresets->boardsPresetsMap.at(OPENFIRE_BOARD).size(); ++i)
            if(OFPresets->boardsPresetsMap.at(OPENFIRE_BOARD).at(i) > -1)
                pins[OFPresets->boardsPresetsMap.at(OPENFIRE_BOARD).at(i)] = i;
    }
}
