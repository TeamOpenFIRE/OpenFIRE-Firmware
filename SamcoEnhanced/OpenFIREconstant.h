 /*!
 * @file OpenFIREcommon.h
 * @brief Shared constants used throughout the OpenFIRE firmware.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#ifndef _OPENFIRECONSTANT_H_
#define _OPENFIRECONSTANT_H_

class FW_Const
{
public:
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

    // numbered index of buttons, must match ButtonDesc[] order
    enum ButtonIndex_e {
        BtnIdx_Trigger = 0,
        BtnIdx_A,
        BtnIdx_B,
        BtnIdx_Reload,
        BtnIdx_Start,
        BtnIdx_Select,
        BtnIdx_Up,
        BtnIdx_Down,
        BtnIdx_Left,
        BtnIdx_Right,
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
        BtnMask_Reload = 1 << BtnIdx_Reload,
        BtnMask_Start = 1 << BtnIdx_Start,
        BtnMask_Select = 1 << BtnIdx_Select,
        BtnMask_Up = 1 << BtnIdx_Up,
        BtnMask_Down = 1 << BtnIdx_Down,
        BtnMask_Left = 1 << BtnIdx_Left,
        BtnMask_Right = 1 << BtnIdx_Right,
        BtnMask_Pedal = 1 << BtnIdx_Pedal,
        BtnMask_Pedal2 = 1 << BtnIdx_Pedal2,
        BtnMask_Pump = 1 << BtnIdx_Pump,
        BtnMask_Home = 1 << BtnIdx_Home
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

    //// Button Masks
    // button combo to send an escape keypress
    static const constexpr uint32_t EscapeKeyBtnMask = BtnMask_Reload | BtnMask_Start;

    // button combo to enter pause mode
    static inline constexpr uint32_t EnterPauseModeBtnMask = BtnMask_Reload | BtnMask_Select;

    // button combo to enter pause mode (holding ver)
    static inline constexpr uint32_t EnterPauseModeHoldBtnMask = BtnMask_Trigger | BtnMask_A;

    // press any button to exit hotkey pause mode back to run mode (this is not a button combo)
    static inline constexpr uint32_t ExitPauseModeBtnMask = BtnMask_Reload | BtnMask_Home;

    // press and hold any button to exit simple pause menu (this is not a button combo)
    static inline constexpr uint32_t ExitPauseModeHoldBtnMask = BtnMask_A | BtnMask_B;

    // button combo to skip the center calibration step
    static inline constexpr uint32_t SkipCalCenterBtnMask = BtnMask_A;

    // button combo to save preferences to non-volatile memory
    static inline constexpr uint32_t SaveBtnMask = BtnMask_Start | BtnMask_Select;

    // button combo to increase IR sensitivity
    static inline constexpr uint32_t IRSensitivityUpBtnMask = BtnMask_B | BtnMask_Up;

    // button combo to decrease IR sensitivity
    static inline constexpr uint32_t IRSensitivityDownBtnMask = BtnMask_B | BtnMask_Down;

    // button combinations to select a run mode
    static inline constexpr uint32_t RunModeNormalBtnMask = BtnMask_Start | BtnMask_A;
    static inline constexpr uint32_t RunModeAverageBtnMask = BtnMask_Start | BtnMask_B;

    // button combination to toggle offscreen button mode in software:
    static inline constexpr uint32_t OffscreenButtonToggleBtnMask = BtnMask_Reload | BtnMask_A;

    // button combination to toggle offscreen button mode in software:
    static inline constexpr uint32_t AutofireSpeedToggleBtnMask = BtnMask_Reload | BtnMask_B;

    // button combination to toggle rumble in software:
    static inline constexpr uint32_t RumbleToggleBtnMask = BtnMask_Left;

    // button combination to toggle solenoid in software:
    static inline constexpr uint32_t SolenoidToggleBtnMask = BtnMask_Right;

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
};

#endif // OPENFIRECONSTANT_H