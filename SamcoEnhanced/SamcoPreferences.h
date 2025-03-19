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

// number of profiles
#define PROFILE_COUNT 4

#include <stdint.h>
#include <OpenFIREBoard.h>
#include "boards/OpenFIREshared.h"
#include "OpenFIREDefines.h"
#include <DFRobotIRPositionEx.h>

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

    enum ProfileDataTypes_e {
        Profile_ProfileNum = 0,
        Profile_TopOffset,
        Profile_BottomOffset,
        Profile_LeftOffset,
        Profile_RightOffset,
        Profile_TLled,
        Profile_TRled,
        Profile_AdjX,
        Profile_AdjY,
        Profile_IrSens,
        Profile_RunMode,
        Profile_IrLayout,
        Profile_Color,
        Profile_Name,
        Profile_Selected = 254
    };

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
        uint32_t irSens;            // IR Sensitivity from 0-2 (padded upto 32-bit for consistency)
        uint32_t runMode;           // Averaging mode (padded upto 32-bit for consistent spacing)
        uint32_t irLayout;          // square or diamond IR for this display? (padded upto 32-bit for consistent spacing)
        uint32_t color;             // packed color blob per profile (uses least sig 24-bits out of 32 bits)
        char name[16];              // Profile display name
    } ProfileData_t;

    static inline ProfileData_t profiles[PROFILE_COUNT] = {
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0xFF0000, "Profile A"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0x00FF00, "Profile B"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0x0000FF, "Profile Start"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0xFF00FF, "Profile Select"}
    };

    static inline uint8_t currentProfile = 0;

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

    /// @brief Initialize filesystem
    /// @return An error code from Errors_e
    static int InitFS();

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

    /// @brief Formats preferences filesystem, clearing any saved user data
    static void ResetPreferences();

    /// @brief Sets pre-set values according to the board
    static void LoadPresets();
};

#endif // _SAMCOPREFERENCES_H_
