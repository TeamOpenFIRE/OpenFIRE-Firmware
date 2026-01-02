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

#include "OpenFIREprefs.h"
#include "OpenFIREdisplay.h"
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
    static bool SelectCalProfile(const uint &profile);

    /// @brief    Set a new IR camera sensitivity, and apply to the currently selected calibration profile
    /// @param    sensitivity
    ///           New sensitivity to set for current cali profile.
    static void SetIrSensitivity(const int &sensitivity);

    /// @brief    Set a new IR layout type, and apply to the currently selected calibration profile
    /// @param    layout
    ///           New layout type to set for current cali profile.
    static void SetIrLayout(const int &layout);

    /// @brief    Saves profile settings to EEPROM
    /// @note     Blinks LEDs (if any) on success or failure.
    static int SavePreferences();

    /// @brief    Updates LightgunButtons::ButtonDesc[] buttons descriptor array
    ///           with new pin mappings and control bindings, if any.
    /// @param    rebindStrSel
    ///           Flag that determines whether to reset the bindings of the special macros playerStartBtn/playerSelectBtn
    static void UpdateBindings(const bool &rebindStrSel);

    /// @brief    Checks Button Descriptor and replaces instances of 0xFF/0xFE with player-relative Start/Select
    static void UpdateStartSelect();

    // initial gunmode
    static inline FW_Const::GunMode_e gunMode = FW_Const::GunMode_Init;

    // run mode
    static inline FW_Const::RunMode_e runMode = FW_Const::RunMode_Normal;

    // state flags, see StateFlag_e
    static inline uint32_t stateFlags = FW_Const::StateFlagsDtrReset;

    //// Camera
    // IR positioning camera
    static inline DFRobotIRPositionEx *dfrIRPos = nullptr;

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

    static inline char playerStartBtn = '1';
    static inline char playerSelectBtn = '5';

    // For offscreen button stuff:
    static inline bool triggerPressedOffscreen = false;            // Set if shot offscreen; determines whether we release trigger btn code 1 or 2

    #ifdef USES_ANALOG
        static inline bool analogIsValid;                          // Flag set true if analog stick is mapped to valid nums
        static inline uint32_t aStickADCLastPos = 0;               // Analog-to-digital direction mask for digital outputs when settings[OF_Const::analogMode] is > 0
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

#endif // _OPENFIRECOMMON_H_