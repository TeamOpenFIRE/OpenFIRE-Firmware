/*!
 * @file SamcoEnhanced.ino
 * @brief IR-GUN4ALL - 4IR LED Lightgun sketch w/ support for force feedback and other features.
 * Based on Prow's Enhanced Fork from https://github.com/Prow7/ir-light-gun,
 * Which in itself is based on the 4IR Beta "Big Code Update" SAMCO project from https://github.com/samuelballantyne/IR-Light-Gun
 *
 * @copyright Samco, https://github.com/samuelballantyne, June 2020
 * @copyright Mike Lynch, July 2021
 * @copyright That One Seong, https://github.com/SeongGino, October 2023
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @date 2023
 */
#define G4ALL_VERSION 2.1
#define G4ALL_CODENAME "Fantasyland-rev1"

 // Remember to check out the enclosed instruction book (README.md) that came with this program for more information!

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


    // IMPORTANT ADDITIONS HERE ------------------------------------------------------------------------------- (*****THE HARDWARE SETTINGS YOU WANNA TWEAK!*****)
  // Enables input processing on the second core, if available. Currently exclusive to Raspberry Pi Pico, or boards based on the RP2040.
  // Isn't necessarily faster, but might make responding to force feedback more consistent.
  // If unsure, leave this commented.
//#define DUAL_CORE

  // Here we define the Manufacturer Name/Device Name/PID:VID of the gun as will be displayed by the operating system.
  // For multiplayer, different guns need different IDs!
  // If unsure, or are only using one gun, just leave these at their defaults!
#define MANUFACTURER_NAME "3DP"
#define DEVICE_NAME "GUN4ALL-Con"
#define DEVICE_VID 0x0920
#define DEVICE_PID 0x1998

  // Set what player this board is mapped to by default (1-4). This will change keyboard mappings appropriate for the respective player.
  // If unsure, just leave this at 1 - the mapping can be changed at runtime by sending an 'XR#' command over Serial, where # = player number
#define PLAYER_NUMBER 1

  // Uncomment this to enable (currently experimental) MAMEHOOKER support, or leave commented out to disable references to serial reading and only use it for debugging.
  // WARNING: Has a chance of making the board lock up if TinyUSB hasn't been patched to fix serial-related lockups.
  // Currently fixed only for RP2040 using the fixed core as provided in the instruction manual. Consider this option potentially dangerous for other boards.
  // If unsure, or you don't plan to use MAMEHOOKER, leave this commented.
//#define MAMEHOOKER

  // Leave this uncommented if your build uses hardware switches, or comment out (//) to disable all references to hw switch functionality.
#define USES_SWITCHES
#ifdef USES_SWITCHES // Here's where they should be defined!
    const byte autofireSwitch = 18;                   // What's the pin number of the autofire switch? Digital.
#endif // USES_SWITCHES

  // Leave this uncommented if your build uses a rumble motor; comment out to disable any references to rumble functionality.
#define USES_RUMBLE
#ifdef USES_RUMBLE
    bool rumbleActive = true;                         // Are we allowed to do rumble? Default to off.
    #ifdef USES_SWITCHES
        const byte rumbleSwitch = 19;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_SWITCHES

    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
#endif // USES_RUMBLE

  // Leave this uncommented if your build uses a solenoid, or comment out to disable any references to solenoid functionality.
#define USES_SOLENOID
#ifdef USES_SOLENOID
    bool solenoidActive = false;                      // Are we allowed to use a solenoid? Default to off.
    #ifdef USES_SWITCHES // If your build uses hardware switches,
        const byte solenoidSwitch = 20;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SWITCHES
    
  // Uncomment if your build uses a TMP36 temperature sensor for a solenoid, or comment out if your solenoid doesn't need babysitting.
    //#define USES_TEMP
    #ifdef USES_TEMP    
        const byte tempPin = A0;                      // What's the pin number of the temp sensor? Needs to be analog.
        const byte tempNormal = 50;                   // Solenoid: Anything below this value is "normal" operating temperature for the solenoid, in Celsius.
        const byte tempWarning = 60;                  // Solenoid: Above normal temps, this is the value up to where we throttle solenoid activation, in Celsius.
    #endif // USES_TEMP                               // **Anything above ^this^ is considered too dangerous, will disallow any further engagement.
#endif // USES_SOLENOID


  // Which software extras should be activated? Set here if your build doesn't use toggle switches.
bool autofireActive = false;                          // Is solenoid firing in autofire (rapid) mode? false = default single shot, true = autofire
bool offscreenButton = false;                         // Does shooting offscreen also send a button input (for buggy games that don't recognize off-screen shots)? Default to off.
bool burstFireActive = false;                         // Is the solenoid firing in burst fire mode? false = default, true = 3-shot burst firing mode


  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
int8_t rumblePin = 24;                            // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 25;                          // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = 6;                            // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = 7;                               // <-- GCon 1-spec
int8_t btnGunB = 8;                               // <-- GCon 1-spec
int8_t btnGunC = 9;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 10;
int8_t btnSelect = 11;
int8_t btnGunUp = 1;
int8_t btnGunDown = 0;
int8_t btnGunLeft = 4;
int8_t btnGunRight = 5;
int8_t btnPedal = 12;                             // If you're using a physical Time Crisis-style pedal, this is for that.
int8_t btnPump = -1;
int8_t btnHome = -1;

  // If your build uses an analog stick, unset and define the stick's pins here!
  // Remember: ANALOG PINS ONLY!
//#define USES_ANALOG
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // If you're using a regular 4-pin RGB LED, unset and define the R/G/B color pins here!
  // Remember: PWM PINS ONLY!
//#define FOURPIN_LED
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // If you're using a custom external NeoPixel unit, unset and define the data pin location here!
  // Any digital pin is fine for NeoPixels. Currently we only use the first "pixel".
//#define CUSTOM_NEOPIXEL
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
    int8_t customLEDpin = -1;                     // Pin number for the custom NeoPixel (strip) being used.
    uint8_t customLEDcount = 1;                   // Amount of pixels; if not using a strip, just set to 1.
#endif // CUSTOM_NEOPIXEL

  // Adjustable aspects:
byte autofireWaitFactor = 3;                          // This is the default time to wait between rapid fire pulses (from 2-4)
#ifdef USES_RUMBLE
    const byte rumbleIntensity = 255;                 // The strength of the rumble motor, 0=off to 255=maxPower.
    const unsigned int rumbleInterval = 110;          // How long to wait for the whole rumble command, in ms.
#endif // USES_RUMBLE
#ifdef USES_SOLENOID
    const unsigned int solenoidNormalInterval = 45;   // Interval for solenoid activation, in ms.
    const unsigned int solenoidFastInterval = 30;     // Interval for faster solenoid activation, in ms.
    const unsigned int solenoidLongInterval = 500;    // for single shot, how long to wait until we start spamming the solenoid? In ms.
#endif // USES_SOLENOID

// Menu options:
bool simpleMenu = false;                              // Flag that determines if Pause Mode will be a simple scrolling menu; else, relies on hotkeys.
bool holdToPause = false;                             // Flag that determines if Pause Mode is invoked by a button combo hold (EnterPauseModeHoldBtnMask) when pointing offscreen; else, relies on EnterPauseModeBtnMask hotkey.
unsigned int pauseHoldLength = 4000;                  // How long the combo should be held for, in ms.

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

// extra position glitch filtering, 
// not required after discoverving the DFRobotIRPositionEx atomic read technique
//#define EXTRA_POS_GLITCH_FILTER

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
    {btnTrigger, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK}, // Barry says: "I'll handle this."
    {btnGunA, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, 1, 15, BTN_AG_MASK2},
    {btnGunB, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Gamepad, 2, 15, BTN_AG_MASK2},
    {btnStart, LightgunButtons::ReportType_Internal, playerStartBtn, LightgunButtons::ReportType_Internal, playerStartBtn, LightgunButtons::ReportType_Gamepad, 5, 20, BTN_AG_MASK2},
    {btnSelect, LightgunButtons::ReportType_Internal, playerSelectBtn, LightgunButtons::ReportType_Internal, playerSelectBtn, LightgunButtons::ReportType_Gamepad, 6, 20, BTN_AG_MASK2},
    {btnGunUp, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, LightgunButtons::ReportType_Gamepad, 7, 20, BTN_AG_MASK2},
    {btnGunDown, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, LightgunButtons::ReportType_Gamepad, 8, 20, BTN_AG_MASK2},
    {btnGunLeft, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, LightgunButtons::ReportType_Gamepad, 9, 20, BTN_AG_MASK2},
    {btnGunRight, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, LightgunButtons::ReportType_Gamepad, 10, 20, BTN_AG_MASK2},
    {btnGunC, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Gamepad, 3, 15, BTN_AG_MASK2},
    {btnPedal, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Gamepad, 4, 15, BTN_AG_MASK2},
    {btnPump, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, 1, 15, BTN_AG_MASK2},
    {btnHome, LightgunButtons::ReportType_Internal, MOUSE_BUTTON5, LightgunButtons::ReportType_Internal, MOUSE_BUTTON5, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK2}
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

// press any button to enter pause mode from Processing mode (this is not a button combo)
uint32_t EnterPauseModeProcessingBtnMask = BtnMask_A | BtnMask_B | BtnMask_Reload;

// button combo to exit pause mode back to run mode
uint32_t ExitPauseModeBtnMask = BtnMask_Reload;

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
uint32_t RunModeProcessingBtnMask = BtnMask_Start | BtnMask_Right;

// button combination to toggle offscreen button mode in software:
uint32_t OffscreenButtonToggleBtnMask = BtnMask_Reload | BtnMask_A;

// button combination to toggle offscreen button mode in software:
uint32_t AutofireSpeedToggleBtnMask = BtnMask_Reload | BtnMask_B;

#ifndef USES_SWITCHES
// button combination to toggle rumble in software:
uint32_t RumbleToggleBtnMask = BtnMask_Left;

// button combination to toggle solenoid in software:
uint32_t SolenoidToggleBtnMask = BtnMask_Right;
#endif

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
    {0, 0, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {0, 0, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {0, 0, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0},
    {0, 0, 0, 0, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, 0, 0}
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

  // ADDITIONS HERE: the boring inits related to the things I added.
// Offscreen bits:
//bool offScreen = false;                        // To tell if we're off-screen, if offXAxis & offYAxis are true. Part of LightgunButtons now.
bool offXAxis = false;                           // Supercedes the inliner every trigger pull, we just check each axis individually.
bool offYAxis = false;

// Boring values for the solenoid timing stuff:
#ifdef USES_SOLENOID
    unsigned long previousMillisSol = 0;         // our timer (holds the last time since a successful interval pass)
    bool solenoidFirstShot = false;              // default to off, but actually set this the first time we shoot.
    #ifdef USES_TEMP
        int tempSensor;                          // Temp sensor changes over time, so just initialize the variable here ig.
        const unsigned int solenoidWarningInterval = solenoidFastInterval * 3; // for if solenoid is getting toasty.
    #endif // USES_TEMP
#endif // USES_SOLENOID

// For burst firing stuff:
byte burstFireCount = 0;                         // What shot are we on?
byte burstFireCountLast = 0;                     // What shot have we last processed?
bool burstFiring = false;                        // Are we in a burst fire command?

// For offscreen button stuff:
bool offscreenBShot = false;                     // For offscreenButton functionality, to track if we shot off the screen.
bool buttonPressed = false;                      // Sanity check.

// For autofire:
bool triggerHeld = false;                        // Trigger SHOULDN'T be being pulled by default, right?

// For rumble:
#ifdef USES_RUMBLE
    unsigned long previousMillisRumble = 0;      // our time since the rumble motor event started
    bool rumbleHappening = false;                // To keep track on if this is a rumble command or not.
    bool rumbleHappened = false;                 // If we're holding, this marks we sent a rumble command already.
    // We need the rumbleHappening because of the variable nature of the PWM controlling the motor.
#endif // USES_RUMBLE

#ifdef USES_ANALOG
    bool analogIsValid;                          // Flag set true if analog stick is mapped to valid nums
    bool analogStickPolled = false;              // Flag set on if the stick was polled recently, to prevent overloading with aStick updates.
#endif // USES_ANALOG

#ifdef FOURPIN_LED
    bool ledIsValid;                             // Flag set true if RGB pins are mapped to valid numbers
#endif // FOURPIN_LED

// For button queuing:
byte buttonsHeld = 0b00000000;                   // Bitmask of what aux buttons we've held on (for btnStart through btnRight)

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

bool justBooted = true;                              // For ops we need to do on initial boot (just joystick centering atm)

#ifdef EXTRA_POS_GLITCH_FILTER00
int badFinalTick = 0;
int badMoveTick = 0;
int badFinalCount = 0;
int badMoveCount = 0;

// number of consecutive bad move values to filter
constexpr unsigned int BadMoveCountThreshold = 3;

// Used to filter out large jumps/glitches
constexpr int BadMoveThreshold = 49 * CamToMouseMult;
#endif // EXTRA_POS_GLITCH_FILTER

// profile in use
unsigned int selectedProfile = 0;

// IR positioning camera
#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040
// ItsyBitsy RP2040 only exposes I2C1 pins on the board
DFRobotIRPositionEx dfrIRPos(Wire1);
#else // Pico et al does not have this limitation
//DFRobotIRPosition myDFRobotIRPosition;
DFRobotIRPositionEx dfrIRPos(Wire);
#endif

// Samco positioning
SamcoPositionEnhanced mySamco;

// operating modes
enum GunMode_e {
    GunMode_Init = -1,
    GunMode_Run = 0,
    GunMode_CalHoriz = 1,
    GunMode_CalVert = 2,
    GunMode_CalCenter = 3,
    GunMode_Pause = 4
};
GunMode_e gunMode = GunMode_Init;   // initial mode

enum PauseModeSelection_e {
    PauseMode_Calibrate = 0,
    PauseMode_ProfileSelect,
    PauseMode_Save,
    #ifndef USES_SWITCHES
    #ifdef USES_RUMBLE
    PauseMode_RumbleToggle,
    #endif // USES_RUMBLE
    #endif // USES_SWITCHES
    #ifdef USES_SOLENOID
    #ifndef USES_SWITCHES
    PauseMode_SolenoidToggle,
    #endif // USES_SWITCHES
    PauseMode_BurstFireToggle,
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
SamcoPreferences::Preferences_t SamcoPreferences::preferences = {
    profileData, ProfileCount, // profiles
    0, // default profile
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
constexpr uint32_t StateFlagsDtrReset = StateFlag_PrintSelectedProfile | StateFlag_PrintPreferences | StateFlag_PrintPreferencesStorage;

// state flags, see StateFlag_e
uint32_t stateFlags = StateFlagsDtrReset;

// internal addressable LEDs inits
#ifdef DOTSTAR_ENABLE
// note if the colours don't match then change the colour format from BGR
// apparently different lots of DotStars may have different colour ordering ¯\_(ツ)_/¯
Adafruit_DotStar dotstar(1, DOTSTAR_DATAPIN, DOTSTAR_CLOCKPIN, DOTSTAR_BGR);
#endif // DOTSTAR_ENABLE

#if defined(NEOPIXEL_PIN) && defined(CUSTOM_NEOPIXEL)
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel externPixel(1, customLEDpin, NEO_GRB + NEO_KHZ800);
#elif defined(CUSTOM_NEOPIXEL)
Adafruit_NeoPixel externPixel(customLEDcount, customLEDpin, NEO_GRB + NEO_KHZ800);
#elif defined(NEOPIXEL_PIN)
Adafruit_NeoPixel neopixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
#endif // CUSTOM_NEOPIXEL

// flash transport instance
#if defined(EXTERNAL_FLASH_USE_QSPI)
    Adafruit_FlashTransport_QSPI flashTransport;
#elif defined(EXTERNAL_FLASH_USE_SPI)
    Adafruit_FlashTransport_SPI flashTransport(EXTERNAL_FLASH_USE_CS, EXTERNAL_FLASH_USE_SPI);
#endif

#ifdef SAMCO_FLASH_ENABLE
// Adafruit_SPIFlashBase non-volatile storage
// flash instance
Adafruit_SPIFlashBase flash(&flashTransport);

static const char* NVRAMlabel = "Flash";

// flag to indicate if non-volatile storage is available
// this will enable in setup()
bool nvAvailable = false;
#endif // SAMCO_FLASH_ENABLE

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

#ifdef SAMCO_NO_HW_TIMER
// use the millis() or micros() counter instead
unsigned long irPosUpdateTime = 0;
// will set this to 1 when the IR position can update
unsigned int irPosUpdateTick = 0;

#define SAMCO_NO_HW_TIMER_UPDATE() NoHardwareTimerCamTickMillis()
//define SAMCO_NO_HW_TIMER_UPDATE() NoHardwareTimerCamTickMicros()

#else
// timer will set this to 1 when the IR position can update
volatile unsigned int irPosUpdateTick = 0;
#endif // SAMCO_NO_HW_TIMER

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

void setup() {
    Serial.begin(9600);   // 9600 = 1ms data transfer rates, default for MAMEHOOKER COM devices.
    Serial.setTimeout(0);
 
#ifdef SAMCO_FLASH_ENABLE
    // init flash and load saved preferences
    nvAvailable = flash.begin();
#else
#if defined(SAMCO_EEPROM_ENABLE) && defined(ARDUINO_ARCH_RP2040)
    // initialize EEPROM device. Arduino AVR has a 1k flash, so use that.
    EEPROM.begin(1024); 
#endif // SAMCO_EEPROM_ENABLE/RP2040
#endif // SAMCO_FLASH_ENABLE
    
    if(nvAvailable) {
        LoadPreferences();
    }

    // use values from preferences
    ApplyInitialPrefs();
 
    // We're setting our custom USB identifiers, as defined in the configuration area!
    #ifdef USE_TINYUSB
    TinyUSBDevice.setManufacturerDescriptor(MANUFACTURER_NAME);
    TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
    TinyUSBDevice.setID(DEVICE_VID, DEVICE_PID);
    #endif // USE_TINYUSB
 
    #ifdef USES_RUMBLE
        pinMode(rumblePin, OUTPUT);
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        pinMode(solenoidPin, OUTPUT);
    #endif // USES_SOLENOID
    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            pinMode(rumbleSwitch, INPUT_PULLUP);
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
            pinMode(solenoidSwitch, INPUT_PULLUP);
        #endif // USES_SOLENOID
        pinMode(autofireSwitch, INPUT_PULLUP);
    #endif // USES_SWITCHES

    #ifdef LED_ENABLE
        LedInit();
    #endif // LED_ENABLE

    #ifdef USES_ANALOG
        analogReadResolution(12);
        #ifdef USES_TEMP
        if(analogPinX >= 0 && analogPinY >= 0 && analogPinX != analogPinY &&
        analogPinX != tempPin && analogPinY != tempPin) {
        #else
        if(analogPinX >= 0 && analogPinY >= 0 && analogPinX != analogPinY) {
        #endif
            //pinMode(analogPinX, INPUT);
            //pinMode(analogPinY, INPUT);
            analogIsValid = true;
        } else {
            analogIsValid = false;
        }
    #endif // USES_ANALOG

#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040
    // ensure Wire1 SDA and SCL are correct for the ItsyBitsy RP2040
    Wire1.setSDA(2);
    Wire1.setSCL(3);
#endif
// Wire (0) should be defaulting to and using GPIO 4 & 5, but in case you need it:
// Wire.setSDA(4);
// Wire.setSCL(5);

#if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)
    // initialize buttons (on the main thread for single core systems)
    buttons.Begin();
#endif // ARDUINO_ARCH_RP2040

    // Start IR Camera with basic data format
    dfrIRPos.begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity);
    
#ifdef USE_TINYUSB
    // Initializing the USB devices chunk.
    TinyUSBDevices.begin(4);
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
    // Check if the initial profiles are blanked.
    if(profileData[selectedProfile].xScale == 0) {
        // SHIT, it's a first boot! Prompt to start calibration.
        Serial.println("Preferences data is empty!");
        SetMode(GunMode_CalCenter);
        Serial.println("Pull the trigger to start your first calibration!");
        while(!(buttons.pressedReleased == BtnMask_Trigger)) {
            buttons.Poll(1);
            buttons.Repeat();
        }
    } else {
        // this will turn off the DotStar/RGB LED and ensure proper transition to Run
        SetMode(GunMode_Run);
    }
}

#if defined(ARDUINO_ARCH_RP2040) && defined(DUAL_CORE)
void setup1()
{
    // Since we need this for the second core to activate, may as well give it something to do.
    // initialize buttons (on the second core)
    buttons.Begin();
}
#endif // ARDUINO_ARCH_RP2040

void startIrCamTimer(int frequencyHz)
{
#if defined(SAMCO_SAMD21)
    startTimerEx(&TC4->COUNT16, GCLK_CLKCTRL_ID_TC4_TC5, TC4_IRQn, frequencyHz);
#elif defined(SAMCO_SAMD51)
    startTimerEx(&TC3->COUNT16, TC3_GCLK_ID, TC3_IRQn, frequencyHz);
#elif defined(SAMCO_ATMEGA32U4)
    startTimer3(frequencyHz);
#elif defined(SAMCO_RP2040)
    rp2040EnablePWMTimer(0, frequencyHz);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, rp2040pwmIrq);
    irq_set_enabled(PWM_IRQ_WRAP, true);
#endif
}

#if defined(SAMCO_SAMD21)
void startTimerEx(TcCount16* ptc, uint16_t gclkCtrlId, IRQn_Type irqn, int frequencyHz)
{
    // use Generic clock generator 0
    GCLK->CLKCTRL.reg = (uint16_t)(GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | gclkCtrlId);
    while(GCLK->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    ptc->CTRLA.bit.ENABLE = 0;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    // Use the 16-bit timer
    ptc->CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    // Use match mode so that the timer counter resets when the count matches the compare register
    ptc->CTRLA.reg |= TC_CTRLA_WAVEGEN_MFRQ;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    // Set prescaler
    ptc->CTRLA.reg |= TIMER_TC_CTRLA_PRESCALER_DIV;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
    
    setTimerFrequency(ptc, frequencyHz);
    
    // Enable the compare interrupt
    ptc->INTENSET.reg = 0;
    ptc->INTENSET.bit.MC0 = 1;
    
    NVIC_EnableIRQ(irqn);
    
    ptc->CTRLA.bit.ENABLE = 1;
    while(ptc->STATUS.bit.SYNCBUSY == 1); // wait for sync
}

void TC4_Handler()
{
    // If this interrupt is due to the compare register matching the timer count
    if(TC4->COUNT16.INTFLAG.bit.MC0 == 1) {
        // clear interrupt
        TC4->COUNT16.INTFLAG.bit.MC0 = 1;

        irPosUpdateTick = 1;
    }
}
#endif // SAMCO_SAMD21

#if defined(SAMCO_SAMD51)
void startTimerEx(TcCount16* ptc, uint16_t gclkCtrlId, IRQn_Type irqn, int frequencyHz)
{
    // use Generic clock generator 0
    GCLK->PCHCTRL[gclkCtrlId].reg = GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN;
    while(GCLK->SYNCBUSY.reg); // wait for sync
    
    ptc->CTRLA.bit.ENABLE = 0;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    // Use the 16-bit timer
    ptc->CTRLA.reg |= TC_CTRLA_MODE_COUNT16;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    // Use match mode so that the timer counter resets when the count matches the compare register
    ptc->WAVE.bit.WAVEGEN = TC_WAVE_WAVEGEN_MFRQ;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    // Set prescaler
    ptc->CTRLA.reg |= TIMER_TC_CTRLA_PRESCALER_DIV;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
    
    setTimerFrequency(ptc, frequencyHz);
    
    // Enable the compare interrupt
    ptc->INTENSET.reg = 0;
    ptc->INTENSET.bit.MC0 = 1;
    
    NVIC_EnableIRQ(irqn);
    
    ptc->CTRLA.bit.ENABLE = 1;
    while(ptc->SYNCBUSY.bit.STATUS == 1); // wait for sync
}

void TC3_Handler()
{
    // If this interrupt is due to the compare register matching the timer count
    if(TC3->COUNT16.INTFLAG.bit.MC0 == 1) {
        // clear interrupt
        TC3->COUNT16.INTFLAG.bit.MC0 = 1;

        irPosUpdateTick = 1;
    }
}
#endif // SAMCO_SAMD51

#if defined(SAMCO_SAMD21) || defined(SAMCO_SAMD51)
void setTimerFrequency(TcCount16* ptc, int frequencyHz)
{
    int compareValue = (F_CPU / (TIMER_PRESCALER_DIV * frequencyHz));

    // Make sure the count is in a proportional position to where it was
    // to prevent any jitter or disconnect when changing the compare value.
    ptc->COUNT.reg = map(ptc->COUNT.reg, 0, ptc->CC[0].reg, 0, compareValue);
    ptc->CC[0].reg = compareValue;

#if defined(SAMCO_SAMD21)
    while(ptc->STATUS.bit.SYNCBUSY == 1);
#elif defined(SAMCO_SAMD51)
    while(ptc->SYNCBUSY.bit.STATUS == 1);
#endif
}
#endif

#ifdef SAMCO_ATMEGA32U4
void startTimer3(unsigned long frequencyHz)
{
    // disable comapre output mode
    TCCR3A = 0;
    
    //set the pre-scalar to 8 and set Clear on Compare
    TCCR3B = (1 << CS31) | (1 << WGM32); 
    
    // set compare value
    OCR3A = F_CPU / (8UL * frequencyHz);
    
    // enable Timer 3 Compare A interrupt
    TIMSK3 = 1 << OCIE3A;
}

// Timer3 compare A interrupt
ISR(TIMER3_COMPA_vect)
{
    irPosUpdateTick = 1;
}
#endif // SAMCO_ATMEGA32U4

#ifdef ARDUINO_ARCH_RP2040
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
#endif

#ifdef SAMCO_NO_HW_TIMER
void NoHardwareTimerCamTickMicros()
{
    unsigned long us = micros();
    if(us - irPosUpdateTime >= 1000000UL / IRCamUpdateRate) {
        irPosUpdateTime = us;
        irPosUpdateTick = 1;
    }
}

void NoHardwareTimerCamTickMillis()
{
    unsigned long ms = millis();
    if(ms - irPosUpdateTime >= (1000UL + (IRCamUpdateRate / 2)) / IRCamUpdateRate) {
        irPosUpdateTime = ms;
        irPosUpdateTick = 1;
    }
}
#endif // SAMCO_NO_HW_TIMER

#if defined(ARDUINO_ARCH_RP2040) && defined(DUAL_CORE)
void loop1()
{
    while(gunMode == GunMode_Run) {
        #ifdef USES_SWITCHES
            #ifdef USES_RUMBLE
                rumbleActive = !digitalRead(rumbleSwitch);
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                solenoidActive = !digitalRead(solenoidSwitch);
            #endif // USES_SOLENOID
            autofireActive = !digitalRead(autofireSwitch);
        #endif

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
                // For processing Start & Select.
                ButtonsPush();
            } else {   // This is if we've received a serial signal pulse in the last n millis.
                // For safety reasons, we're just using the second core for polling, and the main core for sending signals entirely. Too much a headache otherwise. =w='
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                                // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                             // Release button inputs.
                }
                // For processing start & select.
                ButtonsPush();
                SerialHandling();                                       // Process the force feedback.
            }
        #else
            if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                          // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                       // Releasing button inputs and sending stop signals to feedback devices.
            }
            // For processing the buttons tied to the keyboard.
            ButtonsPush();
        #endif // MAMEHOOKER

        #ifdef USES_ANALOG
            if(analogIsValid) {
                // We tie analog stick updates to the IR camera timer, so as to not overload USB bandwidth.
                // For some reason, tying it to irPosUpdateTick explicitly prevents mouse dropout.
                if(!analogStickPolled && irPosUpdateTick) {
                    AnalogStickPoll();
                    analogStickPolled = true;
                }
            }
        #endif // USES_ANALOG
        
        if(buttons.pressedReleased == EscapeKeyBtnMask) {
            SendEscapeKey();
        }

        if(holdToPause) {
            if((buttons.debounced == EnterPauseModeHoldBtnMask)
            && !lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                Serial.println("Started holding pause mode signal buttons!");
            } else if(pauseHoldStarted && (buttons.debounced != EnterPauseModeHoldBtnMask || lastSeen)) {
                pauseHoldStarted = false;
                Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");
            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > pauseHoldLength) {
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
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
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
                    digitalWrite(solenoidPin, LOW);
                    solenoidFirstShot = false;
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    digitalWrite(rumblePin, LOW);
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

void loop()
{
    #ifdef SAMCO_NO_HW_TIMER
        SAMCO_NO_HW_TIMER_UPDATE();
    #endif // SAMCO_NO_HW_TIMER
    
    // poll/update button states with 1ms interval so debounce mask is more effective
    buttons.Poll(1);
    buttons.Repeat();
    #ifdef MAMEHOOKER
    while(Serial.available()) {                             // So we can process serial requests while in Pause Mode.
        SerialProcessing();
    }
    #endif // MAMEHOOKER

    if(holdToPause && pauseHoldStarted) {
        #ifdef USES_RUMBLE
            analogWrite(rumblePin, rumbleIntensity);
            delay(300);
            digitalWrite(rumblePin, LOW);
        #endif // USES_RUMBLE
        while(buttons.debounced != 0) {
            // Should release the buttons to continue, pls.
            buttons.Poll(1);
            buttons.Repeat();
        }
        pauseHoldStarted = false;
        pauseModeSelection = PauseMode_Calibrate;
        pauseModeSelectingProfile = false;
    }

    switch(gunMode) {
        case GunMode_Pause:
            if(simpleMenu) {
                if(pauseModeSelectingProfile) {
                    if(buttons.pressedReleased == BtnMask_A) {
                        SetProfileSelection(false);
                    } else if(buttons.pressedReleased == BtnMask_B) {
                        SetProfileSelection(true);
                    } else if(buttons.pressedReleased == BtnMask_Trigger) {
                        SelectCalProfile(profileModeSelection);
                        pauseModeSelectingProfile = false;
                        pauseModeSelection = PauseMode_Calibrate;
                        Serial.print("Switched to profile: ");
                        Serial.println(profileDesc[selectedProfile].profileLabel);
                        Serial.println("Going back to the main menu...");
                        Serial.println("Selecting: Calibrate current profile");
                    } else if(buttons.pressedReleased == BtnMask_Reload) {
                        Serial.println("Exiting profile selection.");
                        pauseModeSelectingProfile = false;
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
                          Serial.print("Calibrating for current profile: ");
                          Serial.println(profileDesc[selectedProfile].profileLabel);
                          break;
                        case PauseMode_ProfileSelect:
                          Serial.println("Pick a profile!");
                          Serial.print("Current profile in use: ");
                          Serial.println(profileDesc[selectedProfile].profileLabel);
                          pauseModeSelectingProfile = true;
                          profileModeSelection = selectedProfile;
                          #ifdef LED_ENABLE
                              SetLedPackedColor(profileDesc[profileModeSelection].color);
                          #endif // LED_ENABLE
                          break;
                        case PauseMode_Save:
                          Serial.println("Saving...");
                          SavePreferences();
                          break;
                        #ifndef USES_SWITCHES
                        #ifdef USES_RUMBLE
                        case PauseMode_RumbleToggle:
                          Serial.println("Toggling rumble!");
                          RumbleToggle();
                          break;
                        #endif // USES_RUMBLE
                        #ifdef USES_SOLENOID
                        case PauseMode_SolenoidToggle:
                          Serial.println("Toggling solenoid!");
                          SolenoidToggle();
                          break;
                        #endif // USES_SOLENOID
                        #endif // USES_SWITCHES
                        #ifdef USES_SOLENOID
                        case PauseMode_BurstFireToggle:
                          Serial.println("Toggling solenoid burst firing!");
                          BurstFireToggle();
                          break;
                        #endif // USES_SOLENOID
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
                } else if(buttons.pressedReleased == BtnMask_Reload) {
                    Serial.println("Exiting pause mode...");
                    SetMode(GunMode_Run);
                }
                if(pauseExitHoldStarted &&
                (buttons.debounced & ExitPauseModeHoldBtnMask)) {
                    unsigned long t = millis();
                    if(t - pauseHoldStartstamp > (pauseHoldLength / 2)) {
                        Serial.println("Exiting pause mode via hold...");
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
                        #ifdef USES_RUMBLE
                            for(byte i = 0; i < 3; i++) {
                                analogWrite(rumblePin, rumbleIntensity);
                                delay(80);
                                digitalWrite(rumblePin, LOW);
                                delay(50);
                            }
                        #endif // USES_RUMBLE
                        while(buttons.debounced != 0) {
                            //lol
                            buttons.Poll(1);
                            buttons.Repeat();
                        }
                        pauseExitHoldStarted = false;
                    }
                } else if(buttons.debounced & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = true;
                    pauseHoldStartstamp = millis();
                } else if(buttons.pressedReleased & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = false;
                }
            } else if(buttons.pressedReleased == ExitPauseModeBtnMask) {
                SetMode(GunMode_Run);
            } else if(buttons.pressedReleased == BtnMask_Trigger) {
                SetMode(GunMode_CalCenter);
            } else if(buttons.pressedReleased == RunModeNormalBtnMask) {
                SetRunMode(RunMode_Normal);
            } else if(buttons.pressedReleased == RunModeAverageBtnMask) {
                SetRunMode(runMode == RunMode_Average ? RunMode_Average2 : RunMode_Average);
            } else if(buttons.pressedReleased == RunModeProcessingBtnMask) {
                SetRunMode(RunMode_Processing);
            } else if(buttons.pressedReleased == IRSensitivityUpBtnMask) {
                IncreaseIrSensitivity();
            } else if(buttons.pressedReleased == IRSensitivityDownBtnMask) {
                DecreaseIrSensitivity();
            } else if(buttons.pressedReleased == SaveBtnMask) {
                SavePreferences();
            } else if(buttons.pressedReleased == OffscreenButtonToggleBtnMask) {
                OffscreenToggle();
            } else if(buttons.pressedReleased == AutofireSpeedToggleBtnMask) {
                AutofireSpeedToggle();
            } else if(buttons.pressedReleased == EnterPauseModeProcessingBtnMask) { // A+B+C
                BurstFireToggle();
            #ifndef USES_SWITCHES // Builds without hardware switches needs software toggles
                #ifdef USES_RUMBLE
                    } else if(buttons.pressedReleased == RumbleToggleBtnMask) {
                        RumbleToggle();
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    } else if(buttons.pressedReleased == SolenoidToggleBtnMask) {
                        SolenoidToggle();
                #endif // USES_SOLENOID
            #endif // ifndef USES_TOGGLES
            } else {
                SelectCalProfileFromBtnMask(buttons.pressedReleased);
            }

            PrintResults();
            
            break;
        case GunMode_CalCenter:
            AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
            if(buttons.pressedReleased & CancelCalBtnMask) {
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
            if(buttons.pressedReleased & CancelCalBtnMask) {
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
            if(buttons.pressedReleased & CancelCalBtnMask) {
                CancelCalibration();
            } else {
                if(buttons.pressed & BtnMask_Trigger) {
                    ApplyCalToProfile();
                    if(justBooted) {
                        // If this is an initial calibration, save it immediately!
                        SetMode(GunMode_Pause);
                        SavePreferences();
                    }
                    SetMode(GunMode_Run);
                } else {
                    CalHoriz();
                }
            }
            break;
        default:
            /* ---------------------- LET'S GO --------------------------- */
            switch(runMode) {
            case RunMode_Processing:
                ExecRunModeProcessing();
                break;
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
        // If we're on RP2040, we offload the button polling to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)
        // ADDITIONS HERE: setting the state of our toggles, if used.
        #ifdef USES_SWITCHES
            #ifdef USES_RUMBLE
                rumbleActive = !digitalRead(rumbleSwitch);
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                solenoidActive = !digitalRead(solenoidSwitch);
            #endif // USES_SOLENOID
            autofireActive = !digitalRead(autofireSwitch);
        #endif // USES_SWITCHES
        #endif // DUAL_CORE

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
                // For processing the buttons tied to the keyboard.
                ButtonsPush();
            } else {  // Serial handoff mode
                if(bitRead(buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                            // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                         // Release button inputs.
                }
                ButtonsPush();                                      // For processing start & select.
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
            // For processing the buttons tied to the keyboard.
            ButtonsPush();
        #endif // MAMEHOOKER
        #endif // DUAL_CORE

        #ifdef SAMCO_NO_HW_TIMER
            SAMCO_NO_HW_TIMER_UPDATE();
        #endif // SAMCO_NO_HW_TIMER

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
            #endif // USES_ANALOG
        }

        // If using RP2040, we offload the button processing to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)

        #ifdef USES_ANALOG
            if(analogIsValid) {
                // We tie analog stick updates to the IR camera timer, so as to not overload USB bandwidth.
                if(!analogStickPolled && irPosUpdateTick) {
                    AnalogStickPoll();
                    analogStickPolled = true;
                }
            }
        #endif // USES_ANALOG

        if(buttons.pressedReleased == EscapeKeyBtnMask) {
            SendEscapeKey();
        }

        if(holdToPause) {
            if((buttons.debounced == EnterPauseModeHoldBtnMask)
            && !lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                Serial.println("Started holding pause mode signal buttons!");
            } else if(pauseHoldStarted && (buttons.debounced != EnterPauseModeHoldBtnMask || lastSeen)) {
                pauseHoldStarted = false;
                Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");
            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > pauseHoldLength) {
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
        if(buttons.pressedReleased & EnterPauseModeProcessingBtnMask) {
            SetMode(GunMode_Pause);
            return;
        }

        #ifdef SAMCO_NO_HW_TIMER
            SAMCO_NO_HW_TIMER_UPDATE();
        #endif // SAMCO_NO_HW_TIMER
        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
        
            int error = dfrIRPos.basicAtomic(DFRobotIRPositionEx::Retry_2);
            if(error == DFRobotIRPositionEx::Error_Success) {
                mySamco.begin(dfrIRPos.xPositions(), dfrIRPos.yPositions(), dfrIRPos.seen(), MouseMaxX / 2, MouseMaxY / 2);
                UpdateLastSeen();
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
            } else if(error == DFRobotIRPositionEx::Error_IICerror) {
                Serial.println("Device not available!");
            }
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
    int error = dfrIRPos.basicAtomic(DFRobotIRPositionEx::Retry_2);
    if(error == DFRobotIRPositionEx::Error_Success) {
        mySamco.begin(dfrIRPos.xPositions(), dfrIRPos.yPositions(), dfrIRPos.seen(), xCenter, yCenter);
#ifdef EXTRA_POS_GLITCH_FILTER
        if((abs(mySamco.X() - finalX) > BadMoveThreshold || abs(mySamco.Y() - finalY) > BadMoveThreshold) && badFinalTick < BadMoveCountThreshold) {
            ++badFinalTick;
        } else {
            if(badFinalTick) {
                badFinalCount++;
                badFinalTick = 0;
            }
            finalX = mySamco.X();
            finalY = mySamco.Y();
        }
#else
        finalX = mySamco.x();
        finalY = mySamco.y();
#endif // EXTRA_POS_GLITCH_FILTER

        #ifdef MAMEHOOKER
        if(!serialMode) {
            UpdateLastSeen();
        }
        #else
        UpdateLastSeen();
        #endif // MAMEHOOKER
     
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
        #ifdef LED_ENABLE
        if(!lastSeen && mySamco.seen()) {
            LedOff();
        } else if(lastSeen && !mySamco.seen()) {
            SetLedPackedColor(IRSeen0Color);
        }
        #endif // LED_ENABLE
        lastSeen = mySamco.seen();
    }
}

void TriggerFire()                                               // If we pressed the trigger,
{
    if(!buttons.offScreen &&                                     // Check if the X or Y axis is in the screen's boundaries, i.e. "off screen".
    !offscreenBShot) {                                           // And only as long as we haven't fired an off-screen shot,
        if(!buttonPressed) {
            if(buttons.analogOutput) {
                Gamepad16.press(0);                              // No reason to handle this ourselves here, but eh.
            } else {
                AbsMouse5.press(MOUSE_LEFT);                     // We're handling the trigger button press ourselves for a reason.
            }
            buttonPressed = true;                                // Set this so we won't spam a repeat press event again.
        }
        if(!bitRead(buttons.debounced, 3) &&                     // Is the trigger being pulled WITHOUT pressing Start & Select?
        !bitRead(buttons.debounced, 4)) {
            #ifdef USES_SOLENOID
                if(solenoidActive) {                             // (Only activate when the solenoid switch is on!)
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
                            if(autofireActive) {          // If we are in auto mode,
                                solenoidFirstShot = false;          // Immediately set this bit off!
                            }
                        }
                    // Else, these below are all if we've been holding the trigger.
                    } else if(burstFiring) {  // If we're in a burst firing sequence,
                        BurstFire();                                // Process it.
                    } else if(autofireActive &&  // Else, if we've been holding the trigger, is the autofire switch active?
                    !burstFireActive) {          // (WITHOUT burst firing enabled)
                        if(digitalRead(solenoidPin)) {              // Is the solenoid engaged?
                            SolenoidActivation(solenoidFastInterval); // If so, immediately pass the autofire faster interval to solenoid method
                        } else {                                    // Or if it's not,
                            SolenoidActivation(solenoidFastInterval * autofireWaitFactor); // We're holding it for longer.
                        }
                    } else if(solenoidFirstShot) {                  // If we aren't in autofire mode, are we waiting for the initial shot timer still?
                        if(digitalRead(solenoidPin)) {              // If so, are we still engaged? We need to let it go normally, but maintain the single shot flag.
                            unsigned long currentMillis = millis(); // Initialize timer to check if we've passed it.
                            if(currentMillis - previousMillisSol >= solenoidNormalInterval) { // If we finally surpassed the wait threshold...
                                digitalWrite(solenoidPin, LOW);     // Let it go.
                            }
                        } else {                                    // We're waiting on the extended wait before repeating in single shot mode.
                            unsigned long currentMillis = millis(); // Initialize timer.
                            if(currentMillis - previousMillisSol >= solenoidLongInterval) { // If we finally surpassed the LONGER wait threshold...
                                solenoidFirstShot = false;          // We're gonna turn this off so we don't have to pass through this check anymore.
                                SolenoidActivation(solenoidNormalInterval); // Process it now.
                            }
                        }
                    } else if(!burstFireActive) {                   // if we don't have the single shot wait flag on (holding the trigger w/out autofire)
                        if(digitalRead(solenoidPin)) {              // Are we engaged right now?
                            SolenoidActivation(solenoidNormalInterval); // Turn it off with this timer.
                        } else {                                    // Or we're not engaged.
                            SolenoidActivation(solenoidNormalInterval * 2); // So hold it that way for twice the normal timer.
                        }
                    }
                } // We ain't using the solenoid, so just ignore all that.
                #elif RUMBLE_FF
                    if(rumbleActive) {
                        // TODO: actually make stuff here.
                    } // We ain't using the motor, so just ignore all that.
                #endif // USES_SOLENOID/RUMBLE_FF
            }
            #ifdef USES_RUMBLE
                #ifndef RUMBLE_FF
                    if(rumbleActive &&                     // Is rumble activated,
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
                        Gamepad16.press(1);
                    } else {
                        AbsMouse5.press(MOUSE_RIGHT);
                    }
                    offscreenBShot = true;                     // Mark we pressed the right button via offscreen shot mode,
                    buttonPressed = true;                      // Mark so we're not spamming these press events.
                } else {  // Or if we're not in offscreen button mode,
                    if(buttons.analogOutput) {
                        Gamepad16.press(0);
                    } else {
                        AbsMouse5.press(MOUSE_LEFT);
                    }
                    buttonPressed = true;                      // Mark so we're not spamming these press events.
                }
            }
            #ifdef USES_RUMBLE
                #ifndef RUMBLE_FF
                    if(rumbleActive) {  // Only activate if the rumble switch is enabled!
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
                } else if(digitalRead(solenoidPin) &&
                !burstFireActive) {                               // If the solenoid is engaged, since we're not shooting the screen, shut off the solenoid a'la an idle cycle
                    unsigned long currentMillis = millis();         // Calibrate current time
                    if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                        previousMillisSol = currentMillis;          // Timer calibration, yawn.
                        digitalWrite(solenoidPin, LOW);             // Turn it off already, dangit.
                    }
            #elif RUMBLE_FF
                } else if(rumbleHappening && !burstFireActive) {
                    // TODO: actually implement
            #endif // USES_SOLENOID
        }
    }
    triggerHeld = true;                                     // Signal that we've started pulling the trigger this poll cycle.
}

void TriggerNotFire()                                       // ...Or we just didn't press the trigger this cycle.   
{
    triggerHeld = false;                                    // Disable the holding function
    if(buttonPressed) {
        if(offscreenBShot) {                                // If we fired off screen with the offscreenButton set,
            if(buttons.analogOutput) {
                Gamepad16.release(1);
            } else {
                AbsMouse5.release(MOUSE_RIGHT);             // We were pressing the right mouse, so release that.
            }
            offscreenBShot = false;
            buttonPressed = false;
        } else {                                            // Or if not,
            if(buttons.analogOutput) {
                Gamepad16.release(0);
            } else {
                AbsMouse5.release(MOUSE_LEFT);              // We were pressing the left mouse, so release that instead.
            }
            buttonPressed = false;
        }
    }
    #ifdef USES_SOLENOID
        if(solenoidActive) {  // Has the solenoid remain engaged this cycle?
            if(burstFiring) {    // Are we in a burst fire command?
                BurstFire();                                    // Continue processing it.
            } else if(!burstFireActive) { // Else, we're just processing a normal/rapid fire shot.
                solenoidFirstShot = false;                      // Make sure this is unset to prevent "sticking" in single shot mode!
                unsigned long currentMillis = millis();         // Start the timer
                if(currentMillis - previousMillisSol >= solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                    previousMillisSol = currentMillis;          // Timer calibration, yawn.
                    digitalWrite(solenoidPin, LOW);             // Make sure to turn it off.
                }
            }
        }
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        if(rumbleHappening) {                                   // Are we currently in a rumble command? (Implicitly needs rumbleActive)
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
    unsigned int analogValueX = analogRead(analogPinX);
    unsigned int analogValueY = analogRead(analogPinY);
    // Check if we're in the deadzone or not (roughly 15% on each axis)
    if(analogValueX > 1500 && analogValueX < 2500) {
        analogValueX = 2048;
    }
    if(analogValueY > 1500 && analogValueY < 2500) {
        analogValueY = 2048;
    }
    Gamepad16.moveStick(analogValueX, analogValueY);
}
#endif // USES_ANALOG

#ifdef MAMEHOOKER
void SerialProcessing()                                         // Reading the input from the serial buffer.
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
                  digitalWrite(rumblePin, LOW);                          // Turn off stale rumbling from normal gun mode.
                  rumbleHappened = false;
                  rumbleHappening = false;
              #endif // USES_RUMBLE
              #ifdef USES_SOLENOID
                  digitalWrite(solenoidPin, LOW);                        // Turn off stale solenoid-ing from normal gun mode.
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
        case 'M':
          serialInput = Serial.read();                               // Read the second bit.
          if(serialInput == '1') {             // Is it M1? (Gun "offscreen mode" set)?
              serialInput = Serial.read();                           // Nomf the padding bit.
              serialInput = Serial.read();                           // Read the next.
              if(serialInput == '2') {         // Is it the offscreen button mode bit?
                  offscreenButtonSerial = true;                      // Set that if so.
              }
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
                  digitalWrite(rumblePin, LOW);
                  serialRumbPulseStage = 0;
                  serialRumbPulses = 0;
                  serialRumbPulsesLast = 0;
              #endif // USES_RUMBLE
              #ifdef USES_SOLENOID
                  digitalWrite(solenoidPin, LOW);
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
              // Toggle Processing/Run Mode
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
                    SetMode(GunMode_Run);
                } else {
                    Serial.println("Entering Processing Sketch mode...");
                    SetRunMode(RunMode_Processing);
                    SetMode(GunMode_Run);
                    loop();
                }
                break;
              // Toggle Pause/Run Mode
              case 'P':
                if(gunMode != GunMode_Run) {
                    Serial.println("Exiting back to normal run mode...");
                    SetMode(GunMode_Run);
                } else {
                    Serial.println("Entering pause mode...");
                    buttons.ReportDisable();
                    SetMode(GunMode_Pause);
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
                    Serial.print("Now calibrating selected profile: ");
                    Serial.println(profileDesc[selectedProfile].profileLabel);
                    SetMode(GunMode_CalCenter);
                } else {
                    // Eh, just set the current profile to calibrate.
                    Serial.print("Called for calibration without valid profile number, so calibrating current profile: ");
                    Serial.println(profileDesc[selectedProfile].profileLabel);
                    SetMode(GunMode_CalCenter);
                }
                // Force the mouse to center, in case the signal gets dropped by the next camera update
                AbsMouse5.move(MouseMaxX / 2, MouseMaxY / 2);
                break;
              // Save current profile
              case 'S':
                Serial.println("Saving preferences...");
                // We actually need to flick to Pause Mode to save configs.
                bool wasPaused;
                if(gunMode != GunMode_Pause) {
                    wasPaused = false;
                    SetMode(GunMode_Pause);
                } else {
                    wasPaused = true;
                }
                SetMode(GunMode_Pause);
                SavePreferences();
                if(!wasPaused) {
                    SetMode(GunMode_Run);
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
                UpdateBindings();
                break;
              // Switch profiles
              default:
                if(serialInput == '1' || serialInput == '2' ||
                   serialInput == '3' || serialInput == '4') {
                    byte profileNum = serialInput - '0';
                    SelectCalProfile(profileNum-1);
                    Serial.print("Switching to selected profile: ");
                    Serial.println(profileDesc[selectedProfile].profileLabel);
                } else {
                    Serial.println("SERIALREAD: Internal setting command detected, but no valid option found!");
                    Serial.println("Internally recognized commands are:");
                    Serial.println("A(nalog) / T(est) / R(emap)1/2/3/4 / P(ause) / C(alibrate)[1/2/3/4] / S(ave) / [(profile) 1/2/3/4]");
                }
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

void SerialHandling()                                              // Where we let the serial in stream handle things.
{   // As far as I know, DemulShooter/MAMEHOOKER handles all the timing and safety for us.
    // So all we have to do is just read and process what it sends us at face value.
    // The only exception is rumble PULSE bits, where we actually do need to calculate that ourselves.

    // Further MY OWN goals? FURTHER THIS, MOTHERFUCKER:
    #ifdef USES_SOLENOID
      if(solenoidActive) {
          if(bitRead(serialQueue, 0)) {                             // If the solenoid digital bit is on,
              digitalWrite(solenoidPin, HIGH);                           // Make it go!
          } else if(bitRead(serialQueue, 1)) {                      // if the solenoid pulse bit is on,
              if(!serialSolPulsesLast) {                            // Have we started pulsing?
                  analogWrite(solenoidPin, 178);                         // Start pulsing it on!
                  serialSolPulseOn = true;                               // Set that the pulse cycle is in on.
                  serialSolPulsesLast = 1;                               // Start the sequence.
                  serialSolPulses++;                                     // Cheating and scooting the pulses bit up.
              } else if(serialSolPulsesLast <= serialSolPulses) {   // Have we met the pulses quota?
                  unsigned long currentMillis = millis();                // Calibrate timer.
                  if(currentMillis - serialSolPulsesLastUpdate > serialSolPulsesLength) { // Have we passed the set interval length between stages?
                      if(serialSolPulseOn) {                        // If we're currently pulsing on,
                          analogWrite(solenoidPin, 122);                 // Start pulsing it off.
                          serialSolPulseOn = false;                      // Set that we're in off.
                          serialSolPulsesLast++;                         // Iterate that we've done a pulse cycle,
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      } else {                                      // Or if we're pulsing off,
                          analogWrite(solenoidPin, 178);                 // Start pulsing it on.
                          serialSolPulseOn = true;                       // Set that we're in on.
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      }
                  }
              } else {  // let the armature smoothly sink loose for one more pulse length before snapping it shut off.
                  unsigned long currentMillis = millis();                // Calibrate timer.
                  if(currentMillis - serialSolPulsesLastUpdate > serialSolPulsesLength) { // Have we paassed the set interval length between stages?
                      digitalWrite(solenoidPin, LOW);                    // Finally shut it off for good.
                      bitClear(serialQueue, 1);                          // Set the pulse bit as off.
                  }
              }
          } else {  // or if it's not,
              digitalWrite(solenoidPin, LOW);                            // turn it off!
          }
      } else {
          digitalWrite(solenoidPin, LOW);
      }
  #endif // USES_SOLENOID
  #ifdef USES_RUMBLE
      if(rumbleActive) {
          if(bitRead(serialQueue, 2)) {                             // Is the rumble on bit set?
              digitalWrite(rumblePin, HIGH);                             // turn/keep it on.
              //bitClear(serialQueue, 3);
          } else if(bitRead(serialQueue, 3)) {                      // or if the rumble pulse bit is set,
              if(!serialRumbPulsesLast) {                           // is the pulses last bit set to off?
                  analogWrite(rumblePin, 75);                            // we're starting fresh, so use the stage 0 value.
                  serialRumbPulseStage = 0;                              // Set that we're at stage 0.
                  serialRumbPulsesLast = 1;                              // Set that we've started a pulse rumble command, and start counting how many pulses we're doing.
              } else if(serialRumbPulsesLast <= serialRumbPulses) { // Have we exceeded the set amount of pulses the rumble command called for?
                  unsigned long currentMillis = millis();                // Calibrate the timer.
                  if(currentMillis - serialRumbPulsesLastUpdate > serialRumbPulsesLength) { // have we waited enough time between pulse stages?
                      switch(serialRumbPulseStage) {                     // If so, let's start processing.
                          case 0:                                        // Basically, each case
                              analogWrite(rumblePin, 255);               // bumps up the intensity, (lowest to rising)
                              serialRumbPulseStage++;                    // and increments the stage of the pulse.
                              serialRumbPulsesLastUpdate = millis();     // and timestamps when we've had updated this last.
                              break;                                     // Then quits the switch.
                          case 1:
                              analogWrite(rumblePin, 120);               // (rising to peak)
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          case 2:
                              analogWrite(rumblePin, 75);                // (peak to falling,)
                              serialRumbPulseStage = 0;
                              serialRumbPulsesLast++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                      }
                  }
              } else {                                              // ...or the pulses count is complete.
                  digitalWrite(rumblePin, LOW);                          // turn off the motor,
                  bitClear(serialQueue, 3);                              // and set the rumble pulses bit off, now that we've completed it.
              }
          } else {                                                  // ...or we're being told to turn it off outright.
              digitalWrite(rumblePin, LOW);                              // Do that then.
          }
      } else {
          digitalWrite(rumblePin, LOW);
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

void ButtonsPush()
{
    // TinyUSB Devices' handling should be slightly more stable than the old Keyboard library across the board,
    // so we only handle Start and Select differently to not conflict with the Pause Mode button combo.

    if(bitRead(buttons.debounced, 3) && !bitRead(buttons.debounced, 9)) { // Only if not holding Button C/Reload
        if(!bitRead(buttonsHeld, 0)) {
            if(buttons.analogOutput) {
                Gamepad16.press(5);
            } else {
                Keyboard.press(playerStartBtn);
            }
            bitSet(buttonsHeld, 0);
        }
    } else if(!bitRead(buttons.debounced, 3) && bitRead(buttonsHeld, 0)) {
        if(buttons.analogOutput) {
            Gamepad16.release(5);
        } else {
            Keyboard.release(playerStartBtn);
        }
        bitClear(buttonsHeld, 0);
    }

    if(bitRead(buttons.debounced, 4) && !bitRead(buttons.debounced, 9)) { // Only if not holding Button C/Reload
        if(!bitRead(buttonsHeld, 1)) {
            if(buttons.analogOutput) {
                Gamepad16.press(6);
            } else {
                Keyboard.press(playerSelectBtn);
            }
            bitSet(buttonsHeld, 1);
        }
    } else if(!bitRead(buttons.debounced, 4) && bitRead(buttonsHeld, 1)) {
        if(buttons.analogOutput) {
            Gamepad16.release(6);
        } else {
            Keyboard.release(playerSelectBtn);
        }
        bitClear(buttonsHeld, 1);
    }
}

void SendEscapeKey()
{
    Keyboard.press(KEY_ESC);
    delay(20);  // wait a bit so it registers on the PC.
    Keyboard.release(KEY_ESC);
}

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

void SetPauseModeSelection(bool isIncrement)
{
    if(isIncrement) {
        if(pauseModeSelection == PauseMode_EscapeSignal) {
            pauseModeSelection = PauseMode_Calibrate;
        } else {
            pauseModeSelection++;
        }
    } else {
        if(pauseModeSelection == PauseMode_Calibrate) {
            pauseModeSelection = PauseMode_EscapeSignal;
        } else {
            pauseModeSelection--;
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
        #ifndef USES_SWITCHES
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
        #endif // USES_SWITCHES
        #ifdef USES_SOLENOID
        case PauseMode_BurstFireToggle:
          Serial.println("Selecting: Toggle burst-firing mode");
          #ifdef LED_ENABLE
              LedUpdate(0,255,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
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

void PrintCalInterval()
{
    if(millis() - lastPrintMillis < 100) {
        return;
    }
    PrintCal();
    lastPrintMillis = millis();
}

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

void PrintRunMode()
{
    if(runMode < RunMode_Count) {
        Serial.print("Mode: ");
        Serial.println(RunModeLabels[runMode]);
    }
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
    Serial.println(profileDesc[SamcoPreferences::preferences.profile].profileLabel);
    
    Serial.println("Profiles:");
    for(unsigned int i = 0; i < SamcoPreferences::preferences.profileCount; ++i) {
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
        if(rumbleActive) {
            Serial.println("True");
        } else {
            Serial.println("False");
        }
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        Serial.print("Solenoid enabled: ");
        if(solenoidActive) {
            Serial.println("True");
            Serial.print("Rapid fire enabled: ");
            if(autofireActive) {
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
    Serial.print(G4ALL_VERSION, 1);
    Serial.print(" - ");
    Serial.println(G4ALL_CODENAME);
}

void LoadPreferences()
{
    if(!nvAvailable) {
        return;
    }

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Load(flash);
#else
    nvPrefsError = samcoPreferences.Load();
#endif // SAMCO_FLASH_ENABLE
    VerifyPreferences();
}

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
    if(SamcoPreferences::preferences.profile >= ProfileCount) {
        SamcoPreferences::preferences.profile = (uint8_t)selectedProfile;
    }
}

// Apply initial preferences, intended to be called only in setup() after LoadPreferences()
// this will apply the preferences data as the initial values
void ApplyInitialPrefs()
{   
    // if default profile is valid then use it
    if(SamcoPreferences::preferences.profile < ProfileCount) {
        // note, just set the value here not call the function to do the set
        selectedProfile = SamcoPreferences::preferences.profile;

        // set the current IR camera sensitivity
        if(profileData[selectedProfile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max) {
            irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)profileData[selectedProfile].irSensitivity;
        }

        // set the run mode
        if(profileData[selectedProfile].runMode < RunMode_Count) {
            runMode = (RunMode_e)profileData[selectedProfile].runMode;
        }
    }
}

void SavePreferences()
{
    if(!nvAvailable || !(stateFlags & StateFlag_SavePreferencesEn)) {
        return;
    }

    // Only allow one write per pause state until something changes.
    // Extra protection to ensure the same data can't write a bunch of times.
    stateFlags &= ~StateFlag_SavePreferencesEn;
    
    // use selected profile as the default
    SamcoPreferences::preferences.profile = (uint8_t)selectedProfile;

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Save(flash);
#else
    nvPrefsError = SamcoPreferences::Save();
#endif // SAMCO_FLASH_ENABLE
    if(nvPrefsError == SamcoPreferences::Error_Success) {
        Serial.print("Settings saved to ");
        Serial.println(NVRAMlabel);
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
        dfrIRPos.sensitivityLevel(irSensitivity);
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
    // return to pause mode
    SetMode(GunMode_Pause);
}

void PrintSelectedProfile()
{
    Serial.print("Profile: ");
    Serial.println(profileDesc[selectedProfile].profileLabel);
}

// select a profile
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
// (re)initializes RGB LEDs.
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
        if(customLEDpin >= 0) {
            externPixel.begin();
        }
    #endif // CUSTOM_NEOPIXEL

    #ifdef FOURPIN_LED
    if(PinR < 0 || PinG < 0 || PinB < 0) {
        Serial.println("RGB values not valid! Disabling four pin access.");
        ledIsValid = false;
    } else {
        pinMode(PinR, OUTPUT);
        pinMode(PinG, OUTPUT);
        pinMode(PinB, OUTPUT);
        ledIsValid = true;
    }
    #endif // FOURPIN_LED
    LedUpdate(255, 0, 0);
}

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
    if(customLEDpin >= 0) {
        externPixel.fill(0, Adafruit_NeoPixel::gamma32(color & 0x00FFFFFF));
        externPixel.show();
    }
#endif // CUSTOM_NEOPIXEL
 #ifdef FOURPIN_LED
    if(ledIsValid) {
        byte r = highByte(color >> 8);
        byte g = highByte(color);
        byte b = lowByte(color);
        if(commonAnode) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        analogWrite(PinR, r);
        analogWrite(PinG, g);
        analogWrite(PinB, b);
    }
#endif // FOURPIN_LED
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
        if(customLEDpin >= 0) {
            for(byte i = 0; i < customLEDcount; i++) {
                externPixel.setPixelColor(i, r, g, b);
            }
            externPixel.show();
        }
    #endif // CUSTOM_NEOPIXEL
    #ifdef FOURPIN_LED
        if(ledIsValid) {
            if(commonAnode) {
                r = ~r;
                g = ~g;
                b = ~b;
            }
            analogWrite(PinR, r);
            analogWrite(PinG, g);
            analogWrite(PinB, b);
        }
    #endif // FOURPIN_LED
}

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

// ADDITIONS HERE:
void OffscreenToggle()
{
    offscreenButton = !offscreenButton;
    if(offscreenButton) {                                         // If we turned ON this mode,
        Serial.println("Enabled Offscreen Button!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Ghost_white);            // Set a color,
        #endif // LED_ENABLE
        #ifdef USES_RUMBLE
            digitalWrite(rumblePin, HIGH);                        // Set rumble on
            delay(125);                                           // For this long,
            digitalWrite(rumblePin, LOW);                         // Then flick it off,
            delay(150);                                           // wait a little,
            digitalWrite(rumblePin, HIGH);                        // Flick it back on
            delay(200);                                           // For a bit,
            digitalWrite(rumblePin, LOW);                         // and then turn it off,
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

void AutofireSpeedToggle()
{
    switch (autofireWaitFactor) {
        case 2:
            autofireWaitFactor = 3;
            Serial.println("Autofire speed level 2.");
            break;
        case 3:
            autofireWaitFactor = 4;
            Serial.println("Autofire speed level 3.");
            break;
        case 4:
            autofireWaitFactor = 2;
            Serial.println("Autofire speed level 1.");
            break;
    }
    #ifdef LED_ENABLE
        SetLedPackedColor(WikiColor::Magenta);                    // Set a color,
    #endif // LED_ENABLE
    #ifdef USES_SOLENOID
        digitalWrite(solenoidPin, HIGH);                          // And demonstrate the new autofire factor five times!
        delay(solenoidFastInterval);
        digitalWrite(solenoidPin, LOW);
        delay(solenoidFastInterval * autofireWaitFactor);
        digitalWrite(solenoidPin, HIGH);
        delay(solenoidFastInterval);
        digitalWrite(solenoidPin, LOW);
        delay(solenoidFastInterval * autofireWaitFactor);
        digitalWrite(solenoidPin, HIGH);
        delay(solenoidFastInterval);
        digitalWrite(solenoidPin, LOW);
        delay(solenoidFastInterval * autofireWaitFactor);
        digitalWrite(solenoidPin, HIGH);
        delay(solenoidFastInterval);
        digitalWrite(solenoidPin, LOW);
        delay(solenoidFastInterval * autofireWaitFactor);
        digitalWrite(solenoidPin, HIGH);
        delay(solenoidFastInterval);
        digitalWrite(solenoidPin, LOW);
    #endif // USES_SOLENOID
    #ifdef LED_ENABLE
        SetLedPackedColor(profileDesc[selectedProfile].color);    // And reset the LED back to pause mode color
    #endif // LED_ENABLE
    return;
}

void BurstFireToggle()
{
    burstFireActive = !burstFireActive;                           // Toggle burst fire mode.
    if(burstFireActive) {  // Did we flick it on?
        Serial.println("Burst firing enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Orange);
        #endif
        #ifdef USES_SOLENOID
            digitalWrite(solenoidPin, HIGH);                      // Demonstrate it by flicking the solenoid on/off three times!
            delay(solenoidFastInterval);                          // (at a fixed rate to distinguish it from autofire speed toggles)
            digitalWrite(solenoidPin, LOW);
            delay(solenoidFastInterval * 2);
            digitalWrite(solenoidPin, HIGH);
            delay(solenoidFastInterval);
            digitalWrite(solenoidPin, LOW);
            delay(solenoidFastInterval * 2);
            digitalWrite(solenoidPin, HIGH);
            delay(solenoidFastInterval);
            digitalWrite(solenoidPin, LOW);
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

#ifndef USES_SWITCHES
#ifdef USES_RUMBLE
void RumbleToggle()
{
    rumbleActive = !rumbleActive;                                 // Toggle
    if(rumbleActive) {                                            // If we turned ON this mode,
        Serial.println("Rumble enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(rumblePin, HIGH);                            // Pulse the motor on to notify the user,
        delay(300);                                               // Hold that,
        digitalWrite(rumblePin, LOW);                             // Then turn off,
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
void SolenoidToggle()
{
    solenoidActive = !solenoidActive;                             // Toggle
    if(solenoidActive) {                                          // If we turned ON this mode,
        Serial.println("Solenoid enabled!");
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(solenoidPin, HIGH);                          // Engage the solenoid on to notify the user,
        delay(300);                                               // Hold it that way for a bit,
        digitalWrite(solenoidPin, LOW);                           // Release it,
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
#endif // USES_SWITCHES

#ifdef USES_SOLENOID
void SolenoidActivation(int solenoidFinalInterval)
{
    if(solenoidFirstShot) {                                       // If this is the first time we're shooting, it's probably safe to shoot regardless of temps.
        unsigned long currentMillis = millis();                   // Initialize timer.
        previousMillisSol = currentMillis;                        // Calibrate the timer for future calcs.
        digitalWrite(solenoidPin, HIGH);                          // Since we're shooting the first time, just turn it on aaaaand fire.
        return;                                                   // We're done here now.
    }
    #ifdef USES_TEMP                                              // *If the build calls for a TMP36 temperature sensor,
        tempSensor = analogRead(tempPin);                         // Read the temp sensor.
        tempSensor = (tempSensor * 0.32226563) + 0.5;             // Multiply for accurate Celsius reading from 3.3v signal. (rounded up)
        #ifdef PRINT_VERBOSE
            Serial.print("Current Temp near solenoid: ");
            Serial.print(tempSensor);
            Serial.println("*C");
        #endif
        if(tempSensor < tempNormal) {                             // Are we at (relatively) normal operating temps?
            unsigned long currentMillis = millis();               // Start the timer.
            if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
                previousMillisSol = currentMillis;                // Since we've waited long enough, calibrate the timer
                digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
                return;                                           // Aaaand we're done here.
            } else {                                              // If we pass the temp check but fail the timer check, we're here too quick.
                return;                                           // Get out of here, speedy mc loserpants.
            }
        } else if(tempSensor < tempWarning) {                     // If we failed the room temp check, are we beneath the shutoff threshold?
            if(digitalRead(solenoidPin)) {                        // Is the valve being pulled now?
                unsigned long currentMillis = millis();           // If so, we should release it on the shorter timer.
                if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // We're holding it high for the requested time.
                    previousMillisSol = currentMillis;            // Timer calibrate
                    digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // Flip, flop.
                    return;                                       // Yay.
                } else {                                          // OR, Passed the temp check, STILL here too quick.
                    return;                                       // Yeeted.
                }
            } else {                                              // The solenoid's probably off, not on right now. So that means we should wait a bit longer to fire again.
                unsigned long currentMillis = millis();           // Timerrrrrr.
                if(currentMillis - previousMillisSol >= solenoidWarningInterval) { // We're keeping it low for a bit longer, to keep temps stable. Try to give it a bit of time to cool down before we go again.
                    previousMillisSol = currentMillis;            // Since we've waited long enough, calibrate the timer
                    digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
                    return;                                       // Doneso.
                } else {                                          // OR, We passed the temp check but STILL got here too quick.
                    return;                                       // Loser.
                }
            }
        } else {                                                  // Failed both temp checks, so obviously it's not safe to fire.
            #ifdef PRINT_VERBOSE
                Serial.println("Solenoid over safety threshold; not activating!");
            #endif
            digitalWrite(solenoidPin, LOW);                       // Make sure it's off if we're this dangerously close to the sun.
            return;                                               // Go away.
        }
    #else                                                         // *The shorter version of this function if we're not using a temp sensor.
        unsigned long currentMillis = millis();                   // Start the timer.
        if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
            previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
            digitalWrite(solenoidPin, !digitalRead(solenoidPin)); // run the solenoid into the state we've just inverted it to.
            return;                                               // Aaaand we're done here.
        } else {                                                  // If we failed the timer check, we're here too quick.
            return;                                               // Get out of here, speedy mc loserpants.
        }
    #endif
}
#endif // USES_SOLENOID

#ifdef USES_RUMBLE
void RumbleActivation()
{
    if(rumbleHappening) {                                         // Are we in a rumble command rn?
        unsigned long currentMillis = millis();                   // Calibrate a timer to set how long we've been rumbling.
        if(currentMillis - previousMillisRumble >= rumbleInterval) { // If we've been waiting long enough for this whole rumble command,
            digitalWrite(rumblePin, LOW);                         // Make sure the rumble is OFF.
            rumbleHappening = false;                              // This rumble command is done now.
            rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
        }
        return;                                                   // Alright we done here (if we did this before already)
    } else {                                                      // OR, we're rumbling for the first time.
        previousMillisRumble = millis();                          // Mark this as the start of this rumble command.
        analogWrite(rumblePin, rumbleIntensity);                  // Set the motor on.
        rumbleHappening = true;                                   // Mark that we're in a rumble command rn.
        return;                                                   // Now geddoutta here.
    }
}
#endif // USES_RUMBLE

void BurstFire()
{
    if(burstFireCount < 4) {  // Are we within the three shots alotted to a burst fire command?
        #ifdef USES_SOLENOID
            if(!digitalRead(solenoidPin) &&  // Is the solenoid NOT on right now, and the counter hasn't matched?
            (burstFireCount == burstFireCountLast)) {
                burstFireCount++;                                 // Increment the counter.
            }
            if(!digitalRead(solenoidPin)) {  // Now, is the solenoid NOT on right now?
                SolenoidActivation(solenoidFastInterval * 2);     // Hold it off a bit longer,
            } else {                         // or if it IS on,
                burstFireCountLast = burstFireCount;              // sync the counters since we completed one bullet cycle,
                SolenoidActivation(solenoidFastInterval);         // And start trying to activate the dingus.
            }
        #endif // USES_SOLENOID
        return;                                                   // Go on.
    } else {  // If we're at three bullets fired,
        burstFiring = false;                                      // Disable the currently firing tag,
        burstFireCount = 0;                                       // And set the count off.
        return;                                                   // Let's go back.
    }
}

// Updates the button array with new bindings, if any.
// VERY hacky workaround, but seems to work?
void UpdateBindings()
{
    LightgunButtons::Desc_t ButtonDescReplacement[] = {
        {btnTrigger, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, MOUSE_LEFT, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK},
        {btnGunA, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, 1, 15, BTN_AG_MASK2},
        {btnGunB, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Mouse, MOUSE_MIDDLE, LightgunButtons::ReportType_Gamepad, 2, 15, BTN_AG_MASK2},
        {btnStart, LightgunButtons::ReportType_Internal, playerStartBtn, LightgunButtons::ReportType_Internal, playerStartBtn, LightgunButtons::ReportType_Gamepad, 5, 20, BTN_AG_MASK2},
        {btnSelect, LightgunButtons::ReportType_Internal, playerSelectBtn, LightgunButtons::ReportType_Internal, playerSelectBtn, LightgunButtons::ReportType_Gamepad, 6, 20, BTN_AG_MASK2},
        {btnGunUp, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, LightgunButtons::ReportType_Keyboard, KEY_UP_ARROW, LightgunButtons::ReportType_Gamepad, 7, 20, BTN_AG_MASK2},
        {btnGunDown, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, LightgunButtons::ReportType_Keyboard, KEY_DOWN_ARROW, LightgunButtons::ReportType_Gamepad, 8, 20, BTN_AG_MASK2},
        {btnGunLeft, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, LightgunButtons::ReportType_Keyboard, KEY_LEFT_ARROW, LightgunButtons::ReportType_Gamepad, 9, 20, BTN_AG_MASK2},
        {btnGunRight, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, LightgunButtons::ReportType_Keyboard, KEY_RIGHT_ARROW, LightgunButtons::ReportType_Gamepad, 10, 20, BTN_AG_MASK2},
        {btnGunC, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Gamepad, 3, 15, BTN_AG_MASK2},
        {btnPedal, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Gamepad, 4, 15, BTN_AG_MASK2},
        {btnPump, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, 1, 15, BTN_AG_MASK2},
        {btnHome, LightgunButtons::ReportType_Internal, MOUSE_BUTTON5, LightgunButtons::ReportType_Internal, MOUSE_BUTTON5, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK2}
    };

    for(byte n = 0; n < ButtonCount; n++) {
        LightgunButtons::ButtonDesc[n] = ButtonDescReplacement[n];
    }
}

#ifdef DEBUG_SERIAL
void PrintDebugSerial()
{
    // only print every second
    if(millis() - serialDbMs >= 1000 && Serial.dtr()) {
#ifdef EXTRA_POS_GLITCH_FILTER
        Serial.print("bad final count ");
        Serial.print(badFinalCount);
        Serial.print(", bad move count ");
        Serial.println(badMoveCount);
#endif // EXTRA_POS_GLITCH_FILTER
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
