/*!
 * @file SamcoPreferences.cpp
 * @brief Samco Prow Enhanced light gun preferences to save in non-volatile memory.
 *
 * @copyright Mike Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @version V1.0
 * @date 2021
 */

#include "SamcoPreferences.h"

#ifdef SAMCO_FLASH_ENABLE
#include <Adafruit_SPIFlashBase.h>
#endif // SAMCO_FLASH_ENABLE
#ifdef SAMCO_EEPROM_ENABLE
#include <EEPROM.h>
#endif // SAMCO_EEPROM_ENABLE

// 4 byte header ID
const SamcoPreferences::HeaderId_t SamcoPreferences::HeaderId = {'P', 'r', 'o', 'w'};

#if defined(SAMCO_FLASH_ENABLE)

// must match Errors_e order
const char* SamcoPreferences::ErrorText[6] = {
    "Success",
    "No storage memory",
    "Read error",
    "No preferences saved",
    "Write error",
    "Erase failed"
};

const char* SamcoPreferences::ErrorCodeToString(int error)
{
    if(error >= 0) {
        // all positive values are success
        error = 0;
    } else {
        error = -error;
    }
    if(error < sizeof(ErrorText) / sizeof(ErrorText[0])) {
        return ErrorText[error];
    }
    return "";
}

int SamcoPreferences::Load(Adafruit_SPIFlashBase& flash)
{
    uint32_t u32;
    uint32_t fr = flash.readBuffer(0, (uint8_t*)&u32, sizeof(u32));
    if(fr != 4) {
        return Error_Read;
    }

    if(u32 != HeaderId.u32) {
        return Error_NoData;
    }

    fr = flash.readBuffer(4, &preferences.profile, 1);
    if(fr != 1) {
        return Error_Read;
    }
    
    const uint32_t length = sizeof(ProfileData_t) * preferences.profileCount;
    fr = flash.readBuffer(5, (uint8_t*)preferences.pProfileData, length);

    return fr == length ? Error_Success : Error_Read;
}

int SamcoPreferences::Save(Adafruit_SPIFlashBase& flash)
{
    bool success = flash.eraseSector(0);
    if(!success) {
        return Error_Erase;
    }

    uint32_t fw = flash.writeBuffer(0, (uint8_t*)&HeaderId.u32, sizeof(HeaderId.u32));
    if(fw != sizeof(HeaderId.u32)) {
        return Error_Write;
    }

    fw = flash.writeBuffer(4, &preferences.profile, 1);
    if(fw != 1) {
        return Error_Write;
    }

    const uint32_t length = sizeof(ProfileData_t) * preferences.profileCount;
    fw = flash.writeBuffer(5, (uint8_t*)preferences.pProfileData, length);

    return fw == length ? Error_Success : Error_Write;
}

#elif defined(SAMCO_EEPROM_ENABLE)

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
    EEPROM.commit();
    return Error_Success;
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

#endif
