/*!
 * @file SamcoEnhanced.ino
 * @brief OpenFIRE - 4IR LED Lightgun sketch w/ support for force feedback and other features.
 * Forked from IR-GUN4ALL v4.2, which is based on Prow's Enhanced Fork from https://github.com/Prow7/ir-light-gun,
 * Which in itself is based on the 4IR Beta "Big Code Update" SAMCO project from https://github.com/samuelballantyne/IR-Light-Gun
 *
 * @copyright Samco, https://github.com/samuelballantyne, June 2020
 * @copyright Mike Lynch, July 2021
 * @copyright That One Seong, https://github.com/SeongGino, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @date 2024
 */
#define OPENFIRE_VERSION 4.2
#define OPENFIRE_CODENAME "Fault Carol-dev"

 // For custom builders, remember to check (COMPILING.md) for IDE instructions!
 // ISSUERS: REMEMBER TO SPECIFY YOUR USING A CUSTOM BUILD & WHAT CHANGES ARE MADE TO THE SKETCH; OTHERWISE YOUR ISSUE MAY BE CLOSED!

#include <Arduino.h>
#include <SamcoBoard.h>

// include TinyUSB or HID depending on USB stack option
#if defined(USE_TINYUSB)
#include <Adafruit_TinyUSB.h>
#elif defined(CFG_TUSB_MCU)
#error Incompatible USB stack. Use Adafruit TinyUSB.
#else
// Arduino USB stack (currently not supported, will not build)
#include <HID.h>
#endif

#include <TinyUSB_Devices.h>
#include <Wire.h>
#ifdef DOTSTAR_ENABLE
    #define LED_ENABLE
    #include <Adafruit_DotStar.h>
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
#endif
#ifdef ARDUINO_NANO_RP2040_CONNECT
    #define LED_ENABLE
    // Apparently an LED is included, but has to be communicated with through the WiFi chip (or else it throws compiler errors)
    // That said, LEDs are attached to Pins 25(G), 26(B), 27(R).
    #include <WiFiNINA.h>
#endif
#ifdef SAMCO_FLASH_ENABLE
    #include <Adafruit_SPIFlashBase.h>
#elif SAMCO_EEPROM_ENABLE
    #include <EEPROM.h>
#endif // SAMCO_FLASH_ENABLE/EEPROM_ENABLE


#include <DFRobotIRPositionEx.h>
#include <LightgunButtons.h>
#include <SamcoPositionEnhanced.h>
#include <SamcoConst.h>
#include "SamcoColours.h"
#include "SamcoPreferences.h"

#ifdef ARDUINO_ARCH_RP2040
  #include <hardware/pwm.h>
  #include <hardware/irq.h>
  // declare PWM ISR
  void rp2040pwmIrq(void);
#endif


    // GLOBAL PREDEFINES ----------------------------------------------------------------------------------------------------------
  // Enables input processing on the second core, if available. Currently exclusive to Raspberry Pi Pico, or boards based on the RP2040.
  // Isn't necessarily faster, but might make responding to force feedback more consistent.
  // If unsure, leave this uncommented - it only affects RP2040 anyways.
#define DUAL_CORE

  // Here we define the Manufacturer Name/Device Name/PID:VID of the gun as will be displayed by the operating system.
  // For multiplayer, different guns need different IDs!
  // If unsure, or are only using one gun, just leave these at their defaults!
#define MANUFACTURER_NAME "OpenFIRE"
#define DEVICE_NAME "FIRE-Con"
#define DEVICE_VID 0x0920
#define DEVICE_PID 0x1998

  // Set what player this board is mapped to by default (1-4). This will change keyboard mappings appropriate for the respective player.
  // If unsure, just leave this at 1 - the mapping can be changed at runtime by sending an 'XR#' command over Serial, where # = player number
#define PLAYER_NUMBER 1

  // Leave this uncommented to enable MAMEHOOKER support, or comment out (//) to disable references to serial reading and only use it for debugging.
  // WARNING: Has a chance of making the board lock up if TinyUSB hasn't been patched to fix serial-related lockups.
  // If you're building this for RP2040, please make sure that you have NOT installed the TinyUSB library.
  // If unsure, leave uncommented - serial activity is used for configuration, and undefining this will cause errors.
#define MAMEHOOKER

  // Leave this uncommented if your build uses hardware switches, or comment out to disable all references to hw switch functionality.
#define USES_SWITCHES

  // Leave this uncommented if your build uses a rumble motor; comment out to disable any references to rumble functionality.
#define USES_RUMBLE
#ifdef USES_RUMBLE
#endif // USES_RUMBLE

  // Leave this uncommented if your build uses a solenoid, or comment out to disable any references to solenoid functionality.
#define USES_SOLENOID
#ifdef USES_SOLENOID
    // Leave this uncommented for TMP36 temperature sensor support for a solenoid, or comment out to disable references to temperature reading or throttling.
    #define USES_TEMP
#endif // USES_SOLENOID

  // Leave this uncommented if your build uses an analog stick.
#define USES_ANALOG

  // Leave this uncommented if your build uses a four pin RGB LED.
#define FOURPIN_LED
#ifdef FOURPIN_LED
    #define LED_ENABLE
#endif // FOURPIN_LED

  // Leave this uncommented if your build uses an external NeoPixel.
#define CUSTOM_NEOPIXEL
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
#endif // CUSTOM_NEOPIXEL

//--------------------------------------------------------------------------------------------------------------------------------------
// Sanity checks and assignments for player number -> common keyboard assignments
#if PLAYER_NUMBER == 1
    char playerStartBtn = '1';
    char playerSelectBtn = '5';
#elif PLAYER_NUMBER == 2
    char playerStartBtn = '2';
    char playerSelectBtn = '6';
#elif PLAYER_NUMBER == 3
    char playerStartBtn = '3';
    char playerSelectBtn = '7';
#elif PLAYER_NUMBER == 4
    char playerStartBtn = '4';
    char playerSelectBtn = '8';
#else
    #error Undefined or out-of-range player number! Please set PLAYER_NUMBER to 1, 2, 3, or 4.
#endif // PLAYER_NUMBER

// enable extra serial debug during run mode
//#define PRINT_VERBOSE 1
//#define DEBUG_SERIAL 1
//#define DEBUG_SERIAL 2

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
    BtnMask_Pump = 1 << BtnIdx_Pump,
    BtnMask_Home = 1 << BtnIdx_Home
};

// Button descriptor
// The order of the buttons is the order of the button bitmask
// must match ButtonIndex_e order, and the named bitmask values for each button
// see LightgunButtons::Desc_t, format is: 
// {pin, report type, report code (ignored for internal), offscreen report type, offscreen report code, gamepad output report type, gamepad output report code, debounce time, debounce mask, label}
LightgunButtons::Desc_t LightgunButtons::ButtonDesc[] = {
    {SamcoPreferences::pins.bTrigger, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, PAD_RT, 15, BTN_AG_MASK}, // Barry says: "I'll handle this."
    {SamcoPreferences::pins.bGunA, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_LT, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bGunB, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Gamepad, PAD_Y, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bStart, LightgunButtons::ReportType_Keyboard, playerStartBtn, LightgunButtons::ReportType_Keyboard, playerStartBtn, LightgunButtons::ReportType_Gamepad, PAD_START, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins.bSelect, LightgunButtons::ReportType_Keyboard, playerSelectBtn, LightgunButtons::ReportType_Keyboard, playerSelectBtn, LightgunButtons::ReportType_Gamepad, PAD_SELECT, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins.bGunUp, LightgunButtons::ReportType_Gamepad, PAD_UP, LightgunButtons::ReportType_Gamepad, PAD_UP, LightgunButtons::ReportType_Gamepad, PAD_UP, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins.bGunDown, LightgunButtons::ReportType_Gamepad, PAD_DOWN, LightgunButtons::ReportType_Gamepad, PAD_DOWN, LightgunButtons::ReportType_Gamepad, PAD_DOWN, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins.bGunLeft, LightgunButtons::ReportType_Gamepad, PAD_LEFT, LightgunButtons::ReportType_Gamepad, PAD_LEFT, LightgunButtons::ReportType_Gamepad, PAD_LEFT, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins.bGunRight, LightgunButtons::ReportType_Gamepad, PAD_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_RIGHT, 20, BTN_AG_MASK2},
    {SamcoPreferences::pins.bGunC, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Gamepad, PAD_A, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bPedal, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Gamepad, PAD_X, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bPump, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_LT, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bHome, LightgunButtons::ReportType_Internal, 0, LightgunButtons::ReportType_Internal, 0, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK2}
};

// button count constant
constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

// button runtime data arrays
LightgunButtonsStatic<ButtonCount> lgbData;

// button object instance
LightgunButtons buttons(lgbData, ButtonCount);

/*
// WIP, some sort of generic button handler table for pause mode
// pause button function
typedef void (*PauseModeBtnFn_t)();

// pause mode function
typedef struct PauseModeFnEntry_s {
    uint32_t buttonMask;
    PauseModeBtnFn_t pfn;
} PauseModeFnEntry_t;
*/

// button combo to send an escape keypress
uint32_t EscapeKeyBtnMask = BtnMask_Reload | BtnMask_Start;

// button combo to enter pause mode
uint32_t EnterPauseModeBtnMask = BtnMask_Reload | BtnMask_Select;

// button combo to enter pause mode (holding ver)
uint32_t EnterPauseModeHoldBtnMask = BtnMask_Trigger | BtnMask_A;

// press any button to exit hotkey pause mode back to run mode (this is not a button combo)
uint32_t ExitPauseModeBtnMask = BtnMask_Reload | BtnMask_Home;

// press and hold any button to exit simple pause menu (this is not a button combo)
uint32_t ExitPauseModeHoldBtnMask = BtnMask_A | BtnMask_B;

// press any button to cancel the calibration (this is not a button combo)
uint32_t CancelCalBtnMask = BtnMask_Reload | BtnMask_Start | BtnMask_Select;

// button combo to skip the center calibration step
uint32_t SkipCalCenterBtnMask = BtnMask_A;

// button combo to save preferences to non-volatile memory
uint32_t SaveBtnMask = BtnMask_Start | BtnMask_Select;

// button combo to increase IR sensitivity
uint32_t IRSensitivityUpBtnMask = BtnMask_B | BtnMask_Up;

// button combo to decrease IR sensitivity
uint32_t IRSensitivityDownBtnMask = BtnMask_B | BtnMask_Down;

// button combinations to select a run mode
uint32_t RunModeNormalBtnMask = BtnMask_Start | BtnMask_A;
uint32_t RunModeAverageBtnMask = BtnMask_Start | BtnMask_B;

// button combination to toggle offscreen button mode in software:
uint32_t OffscreenButtonToggleBtnMask = BtnMask_Reload | BtnMask_A;

// button combination to toggle offscreen button mode in software:
uint32_t AutofireSpeedToggleBtnMask = BtnMask_Reload | BtnMask_B;

// button combination to toggle rumble in software:
uint32_t RumbleToggleBtnMask = BtnMask_Left;

// button combination to toggle solenoid in software:
uint32_t SolenoidToggleBtnMask = BtnMask_Right;

// colour when no IR points are seen
uint32_t IRSeen0Color = WikiColor::Amber;

// colour when calibrating
uint32_t CalModeColor = WikiColor::Red;

// number of profiles
constexpr byte ProfileCount = 4;

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

// profiles ----------------------------------------------------------------------------------------------
// defaults can be populated here, but any values in EEPROM/Flash will override these.
// if you have original Samco calibration values, multiply by 4 for the center position and
// scale is multiplied by 1000 and stored as an unsigned integer, see SamcoPreferences::Calibration_t
SamcoPreferences::ProfileData_t profileData[ProfileCount] = {
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {1500, 1000, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0}
};
//  ------------------------------------------------------------------------------------------------------
// profile descriptor
typedef struct ProfileDesc_s {
    // button(s) to select the profile
    uint32_t buttonMask;
    
    // LED colour
    uint32_t color;

    // button label
    const char* buttonLabel;
    
    // optional profile label
    const char* profileLabel;
} ProfileDesc_t;

// profile descriptor
static const ProfileDesc_t profileDesc[ProfileCount] = {
    {BtnMask_A, WikiColor::Cerulean_blue, "A", "TV Fisheye Lens"},
    {BtnMask_B, WikiColor::Cornflower_blue, "B", "TV Wide-angle Lens"},
    {BtnMask_Start, WikiColor::Green, "Start", "TV"},
    {BtnMask_Select, WikiColor::Green_Lizard, "Select", "Monitor"}
};



// overall calibration defaults, no need to change if data saved to NV memory or populate the profile table
// see profileData[] array below for specific profile defaults
int xCenter = MouseMaxX / 2;
int yCenter = MouseMaxY / 2;
float xScale = 1.64;
float yScale = 0.95;

// step size for adjusting the scale
constexpr float ScaleStep = 0.001;

int finalX = 0;         // Values after tilt correction
int finalY = 0;

int moveXAxis = 0;      // Unconstrained mouse postion
int moveYAxis = 0;               
int moveXAxisArr[3] = {0, 0, 0};
int moveYAxisArr[3] = {0, 0, 0};
int moveIndex = 0;

int conMoveXAxis = 0;   // Constrained mouse postion
int conMoveYAxis = 0;

  // The boring inits related to the things I added.
// Offscreen bits:
bool offXAxis = false;                           // Supercedes the inliner every trigger pull, we just check each axis individually.
bool offYAxis = false;

// Boring values for the solenoid timing stuff:
#ifdef USES_SOLENOID
    bool burstFireActive = false;
    unsigned long previousMillisSol = 0;         // our timer (holds the last time since a successful interval pass)
    bool solenoidFirstShot = false;              // default to off, but actually set this the first time we shoot.
    #ifdef USES_TEMP
        uint16_t tempNormal = 50;                   // Solenoid: Anything below this value is "normal" operating temperature for the solenoid, in Celsius.
        uint16_t tempWarning = 60;                  // Solenoid: Above normal temps, this is the value up to where we throttle solenoid activation, in Celsius.
        const unsigned int solenoidWarningInterval = SamcoPreferences::settings.solenoidFastInterval * 5; // for if solenoid is getting toasty.
    #endif // USES_TEMP
#endif // USES_SOLENOID

// For burst firing stuff:
byte burstFireCount = 0;                         // What shot are we on?
byte burstFireCountLast = 0;                     // What shot have we last processed?
bool burstFiring = false;                        // Are we in a burst fire command?

// For offscreen button stuff:
bool offscreenButton = false;                    // Does shooting offscreen also send a button input (for buggy games that don't recognize off-screen shots)? Default to off.
bool offscreenBShot = false;                     // For offscreenButton functionality, to track if we shot off the screen.
bool buttonPressed = false;                      // Sanity check.

// For autofire:
bool triggerHeld = false;                        // Trigger SHOULDN'T be being pulled by default, right?

// For rumble:
#ifdef USES_RUMBLE
    uint16_t rumbleIntensity = 255;              // The strength of the rumble motor, 0=off to 255=maxPower.
    uint16_t rumbleInterval = 125;               // How long to wait for the whole rumble command, in ms.
    unsigned long previousMillisRumble = 0;      // our time since the rumble motor event started
    bool rumbleHappening = false;                // To keep track on if this is a rumble command or not.
    bool rumbleHappened = false;                 // If we're holding, this marks we sent a rumble command already.
    // We need the rumbleHappening because of the variable nature of the PWM controlling the motor.
#endif // USES_RUMBLE

#ifdef USES_ANALOG
    bool analogIsValid;                          // Flag set true if analog stick is mapped to valid nums
    bool analogStickPolled = false;              // Flag set on if the stick was polled recently, to prevent overloading with aStick updates.
    unsigned long previousStickPoll = 0;         // timestamp of last stick poll
    byte analogPollInterval = 2;                 // amount of time to wait after irPosUpdateTick to update analog position, in ms
#endif // USES_ANALOG

#ifdef FOURPIN_LED
    bool ledIsValid;                             // Flag set true if RGB pins are mapped to valid numbers
#endif // FOURPIN_LED

#ifdef MAMEHOOKER
// For serial mode:
    bool serialMode = false;                         // Set if we're prioritizing force feedback over serial commands or not.
    bool offscreenButtonSerial = false;              // Serial-only version of offscreenButton toggle.
    byte serialQueue = 0b00000000;                   // Bitmask of events we've queued from the serial receipt.
    // from least to most significant bit: solenoid digital, solenoid pulse, rumble digital, rumble pulse, R/G/B direct, RGB (any) pulse.
    #ifdef LED_ENABLE
    unsigned long serialLEDPulsesLastUpdate = 0;     // The timestamp of the last serial-invoked LED pulse update we iterated.
    unsigned int serialLEDPulsesLength = 2;          // How long each stage of a serial-invoked pulse rumble is, in ms.
    bool serialLEDChange = false;                    // Set on if we set an LED command this cycle.
    bool serialLEDPulseRising = true;                // In LED pulse events, is it rising now? True to indicate rising, false to indicate falling; default to on for very first pulse.
    byte serialLEDPulses = 0;                        // How many LED pulses are we being told to do?
    byte serialLEDPulsesLast = 0;                    // What LED pulse we've processed last.
    byte serialLEDR = 0;                             // For the LED, how strong should it be?
    byte serialLEDG = 0;                             // Each channel is defined as three brightness values
    byte serialLEDB = 0;                             // So yeah.
    byte serialLEDPulseColorMap = 0b00000000;        // The map of what LEDs should be pulsing (we use the rightmost three of this bitmask for R, G, or B).
    #endif // LED_ENABLE
    #ifdef USES_RUMBLE
    unsigned long serialRumbPulsesLastUpdate = 0;    // The timestamp of the last serial-invoked pulse rumble we updated.
    unsigned int serialRumbPulsesLength = 60;        // How long each stage of a serial-invoked pulse rumble is, in ms.
    byte serialRumbPulseStage = 0;                   // 0 = start/rising, 1 = peak, 2 = falling, 3 = reset to start
    byte serialRumbPulses = 0;                       // If rumble is commanded to do pulse responses, how many?
    byte serialRumbPulsesLast = 0;                   // Counter of how many pulse rumbles we did so far.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
    unsigned long serialSolPulsesLastUpdate = 0;     // The timestamp of the last serial-invoked pulse solenoid event we updated.
    unsigned int serialSolPulsesLength = 80;         // How long to wait between each solenoid event in a pulse, in ms.
    bool serialSolPulseOn = false;                   // Because we can't just read it normally, is the solenoid getting PWM high output now?
    int serialSolPulses = 0;                         // How many solenoid pulses are we being told to do?
    int serialSolPulsesLast = 0;                     // What solenoid pulse we've processed last.
    #endif // USES_SOLENOID
#endif // MAMEHOOKER

unsigned int lastSeen = 0;

bool justBooted = true;                              // For ops we need to do on initial boot (custom pins, joystick centering)
bool dockedSaving = false;                           // To block sending test output in docked mode.
bool dockedCalibrating = false;                      // If set, calibration will send back to docked mode.

unsigned long testLastStamp;                         // Timestamp of last print in test mode.
const byte testPrintInterval = 50;                   // Minimum time allowed between test mode printouts.

// profile in use
unsigned int selectedProfile = 0;

// Samco positioning
SamcoPositionEnhanced mySamco;

// operating modes
enum GunMode_e {
    GunMode_Init = -1,
    GunMode_Run = 0,
    GunMode_CalHoriz = 1,
    GunMode_CalVert = 2,
    GunMode_CalCenter = 3,
    GunMode_Pause = 4,
    GunMode_Docked = 5
};
GunMode_e gunMode = GunMode_Init;   // initial mode

enum PauseModeSelection_e {
    PauseMode_Calibrate = 0,
    PauseMode_ProfileSelect,
    PauseMode_Save,
    #ifdef USES_RUMBLE
    PauseMode_RumbleToggle,
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
    PauseMode_SolenoidToggle,
    //PauseMode_BurstFireToggle,
    #endif // USES_SOLENOID
    PauseMode_EscapeSignal
};
// Selector for which option in the simple pause menu you're scrolled on.
byte pauseModeSelection = 0;
// Selector for which profile in the profile selector of the simple pause menu you're picking.
byte profileModeSelection;
// Flag to tell if we're in the profile selector submenu of the simple pause menu.
bool pauseModeSelectingProfile = false;

// Timestamp of when we started holding a buttons combo.
unsigned long pauseHoldStartstamp;
bool pauseHoldStarted = false;
bool pauseExitHoldStarted = false;

// run mode
RunMode_e runMode = RunMode_Normal;

// IR camera sensitivity
DFRobotIRPositionEx::Sensitivity_e irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;

static const char* RunModeLabels[RunMode_Count] = {
    "Normal",
    "Averaging",
    "Averaging2",
    "Processing"
};

// preferences saved in non-volatile memory, populated with defaults 
SamcoPreferences::Preferences_t SamcoPreferences::profiles = {
    profileData, ProfileCount, // profiles
    0, // default profile
};

SamcoPreferences::TogglesMap_t SamcoPreferences::toggles;
SamcoPreferences::PinsMap_t SamcoPreferences::pins;
SamcoPreferences::SettingsMap_t SamcoPreferences::settings;
SamcoPreferences::USBMap_t SamcoPreferences::usb;

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
constexpr uint32_t StateFlagsDtrReset = StateFlag_PrintSelectedProfile | StateFlag_PrintPreferences | StateFlag_PrintPreferencesStorage;

// state flags, see StateFlag_e
uint32_t stateFlags = StateFlagsDtrReset;

// internal addressable LEDs inits
#ifdef DOTSTAR_ENABLE
// note if the colours don't match then change the colour format from BGR
// apparently different lots of DotStars may have different colour ordering ¯\_(ツ)_/¯
Adafruit_DotStar dotstar(1, DOTSTAR_DATAPIN, DOTSTAR_CLOCKPIN, DOTSTAR_BGR);
#endif // DOTSTAR_ENABLE

#ifdef NEOPIXEL_PIN
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif // CUSTOM_NEOPIXEL
#ifdef CUSTOM_NEOPIXEL
Adafruit_NeoPixel* externPixel;
// Amount of pixels in a strand.
uint16_t customLEDcount = 1;
#endif // CUSTOM_NEOPIXEL

#ifdef SAMCO_EEPROM_ENABLE
// EEPROM non-volatile storage
static const char* NVRAMlabel = "EEPROM";

// flag to indicate if non-volatile storage is available
// unconditional for EEPROM
bool nvAvailable = true;
#endif

// non-volatile preferences error code
int nvPrefsError = SamcoPreferences::Error_NoStorage;

// preferences instance
SamcoPreferences samcoPreferences;

// number of times the IR camera will update per second
constexpr unsigned int IRCamUpdateRate = 209;

// timer will set this to 1 when the IR position can update
volatile unsigned int irPosUpdateTick = 0;

#ifdef DEBUG_SERIAL
static unsigned long serialDbMs = 0;
static unsigned long frameCount = 0;
static unsigned long irPosCount = 0;
#endif

// used for periodic serial prints
unsigned long lastPrintMillis = 0;

#ifdef USE_TINYUSB
// AbsMouse5 instance (this is hardcoded as the second definition, so weh)
AbsMouse5_ AbsMouse5(2);
#else
// AbsMouse5 instance
AbsMouse5_ AbsMouse5(1);
#endif

// IR positioning camera
DFRobotIRPositionEx *dfrIRPos;

//-----------------------------------------------------------------------------------------------------
// The main show!
void setup() {
    Serial.begin(9600);   // 9600 = 1ms data transfer rates, default for MAMEHOOKER COM devices.
    Serial.setTimeout(0);
 
    // initialize EEPROM device. Arduino AVR has a 1k flash, so use that.
    EEPROM.begin(1024); 
    
    if(nvAvailable) {
        LoadPreferences();
        if(nvPrefsError == SamcoPreferences::Error_NoData) {
            Serial.println("No data detected, setting defaults!");
            SamcoPreferences::ResetPreferences();
        } else if(nvPrefsError == SamcoPreferences::Error_Success) {
            Serial.println("Data detected, pulling settings from EEPROM!");
            // use values from preferences
            // if default profile is valid then use it
            if(SamcoPreferences::profiles.selectedProfile < ProfileCount) {
                // note, just set the value here not call the function to do the set
                selectedProfile = SamcoPreferences::profiles.selectedProfile;

                // set the current IR camera sensitivity
                if(profileData[selectedProfile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max) {
                    irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)profileData[selectedProfile].irSensitivity;
                }

                // set the run mode
                if(profileData[selectedProfile].runMode < RunMode_Count) {
                    runMode = (RunMode_e)profileData[selectedProfile].runMode;
                }
            }
            SamcoPreferences::LoadToggles();
            if(SamcoPreferences::toggles.customPinsInUse) {
                SamcoPreferences::LoadPins();
                UpdateBindings(SamcoPreferences::toggles.lowButtonMode);
            }
            SamcoPreferences::LoadSettings();
            SamcoPreferences::LoadUSBID();
        }
    }
 
    // We're setting our custom USB identifiers, as defined in the configuration area!
    #ifdef USE_TINYUSB
        TinyUSBInit();
    #endif // USE_TINYUSB

    #if defined(ARDUINO_ADAFRUIT_ITSYBITSY_RP2040) || defined(ARDUINO_ADAFRUIT_KB2040_RP2040)
        // ItsyBitsy only exposes I2C1 pins by default, and KB2040 is meant to be GUN4IR compatible, whose schematic which uses these pins.
        dfrIRPos = new DFRobotIRPositionEx(Wire1);
        // ensure Wire1 SDA and SCL are correct for the ItsyBitsy RP2040 and KB2040
        Wire1.setSDA(2);
        Wire1.setSCL(3);
    #else
        // Every other board (afaik) don't have this limitation.
        dfrIRPos = new DFRobotIRPositionEx(Wire);
        #ifdef ARDUINO_NANO_RP2040_CONNECT
            Wire.setSDA(12);
            Wire.setSCL(13);
        #else // assumes ARDUINO_RASPBERRY_PI_PICO
            Wire.setSDA(20);
            Wire.setSCL(21);
        #endif // board
    #endif

    // Check if custom pins are in use and if pins are valid at a surface level
    if(SamcoPreferences::toggles.customPinsInUse &&
       SamcoPreferences::pins.pCamSDA >= 0 &&
       SamcoPreferences::pins.pCamSCL >= 0 &&
       SamcoPreferences::pins.pCamSDA != SamcoPreferences::pins.pCamSCL) {
        // Sanity check: which channel do these pins correlate to?
        if(bitRead(SamcoPreferences::pins.pCamSCL, 1) && bitRead(SamcoPreferences::pins.pCamSDA, 1)) {
            // I2C1
            if(bitRead(SamcoPreferences::pins.pCamSCL, 0) && !bitRead(SamcoPreferences::pins.pCamSDA, 0)) {
                // SDA/SCL are indeed on verified correct pins
                Wire1.setSDA(SamcoPreferences::pins.pCamSDA);
                Wire1.setSCL(SamcoPreferences::pins.pCamSCL);
                delete dfrIRPos;
                dfrIRPos = new DFRobotIRPositionEx(Wire1);
            }
        } else if(!bitRead(SamcoPreferences::pins.pCamSCL, 1) && !bitRead(SamcoPreferences::pins.pCamSDA, 1)) {
            // I2C0
            if(bitRead(SamcoPreferences::pins.pCamSCL, 0) && !bitRead(SamcoPreferences::pins.pCamSDA, 0)) {
                // SDA/SCL are indeed on verified correct pins
                Wire.setSDA(SamcoPreferences::pins.pCamSDA);
                Wire.setSCL(SamcoPreferences::pins.pCamSCL);
                delete dfrIRPos;
                dfrIRPos = new DFRobotIRPositionEx(Wire);
            }
        }
    }

    // initialize buttons & feedback devices
    buttons.Begin();
    FeedbackSet();
    #ifdef LED_ENABLE
        LedInit();
    #endif // LED_ENABLE

    // Start IR Camera with basic data format
    dfrIRPos->begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity);
    
#ifdef USE_TINYUSB
    // Initializing the USB devices chunk.
    TinyUSBDevices.begin(1);
#endif
    
    AbsMouse5.init(MouseMaxX, MouseMaxY, true);
    
    // fetch the calibration data, other values already handled in ApplyInitialPrefs() 
    SelectCalPrefs(selectedProfile);

#ifdef USE_TINYUSB
    // wait until device mounted
    while(!USBDevice.mounted()) { yield(); }
#else
    // was getting weird hangups... maybe nothing, or maybe related to dragons, so wait a bit
    delay(100);
#endif

    // IR camera maxes out motion detection at ~300Hz, and millis() isn't good enough
    startIrCamTimer(IRCamUpdateRate);

    // First boot sanity checks.
    // Check if loading has failde
    if(nvPrefsError != SamcoPreferences::Error_Success ||
    profileData[selectedProfile].xCenter == 0 || profileData[selectedProfile].yCenter == 0) {
        // SHIT, it's a first boot! Prompt to start calibration.
        Serial.println("Preferences data is empty!");
        SetMode(GunMode_CalCenter);
        Serial.println("Pull the trigger to start your first calibration!");
        unsigned int timerIntervalShort = 600;
        unsigned int timerInterval = 1000;
        LedOff();
        unsigned long lastT = millis();
        bool LEDisOn = false;
        while(!(buttons.pressedReleased == BtnMask_Trigger)) {
            // Check and process serial commands, in case user needs to change EEPROM settings.
            if(Serial.available()) {
                SerialProcessingDocked();
            }
            if(gunMode == GunMode_Docked) {
                ExecGunModeDocked();
                SetMode(GunMode_CalCenter);
            }
            buttons.Poll(1);
            buttons.Repeat();
            // LED update:
            if(LEDisOn) {
                unsigned long t = millis();
                if(t - lastT > timerInterval) {
                    LedOff();
                    LEDisOn = false;
                    lastT = millis();
                }
            } else {
                unsigned long t = millis();
                if(t - lastT > timerIntervalShort) {
                    LedUpdate(255,125,0);
                    LEDisOn = true;
                    lastT = millis();
                } 
            }
        }
    } else {
        // this will turn off the DotStar/RGB LED and ensure proper transition to Run
        SetMode(GunMode_Run);
    }
}

// inits and/or re-sets feedback pins using currently loaded pin values
// is run both in setup and at runtime
void FeedbackSet()
{
    #ifdef USES_RUMBLE
        if(SamcoPreferences::pins.oRumble >= 0) {
            pinMode(SamcoPreferences::pins.oRumble, OUTPUT);
        } else {
            SamcoPreferences::toggles.rumbleActive = false;
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        if(SamcoPreferences::pins.oSolenoid >= 0) {
            pinMode(SamcoPreferences::pins.oSolenoid, OUTPUT);
        } else {
            SamcoPreferences::toggles.solenoidActive = false;
        }
    #endif // USES_SOLENOID
    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(SamcoPreferences::pins.sRumble >= 0) {
                pinMode(SamcoPreferences::pins.sRumble, INPUT_PULLUP);
            }
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            if(SamcoPreferences::pins.sSolenoid >= 0) {
                pinMode(SamcoPreferences::pins.sSolenoid, INPUT_PULLUP);
            }  
        #endif // USES_SOLENOID
        if(SamcoPreferences::pins.sAutofire >= 0) {
            pinMode(SamcoPreferences::pins.sAutofire, INPUT_PULLUP);
        }
    #endif // USES_SWITCHES
    #ifdef USES_ANALOG
        analogReadResolution(12);
        #ifdef USES_TEMP
        if(SamcoPreferences::pins.aStickX >= 0 && SamcoPreferences::pins.aStickY >= 0 && SamcoPreferences::pins.aStickX != SamcoPreferences::pins.aStickY &&
        SamcoPreferences::pins.aStickX != SamcoPreferences::pins.aTMP36 && SamcoPreferences::pins.aStickY != SamcoPreferences::pins.aTMP36) {
        #else
        if(SamcoPreferences::pins.aStickX >= 0 && SamcoPreferences::pins.aStickY >= 0 && SamcoPreferences::pins.aStickX != SamcoPreferences::pins.aStickY) {
        #endif // USES_TEMP
            //pinMode(analogPinX, INPUT);
            //pinMode(analogPinY, INPUT);
            analogIsValid = true;
        } else {
            analogIsValid = false;
        }
    #endif // USES_ANALOG
    #if defined(LED_ENABLE) && defined(FOURPIN_LED)
    if(SamcoPreferences::pins.oLedR < 0 || SamcoPreferences::pins.oLedG < 0 || SamcoPreferences::pins.oLedB < 0) {
        Serial.println("RGB values not valid! Disabling four pin access.");
        ledIsValid = false;
    } else {
        pinMode(SamcoPreferences::pins.oLedR, OUTPUT);
        pinMode(SamcoPreferences::pins.oLedG, OUTPUT);
        pinMode(SamcoPreferences::pins.oLedB, OUTPUT);
        ledIsValid = true;
    }
    #endif // FOURPIN_LED
    #ifdef CUSTOM_NEOPIXEL
    if(SamcoPreferences::pins.oPixel >= 0) {
        externPixel = new Adafruit_NeoPixel(customLEDcount, SamcoPreferences::pins.oPixel, NEO_GRB + NEO_KHZ800);
    }
    #endif // CUSTOM_NEOPIXEL
}

// clears currently loaded feedback pins
// is run both in setup and at runtime
void PinsReset()
{
    #ifdef USES_RUMBLE
        if(SamcoPreferences::pins.oRumble >= 0) {
            pinMode(SamcoPreferences::pins.oRumble, INPUT);
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        if(SamcoPreferences::pins.oSolenoid >= 0) {
            pinMode(SamcoPreferences::pins.oSolenoid, INPUT);
        }
    #endif // USES_SOLENOID
    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(SamcoPreferences::pins.sRumble >= 0) {
                pinMode(SamcoPreferences::pins.sRumble, INPUT);
            }
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            if(SamcoPreferences::pins.sSolenoid >= 0) {
                pinMode(SamcoPreferences::pins.sSolenoid, INPUT);
            }  
        #endif // USES_SOLENOID
        if(SamcoPreferences::pins.sAutofire >= 0) {
            pinMode(SamcoPreferences::pins.sAutofire, INPUT);
        }
    #endif // USES_SWITCHES
    #ifdef LED_ENABLE
        LedOff();
        #ifdef FOURPIN_LED
            if(ledIsValid) {
                pinMode(SamcoPreferences::pins.oLedR, INPUT);
                pinMode(SamcoPreferences::pins.oLedG, INPUT);
                pinMode(SamcoPreferences::pins.oLedB, INPUT);
            }
        #endif // FOURPIN_LED
        #ifdef CUSTOM_NEOPIXEL
            if(externPixel != nullptr) {
                delete externPixel;
            }
            if(SamcoPreferences::pins.oPixel >= 0) {
                externPixel = new Adafruit_NeoPixel(customLEDcount, SamcoPreferences::pins.oPixel, NEO_GRB + NEO_KHZ800);
                externPixel->begin();
            }
        #endif // CUSTOM_NEOPIXEL
    #endif // LED_ENABLE
}

#ifdef USE_TINYUSB
// Initializes TinyUSB identifier
// Values are pulled from EEPROM values that were loaded earlier in setup()
void TinyUSBInit()
{
    TinyUSBDevice.setManufacturerDescriptor(MANUFACTURER_NAME);
    if(SamcoPreferences::usb.devicePID) {
        TinyUSBDevice.setID(DEVICE_VID, SamcoPreferences::usb.devicePID);
        if(SamcoPreferences::usb.deviceName[0] == '\0') {
            TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
        } else {
            TinyUSBDevice.setProductDescriptor(SamcoPreferences::usb.deviceName);
        }
    } else {
        TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
        TinyUSBDevice.setID(DEVICE_VID, DEVICE_PID);
    }
}
#endif // USE_TINYUSB

void startIrCamTimer(int frequencyHz)
{
    rp2040EnablePWMTimer(0, frequencyHz);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, rp2040pwmIrq);
    irq_set_enabled(PWM_IRQ_WRAP, true);
}

void rp2040EnablePWMTimer(unsigned int slice_num, unsigned int frequency)
{
    pwm_config pwmcfg = pwm_get_default_config();
    float clkdiv = (float)clock_get_hz(clk_sys) / (float)(65535 * frequency);
    if(clkdiv < 1.0f) {
        clkdiv = 1.0f;
    } else {
        // really just need to round up 1 lsb
        clkdiv += 2.0f / (float)(1u << PWM_CH1_DIV_INT_LSB);
    }
    
    // set the clock divider in the config and fetch the actual value that is used
    pwm_config_set_clkdiv(&pwmcfg, clkdiv);
    clkdiv = (float)pwmcfg.div / (float)(1u << PWM_CH1_DIV_INT_LSB);
    
    // calculate wrap value that will trigger the IRQ for the target frequency
    pwm_config_set_wrap(&pwmcfg, (float)clock_get_hz(clk_sys) / (frequency * clkdiv));
    
    // initialize and start the slice and enable IRQ
    pwm_init(slice_num, &pwmcfg, true);
    pwm_set_irq_enabled(slice_num, true);
}

void rp2040pwmIrq(void)
{
    pwm_hw->intr = 0xff;
    irPosUpdateTick = 1;
}

#if defined(ARDUINO_ARCH_RP2040) && defined(DUAL_CORE)
// Second core setup
// does... nothing, since timing is kinda important.
void setup1()
{
    // i sleep
}

// Second core main loop
// currently handles all button & serial processing when Core 0 is in ExecRunMode()
void loop1()
{
    while(gunMode == GunMode_Run) {
        // For processing the trigger specifically.
        // (buttons.debounced is a binary variable intended to be read 1 bit at a time, with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
        buttons.Poll(0);

        #ifdef MAMEHOOKER
            // Stalling mitigation: make both cores pause when reading serial
            if(Serial.available()) {
                SerialProcessing();
            }
            if(!serialMode) {   // Have we released a serial signal pulse? If not,
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFire();                                      // Handle button events and feedback ourselves.
                } else {   // Or we haven't pressed the trigger.
                    TriggerNotFire();                                   // Releasing button inputs and sending stop signals to feedback devices.
                }
            } else {   // This is if we've received a serial signal pulse in the last n millis.
                // For safety reasons, we're just using the second core for polling, and the main core for sending signals entirely. Too much a headache otherwise. =w='
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                                // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                             // Release button inputs.
                }
                SerialHandling();                                       // Process the force feedback.
            }
        #else
            if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                          // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                       // Releasing button inputs and sending stop signals to feedback devices.
            }
        #endif // MAMEHOOKER

        #ifdef USES_ANALOG
            if(analogIsValid) {
                // Poll the analog values 2ms after the IR sensor has updated so as not to overload the USB buffer.
                // stickPolled and previousStickPoll are reset/updated after irPosUpdateTick.
                if(!analogStickPolled) {
                    if(buttons.pressed || buttons.released) {
                        previousStickPoll = millis();
                    } else if(millis() - previousStickPoll > analogPollInterval) {
                        AnalogStickPoll();
                        analogStickPolled = true;
                    }
                }
            }
        #endif // USES_ANALOG
        
        if(buttons.pressedReleased == EscapeKeyBtnMask) {
            SendEscapeKey();
        }

        if(SamcoPreferences::toggles.holdToPause) {
            if((buttons.debounced == EnterPauseModeHoldBtnMask)
            && !lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                if(!serialMode) {
                    Serial.println("Started holding pause mode signal buttons!");
                }
            } else if(pauseHoldStarted && (buttons.debounced != EnterPauseModeHoldBtnMask || lastSeen)) {
                pauseHoldStarted = false;
                if(!serialMode) {
                    Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");
                }
            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > SamcoPreferences::settings.pauseHoldLength) {
                    // MAKE SURE EVERYTHING IS DISENGAGED:
                    #ifdef USES_SOLENOID
                        digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
                        solenoidFirstShot = false;
                    #endif // USES_SOLENOID
                    #ifdef USES_RUMBLE
                        digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                        rumbleHappening = false;
                        rumbleHappened = false;
                    #endif // USES_RUMBLE
                    offscreenBShot = false;
                    buttonPressed = false;
                    triggerHeld = false;
                    burstFiring = false;
                    burstFireCount = 0;
                    SetMode(GunMode_Pause);
                    buttons.ReportDisable();
                }
            }
        } else {
            if(buttons.pressedReleased == EnterPauseModeBtnMask) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                buttons.ReportDisable();
                SetMode(GunMode_Pause);
                // at this point, the other core should be stopping us now.
            } else if(buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                buttons.ReportDisable();
                SetMode(GunMode_Pause);
                // at this point, the other core should be stopping us now.
            }
        }
    }
}
#endif // ARDUINO_ARCH_RP2040 || DUAL_CORE

// Main core events hub
// splits off into subsequent ExecModes depending on circumstances
void loop()
{
    // poll/update button states with 1ms interval so debounce mask is more effective
    buttons.Poll(1);
    buttons.Repeat();
    #ifdef MAMEHOOKER
    while(Serial.available()) {                             // So we can process serial requests while in Pause Mode.
        SerialProcessing();
    }
    #endif // MAMEHOOKER

    if(SamcoPreferences::toggles.holdToPause && pauseHoldStarted) {
        #ifdef USES_RUMBLE
            analogWrite(SamcoPreferences::pins.oRumble, rumbleIntensity);
            delay(300);
            digitalWrite(SamcoPreferences::pins.oRumble, LOW);
        #endif // USES_RUMBLE
        while(buttons.debounced != 0) {
            // Should release the buttons to continue, pls.
            buttons.Poll(1);
        }
        pauseHoldStarted = false;
        pauseModeSelection = PauseMode_Calibrate;
        pauseModeSelectingProfile = false;
    }

    switch(gunMode) {
        case GunMode_Pause:
            if(SamcoPreferences::toggles.simpleMenu) {
                if(pauseModeSelectingProfile) {
                    if(buttons.pressedReleased == BtnMask_A) {
                        SetProfileSelection(false);
                    } else if(buttons.pressedReleased == BtnMask_B) {
                        SetProfileSelection(true);
                    } else if(buttons.pressedReleased == BtnMask_Trigger) {
                        SelectCalProfile(profileModeSelection);
                        pauseModeSelectingProfile = false;
                        pauseModeSelection = PauseMode_Calibrate;
                        if(!serialMode) {
                            Serial.print("Switched to profile: ");
                            Serial.println(profileDesc[selectedProfile].profileLabel);
                            Serial.println("Going back to the main menu...");
                            Serial.println("Selecting: Calibrate current profile");
                        }
                    } else if(buttons.pressedReleased & ExitPauseModeBtnMask) {
                        if(!serialMode) {
                            Serial.println("Exiting profile selection.");
                        }
                        pauseModeSelectingProfile = false;
                        #ifdef LED_ENABLE
                            for(byte i = 0; i < 2; i++) {
                                LedUpdate(180,180,180);
                                delay(125);
                                LedOff();
                                delay(100);
                            }
                            LedUpdate(255,0,0);
                        #endif // LED_ENABLE
                        pauseModeSelection = PauseMode_Calibrate;
                    }
                } else if(buttons.pressedReleased == BtnMask_A) {
                    SetPauseModeSelection(false);
                } else if(buttons.pressedReleased == BtnMask_B) {
                    SetPauseModeSelection(true);
                } else if(buttons.pressedReleased == BtnMask_Trigger) {
                    switch(pauseModeSelection) {
                        case PauseMode_Calibrate:
                          SetMode(GunMode_CalCenter);
                          if(!serialMode) {
                              Serial.print("Calibrating for current profile: ");
                              Serial.println(profileDesc[selectedProfile].profileLabel);
                          }
                          break;
                        case PauseMode_ProfileSelect:
                          if(!serialMode) {
                              Serial.println("Pick a profile!");
                              Serial.print("Current profile in use: ");
                              Serial.println(profileDesc[selectedProfile].profileLabel);
                          }
                          pauseModeSelectingProfile = true;
                          profileModeSelection = selectedProfile;
                          #ifdef LED_ENABLE
                              SetLedPackedColor(profileDesc[profileModeSelection].color);
                          #endif // LED_ENABLE
                          break;
                        case PauseMode_Save:
                          if(!serialMode) {
                              Serial.println("Saving...");
                          }
                          SavePreferences();
                          break;
                        #ifdef USES_RUMBLE
                        case PauseMode_RumbleToggle:
                          if(!serialMode) {
                              Serial.println("Toggling rumble!");
                          }
                          RumbleToggle();
                          break;
                        #endif // USES_RUMBLE
                        #ifdef USES_SOLENOID
                        case PauseMode_SolenoidToggle:
                          if(!serialMode) {
                              Serial.println("Toggling solenoid!");
                          }
                          SolenoidToggle();
                          break;
                        #endif // USES_SOLENOID
                        /*
                        #ifdef USES_SOLENOID
                        case PauseMode_BurstFireToggle:
                          Serial.println("Toggling solenoid burst firing!");
                          BurstFireToggle();
                          break;
                        #endif // USES_SOLENOID
                        */
                        case PauseMode_EscapeSignal:
                          SendEscapeKey();
                          #ifdef LED_ENABLE
                              for(byte i = 0; i < 3; i++) {
                                  LedUpdate(150,0,150);
                                  delay(55);
                                  LedOff();
                                  delay(40);
                              }
                          #endif // LED_ENABLE
                          break;
                        /*case PauseMode_Exit:
                          Serial.println("Exiting pause mode...");
                          if(runMode == RunMode_Processing) {
                              switch(profileData[selectedProfile].runMode) {
                                  case RunMode_Normal:
                                    SetRunMode(RunMode_Normal);
                                    break;
                                  case RunMode_Average:
                                    SetRunMode(RunMode_Average);
                                    break;
                                  case RunMode_Average2:
                                    SetRunMode(RunMode_Average2);
                                    break;
                                  default:
                                    break;
                              }
                          }
                          SetMode(GunMode_Run);
                          break;
                        */
                        default:
                          Serial.println("Oops, somethnig went wrong.");
                          break;
                    }
                } else if(buttons.pressedReleased & ExitPauseModeBtnMask) {
                    if(!serialMode) {
                        Serial.println("Exiting pause mode...");
                    }
                    SetMode(GunMode_Run);
                }
                if(pauseExitHoldStarted &&
                (buttons.debounced & ExitPauseModeHoldBtnMask)) {
                    unsigned long t = millis();
                    if(t - pauseHoldStartstamp > (SamcoPreferences::settings.pauseHoldLength / 2)) {
                        if(!serialMode) {
                            Serial.println("Exiting pause mode via hold...");
                        }
                        if(runMode == RunMode_Processing) {
                            switch(profileData[selectedProfile].runMode) {
                                case RunMode_Normal:
                                  SetRunMode(RunMode_Normal);
                                  break;
                                case RunMode_Average:
                                  SetRunMode(RunMode_Average);
                                  break;
                                case RunMode_Average2:
                                  SetRunMode(RunMode_Average2);
                                  break;
                                default:
                                  break;
                            }
                        }
                        #ifdef USES_RUMBLE
                            for(byte i = 0; i < 3; i++) {
                                analogWrite(SamcoPreferences::pins.oRumble, rumbleIntensity);
                                delay(80);
                                digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                                delay(50);
                            }
                        #endif // USES_RUMBLE
                        while(buttons.debounced != 0) {
                            //lol
                            buttons.Poll(1);
                        }
                        SetMode(GunMode_Run);
                        pauseExitHoldStarted = false;
                    }
                } else if(buttons.debounced & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = true;
                    pauseHoldStartstamp = millis();
                } else if(buttons.pressedReleased & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = false;
                }
            } else if(buttons.pressedReleased & ExitPauseModeBtnMask) {
                SetMode(GunMode_Run);
            } else if(buttons.pressedReleased == BtnMask_Trigger) {
                SetMode(GunMode_CalCenter);
            } else if(buttons.pressedReleased == RunModeNormalBtnMask) {
                SetRunMode(RunMode_Normal);
            } else if(buttons.pressedReleased == RunModeAverageBtnMask) {
                SetRunMode(runMode == RunMode_Average ? RunMode_Average2 : RunMode_Average);
            } else if(buttons.pressedReleased == IRSensitivityUpBtnMask) {
                IncreaseIrSensitivity();
            } else if(buttons.pressedReleased == IRSensitivityDownBtnMask) {
                DecreaseIrSensitivity();
            } else if(buttons.pressedReleased == SaveBtnMask) {
                SavePreferences();
            } else if(buttons.pressedReleased == OffscreenButtonToggleBtnMask) {
                OffscreenToggle();
            } else if(buttons.pressedReleased == AutofireSpeedToggleBtnMask) {
                AutofireSpeedToggle(0);
            #ifdef USES_RUMBLE
                } else if(buttons.pressedReleased == RumbleToggleBtnMask && SamcoPreferences::pins.sRumble >= 0) {
                    RumbleToggle();
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                } else if(buttons.pressedReleased == SolenoidToggleBtnMask && SamcoPreferences::pins.sSolenoid >= 0) {
                    SolenoidToggle();
            #endif // USES_SOLENOID
            } else {
                SelectCalProfileFromBtnMask(buttons.pressedReleased);
            }

            if(!serialMode && !dockedCalibrating) {
                PrintResults();
            }
            
            break;
        case GunMode_Docked:
            ExecGunModeDocked();
            break;
        case GunMode_CalCenter:
            AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
            if(buttons.pressedReleased & CancelCalBtnMask && !justBooted) {
                CancelCalibration();
            } else if(buttons.pressedReleased == SkipCalCenterBtnMask) {
                Serial.println("Calibrate Center skipped");
                SetMode(GunMode_CalVert);
            } else if(buttons.pressed & BtnMask_Trigger) {
                // trigger pressed, begin center cal 
                CalCenter();
                // extra delay to wait for trigger to release (though not required)
                SetModeWaitNoButtons(GunMode_CalVert, 500);
            }
            break;
        case GunMode_CalVert:
            if(buttons.pressedReleased & CancelCalBtnMask && !justBooted) {
                CancelCalibration();
            } else {
                if(buttons.pressed & BtnMask_Trigger) {
                    SetMode(GunMode_CalHoriz);
                } else {
                    CalVert();
                }
            }
            break;
        case GunMode_CalHoriz:
            if(buttons.pressedReleased & CancelCalBtnMask && !justBooted) {
                CancelCalibration();
            } else {
                if(buttons.pressed & BtnMask_Trigger) {
                    ApplyCalToProfile();
                    if(justBooted) {
                        // If this is an initial calibration, save it immediately!
                        SetMode(GunMode_Pause);
                        SavePreferences();
                        SetMode(GunMode_Run);
                    } else if(dockedCalibrating) {
                        Serial.print("UpdatedProf: ");
                        Serial.println(selectedProfile);
                        Serial.println(profileData[selectedProfile].xScale);
                        Serial.println(profileData[selectedProfile].yScale);
                        Serial.println(profileData[selectedProfile].xCenter);
                        Serial.println(profileData[selectedProfile].yCenter);
                        SetMode(GunMode_Docked);
                    } else {
                        SetMode(GunMode_Run);
                    }
                } else {
                    CalHoriz();
                }
            }
            break;
        default:
            /* ---------------------- LET'S GO --------------------------- */
            switch(runMode) {
            case RunMode_Processing:
                //ExecRunModeProcessing();
                //break;
            case RunMode_Average:
            case RunMode_Average2:
            case RunMode_Normal:
            default:
                ExecRunMode();
                break;
            }
            break;
    }

#ifdef DEBUG_SERIAL
    PrintDebugSerial();
#endif // DEBUG_SERIAL

}

/*        -----------------------------------------------        */
/* --------------------------- METHODS ------------------------- */
/*        -----------------------------------------------        */

// Main core loop
void ExecRunMode()
{
#ifdef DEBUG_SERIAL
    Serial.print("exec run mode ");
    Serial.println(RunModeLabels[runMode]);
#endif
    moveIndex = 0;
    buttons.ReportEnable();
    if(justBooted) {
        // center the joystick so RetroArch doesn't throw a hissy fit about uncentered joysticks
        delay(100);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
        justBooted = false;
    }
    for(;;) {
        // Setting the state of our toggles, if used.
        // Only sets these values if the switches are mapped to valid pins.
        #ifdef USES_SWITCHES
            #ifdef USES_RUMBLE
                if(SamcoPreferences::pins.sRumble >= 0) {
                    SamcoPreferences::toggles.rumbleActive = !digitalRead(SamcoPreferences::pins.sRumble);
                }
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                if(SamcoPreferences::pins.sSolenoid >= 0) {
                    SamcoPreferences::toggles.solenoidActive = !digitalRead(SamcoPreferences::pins.sSolenoid);
                }
            #endif // USES_SOLENOID
            if(SamcoPreferences::pins.sAutofire >= 0) {
                SamcoPreferences::toggles.autofireActive = !digitalRead(SamcoPreferences::pins.sAutofire);
            }
        #endif // USES_SWITCHES

        // If we're on RP2040, we offload the button polling to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)
        buttons.Poll(0);

        // The main gunMode loop: here it splits off to different paths,
        // depending on if we're in serial handoff (MAMEHOOK) or normal mode.
        #ifdef MAMEHOOKER
            if(Serial.available()) {                             // Have we received serial input? (This is cleared after we've read from it in full.)
                SerialProcessing();                                 // Run through the serial processing method (repeatedly, if there's leftover bits)
            }
            if(!serialMode) {  // Normal (gun-handled) mode
                // For processing the trigger specifically.
                // (buttons.debounced is a binary variable intended to be read 1 bit at a time,
                // with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFire();                                  // Handle button events and feedback ourselves.
                } else {   // Or we haven't pressed the trigger.
                    TriggerNotFire();                               // Releasing button inputs and sending stop signals to feedback devices.
                }
            } else {  // Serial handoff mode
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                            // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                         // Release button inputs.
                }
                SerialHandling();                                   // Process the force feedback from the current queue.
            }
        #else
            // For processing the trigger specifically.
            // (buttons.debounced is a binary variable intended to be read 1 bit at a time,
            // with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
            if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                      // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                   // Releasing button inputs and sending stop signals to feedback devices.
            }
        #endif // MAMEHOOKER
        #endif // DUAL_CORE

        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
            GetPosition();
            
            int halfHscale = (int)(mySamco.h() * xScale + 0.5f) / 2;
            moveXAxis = map(finalX, xCenter + halfHscale, xCenter - halfHscale, 0, MouseMaxX);
            halfHscale = (int)(mySamco.h() * yScale + 0.5f) / 2;
            moveYAxis = map(finalY, yCenter + halfHscale, yCenter - halfHscale, 0, MouseMaxY);

            switch(runMode) {
            case RunMode_Average:
                // 2 position moving average
                moveIndex ^= 1;
                moveXAxisArr[moveIndex] = moveXAxis;
                moveYAxisArr[moveIndex] = moveYAxis;
                moveXAxis = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                moveYAxis = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                break;
            case RunMode_Average2:
                // weighted average of current position and previous 2
                if(moveIndex < 2) {
                    ++moveIndex;
                } else {
                    moveIndex = 0;
                }
                moveXAxisArr[moveIndex] = moveXAxis;
                moveYAxisArr[moveIndex] = moveYAxis;
                moveXAxis = (moveXAxis + moveXAxisArr[0] + moveXAxisArr[1] + moveXAxisArr[1] + 2) / 4;
                moveYAxis = (moveYAxis + moveYAxisArr[0] + moveYAxisArr[1] + moveYAxisArr[1] + 2) / 4;
                break;
            case RunMode_Normal:
            default:
                break;
            }

            conMoveXAxis = constrain(moveXAxis, 0, MouseMaxX);
            if(conMoveXAxis == 0 || conMoveXAxis == MouseMaxX) {
                offXAxis = true;
            } else {
                offXAxis = false;
            }
            conMoveYAxis = constrain(moveYAxis, 0, MouseMaxY);  
            if(conMoveYAxis == 0 || conMoveYAxis == MouseMaxY) {
                offYAxis = true;
            } else {
                offYAxis = false;
            }

            if(buttons.analogOutput) {
                Gamepad16.moveCam(conMoveXAxis, conMoveYAxis);
            } else {
                AbsMouse5.move(conMoveXAxis, conMoveYAxis);
            }

            if(offXAxis || offYAxis) {
                buttons.offScreen = true;
            } else {
                buttons.offScreen = false;
            }

            #ifdef USES_ANALOG
                analogStickPolled = false;
                previousStickPoll = millis();
            #endif // USES_ANALOG
        }

        // If using RP2040, we offload the button processing to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)

        #ifdef USES_ANALOG
            if(analogIsValid) {
                // Poll the analog values 2ms after the IR sensor has updated so as not to overload the USB buffer.
                // stickPolled and previousStickPoll are reset/updated after irPosUpdateTick.
                if(!analogStickPolled) {
                    if(buttons.pressed || buttons.released) {
                        previousStickPoll = millis();
                    } else if(millis() - previousStickPoll > analogPollInterval) {
                        AnalogStickPoll();
                        analogStickPolled = true;
                    }
                }
            }
        #endif // USES_ANALOG

        if(buttons.pressedReleased == EscapeKeyBtnMask) {
            SendEscapeKey();
        }

        if(SamcoPreferences::toggles.holdToPause) {
            if((buttons.debounced == EnterPauseModeHoldBtnMask)
            && !lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                if(!serialMode) {
                    Serial.println("Started holding pause mode signal buttons!");
                }
            } else if(pauseHoldStarted && (buttons.debounced != EnterPauseModeHoldBtnMask || lastSeen)) {
                pauseHoldStarted = false;
                if(!serialMode) {
                    Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");
                }
            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > SamcoPreferences::settings.pauseHoldLength) {
                    // MAKE SURE EVERYTHING IS DISENGAGED:
                    #ifdef USES_SOLENOID
                        digitalWrite(solenoidPin, LOW);
                        solenoidFirstShot = false;
                    #endif // USES_SOLENOID
                    #ifdef USES_RUMBLE
                        digitalWrite(rumblePin, LOW);
                        rumbleHappening = false;
                        rumbleHappened = false;
                    #endif // USES_RUMBLE
                    Keyboard.releaseAll();
                    delay(1);
                    AbsMouse5.release(MOUSE_LEFT);
                    AbsMouse5.release(MOUSE_RIGHT);
                    offscreenBShot = false;
                    buttonPressed = false;
                    triggerHeld = false;
                    burstFiring = false;
                    burstFireCount = 0;
                    SetMode(GunMode_Pause);
                    buttons.ReportDisable();
                    return;
                }
            }
        } else {
            if(buttons.pressedReleased == EnterPauseModeBtnMask) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                Keyboard.releaseAll();
                delay(1);
                AbsMouse5.release(MOUSE_LEFT);
                AbsMouse5.release(MOUSE_RIGHT);
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                SetMode(GunMode_Pause);
                buttons.ReportDisable();
                return;
            } else if(buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                #ifdef USES_SOLENOID
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
                    rumbleHappening = false;
                    rumbleHappened = false;
                #endif // USES_RUMBLE
                Keyboard.releaseAll();
                delay(1);
                AbsMouse5.release(MOUSE_LEFT);
                AbsMouse5.release(MOUSE_RIGHT);
                offscreenBShot = false;
                buttonPressed = false;
                triggerHeld = false;
                burstFiring = false;
                burstFireCount = 0;
                SetMode(GunMode_Pause);
                buttons.ReportDisable();
                return;
            }
        }
        #else                                                       // if we're using dual cores,
        if(gunMode != GunMode_Run) {                                // We just check if the gunmode has been changed by the other thread.
            Keyboard.releaseAll();
            delay(1);
            AbsMouse5.release(MOUSE_LEFT);
            AbsMouse5.release(MOUSE_RIGHT);
            return;
        }
        #endif // ARDUINO_ARCH_RP2040 || DUAL_CORE

#ifdef DEBUG_SERIAL
        ++frameCount;
        PrintDebugSerial();
#endif // DEBUG_SERIAL
    }
}

// from Samco_4IR_Test_BETA sketch
// for use with the Samco_4IR_Processing_Sketch_BETA Processing sketch
void ExecRunModeProcessing()
{
    // constant offset added to output values
    const int processingOffset = 100;

    buttons.ReportDisable();
    for(;;) {
        buttons.Poll(1);
        if(Serial.available()) {
            SerialProcessingDocked();
        }
        if(runMode != RunMode_Processing) {
            return;
        }

        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
        
            int error = dfrIRPos->basicAtomic(DFRobotIRPositionEx::Retry_2);
            if(error == DFRobotIRPositionEx::Error_Success) {
                mySamco.begin(dfrIRPos->xPositions(), dfrIRPos->yPositions(), dfrIRPos->seen(), MouseMaxX / 2, MouseMaxY / 2);
                UpdateLastSeen();
                if(millis() - testLastStamp > testPrintInterval) {
                    testLastStamp = millis();
                    for(int i = 0; i < 4; i++) {
                        Serial.print(map(mySamco.testX(i), 0, MouseMaxX, CamMaxX, 0) + processingOffset);
                        Serial.print(",");
                        Serial.print(map(mySamco.testY(i), 0, MouseMaxY, CamMaxY, 0) + processingOffset);
                        Serial.print(",");
                    }
                    Serial.print(map(mySamco.x(), 0, MouseMaxX, CamMaxX, 0) + processingOffset);
                    Serial.print(",");
                    Serial.print(map(mySamco.y(), 0, MouseMaxY, CamMaxY, 0) + processingOffset);
                    Serial.print(",");
                    Serial.print(map(mySamco.testMedianX(), 0, MouseMaxX, CamMaxX, 0) + processingOffset);
                    Serial.print(",");
                    Serial.println(map(mySamco.testMedianY(), 0, MouseMaxY, CamMaxY, 0) + processingOffset);
                }
            } else if(error == DFRobotIRPositionEx::Error_IICerror) {
                Serial.println("Device not available!");
            }
        }
    }
}

// For use with GUN4ALL-GUI when app connects to this board.
void ExecGunModeDocked()
{
    buttons.ReportDisable();
    if(justBooted) {
        // center the joystick so RetroArch/Windows doesn't throw a hissy fit about uncentered joysticks
        delay(250);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
    }
    #ifdef LED_ENABLE
        LedUpdate(127, 127, 255);
    #endif // LED_ENABLE
    unsigned long tempChecked = millis();
    for(;;) {
        buttons.Poll(1);

        if(Serial.available()) {
            SerialProcessingDocked();
        }

        if(dockedCalibrating) {
            if(irPosUpdateTick) {
                irPosUpdateTick = 0;
                GetPosition();
                
                int halfHscale = (int)(mySamco.h() * xScale + 0.5f) / 2;
                moveXAxis = map(finalX, xCenter + halfHscale, xCenter - halfHscale, 0, MouseMaxX);
                halfHscale = (int)(mySamco.h() * yScale + 0.5f) / 2;
                moveYAxis = map(finalY, yCenter + halfHscale, yCenter - halfHscale, 0, MouseMaxY);

                switch(runMode) {
                case RunMode_Average:
                    // 2 position moving average
                    moveIndex ^= 1;
                    moveXAxisArr[moveIndex] = moveXAxis;
                    moveYAxisArr[moveIndex] = moveYAxis;
                    moveXAxis = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                    moveYAxis = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                    break;
                case RunMode_Average2:
                    // weighted average of current position and previous 2
                    if(moveIndex < 2) {
                        ++moveIndex;
                    } else {
                        moveIndex = 0;
                    }
                    moveXAxisArr[moveIndex] = moveXAxis;
                    moveYAxisArr[moveIndex] = moveYAxis;
                    moveXAxis = (moveXAxis + moveXAxisArr[0] + moveXAxisArr[1] + moveXAxisArr[1] + 2) / 4;
                    moveYAxis = (moveYAxis + moveYAxisArr[0] + moveYAxisArr[1] + moveYAxisArr[1] + 2) / 4;
                    break;
                case RunMode_Normal:
                default:
                    break;
                }

                conMoveXAxis = constrain(moveXAxis, 0, MouseMaxX);
                conMoveYAxis = constrain(moveYAxis, 0, MouseMaxY);  

                AbsMouse5.move(conMoveXAxis, conMoveYAxis);
            }
            if(buttons.pressed == BtnMask_Trigger) {
                dockedCalibrating = false;
            }
        } else if(!dockedSaving) {
            switch(buttons.pressed) {
                case BtnMask_Trigger:
                  Serial.println("Pressed: 1");
                  break;
                case BtnMask_A:
                  Serial.println("Pressed: 2");
                  break;
                case BtnMask_B:
                  Serial.println("Pressed: 3");
                  break;
                case BtnMask_Reload:
                  Serial.println("Pressed: 4");
                  break;
                case BtnMask_Start:
                  Serial.println("Pressed: 5");
                  break;
                case BtnMask_Select:
                  Serial.println("Pressed: 6");
                  break;
                case BtnMask_Up:
                  Serial.println("Pressed: 7");
                  break;
                case BtnMask_Down:
                  Serial.println("Pressed: 8");
                  break;
                case BtnMask_Left:
                  Serial.println("Pressed: 9");
                  break;
                case BtnMask_Right:
                  Serial.println("Pressed: 10");
                  break;
                case BtnMask_Pedal:
                  Serial.println("Pressed: 11");
                  break;
                case BtnMask_Home:
                  Serial.println("Pressed: 12");
                  break;
                case BtnMask_Pump:
                  Serial.println("Pressed: 13");
                  break;
            }

            switch(buttons.released) {
                case BtnMask_Trigger:
                  Serial.println("Released: 1");
                  break;
                case BtnMask_A:
                  Serial.println("Released: 2");
                  break;
                case BtnMask_B:
                  Serial.println("Released: 3");
                  break;
                case BtnMask_Reload:
                  Serial.println("Released: 4");
                  break;
                case BtnMask_Start:
                  Serial.println("Released: 5");
                  break;
                case BtnMask_Select:
                  Serial.println("Released: 6");
                  break;
                case BtnMask_Up:
                  Serial.println("Released: 7");
                  break;
                case BtnMask_Down:
                  Serial.println("Released: 8");
                  break;
                case BtnMask_Left:
                  Serial.println("Released: 9");
                  break;
                case BtnMask_Right:
                  Serial.println("Released: 10");
                  break;
                case BtnMask_Pedal:
                  Serial.println("Released: 11");
                  break;
                case BtnMask_Home:
                  Serial.println("Released: 12");
                  break;
                case BtnMask_Pump:
                  Serial.println("Released: 13");
                  break;
            }
            unsigned long currentMillis = millis();
            if(currentMillis - tempChecked >= 1000) {
                if(SamcoPreferences::pins.aTMP36 >= 0) {
                    int tempSensor = analogRead(SamcoPreferences::pins.aTMP36);
                    tempSensor = (((tempSensor * 3.3) / 4096) - 0.5) * 100;
                    Serial.print("Temperature: ");
                    Serial.println(tempSensor);
                }
                tempChecked = currentMillis;
            }
        }

        if(gunMode != GunMode_Docked) {
            return;
        }
        if(runMode == RunMode_Processing) {
            ExecRunModeProcessing();
        }
    }
}

// center calibration with a bit of averaging
void CalCenter()
{
    unsigned int xAcc = 0;
    unsigned int yAcc = 0;
    unsigned int count = 0;
    unsigned long ms = millis();
    
    // accumulate center position over a bit of time for some averaging
    while(millis() - ms < 333) {
        // center pointer
        AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
        
        // get position
        if(GetPositionIfReady()) {
            xAcc += finalX;
            yAcc += finalY;
            count++;
            
            xCenter = finalX;
            yCenter = finalY;
            PrintCalInterval();
        }

        // poll buttons
        buttons.Poll(1);
        
        // if trigger not pressed then break out of loop early
        if(!(buttons.debounced & BtnMask_Trigger)) {
            break;
        }
    }

    // unexpected, but make sure x and y positions are accumulated
    if(count) {
        xCenter = xAcc / count;
        yCenter = yAcc / count;
    } else {
        Serial.print("Unexpected Center calibration failure, no center position was acquired!");
        // just continue anyway
    }

    PrintCalInterval();
}

// vertical calibration 
void CalVert()
{
    if(GetPositionIfReady()) {
        int halfH = (int)(mySamco.h() * yScale + 0.5f) / 2;
        moveYAxis = map(finalY, yCenter + halfH, yCenter - halfH, 0, MouseMaxY);
        conMoveXAxis = MouseMaxX / 2;
        conMoveYAxis = constrain(moveYAxis, 0, MouseMaxY);
        AbsMouse5.move(conMoveXAxis, conMoveYAxis);
        if(conMoveYAxis == 0 || conMoveYAxis == MouseMaxY) {
            buttons.offScreen = true;
        } else {
            buttons.offScreen = false;
        }
    }
    
    if((buttons.repeat & BtnMask_B) || (buttons.offScreen && (buttons.repeat & BtnMask_A))) {
        yScale = yScale + ScaleStep;
    } else if(buttons.repeat & BtnMask_A) {
        if(yScale > 0.005f) {
            yScale = yScale - ScaleStep;
        }
    }

    if(buttons.pressedReleased == BtnMask_Up) {
        yCenter--;
    } else if(buttons.pressedReleased == BtnMask_Down) {
        yCenter++;
    }
    
    PrintCalInterval();
}

// horizontal calibration 
void CalHoriz()
{
    if(GetPositionIfReady()) {    
        int halfH = (int)(mySamco.h() * xScale + 0.5f) / 2;
        moveXAxis = map(finalX, xCenter + halfH, xCenter - halfH, 0, MouseMaxX);
        conMoveXAxis = constrain(moveXAxis, 0, MouseMaxX);
        conMoveYAxis = MouseMaxY / 2;
        AbsMouse5.move(conMoveXAxis, conMoveYAxis);
        if(conMoveXAxis == 0 || conMoveXAxis == MouseMaxX) {
            buttons.offScreen = true;
        } else {
            buttons.offScreen = false;
        }
    }

    if((buttons.repeat & BtnMask_B) || (buttons.offScreen && (buttons.repeat & BtnMask_A))) {
        xScale = xScale + ScaleStep;
    } else if(buttons.repeat & BtnMask_A) {
        if(xScale > 0.005f) {
            xScale = xScale - ScaleStep;
        }
    }
    
    if(buttons.pressedReleased == BtnMask_Left) {
        xCenter--;
    } else if(buttons.pressedReleased == BtnMask_Right) {
        xCenter++;
    }

    PrintCalInterval();
}

// Helper to get position if the update tick is set
bool GetPositionIfReady()
{
    if(irPosUpdateTick) {
        irPosUpdateTick = 0;
        GetPosition();
        return true;
    }
    return false;
}

// Get tilt adjusted position from IR postioning camera
// Updates finalX and finalY values
void GetPosition()
{
    int error = dfrIRPos->basicAtomic(DFRobotIRPositionEx::Retry_2);
    if(error == DFRobotIRPositionEx::Error_Success) {
        mySamco.begin(dfrIRPos->xPositions(), dfrIRPos->yPositions(), dfrIRPos->seen(), xCenter, yCenter);
        finalX = mySamco.x();
        finalY = mySamco.y();

        UpdateLastSeen();
     
#if DEBUG_SERIAL == 2
        Serial.print(finalX);
        Serial.print(' ');
        Serial.print(finalY);
        Serial.print("   ");
        Serial.println(mySamco.h());
#endif
    } else if(error != DFRobotIRPositionEx::Error_DataMismatch) {
        Serial.println("Device not available!");
    }
}

// wait up to given amount of time for no buttons to be pressed before setting the mode
void SetModeWaitNoButtons(GunMode_e newMode, unsigned long maxWait)
{
    unsigned long ms = millis();
    while(buttons.debounced && (millis() - ms < maxWait)) {
        buttons.Poll(1);
    }
    SetMode(newMode);
}

// update the last seen value
// only to be called during run mode since this will modify the LED colour
void UpdateLastSeen()
{
    if(lastSeen != mySamco.seen()) {
        #ifdef MAMEHOOKER
        if(!serialMode) {
        #endif // MAMEHOOKER
            #ifdef LED_ENABLE
            if(!lastSeen && mySamco.seen()) {
                LedOff();
            } else if(lastSeen && !mySamco.seen()) {
                SetLedPackedColor(IRSeen0Color);
            }
            #endif // LED_ENABLE
        #ifdef MAMEHOOKER
        }
        #endif // MAMEHOOKER
        lastSeen = mySamco.seen();
    }
}

// Handles events when trigger is pulled/held
void TriggerFire()
{
    if(!buttons.offScreen &&                                     // Check if the X or Y axis is in the screen's boundaries, i.e. "off screen".
    !offscreenBShot) {                                           // And only as long as we haven't fired an off-screen shot,
        if(!buttonPressed) {
            if(buttons.analogOutput) {
                Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3); // No reason to handle this ourselves here, but eh.
            } else {
                AbsMouse5.press(MOUSE_LEFT);                     // We're handling the trigger button press ourselves for a reason.
            }
            buttonPressed = true;                                // Set this so we won't spam a repeat press event again.
        }
        if(!bitRead(buttons.debounced, 3) &&                     // Is the trigger being pulled WITHOUT pressing Start & Select?
        !bitRead(buttons.debounced, 4)) {
            #ifdef USES_SOLENOID
                if(SamcoPreferences::toggles.solenoidActive) {                             // (Only activate when the solenoid switch is on!)
                    if(!triggerHeld) {  // If this is the first time we're firing,
                        if(burstFireActive && !burstFiring) {  // Are we in burst firing mode?
                            solenoidFirstShot = true;               // Set this so we use the instant solenoid fire path,
                            SolenoidActivation(0);                  // Engage it,
                            solenoidFirstShot = false;              // And disable the flag to mitigate confusion.
                            burstFiring = true;                     // Set that we're in a burst fire event.
                            burstFireCount = 1;                     // Set this as the first shot in a burst fire sequence,
                            burstFireCountLast = 1;                 // And reset the stored counter,
                        } else if(!burstFireActive) {  // Or, if we're in normal or rapid fire mode,
                            solenoidFirstShot = true;               // Set the First Shot flag on.
                            SolenoidActivation(0);                  // Just activate the Solenoid already!
                            if(SamcoPreferences::toggles.autofireActive) {          // If we are in auto mode,
                                solenoidFirstShot = false;          // Immediately set this bit off!
                            }
                        }
                    // Else, these below are all if we've been holding the trigger.
                    } else if(burstFiring) {  // If we're in a burst firing sequence,
                        BurstFire();                                // Process it.
                    } else if(SamcoPreferences::toggles.autofireActive &&  // Else, if we've been holding the trigger, is the autofire switch active?
                    !burstFireActive) {          // (WITHOUT burst firing enabled)
                        if(digitalRead(SamcoPreferences::pins.oSolenoid)) {              // Is the solenoid engaged?
                            SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval); // If so, immediately pass the autofire faster interval to solenoid method
                        } else {                                    // Or if it's not,
                            SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval * SamcoPreferences::settings.autofireWaitFactor); // We're holding it for longer.
                        }
                    } else if(solenoidFirstShot) {                  // If we aren't in autofire mode, are we waiting for the initial shot timer still?
                        if(digitalRead(SamcoPreferences::pins.oSolenoid)) {              // If so, are we still engaged? We need to let it go normally, but maintain the single shot flag.
                            unsigned long currentMillis = millis(); // Initialize timer to check if we've passed it.
                            if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidNormalInterval) { // If we finally surpassed the wait threshold...
                                digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);     // Let it go.
                            }
                        } else {                                    // We're waiting on the extended wait before repeating in single shot mode.
                            unsigned long currentMillis = millis(); // Initialize timer.
                            if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidLongInterval) { // If we finally surpassed the LONGER wait threshold...
                                solenoidFirstShot = false;          // We're gonna turn this off so we don't have to pass through this check anymore.
                                SolenoidActivation(SamcoPreferences::settings.solenoidNormalInterval); // Process it now.
                            }
                        }
                    } else if(!burstFireActive) {                   // if we don't have the single shot wait flag on (holding the trigger w/out autofire)
                        if(digitalRead(SamcoPreferences::pins.oSolenoid)) {              // Are we engaged right now?
                            SolenoidActivation(SamcoPreferences::settings.solenoidNormalInterval); // Turn it off with this timer.
                        } else {                                    // Or we're not engaged.
                            SolenoidActivation(SamcoPreferences::settings.solenoidNormalInterval * 2); // So hold it that way for twice the normal timer.
                        }
                    }
                } // We ain't using the solenoid, so just ignore all that.
                #elif RUMBLE_FF
                    if(SamcoPreferences::toggles.rumbleActive) {
                        // TODO: actually make stuff here.
                    } // We ain't using the motor, so just ignore all that.
                #endif // USES_SOLENOID/RUMBLE_FF
            }
            #ifdef USES_RUMBLE
                #ifndef RUMBLE_FF
                    if(SamcoPreferences::toggles.rumbleActive &&                     // Is rumble activated,
                        rumbleHappening && triggerHeld) {  // AND we're in a rumbling command WHILE the trigger's held?
                        RumbleActivation();                    // Continue processing the rumble command, to prevent infinite rumble while going from on-screen to off mid-command.
                    }
                #endif // RUMBLE_FF
            #endif // USES_RUMBLE
        } else {  // We're shooting outside of the screen boundaries!
            #ifdef PRINT_VERBOSE
                Serial.println("Shooting outside of the screen! RELOAD!");
            #endif
            if(!buttonPressed) {  // If we haven't pressed a trigger key yet,
                if(!triggerHeld && offscreenButton) {  // If we are in offscreen button mode (and aren't dragging a shot offscreen)
                    if(buttons.analogOutput) {
                        Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
                    } else {
                        AbsMouse5.press(MOUSE_RIGHT);
                    }
                    offscreenBShot = true;                     // Mark we pressed the right button via offscreen shot mode,
                    buttonPressed = true;                      // Mark so we're not spamming these press events.
                } else {  // Or if we're not in offscreen button mode,
                    if(buttons.analogOutput) {
                        Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
                    } else {
                        AbsMouse5.press(MOUSE_LEFT);
                    }
                    buttonPressed = true;                      // Mark so we're not spamming these press events.
                }
            }
            #ifdef USES_RUMBLE
                #ifndef RUMBLE_FF
                    if(SamcoPreferences::toggles.rumbleActive) {  // Only activate if the rumble switch is enabled!
                        if(!rumbleHappened && !triggerHeld) {  // Is this the first time we're rumbling AND only started pulling the trigger (to prevent starting a rumble w/ trigger hold)?
                            RumbleActivation();                        // Start a rumble command.
                        } else if(rumbleHappening) {  // We are currently processing a rumble command.
                            RumbleActivation();                        // Keep processing that command then.
                        }  // Else, we rumbled already, so don't do anything to prevent infinite rumbling.
                    }
                #endif // RUMBLE_FF
            #endif // USES_RUMBLE
            if(burstFiring) {                                  // If we're in a burst firing sequence,
                BurstFire();
            #ifdef USES_SOLENOID
                } else if(digitalRead(SamcoPreferences::pins.oSolenoid) &&
                !burstFireActive) {                               // If the solenoid is engaged, since we're not shooting the screen, shut off the solenoid a'la an idle cycle
                    unsigned long currentMillis = millis();         // Calibrate current time
                    if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                        previousMillisSol = currentMillis;          // Timer calibration, yawn.
                        digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);             // Turn it off already, dangit.
                    }
            #elif RUMBLE_FF
                } else if(rumbleHappening && !burstFireActive) {
                    // TODO: actually implement
            #endif // USES_SOLENOID
        }
    }
    triggerHeld = true;                                     // Signal that we've started pulling the trigger this poll cycle.
}

// Handles events when trigger is released
void TriggerNotFire()
{
    triggerHeld = false;                                    // Disable the holding function
    if(buttonPressed) {
        if(offscreenBShot) {                                // If we fired off screen with the offscreenButton set,
            if(buttons.analogOutput) {
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
            } else {
                AbsMouse5.release(MOUSE_RIGHT);             // We were pressing the right mouse, so release that.
            }
            offscreenBShot = false;
            buttonPressed = false;
        } else {                                            // Or if not,
            if(buttons.analogOutput) {
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
            } else {
                AbsMouse5.release(MOUSE_LEFT);              // We were pressing the left mouse, so release that instead.
            }
            buttonPressed = false;
        }
    }
    #ifdef USES_SOLENOID
        if(SamcoPreferences::toggles.solenoidActive) {  // Has the solenoid remain engaged this cycle?
            if(burstFiring) {    // Are we in a burst fire command?
                BurstFire();                                    // Continue processing it.
            } else if(!burstFireActive) { // Else, we're just processing a normal/rapid fire shot.
                solenoidFirstShot = false;                      // Make sure this is unset to prevent "sticking" in single shot mode!
                unsigned long currentMillis = millis();         // Start the timer
                if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                    previousMillisSol = currentMillis;          // Timer calibration, yawn.
                    digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);             // Make sure to turn it off.
                }
            }
        }
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        if(rumbleHappening) {                                   // Are we currently in a rumble command? (Implicitly needs SamcoPreferences::toggles.rumbleActive)
            RumbleActivation();                                 // Continue processing our rumble command.
            // (This is to prevent making the lack of trigger pull actually activate a rumble command instead of skipping it like we should.)
        } else if(rumbleHappened) {                             // If rumble has happened,
            rumbleHappened = false;                             // well we're clear now that we've stopped holding.
        }
    #endif // USES_RUMBLE
}

#ifdef USES_ANALOG
void AnalogStickPoll()
{
    unsigned int analogValueX = analogRead(SamcoPreferences::pins.aStickX);
    unsigned int analogValueY = analogRead(SamcoPreferences::pins.aStickY);
    Gamepad16.moveStick(analogValueX, analogValueY);
}
#endif // USES_ANALOG

// Limited subset of SerialProcessing specifically for "docked" mode
// contains setting submethods for use by GUN4ALL-GUI
void SerialProcessingDocked()
{
    char serialInput = Serial.read();
    char serialInputS[3] = {0, 0, 0};

    switch(serialInput) {
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Set IR Brightness
              case 'B':
                serialInput = Serial.read();
                if(serialInput == '0' || serialInput == '1' || serialInput == '2') {
                    if(gunMode != GunMode_Pause || gunMode != GunMode_Docked) {
                        Serial.println("Can't set sensitivity in run mode! Please enter pause mode if you'd like to change IR sensitivity.");
                    } else {
                        byte brightnessLvl = serialInput - '0';
                        SetIrSensitivity(brightnessLvl);
                    }
                } else {
                    Serial.println("SERIALREAD: No valid IR sensitivity level set! (Expected 0 to 2)");
                }
                break;
              // Toggle Test/Processing Mode
              case 'T':
                if(runMode == RunMode_Processing) {
                    Serial.println("Exiting processing mode...");
                    switch(profileData[selectedProfile].runMode) {
                        case RunMode_Normal:
                          SetRunMode(RunMode_Normal);
                          break;
                        case RunMode_Average:
                          SetRunMode(RunMode_Average);
                          break;
                        case RunMode_Average2:
                          SetRunMode(RunMode_Average2);
                          break;
                    }
                } else {
                    Serial.println("Entering Test Mode...");
                    SetRunMode(RunMode_Processing);
                }
                break;
              // Enter Docked Mode
              case 'P':
                SetMode(GunMode_Docked);
                break;
              // Exit Docked Mode
              case 'E':
                if(!justBooted) {
                    SetMode(GunMode_Run);
                } else {
                    SetMode(GunMode_Init);
                }
                switch(profileData[selectedProfile].runMode) {
                    case RunMode_Normal:
                      SetRunMode(RunMode_Normal);
                      break;
                    case RunMode_Average:
                      SetRunMode(RunMode_Average);
                      break;
                    case RunMode_Average2:
                      SetRunMode(RunMode_Average2);
                      break;
                }
                break;
              // Enter Calibration mode (optional: switch to cal profile if detected)
              case 'C':
                serialInput = Serial.read();
                if(serialInput == '1' || serialInput == '2' ||
                   serialInput == '3' || serialInput == '4') {
                    // Converting char to its real respective number
                    byte profileNum = serialInput - '0';
                    SelectCalProfile(profileNum-1);
                    Serial.print("Profile: ");
                    Serial.println(profileNum-1);
                    serialInput = Serial.read();
                    if(serialInput == 'C') {
                        dockedCalibrating = true;
                        //Serial.print("Now calibrating selected profile: ");
                        //Serial.println(profileDesc[selectedProfile].profileLabel);
                        SetMode(GunMode_CalCenter);
                    }
                }
                break;
              // Save current profile
              case 'S':
                Serial.println("Saving preferences...");
                // Update bindings so LED/Pixel changes are reflected immediately
                FeedbackSet();
                // dockedSaving flag is set by Xm, since that's required anyways for this to make any sense.
                SavePreferences();
                // load everything back to commit custom pins setting to memory
                if(nvPrefsError == SamcoPreferences::Error_Success) {
                    #ifdef LED_ENABLE
                    // Save op above resets color, so re-set it back to docked idle color
                    LedUpdate(127, 127, 255);
                    #endif // LED_ENABLE
                }
                UpdateBindings(SamcoPreferences::toggles.lowButtonMode);
                buttons.Begin();
                dockedSaving = false;
                break;
              // Clear EEPROM.
              case 'c':
                //Serial.println(EEPROM.length());
                dockedSaving = true;
                SamcoPreferences::ResetPreferences();
                Serial.println("Cleared! Please reset the board.");
                dockedSaving = false;
                break;
              // Mapping new values to commit to EEPROM.
              case 'm':
              {
                if(!dockedSaving) {
                    buttons.Unset();
                    PinsReset();
                    dockedSaving = true; // mark so button presses won't interrupt this process.
                } else {
                    serialInput = Serial.read(); // nomf
                    serialInput = Serial.read();
                    // bool change
                    if(serialInput == '0') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          case '0':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.customPinsInUse = serialInput - '0';
                            SamcoPreferences::toggles.customPinsInUse = constrain(SamcoPreferences::toggles.customPinsInUse, 0, 1);
                            Serial.println("OK: Toggled Custom Pin setting.");
                            break;
                          #ifdef USES_RUMBLE
                          case '1':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.rumbleActive = serialInput - '0';
                            SamcoPreferences::toggles.rumbleActive = constrain(SamcoPreferences::toggles.rumbleActive, 0, 1);
                            Serial.println("OK: Toggled Rumble setting.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case '2':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.solenoidActive = serialInput - '0';
                            SamcoPreferences::toggles.solenoidActive = constrain(SamcoPreferences::toggles.solenoidActive, 0, 1);
                            Serial.println("OK: Toggled Solenoid setting.");
                            break;
                          #endif
                          case '3':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.autofireActive = serialInput - '0';
                            SamcoPreferences::toggles.autofireActive = constrain(SamcoPreferences::toggles.autofireActive, 0, 1);
                            Serial.println("OK: Toggled Autofire setting.");
                            break;
                          case '4':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.simpleMenu = serialInput - '0';
                            SamcoPreferences::toggles.simpleMenu = constrain(SamcoPreferences::toggles.simpleMenu, 0, 1);
                            Serial.println("OK: Toggled Simple Pause Menu setting.");
                            break;
                          case '5':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.holdToPause = serialInput - '0';
                            SamcoPreferences::toggles.holdToPause = constrain(SamcoPreferences::toggles.holdToPause, 0, 1);
                            Serial.println("OK: Toggled Hold to Pause setting.");
                            break;
                          #ifdef FOURPIN_LED
                          case '6':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.commonAnode = serialInput - '0';
                            SamcoPreferences::toggles.commonAnode = constrain(SamcoPreferences::toggles.commonAnode, 0, 1);
                            Serial.println("OK: Toggled Common Anode setting.");
                            break;
                          #endif
                          case '7':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.lowButtonMode = serialInput - '0';
                            SamcoPreferences::toggles.lowButtonMode = constrain(SamcoPreferences::toggles.lowButtonMode, 0, 1);
                            Serial.println("OK: Toggled Low Button Mode setting.");
                            break;
                          case '8':
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            SamcoPreferences::toggles.rumbleFF = serialInput - '0';
                            SamcoPreferences::toggles.rumbleFF = constrain(SamcoPreferences::toggles.rumbleFF, 0, 1);
                            Serial.println("OK: Toggled Rumble FF setting.");
                            break;
                          default:
                            while(!Serial.available()) {
                              serialInput = Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    // Pins
                    } else if(serialInput == '1') {
                        serialInput = Serial.read(); // nomf
                        byte sCase = Serial.parseInt();
                        switch(sCase) {
                          case 0:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bTrigger = Serial.parseInt();
                            SamcoPreferences::pins.bTrigger = constrain(SamcoPreferences::pins.bTrigger, -1, 40);
                            Serial.println("OK: Set trigger button pin.");
                            break;
                          case 1:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunA = Serial.parseInt();
                            SamcoPreferences::pins.bGunA = constrain(SamcoPreferences::pins.bGunA, -1, 40);
                            Serial.println("OK: Set A button pin.");
                            break;
                          case 2:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunB = Serial.parseInt();
                            SamcoPreferences::pins.bGunB = constrain(SamcoPreferences::pins.bGunB, -1, 40);
                            Serial.println("OK: Set B button pin.");
                            break;
                          case 3:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunC = Serial.parseInt();
                            SamcoPreferences::pins.bGunC = constrain(SamcoPreferences::pins.bGunC, -1, 40);
                            Serial.println("OK: Set C button pin.");
                            break;
                          case 4:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bStart = Serial.parseInt();
                            SamcoPreferences::pins.bStart = constrain(SamcoPreferences::pins.bStart, -1, 40);
                            Serial.println("OK: Set Start button pin.");
                            break;
                          case 5:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bSelect = Serial.parseInt();
                            SamcoPreferences::pins.bSelect = constrain(SamcoPreferences::pins.bSelect, -1, 40);
                            Serial.println("OK: Set Select button pin.");
                            break;
                          case 6:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunUp = Serial.parseInt();
                            SamcoPreferences::pins.bGunUp = constrain(SamcoPreferences::pins.bGunUp, -1, 40);
                            Serial.println("OK: Set D-Pad Up button pin.");
                            break;
                          case 7:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunDown = Serial.parseInt();
                            SamcoPreferences::pins.bGunDown = constrain(SamcoPreferences::pins.bGunDown, -1, 40);
                            Serial.println("OK: Set D-Pad Down button pin.");
                            break;
                          case 8:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunLeft = Serial.parseInt();
                            SamcoPreferences::pins.bGunLeft = constrain(SamcoPreferences::pins.bGunLeft, -1, 40);
                            Serial.println("OK: Set D-Pad Left button pin.");
                            break;
                          case 9:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bGunRight = Serial.parseInt();
                            SamcoPreferences::pins.bGunRight = constrain(SamcoPreferences::pins.bGunRight, -1, 40);
                            Serial.println("OK: Set D-Pad Right button pin.");
                            break;
                          case 10:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bPedal = Serial.parseInt();
                            SamcoPreferences::pins.bPedal = constrain(SamcoPreferences::pins.bPedal, -1, 40);
                            Serial.println("OK: Set External Pedal button pin.");
                            break;
                          case 11:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bHome = Serial.parseInt();
                            SamcoPreferences::pins.bHome = constrain(SamcoPreferences::pins.bHome, -1, 40);
                            Serial.println("OK: Set Home button pin.");
                            break;
                          case 12:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.bPump = Serial.parseInt();
                            SamcoPreferences::pins.bPump = constrain(SamcoPreferences::pins.bPump, -1, 40);
                            Serial.println("OK: Set Pump Action button pin.");
                            break;
                          #ifdef USES_RUMBLE
                          case 13:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.oRumble = Serial.parseInt();
                            SamcoPreferences::pins.oRumble = constrain(SamcoPreferences::pins.oRumble, -1, 40);
                            Serial.println("OK: Set Rumble signal pin.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case 14:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.oSolenoid = Serial.parseInt();
                            SamcoPreferences::pins.oSolenoid = constrain(SamcoPreferences::pins.oSolenoid, -1, 40);
                            Serial.println("OK: Set Solenoid signal pin.");
                            break;
                          #endif
                          #ifdef USES_SWITCHES
                          #ifdef USES_RUMBLE
                          case 15:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.sRumble = Serial.parseInt();
                            SamcoPreferences::pins.sRumble = constrain(SamcoPreferences::pins.sRumble, -1, 40);
                            Serial.println("OK: Set Rumble Switch pin.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case 16:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.sSolenoid = Serial.parseInt();
                            SamcoPreferences::pins.sSolenoid = constrain(SamcoPreferences::pins.sSolenoid, -1, 40);
                            Serial.println("OK: Set Solenoid Switch pin.");
                            break;
                          #endif
                          case 17:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.sAutofire = Serial.parseInt();
                            SamcoPreferences::pins.sAutofire = constrain(SamcoPreferences::pins.sAutofire, -1, 40);
                            Serial.println("OK: Set Autofire Switch pin.");
                            break;
                          #endif
                          #ifdef FOURPIN_LED
                          case 18:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.oLedR = Serial.parseInt();
                            SamcoPreferences::pins.oLedR = constrain(SamcoPreferences::pins.oLedR, -1, 40);
                            Serial.println("OK: Set RGB LED R pin.");
                            break;
                          case 19:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.oLedG = Serial.parseInt();
                            SamcoPreferences::pins.oLedG = constrain(SamcoPreferences::pins.oLedG, -1, 40);
                            Serial.println("OK: Set RGB LED G pin.");
                            break;
                          case 20:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.oLedB = Serial.parseInt();
                            SamcoPreferences::pins.oLedB = constrain(SamcoPreferences::pins.oLedB, -1, 40);
                            Serial.println("OK: Set RGB LED B pin.");
                            break;
                          #endif
                          #ifdef CUSTOM_NEOPIXEL
                          case 21:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.oPixel = Serial.parseInt();
                            SamcoPreferences::pins.oPixel = constrain(SamcoPreferences::pins.oPixel, -1, 40);
                            Serial.println("OK: Set Custom NeoPixel pin.");
                            break;
                          #endif
                          case 22:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.pCamSDA = Serial.parseInt();
                            SamcoPreferences::pins.pCamSDA = constrain(SamcoPreferences::pins.pCamSDA, -1, 40);
                            Serial.println("OK: Set Camera SDA pin.");
                            break;
                          case 23:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.pCamSCL = Serial.parseInt();
                            SamcoPreferences::pins.pCamSCL = constrain(SamcoPreferences::pins.pCamSCL, -1, 40);
                            Serial.println("OK: Set Camera SCL pin.");
                            break;
                          case 24:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.pPeriphSDA = Serial.parseInt();
                            SamcoPreferences::pins.pPeriphSDA = constrain(SamcoPreferences::pins.pPeriphSDA, -1, 40);
                            Serial.println("OK: Set Peripherals SDA pin.");
                            break;
                          case 25:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.pPeriphSCL = Serial.parseInt();
                            SamcoPreferences::pins.pPeriphSCL = constrain(SamcoPreferences::pins.pPeriphSCL, -1, 40);
                            Serial.println("OK: Set Peripherals SCL pin.");
                            break;
                          #ifdef USES_ANALOG
                          case 26:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.aStickX = Serial.parseInt();
                            SamcoPreferences::pins.aStickX = constrain(SamcoPreferences::pins.aStickX, -1, 40);
                            Serial.println("OK: Set Analog X pin.");
                            break;
                          case 27:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.aStickY = Serial.parseInt();
                            SamcoPreferences::pins.aStickY = constrain(SamcoPreferences::pins.aStickY, -1, 40);
                            Serial.println("OK: Set Analog Y pin.");
                            break;
                          #endif
                          #ifdef USES_TEMP
                          case 28:
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::pins.aTMP36 = Serial.parseInt();
                            SamcoPreferences::pins.aTMP36 = constrain(SamcoPreferences::pins.aTMP36, -1, 40);
                            Serial.println("OK: Set Temperature Sensor pin.");
                            break;
                          #endif
                          default:
                            while(!Serial.available()) {
                              serialInput = Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    // Extended Settings
                    } else if(serialInput == '2') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          #ifdef USES_RUMBLE
                          case '0':
                            serialInput = Serial.read(); // nomf
                            rumbleIntensity = Serial.parseInt();
                            rumbleIntensity = constrain(rumbleIntensity, 0, 255);
                            Serial.println("OK: Set Rumble Intensity setting.");
                            break;
                          case '1':
                            serialInput = Serial.read(); // nomf
                            rumbleInterval = Serial.parseInt();
                            Serial.println("OK: Set Rumble Length setting.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case '2':
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::settings.solenoidNormalInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Normal Interval setting.");
                            break;
                          case '3':
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::settings.solenoidFastInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Fast Interval setting.");
                            break;
                          case '4':
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::settings.solenoidLongInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Hold Length setting.");
                            break;
                          #endif
                          #ifdef CUSTOM_NEOPIXEL
                          case '5':
                            serialInput = Serial.read(); // nomf
                            customLEDcount = Serial.parseInt();
                            customLEDcount = constrain(customLEDcount, 1, 255);
                            Serial.println("OK: Set NeoPixel strand length setting.");
                            break;
                          #endif
                          case '6':
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::settings.autofireWaitFactor = Serial.parseInt();
                            SamcoPreferences::settings.autofireWaitFactor = constrain(SamcoPreferences::settings.autofireWaitFactor, 2, 4);
                            Serial.println("OK: Set Autofire Wait Factor setting.");
                            break;
                          case '7':
                            serialInput = Serial.read(); // nomf
                            SamcoPreferences::settings.pauseHoldLength = Serial.parseInt();
                            Serial.println("OK: Set Hold to Pause length setting.");
                            break;
                          default:
                            while(!Serial.available()) {
                              serialInput = Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    #ifdef USE_TINYUSB
                    // TinyUSB Identifier Settings
                    } else if(serialInput == '3') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          // Device PID
                          case '0':
                            {
                              serialInput = Serial.read(); // nomf
                              SamcoPreferences::usb.devicePID = Serial.parseInt();
                              Serial.println("OK: Updated TinyUSB Device ID.");
                              EEPROM.put(EEPROM.length() - 22, SamcoPreferences::usb.devicePID);
                              #ifdef ARDUINO_ARCH_RP2040
                                  EEPROM.commit();
                              #endif // ARDUINO_ARCH_RP2040
                              break;
                            }
                          // Device name
                          case '1':
                            serialInput = Serial.read(); // nomf
                            for(byte i = 0; i < sizeof(SamcoPreferences::usb.deviceName); i++) {
                                SamcoPreferences::usb.deviceName[i] = '\0';
                            }
                            for(byte i = 0; i < 15; i++) {
                                SamcoPreferences::usb.deviceName[i] = Serial.read();
                                if(!Serial.available()) {
                                    break;
                                }
                            }
                            Serial.println("OK: Updated TinyUSB Device String.");
                            for(byte i = 0; i < 16; i++) {
                                EEPROM.update(EEPROM.length() - 18 + i, SamcoPreferences::usb.deviceName[i]);
                            }
                            #ifdef ARDUINO_ARCH_RP2040
                                EEPROM.commit();
                            #endif // ARDUINO_ARCH_RP2040
                            break;
                        }
                    #endif // USE_TINYUSB
                    // Profile settings
                    } else if(serialInput == 'P') {
                        serialInput = Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          case 'i':
                          {
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t i = serialInput - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t v = serialInput - '0';
                            v = constrain(v, 0, 2);
                            profileData[i].irSensitivity = v;
                            if(i == selectedProfile) {
                                SetIrSensitivity(v);
                            }
                            Serial.println("OK: Set IR sensitivity");
                            break;
                          }
                          case 'r':
                          {
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t i = serialInput - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            serialInput = Serial.read(); // nomf
                            serialInput = Serial.read();
                            uint8_t v = serialInput - '0';
                            v = constrain(v, 0, 2);
                            profileData[i].runMode = v;
                            if(i == selectedProfile) {
                                switch(v) {
                                  case 0:
                                    SetRunMode(RunMode_Normal);
                                    break;
                                  case 1:
                                    SetRunMode(RunMode_Average);
                                    break;
                                  case 2:
                                    SetRunMode(RunMode_Average2);
                                    break;
                                }
                            }
                            Serial.println("OK: Set Run Mode");
                            break;
                          }
                        }
                    }
                }
                break;
              }
              // Print EEPROM values.
              case 'l':
              {
                //Serial.println("Printing values saved in EEPROM...");
                serialInput = Serial.read();
                switch(serialInput) {
                  case 'b':
                    //Serial.println("----------BOOL SETTINGS----------");
                    //Serial.print("Custom pins layout enabled: ");
                    Serial.println(SamcoPreferences::toggles.customPinsInUse);
                    //Serial.print("Rumble Active: ");
                    Serial.println(SamcoPreferences::toggles.rumbleActive);
                    //Serial.print("Solenoid Active: ");
                    Serial.println(SamcoPreferences::toggles.solenoidActive);
                    //Serial.print("Autofire Active: ");
                    Serial.println(SamcoPreferences::toggles.autofireActive);
                    //Serial.print("Simple Pause Menu Enabled: ");
                    Serial.println(SamcoPreferences::toggles.simpleMenu);
                    //Serial.print("Hold to Pause Enabled: ");
                    Serial.println(SamcoPreferences::toggles.holdToPause);
                    //Serial.print("Common Anode Active: ");
                    Serial.println(SamcoPreferences::toggles.commonAnode);
                    //Serial.print("Low Buttons Mode Active: ");
                    Serial.println(SamcoPreferences::toggles.lowButtonMode);
                    //Serial.print("Rumble Force Feedback Active: ");
                    Serial.println(SamcoPreferences::toggles.rumbleFF);
                    break;
                  case 'p':
                    //Serial.println("----------PIN MAPPINGS-----------");
                    //Serial.print("Trigger: ");
                    Serial.println(SamcoPreferences::pins.bTrigger);
                    //Serial.print("Button A: ");
                    Serial.println(SamcoPreferences::pins.bGunA);
                    //Serial.print("Button B: ");
                    Serial.println(SamcoPreferences::pins.bGunB);
                    //Serial.print("Button C: ");
                    Serial.println(SamcoPreferences::pins.bGunC);
                    //Serial.print("Start: ");
                    Serial.println(SamcoPreferences::pins.bStart);
                    //Serial.print("Select: ");
                    Serial.println(SamcoPreferences::pins.bSelect);
                    //Serial.print("D-Pad Up: ");
                    Serial.println(SamcoPreferences::pins.bGunUp);
                    //Serial.print("D-Pad Down: ");
                    Serial.println(SamcoPreferences::pins.bGunDown);
                    //Serial.print("D-Pad Left: ");
                    Serial.println(SamcoPreferences::pins.bGunLeft);
                    //Serial.print("D-Pad Right: ");
                    Serial.println(SamcoPreferences::pins.bGunRight);
                    //Serial.print("External Pedal: ");
                    Serial.println(SamcoPreferences::pins.bPedal);
                    //Serial.print("Home Button: ");
                    Serial.println(SamcoPreferences::pins.bHome);
                    //Serial.print("Pump Action: ");
                    Serial.println(SamcoPreferences::pins.bPump);
                    //Serial.print("Rumble Signal Wire: ");
                    Serial.println(SamcoPreferences::pins.oRumble);
                    //Serial.print("Solenoid Signal Wire: ");
                    Serial.println(SamcoPreferences::pins.oSolenoid);
                    // for some reason, at this point, QT stops reading output.
                    // so we'll just wait for a ping to print out the rest.
                    while(!Serial.available()) {
                      // derp
                    }
                    //Serial.print("Rumble Switch: ");
                    Serial.println(SamcoPreferences::pins.sRumble);
                    //Serial.print("Solenoid Switch: ");
                    Serial.println(SamcoPreferences::pins.sSolenoid);
                    //Serial.print("Autofire Switch: ");
                    Serial.println(SamcoPreferences::pins.sAutofire);
                    //Serial.print("LED R: ");
                    Serial.println(SamcoPreferences::pins.oLedR);
                    //Serial.print("LED G: ");
                    Serial.println(SamcoPreferences::pins.oLedG);
                    //Serial.print("LED B: ");
                    Serial.println(SamcoPreferences::pins.oLedB);
                    //Serial.print("Custom NeoPixel Pin: ");
                    Serial.println(SamcoPreferences::pins.oPixel);
                    //Serial.print("Camera SDA: ");
                    Serial.println(SamcoPreferences::pins.pCamSDA);
                    //Serial.print("Camera SCL: ");
                    Serial.println(SamcoPreferences::pins.pCamSCL);
                    //Serial.print("Peripheral SDA: ");
                    Serial.println(SamcoPreferences::pins.pPeriphSDA);
                    //Serial.print("Peripheral SCL: ");
                    Serial.println(SamcoPreferences::pins.pPeriphSCL);
                    //Serial.print("Analog Joystick X: ");
                    Serial.println(SamcoPreferences::pins.aStickX);
                    //Serial.print("Analog Joystick Y: ");
                    Serial.println(SamcoPreferences::pins.aStickY);
                    //Serial.print("Temperature Sensor: ");
                    Serial.println(SamcoPreferences::pins.aTMP36);
                    // clean house now from my dirty hack.
                    while(Serial.available()) {
                      serialInput = Serial.read();
                    }
                    break;
                  case 's':
                    //Serial.println("----------OTHER SETTINGS-----------");
                    //Serial.print("Rumble Intensity Value: ");
                    Serial.println(SamcoPreferences::settings.rumbleIntensity);
                    //Serial.print("Rumble Length: ");
                    Serial.println(SamcoPreferences::settings.rumbleInterval);
                    //Serial.print("Solenoid Normal Interval: ");
                    Serial.println(SamcoPreferences::settings.solenoidNormalInterval);
                    //Serial.print("Solenoid Fast Interval: ");
                    Serial.println(SamcoPreferences::settings.solenoidFastInterval);
                    //Serial.print("Solenoid Hold Length: ");
                    Serial.println(SamcoPreferences::settings.solenoidLongInterval);
                    //Serial.print("Custom NeoPixel Strip Length: ");
                    Serial.println(SamcoPreferences::settings.customLEDcount);
                    //Serial.print("Autofire Wait Factor: ");
                    Serial.println(SamcoPreferences::settings.autofireWaitFactor);
                    //Serial.print("Hold to Pause Length: ");
                    Serial.println(SamcoPreferences::settings.pauseHoldLength);
                    break;
                  case 'P':
                    serialInput = Serial.read();
                    if(serialInput == '0' || serialInput == '1' ||
                       serialInput == '2' || serialInput == '3') {
                        uint8_t i = serialInput - '0';
                        Serial.println(profileData[i].xScale);
                        Serial.println(profileData[i].yScale);
                        Serial.println(profileData[i].xCenter);
                        Serial.println(profileData[i].yCenter);
                        Serial.println(profileData[i].irSensitivity);
                        Serial.println(profileData[i].runMode);
                    }
                    break;
                  #ifdef USE_TINYUSB
                  case 'n':
                    for(byte i = 0; i < sizeof(SamcoPreferences::usb.deviceName); i++) {
                        SamcoPreferences::usb.deviceName[i] = '\0';
                    }
                    for(byte i = 0; i < 16; i++) {
                        SamcoPreferences::usb.deviceName[i] = EEPROM.read(EEPROM.length() - 18 + i);
                    }
                    if(SamcoPreferences::usb.deviceName[0] == '\0') {
                        Serial.println("SERIALREADERR01");
                    } else {
                        Serial.println(SamcoPreferences::usb.deviceName);
                    }
                    break;
                  case 'i':
                    SamcoPreferences::usb.devicePID = 0;
                    EEPROM.get(EEPROM.length() - 22, SamcoPreferences::usb.devicePID);
                    Serial.println(SamcoPreferences::usb.devicePID);
                    break;
                  #endif // USE_TINYUSB
                }
                break;
              }
              // Testing feedback
              case 't':
                serialInput = Serial.read();
                if(serialInput == 's') {
                  digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);
                  delay(SamcoPreferences::settings.solenoidNormalInterval);
                  digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
                } else if(serialInput == 'r') {
                  analogWrite(SamcoPreferences::pins.oRumble, rumbleIntensity);
                  delay(rumbleInterval);
                  digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                }
                break;
          }
          break;
    }
}

#ifdef MAMEHOOKER
// Reading the input from the serial buffer.
// for normal runmode runtime use w/ e.g. Mamehook et al
void SerialProcessing()
{
    // So, APPARENTLY there is a map of what the serial commands are, at least wrt gun controllers.
    // "Sx" is a start command, the x being a number from 0-6 (0=sol, 1=motor, 2-4=led colors, 6=everything) but this is more a formal suggestion than anything.
    // "E" is an end command, which is the cue to hand control over back to the sketch's force feedback.
    // "M" sets parameters for how the gun should be handled when the game starts:
    //  - M3 is fullscreen or 4:3, which we don't do so ignore this
    //  - M5 is auto reload(?). How do we use this?
    //  - M1 is the reload type (0=none, 1=shoot lower left(?), 2=offscreen button, 3=real offscreen reload)
    //  - M8 is the type of auto fire (0=single shot, 1="auto"(is this a burst fire, or what?), 2=always/full auto)
    // "Fx" is the type of force feedback command being received (periods are for readability and not in the actual commands):
    //  - F0.x = solenoid - 0=off, 1=on, 2=pulse (which is the same as 1 for the solenoid)
    //  - F1.x.y = rumble motor - 0=off, 1=normal on, 2=pulse (where Y is the number of "pulses" it does)
    //  - F2/3/4.x.y = Red/Green/Blue (respectively) LED - 0=off, 1=on, 2=pulse (where Y is the strength of the LED)
    // These commands are all sent as one clump of data, with the letter being the distinguisher.
    // Apart from "E" (end), each setting bit (S/M) is two chars long, and each feedback bit (F) is four (the command, a padding bit of some kind, and the value).

    char serialInput = Serial.read();                              // Read the serial input one byte at a time (we'll read more later)
    char serialInputS[3] = {0, 0, 0};

    switch(serialInput) {
        // Start Signal
        case 'S':
          if(serialMode) {
              Serial.println("SERIALREAD: Detected Serial Start command while already in Serial handoff mode!");
          } else {
              serialMode = true;                                         // Set it on, then!
              #ifdef USES_RUMBLE
                  digitalWrite(SamcoPreferences::pins.oRumble, LOW);                          // Turn off stale rumbling from normal gun mode.
                  rumbleHappened = false;
                  rumbleHappening = false;
              #endif // USES_RUMBLE
              #ifdef USES_SOLENOID
                  digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                        // Turn off stale solenoid-ing from normal gun mode.
                  solenoidFirstShot = false;
              #endif // USES_SOLENOID
              triggerHeld = false;                                       // Turn off other stale values that serial mode doesn't use.
              burstFiring = false;
              burstFireCount = 0;
              offscreenBShot = false;
              #ifdef LED_ENABLE
                  // Set the LEDs to a mid-intense white.
                  LedUpdate(127, 127, 127);
              #endif // LED_ENABLE
          }
          break;
        // Modesetting Signal
        case 'M':
          serialInput = Serial.read();                               // Read the second bit.
          switch(serialInput) {
              case '1':
                if(serialMode) {
                    offscreenButtonSerial = true;
                } else {
                    // eh, might be useful for Linux Supermodel users.
                    offscreenButton = !offscreenButton;
                    if(offscreenButton) {
                        Serial.println("Setting offscreen button mode on!");
                    } else {
                        Serial.println("Setting offscreen button mode off!");
                    }
                }
                break;
              #ifdef USES_SOLENOID
              case '8':
                serialInput = Serial.read();                           // Nomf the padding bit.
                serialInput = Serial.read();                           // Read the next.
                if(serialInput == '1') {
                    burstFireActive = true;
                    SamcoPreferences::toggles.autofireActive = false;
                } else if(serialInput == '2') {
                    SamcoPreferences::toggles.autofireActive = true;
                    burstFireActive = false;
                } else if(serialInput == '0') {
                    SamcoPreferences::toggles.autofireActive = false;
                    burstFireActive = false;
                }
                break;
              #endif // USES_SOLENOID
              default:
                if(!serialMode) {
                    Serial.println("SERIALREAD: Serial modesetting command found, but no valid set bit found!");
                }
                break;
          }
          break;
        // End Signal
        case 'E':
          if(!serialMode) {
              Serial.println("SERIALREAD: Detected Serial End command while Serial Handoff mode is already off!");
          } else {
              serialMode = false;                                    // Turn off serial mode then.
              offscreenButtonSerial = false;                         // And clear the stale serial offscreen button mode flag.
              serialQueue = 0b00000000;
              #ifdef LED_ENABLE
                  serialLEDPulseColorMap = 0b00000000;               // Clear any stale serial LED pulses
                  serialLEDPulses = 0;
                  serialLEDPulsesLast = 0;
                  serialLEDPulseRising = true;
                  serialLEDR = 0;                                    // Clear stale serial LED values.
                  serialLEDG = 0;
                  serialLEDB = 0;
                  serialLEDChange = false;
                  LedOff();                                          // Turn it off, and let lastSeen handle it from here.
              #endif // LED_ENABLE
              #ifdef USES_RUMBLE
                  digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                  serialRumbPulseStage = 0;
                  serialRumbPulses = 0;
                  serialRumbPulsesLast = 0;
              #endif // USES_RUMBLE
              #ifdef USES_SOLENOID
                  digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
                  serialSolPulseOn = false;
                  serialSolPulses = 0;
                  serialSolPulsesLast = 0;
              #endif // USES_SOLENOID
              AbsMouse5.release(MOUSE_LEFT);
              AbsMouse5.release(MOUSE_RIGHT);
              AbsMouse5.release(MOUSE_MIDDLE);
              AbsMouse5.release(MOUSE_BUTTON4);
              AbsMouse5.release(MOUSE_BUTTON5);
              Keyboard.releaseAll();
              delay(5);
              Serial.println("Received end serial pulse, releasing FF override.");
          }
          break;
        // owo SPECIAL SETUP EH?
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Toggle Gamepad Output Mode
              case 'A':
                serialInput = Serial.read();
                switch(serialInput) {
                    case 'L':
                      if(!buttons.analogOutput) {
                          buttons.analogOutput = true;
                          AbsMouse5.release(MOUSE_LEFT);
                          AbsMouse5.release(MOUSE_RIGHT);
                          AbsMouse5.release(MOUSE_MIDDLE);
                          AbsMouse5.release(MOUSE_BUTTON4);
                          AbsMouse5.release(MOUSE_BUTTON5);
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      }
                      Gamepad16.stickRight = true;
                      Serial.println("Setting camera to the Left Stick.");
                      break;
                    case 'R':
                      if(!buttons.analogOutput) {
                          buttons.analogOutput = true;
                          AbsMouse5.release(MOUSE_LEFT);
                          AbsMouse5.release(MOUSE_RIGHT);
                          AbsMouse5.release(MOUSE_MIDDLE);
                          AbsMouse5.release(MOUSE_BUTTON4);
                          AbsMouse5.release(MOUSE_BUTTON5);
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      }
                      Gamepad16.stickRight = false;
                      Serial.println("Setting camera to the Right Stick.");
                      break;
                    default:
                      buttons.analogOutput = !buttons.analogOutput;
                      if(buttons.analogOutput) {
                          AbsMouse5.release(MOUSE_LEFT);
                          AbsMouse5.release(MOUSE_RIGHT);
                          AbsMouse5.release(MOUSE_MIDDLE);
                          AbsMouse5.release(MOUSE_BUTTON4);
                          AbsMouse5.release(MOUSE_BUTTON5);
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      } else {
                          Gamepad16.releaseAll();
                          Keyboard.releaseAll();
                          Serial.println("Switched to Mouse Output mode!");
                      }
                      break;
                }
                break;
              // Set Autofire Interval Length
              case 'I':
                serialInput = Serial.read();
                if(serialInput == '2' || serialInput == '3' || serialInput == '4') {
                    byte afSetting = serialInput - '0';
                    AutofireSpeedToggle(afSetting);
                } else {
                    Serial.println("SERIALREAD: No valid interval set! (Expected 2 to 4)");
                }
                break;
              // Remap player numbers
              case 'R':
                serialInput = Serial.read();
                switch(serialInput) {
                    case '1':
                      playerStartBtn = '1';
                      playerSelectBtn = '5';
                      Serial.println("Remapping to player slot 1.");
                      break;
                    case '2':
                      playerStartBtn = '2';
                      playerSelectBtn = '6';
                      Serial.println("Remapping to player slot 2.");
                      break;
                    case '3':
                      playerStartBtn = '3';
                      playerSelectBtn = '7';
                      Serial.println("Remapping to player slot 3.");
                      break;
                    case '4':
                      playerStartBtn = '4';
                      playerSelectBtn = '8';
                      Serial.println("Remapping to player slot 4.");
                      break;
                    default:
                      Serial.println("SERIALREAD: Player remap command called, but an invalid or no slot number was declared!");
                      break;
                }
                UpdateBindings(SamcoPreferences::toggles.lowButtonMode);
                break;
              // Enter Docked Mode
              case 'P':
                SetMode(GunMode_Docked);
                break;
              default:
                Serial.println("SERIALREAD: Internal setting command detected, but no valid option found!");
                Serial.println("Internally recognized commands are:");
                Serial.println("A(nalog)[L/R] / I(nterval Autofire)2/3/4 / R(emap)1/2/3/4 / P(ause)");
                break;
          }
          // End of 'X'
          break;
        // Force Feedback
        case 'F':
          serialInput = Serial.read();
          switch(serialInput) {
              #ifdef USES_SOLENOID
              // Solenoid bits
              case '0':
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number.
                if(serialInput == '1') {         // Is it a solenoid "on" command?)
                    bitSet(serialQueue, 0);                            // Queue the solenoid on bit.
                } else if(serialInput == '2' &&  // Is it a solenoid pulse command?
                !bitRead(serialQueue, 1)) {      // (and we aren't already pulsing?)
                    bitSet(serialQueue, 1);                            // Set the solenoid pulsing bit!
                    serialInput = Serial.read();                       // nomf the padding bit.
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialSolPulses = atoi(serialInputS);              // Import the amount of pulses we're being told to do.
                    serialSolPulsesLast = 0;                           // PulsesLast on zero indicates we haven't started pulsing.
                } else if(serialInput == '0') {  // Else, it's a solenoid off signal.
                    bitClear(serialQueue, 0);                          // Disable the solenoid off bit!
                }
                break;
              #endif // USES_SOLENOID
              #ifdef USES_RUMBLE
              // Rumble bits
              case '1':
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // read the next number.
                if(serialInput == '1') {         // Is it an on signal?
                    bitSet(serialQueue, 2);                            // Queue the rumble on bit.
                } else if(serialInput == '2' &&  // Is it a pulsed on signal?
                !bitRead(serialQueue, 3)) {      // (and we aren't already pulsing?)
                    bitSet(serialQueue, 3);                            // Set the rumble pulsed bit.
                    serialInput = Serial.read();                       // nomf the x
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialRumbPulses = atoi(serialInputS);             // and set as the amount of rumble pulses queued.
                    serialRumbPulsesLast = 0;                          // Reset the serialPulsesLast count.
                } else if(serialInput == '0') {  // Else, it's a rumble off signal.
                    bitClear(serialQueue, 2);                          // Queue the rumble off bit... 
                    //bitClear(serialQueue, 3); // And the rumble pulsed bit.
                    // TODO: do we want to set this off if we get a rumble off bit?
                }
                break;
              #endif // USES_RUMBLE
              #ifdef LED_ENABLE
              // LED Red bits
              case '2':
                serialLEDChange = true;                                // Set that we've changed an LED here!
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 4);                            // set that here!
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDR = atoi(serialInputS);                   // And set that as the strength of the red value that's requested!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                            // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000001;               // Set the R LED as the one pulsing only (overwrites the others).
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 4);                          // Set the R bit off.
                    serialLEDR = 0;                                    // Clear the R value.
                }
                break;
              // LED Green bits
              case '3':
                serialLEDChange = true;                                // Set that we've changed an LED here!
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 5);                            // set that here!
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDG = atoi(serialInputS);                   // And set that here!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                            // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000010;               // Set the G LED as the one pulsing only (overwrites the others).
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 5);                       // Set the G bit off.
                    serialLEDG = 0;                                    // Clear the G value.
                }
                break;
              // LED Blue bits
              case '4':
                serialLEDChange = true;                                // Set that we've changed an LED here!
                serialInput = Serial.read();                           // nomf the padding since it's meaningless.
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 6);                       // set that here!
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDB = atoi(serialInputS);                   // And set that as the strength requested here!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                       // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000100;               // Set the B LED as the one pulsing only (overwrites the others).
                    serialInput = Serial.read();                       // nomf the padding
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(serialInputS[n] == 'x' || serialInputS[n] == '.') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 6);                          // Set the B bit off.
                    serialLEDB = 0;                                    // Clear the G value.
                }
                break;
              #endif // LED_ENABLE
              #if !defined(USES_SOLENOID) && !defined(USES_RUMBLE) && !defined(LED_ENABLE)
              default:
                Serial.println("SERIALREAD: Feedback command detected, but no feedback devices are built into this firmware!");
              #endif
          }
          // End of 'F'
          break;
    }
}

// Handling the serial events received from SerialProcessing()
void SerialHandling()
{   // The Mamehook feedback system handles all the timing and safety for us.
    // So all we have to do is just read and process what it sends us at face value.
    // The only exception is rumble PULSE bits, where we actually do need to calculate that ourselves.

    #ifdef USES_SOLENOID
      if(SamcoPreferences::toggles.solenoidActive) {
          if(bitRead(serialQueue, 0)) {                             // If the solenoid digital bit is on,
              digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);                           // Make it go!
          } else if(bitRead(serialQueue, 1)) {                      // if the solenoid pulse bit is on,
              if(!serialSolPulsesLast) {                            // Have we started pulsing?
                  analogWrite(SamcoPreferences::pins.oSolenoid, 178);                         // Start pulsing it on!
                  serialSolPulseOn = true;                               // Set that the pulse cycle is in on.
                  serialSolPulsesLast = 1;                               // Start the sequence.
                  serialSolPulses++;                                     // Cheating and scooting the pulses bit up.
              } else if(serialSolPulsesLast <= serialSolPulses) {   // Have we met the pulses quota?
                  unsigned long currentMillis = millis();                // Calibrate timer.
                  if(currentMillis - serialSolPulsesLastUpdate > serialSolPulsesLength) { // Have we passed the set interval length between stages?
                      if(serialSolPulseOn) {                        // If we're currently pulsing on,
                          analogWrite(SamcoPreferences::pins.oSolenoid, 122);                 // Start pulsing it off.
                          serialSolPulseOn = false;                      // Set that we're in off.
                          serialSolPulsesLast++;                         // Iterate that we've done a pulse cycle,
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      } else {                                      // Or if we're pulsing off,
                          analogWrite(SamcoPreferences::pins.oSolenoid, 178);                 // Start pulsing it on.
                          serialSolPulseOn = true;                       // Set that we're in on.
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      }
                  }
              } else {  // let the armature smoothly sink loose for one more pulse length before snapping it shut off.
                  unsigned long currentMillis = millis();                // Calibrate timer.
                  if(currentMillis - serialSolPulsesLastUpdate > serialSolPulsesLength) { // Have we paassed the set interval length between stages?
                      digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                    // Finally shut it off for good.
                      bitClear(serialQueue, 1);                          // Set the pulse bit as off.
                  }
              }
          } else {  // or if it's not,
              digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                            // turn it off!
          }
      } else {
          digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
      }
  #endif // USES_SOLENOID
  #ifdef USES_RUMBLE
      if(SamcoPreferences::toggles.rumbleActive) {
          if(bitRead(serialQueue, 2)) {                             // Is the rumble on bit set?
              analogWrite(SamcoPreferences::pins.oRumble, rumbleIntensity);              // turn/keep it on.
              //bitClear(serialQueue, 3);
          } else if(bitRead(serialQueue, 3)) {                      // or if the rumble pulse bit is set,
              if(!serialRumbPulsesLast) {                           // is the pulses last bit set to off?
                  analogWrite(SamcoPreferences::pins.oRumble, 75);                            // we're starting fresh, so use the stage 0 value.
                  serialRumbPulseStage = 0;                              // Set that we're at stage 0.
                  serialRumbPulsesLast = 1;                              // Set that we've started a pulse rumble command, and start counting how many pulses we're doing.
              } else if(serialRumbPulsesLast <= serialRumbPulses) { // Have we exceeded the set amount of pulses the rumble command called for?
                  unsigned long currentMillis = millis();                // Calibrate the timer.
                  if(currentMillis - serialRumbPulsesLastUpdate > serialRumbPulsesLength) { // have we waited enough time between pulse stages?
                      switch(serialRumbPulseStage) {                     // If so, let's start processing.
                          case 0:                                        // Basically, each case
                              analogWrite(SamcoPreferences::pins.oRumble, 255);               // bumps up the intensity, (lowest to rising)
                              serialRumbPulseStage++;                    // and increments the stage of the pulse.
                              serialRumbPulsesLastUpdate = millis();     // and timestamps when we've had updated this last.
                              break;                                     // Then quits the switch.
                          case 1:
                              analogWrite(SamcoPreferences::pins.oRumble, 120);               // (rising to peak)
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          case 2:
                              analogWrite(SamcoPreferences::pins.oRumble, 75);                // (peak to falling,)
                              serialRumbPulseStage = 0;
                              serialRumbPulsesLast++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                      }
                  }
              } else {                                              // ...or the pulses count is complete.
                  digitalWrite(SamcoPreferences::pins.oRumble, LOW);                          // turn off the motor,
                  bitClear(serialQueue, 3);                              // and set the rumble pulses bit off, now that we've completed it.
              }
          } else {                                                  // ...or we're being told to turn it off outright.
              digitalWrite(SamcoPreferences::pins.oRumble, LOW);                              // Do that then.
          }
      } else {
          digitalWrite(SamcoPreferences::pins.oRumble, LOW);
      }
  #endif // USES_RUMBLE
  #ifdef LED_ENABLE
    if(serialLEDChange) {                                     // Has the LED command state changed?
        if(bitRead(serialQueue, 4) ||                         // Are either the R,
        bitRead(serialQueue, 5) ||                            // G,
        bitRead(serialQueue, 6)) {                            // OR B digital bits set to on?
            // Command the LED to change/turn on with the values serialProcessing set for us.
            LedUpdate(serialLEDR, serialLEDG, serialLEDB);
            serialLEDChange = false;                               // Set the bit to off.
        } else if(bitRead(serialQueue, 7)) {                  // Or is it an LED pulse command?
            if(!serialLEDPulsesLast) {                        // Are we just starting?
                serialLEDPulsesLast = 1;                           // Set that we have started.
                serialLEDPulseRising = true;                       // Set the LED cycle to rising.
                // Reset all the LEDs to zero, the color map will tell us which one to focus on.
                serialLEDR = 0;
                serialLEDG = 0;
                serialLEDB = 0;
            } else if(serialLEDPulsesLast <= serialLEDPulses) { // Else, have we not reached the number of pulses requested?
                unsigned long currentMillis = millis();            // Calibrate the timer.
                if(currentMillis - serialLEDPulsesLastUpdate > serialLEDPulsesLength) { // have we waited enough time between pulse stages?
                    if(serialLEDPulseRising) {                // If we're in the rising stage,
                        switch(serialLEDPulseColorMap) {           // Check the color map
                            case 0b00000001:                       // Basically for R, G, or B,
                                serialLEDR += 3;                   // Set the LED value up by three (it's easiest to do blindly like this without over/underflowing tbh)
                                if(serialLEDR == 255) {       // If we've reached the max value,
                                    serialLEDPulseRising = false;  // Set that we're in the falling state now.
                                }
                                serialLEDPulsesLastUpdate = millis(); // Timestamp this event.
                                break;                             // And get out.
                            case 0b00000010:
                                serialLEDG += 3;
                                if(serialLEDG == 255) {
                                    serialLEDPulseRising = false;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                            case 0b00000100:
                                serialLEDB += 3;
                                if(serialLEDB == 255) {
                                    serialLEDPulseRising = false;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                        }
                    } else {                                  // Or, we're in the falling stage.
                        switch(serialLEDPulseColorMap) {           // Check the color map.
                            case 0b00000001:                       // Then here, for the set color,
                                serialLEDR -= 3;                   // Decrement the value.
                                if(serialLEDR == 0) {         // If the LED value has reached the lowest point,
                                    serialLEDPulseRising = true;   // Set that we should be in the rising part of a new cycle.
                                    serialLEDPulsesLast++;         // This was a pulse cycle, so increment that.
                                }
                                serialLEDPulsesLastUpdate = millis(); // Timestamp this event.
                                break;                             // Get outta here.
                            case 0b00000010:
                                serialLEDG -= 3;
                                if(serialLEDG == 0) {
                                    serialLEDPulseRising = true;
                                    serialLEDPulsesLast++;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                            case 0b00000100:
                                serialLEDB -= 3;
                                if(serialLEDB == 0) {
                                    serialLEDPulseRising = true;
                                    serialLEDPulsesLast++;
                                }
                                serialLEDPulsesLastUpdate = millis();
                                break;
                        }
                    }
                    // Then, commit the changed value.
                    LedUpdate(serialLEDR, serialLEDG, serialLEDB);
                }
            } else {                                       // Or, we're done with the amount of pulse commands.
                serialLEDPulseColorMap = 0b00000000;               // Clear the now-stale pulse color map,
                bitClear(serialQueue, 7);                          // And flick the pulse command bit off.
            }
        } else {                                           // Or, all the LED bits are off, so we should be setting it off entirely.
            LedOff();                                              // Turn it off.
            serialLEDChange = false;                               // We've done the change, so set it off to reduce redundant LED updates.
        }
    }
    #endif // LED_ENABLE
}

// Trigger execution path while in Serial handoff mode - pulled
void TriggerFireSimple()
{
    if(!buttonPressed &&                             // Have we not fired the last cycle,
    offscreenButtonSerial && buttons.offScreen) {    // and are pointing the gun off screen WITH the offScreen button mode set?
        AbsMouse5.press(MOUSE_RIGHT);                // Press the right mouse button
        offscreenBShot = true;                       // Mark we pressed the right button via offscreen shot mode,
        buttonPressed = true;                        // Mark so we're not spamming these press events.
    } else if(!buttonPressed) {                      // Else, have we simply not fired the last cycle?
        AbsMouse5.press(MOUSE_LEFT);                 // We're handling the trigger button press ourselves for a reason.
        buttonPressed = true;                        // Set this so we won't spam a repeat press event again.
    }
}

// Trigger execution path while in Serial handoff mode - released
void TriggerNotFireSimple()
{
    if(buttonPressed) {                              // Just to make sure we aren't spamming mouse button events.
        if(offscreenBShot) {                         // if it was marked as an offscreen button shot,
            AbsMouse5.release(MOUSE_RIGHT);          // Release the right mouse,
            offscreenBShot = false;                  // And set it off.
        } else {                                     // Else,
            AbsMouse5.release(MOUSE_LEFT);           // It was a normal shot, so just release the left mouse button.
        }
        buttonPressed = false;                       // Unset the button pressed bit.
    }
}
#endif // MAMEHOOKER


void SendEscapeKey()
{
    Keyboard.press(KEY_ESC);
    delay(20);  // wait a bit so it registers on the PC.
    Keyboard.release(KEY_ESC);
}

// Macro for functions to run when gun enters new gunmode
void SetMode(GunMode_e newMode)
{
    if(gunMode == newMode) {
        return;
    }
    
    // exit current mode
    switch(gunMode) {
    case GunMode_Run:
        stateFlags |= StateFlag_PrintPreferences;
        break;
    case GunMode_CalHoriz:
        break;
    case GunMode_CalVert:
        break;
    case GunMode_CalCenter:
        break;
    case GunMode_Pause:
        break;
    case GunMode_Docked:
        if(!dockedCalibrating) {
            Serial.println("Undocking.");
        }
        break;
    }
    
    // enter new mode
    gunMode = newMode;
    switch(newMode) {
    case GunMode_Run:
        // begin run mode with all 4 points seen
        lastSeen = 0x0F;        
        break;
    case GunMode_CalHoriz:
        break;
    case GunMode_CalVert:
        break;
    case GunMode_CalCenter:
        break;
    case GunMode_Pause:
        stateFlags |= StateFlag_SavePreferencesEn | StateFlag_PrintSelectedProfile;
        break;
    case GunMode_Docked:
        stateFlags |= StateFlag_SavePreferencesEn;
        if(!dockedCalibrating) {
            Serial.println("OpenFIRE");
            Serial.println(OPENFIRE_VERSION, 1);
            Serial.println(OPENFIRE_CODENAME);
            Serial.println(OPENFIRE_BOARD);
            Serial.println(selectedProfile);
        }
        break;
    }

    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE
}

// set new run mode and apply it to the selected profile
void SetRunMode(RunMode_e newMode)
{
    if(newMode >= RunMode_Count) {
        return;
    }

    // block Processing/test modes being applied to a profile
    if(newMode <= RunMode_ProfileMax && profileData[selectedProfile].runMode != newMode) {
        profileData[selectedProfile].runMode = newMode;
        stateFlags |= StateFlag_SavePreferencesEn;
    }
    
    if(runMode != newMode) {
        runMode = newMode;
        if(!(stateFlags & StateFlag_PrintSelectedProfile)) {
            PrintRunMode();
        }
    }
}

// Simple Pause Menu scrolling function
// Bool determines if it's incrementing or decrementing the list
// LEDs update according to the setting being scrolled onto, if any.
void SetPauseModeSelection(bool isIncrement)
{
    if(isIncrement) {
        if(pauseModeSelection == PauseMode_EscapeSignal) {
            pauseModeSelection = PauseMode_Calibrate;
        } else {
            pauseModeSelection++;
            // If we use switches, and they ARE mapped to valid pins,
            // then skip over the manual toggle options.
            #ifdef USES_SWITCHES
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    SamcoPreferences::pins.sRumble >= 0 && SamcoPreferences::pins.oRumble >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    SamcoPreferences::pins.sSolenoid >= 0 && SamcoPreferences::pins.oSolenoid >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #else
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    SamcoPreferences::pins.oRumble >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    SamcoPreferences::pins.oSolenoid >= 0) {
                        pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #endif // USES_SWITCHES
        }
    } else {
        if(pauseModeSelection == PauseMode_Calibrate) {
            pauseModeSelection = PauseMode_EscapeSignal;
        } else {
            pauseModeSelection--;
            #ifdef USES_SWITCHES
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    SamcoPreferences::pins.sSolenoid >= 0 && SamcoPreferences::pins.oSolenoid >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    SamcoPreferences::pins.sRumble >= 0 && SamcoPreferences::pins.oRumble >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #else
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    SamcoPreferences::pins.oSolenoid >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    SamcoPreferences::pins.oRumble >= 0) {
                        pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #endif // USES_SWITCHES
        }
    }
    switch(pauseModeSelection) {
        case PauseMode_Calibrate:
          Serial.println("Selecting: Calibrate current profile");
          #ifdef LED_ENABLE
              LedUpdate(255,0,0);
          #endif // LED_ENABLE
          break;
        case PauseMode_ProfileSelect:
          Serial.println("Selecting: Switch profile");
          #ifdef LED_ENABLE
              LedUpdate(200,50,0);
          #endif // LED_ENABLE
          break;
        case PauseMode_Save:
          Serial.println("Selecting: Save Settings");
          #ifdef LED_ENABLE
              LedUpdate(155,100,0);
          #endif // LED_ENABLE
          break;
        #ifdef USES_RUMBLE
        case PauseMode_RumbleToggle:
          Serial.println("Selecting: Toggle rumble On/Off");
          #ifdef LED_ENABLE
              LedUpdate(100,155,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
        case PauseMode_SolenoidToggle:
          Serial.println("Selecting: Toggle solenoid On/Off");
          #ifdef LED_ENABLE
              LedUpdate(55,200,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
        /*#ifdef USES_SOLENOID
        case PauseMode_BurstFireToggle:
          Serial.println("Selecting: Toggle burst-firing mode");
          #ifdef LED_ENABLE
              LedUpdate(0,255,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
        */
        case PauseMode_EscapeSignal:
          Serial.println("Selecting: Send Escape key signal");
          #ifdef LED_ENABLE
              LedUpdate(150,0,150);
          #endif // LED_ENABLE
          break;
        /*case PauseMode_Exit:
          Serial.println("Selecting: Exit pause mode");
          break;
        */
        default:
          Serial.println("YOU'RE NOT SUPPOSED TO BE SEEING THIS");
          break;
    }
}

// Simple Pause Mode - scrolls up/down profiles list
// Bool determines if it's incrementing or decrementing the list
// LEDs update according to the profile being scrolled on, if any.
void SetProfileSelection(bool isIncrement)
{
    if(isIncrement) {
        if(profileModeSelection >= ProfileCount - 1) {
            profileModeSelection = 0;
        } else {
            profileModeSelection++;
        }
    } else {
        if(profileModeSelection <= 0) {
            profileModeSelection = ProfileCount - 1;
        } else {
            profileModeSelection--;
        }
    }
    #ifdef LED_ENABLE
        SetLedPackedColor(profileDesc[profileModeSelection].color);
    #endif // LED_ENABLE
    Serial.print("Selecting profile: ");
    Serial.println(profileDesc[profileModeSelection].profileLabel);
    return;
}

// Main routine that prints information to connected serial monitor when the gun enters Pause Mode.
void PrintResults()
{
    if(millis() - lastPrintMillis < 100) {
        return;
    }

    if(!Serial.dtr()) {
        stateFlags |= StateFlagsDtrReset;
        return;
    }

    PrintPreferences();
    /*
    Serial.print(finalX);
    Serial.print(" (");
    Serial.print(MoveXAxis);
    Serial.print("), ");
    Serial.print(finalY);
    Serial.print(" (");
    Serial.print(MoveYAxis);
    Serial.print("), H ");
    Serial.println(mySamco.H());*/

    //Serial.print("conMove ");
    //Serial.print(conMoveXAxis);
    //Serial.println(conMoveYAxis);
    
    if(stateFlags & StateFlag_PrintSelectedProfile) {
        stateFlags &= ~StateFlag_PrintSelectedProfile;
        PrintSelectedProfile();
        PrintIrSensitivity();
        PrintRunMode();
        PrintCal();
        PrintExtras();
    }
        
    lastPrintMillis = millis();
}

// what does this do again? lol
void PrintCalInterval()
{
    if(millis() - lastPrintMillis < 100 || dockedCalibrating) {
        return;
    }
    PrintCal();
    lastPrintMillis = millis();
}

// helper in case this changes
float CalScalePrefToFloat(uint16_t scale)
{
    return (float)scale / 1000.0f;
}

// helper in case this changes
uint16_t CalScaleFloatToPref(float scale)
{
    return (uint16_t)(scale * 1000.0f);
}

// Subroutine that prints all stored preferences information in a table
void PrintPreferences()
{
    if(!(stateFlags & StateFlag_PrintPreferences) || !Serial.dtr()) {
        return;
    }

    stateFlags &= ~StateFlag_PrintPreferences;
    
    PrintNVPrefsError();

    if(stateFlags & StateFlag_PrintPreferencesStorage) {
        stateFlags &= ~StateFlag_PrintPreferencesStorage;
        PrintNVStorage();
    }
    
    Serial.print("Default Profile: ");
    Serial.println(profileDesc[SamcoPreferences::profiles.selectedProfile].profileLabel);
    
    Serial.println("Profiles:");
    for(unsigned int i = 0; i < SamcoPreferences::profiles.profileCount; ++i) {
        // report if a profile has been cal'd
        if(profileData[i].xCenter && profileData[i].yCenter) {
            size_t len = strlen(profileDesc[i].buttonLabel) + 2;
            Serial.print(profileDesc[i].buttonLabel);
            Serial.print(": ");
            if(profileDesc[i].profileLabel && profileDesc[i].profileLabel[0]) {
                Serial.print(profileDesc[i].profileLabel);
                len += strlen(profileDesc[i].profileLabel);
            }
            while(len < 26) {
                Serial.print(' ');
                ++len;
            }
            Serial.print("Center: ");
            Serial.print(profileData[i].xCenter);
            Serial.print(",");
            Serial.print(profileData[i].yCenter);
            Serial.print(" Scale: ");
            Serial.print(CalScalePrefToFloat(profileData[i].xScale), 3);
            Serial.print(",");
            Serial.print(CalScalePrefToFloat(profileData[i].yScale), 3);
            Serial.print(" IR: ");
            Serial.print((unsigned int)profileData[i].irSensitivity);
            Serial.print(" Mode: ");
            Serial.println((unsigned int)profileData[i].runMode);
        }
    }
}

// Subroutine that prints current calibration profile
void PrintCal()
{
    Serial.print("Calibration: Center x,y: ");
    Serial.print(xCenter);
    Serial.print(",");
    Serial.print(yCenter);
    Serial.print(" Scale x,y: ");
    Serial.print(xScale, 3);
    Serial.print(",");
    Serial.println(yScale, 3);
}

// Subroutine that prints current runmode
void PrintRunMode()
{
    if(runMode < RunMode_Count) {
        Serial.print("Mode: ");
        Serial.println(RunModeLabels[runMode]);
    }
}

// Prints basic storage device information
// an estimation of storage used, though doesn't take extended prefs into account.
void PrintNVStorage()
{
#ifdef SAMCO_FLASH_ENABLE
    unsigned int required = SamcoPreferences::Size();
#ifndef PRINT_VERBOSE
    if(required < flash.size()) {
        return;
    }
#endif
    Serial.print("NV Storage capacity: ");
    Serial.print(flash.size());
    Serial.print(", required size: ");
    Serial.println(required);
#ifdef PRINT_VERBOSE
    Serial.print("Profile struct size: ");
    Serial.print((unsigned int)sizeof(SamcoPreferences::ProfileData_t));
    Serial.print(", Profile data array size: ");
    Serial.println((unsigned int)sizeof(profileData));
#endif
#endif // SAMCO_FLASH_ENABLE
}

// Prints error to connected serial monitor if no storage device is present
void PrintNVPrefsError()
{
    if(nvPrefsError != SamcoPreferences::Error_Success) {
        Serial.print(NVRAMlabel);
        Serial.print(" error: ");
#ifdef SAMCO_FLASH_ENABLE
        Serial.println(SamcoPreferences::ErrorCodeToString(nvPrefsError));
#else
        Serial.println(nvPrefsError);
#endif // SAMCO_FLASH_ENABLE
    }
}

// Extra settings to print to connected serial monitor when entering Pause Mode
void PrintExtras()
{
    Serial.print("Offscreen button mode enabled: ");
    if(offscreenButton) {
        Serial.println("True");
    } else {
        Serial.println("False");
    }
    #ifdef USES_RUMBLE
        Serial.print("Rumble enabled: ");
        if(SamcoPreferences::toggles.rumbleActive) {
            Serial.println("True");
        } else {
            Serial.println("False");
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        Serial.print("Solenoid enabled: ");
        if(SamcoPreferences::toggles.solenoidActive) {
            Serial.println("True");
            Serial.print("Rapid fire enabled: ");
            if(SamcoPreferences::toggles.autofireActive) {
                Serial.println("True");
            } else {
                Serial.println("False");
            }
            Serial.print("Burst fire enabled: ");
            if(burstFireActive) {
                Serial.println("True");
            } else {
                Serial.println("False");
            }
        } else {
            Serial.println("False");
        }
    #endif // USES_SOLENOID
    #ifdef ARDUINO_ARCH_RP2040
    #ifdef DUAL_CORE
        Serial.println("Running on dual cores.");
    #else
        Serial.println("Running on one core.");
    #endif // DUAL_CORE
    #endif // ARDUINO_ARCH_RP2040
    Serial.print("Firmware version: v");
    Serial.print(OPENFIRE_VERSION, 1);
    Serial.print(" - ");
    Serial.println(OPENFIRE_CODENAME);
}

// Loads preferences from EEPROM, then verifies.
void LoadPreferences()
{
    if(!nvAvailable) {
        return;
    }

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Load(flash);
#else
    nvPrefsError = samcoPreferences.LoadProfiles();
#endif // SAMCO_FLASH_ENABLE
    VerifyPreferences();
}

// Profile sanity checks
void VerifyPreferences()
{
    // center 0 is used as "no cal data"
    for(unsigned int i = 0; i < ProfileCount; ++i) {
        if(profileData[i].xCenter >= MouseMaxX || profileData[i].yCenter >= MouseMaxY || profileData[i].xScale == 0 || profileData[i].yScale == 0) {
            profileData[i].xCenter = 0;
            profileData[i].yCenter = 0;
        }

        // if the scale values are large, assign 0 so the values will be ignored
        if(profileData[i].xScale >= 30000) {
            profileData[i].xScale = 0;
        }
        if(profileData[i].yScale >= 30000) {
            profileData[i].yScale = 0;
        }
    
        if(profileData[i].irSensitivity > DFRobotIRPositionEx::Sensitivity_Max) {
            profileData[i].irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;
        }

        if(profileData[i].runMode >= RunMode_Count) {
            profileData[i].runMode = RunMode_Normal;
        }
    }

    // if default profile is not valid, use current selected profile instead
    if(SamcoPreferences::profiles.selectedProfile >= ProfileCount) {
        SamcoPreferences::profiles.selectedProfile = (uint8_t)selectedProfile;
    }
}

// Saves profile settings to EEPROM
// Blinks LEDs (if any) on success or failure.
void SavePreferences()
{
    // Unless the user's Docked,
    // Only allow one write per pause state until something changes.
    // Extra protection to ensure the same data can't write a bunch of times.
    if(gunMode != GunMode_Docked) {
        if(!nvAvailable || !(stateFlags & StateFlag_SavePreferencesEn)) {
            return;
        }

        stateFlags &= ~StateFlag_SavePreferencesEn;
    }
    
    // use selected profile as the default
    SamcoPreferences::profiles.selectedProfile = (uint8_t)selectedProfile;

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = samcoPreferences.Save(flash);
#else
    nvPrefsError = samcoPreferences.SaveProfiles();
#endif // SAMCO_FLASH_ENABLE
    if(nvPrefsError == SamcoPreferences::Error_Success) {
        Serial.print("Settings saved to ");
        Serial.println(NVRAMlabel);
        samcoPreferences.SaveToggles();
        samcoPreferences.SavePins();
        samcoPreferences.SaveSettings();
        samcoPreferences.SaveUSBID();
        #ifdef LED_ENABLE
            for(byte i = 0; i < 3; i++) {
                LedUpdate(25,25,255);
                delay(55);
                LedOff();
                delay(40);
            }
        #endif // LED_ENABLE
    } else {
        Serial.println("Error saving Preferences.");
        PrintNVPrefsError();
        #ifdef LED_ENABLE
            for(byte i = 0; i < 2; i++) {
                LedUpdate(255,10,5);
                delay(145);
                LedOff();
                delay(60);
            }
        #endif // LED_ENABLE
    }
}

void SelectCalProfileFromBtnMask(uint32_t mask)
{
    // only check if buttons are set in the mask
    if(!mask) {
        return;
    }
    for(unsigned int i = 0; i < ProfileCount; ++i) {
        if(profileDesc[i].buttonMask == mask) {
            SelectCalProfile(i);
            return;
        }
    }
}

void CycleIrSensitivity()
{
    uint8_t sens = irSensitivity;
    if(irSensitivity < DFRobotIRPositionEx::Sensitivity_Max) {
        sens++;
    } else {
        sens = DFRobotIRPositionEx::Sensitivity_Min;
    }
    SetIrSensitivity(sens);
}

void IncreaseIrSensitivity()
{
    uint8_t sens = irSensitivity;
    if(irSensitivity < DFRobotIRPositionEx::Sensitivity_Max) {
        sens++;
        SetIrSensitivity(sens);
    }
}

void DecreaseIrSensitivity()
{
    uint8_t sens = irSensitivity;
    if(irSensitivity > DFRobotIRPositionEx::Sensitivity_Min) {
        sens--;
        SetIrSensitivity(sens);
    }
}

// set a new IR camera sensitivity and apply to the selected profile
void SetIrSensitivity(uint8_t sensitivity)
{
    if(sensitivity > DFRobotIRPositionEx::Sensitivity_Max) {
        return;
    }

    if(profileData[selectedProfile].irSensitivity != sensitivity) {
        profileData[selectedProfile].irSensitivity = sensitivity;
        stateFlags |= StateFlag_SavePreferencesEn;
    }

    if(irSensitivity != (DFRobotIRPositionEx::Sensitivity_e)sensitivity) {
        irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)sensitivity;
        dfrIRPos->sensitivityLevel(irSensitivity);
        if(!(stateFlags & StateFlag_PrintSelectedProfile)) {
            PrintIrSensitivity();
        }
    }
}

void PrintIrSensitivity()
{
    Serial.print("IR Camera Sensitivity: ");
    Serial.println((int)irSensitivity);
}

void CancelCalibration()
{
    Serial.println("Calibration cancelled");
    // re-print the profile
    stateFlags |= StateFlag_PrintSelectedProfile;
    // re-apply the cal stored in the profile
    RevertToCalProfile(selectedProfile);
    if(dockedCalibrating) {
        SetMode(GunMode_Docked);
        dockedCalibrating = false;
    } else {
        // return to pause mode
        SetMode(GunMode_Pause);
    } 
}

void PrintSelectedProfile()
{
    Serial.print("Profile: ");
    Serial.println(profileDesc[selectedProfile].profileLabel);
}

// applies loaded gun profile settings
bool SelectCalProfile(unsigned int profile)
{
    if(profile >= ProfileCount) {
        return false;
    }

    if(selectedProfile != profile) {
        stateFlags |= StateFlag_PrintSelectedProfile;
        selectedProfile = profile;
    }

    bool valid = SelectCalPrefs(profile);

    // set IR sensitivity
    if(profileData[profile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max) {
        SetIrSensitivity(profileData[profile].irSensitivity);
    }

    // set run mode
    if(profileData[profile].runMode < RunMode_Count) {
        SetRunMode((RunMode_e)profileData[profile].runMode);
    }
 
    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE

    // enable save to allow setting new default profile
    stateFlags |= StateFlag_SavePreferencesEn;
    return valid;
}

// applies loaded screen calibration profile
bool SelectCalPrefs(unsigned int profile)
{
    if(profile >= ProfileCount) {
        return false;
    }

    // if center values are set, assume profile is populated
    if(profileData[profile].xCenter && profileData[profile].yCenter) {
        xCenter = profileData[profile].xCenter;
        yCenter = profileData[profile].yCenter;
        
        // 0 scale will be ignored
        if(profileData[profile].xScale) {
            xScale = CalScalePrefToFloat(profileData[profile].xScale);
        }
        if(profileData[profile].yScale) {
            yScale = CalScalePrefToFloat(profileData[profile].yScale);
        }
        return true;
    }
    return false;
}

// revert back to useable settings, even if not cal'd
void RevertToCalProfile(unsigned int profile)
{
    // if the current profile isn't valid
    if(!SelectCalProfile(profile)) {
        // revert to settings from any valid profile
        for(unsigned int i = 0; i < ProfileCount; ++i) {
            if(SelectCalProfile(i)) {
                break;
            }
        }

        // stay selected on the specified profile
        SelectCalProfile(profile);
    }
}

// apply current cal to the selected profile
void ApplyCalToProfile()
{
    profileData[selectedProfile].xCenter = xCenter;
    profileData[selectedProfile].yCenter = yCenter;
    profileData[selectedProfile].xScale = CalScaleFloatToPref(xScale);
    profileData[selectedProfile].yScale = CalScaleFloatToPref(yScale);

    stateFlags |= StateFlag_PrintSelectedProfile;
}

#ifdef LED_ENABLE
// initializes system and 4pin RGB LEDs.
// ONLY to be used from setup()
void LedInit()
{
    // init DotStar and/or NeoPixel to red during setup()
    // For the onboard NEOPIXEL, if any; it needs to be enabled.
    #ifdef NEOPIXEL_ENABLEPIN
        pinMode(NEOPIXEL_ENABLEPIN, OUTPUT);
        digitalWrite(NEOPIXEL_ENABLEPIN, HIGH);
    #endif // NEOPIXEL_ENABLEPIN
 
    #ifdef DOTSTAR_ENABLE
        dotstar.begin();
    #endif // DOTSTAR_ENABLE

    #ifdef NEOPIXEL_PIN
        neopixel.begin();
    #endif // NEOPIXEL_PIN
    #ifdef CUSTOM_NEOPIXEL
        if(SamcoPreferences::pins.oPixel >= 0) {
            externPixel->begin();
        }
    #endif // CUSTOM_NEOPIXEL
 
    #ifdef ARDUINO_NANO_RP2040_CONNECT
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    #endif // NANO_RP2040
    LedUpdate(255, 0, 0);
}

// 32-bit packed color value update across all LED units
void SetLedPackedColor(uint32_t color)
{
#ifdef DOTSTAR_ENABLE
    dotstar.setPixelColor(0, Adafruit_DotStar::gamma32(color & 0x00FFFFFF));
    dotstar.show();
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    neopixel.setPixelColor(0, Adafruit_NeoPixel::gamma32(color & 0x00FFFFFF));
    neopixel.show();
#endif // NEOPIXEL_PIN
#ifdef CUSTOM_NEOPIXEL
    if(SamcoPreferences::pins.oPixel >= 0) {
        externPixel->fill(0, Adafruit_NeoPixel::gamma32(color & 0x00FFFFFF));
        externPixel->show();
    }
#endif // CUSTOM_NEOPIXEL
#ifdef FOURPIN_LED
    if(ledIsValid) {
        byte r = highByte(color >> 8);
        byte g = highByte(color);
        byte b = lowByte(color);
        if(SamcoPreferences::toggles.commonAnode) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        analogWrite(SamcoPreferences::pins.oLedR, r);
        analogWrite(SamcoPreferences::pins.oLedG, g);
        analogWrite(SamcoPreferences::pins.oLedB, b);
    }
#endif // FOURPIN_LED
#ifdef ARDUINO_NANO_RP2040_CONNECT
    byte r = highByte(color >> 8);
    byte g = highByte(color);
    byte b = lowByte(color);
    r = ~r;
    g = ~g;
    b = ~b;
    analogWrite(LEDR, r);
    analogWrite(LEDG, g);
    analogWrite(LEDB, b);
#endif // NANO_RP2040
}

void LedOff()
{
    LedUpdate(0, 0, 0);
}

// Generic R/G/B value update across all LED units
void LedUpdate(byte r, byte g, byte b)
{
    #ifdef DOTSTAR_ENABLE
        dotstar.setPixelColor(0, r, g, b);
        dotstar.show();
    #endif // DOTSTAR_ENABLE
    #ifdef NEOPIXEL_PIN
        neopixel.setPixelColor(0, r, g, b);
        neopixel.show();
    #endif // NEOPIXEL_PIN
    #ifdef CUSTOM_NEOPIXEL
        if(SamcoPreferences::pins.oPixel >= 0) {
            for(byte i = 0; i < customLEDcount; i++) {
                externPixel->setPixelColor(i, r, g, b);
            }
            externPixel->show();
        }
    #endif // CUSTOM_NEOPIXEL
    #ifdef FOURPIN_LED
        if(ledIsValid) {
            if(SamcoPreferences::toggles.commonAnode) {
                r = ~r;
                g = ~g;
                b = ~b;
            }
            analogWrite(SamcoPreferences::pins.oLedR, r);
            analogWrite(SamcoPreferences::pins.oLedG, g);
            analogWrite(SamcoPreferences::pins.oLedB, b);
        }
    #endif // FOURPIN_LED
    #ifdef ARDUINO_NANO_RP2040_CONNECT
        #ifdef FOURPIN_LED
        // Nano's builtin is a common anode, so we use that logic by default if it's enabled on the external 4-pin;
        // otherwise, invert the values.
        if((ledIsValid && !SamcoPreferences::toggles.commonAnode) || !ledIsValid) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        #else
            r = ~r;
            g = ~g;
            b = ~b;
        #endif // FOURPIN_LED
        analogWrite(LEDR, r);
        analogWrite(LEDG, g);
        analogWrite(LEDB, b);
    #endif // NANO_RP2040
}

// Macro that sets LEDs color depending on the mode it's set to
void SetLedColorFromMode()
{
    switch(gunMode) {
    case GunMode_CalHoriz:
    case GunMode_CalVert:
    case GunMode_CalCenter:
        SetLedPackedColor(CalModeColor);
        break;
    case GunMode_Pause:
        SetLedPackedColor(profileDesc[selectedProfile].color);
        break;
    case GunMode_Run:
        if(lastSeen) {
            LedOff();
        } else {
            SetLedPackedColor(IRSeen0Color);
        }
        break;
    default:
        break;
    }
}
#endif // LED_ENABLE

// Pause mode offscreen trigger mode toggle widget
// Blinks LEDs (if any) according to enabling or disabling this obtuse setting for finnicky/old programs that need right click as an offscreen shot substitute.
void OffscreenToggle()
{
    offscreenButton = !offscreenButton;
    if(offscreenButton) {                                         // If we turned ON this mode,
        Serial.println("Enabled Offscreen Button!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Ghost_white);            // Set a color,
        #endif // LED_ENABLE
        #ifdef USES_RUMBLE
            digitalWrite(SamcoPreferences::pins.oRumble, HIGH);                        // Set rumble on
            delay(125);                                           // For this long,
            digitalWrite(SamcoPreferences::pins.oRumble, LOW);                         // Then flick it off,
            delay(150);                                           // wait a little,
            digitalWrite(SamcoPreferences::pins.oRumble, HIGH);                        // Flick it back on
            delay(200);                                           // For a bit,
            digitalWrite(SamcoPreferences::pins.oRumble, LOW);                         // and then turn it off,
        #else
            delay(450);
        #endif // USES_RUMBLE
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {                                                      // Or we're turning this OFF,
        Serial.println("Disabled Offscreen Button!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Ghost_white);            // Just set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Ghost_white);            // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}

// Pause mode autofire factor toggle widget
// Does a test fire demonstrating the autofire speed being toggled
void AutofireSpeedToggle(byte setting)
{
    // If a number is passed, assume this is from Serial and directly set it.
    if(setting >= 2 && setting <= 4) {
        SamcoPreferences::settings.autofireWaitFactor = setting;
        Serial.print("Autofire speed level ");
        Serial.println(setting);
        return;
    // Else, this is a button toggle, so cycle.
    } else {
        switch (SamcoPreferences::settings.autofireWaitFactor) {
            case 2:
                SamcoPreferences::settings.autofireWaitFactor = 3;
                Serial.println("Autofire speed level 2.");
                break;
            case 3:
                SamcoPreferences::settings.autofireWaitFactor = 4;
                Serial.println("Autofire speed level 3.");
                break;
            case 4:
                SamcoPreferences::settings.autofireWaitFactor = 2;
                Serial.println("Autofire speed level 1.");
                break;
        }
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Magenta);                    // Set a color,
        #endif // LED_ENABLE
        #ifdef USES_SOLENOID
            for(byte i = 0; i < 5; i++) {                             // And demonstrate the new autofire factor five times!
                digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);
                delay(SamcoPreferences::settings.solenoidFastInterval);
                digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
                delay(SamcoPreferences::settings.solenoidFastInterval * SamcoPreferences::settings.autofireWaitFactor);
            }
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);    // And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}

/*
// Unused since runtime burst fire toggle was removed from pause mode, and is accessed from the serial command M8x1
void BurstFireToggle()
{
    burstFireActive = !burstFireActive;                           // Toggle burst fire mode.
    if(burstFireActive) {  // Did we flick it on?
        Serial.println("Burst firing enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Orange);
        #endif
        #ifdef USES_SOLENOID
            for(byte i = 0; i < 4; i++) {
                digitalWrite(solenoidPin, HIGH);                  // Demonstrate it by flicking the solenoid on/off three times!
                delay(SamcoPreferences::settings.solenoidFastInterval);                      // (at a fixed rate to distinguish it from autofire speed toggles)
                digitalWrite(solenoidPin, LOW);
                delay(SamcoPreferences::settings.solenoidFastInterval * 2);
            }
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {  // Or we flicked it off.
        Serial.println("Burst firing disabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Orange);
        #endif // LED_ENABLE
        #ifdef USES_SOLENOID
            digitalWrite(solenoidPin, HIGH);                      // Just hold it on for a second.
            delay(300);
            digitalWrite(solenoidPin, LOW);                       // Then off.
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
*/

#ifdef USES_RUMBLE
// Pause mode rumble enabling widget
// Does a cute rumble pattern when on, or blinks LEDs (if any)
void RumbleToggle()
{
    SamcoPreferences::toggles.rumbleActive = !SamcoPreferences::toggles.rumbleActive;                                 // Toggle
    if(SamcoPreferences::toggles.rumbleActive) {                                            // If we turned ON this mode,
        Serial.println("Rumble enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(SamcoPreferences::pins.oRumble, HIGH);                            // Pulse the motor on to notify the user,
        delay(300);                                               // Hold that,
        digitalWrite(SamcoPreferences::pins.oRumble, LOW);                             // Then turn off,
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {                                                      // Or if we're turning it OFF,
        Serial.println("Rumble disabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Salmon);                 // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
// Pause mode solenoid enabling widget
// Does a cute solenoid engagement, or blinks LEDs (if any)
void SolenoidToggle()
{
    SamcoPreferences::toggles.solenoidActive = !SamcoPreferences::toggles.solenoidActive;                             // Toggle
    if(SamcoPreferences::toggles.solenoidActive) {                                          // If we turned ON this mode,
        Serial.println("Solenoid enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);                          // Engage the solenoid on to notify the user,
        delay(300);                                               // Hold it that way for a bit,
        digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                           // Release it,
        #ifdef LED_ENABLE
            SetLedPackedColor(profileDesc[selectedProfile].color);    // And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {                                                      // Or if we're turning it OFF,
        Serial.println("Solenoid disabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Yellow);                 // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileDesc[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
#endif // USES_SOLENOID

#ifdef USES_SOLENOID
// Subroutine managing solenoid state and temperature monitoring (if any)
void SolenoidActivation(int solenoidFinalInterval)
{
    if(solenoidFirstShot) {                                       // If this is the first time we're shooting, it's probably safe to shoot regardless of temps.
        unsigned long currentMillis = millis();                   // Initialize timer.
        previousMillisSol = currentMillis;                        // Calibrate the timer for future calcs.
        digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);                          // Since we're shooting the first time, just turn it on aaaaand fire.
        return;                                                   // We're done here now.
    }
    #ifdef USES_TEMP                                              // *If the build calls for a TMP36 temperature sensor,
        if(SamcoPreferences::pins.aTMP36 >= 0) { // If a temp sensor is installed and enabled,
            int tempSensor = analogRead(SamcoPreferences::pins.aTMP36);
            tempSensor = (((tempSensor * 3.3) / 4096) - 0.5) * 100; // Convert reading from mV->3.3->12-bit->Celsius
            #ifdef PRINT_VERBOSE
                Serial.print("Current Temp near solenoid: ");
                Serial.print(tempSensor);
                Serial.println("*C");
            #endif
            if(tempSensor < tempNormal) { // Are we at (relatively) normal operating temps?
                unsigned long currentMillis = millis();
                if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                    previousMillisSol = currentMillis;
                    digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // run the solenoid into the state we've just inverted it to.
                    return;
                } else { // If we pass the temp check but fail the timer check, we're here too quick.
                    return;
                }
            } else if(tempSensor < tempWarning) { // If we failed the room temp check, are we beneath the shutoff threshold?
                if(digitalRead(SamcoPreferences::pins.oSolenoid)) {    // Is the valve being pulled now?
                    unsigned long currentMillis = millis();           // If so, we should release it on the shorter timer.
                    if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                        previousMillisSol = currentMillis;
                        digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // Flip, flop.
                        return;
                    } else { // OR, Passed the temp check, STILL here too quick.
                        return;
                    }
                } else { // The solenoid's probably off, not on right now. So that means we should wait a bit longer to fire again.
                    unsigned long currentMillis = millis();
                    if(currentMillis - previousMillisSol >= solenoidWarningInterval) { // We're keeping it low for a bit longer, to keep temps stable. Try to give it a bit of time to cool down before we go again.
                        previousMillisSol = currentMillis;
                        digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid));
                        return;
                    } else { // OR, We passed the temp check but STILL got here too quick.
                        return;
                    }
                }
            } else { // Failed both temp checks, so obviously it's not safe to fire.
                #ifdef PRINT_VERBOSE
                    Serial.println("Solenoid over safety threshold; not activating!");
                #endif
                digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                       // Make sure it's off if we're this dangerously close to the sun.
                return;
            }
        } else { // No temp sensor, so just go ahead.
            unsigned long currentMillis = millis();                   // Start the timer.
            if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
                previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
                digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // run the solenoid into the state we've just inverted it to.
                return;                                               // Aaaand we're done here.
            } else {                                                  // If we failed the timer check, we're here too quick.
                return;                                               // Get out of here, speedy mc loserpants.
            }
        }
    #else                                                         // *The shorter version of this function if we're not using a temp sensor.
        unsigned long currentMillis = millis();                   // Start the timer.
        if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
            previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
            digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // run the solenoid into the state we've just inverted it to.
            return;                                               // Aaaand we're done here.
        } else {                                                  // If we failed the timer check, we're here too quick.
            return;                                               // Get out of here, speedy mc loserpants.
        }
    #endif
}
#endif // USES_SOLENOID

#ifdef USES_RUMBLE
// Subroutine managing rumble state
void RumbleActivation()
{
    if(rumbleHappening) {                                         // Are we in a rumble command rn?
        unsigned long currentMillis = millis();                   // Calibrate a timer to set how long we've been rumbling.
        if(currentMillis - previousMillisRumble >= rumbleInterval) { // If we've been waiting long enough for this whole rumble command,
            digitalWrite(SamcoPreferences::pins.oRumble, LOW);                         // Make sure the rumble is OFF.
            rumbleHappening = false;                              // This rumble command is done now.
            rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
        }
        return;                                                   // Alright we done here (if we did this before already)
    } else {                                                      // OR, we're rumbling for the first time.
        previousMillisRumble = millis();                          // Mark this as the start of this rumble command.
        analogWrite(SamcoPreferences::pins.oRumble, rumbleIntensity);                  // Set the motor on.
        rumbleHappening = true;                                   // Mark that we're in a rumble command rn.
        return;                                                   // Now geddoutta here.
    }
}
#endif // USES_RUMBLE

// Solenoid 3-shot burst firing subroutine
void BurstFire()
{
    if(burstFireCount < 4) {  // Are we within the three shots alotted to a burst fire command?
        #ifdef USES_SOLENOID
            if(!digitalRead(SamcoPreferences::pins.oSolenoid) &&  // Is the solenoid NOT on right now, and the counter hasn't matched?
            (burstFireCount == burstFireCountLast)) {
                burstFireCount++;                                 // Increment the counter.
            }
            if(!digitalRead(SamcoPreferences::pins.oSolenoid)) {  // Now, is the solenoid NOT on right now?
                SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval * 2);     // Hold it off a bit longer,
            } else {                         // or if it IS on,
                burstFireCountLast = burstFireCount;              // sync the counters since we completed one bullet cycle,
                SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval);         // And start trying to activate the dingus.
            }
        #endif // USES_SOLENOID
        return;
    } else {  // If we're at three bullets fired,
        burstFiring = false;                                      // Disable the currently firing tag,
        burstFireCount = 0;                                       // And set the count off.
        return;                                                   // Let's go back.
    }
}

// Updates the button array with new pin mappings and control bindings, if any.
void UpdateBindings(bool offscreenEnable)
{
    // Updates pins
    LightgunButtons::ButtonDesc[BtnIdx_Trigger].pin = SamcoPreferences::pins.bTrigger;
    LightgunButtons::ButtonDesc[BtnIdx_A].pin = SamcoPreferences::pins.bGunA;
    LightgunButtons::ButtonDesc[BtnIdx_B].pin = SamcoPreferences::pins.bGunB;
    LightgunButtons::ButtonDesc[BtnIdx_Reload].pin = SamcoPreferences::pins.bGunC;
    LightgunButtons::ButtonDesc[BtnIdx_Start].pin = SamcoPreferences::pins.bStart;
    LightgunButtons::ButtonDesc[BtnIdx_Select].pin = SamcoPreferences::pins.bSelect;
    LightgunButtons::ButtonDesc[BtnIdx_Up].pin = SamcoPreferences::pins.bGunUp;
    LightgunButtons::ButtonDesc[BtnIdx_Down].pin = SamcoPreferences::pins.bGunDown;
    LightgunButtons::ButtonDesc[BtnIdx_Left].pin = SamcoPreferences::pins.bGunLeft;
    LightgunButtons::ButtonDesc[BtnIdx_Right].pin = SamcoPreferences::pins.bGunRight;
    LightgunButtons::ButtonDesc[BtnIdx_Pedal].pin = SamcoPreferences::pins.bPedal;
    LightgunButtons::ButtonDesc[BtnIdx_Pump].pin = SamcoPreferences::pins.bPump;
    LightgunButtons::ButtonDesc[BtnIdx_Home].pin = SamcoPreferences::pins.bHome;

    // Updates button functions for low-button mode
    if(offscreenEnable) {
        LightgunButtons::ButtonDesc[1].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[1].reportCode2 = playerStartBtn;
        LightgunButtons::ButtonDesc[2].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[2].reportCode2 = playerSelectBtn;
        LightgunButtons::ButtonDesc[3].reportCode = playerStartBtn;
        LightgunButtons::ButtonDesc[4].reportCode = playerSelectBtn;
    } else {
        LightgunButtons::ButtonDesc[1].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[1].reportCode2 = MOUSE_RIGHT;
        LightgunButtons::ButtonDesc[2].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[2].reportCode2 = MOUSE_MIDDLE;
        LightgunButtons::ButtonDesc[3].reportCode = playerStartBtn;
        LightgunButtons::ButtonDesc[4].reportCode = playerSelectBtn;
    }
}

#ifdef DEBUG_SERIAL
void PrintDebugSerial()
{
    // only print every second
    if(millis() - serialDbMs >= 1000 && Serial.dtr()) {
        Serial.print("mode ");
        Serial.print(gunMode);
        Serial.print(", IR pos fps ");
        Serial.print(irPosCount);
        Serial.print(", loop/sec ");
        Serial.print(frameCount);

        Serial.print(", Mouse X,Y ");
        Serial.print(conMoveXAxis);
        Serial.print(",");
        Serial.println(conMoveYAxis);
        
        frameCount = 0;
        irPosCount = 0;
        serialDbMs = millis();
    }
}
#endif // DEBUG_SERIAL
