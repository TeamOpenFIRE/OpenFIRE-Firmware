#include <cstring>
#include <sys/_stdint.h>
#include "FS.h"
/*!
 * @file SamcoPreferences.cpp
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

#include "SamcoPreferences.h"
#include <Arduino.h>
#include "OpenFIREcommon.h"

#include "LittleFS.h"

int SamcoPreferences::InitFS()
{
    if(LittleFS.begin())
        return Error_Success;
    else return Error_NoData;
}

void SamcoPreferences::Load()
{
    LoadToggles();
    if(toggles[OF_Const::customPins])
        LoadPins();
    if(pins[OF_Const::periphSDA])
        LoadPeriphs();
    LoadSettings();
    LoadUSBID();
}

int SamcoPreferences::LoadProfiles()
{
    File prefs = LittleFS.open("profiles.conf", "r");
    if(prefs) {
        int profileNum = 0;
        while(prefs.available()) {
            switch(prefs.read()) {
            case Profile_ProfileNum:
                profileNum = prefs.read();
                prefs.seek(3, fs::SeekCur);
                break;
            case Profile_TopOffset:
              {
                char buf[sizeof(ProfileData_t::topOffset)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::topOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].topOffset, &buf, sizeof(buf));
                break;
              }
            case Profile_BottomOffset:
              {
                char buf[sizeof(ProfileData_t::bottomOffset)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::bottomOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].bottomOffset, &buf, sizeof(buf));
                break;
              }
            case Profile_LeftOffset:
              {
                char buf[sizeof(ProfileData_t::leftOffset)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::leftOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].leftOffset, &buf, sizeof(buf));
                break;
              }
            case Profile_RightOffset:
              {
                char buf[sizeof(ProfileData_t::rightOffset)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::rightOffset));
                if(bWritten > 0) memcpy(&profiles[profileNum].rightOffset, &buf, sizeof(buf));
                break;
              }
            case Profile_TLled:
              {
                char buf[sizeof(ProfileData_t::TLled)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::TLled));
                if(bWritten > 0) memcpy(&profiles[profileNum].TLled, &buf, sizeof(buf));
                break;
              }
            case Profile_TRled:
              {
                char buf[sizeof(ProfileData_t::TRled)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::TRled));
                if(bWritten > 0) memcpy(&profiles[profileNum].TRled, &buf, sizeof(buf));
                break;
              }
            case Profile_AdjX:
              {
                char buf[sizeof(ProfileData_t::adjX)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::adjX));
                if(bWritten > 0) memcpy(&profiles[profileNum].adjX, &buf, sizeof(buf));
                break;
              }
            case Profile_AdjY:
              {
                char buf[sizeof(ProfileData_t::adjY)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::adjY));
                if(bWritten > 0) memcpy(&profiles[profileNum].adjY, &buf, sizeof(buf));
                break;
              }
            case Profile_IrSens:
              {
                char buf[sizeof(ProfileData_t::irSens)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::irSens));
                if(bWritten > 0) memcpy(&profiles[profileNum].irSens, &buf, sizeof(buf));
                break;
              }
            case Profile_RunMode:
              {
                char buf[sizeof(ProfileData_t::runMode)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::runMode));
                if(bWritten > 0) memcpy(&profiles[profileNum].runMode, &buf, sizeof(buf));
                break;
              }
            case Profile_IrLayout:
              {
                char buf[sizeof(ProfileData_t::irLayout)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::irLayout));
                if(bWritten > 0) memcpy(&profiles[profileNum].irLayout, &buf, sizeof(buf));
                break;
              }
            case Profile_Color:
              {
                char buf[sizeof(ProfileData_t::color)];
                int bWritten = prefs.readBytes(buf, sizeof(ProfileData_t::color));
                if(bWritten > 0) memcpy(&profiles[profileNum].color, &buf, sizeof(buf));
                break;
              }
            case Profile_Name:
              {
                char buf[sizeof(ProfileData_t::name)];
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

int SamcoPreferences::SaveProfiles()
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
            prefs.write(Profile_Name), prefs.write(profiles[i].name, sizeof(ProfileData_t::name));
        }
        prefs.write(Profile_Selected), prefs.write(currentProfile);

        prefs.close();
        return Error_Success;
    } else return Error_Write;
}

int SamcoPreferences::LoadToggles()
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

int SamcoPreferences::SaveToggles()
{
    File togglesFile = LittleFS.open("toggles.conf", "w");
    if(togglesFile) {
        for(uint8_t i = 0; i < OF_Const::boolTypesCount; i++)
            togglesFile.write(i), togglesFile.write((uint8_t)toggles[i]);

        togglesFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int SamcoPreferences::LoadPins()
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

int SamcoPreferences::SavePins()
{
    File pinsFile = LittleFS.open("pins.conf", "w");
    if(pinsFile) {
        for(uint8_t i = 0; i < OF_Const::boardInputsCount; i++)
            pinsFile.write(i), pinsFile.write((uint8_t*)&pins[i], sizeof(int8_t));
        
        pinsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int SamcoPreferences::LoadSettings()
{
    File settingsFile = LittleFS.open("settings.conf", "r");
    if(settingsFile) {
        while(settingsFile.available()) {
            int type = settingsFile.read();
            if(type > -1 && type < OF_Const::settingsTypesCount) {
                char buf[sizeof(uint32_t)];
                int bWritten = settingsFile.readBytes(buf, sizeof(uint32_t));
                if(bWritten > 0) memcpy(&settings[type], buf, sizeof(uint32_t));
            } else settingsFile.seek(sizeof(uint32_t), fs::SeekCur);
        }

        settingsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int SamcoPreferences::SaveSettings()
{
    File settingsFile = LittleFS.open("settings.conf", "w");
    if(settingsFile) {
        for(uint8_t i = 0; i < OF_Const::settingsTypesCount; i++)
            settingsFile.write(i), settingsFile.write((uint8_t*)&settings[i], sizeof(uint32_t));
        
        settingsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int SamcoPreferences::LoadPeriphs()
{
    File periphsFile = LittleFS.open("i2cperiphs.conf", "r");
    if(periphsFile) {
        while(periphsFile.available()) {
            switch(periphsFile.read()) {
            case OF_Const::i2cDevicesEnabled:
            {
                int type = periphsFile.read();
                if(type > -1 && type < OF_Const::i2cDevicesCount)
                    i2cPeriphs[type] = periphsFile.read();
                break;
            }
            case OF_Const::i2cOLED:
            default:
                periphsFile.seek(sizeof(uint16_t), fs::SeekCur);
                break;
            }
        }

        periphsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int SamcoPreferences::SavePeriphs()
{
    File periphsFile = LittleFS.open("i2cperiphs.conf", "w");
    if(periphsFile) {
        for(uint8_t i = 0; i < OF_Const::i2cDevicesCount; i++) {
            periphsFile.write(OF_Const::i2cDevicesEnabled), periphsFile.write(i), periphsFile.write((uint8_t)i2cPeriphs[i]);
        }
        
        periphsFile.close();
        return Error_Success;
    } else return Error_NoData;
}

int SamcoPreferences::LoadUSBID()
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

int SamcoPreferences::SaveUSBID()
{
    File idFile = LittleFS.open("USB.conf", "w");
    if(idFile) {
        idFile.write((uint8_t)0), idFile.write((uint8_t*)&usb.devicePID, sizeof(USBMap_t::devicePID));
        idFile.write((uint8_t)1), idFile.write(usb.deviceName, sizeof(USBMap_t::deviceName));

        idFile.close();
        return Error_Success;
    } else return Error_NoData;
}

void SamcoPreferences::ResetPreferences()
{
    LittleFS.format();
}

void SamcoPreferences::LoadPresets()
{
    for(int i = 0; i < OF_Const::boardInputsCount; i++)
        pins[i] = -1;

    if(OF_Const::boardsPresetsMap.count(OPENFIRE_BOARD)) {
        for(int i = 0; i < sizeof(OF_Const::boardMap_t); i++)
            if(OF_Const::boardsPresetsMap.at(OPENFIRE_BOARD).pin[i] > -1)
                pins[OF_Const::boardsPresetsMap.at(OPENFIRE_BOARD).pin[i]] = i;
    } else for(int i = 0; i < OF_Const::boardInputsCount; i++)
        pins[i] = -1;
}
