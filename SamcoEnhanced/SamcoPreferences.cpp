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

#ifdef SAMCO_EEPROM_ENABLE
#include <EEPROM.h>
#endif // SAMCO_EEPROM_ENABLE

// 4 byte header ID
/*
 * The latter two characters of the header correlate to the Save Table Version (see SamcoPreferences.h).
 * If the save table is ever changed (values added/removed or changed order/size),
 * the version number MUST be incremented in tandem so that the firmware will
 * appropriately update previously saved tables (TODO) or reset current NVRAM.
 * Failure to do this might cause save corruption or other undefined behavior.
 */
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
    if(u32 != HeaderId.u32)
        return Error_NoData;
    else return Error_Success;
}

int SamcoPreferences::LoadProfiles()
{
    int status = CheckHeader();
    if(status == Error_Success) {
        FW_Common::profiles.selectedProfile = EEPROM.read(4);
        uint8_t* p = ((uint8_t*)FW_Common::profiles.pProfileData);
        for(unsigned int i = 0; i < sizeof(ProfileData_t) * FW_Common::profiles.profileCount; ++i)
            p[i] = EEPROM.read(5 + i);
        
        return Error_Success;
    } else return status;
}

int SamcoPreferences::SaveProfiles()
{
    WriteHeader();
    EEPROM.put(4, FW_Common::profiles.selectedProfile);
    uint8_t* p = ((uint8_t*)FW_Common::profiles.pProfileData);
    for(unsigned int i = 0; i < sizeof(ProfileData_t) * FW_Common::profiles.profileCount; ++i)
        EEPROM.write(5 + i, p[i]);

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
    } else return status;
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
    } else return status;
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
    } else return status;
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
    } else return status;
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
    for(uint16_t i = 0; i < EEPROM.length(); ++i)
        EEPROM.put(i, 0);

    EEPROM.commit();
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
