/*!
 * @file SamcoPresets.h
 * @brief Board pinout presets for different microcontrollers.
 *
 * @copyright That One Seong, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author That One Seong
 * @version V1.0
 * @date 2024
 */ 

#ifndef _SAMCOPRESETS_H_
#define _SAMCOPRESETS_H_

#include <stdint.h>

  // Leave this uncommented if your build uses hardware switches, or comment out to disable all references to hw switch functionality.
#define USES_SWITCHES

  // Leave this uncommented if your build uses a rumble motor; comment out to disable any references to rumble functionality.
#define USES_RUMBLE
#ifdef USES_RUMBLE
    // If you'd rather not use a solenoid for force-feedback effects, this will change all on-screen force feedback events to use the motor instead.
    // TODO: actually finish this.
    //#define RUMBLE_FF
    #if defined(RUMBLE_FF) && defined(USES_SOLENOID)
        #error Rumble Force-feedback is incompatible with Solenoids! Use either one or the other.
    #endif // RUMBLE_FF && USES_SOLENOID
    bool rumbleActive = true;                        // Are we allowed to do rumble?
#endif // USES_RUMBLE

  // Leave this uncommented if your build uses a solenoid, or comment out to disable any references to solenoid functionality.
#define USES_SOLENOID
#ifdef USES_SOLENOID
    bool solenoidActive = true;                      // Are we allowed to use a solenoid?
    // Leave this uncommented for TMP36 temperature sensor support for a solenoid, or comment out to disable references to temperature reading or throttling.
    #define USES_TEMP
#endif // USES_SOLENOID

  // Leave this uncommented if your build uses an analog stick.
#define USES_ANALOG

  // Leave this uncommented if your build uses a four pin RGB LED.
#define FOURPIN_LED
#ifdef FOURPIN_LED
    #define LED_ENABLE
    // Set if your LED is Common Anode (+, connects to 5V) rather than Common Cathode (-, connects to GND)
    bool commonAnode = true;
#endif // FOURPIN_LED

  // Leave this uncommented if your build uses an external NeoPixel.
#define CUSTOM_NEOPIXEL
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    #include <Adafruit_NeoPixel.h>
#endif // CUSTOM_NEOPIXEL

class BoardPins {
public:
#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040 // For the Adafruit ItsyBitsy RP2040
#ifdef USES_SWITCHES
    int8_t autofireSwitch = 18;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = 19;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = 20;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
#endif // USES_SWITCHES

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

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
int8_t btnPedal = 12;
int8_t btnPump = -1;
int8_t btnHome = -1;

#elifdef ARDUINO_ADAFRUIT_KB2040_RP2040           // For the Adafruit KB2040 - GUN4IR-compatible defaults
#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
#endif // USES_SWITCHES

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        int8_t tempPin = A0;                      // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
int8_t rumblePin = 5;                             // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 7;                           // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = A2;                           // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = A3;                              // <-- GCon 1-spec
int8_t btnGunB = 4;                               // <-- GCon 1-spec
int8_t btnGunC = 6;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 9;
int8_t btnSelect = 8;
int8_t btnGunUp = 18;
int8_t btnGunDown = 20;
int8_t btnGunLeft = 19;
int8_t btnGunRight = 10;
int8_t btnPedal = -1;
int8_t btnPump = -1;
int8_t btnHome = A1;

#elifdef ARDUINO_NANO_RP2040_CONNECT
// todo: finalize arduino nano layout soon - these are just test values for now.
#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
#endif // USES_SWITCHES

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

  // Pins setup - where do things be plugged into like? Uses GPIO codes ONLY! See also: https://learn.adafruit.com/adafruit-itsybitsy-rp2040/pinouts
int8_t rumblePin = 17;
int8_t solenoidPin = 16;
int8_t btnTrigger = 15;
int8_t btnGunA = 0;
int8_t btnGunB = 1;
int8_t btnGunC = 18;
int8_t btnStart = 19;
int8_t btnSelect = 20;
int8_t btnGunUp = -1;
int8_t btnGunDown = -1;
int8_t btnGunLeft = -1;
int8_t btnGunRight = -1;
int8_t btnPedal = -1;
int8_t btnPump = -1;
int8_t btnHome = -1;

#elifdef ARDUINO_RASPBERRY_PI_PICO // For the Raspberry Pi Pico

#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
#endif // USES_SWITCHES

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        int8_t tempPin = A2;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = 10;
    int8_t PinG = 11;
    int8_t PinB = 12;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

int8_t rumblePin = 17;                            // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = 16;                          // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = 15;                           // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = 0;                               // <-- GCon 1-spec
int8_t btnGunB = 1;                               // <-- GCon 1-spec
int8_t btnGunC = 2;                               // Everything below are for GCon 2-spec only 
int8_t btnStart = 3;
int8_t btnSelect = 4;
int8_t btnGunUp = 6;
int8_t btnGunDown = 7;
int8_t btnGunLeft = 8;
int8_t btnGunRight = 9;
int8_t btnPedal = 14;
int8_t btnPump = 13;
int8_t btnHome = 5;

#else // for Generic - basically no pins mapped by default at all

#ifdef USES_SWITCHES
    int8_t autofireSwitch = -1;                   // What's the pin number of the autofire switch? Digital.
    #ifdef USES_SOLENOID
        int8_t solenoidSwitch = -1;               // What's the pin number of the solenoid switch? Digital.
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
        int8_t rumbleSwitch = -1;                 // What's the pin number of the rumble switch? Digital.
    #endif // USES_RUMBLE
#endif // USES_SWITCHES

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        int8_t tempPin = -1;                         // What's the pin number of the temp sensor? Needs to be analog.
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: ANALOG PINS ONLY!
#ifdef USES_ANALOG
    int8_t analogPinX = -1;
    int8_t analogPinY = -1;
#endif // USES_ANALOG

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    int8_t PinR = -1;
    int8_t PinG = -1;
    int8_t PinB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    int8_t customLEDpin = -1;                      // Pin number for the custom NeoPixel (strip) being used.
#endif // CUSTOM_NEOPIXEL

int8_t rumblePin = -1;                            // What's the pin number of the rumble output? Needs to be digital.
int8_t solenoidPin = -1;                          // What's the pin number of the solenoid output? Needs to be digital.
int8_t btnTrigger = -1;                           // Programmer's note: made this just to simplify the trigger pull detection, guh.
int8_t btnGunA = -1;                              // <-- GCon 1-spec
int8_t btnGunB = -1;                              // <-- GCon 1-spec
int8_t btnGunC = -1;                              // Everything below are for GCon 2-spec only 
int8_t btnStart = -1;
int8_t btnSelect = -1;
int8_t btnGunUp = -1;
int8_t btnGunDown = -1;
int8_t btnGunLeft = -1;
int8_t btnGunRight = -1;
int8_t btnPedal = -1;
int8_t btnPump = -1;
int8_t btnHome = -1;

#endif // ARDUINO_ADAFRUIT_ITSYBITSY_RP2040

};

#endif // _SAMCOPRESETS_H_
