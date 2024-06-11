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
#define OPENFIRE_VERSION 5.0
#define OPENFIRE_CODENAME "Heartful"

 // For custom builders, remember to check (COMPILING.md) for IDE instructions!
 // ISSUERS: REMEMBER TO SPECIFY YOUR USING A CUSTOM BUILD & WHAT CHANGES ARE MADE TO THE SKETCH; OTHERWISE YOUR ISSUE MAY BE CLOSED!

#include <Arduino.h>
#include <RP2040.h>
#include <OpenFIREBoard.h>

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
#include <OpenFIRE_Square.h>
#include <OpenFIRE_Diamond.h>
#include <OpenFIRE_Perspective.h>
#include <OpenFIREConst.h>
#include "SamcoColours.h"
#include "SamcoPreferences.h"
#include "OpenFIREFeedback.h"

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
#define DEVICE_NAME "FIRECon"
#define DEVICE_VID 0xF143
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

  // Leave this uncommented to enable optional support for SSD1306 monochrome OLED displays.
#define USES_DISPLAY
#ifdef USES_DISPLAY
  #include "SamcoDisplay.h"
#endif // USES_DISPLAY

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
    {SamcoPreferences::pins.bPedal, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON4, LightgunButtons::ReportType_Gamepad, PAD_X, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bPedal2, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Mouse, MOUSE_BUTTON5, LightgunButtons::ReportType_Gamepad, PAD_B, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bPump, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Mouse, MOUSE_RIGHT, LightgunButtons::ReportType_Gamepad, PAD_LT, 15, BTN_AG_MASK2},
    {SamcoPreferences::pins.bHome, LightgunButtons::ReportType_Internal, 0, LightgunButtons::ReportType_Internal, 0, LightgunButtons::ReportType_Internal, 0, 15, BTN_AG_MASK2}
};

// button count constant
constexpr unsigned int ButtonCount = sizeof(LightgunButtons::ButtonDesc) / sizeof(LightgunButtons::ButtonDesc[0]);

// button runtime data arrays
LightgunButtonsStatic<ButtonCount> lgbData;

// button object instance
LightgunButtons buttons(lgbData, ButtonCount);

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
// top/bottom/left/right offsets, TLled/TRled, adjX/adjY, sensitivity, runmode, button mask mapped to profile, layout toggle, color, name
SamcoPreferences::ProfileData_t profileData[ProfileCount] = {
    {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_A,      false, 0xFF0000, "Bringus"},
    {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_B,      false, 0x00FF00, "Brongus"},
    {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_Start,  false, 0x0000FF, "Broongas"},
    {0, 0, 0, 0, 500 << 2, 1420 << 2, 512 << 2, 384 << 2, DFRobotIRPositionEx::Sensitivity_Default, RunMode_Average, BtnMask_Select, false, 0xFF00FF, "Parace L'sia"}
};
//  ------------------------------------------------------------------------------------------------------

int mouseX;
int mouseY;
int moveXAxisArr[3] = {0, 0, 0};
int moveYAxisArr[3] = {0, 0, 0};
int moveIndex = 0;

// For offscreen button stuff:
bool offscreenButton = false;                    // Does shooting offscreen also send a button input (for buggy games that don't recognize off-screen shots)? Default to off.
bool offscreenBShot = false;                     // For offscreenButton functionality, to track if we shot off the screen.
bool buttonPressed = false;                      // Sanity check.

#ifdef USES_ANALOG
    bool analogIsValid;                          // Flag set true if analog stick is mapped to valid nums
#endif // USES_ANALOG

#ifdef FOURPIN_LED
    bool ledIsValid;                             // Flag set true if RGB pins are mapped to valid numbers
#endif // FOURPIN_LED

#ifdef MAMEHOOKER
// For serial mode:
    bool serialMode = false;                         // Set if we're prioritizing force feedback over serial commands or not.
    bool offscreenButtonSerial = false;              // Serial-only version of offscreenButton toggle.
    byte serialQueue = 0b00000000;                   // Bitmask of events we've queued from the serial receipt.
    bool serialARcorrection = false;                 // 4:3 AR correction mode flag
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
    #ifdef USES_DISPLAY
    bool serialDisplayChange = false;                // Signal of pending display update, sent by Core 2 to be used by Core 1 in dual core configs
    uint8_t serialLifeCount = 0;
    uint8_t serialAmmoCount = 0;
    #endif // USES_DISPLAY
#endif // MAMEHOOKER

#ifdef USES_DISPLAY
// Display wrapper interface
ExtDisplay OLED;
#endif // USES_DISPLAY

// Force feedback interface
FFB OF_FFB;

unsigned int lastSeen = 0;

bool justBooted = true;                              // For ops we need to do on initial boot (custom pins, joystick centering)
bool dockedSaving = false;                           // To block sending test output in docked mode.
bool dockedCalibrating = false;                      // If set, calibration will send back to docked mode.

unsigned long testLastStamp;                         // Timestamp of last print in test mode.
const byte testPrintInterval = 50;                   // Minimum time allowed between test mode printouts.

// profile in use
unsigned int selectedProfile = 0;

// OpenFIRE Positioning - one for Square, one for Diamond, and a share perspective object
OpenFIRE_Square OpenFIREsquare;
OpenFIRE_Diamond OpenFIREdiamond;
OpenFIRE_Perspective OpenFIREper;

// operating modes
enum GunMode_e {
    GunMode_Init = -1,
    GunMode_Run = 0,
    GunMode_Calibration,
    GunMode_Verification,
    GunMode_Pause,
    GunMode_Docked
};
GunMode_e gunMode = GunMode_Init;   // initial mode

enum CaliStage_e {
    Cali_Init = 0,
    Cali_Top,
    Cali_Bottom,
    Cali_Left,
    Cali_Right,
    Cali_Center,
    Cali_Verify
};

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
    // initialize EEPROM device. Arduino AVR has a 1k flash, so use that.
    EEPROM.begin(1024);

    SamcoPreferences::LoadPresets();
    
    if(nvAvailable) {
        LoadPreferences();
        if(nvPrefsError == SamcoPreferences::Error_NoData) {
            SamcoPreferences::ResetPreferences();
        } else if(nvPrefsError == SamcoPreferences::Error_Success) {
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
            }
            SamcoPreferences::LoadSettings();
            SamcoPreferences::LoadUSBID();
        }
        // this is needed for both customs and builtins, as defaults are all uninitialized
        UpdateBindings(SamcoPreferences::toggles.lowButtonMode);
    }
 
    // We're setting our custom USB identifiers, as defined in the configuration area!
    #ifdef USE_TINYUSB
        TinyUSBInit();
    #endif // USE_TINYUSB

    // Initialize DFRobot Camera Wires & Object
    CameraSet();

    // initialize buttons & feedback devices
    buttons.Begin();
    FeedbackSet();
    #ifdef LED_ENABLE
        LedInit();
    #endif // LED_ENABLE
    
#ifdef USE_TINYUSB
    #if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
    // is VBUS (USB voltage) detected?
    if(digitalRead(34)) {
        // If so, we're connected via USB, so initializing the USB devices chunk.
        TinyUSBDevices.begin(1);
        // wait until device mounted
        while(!USBDevice.mounted()) { yield(); }
        Serial.begin(9600);
        Serial.setTimeout(0);
    } else {
        // Else, we're on batt, so init the Bluetooth chunks.
        if(SamcoPreferences::usb.deviceName[0] == '\0') {
            TinyUSBDevices.beginBT(DEVICE_NAME, DEVICE_NAME);
        } else {
            TinyUSBDevices.beginBT(SamcoPreferences::usb.deviceName, SamcoPreferences::usb.deviceName);
        }
    }
    #else
    // Initializing the USB devices chunk.
    TinyUSBDevices.begin(1);
    // wait until device mounted
    while(!USBDevice.mounted()) { yield(); }
    Serial.begin(9600);   // 9600 = 1ms data transfer rates, default for MAMEHOOKER COM devices.
    Serial.setTimeout(0);
    #endif // ARDUINO_RASPBERRY_PI_PICO_W
#else
    // was getting weird hangups... maybe nothing, or maybe related to dragons, so wait a bit
    delay(100);
#endif
    
    AbsMouse5.init(true);

    // IR camera maxes out motion detection at ~300Hz, and millis() isn't good enough
    startIrCamTimer(IRCamUpdateRate);

    OpenFIREper.source(profileData[selectedProfile].adjX, profileData[selectedProfile].adjY);
    OpenFIREper.deinit(0);

    // First boot sanity checks.
    // Check if loading has failde
    if((nvPrefsError != SamcoPreferences::Error_Success) ||
    (profileData[selectedProfile].topOffset == 0 &&
     profileData[selectedProfile].bottomOffset == 0 && 
     profileData[selectedProfile].leftOffset == 0 &&
     profileData[selectedProfile].rightOffset == 0)) {
        // SHIT, it's a first boot! Prompt to start calibration.
        Serial.println("Preferences data is empty!");
        Serial.println("Pull the trigger to start your first calibration!");
        unsigned int timerIntervalShort = 600;
        unsigned int timerInterval = 1000;
        LedOff();
        unsigned long lastT = millis();
        bool LEDisOn = false;
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_Init);
        #endif // USES_DISPLAY
        while(!(buttons.pressedReleased == BtnMask_Trigger)) {
            // Check and process serial commands, in case user needs to change EEPROM settings.
            if(Serial.available()) {
                SerialProcessingDocked();
            }
            if(gunMode == GunMode_Docked) {
                ExecGunModeDocked();
                #ifdef USES_DISPLAY
                  OLED.ScreenModeChange(ExtDisplay::Screen_Init);
                #endif // USES_DISPLAY
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
        // Because the app offers cali options, just check to make sure that
        // current prof wasn't cal'd from THERE for sanity.
        if((profileData[selectedProfile].topOffset == 0 &&
            profileData[selectedProfile].bottomOffset == 0 && 
            profileData[selectedProfile].leftOffset == 0 &&
            profileData[selectedProfile].rightOffset == 0)) { SetMode(GunMode_Calibration); }
            else { SetMode(GunMode_Run); }
    } else {
        // unofficial official "MiSTer mode" - default to camera -> left stick if trigger's held.
        // For some reason, doing this through buttons.Poll doesn't work, so just directly read the pin.
        // (pullup resistors make this normally closed/1 when unpressed, open/0 when pressed.)
        if(SamcoPreferences::pins.bTrigger >= 0 && !digitalRead(SamcoPreferences::pins.bTrigger)) {
            buttons.analogOutput = true;
            Gamepad16.stickRight = true;
        }
        // this will turn off the DotStar/RGB LED and ensure proper transition to Run
        SetMode(GunMode_Run);
    }
}

// (Re-)initializes DFRobot Camera object with wire set by current pins.
void CameraSet()
{
    if(dfrIRPos != nullptr) {
        delete dfrIRPos;
    }
    // Sanity check: which channel do these pins correlate to?
    if(!SamcoPreferences::toggles.customPinsInUse) {
        SamcoPreferences::PresetCam();
    }
    if(bitRead(SamcoPreferences::pins.pCamSCL, 1) && bitRead(SamcoPreferences::pins.pCamSDA, 1)) {
        // I2C1
        if(bitRead(SamcoPreferences::pins.pCamSCL, 0) && !bitRead(SamcoPreferences::pins.pCamSDA, 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire1.setSDA(SamcoPreferences::pins.pCamSDA);
            Wire1.setSCL(SamcoPreferences::pins.pCamSCL);
        }
        dfrIRPos = new DFRobotIRPositionEx(Wire1);
    } else if(!bitRead(SamcoPreferences::pins.pCamSCL, 1) && !bitRead(SamcoPreferences::pins.pCamSDA, 1)) {
        // I2C0
        if(bitRead(SamcoPreferences::pins.pCamSCL, 0) && !bitRead(SamcoPreferences::pins.pCamSDA, 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire.setSDA(SamcoPreferences::pins.pCamSDA);
            Wire.setSCL(SamcoPreferences::pins.pCamSCL);
        }
        dfrIRPos = new DFRobotIRPositionEx(Wire);
    }
    // Start IR Camera with basic data format
    dfrIRPos->begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity);
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
        externPixel = new Adafruit_NeoPixel(SamcoPreferences::settings.customLEDcount, SamcoPreferences::pins.oPixel, NEO_GRB + NEO_KHZ800);
        externPixel->begin();
        if(SamcoPreferences::settings.customLEDstatic < SamcoPreferences::settings.customLEDcount) {
            for(byte i = 0; i < SamcoPreferences::settings.customLEDstatic; i++) {
                uint32_t color;
                switch(i) {
                  case 0:
                    color = SamcoPreferences::settings.customLEDcolor1;
                    break;
                  case 1:
                    color = SamcoPreferences::settings.customLEDcolor2;
                    break;
                  case 2:
                    color = SamcoPreferences::settings.customLEDcolor3;
                    break;
                }
                externPixel->setPixelColor(i, color);
            }
            externPixel->show();
        }
    }
    #endif // CUSTOM_NEOPIXEL
    #ifdef USES_DISPLAY
    // wrapper will manage display validity
    if(SamcoPreferences::pins.pPeriphSCL >= 0 && SamcoPreferences::pins.pPeriphSDA >= 0 &&
      // check it's not using the camera's I2C line
       bitRead(SamcoPreferences::pins.pCamSCL, 1) != bitRead(SamcoPreferences::pins.pPeriphSCL, 1) &&
       bitRead(SamcoPreferences::pins.pCamSDA, 1) != bitRead(SamcoPreferences::pins.pPeriphSDA, 1)) {
        OLED.Begin();
    }
    #endif // USES_DISPLAY
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
                externPixel->clear();
                delete externPixel;
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

#ifdef ARDUINO_ARCH_RP2040
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
#endif // ARDUINO_ARCH_RP2040

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
    #ifdef USES_ANALOG
        unsigned long lastAnalogPoll = millis();
    #endif // USES_ANALOG
    while(gunMode == GunMode_Run) {
        // For processing the trigger specifically.
        // (buttons.debounced is a binary variable intended to be read 1 bit at a time, with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
        buttons.Poll(0);

        #ifdef MAMEHOOKER
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
            if(analogIsValid && (millis() - lastAnalogPoll > 1)) {
                AnalogStickPoll();
                lastAnalogPoll = millis();
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
                    OF_FFB.FFBShutdown();
                    offscreenBShot = false;
                    buttonPressed = false;
                    pauseModeSelection = PauseMode_Calibrate;
                    buttons.ReportDisable();
                    SetMode(GunMode_Pause);
                }
            }
        } else {
            if(buttons.pressedReleased == EnterPauseModeBtnMask || buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                OF_FFB.FFBShutdown();
                offscreenBShot = false;
                buttonPressed = false;
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

    if(SamcoPreferences::toggles.holdToPause && pauseHoldStarted) {
        #ifdef USES_RUMBLE
            analogWrite(SamcoPreferences::pins.oRumble, SamcoPreferences::settings.rumbleIntensity);
            delay(300);
            digitalWrite(SamcoPreferences::pins.oRumble, LOW);
        #endif // USES_RUMBLE
        while(buttons.debounced != 0) {
            // Should release the buttons to continue, pls.
            buttons.Poll(1);
        }
        pauseHoldStarted = false;
        pauseModeSelectingProfile = false;
    }

    #ifdef MAMEHOOKER
        if(Serial.available()) { SerialProcessing(); }
    #endif // MAMEHOOKER

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
                            Serial.println(profileData[selectedProfile].name);
                            Serial.println("Going back to the main menu...");
                            Serial.println("Selecting: Calibrate current profile");
                        }
                        #ifdef USES_DISPLAY
                            OLED.PauseListUpdate(ExtDisplay::ScreenPause_Calibrate);
                        #endif // USES_DISPLAY
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
                        #ifdef USES_DISPLAY
                            OLED.PauseListUpdate(ExtDisplay::ScreenPause_Calibrate);
                        #endif // USES_DISPLAY
                    }
                } else if(buttons.pressedReleased == BtnMask_A) {
                    SetPauseModeSelection(false);
                } else if(buttons.pressedReleased == BtnMask_B) {
                    SetPauseModeSelection(true);
                } else if(buttons.pressedReleased == BtnMask_Trigger) {
                    switch(pauseModeSelection) {
                        case PauseMode_Calibrate:
                          SetMode(GunMode_Calibration);
                          if(!serialMode) {
                              Serial.print("Calibrating for current profile: ");
                              Serial.println(profileData[selectedProfile].name);
                          }
                          break;
                        case PauseMode_ProfileSelect:
                          if(!serialMode) {
                              Serial.println("Pick a profile!");
                              Serial.print("Current profile in use: ");
                              Serial.println(profileData[selectedProfile].name);
                          }
                          pauseModeSelectingProfile = true;
                          profileModeSelection = selectedProfile;
                          #ifdef USES_DISPLAY
                              OLED.PauseProfileUpdate(profileModeSelection, profileData[0].name, profileData[1].name, profileData[2].name, profileData[3].name);
                          #endif // USES_DISPLAY
                          #ifdef LED_ENABLE
                              SetLedPackedColor(profileData[selectedProfile].color);
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
                          #ifdef USES_DISPLAY
                              OLED.TopPanelUpdate("", "Sent Escape Key!");
                          #endif // USES_DISPLAY
                          #ifdef LED_ENABLE
                              for(byte i = 0; i < 3; i++) {
                                  LedUpdate(150,0,150);
                                  delay(55);
                                  LedOff();
                                  delay(40);
                              }
                          #endif // LED_ENABLE
                          #ifdef USES_DISPLAY
                              OLED.TopPanelUpdate("Using ", profileData[selectedProfile].name);
                          #endif // USES_DISPLAY
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
                                analogWrite(SamcoPreferences::pins.oRumble, SamcoPreferences::settings.rumbleIntensity);
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
                SetMode(GunMode_Calibration);
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
        case GunMode_Calibration:
            ExecCalMode();
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
    buttons.ReportEnable();
    if(justBooted) {
        // center the joystick so RetroArch doesn't throw a hissy fit about uncentered joysticks
        delay(100);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
        justBooted = false;
    }
    #ifdef USES_ANALOG
        unsigned long lastAnalogPoll = millis();
    #endif // USES_ANALOG
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
        }

        #ifdef MAMEHOOKER
            #ifdef USES_DISPLAY
                // For some reason, solenoid feedback is hella wonky when ammo updates are performed on the second core,
                // so just do it here using the signal sent by it.
                if(serialDisplayChange) {
                    if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Ammo) { OLED.PrintAmmo(serialAmmoCount); }
                    else if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Life) { OLED.PrintLife(serialLifeCount); }
                    else if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both) {
                      OLED.PrintAmmo(serialAmmoCount);
                      OLED.PrintLife(serialLifeCount);
                    }
                    serialDisplayChange = false;
                }
            #endif // USES_DISPLAY
        #endif // MAMEHOOKER

        // If using RP2040, we offload the button processing to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)

        #ifdef USES_ANALOG
            if(analogIsValid && (millis() - lastAnalogPoll > 1)) {
                AnalogStickPoll();
                lastAnalogPoll = millis();
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
                    Keyboard.releaseAll();
                    AbsMouse5.releaseAll();
                    offscreenBShot = false;
                    buttonPressed = false;
                    triggerHeld = false;
                    burstFiring = false;
                    burstFireCount = 0;
                    pauseModeSelection = PauseMode_Calibrate;
                    SetMode(GunMode_Pause);
                    buttons.ReportDisable();
                    return;
                }
            }
        } else {
            if(buttons.pressedReleased == EnterPauseModeBtnMask || buttons.pressedReleased == BtnMask_Home) {
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
                Keyboard.releaseAll();
                AbsMouse5.releaseAll();
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
            AbsMouse5.releaseAll();
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
    buttons.ReportDisable();
    #ifdef USES_DISPLAY
        OLED.ScreenModeChange(ExtDisplay::Screen_IRTest);
    #endif // USES_DISPLAY
    for(;;) {
        buttons.Poll(1);
        if(Serial.available()) {
            #ifdef USES_DISPLAY
                OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
            #endif // USES_DISPLAY
            SerialProcessingDocked();
        }
        if(runMode != RunMode_Processing) {
            return;
        }

        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
            GetPosition();
        }
    }
}

// For use with the OpenFIRE app when it connects to this board.
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
    unsigned long aStickChecked = millis();
    uint8_t aStickDirPrev;

    Serial.printf("OpenFIRE,%.1f,%s,%s,%i\r\n",
    OPENFIRE_VERSION,
    OPENFIRE_CODENAME,
    OPENFIRE_BOARD,
    selectedProfile);
    for(;;) {
        buttons.Poll(1);

        if(Serial.available()) {
            SerialProcessingDocked();
        }

        if(!dockedSaving) {
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
                case BtnMask_Pedal2:
                  Serial.println("Pressed: 12");
                  break;
                case BtnMask_Home:
                  Serial.println("Pressed: 13");
                  break;
                case BtnMask_Pump:
                  Serial.println("Pressed: 14");
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
                case BtnMask_Pedal2:
                  Serial.println("Released: 12");
                  break;
                case BtnMask_Home:
                  Serial.println("Released: 13");
                  break;
                case BtnMask_Pump:
                  Serial.println("Released: 14");
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
            if(analogIsValid) {
                if(currentMillis - aStickChecked >= 16) {
                    unsigned int analogValueX = analogRead(SamcoPreferences::pins.aStickX);
                    unsigned int analogValueY = analogRead(SamcoPreferences::pins.aStickY);
                    // Analog stick deadzone should help mitigate overwriting USB commands for the other input channels.
                    uint8_t aStickDir = 0;
                    if((analogValueX < 1900 || analogValueX > 2200) ||
                      (analogValueY < 1900 || analogValueY > 2200)) {
                        if(analogValueX > 2200) {
                            bitSet(aStickDir, 0), bitClear(aStickDir, 1);
                        } else if(analogValueX < 1900) {
                            bitSet(aStickDir, 1), bitClear(aStickDir, 0);
                        } else {
                            bitClear(aStickDir, 0), bitClear(aStickDir, 1);
                        }
                        if(analogValueY > 2200) { 
                            bitSet(aStickDir, 2), bitClear(aStickDir, 3);
                        } else if(analogValueY < 1900) {
                            bitSet(aStickDir, 3), bitClear(aStickDir, 2);
                        } else {
                            bitClear(aStickDir, 2), bitClear(aStickDir, 3);
                        }
                    }
                    if(aStickDir != aStickDirPrev) {
                        switch(aStickDir) {
                        case 0b00000100: // up
                          Serial.println("Analog: 1");
                          break;
                        case 0b00000101: // up-left
                          Serial.println("Analog: 2");
                          break;
                        case 0b00000001: // left
                          Serial.println("Analog: 3");
                          break;
                        case 0b00001001: // down-left
                          Serial.println("Analog: 4");
                          break;
                        case 0b00001000: // down
                          Serial.println("Analog: 5");
                          break;
                        case 0b00001010: // down-right
                          Serial.println("Analog: 6");
                          break;
                        case 0b00000010: // right
                          Serial.println("Analog: 7");
                          break;
                        case 0b00000110: // up-right
                          Serial.println("Analog: 8");
                          break;
                        default:         // center
                          Serial.println("Analog: 0");
                          break;
                        }
                        aStickDirPrev = aStickDir;
                    }
                }
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

// Dedicated calibration method
void ExecCalMode()
{
    buttons.ReportDisable();
    uint8_t calStage = 0;
    // backup current values in case the user cancels
    int _topOffset = profileData[selectedProfile].topOffset;
    int _bottomOffset = profileData[selectedProfile].bottomOffset;
    int _leftOffset = profileData[selectedProfile].leftOffset;
    int _rightOffset = profileData[selectedProfile].rightOffset;
    float _TLled = profileData[selectedProfile].TLled;
    float _TRled = profileData[selectedProfile].TRled;
    float _adjX = profileData[selectedProfile].adjX;
    float _adjY = profileData[selectedProfile].adjY;
    // set current values to factory defaults
    profileData[selectedProfile].topOffset = 0, profileData[selectedProfile].bottomOffset = 0,
    profileData[selectedProfile].leftOffset = 0, profileData[selectedProfile].rightOffset = 0;
    if(profileData[selectedProfile].irLayout) {
        profileData[selectedProfile].TLled = 0, profileData[selectedProfile].TRled = 1920 << 2;
    } else {
        profileData[selectedProfile].TLled = 500 << 2, profileData[selectedProfile].TRled = 1420 << 2;
    }
    profileData[selectedProfile].adjX = 512 << 2, profileData[selectedProfile].adjY = 384 << 2;
    // force center mouse to center
    AbsMouse5.move(32768/2, 32768/2);
    // jack in, CaliMan, execute!!!
    SetMode(GunMode_Calibration);
    while(gunMode == GunMode_Calibration) {
        buttons.Poll(1);

        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
            GetPosition();
        }
        
        if((buttons.pressedReleased & (ExitPauseModeBtnMask | ExitPauseModeHoldBtnMask)) && !justBooted) {
            Serial.println("Calibration cancelled");
            // Reapplying backed up data
            profileData[selectedProfile].topOffset = _topOffset;
            profileData[selectedProfile].bottomOffset = _bottomOffset;
            profileData[selectedProfile].leftOffset = _leftOffset;
            profileData[selectedProfile].rightOffset = _rightOffset;
            profileData[selectedProfile].TLled = _TLled;
            profileData[selectedProfile].TRled = _TRled;
            profileData[selectedProfile].adjX = _adjX;
            profileData[selectedProfile].adjY = _adjY;
            // re-print the profile
            stateFlags |= StateFlag_PrintSelectedProfile;
            // re-apply the cal stored in the profile
            if(dockedCalibrating) {
                SetMode(GunMode_Docked);
                dockedCalibrating = false;
            } else {
                // return to pause mode
                SetMode(GunMode_Run);
            }
            return;
        } else if(buttons.pressed == BtnMask_Trigger) {
            calStage++;
            CaliMousePosMove(calStage);
            switch(calStage) {
                case Cali_Init:
                  break;
                case Cali_Top:
                  // Set Cam center offsets
                  if(profileData[selectedProfile].irLayout) {
                      profileData[selectedProfile].adjX = (OpenFIREdiamond.testMedianX() - (512 << 2)) * cos(OpenFIREdiamond.Ang()) - (OpenFIREdiamond.testMedianY() - (384 << 2)) * sin(OpenFIREdiamond.Ang()) + (512 << 2);       
                      profileData[selectedProfile].adjY = (OpenFIREdiamond.testMedianX() - (512 << 2)) * sin(OpenFIREdiamond.Ang()) + (OpenFIREdiamond.testMedianY() - (384 << 2)) * cos(OpenFIREdiamond.Ang()) + (384 << 2);
                      // Work out Led locations by assuming height is 100%
                      profileData[selectedProfile].TLled = (res_x / 2) - ( (OpenFIREdiamond.W() * (res_y  / OpenFIREdiamond.H()) ) / 2);            
                      profileData[selectedProfile].TRled = (res_x / 2) + ( (OpenFIREdiamond.W() * (res_y  / OpenFIREdiamond.H()) ) / 2);
                  } else {
                      profileData[selectedProfile].adjX = (OpenFIREsquare.testMedianX() - (512 << 2)) * cos(OpenFIREsquare.Ang()) - (OpenFIREsquare.testMedianY() - (384 << 2)) * sin(OpenFIREsquare.Ang()) + (512 << 2);       
                      profileData[selectedProfile].adjY = (OpenFIREsquare.testMedianX() - (512 << 2)) * sin(OpenFIREsquare.Ang()) + (OpenFIREsquare.testMedianY() - (384 << 2)) * cos(OpenFIREsquare.Ang()) + (384 << 2);
                      // Work out Led locations by assuming height is 100%
                      profileData[selectedProfile].TLled = (res_x / 2) - ( (OpenFIREsquare.W() * (res_y  / OpenFIREsquare.H()) ) / 2);            
                      profileData[selectedProfile].TRled = (res_x / 2) + ( (OpenFIREsquare.W() * (res_y  / OpenFIREsquare.H()) ) / 2);
                  }
                  
                  // Update Cam centre in perspective library
                  OpenFIREper.source(profileData[selectedProfile].adjX, profileData[selectedProfile].adjY);                                                          
                  OpenFIREper.deinit(0);
                  // Move to top calibration point
                  AbsMouse5.move(32768/2, 0);
                  break;

                case Cali_Bottom:
                  // Set Offset buffer
                  profileData[selectedProfile].topOffset = mouseY;
                  // Move to bottom calibration point
                  AbsMouse5.move(32768/2, 32768);
                  break;

                case Cali_Left:
                  // Set Offset buffer
                  profileData[selectedProfile].bottomOffset = (res_y - mouseY);
                  // Move to left calibration point
                  AbsMouse5.move(0, 32768/2);
                  break;

                case Cali_Right:
                  // Set Offset buffer
                  profileData[selectedProfile].leftOffset = mouseX;
                  // Move to right calibration point
                  AbsMouse5.move(32768, 32768/2);
                  break;

                case Cali_Center:
                  // Set Offset buffer
                  profileData[selectedProfile].rightOffset = (res_x - mouseX);
                  // Move back to center calibration point
                  AbsMouse5.move(32768/2, 32768/2);
                  break;

                case Cali_Verify:
                  // Apply new Cam center offsets with Offsets applied
                  if(profileData[selectedProfile].irLayout) {
                      profileData[selectedProfile].adjX = (OpenFIREdiamond.testMedianX() - (512 << 2)) * cos(OpenFIREdiamond.Ang()) - (OpenFIREdiamond.testMedianY() - (384 << 2)) * sin(OpenFIREdiamond.Ang()) + (512 << 2);       
                      profileData[selectedProfile].adjY = (OpenFIREdiamond.testMedianX() - (512 << 2)) * sin(OpenFIREdiamond.Ang()) + (OpenFIREdiamond.testMedianY() - (384 << 2)) * cos(OpenFIREdiamond.Ang()) + (384 << 2);
                  } else {
                      profileData[selectedProfile].adjX = (OpenFIREsquare.testMedianX() - (512 << 2)) * cos(OpenFIREsquare.Ang()) - (OpenFIREsquare.testMedianY() - (384 << 2)) * sin(OpenFIREsquare.Ang()) + (512 << 2);       
                      profileData[selectedProfile].adjY = (OpenFIREsquare.testMedianX() - (512 << 2)) * sin(OpenFIREsquare.Ang()) + (OpenFIREsquare.testMedianY() - (384 << 2)) * cos(OpenFIREsquare.Ang()) + (384 << 2);
                  }
                  // Update Cam centre in perspective library
                  OpenFIREper.source(profileData[selectedProfile].adjX, profileData[selectedProfile].adjY);
                  OpenFIREper.deinit(0);

                  // let the user test.
                  SetMode(GunMode_Verification);
                  while(gunMode == GunMode_Verification) {
                      buttons.Poll();
                      if(irPosUpdateTick) {
                          irPosUpdateTick = 0;
                          GetPosition();
                      }
                      // If it's good, move onto cali finish.
                      if(buttons.pressed == BtnMask_Trigger) {
                          calStage++;
                          SetMode(GunMode_Run);
                      // Press A/B to restart cali for current profile
                      } else if(buttons.pressedReleased & ExitPauseModeHoldBtnMask) {
                          calStage = 0;
                          // (re)set current values to factory defaults
                          profileData[selectedProfile].topOffset = 0, profileData[selectedProfile].bottomOffset = 0,
                          profileData[selectedProfile].leftOffset = 0, profileData[selectedProfile].rightOffset = 0;
                          if(profileData[selectedProfile].irLayout) {
                              profileData[selectedProfile].TLled = 0, profileData[selectedProfile].TRled = 1920 << 2;
                          } else {
                              profileData[selectedProfile].TLled = 500 << 2, profileData[selectedProfile].TRled = 1420 << 2;
                          }
                          profileData[selectedProfile].adjX = 512 << 2, profileData[selectedProfile].adjY = 384 << 2;
                          SetMode(GunMode_Calibration);
                          delay(1);
                          AbsMouse5.move(32768/2, 32768/2);
                      // Press C/Home to exit wholesale without committing new cali values
                      } else if(buttons.pressedReleased & ExitPauseModeBtnMask && !justBooted) {
                          Serial.println("Calibration cancelled");
                          // Reapplying backed up data
                          profileData[selectedProfile].topOffset = _topOffset;
                          profileData[selectedProfile].bottomOffset = _bottomOffset;
                          profileData[selectedProfile].leftOffset = _leftOffset;
                          profileData[selectedProfile].rightOffset = _rightOffset;
                          profileData[selectedProfile].TLled = _TLled;
                          profileData[selectedProfile].TRled = _TRled;
                          profileData[selectedProfile].adjX = _adjX;
                          profileData[selectedProfile].adjY = _adjY;
                          // re-print the profile
                          stateFlags |= StateFlag_PrintSelectedProfile;
                          // re-apply the cal stored in the profile
                          if(dockedCalibrating) {
                              SetMode(GunMode_Docked);
                              dockedCalibrating = false;
                          } else {
                              SetMode(GunMode_Run);
                          }
                          return;
                      }
                  }
                  break;
            }
        }
    }
    // Break Cali
    if(justBooted) {
        // If this is an initial calibration, save it immediately!
        stateFlags |= StateFlag_SavePreferencesEn;
        SavePreferences();
    } else if(dockedCalibrating) {
        Serial.print("UpdatedProf: ");
        Serial.println(selectedProfile);
        Serial.println(profileData[selectedProfile].topOffset);
        Serial.println(profileData[selectedProfile].bottomOffset);
        Serial.println(profileData[selectedProfile].leftOffset);
        Serial.println(profileData[selectedProfile].rightOffset);
        Serial.println(profileData[selectedProfile].TLled);
        Serial.println(profileData[selectedProfile].TRled);
        //Serial.println(profileData[selectedProfile].adjX);
        //Serial.println(profileData[selectedProfile].adjY);
        SetMode(GunMode_Docked);
    } else {
        SetMode(GunMode_Run);
    }
    #ifdef USES_RUMBLE
        if(SamcoPreferences::toggles.rumbleActive) {
            digitalWrite(SamcoPreferences::pins.oRumble, true);
            delay(80);
            digitalWrite(SamcoPreferences::pins.oRumble, false);
            delay(50);
            digitalWrite(SamcoPreferences::pins.oRumble, true);
            delay(125);
            digitalWrite(SamcoPreferences::pins.oRumble, false);
        }
    #endif // USES_RUMBLE
}

// Locking function moving from cali point to point
void CaliMousePosMove(uint8_t caseNumber)
{
    int32_t xPos;
    int32_t yPos;

    // Note: All TinyUSB device updates are blocking when a previous signal hasn't finished transmitting,
    // so we skip chunks to speed things the hell up.
    // These originally were inc/decrementing by 1 with a 20us delay between.

    switch(caseNumber) {
        case Cali_Top:
          for(yPos = 32768/2; yPos > 0; yPos = yPos - 30) {
              if(yPos < 31) { yPos = 0; }
              AbsMouse5.move(32768/2, yPos);
          }
          delay(5);
          break;
        case Cali_Bottom:
          for(yPos = 0; yPos < 32768; yPos = yPos + 30) {
              if(yPos > 32768-31) { yPos = 32768; }
              AbsMouse5.move(32768/2, yPos);
          }
          delay(5);
          break;
        case Cali_Left:
          yPos = 32768;
          for(xPos = 32768/2; xPos > 0; xPos = xPos - 30) {
              if(xPos < 31) { xPos = 0; }
              if(yPos > 32768/2) { yPos = yPos - 30; }
              else { yPos = 32768/2; }
              AbsMouse5.move(xPos, yPos);
          }
          delay(5);
          break;
        case Cali_Right:
          for(xPos = 0; xPos < 32768; xPos = xPos + 30) {
              if(xPos > 32768-31) { xPos = 32768; }
              AbsMouse5.move(xPos, 32768/2);
          }
          delay(5);
          break;
        case Cali_Center:
          for(xPos = 32768; xPos > 32768/2; xPos = xPos - 30) {
              if(xPos < 32768/2) { xPos = 32768/2; }
              AbsMouse5.move(xPos, 32768/2);
          }
          delay(5);
          break;
        default:
          break;
    }
}

// Get tilt adjusted position from IR postioning camera
// Updates finalX and finalY values
void GetPosition()
{
    int error = dfrIRPos->basicAtomic(DFRobotIRPositionEx::Retry_2);
    if(error == DFRobotIRPositionEx::Error_Success) {
        // if diamond layout, or square
        if(profileData[selectedProfile].irLayout) {
            OpenFIREdiamond.begin(dfrIRPos->xPositions(), dfrIRPos->yPositions(), dfrIRPos->seen());
            OpenFIREper.warp(OpenFIREdiamond.X(0), OpenFIREdiamond.Y(0),
                             OpenFIREdiamond.X(1), OpenFIREdiamond.Y(1),
                             OpenFIREdiamond.X(2), OpenFIREdiamond.Y(2),
                             OpenFIREdiamond.X(3), OpenFIREdiamond.Y(3),
                             res_x / 2, 0, 0,
                             res_y / 2, res_x / 2,
                             res_y, res_x, res_y / 2);
        } else {
            OpenFIREsquare.begin(dfrIRPos->xPositions(), dfrIRPos->yPositions(), dfrIRPos->seen());
            OpenFIREper.warp(OpenFIREsquare.X(0), OpenFIREsquare.Y(0),
                             OpenFIREsquare.X(1), OpenFIREsquare.Y(1),
                             OpenFIREsquare.X(2), OpenFIREsquare.Y(2),
                             OpenFIREsquare.X(3), OpenFIREsquare.Y(3),
                             profileData[selectedProfile].TLled, 0,
                             profileData[selectedProfile].TRled, 0,
                             profileData[selectedProfile].TLled, res_y,
                             profileData[selectedProfile].TRled, res_y);
        }

        // Output mapped to screen resolution because offsets are measured in pixels
        mouseX = map(OpenFIREper.getX(), 0, res_x, (0 - profileData[selectedProfile].leftOffset), (res_x + profileData[selectedProfile].rightOffset));                 
        mouseY = map(OpenFIREper.getY(), 0, res_y, (0 - profileData[selectedProfile].topOffset), (res_y + profileData[selectedProfile].bottomOffset));

        switch(runMode) {
            case RunMode_Average:
                // 2 position moving average
                moveIndex ^= 1;
                moveXAxisArr[moveIndex] = mouseX;
                moveYAxisArr[moveIndex] = mouseY;
                mouseX = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                mouseY = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                break;
            case RunMode_Average2:
                // weighted average of current position and previous 2
                if(moveIndex < 2) {
                    ++moveIndex;
                } else {
                    moveIndex = 0;
                }
                moveXAxisArr[moveIndex] = mouseX;
                moveYAxisArr[moveIndex] = mouseY;
                mouseX = (mouseX + moveXAxisArr[0] + moveXAxisArr[1] + moveXAxisArr[2]) / 4;
                mouseY = (mouseY + moveYAxisArr[0] + moveYAxisArr[1] + moveYAxisArr[2]) / 4;
                break;
            default:
                break;
            }

        // Constrain that bisch so negatives don't cause underflow
        int32_t conMoveX = constrain(mouseX, 0, res_x);
        int32_t conMoveY = constrain(mouseY, 0, res_y);

        // Output mapped to Mouse resolution
        conMoveX = map(conMoveX, 0, res_x, 0, 32768);
        conMoveY = map(conMoveY, 0, res_y, 0, 32768);

        if(gunMode == GunMode_Run) {
            UpdateLastSeen();

            if(serialARcorrection) {
                conMoveX = map(conMoveX, 4147, 28697, 0, 32768);
                conMoveX = constrain(conMoveX, 0, 32768);
            }

            bool offXAxis = false;
            bool offYAxis = false;

            if(conMoveX == 0 || conMoveX == 32768) {
                offXAxis = true;
            }
            
            if(conMoveY == 0 || conMoveY == 32768) {
                offYAxis = true;
            }

            if(offXAxis || offYAxis) {
                buttons.offScreen = true;
            } else {
                buttons.offScreen = false;
            }

            if(buttons.analogOutput) {
                Gamepad16.moveCam(conMoveX, conMoveY);
            } else {
                AbsMouse5.move(conMoveX, conMoveY);
            }
        } else if(gunMode == GunMode_Verification) {
            AbsMouse5.move(conMoveX, conMoveY);
        } else {
            if(millis() - testLastStamp > testPrintInterval) {
                testLastStamp = millis();
                // RAW Camera Output mapped to screen res (1920x1080)
                int rawX[4];
                int rawY[4];
                // RAW Output for viewing in processing sketch mapped to 1920x1080 screen resolution
                for (int i = 0; i < 4; i++) {
                    if(profileData[selectedProfile].irLayout) {
                        rawX[i] = map(OpenFIREdiamond.X(i), 0, 1023 << 2, 1920, 0);
                        rawY[i] = map(OpenFIREdiamond.Y(i), 0, 768 << 2, 0, 1080);
                    } else {
                        rawX[i] = map(OpenFIREsquare.X(i), 0, 1023 << 2, 0, 1920);
                        rawY[i] = map(OpenFIREsquare.Y(i), 0, 768 << 2, 0, 1080);
                    }
                }
                if(runMode == RunMode_Processing) {
                    for(int i = 0; i < 4; i++) {
                        Serial.print(rawX[i]);
                        Serial.print( "," );
                        Serial.print(rawY[i]);
                        Serial.print( "," );
                    }
                    Serial.print( mouseX / 4 );
                    Serial.print( "," );
                    Serial.print( mouseY / 4 );
                    Serial.print( "," );
                    // Median for viewing in processing
                    if(profileData[selectedProfile].irLayout) {
                        Serial.print(map(OpenFIREdiamond.testMedianX(), 0, 1023 << 2, 1920, 0));
                        Serial.print( "," );
                        Serial.println(map(OpenFIREdiamond.testMedianY(), 0, 768 << 2, 0, 1080));
                    } else {
                        Serial.print(map(OpenFIREsquare.testMedianX(), 0, 1023 << 2, 0, 1920));
                        Serial.print( "," );
                        Serial.println(map(OpenFIREsquare.testMedianY(), 0, 768 << 2, 0, 1080));
                    }
                }
                #ifdef USES_DISPLAY
                    OLED.DrawVisibleIR(rawX, rawY);
                #endif // USES_DISPLAY
            }
        }
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
    if(profileData[selectedProfile].irLayout) {
        if(lastSeen != OpenFIREdiamond.seen()) {
            #ifdef MAMEHOOKER
            if(!serialMode) {
            #endif // MAMEHOOKER
                #ifdef LED_ENABLE
                if(!lastSeen && OpenFIREdiamond.seen()) {
                    LedOff();
                } else if(lastSeen && !OpenFIREdiamond.seen()) {
                    SetLedPackedColor(IRSeen0Color);
                }
                #endif // LED_ENABLE
            #ifdef MAMEHOOKER
            }
            #endif // MAMEHOOKER
            lastSeen = OpenFIREdiamond.seen();
        }
    } else {
        if(lastSeen != OpenFIREsquare.seen()) {
            #ifdef MAMEHOOKER
            if(!serialMode) {
            #endif // MAMEHOOKER
                #ifdef LED_ENABLE
                if(!lastSeen && OpenFIREsquare.seen()) {
                    LedOff();
                } else if(lastSeen && !OpenFIREsquare.seen()) {
                    SetLedPackedColor(IRSeen0Color);
                }
                #endif // LED_ENABLE
            #ifdef MAMEHOOKER
            }
            #endif // MAMEHOOKER
            lastSeen = OpenFIREsquare.seen();
        }
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
            OF_FFB.FFBOnScreen();
        }
    } else {  // We're shooting outside of the screen boundaries!
        if(!buttonPressed) {  // If we haven't pressed a trigger key yet,
            if(!OF_FFB.triggerHeld && offscreenButton) {  // If we are in offscreen button mode (and aren't dragging a shot offscreen)
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
        OF_FFB.FFBOffScreen();
    }
    OF_FFB.triggerHeld = true;                                   // Signal that we've started pulling the trigger this poll cycle.
}

// Handles events when trigger is released
void TriggerNotFire()
{
    OF_FFB.triggerHeld = false;                                    // Disable the holding function
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
    
    OF_FFB.FFBRelease();
}

#ifdef USES_ANALOG
void AnalogStickPoll()
{
    unsigned int analogValueX = analogRead(SamcoPreferences::pins.aStickX);
    unsigned int analogValueY = analogRead(SamcoPreferences::pins.aStickY);
    // Analog stick deadzone should help mitigate overwriting USB commands for the other input channels.
    if((analogValueX < 1900 || analogValueX > 2200) ||
       (analogValueY < 1900 || analogValueY > 2200)) {
          Gamepad16.moveStick(analogValueX, analogValueY);
    } else {
        // Duplicate coords won't be reported, so no worries.
        Gamepad16.moveStick(2048, 2048);
    }
}
#endif // USES_ANALOG

// Limited subset of SerialProcessing specifically for "docked" mode
// contains setting submethods for use by the desktop app
void SerialProcessingDocked()
{
    char serialInput = Serial.read();

    switch(serialInput) {
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Set IR Brightness
              case 'B':
              {
                byte lvl = Serial.read() - '0';
                if(lvl >= 0 && lvl <= 2) {
                    if(gunMode != GunMode_Pause || gunMode != GunMode_Docked) {
                        Serial.println("Can't set sensitivity in run mode! Please enter pause mode if you'd like to change IR sensitivity.");
                    } else {
                        SetIrSensitivity(lvl);
                    }
                } else {
                    Serial.println("SERIALREAD: No valid IR sensitivity level set! (Expected 0 to 2)");
                }
                break;
              }
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
              {
                byte i = Serial.read() - '0';
                if(i >= 1 && i <= 4) {
                    SelectCalProfile(i-1);
                    Serial.print("Profile: ");
                    Serial.println(i-1);
                    if(Serial.peek() == 'C') {
                        Serial.read(); // nomf
                        dockedCalibrating = true;
                        //Serial.print("Now calibrating selected profile: ");
                        //Serial.println(profileDesc[selectedProfile].profileLabel);
                        SetMode(GunMode_Calibration);
                    }
                }
                break;
              }
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
                    if(gunMode == GunMode_Docked) {
                        LedUpdate(127, 127, 255);
                    } else if(gunMode == GunMode_Pause) {
                        SetLedPackedColor(profileData[selectedProfile].color);
                    }
                    #endif // LED_ENABLE
                }
                CameraSet();
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
                    Serial.read(); // nomf
                    serialInput = Serial.read();
                    // bool change
                    if(serialInput == '0') {
                        Serial.read(); // nomf
                        byte sCase = Serial.parseInt();
                        switch(sCase) {
                          case SamcoPreferences::Bool_CustomPins:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.customPinsInUse = Serial.read() - '0';
                            SamcoPreferences::toggles.customPinsInUse = constrain(SamcoPreferences::toggles.customPinsInUse, 0, 1);
                            Serial.println("OK: Toggled Custom Pin setting.");
                            break;
                          #ifdef USES_RUMBLE
                          case SamcoPreferences::Bool_Rumble:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.rumbleActive = Serial.read() - '0';
                            SamcoPreferences::toggles.rumbleActive = constrain(SamcoPreferences::toggles.rumbleActive, 0, 1);
                            Serial.println("OK: Toggled Rumble setting.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case SamcoPreferences::Bool_Solenoid:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.solenoidActive = Serial.read() - '0';
                            SamcoPreferences::toggles.solenoidActive = constrain(SamcoPreferences::toggles.solenoidActive, 0, 1);
                            Serial.println("OK: Toggled Solenoid setting.");
                            break;
                          #endif
                          case SamcoPreferences::Bool_Autofire:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.autofireActive = Serial.read() - '0';
                            SamcoPreferences::toggles.autofireActive = constrain(SamcoPreferences::toggles.autofireActive, 0, 1);
                            Serial.println("OK: Toggled Autofire setting.");
                            break;
                          case SamcoPreferences::Bool_SimpleMenu:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.simpleMenu = Serial.read() - '0';
                            SamcoPreferences::toggles.simpleMenu = constrain(SamcoPreferences::toggles.simpleMenu, 0, 1);
                            Serial.println("OK: Toggled Simple Pause Menu setting.");
                            break;
                          case SamcoPreferences::Bool_HoldToPause:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.holdToPause = Serial.read() - '0';
                            SamcoPreferences::toggles.holdToPause = constrain(SamcoPreferences::toggles.holdToPause, 0, 1);
                            Serial.println("OK: Toggled Hold to Pause setting.");
                            break;
                          #ifdef FOURPIN_LED
                          case SamcoPreferences::Bool_CommonAnode:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.commonAnode = Serial.read() - '0';
                            SamcoPreferences::toggles.commonAnode = constrain(SamcoPreferences::toggles.commonAnode, 0, 1);
                            Serial.println("OK: Toggled Common Anode setting.");
                            break;
                          #endif
                          case SamcoPreferences::Bool_LowButtons:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.lowButtonMode = Serial.read() - '0';
                            SamcoPreferences::toggles.lowButtonMode = constrain(SamcoPreferences::toggles.lowButtonMode, 0, 1);
                            Serial.println("OK: Toggled Low Button Mode setting.");
                            break;
                          case SamcoPreferences::Bool_RumbleFF:
                            Serial.read(); // nomf
                            SamcoPreferences::toggles.rumbleFF = Serial.read() - '0';
                            SamcoPreferences::toggles.rumbleFF = constrain(SamcoPreferences::toggles.rumbleFF, 0, 1);
                            Serial.println("OK: Toggled Rumble FF setting.");
                            break;
                          default:
                            while(!Serial.available()) {
                              Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    // Pins
                    } else if(serialInput == '1') {
                        Serial.read(); // nomf
                        byte sCase = Serial.parseInt();
                        switch(sCase) {
                          case SamcoPreferences::Pin_Trigger:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bTrigger = Serial.parseInt();
                            SamcoPreferences::pins.bTrigger = constrain(SamcoPreferences::pins.bTrigger, -1, 40);
                            Serial.println("OK: Set trigger button pin.");
                            break;
                          case SamcoPreferences::Pin_GunA:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunA = Serial.parseInt();
                            SamcoPreferences::pins.bGunA = constrain(SamcoPreferences::pins.bGunA, -1, 40);
                            Serial.println("OK: Set A button pin.");
                            break;
                          case SamcoPreferences::Pin_GunB:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunB = Serial.parseInt();
                            SamcoPreferences::pins.bGunB = constrain(SamcoPreferences::pins.bGunB, -1, 40);
                            Serial.println("OK: Set B button pin.");
                            break;
                          case SamcoPreferences::Pin_GunC:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunC = Serial.parseInt();
                            SamcoPreferences::pins.bGunC = constrain(SamcoPreferences::pins.bGunC, -1, 40);
                            Serial.println("OK: Set C button pin.");
                            break;
                          case SamcoPreferences::Pin_Start:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bStart = Serial.parseInt();
                            SamcoPreferences::pins.bStart = constrain(SamcoPreferences::pins.bStart, -1, 40);
                            Serial.println("OK: Set Start button pin.");
                            break;
                          case SamcoPreferences::Pin_Select:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bSelect = Serial.parseInt();
                            SamcoPreferences::pins.bSelect = constrain(SamcoPreferences::pins.bSelect, -1, 40);
                            Serial.println("OK: Set Select button pin.");
                            break;
                          case SamcoPreferences::Pin_GunUp:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunUp = Serial.parseInt();
                            SamcoPreferences::pins.bGunUp = constrain(SamcoPreferences::pins.bGunUp, -1, 40);
                            Serial.println("OK: Set D-Pad Up button pin.");
                            break;
                          case SamcoPreferences::Pin_GunDown:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunDown = Serial.parseInt();
                            SamcoPreferences::pins.bGunDown = constrain(SamcoPreferences::pins.bGunDown, -1, 40);
                            Serial.println("OK: Set D-Pad Down button pin.");
                            break;
                          case SamcoPreferences::Pin_GunLeft:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunLeft = Serial.parseInt();
                            SamcoPreferences::pins.bGunLeft = constrain(SamcoPreferences::pins.bGunLeft, -1, 40);
                            Serial.println("OK: Set D-Pad Left button pin.");
                            break;
                          case SamcoPreferences::Pin_GunRight:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bGunRight = Serial.parseInt();
                            SamcoPreferences::pins.bGunRight = constrain(SamcoPreferences::pins.bGunRight, -1, 40);
                            Serial.println("OK: Set D-Pad Right button pin.");
                            break;
                          case SamcoPreferences::Pin_Pedal:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bPedal = Serial.parseInt();
                            SamcoPreferences::pins.bPedal = constrain(SamcoPreferences::pins.bPedal, -1, 40);
                            Serial.println("OK: Set External Pedal button pin.");
                            break;
                          case SamcoPreferences::Pin_Pedal2:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bPedal2 = Serial.parseInt();
                            SamcoPreferences::pins.bPedal2 = constrain(SamcoPreferences::pins.bPedal2, -1, 40);
                            Serial.println("OK: Set External Pedal 2 button pin.");
                            break;
                          case SamcoPreferences::Pin_Home:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bHome = Serial.parseInt();
                            SamcoPreferences::pins.bHome = constrain(SamcoPreferences::pins.bHome, -1, 40);
                            Serial.println("OK: Set Home button pin.");
                            break;
                          case SamcoPreferences::Pin_Pump:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.bPump = Serial.parseInt();
                            SamcoPreferences::pins.bPump = constrain(SamcoPreferences::pins.bPump, -1, 40);
                            Serial.println("OK: Set Pump Action button pin.");
                            break;
                          #ifdef USES_RUMBLE
                          case SamcoPreferences::Pin_RumbleSignal:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.oRumble = Serial.parseInt();
                            SamcoPreferences::pins.oRumble = constrain(SamcoPreferences::pins.oRumble, -1, 40);
                            Serial.println("OK: Set Rumble signal pin.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case SamcoPreferences::Pin_SolenoidSignal:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.oSolenoid = Serial.parseInt();
                            SamcoPreferences::pins.oSolenoid = constrain(SamcoPreferences::pins.oSolenoid, -1, 40);
                            Serial.println("OK: Set Solenoid signal pin.");
                            break;
                          #endif
                          #ifdef USES_SWITCHES
                          #ifdef USES_RUMBLE
                          case SamcoPreferences::Pin_RumbleSwitch:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.sRumble = Serial.parseInt();
                            SamcoPreferences::pins.sRumble = constrain(SamcoPreferences::pins.sRumble, -1, 40);
                            Serial.println("OK: Set Rumble Switch pin.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case SamcoPreferences::Pin_SolenoidSwitch:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.sSolenoid = Serial.parseInt();
                            SamcoPreferences::pins.sSolenoid = constrain(SamcoPreferences::pins.sSolenoid, -1, 40);
                            Serial.println("OK: Set Solenoid Switch pin.");
                            break;
                          #endif
                          case SamcoPreferences::Pin_AutofireSwitch:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.sAutofire = Serial.parseInt();
                            SamcoPreferences::pins.sAutofire = constrain(SamcoPreferences::pins.sAutofire, -1, 40);
                            Serial.println("OK: Set Autofire Switch pin.");
                            break;
                          #endif
                          #ifdef CUSTOM_NEOPIXEL
                          case SamcoPreferences::Pin_NeoPixel:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.oPixel = Serial.parseInt();
                            SamcoPreferences::pins.oPixel = constrain(SamcoPreferences::pins.oPixel, -1, 40);
                            Serial.println("OK: Set Custom NeoPixel pin.");
                            break;
                          #endif
                          #ifdef FOURPIN_LED
                          case SamcoPreferences::Pin_LEDR:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.oLedR = Serial.parseInt();
                            SamcoPreferences::pins.oLedR = constrain(SamcoPreferences::pins.oLedR, -1, 40);
                            Serial.println("OK: Set RGB LED R pin.");
                            break;
                          case SamcoPreferences::Pin_LEDG:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.oLedG = Serial.parseInt();
                            SamcoPreferences::pins.oLedG = constrain(SamcoPreferences::pins.oLedG, -1, 40);
                            Serial.println("OK: Set RGB LED G pin.");
                            break;
                          case SamcoPreferences::Pin_LEDB:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.oLedB = Serial.parseInt();
                            SamcoPreferences::pins.oLedB = constrain(SamcoPreferences::pins.oLedB, -1, 40);
                            Serial.println("OK: Set RGB LED B pin.");
                            break;
                          #endif
                          case SamcoPreferences::Pin_CameraSDA:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.pCamSDA = Serial.parseInt();
                            SamcoPreferences::pins.pCamSDA = constrain(SamcoPreferences::pins.pCamSDA, -1, 40);
                            Serial.println("OK: Set Camera SDA pin.");
                            break;
                          case SamcoPreferences::Pin_CameraSCL:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.pCamSCL = Serial.parseInt();
                            SamcoPreferences::pins.pCamSCL = constrain(SamcoPreferences::pins.pCamSCL, -1, 40);
                            Serial.println("OK: Set Camera SCL pin.");
                            break;
                          case SamcoPreferences::Pin_PeripheralSDA:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.pPeriphSDA = Serial.parseInt();
                            SamcoPreferences::pins.pPeriphSDA = constrain(SamcoPreferences::pins.pPeriphSDA, -1, 40);
                            Serial.println("OK: Set Peripherals SDA pin.");
                            break;
                          case SamcoPreferences::Pin_PeripheralSCL:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.pPeriphSCL = Serial.parseInt();
                            SamcoPreferences::pins.pPeriphSCL = constrain(SamcoPreferences::pins.pPeriphSCL, -1, 40);
                            Serial.println("OK: Set Peripherals SCL pin.");
                            break;
                          case SamcoPreferences::Pin_Battery:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.aBattRead = Serial.parseInt();
                            SamcoPreferences::pins.aBattRead = constrain(SamcoPreferences::pins.aBattRead, -1, 40);
                            Serial.println("OK: Set Battery Sensor pin.");
                            break;
                          #ifdef USES_ANALOG
                          case SamcoPreferences::Pin_AnalogX:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.aStickX = Serial.parseInt();
                            SamcoPreferences::pins.aStickX = constrain(SamcoPreferences::pins.aStickX, -1, 40);
                            Serial.println("OK: Set Analog X pin.");
                            break;
                          case SamcoPreferences::Pin_AnalogY:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.aStickY = Serial.parseInt();
                            SamcoPreferences::pins.aStickY = constrain(SamcoPreferences::pins.aStickY, -1, 40);
                            Serial.println("OK: Set Analog Y pin.");
                            break;
                          #endif
                          #ifdef USES_TEMP
                          case SamcoPreferences::Pin_AnalogTMP:
                            Serial.read(); // nomf
                            SamcoPreferences::pins.aTMP36 = Serial.parseInt();
                            SamcoPreferences::pins.aTMP36 = constrain(SamcoPreferences::pins.aTMP36, -1, 40);
                            Serial.println("OK: Set Temperature Sensor pin.");
                            break;
                          #endif
                          default:
                            while(!Serial.available()) {
                              Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    // Extended Settings
                    } else if(serialInput == '2') {
                        Serial.read(); // nomf
                        byte sCase = Serial.parseInt();
                        switch(sCase) {
                          #ifdef USES_RUMBLE
                          case SamcoPreferences::Setting_RumbleIntensity:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.rumbleIntensity = Serial.parseInt();
                            SamcoPreferences::settings.rumbleIntensity = constrain(SamcoPreferences::settings.rumbleIntensity, 0, 255);
                            Serial.println("OK: Set Rumble Intensity setting.");
                            break;
                          case SamcoPreferences::Setting_RumbleInterval:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.rumbleInterval = Serial.parseInt();
                            Serial.println("OK: Set Rumble Length setting.");
                            break;
                          #endif
                          #ifdef USES_SOLENOID
                          case SamcoPreferences::Setting_SolenoidNormInt:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.solenoidNormalInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Normal Interval setting.");
                            break;
                          case SamcoPreferences::Setting_SolenoidFastInt:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.solenoidFastInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Fast Interval setting.");
                            break;
                          case SamcoPreferences::Setting_SolenoidLongInt:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.solenoidLongInterval = Serial.parseInt();
                            Serial.println("OK: Set Solenoid Hold Length setting.");
                            break;
                          #endif
                          case SamcoPreferences::Setting_AutofireFactor:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.autofireWaitFactor = Serial.parseInt();
                            SamcoPreferences::settings.autofireWaitFactor = constrain(SamcoPreferences::settings.autofireWaitFactor, 2, 4);
                            Serial.println("OK: Set Autofire Wait Factor setting.");
                            break;
                          case SamcoPreferences::Setting_PauseHoldLength:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.pauseHoldLength = Serial.parseInt();
                            Serial.println("OK: Set Hold to Pause length setting.");
                            break;
                          #ifdef CUSTOM_NEOPIXEL
                          case SamcoPreferences::Setting_CustomLEDCount:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.customLEDcount = Serial.parseInt();
                            SamcoPreferences::settings.customLEDcount = constrain(SamcoPreferences::settings.customLEDcount, 1, 255);
                            Serial.println("OK: Set NeoPixel strand length setting.");
                            break;
                          case SamcoPreferences::Setting_CustomLEDStatic:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.customLEDstatic = Serial.parseInt();
                            SamcoPreferences::settings.customLEDstatic = constrain(SamcoPreferences::settings.customLEDstatic, 0, 3);
                            Serial.println("OK: Set Static Pixels Count setting.");
                            break;
                          case SamcoPreferences::Setting_Color1:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.customLEDcolor1 = Serial.parseInt();
                            Serial.println("OK: Set Static Color 1 setting.");
                            break;
                          case SamcoPreferences::Setting_Color2:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.customLEDcolor2 = Serial.parseInt();
                            Serial.println("OK: Set Static Color 2 setting.");
                            break;
                          case SamcoPreferences::Setting_Color3:
                            Serial.read(); // nomf
                            SamcoPreferences::settings.customLEDcolor3 = Serial.parseInt();
                            Serial.println("OK: Set Static Color 3 setting.");
                            break;
                          #endif
                          default:
                            while(!Serial.available()) {
                              Serial.read(); // nomf it all
                            }
                            Serial.println("NOENT: No matching case (feature disabled).");
                            break;
                        }
                    #ifdef USE_TINYUSB
                    // TinyUSB Identifier Settings
                    } else if(serialInput == '3') {
                        Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          // Device PID
                          case '0':
                            {
                              Serial.read(); // nomf
                              SamcoPreferences::usb.devicePID = Serial.parseInt();
                              Serial.println("OK: Updated TinyUSB Device ID.");
                              break;
                            }
                          // Device name
                          case '1':
                            Serial.read(); // nomf
                            // clears name
                            for(byte i = 0; i < sizeof(SamcoPreferences::usb.deviceName); i++) {
                                SamcoPreferences::usb.deviceName[i] = '\0';
                            }
                            for(byte i = 0; i < sizeof(SamcoPreferences::usb.deviceName); i++) {
                                SamcoPreferences::usb.deviceName[i] = Serial.read();
                                if(!Serial.available()) {
                                    break;
                                }
                            }
                            Serial.println("OK: Updated TinyUSB Device String.");
                            break;
                        }
                    #endif // USE_TINYUSB
                    // Profile settings
                    } else if(serialInput == 'P') {
                        Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          case 'i':
                          {
                            Serial.read(); // nomf
                            uint8_t i = Serial.read() - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            Serial.read(); // nomf
                            uint8_t v = Serial.read() - '0';
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
                            Serial.read(); // nomf
                            uint8_t i = Serial.read() - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            Serial.read(); // nomf
                            uint8_t v = Serial.read() - '0';
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
                          case 'l':
                          {
                            Serial.read(); // nomf
                            uint8_t i = Serial.read() - '0';
                            i = constrain(i, 0, ProfileCount - 1);
                            Serial.read(); // nomf
                            uint8_t v = Serial.read() - '0';
                            v = constrain(v, 0, 1);
                            profileData[i].irLayout = v;
                            Serial.println("OK: Set IR layout type");
                            break;
                          }
                          case 'n':
                          {
                            Serial.read(); // nomf
                            uint8_t s = Serial.read() - '0';
                            s = constrain(s, 0, ProfileCount - 1);
                            Serial.read(); // nomf
                            for(byte i = 0; i < sizeof(profileData[s].name); i++) {
                                profileData[s].name[i] = '\0';
                            }
                            for(byte i = 0; i < sizeof(profileData[s].name); i++) {
                                profileData[s].name[i] = Serial.read();
                                if(!Serial.available()) {
                                    break;
                                }
                            }
                            Serial.println("OK: Set Profile Name");
                            break;
                          }
                          case 'c':
                          {
                            Serial.read(); // nomf
                            uint8_t s = Serial.read() - '0';
                            s = constrain(s, 0, ProfileCount - 1);
                            Serial.read(); // nomf
                            profileData[s].color = Serial.parseInt();
                            Serial.println("OK: Set Profile Color");
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
                    Serial.printf("%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n",
                    SamcoPreferences::toggles.customPinsInUse,
                    SamcoPreferences::toggles.rumbleActive,
                    SamcoPreferences::toggles.solenoidActive,
                    SamcoPreferences::toggles.autofireActive,
                    SamcoPreferences::toggles.simpleMenu,
                    SamcoPreferences::toggles.holdToPause,
                    SamcoPreferences::toggles.commonAnode,
                    SamcoPreferences::toggles.lowButtonMode,
                    SamcoPreferences::toggles.rumbleFF
                    );
                    break;
                  case 'p':
                    Serial.printf(
                    "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n",
                    SamcoPreferences::pins.bTrigger,
                    SamcoPreferences::pins.bGunA,
                    SamcoPreferences::pins.bGunB,
                    SamcoPreferences::pins.bGunC,
                    SamcoPreferences::pins.bStart,
                    SamcoPreferences::pins.bSelect,
                    SamcoPreferences::pins.bGunUp,
                    SamcoPreferences::pins.bGunDown,
                    SamcoPreferences::pins.bGunLeft,
                    SamcoPreferences::pins.bGunRight,
                    SamcoPreferences::pins.bPedal,
                    SamcoPreferences::pins.bPedal2,
                    SamcoPreferences::pins.bHome,
                    SamcoPreferences::pins.bPump,
                    SamcoPreferences::pins.oRumble,
                    SamcoPreferences::pins.oSolenoid,
                    SamcoPreferences::pins.sRumble,
                    SamcoPreferences::pins.sSolenoid,
                    SamcoPreferences::pins.sAutofire,
                    SamcoPreferences::pins.oPixel,
                    SamcoPreferences::pins.oLedR,
                    SamcoPreferences::pins.oLedG,
                    SamcoPreferences::pins.oLedB,
                    SamcoPreferences::pins.pCamSDA,
                    SamcoPreferences::pins.pCamSCL,
                    SamcoPreferences::pins.pPeriphSDA,
                    SamcoPreferences::pins.pPeriphSCL,
                    SamcoPreferences::pins.aBattRead,
                    SamcoPreferences::pins.aStickX,
                    SamcoPreferences::pins.aStickY,
                    SamcoPreferences::pins.aTMP36
                    );
                    break;
                  case 's':
                    Serial.printf("%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n",
                    SamcoPreferences::settings.rumbleIntensity,
                    SamcoPreferences::settings.rumbleInterval,
                    SamcoPreferences::settings.solenoidNormalInterval,
                    SamcoPreferences::settings.solenoidFastInterval,
                    SamcoPreferences::settings.solenoidLongInterval,
                    SamcoPreferences::settings.autofireWaitFactor,
                    SamcoPreferences::settings.pauseHoldLength,
                    SamcoPreferences::settings.customLEDcount,
                    SamcoPreferences::settings.customLEDstatic,
                    SamcoPreferences::settings.customLEDcolor1,
                    SamcoPreferences::settings.customLEDcolor2,
                    SamcoPreferences::settings.customLEDcolor3
                    );
                    break;
                  case 'P':
                    serialInput = Serial.read();
                    if(serialInput >= '0' && serialInput <= '3') {
                        uint8_t i = serialInput - '0';
                        Serial.printf("%i,%i,%i,%i,%.2f,%.2f,%i,%i,%i,%i,",
                        profileData[i].topOffset,
                        profileData[i].bottomOffset,
                        profileData[i].leftOffset,
                        profileData[i].rightOffset,
                        profileData[i].TLled,
                        profileData[i].TRled,
                        profileData[i].irSensitivity,
                        profileData[i].runMode,
                        profileData[i].irLayout,
                        profileData[i].color
                        );
                        Serial.println(profileData[i].name);
                    }
                    break;
                  #ifdef USE_TINYUSB
                  case 'i':
                    Serial.printf("%i,",SamcoPreferences::usb.devicePID);
                    if(SamcoPreferences::usb.deviceName[0] == '\0') {
                        Serial.println("SERIALREADERR01");
                    } else {
                        Serial.println(SamcoPreferences::usb.deviceName);
                    }
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
                  analogWrite(SamcoPreferences::pins.oRumble, SamcoPreferences::settings.rumbleIntensity);
                  delay(SamcoPreferences::settings.rumbleInterval);
                  digitalWrite(SamcoPreferences::pins.oRumble, LOW);
                } else if(serialInput == 'R') {
                  digitalWrite(SamcoPreferences::pins.oLedR, HIGH);
                  digitalWrite(SamcoPreferences::pins.oLedG, LOW);
                  digitalWrite(SamcoPreferences::pins.oLedB, LOW);
                } else if(serialInput == 'G') {
                  digitalWrite(SamcoPreferences::pins.oLedR, LOW);
                  digitalWrite(SamcoPreferences::pins.oLedG, HIGH);
                  digitalWrite(SamcoPreferences::pins.oLedB, LOW);
                } else if(serialInput == 'B') {
                  digitalWrite(SamcoPreferences::pins.oLedR, LOW);
                  digitalWrite(SamcoPreferences::pins.oLedG, LOW);
                  digitalWrite(SamcoPreferences::pins.oLedB, HIGH);
                }
                break;
              case 'x':
                if(Serial.peek() == 'x') { rp2040.rebootToBootloader(); }
                // we probably left the firmware by now, but eh.
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
    // For more info about Serial commands, see the OpenFIRE repo wiki.

    char serialInput = Serial.read();                              // Read the serial input one byte at a time (we'll read more later)

    switch(serialInput) {
        // Start Signal
        case 'S':
          if(serialMode) {
              Serial.println("SERIALREAD: Detected Serial Start command while already in Serial handoff mode!");
          } else {
              serialMode = true;                                         // Set it on, then!
              OF_FFB.FFBShutdown();
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
                Serial.read();
                serialInput = Serial.read();
                if(serialInput > '0') {
                    if(serialMode) {
                        offscreenButtonSerial = true;
                    } else {
                        // eh, might be useful for Linux Supermodel users.
                        offscreenButton = true;
                        Serial.println("Setting offscreen button mode on!");
                    }
                } else {
                    if(serialMode) { offscreenButtonSerial = false; }
                    else {
                        offscreenButton = false;
                        Serial.println("Setting offscreen button mode off!");
                    }
                }
                break;
              case '3':
                Serial.read();                                         // nomf
                serialARcorrection = Serial.read() - '0';
                if(!serialMode) {
                    if(serialARcorrection) { Serial.println("Setting 4:3 correction on!"); }
                    else { Serial.println("Setting 4:3 correction off!"); }
                }
                break;
              #ifdef USES_SOLENOID
              case '8':
                Serial.read();                                         // Nomf the padding bit.
                serialInput = Serial.read();                           // Read the next.
                if(serialInput == '1') {
                    OF_FFB.burstFireActive = true;
                    SamcoPreferences::toggles.autofireActive = false;
                } else if(serialInput == '2') {
                    SamcoPreferences::toggles.autofireActive = true;
                    OF_FFB.burstFireActive = false;
                } else if(serialInput == '0') {
                    SamcoPreferences::toggles.autofireActive = false;
                    OF_FFB.burstFireActive = false;
                }
                break;
              #endif // USES_SOLENOID
              #ifdef USES_DISPLAY
              case 'D':
                Serial.read();                                         // Nomf padding byte
                serialInput = Serial.read();
                switch(serialInput) {
                    case '0':
                      OLED.serialDisplayType = ExtDisplay::ScreenSerial_None;
                      break;
                    case '1':
                      OLED.serialDisplayType = ExtDisplay::ScreenSerial_Life;
                      break;
                    case '2':
                      OLED.serialDisplayType = ExtDisplay::ScreenSerial_Ammo;
                      break;
                    case '3':
                      OLED.serialDisplayType = ExtDisplay::ScreenSerial_Both;
                      break;
                }
                if(Serial.read() == 'B') {
                    OLED.lifeBar = true;
                } else { OLED.lifeBar = false; }
                // prevent glitching if currently in pause mode
                if(gunMode == GunMode_Run) {
                    if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both) {
                        OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
                    } else if(OLED.serialDisplayType > ExtDisplay::ScreenSerial_None) {
                        OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, buttons.analogOutput);
                    }
                }
                break;
              #endif // USES_DISPLAY
              default:
                if(!serialMode) {
                    Serial.println("SERIALREAD: Serial modesetting command found, but no valid set bit found!");
                }
                break;
          }
          break;
        // End Signal
        // Check to make sure that 'E' is not actually a glitched command bit
        // by ensuring that there's no adjacent bit.
        case 'E':
          if(Serial.peek() == -1) {
              if(!serialMode) {
                  Serial.println("SERIALREAD: Detected Serial End command while Serial Handoff mode is already off!");
              } else {
                  serialMode = false;                                    // Turn off serial mode then.
                  offscreenButtonSerial = false;                         // And clear the stale serial offscreen button mode flag.
                  serialQueue = 0b00000000;
                  serialARcorrection = false;
                  #ifdef USES_DISPLAY
                  OLED.serialDisplayType = ExtDisplay::ScreenSerial_None;
                  if(gunMode == GunMode_Run) { OLED.ScreenModeChange(ExtDisplay::Screen_Normal, buttons.analogOutput); }
                  #endif // USES_DISPLAY
                  #ifdef LED_ENABLE
                      serialLEDPulseColorMap = 0b00000000;               // Clear any stale serial LED pulses
                      serialLEDPulses = 0;
                      serialLEDPulsesLast = 0;
                      serialLEDPulseRising = true;
                      serialLEDR = 0;                                    // Clear stale serial LED values.
                      serialLEDG = 0;
                      serialLEDB = 0;
                      serialLEDChange = false;
                      if(gunMode == GunMode_Run) { LedOff(); }           // Turn it off, and let lastSeen handle it from here.
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
                  AbsMouse5.releaseAll();
                  Keyboard.releaseAll();
                  Serial.println("Received end serial pulse, releasing FF override.");
              }
              break;
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
                          AbsMouse5.releaseAll();
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      }
                      Gamepad16.stickRight = true;
                      Serial.println("Setting camera to the Left Stick.");
                      break;
                    case 'R':
                      if(!buttons.analogOutput) {
                          buttons.analogOutput = true;
                          AbsMouse5.releaseAll();
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      }
                      Gamepad16.stickRight = false;
                      Serial.println("Setting camera to the Right Stick.");
                      break;
                    default:
                      buttons.analogOutput = !buttons.analogOutput;
                      if(buttons.analogOutput) {
                          AbsMouse5.releaseAll();
                          Keyboard.releaseAll();
                          Serial.println("Switched to Analog Output mode!");
                      } else {
                          Gamepad16.releaseAll();
                          Keyboard.releaseAll();
                          Serial.println("Switched to Mouse Output mode!");
                      }
                      break;
                }
                #ifdef USES_DISPLAY
                    if(!serialMode && gunMode == GunMode_Run) { OLED.ScreenModeChange(ExtDisplay::Screen_Normal, buttons.analogOutput); }
                    else if(serialMode && gunMode == GunMode_Run &&
                            OLED.serialDisplayType > ExtDisplay::ScreenSerial_None &&
                            OLED.serialDisplayType < ExtDisplay::ScreenSerial_Both) {
                        OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, buttons.analogOutput);
                    }
                #endif // USES_DISPLAY
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
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();                           // Read the next number.
                if(serialInput == '1') {         // Is it a solenoid "on" command?)
                    bitSet(serialQueue, 0);                            // Queue the solenoid on bit.
                } else if(serialInput == '2' &&  // Is it a solenoid pulse command?
                !bitRead(serialQueue, 1)) {      // (and we aren't already pulsing?)
                    bitSet(serialQueue, 1);                            // Set the solenoid pulsing bit!
                    Serial.read();                                     // nomf the padding bit.
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
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
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();                           // read the next number.
                if(serialInput == '1') {         // Is it an on signal?
                    bitSet(serialQueue, 2);                            // Queue the rumble on bit.
                } else if(serialInput == '2' &&  // Is it a pulsed on signal?
                !bitRead(serialQueue, 3)) {      // (and we aren't already pulsing?)
                    bitSet(serialQueue, 3);                            // Set the rumble pulsed bit.
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
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
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 4);                            // set that here!
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
                            break;
                        }
                    }
                    serialLEDR = atoi(serialInputS);                   // And set that as the strength of the red value that's requested!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                            // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000001;               // Set the R LED as the one pulsing only (overwrites the others).
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
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
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 5);                            // set that here!
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
                            break;
                        }
                    }
                    serialLEDG = atoi(serialInputS);                   // And set that here!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                            // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000010;               // Set the G LED as the one pulsing only (overwrites the others).
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
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
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();                           // Read the next number
                if(serialInput == '1') {         // is it an "on" command?
                    bitSet(serialQueue, 6);                            // set that here!
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
                            break;
                        }
                    }
                    serialLEDB = atoi(serialInputS);                   // And set that as the strength requested here!
                } else if(serialInput == '2' &&  // else, is it a pulse command?
                !bitRead(serialQueue, 7)) {      // (and we haven't already sent a pulse command?)
                    bitSet(serialQueue, 7);                       // Set the pulse bit!
                    serialLEDPulseColorMap = 0b00000100;               // Set the B LED as the one pulsing only (overwrites the others).
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
                            break;
                        }
                    }
                    serialLEDPulses = atoi(serialInputS);              // and set that as the amount of pulses requested
                    serialLEDPulsesLast = 0;                           // reset the pulses done count.
                } else if(serialInput == '0') {  // else, it's an off command.
                    bitClear(serialQueue, 6);                          // Set the B bit off.
                    serialLEDB = 0;                                    // Clear the B value.
                }
                break;
              #endif // LED_ENABLE
              #ifdef USES_DISPLAY
              case 'D':
                serialInput = Serial.read();
                switch(serialInput) {
                  case 'A':
                  {
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
                            break;
                        }
                    }
                    serialAmmoCount = atoi(serialInputS);
                    serialAmmoCount = constrain(serialAmmoCount, 0, 99);
                    break;
                  }
                  case 'L':
                  {
                    Serial.read();                                     // nomf the padding
                    char serialInputS[4];
                    for(byte n = 0; n < 3; n++) {                      // For three runs,
                        serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                        if(Serial.peek() < '0' || Serial.peek() > '9') {
                            break;
                        }
                    }
                    serialLifeCount = atoi(serialInputS);
                    break;
                  }
                }
                // screen is handled by core 0, so just signal that it's ready to go now.
                serialDisplayChange = true;
                break;
              #endif // USES_DISPLAY
              #if !defined(USES_SOLENOID) && !defined(USES_RUMBLE) && !defined(LED_ENABLE)
              default:
                //Serial.println("SERIALREAD: Feedback command detected, but no feedback devices are built into this firmware!");
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
              analogWrite(SamcoPreferences::pins.oRumble, SamcoPreferences::settings.rumbleIntensity);              // turn/keep it on.
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
    // Display is handled in core 0
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
        #ifdef USES_DISPLAY
          if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both) {
            OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
          } else if(OLED.serialDisplayType > ExtDisplay::ScreenSerial_None) {
            OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, buttons.analogOutput);
          } else {
            OLED.ScreenModeChange(ExtDisplay::Screen_Normal, buttons.analogOutput);
          }
          OLED.TopPanelUpdate("Prof: ", profileData[selectedProfile].name);
        #endif // USES_DISPLAY
        break;
    case GunMode_Calibration:
        #ifdef USES_DISPLAY
          OLED.ScreenModeChange(ExtDisplay::Screen_Calibrating);
          OLED.TopPanelUpdate("Cali: ", profileData[selectedProfile].name);
        #endif // USES_DISPLAY
        break;
    case GunMode_Pause:
        stateFlags |= StateFlag_SavePreferencesEn | StateFlag_PrintSelectedProfile;
        #ifdef USES_DISPLAY
          OLED.ScreenModeChange(ExtDisplay::Screen_Pause);
          OLED.TopPanelUpdate("Using ", profileData[selectedProfile].name);
          if(SamcoPreferences::toggles.simpleMenu) { OLED.PauseListUpdate(pauseModeSelection); }
          else { OLED.PauseScreenShow(selectedProfile, profileData[0].name, profileData[1].name, profileData[2].name, profileData[3].name); }
        #endif // USES_DISPLAY
        break;
    case GunMode_Docked:
        stateFlags |= StateFlag_SavePreferencesEn;
        #ifdef USES_DISPLAY
          OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
        #endif // USES_DISPLAY
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
                    (SamcoPreferences::pins.sRumble >= 0 || SamcoPreferences::pins.oRumble == -1)) {
                        pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    (SamcoPreferences::pins.sSolenoid >= 0 || SamcoPreferences::pins.oSolenoid == -1)) {
                        pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #else
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    !(SamcoPreferences::pins.oRumble >= 0)) {
                        pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    !(SamcoPreferences::pins.oSolenoid >= 0)) {
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
                    (SamcoPreferences::pins.sSolenoid >= 0 || SamcoPreferences::pins.oSolenoid == -1)) {
                        pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    (SamcoPreferences::pins.sRumble >= 0 || SamcoPreferences::pins.oRumble == -1)) {
                        pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #else
                #ifdef USES_SOLENOID
                    if(pauseModeSelection == PauseMode_SolenoidToggle &&
                    !(SamcoPreferences::pins.oSolenoid >= 0)) {
                        pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(pauseModeSelection == PauseMode_RumbleToggle &&
                    !(SamcoPreferences::pins.oRumble >= 0)) {
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
    #ifdef USES_DISPLAY
        OLED.PauseListUpdate(pauseModeSelection);
    #endif // USES_DISPLAY
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
        SetLedPackedColor(profileData[profileModeSelection].color);
    #endif // LED_ENABLE
    #ifdef USES_DISPLAY
        OLED.PauseProfileUpdate(profileModeSelection, profileData[0].name, profileData[1].name, profileData[2].name, profileData[3].name);
    #endif // USES_DISPLAY
    Serial.print("Selecting profile: ");
    Serial.println(profileData[profileModeSelection].name);
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
        PrintExtras();
    }
        
    lastPrintMillis = millis();
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
    Serial.println(profileData[SamcoPreferences::profiles.selectedProfile].name);
    
    Serial.println("Profiles:");
    for(unsigned int i = 0; i < SamcoPreferences::profiles.profileCount; ++i) {
        // report if a profile has been cal'd
        if(profileData[i].topOffset && profileData[i].bottomOffset &&
           profileData[i].leftOffset && profileData[i].rightOffset) {
            size_t len = strlen(profileData[i].name);
            Serial.print(profileData[i].name);
            while(len < 18) {
                Serial.print(' ');
                ++len;
            }
            Serial.print("Top: ");
            Serial.print(profileData[i].topOffset);
            Serial.print(", Bottom: ");
            Serial.print(profileData[i].bottomOffset);
            Serial.print(", Left: ");
            Serial.print(profileData[i].leftOffset);
            Serial.print(", Right: ");
            Serial.print(profileData[i].rightOffset);
            Serial.print(", TLled: ");
            Serial.print(profileData[i].TLled);
            Serial.print(", TRled: ");
            Serial.print(profileData[i].TRled);
            //Serial.print(", AdjX: ");
            //Serial.print(profileData[i].adjX);
            //Serial.print(", AdjY: ");
            //Serial.print(profileData[i].adjY);
            Serial.print(" IR: ");
            Serial.print((unsigned int)profileData[i].irSensitivity);
            Serial.print(" Mode: ");
            Serial.print((unsigned int)profileData[i].runMode);
            Serial.print(" Layout: ");
            if(profileData[i].irLayout) {
              Serial.println("Diamond");
            } else {
              Serial.println("Square");
            }
        }
    }
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
            if(OF_FFB.burstFireActive) {
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
    nvPrefsError = SamcoPreferences::LoadProfiles();
#endif // SAMCO_FLASH_ENABLE
    VerifyPreferences();
}

// Profile sanity checks
void VerifyPreferences()
{
    // center 0 is used as "no cal data"
    for(unsigned int i = 0; i < ProfileCount; ++i) {
        if(profileData[i].rightOffset >= 32768 || profileData[i].bottomOffset >= 32768 ||
           profileData[i].topOffset >= 32768 || profileData[i].leftOffset >= 32768) {
            profileData[i].topOffset = 0;
            profileData[i].bottomOffset = 0;
            profileData[i].leftOffset = 0;
            profileData[i].rightOffset = 0;
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
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_Saving);
        #endif // USES_DISPLAY
    }
    
    // use selected profile as the default
    SamcoPreferences::profiles.selectedProfile = (uint8_t)selectedProfile;

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Save(flash);
#else
    nvPrefsError = SamcoPreferences::SaveProfiles();
#endif // SAMCO_FLASH_ENABLE
    if(nvPrefsError == SamcoPreferences::Error_Success) {
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_SaveSuccess);
        #endif // USES_DISPLAY
        Serial.print("Settings saved to ");
        Serial.println(NVRAMlabel);
        SamcoPreferences::SaveToggles();
        if(SamcoPreferences::toggles.customPinsInUse) {
            SamcoPreferences::SavePins();
        }
        SamcoPreferences::SaveSettings();
        SamcoPreferences::SaveUSBID();
        #ifdef LED_ENABLE
            for(byte i = 0; i < 3; i++) {
                LedUpdate(25,25,255);
                delay(55);
                LedOff();
                delay(40);
            }
        #endif // LED_ENABLE
    } else {
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_SaveError);
        #endif // USES_DISPLAY
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
    #ifdef USES_DISPLAY
        if(gunMode == GunMode_Docked) { OLED.ScreenModeChange(ExtDisplay::Screen_Docked); }
        else if(gunMode == GunMode_Pause) {
          OLED.ScreenModeChange(ExtDisplay::Screen_Pause);
          if(SamcoPreferences::toggles.simpleMenu) { OLED.PauseListUpdate(ExtDisplay::ScreenPause_Save); }
          else { OLED.PauseScreenShow(selectedProfile, profileData[0].name, profileData[1].name, profileData[2].name, profileData[3].name); }
        }
    #endif // USES_DISPLAY
}

void SelectCalProfileFromBtnMask(uint32_t mask)
{
    // only check if buttons are set in the mask
    if(!mask) {
        return;
    }
    for(unsigned int i = 0; i < ProfileCount; ++i) {
        if(profileData[i].buttonMask == mask) {
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

void PrintSelectedProfile()
{
    Serial.print("Profile: ");
    Serial.println(profileData[selectedProfile].name);
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

    OpenFIREper.source(profileData[selectedProfile].adjX, profileData[selectedProfile].adjY);                                                          
    OpenFIREper.deinit(0);

    // set IR sensitivity
    if(profileData[profile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max) {
        SetIrSensitivity(profileData[profile].irSensitivity);
    }

    // set run mode
    if(profileData[profile].runMode < RunMode_Count) {
        SetRunMode((RunMode_e)profileData[profile].runMode);
    }

    #ifdef USES_DISPLAY
        if(gunMode != GunMode_Docked) { OLED.TopPanelUpdate("Using ", profileData[selectedProfile].name); }
    #endif // USES_DISPLAY
 
    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE

    // enable save to allow setting new default profile
    stateFlags |= StateFlag_SavePreferencesEn;
    return true;
}

/*
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
*/

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
    dotstar.setPixelColor(0, color);
    dotstar.show();
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    neopixel.setPixelColor(0, color);
    neopixel.show();
#endif // NEOPIXEL_PIN
#ifdef CUSTOM_NEOPIXEL
    if(SamcoPreferences::pins.oPixel >= 0) {
        if(SamcoPreferences::settings.customLEDstatic < SamcoPreferences::settings.customLEDcount) {
            externPixel->fill(color, SamcoPreferences::settings.customLEDstatic);
            externPixel->show();
        }
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
            if(SamcoPreferences::settings.customLEDstatic < SamcoPreferences::settings.customLEDcount) {
                externPixel->fill(Adafruit_NeoPixel::Color(r, g, b), SamcoPreferences::settings.customLEDstatic);
                externPixel->show();
            }
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
    case GunMode_Calibration:
        SetLedPackedColor(CalModeColor);
        break;
    case GunMode_Pause:
        SetLedPackedColor(profileData[selectedProfile].color);
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
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
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
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
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
            SetLedPackedColor(profileData[selectedProfile].color);    // And reset the LED back to pause mode color
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
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
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
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
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
    SamcoPreferences::toggles.rumbleActive = !SamcoPreferences::toggles.rumbleActive;
    if(SamcoPreferences::toggles.rumbleActive) {
        if(!serialMode) { Serial.println("Rumble enabled!"); }
        #ifdef USES_DISPLAY
            OLED.TopPanelUpdate("Toggli", "ng Rumble ON");
        #endif // USES_DISPLAY
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);
        #endif // LED_ENABLE
        digitalWrite(SamcoPreferences::pins.oRumble, HIGH);       // Pulse the motor on to notify the user,
        delay(300);                                               // Hold that,
        digitalWrite(SamcoPreferences::pins.oRumble, LOW);        // Then turn off,
        #ifdef LED_ENABLE
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
    } else {                                                      // Or if we're turning it OFF,
        if(!serialMode) { Serial.println("Rumble disabled!"); }
        #ifdef USES_DISPLAY
            OLED.TopPanelUpdate("Toggli", "ng Rumble OFF");
        #endif // USES_DISPLAY
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Salmon);                 // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
    }
    #ifdef USES_DISPLAY
        OLED.TopPanelUpdate("Using ", profileData[selectedProfile].name);
    #endif // USES_DISPLAY
}
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
// Pause mode solenoid enabling widget
// Does a cute solenoid engagement, or blinks LEDs (if any)
void SolenoidToggle()
{
    SamcoPreferences::toggles.solenoidActive = !SamcoPreferences::toggles.solenoidActive;                             // Toggle
    if(SamcoPreferences::toggles.solenoidActive) {                                          // If we turned ON this mode,
        if(!serialMode) { Serial.println("Solenoid enabled!"); }
        #ifdef USES_DISPLAY
            OLED.TopPanelUpdate("Toggli", "ng Solenoid ON");
        #endif // USES_DISPLAY
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
        #endif // LED_ENABLE
        digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);                          // Engage the solenoid on to notify the user,
        delay(300);                                               // Hold it that way for a bit,
        digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                           // Release it,
        #ifdef LED_ENABLE
            SetLedPackedColor(profileData[selectedProfile].color);    // And reset the LED back to pause mode color
        #endif // LED_ENABLE
    } else {                                                      // Or if we're turning it OFF,
        if(!serialMode) { Serial.println("Solenoid disabled!"); }
        #ifdef USES_DISPLAY
            OLED.TopPanelUpdate("Toggli", "ng Solenoid OFF");
        #endif // USES_DISPLAY
        #ifdef LED_ENABLE
            SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
            delay(150);                                           // Keep it on,
            LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            SetLedPackedColor(WikiColor::Yellow);                 // Flicker it back on
            delay(150);                                           // for a bit,
            LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            SetLedPackedColor(profileData[selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
    }
    #ifdef USES_DISPLAY
        OLED.TopPanelUpdate("Using ", profileData[selectedProfile].name);
    #endif // USES_DISPLAY
}
#endif // USES_SOLENOID

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
    LightgunButtons::ButtonDesc[BtnIdx_Pedal2].pin = SamcoPreferences::pins.bPedal2;
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
