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

#include <OpenFIREBoard.h>
#include <stdint.h>

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

    // single instance of the preference data
    static Preferences_t profiles;

    typedef struct TogglesMap_s {
        bool customPinsInUse = false;   // Are we using custom pins mapping?
        bool rumbleActive = true;       // Are we allowed to do rumble?
        bool solenoidActive = true;     // Are we allowed to use a solenoid?
        bool autofireActive = false;    // Is autofire enabled?
        bool simpleMenu = false;        // Is simple pause menu active?
        bool holdToPause = false;       // Is holding A/B buttons to enter pause mode allowed?
        bool commonAnode = true;        // If LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
        bool lowButtonMode = false;     // Is low buttons mode active?
        bool rumbleFF = false;          // Rumble force-feedback, instead of Solenoid
    } __attribute__ ((packed)) TogglesMap_t;

    static TogglesMap_t toggles;

    typedef struct PinsMap_s {
        int8_t bTrigger = -1;              // Trigger
        int8_t bGunA = -1;                 // Button A (GunCon 1/Stunner/Justifier)
        int8_t bGunB = -1;                 // Button B (GunCon 1)
        int8_t bStart = -1;                // Start Button (GCon-2)
        int8_t bSelect = -1;               // Select Button (GCon-2)
        int8_t bGunUp = -1;                // D-Pad Up (GCon-2)
        int8_t bGunDown = -1;              // D-Pad Down (GCon-2)
        int8_t bGunLeft = -1;              // D-Pad Left (GCon-2)
        int8_t bGunRight = -1;             // D-Pad Right (GCon-2)
        int8_t bGunC = -1;                 // Button C (GCon-2)
        int8_t bPedal = -1;                // External Pedal (DIY)
        int8_t bHome = -1;                 // Home Button (Top Shot Elite)
        int8_t bPump = -1;                 // Pump Action Reload Button (Top Shot Elite)
        int8_t sRumble = -1;               // Rumble Switch
        int8_t sSolenoid = -1;             // Solenoid Switch
        int8_t sAutofire = -1;             // Autofire Switch
        int8_t oRumble = -1;               // Rumble Signal Pin
        int8_t oSolenoid = -1;             // Solenoid Signal Pin
        int8_t oPixel = -1;                // Custom NeoPixel Pin
        int8_t oLedR = -1;                 // 4-Pin RGB Red Pin
        int8_t oLedB = -1;                 // 4-Pin RGB Blue Pin
        int8_t oLedG = -1;                 // 4-Pin RGB Green Pin
        int8_t pCamSDA = -1;               // Camera I2C Data Pin
        int8_t pCamSCL = -1;               // Camera I2C Clock Pin
        int8_t pPeriphSDA = -1;            // Other I2C Peripherals Data Pin
        int8_t pPeriphSCL = -1;            // Other I2C Peripherals Clock Pin
        int8_t aStickX = -1;               // Analog Stick X-axis
        int8_t aStickY = -1;               // Analog Stick Y-axis
        int8_t aTMP36 = -1;                // Analog TMP36 Temperature Sensor Pin
    } PinsMap_t;

    static PinsMap_t pins;

    typedef struct SettingsMap_s {
        uint8_t rumbleIntensity = 255;
        uint16_t rumbleInterval = 150;
        uint16_t solenoidNormalInterval = 45;
        uint16_t solenoidFastInterval = 30;
        uint16_t solenoidLongInterval = 500;
        uint8_t autofireWaitFactor = 3;
        uint16_t pauseHoldLength = 2500;
        uint8_t customLEDcount = 1;
        uint8_t customLEDstatic = 0;
        uint32_t customLEDcolor1 = 0xFF0000;
        uint32_t customLEDcolor2 = 0x00FF00;
        uint32_t customLEDcolor3 = 0x0000FF;
    } SettingsMap_t;

    static SettingsMap_t settings;

    typedef struct USBMap_s {
        char deviceName[16] = "FIRECon";
        uint16_t devicePID;
    } USBMap_t;

    static USBMap_t usb;

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

    /// @brief Sets pre-set camera pins according to the board
    /// @return Nothing
    static void PresetCam();
};

#endif // _SAMCOPREFERENCES_H_
