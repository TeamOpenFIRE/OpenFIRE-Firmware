/*!
 * @file OpenFIREprefs.cpp
 * @brief Samco Prow Enhanced light gun preferences to save in non-volatile memory.
 *
 * @copyright Mike Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @version V1.1
 * @date 2023
 */

#include <LittleFS.h>

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
    if(toggles[OF_Const::customPins])
        LoadPins();
    if(pins[OF_Const::periphSDA])
        LoadPeriphs();
    LoadSettings();
    LoadUSBID();
}

int OF_Prefs::LoadProfiles()
{
    File prefs = LittleFS.open("profiles.conf", "r");
    if(prefs) {
        int profileNum = 0;
        char buf[sizeof(ProfileData_t::name)];
        while(prefs.available()) {
            switch(prefs.read()) {
            case Profile_ProfileNum:
                profileNum = prefs.read();
                prefs.seek(3, fs::SeekCur);
                break;
            case Profile_TopOffset:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::topOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].topOffset, &buf, sizeof(ProfileData_t::topOffset));
                break;
              }
            case Profile_BottomOffset:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::bottomOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].bottomOffset, &buf, sizeof(ProfileData_t::bottomOffset));
                break;
              }
            case Profile_LeftOffset:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::leftOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].leftOffset, &buf, sizeof(ProfileData_t::leftOffset));
                break;
              }
            case Profile_RightOffset:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::rightOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].rightOffset, &buf, sizeof(ProfileData_t::rightOffset));
                break;
              }
            case Profile_TLled:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::TLled));
                if(bWritten > 0) memcpy(&profiles[profileNum].TLled, &buf, sizeof(ProfileData_t::TLled));
                break;
              }
            case Profile_TRled:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::TRled));
                if(bWritten > 0) memcpy(&profiles[profileNum].TRled, &buf, sizeof(ProfileData_t::TRled));
                break;
              }
            case Profile_AdjX:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::adjX));
                if(bWritten > 0) memcpy(&profiles[profileNum].adjX, &buf, sizeof(ProfileData_t::adjX));
                break;
              }
            case Profile_AdjY:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::adjY));
                if(bWritten > 0) memcpy(&profiles[profileNum].adjY, &buf, sizeof(ProfileData_t::adjY));
                break;
              }
            case Profile_IrSens:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::irSens));
                if(bWritten > 0) memcpy(&profiles[profileNum].irSens, &buf, sizeof(ProfileData_t::irSens));
                break;
              }
            case Profile_RunMode:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::runMode));
                if(bWritten > 0) memcpy(&profiles[profileNum].runMode, &buf, sizeof(ProfileData_t::runMode));
                break;
              }
            case Profile_IrLayout:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::irLayout));
                if(bWritten > 0) memcpy(&profiles[profileNum].irLayout, &buf, sizeof(ProfileData_t::irLayout));
                break;
              }
            case Profile_Color:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::color));
                if(bWritten > 0) memcpy(&profiles[profileNum].color, &buf, sizeof(ProfileData_t::color));
                break;
              }
            case Profile_Name:
              {
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::name));
                if(bWritten > 0) {
                  memset(profiles[profileNum].name, '\0', sizeof(ProfileData_t::name));
                  sprintf(profiles[profileNum].name, buf);
                }
                break;
              }
            case Profile_Selected:
              currentProfile = prefs.read();
              break;
            default:
              prefs.seek(sizeof(uint32_t), fs::SeekCur);
              break;
            }
        }

        prefs.close();
        return Error_Success;
    } else return Error_Read;
}

int OF_Prefs::SaveProfiles()
{
    File prefs = LittleFS.open("profiles.conf", "w");
    if(prefs) {
        for(uint32_t i = 0; i < PROFILE_COUNT; i++) {
            // profile number
            prefs.write(Profile_ProfileNum), prefs.write((uint8_t*)&i, sizeof(uint32_t));
            // offsets
            prefs.write(Profile_TopOffset),    prefs.write((uint8_t*)&profiles[i].topOffset,    sizeof(ProfileData_t::topOffset));
            prefs.write(Profile_BottomOffset), prefs.write((uint8_t*)&profiles[i].bottomOffset, sizeof(ProfileData_t::bottomOffset));
            prefs.write(Profile_LeftOffset),   prefs.write((uint8_t*)&profiles[i].leftOffset,   sizeof(ProfileData_t::leftOffset));
            prefs.write(Profile_RightOffset),  prefs.write((uint8_t*)&profiles[i].rightOffset,  sizeof(ProfileData_t::rightOffset));
            // LED relatives
            prefs.write(Profile_TLled), prefs.write((uint8_t*)&profiles[i].TLled, sizeof(ProfileData_t::TLled));
            prefs.write(Profile_TRled), prefs.write((uint8_t*)&profiles[i].TRled, sizeof(ProfileData_t::TRled));
            // Adjustments
            prefs.write(Profile_AdjX), prefs.write((uint8_t*)&profiles[i].adjX, sizeof(ProfileData_t::adjX));
            prefs.write(Profile_AdjY), prefs.write((uint8_t*)&profiles[i].adjY, sizeof(ProfileData_t::adjY));
            // Other settings
            prefs.write(Profile_IrSens),   prefs.write((uint8_t*)&profiles[i].irSens,   sizeof(ProfileData_t::irSens));
            prefs.write(Profile_RunMode),  prefs.write((uint8_t*)&profiles[i].runMode,  sizeof(ProfileData_t::runMode));
            prefs.write(Profile_IrLayout), prefs.write((uint8_t*)&profiles[i].irLayout, sizeof(ProfileData_t::irLayout));
            prefs.write(Profile_Color),    prefs.write((uint8_t*)&profiles[i].color,    sizeof(ProfileData_t::color));
            // Name
            prefs.write(Profile_Name), prefs.write((uint8_t*)profiles[i].name, sizeof(ProfileData_t::name));
        }
        prefs.write(Profile_Selected), prefs.write(currentProfile);

        prefs.close();
        return Error_Success;
    } else return Error_Write;
}

int OF_Prefs::LoadToggles()
{
    File togglesFile = LittleFS.open("toggles.conf", "r");
    if(togglesFile) {
        while(togglesFile.available()) {
            int type = togglesFile.read();
            if(type > -1 && type < OF_Const::boolTypesCount)
                toggles[type] = togglesFile.read();
            else togglesFile.seek(1, fs::SeekCur);
        }
        
        togglesFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SaveToggles()
{
    File togglesFile = LittleFS.open("toggles.conf", "w");
    if(togglesFile) {
        for(uint8_t i = 0; i < OF_Const::boolTypesCount; i++)
            togglesFile.write(i), togglesFile.write((uint8_t)toggles[i]);

        togglesFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadPins()
{
    File pinsFile = LittleFS.open("pins.conf", "r");
    if(pinsFile) {
        while(pinsFile.available()) {
            int type = pinsFile.read();
            if(type > -1 && type < OF_Const::boardInputsCount)
                pins[type] = pinsFile.read();
            else pinsFile.seek(1, fs::SeekCur);
        }
        
        pinsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SavePins()
{
    File pinsFile = LittleFS.open("pins.conf", "w");
    if(pinsFile) {
        for(uint8_t i = 0; i < OF_Const::boardInputsCount; i++)
            pinsFile.write(i), pinsFile.write((uint8_t*)&pins[i], sizeof(int8_t));
        
        pinsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadSettings()
{
    File settingsFile = LittleFS.open("settings.conf", "r");
    if(settingsFile) {
        char buf[sizeof(uint32_t)];
        while(settingsFile.available()) {
            int type = settingsFile.read();
            if(type > -1 && type < OF_Const::settingsTypesCount) {
                int bWritten = settingsFile.readBytes(buf, sizeof(uint32_t));
                if(bWritten > 0) memcpy(&settings[type], buf, sizeof(uint32_t));
            } else settingsFile.seek(sizeof(uint32_t), fs::SeekCur);
        }

        settingsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SaveSettings()
{
    File settingsFile = LittleFS.open("settings.conf", "w");
    if(settingsFile) {
        for(uint8_t i = 0; i < OF_Const::settingsTypesCount; i++)
            settingsFile.write(i), settingsFile.write((uint8_t*)&settings[i], sizeof(uint32_t));
        
        settingsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadPeriphs()
{
    File periphsFile = LittleFS.open("i2cperiphs.conf", "r");
    if(periphsFile) {
        char buf[sizeof(uint32_t)];
        while(periphsFile.available()) {
            switch(periphsFile.read()) {
            case OF_Const::i2cDevicesEnabled:
            {
                while(periphsFile.available() && periphsFile.peek() != OF_Const::serialTerminator) {
                    int type = periphsFile.read();
                    if(type > -1 && type < OF_Const::i2cDevicesCount)
                        i2cPeriphs[type] = periphsFile.read();
                    else periphsFile.seek(sizeof(bool), fs::SeekCur);
                }
                if(periphsFile.peek() == OF_Const::serialTerminator) periphsFile.seek(1, fs::SeekCur);
                break;
            }
            case OF_Const::i2cOLED:
            {
                while(periphsFile.available() && periphsFile.peek() != OF_Const::serialTerminator) {
                    int type = periphsFile.read();
                    if(type > -1 && type < OF_Const::oledSettingsTypes) {
                        int bWritten = periphsFile.readBytes(buf, sizeof(uint32_t));
                        if(bWritten > 0) memcpy(&oledPrefs[type], buf, sizeof(uint32_t));
                    } else periphsFile.seek(sizeof(uint32_t), fs::SeekCur);
                }
                if(periphsFile.peek() == OF_Const::serialTerminator) periphsFile.seek(1, fs::SeekCur);
                break;
            }
            default:
                periphsFile.seek(0, fs::SeekEnd);
                break;
            }
        }

        periphsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::SavePeriphs()
{
    File periphsFile = LittleFS.open("i2cperiphs.conf", "w");
    if(periphsFile) {
        // Main "devices enabled" array
        periphsFile.write(OF_Const::i2cDevicesEnabled);
        for(uint8_t i = 0; i < OF_Const::i2cDevicesCount; i++) {
            periphsFile.write(i), periphsFile.write((uint8_t)i2cPeriphs[i]);
        }
        periphsFile.write(OF_Const::serialTerminator);

        // OLED settings
        periphsFile.write(OF_Const::i2cOLED);
        for(uint8_t i = 0; i < OF_Const::oledSettingsTypes; i++) {
            periphsFile.write(i), periphsFile.write((uint8_t*)&oledPrefs[i], sizeof(uint32_t));
        }
        periphsFile.write(OF_Const::serialTerminator);
        
        periphsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int OF_Prefs::LoadUSBID()
{
    File idFile = LittleFS.open("USB.conf", "r");
    if(idFile) {
        while(idFile.available()) {
            // TODO: maybe just shove this into settings instead?
            switch(idFile.read()) {
            case 0:
            {
              char buf[sizeof(USBMap_t::devicePID)];
              int bWritten = idFile.readBytes(buf, sizeof(USBMap_t::devicePID));
              if(bWritten > 0) memcpy(&usb.devicePID, buf, sizeof(USBMap_t::devicePID));
              break;
            }
            case 1:
            {
              char buf[sizeof(USBMap_t::deviceName)];
              int bWritten = idFile.readBytes(buf, sizeof(USBMap_t::deviceName));
              if(bWritten > 0) {
                  memset(usb.deviceName, '\0', sizeof(USBMap_t::deviceName));
                  strcpy(usb.deviceName, buf);
              }
              break;
            }
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
    File idFile = LittleFS.open("USB.conf", "w");
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
    for(int i = 0; i < OF_Const::boardInputsCount; i++)
        pins[i] = -1;

    if(OF_Const::boardsPresetsMap.count(OPENFIRE_BOARD)) {
        for(int i = 0; i < OF_Const::boardsPresetsMap.at(OPENFIRE_BOARD).size(); i++)
            if(OF_Const::boardsPresetsMap.at(OPENFIRE_BOARD).at(i) > -1)
                pins[OF_Const::boardsPresetsMap.at(OPENFIRE_BOARD).at(i)] = i;
    } else for(int i = 0; i < OF_Const::boardInputsCount; i++)
        pins[i] = -1;
}
