#include "Wire.h"
 /*!
 * @file OpenFIREcommon.h
 * @brief Shared objects and values used throughout the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#ifndef _OPENFIRECOMMON_H_
#define _OPENFIRECOMMON_H_

#include <stdint.h>
#include <DFRobotIRPositionEx.h>
#include <OpenFIRE_Square.h>
#include <OpenFIRE_Diamond.h>
#include <OpenFIRE_Perspective.h>
#include <OpenFIREConst.h>
#include <LightgunButtons.h>
#include <TinyUSB_Devices.h>

#include "SamcoPreferences.h"
#include "SamcoDisplay.h"
#include "OpenFIREDefines.h"

// number of profiles
#define PROFILE_COUNT 4

inline AbsMouse5_ AbsMouse5(2);

// operating modes
    enum GunMode_e {
        GunMode_Init = -1,
        GunMode_Run = 0,
        GunMode_Calibration,
        GunMode_Verification,
        GunMode_Pause,
        GunMode_Docked
    };

    // run modes
    // note that this is a 5 bit value when stored in the profiles
    enum RunMode_e {
        RunMode_Normal = 0,         ///< Normal gun mode, no averaging
        RunMode_Average = 1,        ///< 2 frame moving average
        RunMode_Average2 = 2,       ///< weighted average with 3 frames
        RunMode_ProfileMax = 2,     ///< maximum mode allowed for profiles
        RunMode_Processing = 3,     ///< Processing test mode
        RunMode_Count
    };

    enum CaliStage_e {
        Cali_Init = 0,
        Cali_Top,
        Cali_Bottom,
        Cali_Left,
        Cali_Right,
        Cali_Center,
        Cali_Verify
    };

    // numbered index of buttons, must match ButtonDesc[] order
    enum ButtonIndex_e {
        BtnIdx_Trigger = 0,
        BtnIdx_A,
        BtnIdx_B,
        BtnIdx_Start,
        BtnIdx_Select,
        BtnIdx_Up,
        BtnIdx_Down,
        BtnIdx_Left,
        BtnIdx_Right,
        BtnIdx_Reload,
        BtnIdx_Pedal,
        BtnIdx_Pedal2,
        BtnIdx_Pump,
        BtnIdx_Home
    };

    // bit mask for each button, must match ButtonDesc[] order to match the proper button events
    enum ButtonMask_e {
        BtnMask_Trigger = 1 << BtnIdx_Trigger,
        BtnMask_A = 1 << BtnIdx_A,
        BtnMask_B = 1 << BtnIdx_B,
        BtnMask_Start = 1 << BtnIdx_Start,
        BtnMask_Select = 1 << BtnIdx_Select,
        BtnMask_Up = 1 << BtnIdx_Up,
        BtnMask_Down = 1 << BtnIdx_Down,
        BtnMask_Left = 1 << BtnIdx_Left,
        BtnMask_Right = 1 << BtnIdx_Right,
        BtnMask_Reload = 1 << BtnIdx_Reload,
        BtnMask_Pedal = 1 << BtnIdx_Pedal,
        BtnMask_Pedal2 = 1 << BtnIdx_Pedal2,
        BtnMask_Pump = 1 << BtnIdx_Pump,
        BtnMask_Home = 1 << BtnIdx_Home
    };

    //// Button Masks
    // button combo to send an escape keypress
    static inline uint32_t EscapeKeyBtnMask = BtnMask_Reload | BtnMask_Start;

    // button combo to enter pause mode
    static inline uint32_t EnterPauseModeBtnMask = BtnMask_Reload | BtnMask_Select;

    // button combo to enter pause mode (holding ver)
    static inline uint32_t EnterPauseModeHoldBtnMask = BtnMask_Trigger | BtnMask_A;

    // press any button to exit hotkey pause mode back to run mode (this is not a button combo)
    static inline uint32_t ExitPauseModeBtnMask = BtnMask_Reload | BtnMask_Home;

    // press and hold any button to exit simple pause menu (this is not a button combo)
    static inline uint32_t ExitPauseModeHoldBtnMask = BtnMask_A | BtnMask_B;

    // button combo to skip the center calibration step
    static inline uint32_t SkipCalCenterBtnMask = BtnMask_A;

    // button combo to save preferences to non-volatile memory
    static inline uint32_t SaveBtnMask = BtnMask_Start | BtnMask_Select;

    // button combo to increase IR sensitivity
    static inline uint32_t IRSensitivityUpBtnMask = BtnMask_B | BtnMask_Up;

    // button combo to decrease IR sensitivity
    static inline uint32_t IRSensitivityDownBtnMask = BtnMask_B | BtnMask_Down;

    // button combinations to select a run mode
    static inline uint32_t RunModeNormalBtnMask = BtnMask_Start | BtnMask_A;
    static inline uint32_t RunModeAverageBtnMask = BtnMask_Start | BtnMask_B;

    // button combination to toggle offscreen button mode in software:
    static inline uint32_t OffscreenButtonToggleBtnMask = BtnMask_Reload | BtnMask_A;

    // button combination to toggle offscreen button mode in software:
    static inline uint32_t AutofireSpeedToggleBtnMask = BtnMask_Reload | BtnMask_B;

    // button combination to toggle rumble in software:
    static inline uint32_t RumbleToggleBtnMask = BtnMask_Left;

    // button combination to toggle solenoid in software:
    static inline uint32_t SolenoidToggleBtnMask = BtnMask_Right;

    static inline const char* RunModeLabels[RunMode_Count] = {
        "Normal",
        "Averaging",
        "Averaging2",
        "Processing"
    };

    enum StateFlag_e {
        // print selected profile once per pause state when the COM port is open
        StateFlag_PrintSelectedProfile = (1 << 0),
        
        // report preferences once per pause state when the COM port is open
        StateFlag_PrintPreferences = (1 << 1),
        
        // enable save (allow save once per pause state)
        StateFlag_SavePreferencesEn = (1 << 2),
        
        // print preferences storage
        StateFlag_PrintPreferencesStorage = (1 << 3)
    };

    // when serial connection resets, these flags are set
    static inline constexpr uint32_t StateFlagsDtrReset = StateFlag_PrintSelectedProfile | StateFlag_PrintPreferences | StateFlag_PrintPreferencesStorage;

class FW_Common
{
public:
    //// Methods
    static void FeedbackSet();

    static void PinsReset();

    static void CameraSet();

    // Macro for functions to run when gun enters new gunmode
    static void SetMode(const GunMode_e&);

    // set new run mode and apply it to the selected profile
    static void SetRunMode(const RunMode_e&);

    /// @brief    Gun Mode that handles calibration.
    /// @details  Bool represents whether this session is invoked from the PC App.
    ///           When true, mouse movement is skipped entirely.
    static void ExecCalMode(const bool& = false);

    /// @brief    Updates current sight position from IR cam
    /// @details  Updates finalX and finalY values.
    static void GetPosition();

    /// @brief    Update the last seen value
    /// @details  Only to be called during run mode since this will modify the LED colour
    static void UpdateLastSeen();

    #ifdef LED_ENABLE
    /// @brief    Macro that sets LEDs color depending on the mode it's set to
    static void SetLedColorFromMode();
    #endif // LED_ENABLE

    // applies loaded gun profile settings
    static bool SelectCalProfile(const uint8_t&);

    // set a new IR camera sensitivity and apply to the selected profile
    static void SetIrSensitivity(const uint8_t&);

    // Loads preferences from EEPROM, then verifies.
    static void LoadPreferences();

    // Saves profile settings to EEPROM
    // Blinks LEDs (if any) on success or failure.
    static void SavePreferences();

    // Updates the button array with new pin mappings and control bindings, if any.
    static void UpdateBindings(const bool & = false);

    // initial gunmode
    static inline GunMode_e gunMode = GunMode_Init;

    // run mode
    static inline RunMode_e runMode = RunMode_Normal;

    // state flags, see StateFlag_e
    static inline uint32_t stateFlags = StateFlagsDtrReset;

    // profiles ----------------------------------------------------------------------------------------------
    // defaults can be populated here, but any values in EEPROM/Flash will override these.
    // top/bottom/left/right offsets, TLled/TRled, adjX/adjY, sensitivity, runmode, button mask mapped to profile, layout toggle, color, name
    static inline SamcoPreferences::ProfileData_t profileData[PROFILE_COUNT] = {
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_A,      false, 0xFF0000, "Profile A"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_B,      false, 0x00FF00, "Profile B"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_Start,  false, 0x0000FF, "Profile Start"},
        {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_Select, false, 0xFF00FF, "Profile Select"}
    };

    // single instance of the preference data
    static inline SamcoPreferences::Preferences_t profiles = {
        profileData, PROFILE_COUNT, // profiles
        0, // default profile
    };

    //// Camera
    // IR positioning camera
    static inline DFRobotIRPositionEx *dfrIRPos = nullptr;

    // IR camera sensitivity
    static inline DFRobotIRPositionEx::Sensitivity_e irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;

    // OpenFIRE Positioning - one for Square, one for Diamond, and a shared perspective object
    static inline OpenFIRE_Square OpenFIREsquare;
    static inline OpenFIRE_Diamond OpenFIREdiamond;
    static inline OpenFIRE_Perspective OpenFIREper;

    // IR coords stuff:
    static inline int mouseX;
    static inline int mouseY;
    static inline int moveXAxisArr[3] = {0, 0, 0};
    static inline int moveYAxisArr[3] = {0, 0, 0};
    static inline int moveIndex = 0;

    // timer will set this to 1 when the IR position can update
    static inline volatile unsigned int irPosUpdateTick = 0;

    // Amount of IR points seen from last camera poll
    static inline unsigned int lastSeen = 0;

    // Cam Test Mode last timestamp of last print
    static inline unsigned long testLastStamp = 0;

    //// Non-volatile storage
    #ifdef SAMCO_EEPROM_ENABLE
    // EEPROM non-volatile storage
    static inline const char* NVRAMlabel = "EEPROM";

    // flag to indicate if non-volatile storage is available
    // unconditional for EEPROM
    static inline bool nvAvailable = true;
    #endif

    // non-volatile preferences error code
    static inline int nvPrefsError = SamcoPreferences::Error_NoStorage;
    
    //// General Runtime Flags
    static inline bool justBooted = true;                              // For ops we need to do on initial boot (custom pins, joystick centering)
    static inline bool dockedSaving = false;                           // To block sending test output in docked mode.

    // For offscreen button stuff:
    static inline bool offscreenButton = false;                    // Does shooting offscreen also send a button input (for buggy games that don't recognize off-screen shots)? Default to off.
    static inline bool offscreenBShot = false;                     // For offscreenButton functionality, to track if we shot off the screen.
    static inline bool buttonPressed = false;                      // Sanity check.

    #ifdef USES_ANALOG
        static inline bool analogIsValid;                          // Flag set true if analog stick is mapped to valid nums
    #endif // USES_ANALOG

    #ifdef FOURPIN_LED
        static inline bool ledIsValid;                             // Flag set true if RGB pins are mapped to valid numbers
    #endif // FOURPIN_LED

    //// OLED Display interface
    #ifdef USES_DISPLAY
    static inline ExtDisplay OLED;
    // Selector for which option in the simple pause menu you're scrolled on.
    static inline uint8_t pauseModeSelection = 0;
    #ifdef MAMEHOOKER
    static inline uint16_t dispMaxLife = 0; 			                 // Max value for life in lifebar mode (100%)
    static inline uint16_t dispLifePercentage = 0; 		             // Actual value to show in lifebar mode #%
    #endif // MAMEHOOKER
    #endif // USES_DISPLAY
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
    {SamcoPreferences::pins[OF_Const::btnTrigger],  LightgunButtons::ReportType_Internal, MOUSE_LEFT,                 LightgunButtons::ReportType_Internal, MOUSE_LEFT,                 LightgunButtons::ReportType_Internal, PAD_RT,     15, BTN_AG_MASK}, // Barry says: "I'll handle this."
    {SamcoPreferences::pins[OF_Const::btnGunA],     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,                LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,                LightgunButtons::ReportType_Gamepad,  PAD_LT,     15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunB],     LightgunButtons::ReportType_Mouse,    MOUSE_MIDDLE,               LightgunButtons::ReportType_Mouse,    MOUSE_MIDDLE,               LightgunButtons::ReportType_Gamepad,  PAD_Y,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnStart],    LightgunButtons::ReportType_Keyboard, playerStartBtn,             LightgunButtons::ReportType_Keyboard, playerStartBtn,             LightgunButtons::ReportType_Gamepad,  PAD_START,  20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnSelect],   LightgunButtons::ReportType_Keyboard, playerSelectBtn,            LightgunButtons::ReportType_Keyboard, playerSelectBtn,            LightgunButtons::ReportType_Gamepad,  PAD_SELECT, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunUp],    LightgunButtons::ReportType_Gamepad,  PAD_UP,                     LightgunButtons::ReportType_Gamepad,  PAD_UP,                     LightgunButtons::ReportType_Gamepad,  PAD_UP,     20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunDown],  LightgunButtons::ReportType_Gamepad,  PAD_DOWN,                   LightgunButtons::ReportType_Gamepad,  PAD_DOWN,                   LightgunButtons::ReportType_Gamepad,  PAD_DOWN,   20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunLeft],  LightgunButtons::ReportType_Gamepad,  PAD_LEFT,                   LightgunButtons::ReportType_Gamepad,  PAD_LEFT,                   LightgunButtons::ReportType_Gamepad,  PAD_LEFT,   20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunRight], LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,                  LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,                  LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,  20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunC],     LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Gamepad,  PAD_A,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnPedal],    LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Gamepad,  PAD_X,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnPedal2],   LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON5,              LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON5,              LightgunButtons::ReportType_Gamepad,  PAD_B,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnPump],     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,                LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,                LightgunButtons::ReportType_Gamepad,  PAD_LT,     15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnHome],     LightgunButtons::ReportType_Internal, 0,                          LightgunButtons::ReportType_Internal, 0,                          LightgunButtons::ReportType_Internal, 0,          15, BTN_AG_MASK2}
};

    // button count constant
    static inline constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

    // button runtime data arrays
    static inline LightgunButtonsStatic<ButtonCount> lgbData;

    // button object instance
    static inline LightgunButtons buttons(lgbData, ButtonCount);

#endif // _OPENFIRECOMMON_H_