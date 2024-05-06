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
const SamcoPreferences::HeaderId_t SamcoPreferences::HeaderId = {'P', 'r', 'o', 'w'};

#ifdef SAMCO_EEPROM_ENABLE
int SamcoPreferences::Load()
{
    uint32_t u32;
    EEPROM.get(0, u32);
    if(u32 != HeaderId.u32) {
        return Error_NoData;
    }

    preferences.profile = EEPROM.read(4);
    uint8_t* p = ((uint8_t*)preferences.pProfileData);
    for(unsigned int i = 0; i < sizeof(ProfileData_t) * preferences.profileCount; ++i) {
        p[i] = EEPROM.read(5 + i);
    }
    return Error_Success;
}

int SamcoPreferences::Save()
{
    EEPROM.put(0, HeaderId.u32);
    EEPROM.write(4, preferences.profile);
    uint8_t* p = ((uint8_t*)preferences.pProfileData);
    for(unsigned int i = 0; i < sizeof(ProfileData_t) * preferences.profileCount; ++i) {
        EEPROM.write(5 + i, p[i]);
    }

    // Remember that we need to commit changes to the virtual EEPROM on RP2040!
    EEPROM.commit();
    return Error_Success;
}

void SamcoPreferences::LoadExtended(uint8_t *dataBools, int8_t *dataMappings, uint16_t *dataSettings)
{
    // Sizes of the arrays: dataBools = 1, dataMappings = 27, dataSettings = 8
    // Basic booleans
    (*dataBools) = EEPROM.read(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 1);

    // Custom Pins (checks if this feature is enabled first, but space is always reserved)
    (*dataMappings) = EEPROM.read(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 2);
    if(*dataMappings) { // dataMappings[0] corresponds to the custom pin bool.
        for(uint8_t i = 1; i < 27; ++i) {
            *(dataMappings + i) = EEPROM.read(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 2 + i);
        }
    }

    // Main Settings
    uint8_t n = 0;
    for(uint8_t i = 0; i < 8; ++i) {
        EEPROM.get(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 1 + 27 + 1 + n, *(dataSettings + i));
        n += 2;
    }
    return;
}

int SamcoPreferences::SaveExtended(uint8_t *dataBools, int8_t *dataMappings, uint16_t *dataSettings)
{
    // Basic booleans
    EEPROM.update(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 1, *dataBools);

    // Custom Pins (always gets saved, regardless of if it's used or not)
    for(uint8_t i = 0; i < 27; ++i) {
        EEPROM.update(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 2 + i, *(dataMappings + i));
    }

    // Main Settings
    uint8_t n = 0;
    for(uint8_t i = 0; i < 8; ++i) {
        EEPROM.put(5 + (sizeof(ProfileData_t) * preferences.profileCount) + 1 + 27 + 1 + n, *(dataSettings + i));
        n += 2;
    }

    EEPROM.commit();
    return Error_Success;
}

void SamcoPreferences::ResetPreferences()
{
    for(uint16_t i = 0; i < EEPROM.length(); ++i) {
        EEPROM.write(i, 0);
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
