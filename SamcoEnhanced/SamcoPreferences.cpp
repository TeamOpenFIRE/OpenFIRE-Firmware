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

#ifdef SAMCO_EEPROM_ENABLE
#include <EEPROM.h>
#endif // SAMCO_EEPROM_ENABLE

// 4 byte header ID
const SamcoPreferences::HeaderId_t SamcoPreferences::HeaderId = {'O', 'F', '0', '1'};

#ifdef SAMCO_EEPROM_ENABLE
void SamcoPreferences::WriteHeader()
{
    EEPROM.put(0, HeaderId.u32);
}

int SamcoPreferences::CheckHeader()
{
    uint32_t u32;
    EEPROM.get(0, u32);
    if(u32 != HeaderId.u32) {
        return Error_NoData;
    } else {
        return Error_Success;
    }
}

int SamcoPreferences::LoadProfiles()
{
    int status = CheckHeader();
    if(status == Error_Success) {
        profiles.selectedProfile = EEPROM.read(4);
        uint8_t* p = ((uint8_t*)profiles.pProfileData);
        for(unsigned int i = 0; i < sizeof(ProfileData_t) * profiles.profileCount; ++i) {
            p[i] = EEPROM.read(5 + i);
        }
        return Error_Success;
    } else {
        return status;
    }
}

int SamcoPreferences::SaveProfiles()
{
    WriteHeader();
    EEPROM.update(4, profiles.selectedProfile);
    uint8_t* p = ((uint8_t*)profiles.pProfileData);
    for(unsigned int i = 0; i < sizeof(ProfileData_t) * profiles.profileCount; ++i) {
        EEPROM.write(5 + i, p[i]);
    }

    // Remember that we need to commit changes to the virtual EEPROM on RP2040!
    EEPROM.commit();
    return Error_Success;
}

int SamcoPreferences::LoadToggles()
{
    int status = CheckHeader();
    if(status == Error_Success) {
        EEPROM.get(300, toggles);
        return Error_Success;
    } else {
        return status;
    }
}

int SamcoPreferences::SaveToggles()
{
    WriteHeader();
    EEPROM.put(300, toggles);
    EEPROM.commit();
    return Error_Success;
}

int SamcoPreferences::LoadPins()
{
    int status = CheckHeader();
    if(status == Error_Success) {
        EEPROM.get(350, pins);
        return Error_Success;
    } else {
        return status;
    }
}

int SamcoPreferences::SavePins()
{
    WriteHeader();
    EEPROM.put(350, pins);
    EEPROM.commit();
    return Error_Success;
}

int SamcoPreferences::LoadSettings()
{
    int status = CheckHeader();
    if(status == Error_Success) {
        EEPROM.get(400, settings);
        return Error_Success;
    } else {
        return status;
    }
}

int SamcoPreferences::SaveSettings()
{
    WriteHeader();
    EEPROM.put(400, settings);
    EEPROM.commit();
    return Error_Success;
}

int SamcoPreferences::LoadUSBID()
{
    int status = CheckHeader();
    if(status == Error_Success) {
        EEPROM.get(900, usb);
        return Error_Success;
    } else {
        return status;
    }
}

int SamcoPreferences::SaveUSBID()
{
    WriteHeader();
    EEPROM.put(900, usb);
    EEPROM.commit();
    return Error_Success;
}

void SamcoPreferences::ResetPreferences()
{
    for(uint16_t i = 0; i < EEPROM.length(); ++i) {
        EEPROM.update(i, 0);
    }

    EEPROM.commit();
}

#else

int SamcoPreferences::Load()
{
    return Error_NoStorage;
}

int SamcoPreferences::Save()
{
    return Error_NoStorage;
}

#endif // SAMCO_EEPROM_ENABLE
