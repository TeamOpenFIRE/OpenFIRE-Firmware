 /*!
 * @file OpenFIREcommon.h
 * @brief Shared runtime objects and methods used throughout the OpenFIRE firmware.
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
#include "OpenFIREconstant.h"

inline AbsMouse5_ AbsMouse5(2);

class FW_Common
{
public:
    //// Methods
    /// @brief    Sets feedback devices to appropriate I/O modes and initializes external devices.
    /// @details  Mainly Force Feedbacks, analog RGB, and digital devices (I2C peripherals or
    //            external NeoPixels)
    static void FeedbackSet();

    /// @brief    Unsets any currently mapped pins back to board defaults (non-pullup inputs).
    /// @note     This should be run before any sync operation, to ensure no problems
    ///           when setting pins to any new values.
    static void PinsReset();

    /// @brief    (Re-)sets IR camera position object.
    /// @note     This is run at startup, and can also be run from Docked mode
    ///           when new camera pins are defined.
    static void CameraSet();

    /// @brief    Macro for functions to run when gun enters new gunmode
    /// @param    GunMode_e
    ///           GunMode to switch to
    /// @note     Some cleanup functions are performed depending on the mode
    ///           being switched away from.
    static void SetMode(const FW_Const::GunMode_e&);

    /// @brief    Set new IR mode and apply it to the selected profile.
    /// @param    RunMode_e
    ///           IR Mode to switch to (either averaging modes, or Processing test mode)
    static void SetRunMode(const FW_Const::RunMode_e&);

    /// @brief    Gun Mode that handles calibration.
    /// @param    fromDesktop
    ///           Flag that's set when calibration is signalled from the Desktop App.
    ///           When true, any mouse position movements during calibration are skipped
    ///           to keep the process smooth for the end user.
    static void ExecCalMode(const bool &fromDesktop = false);

    /// @brief    Updates current sight position from IR cam.
    /// @details  Updates finalX and finalY values.
    static void GetPosition();

    /// @brief    Updates state of bad camera, prints when interval is met
    static void PrintIrError();

    /// @brief    Update the last seen value.
    /// @note     Only to be called during run mode, since this will modify the LED colour
    ///           of any (non-static) devices.
    static void UpdateLastSeen();

    #ifdef LED_ENABLE
    /// @brief    Macro that sets LEDs color depending on the mode it's set to
    static void SetLedColorFromMode();
    #endif // LED_ENABLE

    #ifdef USES_DISPLAY
    /// @brief    Redraws display based on current state.
    static void RedrawDisplay();
    #endif // USES_DISPLAY

    /// @brief    Applies loaded gun profile settings from profileData[PROFILE_COUNT]
    /// @param    profile
    ///           Profile slot number to load settings from.
    static bool SelectCalProfile(const uint8_t &profile);

    /// @brief    Set a new IR camera sensitivity, and apply to the currently selected calibration profile
    /// @param    sensitivity
    ///           New sensitivity to set for current cali profile.
    static void SetIrSensitivity(const uint8_t &sensitivity);

    /// @brief    Set a new IR layout type, and apply to the currently selected calibration profile
    /// @param    layout
    ///           New layout type to set for current cali profile.
    static void SetIrLayout(const uint8_t &layout);

    /// @brief    Saves profile settings to EEPROM
    /// @note     Blinks LEDs (if any) on success or failure.
    static int SavePreferences();

    /// @brief    Updates LightgunButtons::ButtonDesc[] buttons descriptor array
    ///           with new pin mappings and control bindings, if any.
    /// @param    lowButtons
    ///           Flag that determines whether offscreen button compatibility bit is enabled.
    ///           When true, Mouse+Keyboard slots' offscreen mapping is set to a different key.
    ///           TODO: should be able to set offscreen button mode mappings too,
    ///           but these are handled directly in firing modes currently.
    static void UpdateBindings(const bool &lowButtons = false);

    // initial gunmode
    static inline FW_Const::GunMode_e gunMode = FW_Const::GunMode_Init;

    // run mode
    static inline FW_Const::RunMode_e runMode = FW_Const::RunMode_Normal;

    // state flags, see StateFlag_e
    static inline uint32_t stateFlags = FW_Const::StateFlagsDtrReset;

    //// Camera
    // IR positioning camera
    static inline DFRobotIRPositionEx *dfrIRPos = nullptr;

    // IR camera sensitivity
    static inline DFRobotIRPositionEx::Sensitivity_e irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;

    // flag to warn docked server if camera is not working currently
    static inline bool camNotAvailable = false;

    static inline uint32_t camWarningTimestamp = 0;
    #define CAM_WARNING_INTERVAL 3000

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
    
    //// General Runtime Flags
    static inline bool justBooted = true;                              // For ops we need to do on initial boot (custom pins, joystick centering)
    static inline bool dockedSaving = false;                           // To block sending test output in docked mode.

    static LightgunButtons buttons;

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
    {SamcoPreferences::pins[OF_Const::btnGunC],     LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Gamepad,  PAD_A,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnStart],    LightgunButtons::ReportType_Keyboard, playerStartBtn,             LightgunButtons::ReportType_Keyboard, playerStartBtn,             LightgunButtons::ReportType_Gamepad,  PAD_START,  20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnSelect],   LightgunButtons::ReportType_Keyboard, playerSelectBtn,            LightgunButtons::ReportType_Keyboard, playerSelectBtn,            LightgunButtons::ReportType_Gamepad,  PAD_SELECT, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunUp],    LightgunButtons::ReportType_Gamepad,  PAD_UP,                     LightgunButtons::ReportType_Gamepad,  PAD_UP,                     LightgunButtons::ReportType_Gamepad,  PAD_UP,     20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunDown],  LightgunButtons::ReportType_Gamepad,  PAD_DOWN,                   LightgunButtons::ReportType_Gamepad,  PAD_DOWN,                   LightgunButtons::ReportType_Gamepad,  PAD_DOWN,   20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunLeft],  LightgunButtons::ReportType_Gamepad,  PAD_LEFT,                   LightgunButtons::ReportType_Gamepad,  PAD_LEFT,                   LightgunButtons::ReportType_Gamepad,  PAD_LEFT,   20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnGunRight], LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,                  LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,                  LightgunButtons::ReportType_Gamepad,  PAD_RIGHT,  20, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnPedal],    LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON4,              LightgunButtons::ReportType_Gamepad,  PAD_X,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnPedal2],   LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON5,              LightgunButtons::ReportType_Mouse,    MOUSE_BUTTON5,              LightgunButtons::ReportType_Gamepad,  PAD_B,      15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnPump],     LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,                LightgunButtons::ReportType_Mouse,    MOUSE_RIGHT,                LightgunButtons::ReportType_Gamepad,  PAD_LT,     15, BTN_AG_MASK2},
    {SamcoPreferences::pins[OF_Const::btnHome],     LightgunButtons::ReportType_Internal, 0,                          LightgunButtons::ReportType_Internal, 0,                          LightgunButtons::ReportType_Internal, 0,          15, BTN_AG_MASK2}
};

    // button count constant
    static inline constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

    // button runtime data arrays
    static inline LightgunButtonsStatic<ButtonCount> lgbData;

#endif // _OPENFIRECOMMON_H_