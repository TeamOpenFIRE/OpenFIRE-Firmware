/*!
 * @file SamcoPreferences.h
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

#ifndef _SAMCOPREFERENCES_H_
#define _SAMCOPREFERENCES_H_

#include <stdint.h>
#include <OpenFIREBoard.h>
#include "boards/OpenFIREshared.h"
#include "OpenFIREDefines.h"

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
        int topOffset;              // Perspective: Offsets
        int bottomOffset;
        int leftOffset;
        int rightOffset;
        float TLled;                // Perspective: LED relative anchors
        float TRled;
        float adjX;                 // Perspective: adjusted axis
        float adjY;
        uint32_t irSensitivity : 3; // IR Sensitivity from 0-2
        uint32_t runMode : 5;       // Averaging mode
        uint32_t buttonMask : 16;   // Button mask assigned to this profile
        bool irLayout;              // square or diamond IR for this display?
        uint32_t color   : 24;      // packed color blob per profile
        char name[16];               // Profile display name
    } __attribute__ ((packed)) ProfileData_t;

    /// @brief Preferences that can be stored in flash
    typedef struct Preferences_s {
        // pointer to ProfileData_t array
        SamcoPreferences::ProfileData_t* pProfileData;
        
        // number of ProfileData_t entries
        uint8_t profileCount;

        // default profile
        uint8_t selectedProfile;
    } __attribute__ ((packed)) Preferences_t;

    static Preferences_t profiles;

    static inline bool toggles[OF_Const::boolTypesCount] = {
        false,
        true,
        true,
        false,
        false,
        false,
        true,
        false,
        false
    };

    static inline int8_t pins[OF_Const::boardInputsCount] = { -1 };

    static inline uint32_t settings[OF_Const::settingsTypesCount] = {
        255,
        150,
        45,
        30,
        500,
        3,
        2500,
        1,
        0,
        0xFF0000,
        0x00FF00,
        0x0000FF
    };

    typedef struct USBMap_s {
        char deviceName[16];
        uint16_t devicePID;
    } USBMap_t;

    static inline USBMap_t usb = {
        { 'F', 'I', 'R', 'E', 'C', 'o', 'n', ' ', 'P', PLAYER_NUMBER+'0' },
        PLAYER_NUMBER
    };

    // header ID to ensure junk isn't loaded if preferences aren't saved
    static const HeaderId_t HeaderId;

    /// @brief Required size for the preferences
    static unsigned int Size() { return sizeof(ProfileData_t) * profiles.profileCount + sizeof(HeaderId_u) + sizeof(profiles.selectedProfile); }

    /// @brief Save/Update header
    /// @return Nothing
    static void WriteHeader();

    /// @brief Load and compare the header
    /// @return An error code from Errors_e
    static int CheckHeader();

    /// @brief Load preferences
    /// @return An error code from Errors_e
    static int LoadProfiles();

    /// @brief Save current preferences
    /// @return An error code from Errors_e
    static int SaveProfiles();

    /// @brief Load toggles
    /// @return An error code from Errors_e
    static int LoadToggles();

    /// @brief Save current toggles states
    /// @return An error code from Errors_e
    static int SaveToggles();

    /// @brief Load pin mapping
    /// @return An error code from Errors_e
    static int LoadPins();

    /// @brief Save current pin mapping
    /// @return An error code from Errors_e
    static int SavePins();

    /// @brief Load settings
    /// @return An error code from Errors_e
    static int LoadSettings();

    /// @brief Save current settings
    /// @return An error code from Errors_e
    static int SaveSettings();

    /// @brief Load USB identifier info
    /// @return An error code from Errors_e
    static int LoadUSBID();

    /// @brief Save current USB ID
    /// @return An error code from Errors_e
    static int SaveUSBID();

    /// @brief Resets preferences with a zero-fill to the EEPROM.
    /// @return Nothing
    static void ResetPreferences();

    /// @brief Sets pre-set values according to the board
    /// @return Nothing
    static void LoadPresets();
};

#endif // _SAMCOPREFERENCES_H_
