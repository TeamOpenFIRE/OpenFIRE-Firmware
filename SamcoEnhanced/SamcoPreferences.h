/*!
 * @file SamcoPreferences.h
 * @brief Samco Prow Enhanced light gun preferences to save in non-volatile memory.
 *
 * @copyright Mike Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @version V1.0
 * @date 2021
 */

#ifndef _SAMCOPREFERENCES_H_
#define _SAMCOPREFERENCES_H_

#include <SamcoBoard.h>
#include <stdint.h>

#ifdef SAMCO_FLASH_ENABLE
class Adafruit_SPIFlashBase;
#endif // SAMCO_FLASH_ENABLE

/// @brief Static instance of preferences to save in non-volatile memory
class SamcoPreferences
{
public:
    /// @brief Error codes
    enum Errors_e {
        Error_Success = 0,
        Error_NoStorage = -1,
        Error_Read = -2,
        Error_NoData = -3,
        Error_Write = -4,
        Error_Erase = -5
    };
    
    /// @brief Header ID
    typedef union HeaderId_u {
        uint8_t bytes[4];
        uint32_t u32;
    } __attribute__ ((packed)) HeaderId_t;

    /// @brief Profile data
    typedef struct ProfileData_s {
        uint16_t xScale;	        ///< X Scale * 1000
        uint16_t yScale;            ///< Y Scale * 1000
        uint32_t xCenter : 12;
        uint32_t yCenter : 12;
        uint32_t irSensitivity : 3;
        uint32_t runMode : 5;
        uint32_t reserved;
        uint32_t reserved2;
    } __attribute__ ((packed)) ProfileData_t;

    /// @brief Preferences that can be stored in flash
    typedef struct Preferences_s {
        // pointer to ProfileData_t array
        SamcoPreferences::ProfileData_t* pProfileData;
        
        // number of ProfileData_t entries
        uint8_t profileCount;

        // default profile
        uint8_t profile;
    } __attribute__ ((packed)) Preferences_t;

    // single instance of the preference data
    static Preferences_t preferences;

    // header ID to ensure junk isn't loaded if preferences aren't saved
    static const HeaderId_t HeaderId;

    /// @brief Required size for the preferences
    static unsigned int Size() { return sizeof(ProfileData_t) * preferences.profileCount + sizeof(HeaderId_u) + sizeof(preferences.profile); }

#ifdef SAMCO_FLASH_ENABLE

    /// @brief Load preferences
    /// @return An error code from Errors_e
    static int Load(Adafruit_SPIFlashBase& flash);

    /// @brief Save preferences
    /// @return An error code from Errors_e
    static int Save(Adafruit_SPIFlashBase& flash);

    /// @brief Get a string for a given error code
    static const char* ErrorCodeToString(int error);

private:
    // error text table
    static const char* ErrorText[6];

#else
    /// @brief Load preferences
    /// @return An error code from Errors_e
    static int Load();

    /// @brief Save preferences
    /// @return An error code from Errors_e
    static int Save();

    /// @brief Load extended preferences
    /// @return Nothing
    static void LoadExtended(uint8_t *dataBools, int8_t *dataMappings, uint16_t *dataSettings);

    /// @brief Save extended preferences
    /// @return An error code from Errors_e
    static int SaveExtended(uint8_t *dataBools, int8_t *dataMappings, uint16_t *dataSettings);

    /// @brief Resets preferences with a zero-fill to the EEPROM.
    /// @return Nothing
    static void ResetPreferences();

    #ifdef USE_TINYUSB
    /// @brief Load TinyUSB identifiers
    /// @return An error code from Errors_e
    static int LoadTinyID(char *tinyName, unsigned int *tinyID);

    /// @brief Save TinyID identifiers
    /// @return An error code from Errors_e
    static int SaveTinyID(char deviceName[16], unsigned int *tinyID);
    #endif // USE_TINYUSB

#endif
};

#endif // _SAMCOPREFERENCES_H_
