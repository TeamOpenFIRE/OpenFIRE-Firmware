/*!
 * @file OpenFIREprefs.h
 * @brief OpenFIRE file system loading/saving and presets access.
 *
 * @copyright Mike Lynch & That One Seong, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @date 2025
 */

#ifndef _OPENFIREPREFS_H_
#define _OPENFIREPREFS_H_

// number of profiles
#define PROFILE_COUNT 4

#include <stdint.h>
#include <unordered_map>
#include <FS.h>
#include <LittleFS.h>
#include <OpenFIREBoard.h>
#include <DFRobotIRPositionEx.h>
#include <LightgunButtons.h>
#include <TinyUSB_Devices.h>

#include "boards/OpenFIREshared.h"
#include "OpenFIREDefines.h"
#include "OpenFIREconstant.h"

/// @brief Static instance of preferences to save in non-volatile memory
class OF_Prefs
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

    /// @brief Profile data
    /// @details
    typedef struct ProfileData_s {
        int topOffset;              // Perspective: Offsets
        int bottomOffset;
        int leftOffset;
        int rightOffset;
        float TLled;                // Perspective: LED relative anchors
        float TRled;
        float adjX;                 // Perspective: adjusted axis
        float adjY;
        uint irSens;                // IR Sensitivity from 0-2 (padded upto 32-bit for consistency)
        uint runMode;               // Averaging mode (padded upto 32-bit for consistent spacing)
        uint irLayout;              // square or diamond IR for this display? (padded upto 32-bit for consistent spacing)
        uint color;                 // packed color blob per profile (uses least sig 24-bits out of 32 bits)
        char name[16];              // Profile display name
    } ProfileData_t;

    /// @brief Instance of profile data 
    static inline ProfileData_t profiles[PROFILE_COUNT] = {
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0xFF0000, "Profile A"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0x00FF00, "Profile B"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0x0000FF, "Profile Start"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, 1, false, 0xFF00FF, "Profile Select"}
    };

    static inline uint currentProfile = 0;

    /// @brief System toggles array
    static inline bool toggles[OF_Const::boolTypesCount] = {
        false,          // custom pins
        true,           // rumble
        true,           // solenoid
        false,          // autofire
        false,          // simple pause menu
        false,          // hold to pause
        true,           // 4pin common anode
        false,          // low buttons mode
        false,          // rumble force-feedback mode
        false,          // invert static pixels
        false,          // i2c OLED enabled
        false,          // i2c OLED alt address
    };

    /// @brief Pin functions array
    static inline int8_t pins[OF_Const::boardInputsCount] = { -1 };

    /// @brief System variables array
    static inline uint32_t settings[OF_Const::settingsTypesCount] = {
        255,            // rumble strength
        150,            // rumble length
        45,             // solenoid on length
        80,             // solenoid off length
        500,            // solenoid hold length
        2500,           // hold-to-pause length
        1,              // custom NeoPixel strand length
        0,              // custom NeoPixel static count
        0xFF0000,       // custom pixel color 1
        0x00FF00,       // custom pixel color 2
        0x0000FF,       // custom pixel color 3
        38,             // temp warning
        45,             // temp shutoff
    };

    typedef struct USBMap_s {
        uint16_t devicePID;
        char deviceName[16];
    } USBMap_t;

    /// @brief Instance of TinyUSB identifier data
    static inline USBMap_t usb = {
        PLAYER_NUMBER,
        { 'F', 'I', 'R', 'E', 'C', 'o', 'n', ' ', 'P', PLAYER_NUMBER+'0' }
    };

    // Backup instance of the buttons descriptor
    static uint8_t backupButtonDesc[][6];

    /// @brief Instance of OpenFIREshared's presets and I/O table data
    static inline OF_Const OFPresets;

    /// @brief Initialize filesystem
    /// @return An error code from Errors_e
    static int InitFS();

    /// @brief Macro for loading all non-cali profile settings
    static void Load();

    /// @brief Generic saving method using provided pointers to a block of data and its associated string map for lookups
    /// @return An error code from Errors_e
    static int SaveToPtr(File, void*, const std::unordered_map<std::string, int>&, const size_t&);

    /// @brief Generic loading method using provided pointers to a block of data and its associated string map for lookups
    /// @return An error code from Errors_e
    static int LoadToPtr(File, void*, const std::unordered_map<std::string, int>&);

    /// @brief Load preferences
    /// @return An error code from Errors_e
    static int LoadProfiles();

    /// @brief Save current preferences
    /// @return An error code from Errors_e
    static int SaveProfiles();

    /// @brief Load toggles (macro for LoadToPtr)
    /// @return An error code from Errors_e
    static int LoadToggles() { return LoadToPtr(LittleFS.open("/toggles.conf", "r"), &toggles, OFPresets.boolTypes_Strings); }

    /// @brief Save current toggles states (macro for SaveToPtr)
    /// @return An error code from Errors_e
    static int SaveToggles() { return SaveToPtr(LittleFS.open("/toggles.conf", "w"), &toggles, OFPresets.boolTypes_Strings, sizeof(toggles) / OF_Const::boolTypesCount); }

    /// @brief Load pin mapping (macro for LoadToPtr)
    /// @return An error code from Errors_e
    static int LoadPins() { return LoadToPtr(LittleFS.open("/pins.conf", "r"), &pins, OFPresets.boardInputs_Strings); }

    /// @brief Save current pin mapping (macro for SaveToPtr)
    /// @return An error code from Errors_e
    static int SavePins() { return SaveToPtr(LittleFS.open("/pins.conf", "w"), &pins, OFPresets.boardInputs_Strings, sizeof(pins) / OF_Const::boardInputsCount); }

    /// @brief Load settings (macro for LoadToPtr)
    /// @return An error code from Errors_e
    static int LoadSettings() { return LoadToPtr(LittleFS.open("/settings.conf", "r"), &settings, OFPresets.settingsTypes_Strings); }

    /// @brief Save current settings (macro for SaveToPtr)
    /// @return An error code from Errors_e
    static int SaveSettings() { return SaveToPtr(LittleFS.open("/settings.conf", "w"), &settings, OFPresets.settingsTypes_Strings, sizeof(settings) / OF_Const::settingsTypesCount); }

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

// Sanity checks and assignments for player number -> common keyboard assignments
    #if PLAYER_NUMBER == 1
        static inline char playerStartBtn = '1';
        static inline char playerSelectBtn = '5';
    #elif PLAYER_NUMBER == 2
        static inline char playerStartBtn = '2';
        static inline char playerSelectBtn = '6';
    #elif PLAYER_NUMBER == 3
        static inline char playerStartBtn = '3';
        static inline char playerSelectBtn = '7';
    #elif PLAYER_NUMBER == 4
        static inline char playerStartBtn = '4';
        static inline char playerSelectBtn = '8';
    #else
        #error Undefined or out-of-range player number! Please set PLAYER_NUMBER to 1, 2, 3, or 4.
    #endif // PLAYER_NUMBER

// Button descriptor
// The order of the buttons is the order of the button bitmask
// must match ButtonIndex_e order, and the named bitmask values for each button
// see LightgunButtons::Desc_t, format is: 
// {pin, report type, report code (ignored for internal), offscreen report type, offscreen report code, gamepad output report type, gamepad output report code, debounce time, debounce mask, label}
inline LightgunButtons::Desc_t LightgunButtons::ButtonDesc[] = {
    {OF_Prefs::pins[OF_Const::btnTrigger],  LightgunButtons::ReportType_Mouse,    MOUSE_LEFT,      LightgunButtons::ReportType_Mouse,    MOUSE_LEFT,      LightgunButtons::ReportType_Gamepad,  PAD_RT,     15, BTN_AG_MASK}, // Barry says: "I'll handle this."
    {OF_Prefs::pins[OF_Const::btnGunA],     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,     LightgunButtons::ReportType_Gamepad,  PAD_LT,     15, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnGunB],     LightgunButtons::ReportType_Mouse,    MOUSE_MIDDLE,    LightgunButtons::ReportType_Mouse,    MOUSE_MIDDLE,    LightgunButtons::ReportType_Gamepad,  PAD_Y,      15, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnGunC],     LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,   LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,   LightgunButtons::ReportType_Gamepad,  PAD_A,      15, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnStart],    LightgunButtons::ReportType_Keyboard, playerStartBtn,  LightgunButtons::ReportType_Keyboard, playerStartBtn,  LightgunButtons::ReportType_Gamepad,  PAD_START,  20, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnSelect],   LightgunButtons::ReportType_Keyboard, playerSelectBtn, LightgunButtons::ReportType_Keyboard, playerSelectBtn, LightgunButtons::ReportType_Gamepad,  PAD_SELECT, 20, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnGunUp],    LightgunButtons::ReportType_Gamepad,  PAD_UP,          LightgunButtons::ReportType_Gamepad,  PAD_UP,          LightgunButtons::ReportType_Gamepad,  PAD_UP,     20, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnGunDown],  LightgunButtons::ReportType_Gamepad,  PAD_DOWN,        LightgunButtons::ReportType_Gamepad,  PAD_DOWN,        LightgunButtons::ReportType_Gamepad,  PAD_DOWN,   20, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnGunLeft],  LightgunButtons::ReportType_Gamepad,  PAD_LEFT,        LightgunButtons::ReportType_Gamepad,  PAD_LEFT,        LightgunButtons::ReportType_Gamepad,  PAD_LEFT,   20, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnGunRight], LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,       LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,       LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,  20, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnPedal],    LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,   LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,   LightgunButtons::ReportType_Gamepad,  PAD_X,      15, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnPedal2],   LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON5,   LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON5,   LightgunButtons::ReportType_Gamepad,  PAD_B,      15, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnPump],     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,     LightgunButtons::ReportType_Gamepad,  PAD_LT,     15, BTN_AG_MASK2},
    {OF_Prefs::pins[OF_Const::btnHome],     LightgunButtons::ReportType_Internal, 0,               LightgunButtons::ReportType_Internal, 0,               LightgunButtons::ReportType_Internal, 0,          15, BTN_AG_MASK2}
};

// button count constant
static inline constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

inline uint8_t OF_Prefs::backupButtonDesc[ButtonCount][6];

// button runtime data arrays
static inline LightgunButtonsStatic<ButtonCount> lgbData;

#endif // _OPENFIREPREFS_H_
