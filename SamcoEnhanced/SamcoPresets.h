/*!
 * @file SamcoPresets.ino
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

//-----------------------------------------------------------------------------------------------------
// Per-Board Defaults:

/*
// For the Adafruit ItsyBitsy RP2040
#ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        SamcoPreferences::pins.aTMP36 = A2;
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    SamcoPreferences::pins.oLedR = -1;
    SamcoPreferences::pins.oLedG = -1;
    SamcoPreferences::pins.oLedB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    SamcoPreferences::pins.customLEDpin = -1;
#endif // CUSTOM_NEOPIXEL

SamcoPreferences::pins.oRumble = 24;
SamcoPreferences::pins.oSolenoid = 25;
SamcoPreferences::pins.bTrigger = 6;
SamcoPreferences::pins.bGunA = 7;
SamcoPreferences::pins.bGunB = 8;
SamcoPreferences::pins.bGunC = 9;
SamcoPreferences::pins.bStart = 10;
SamcoPreferences::pins.bSelect = 11;
SamcoPreferences::pins.bGunUp = 1;
SamcoPreferences::pins.bGunDown = 0;
SamcoPreferences::pins.bGunLeft = 4;
SamcoPreferences::pins.bGunRight = 5;
SamcoPreferences::pins.bPedal = 12;
SamcoPreferences::pins.bPump = -1;
SamcoPreferences::pins.bHome = -1;

// For the Adafruit KB2040 - GUN4IR-compatible defaults
#elifdef ARDUINO_ADAFRUIT_KB2040_RP2040

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        SamcoPreferences::pins.aTMP36 = A0;
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    SamcoPreferences::pins.oLedR = -1;
    SamcoPreferences::pins.oLedG = -1;
    SamcoPreferences::pins.oLedB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    SamcoPreferences::pins.oPixel = -1;
#endif // CUSTOM_NEOPIXEL

SamcoPreferences::pins.oRumble = 5;
SamcoPreferences::pins.oSolenoid = 7;
SamcoPreferences::pins.bTrigger = A2;
SamcoPreferences::pins.bGunA = A3;
SamcoPreferences::pins.bGunB = 4;
SamcoPreferences::pins.bGunC = 6;
SamcoPreferences::pins.bStart = 9;
SamcoPreferences::pins.bSelect = 8;
SamcoPreferences::pins.bGunUp = 18;
SamcoPreferences::pins.bGunDown = 20;
SamcoPreferences::pins.bGunLeft = 19;
SamcoPreferences::pins.bGunRight = 10;
SamcoPreferences::pins.bPedal = -1;
SamcoPreferences::pins.bPump = -1;
SamcoPreferences::pins.bHome = A1;

#elifdef ARDUINO_NANO_RP2040_CONNECT

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        SamcoPreferences::pins.aTMP36 = A2;
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    SamcoPreferences::pins.oLedR = -1;
    SamcoPreferences::pins.oLedG = -1;
    SamcoPreferences::pins.oLedB = -1;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    SamcoPreferences::pins.oPixel = -1;
#endif // CUSTOM_NEOPIXEL

  // Button Pins setup
SamcoPreferences::pins.oRumble = 17;
SamcoPreferences::pins.oSolenoid = 16;
SamcoPreferences::pins.bTrigger = 15;
SamcoPreferences::pins.bGunA = 0;
SamcoPreferences::pins.bGunB = 1;
SamcoPreferences::pins.bGunC = 18;
SamcoPreferences::pins.bStart = 19;
SamcoPreferences::pins.bSelect = 20;
SamcoPreferences::pins.bGunUp = -1;
SamcoPreferences::pins.bGunDown = -1;
SamcoPreferences::pins.bGunLeft = -1;
SamcoPreferences::pins.bGunRight = -1;
SamcoPreferences::pins.bPedal = -1;
SamcoPreferences::pins.bPump = -1;
SamcoPreferences::pins.bHome = -1;

// For the Raspberry Pi Pico
#elifdef ARDUINO_RASPBERRY_PI_PICO

#ifdef USES_SOLENOID
    #ifdef USES_TEMP    
        SamcoPreferences::pins.aTMP36 = A2;
    #endif // USES_TEMP
#endif // USES_SOLENOID

  // Remember: PWM PINS ONLY!
#ifdef FOURPIN_LED
    #define LED_ENABLE
    SamcoPreferences::pins.oLedR = 10;
    SamcoPreferences::pins.oLedG = 11;
    SamcoPreferences::pins.oLedB = 12;
#endif // FOURPIN_LED

  // Any digital pin is fine for NeoPixels.
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
    SamcoPreferences::pins.oPixel = -1
#endif // CUSTOM_NEOPIXEL

SamcoPreferences::pins.oRumble = 17;
SamcoPreferences::pins.oSolenoid = 16;
SamcoPreferences::pins.bTrigger = 15;
SamcoPreferences::pins.bGunA = 0;
SamcoPreferences::pins.bGunB = 1;
SamcoPreferences::pins.bGunC = 2;
SamcoPreferences::pins.bStart = 3;
SamcoPreferences::pins.bSelect = 4;
SamcoPreferences::pins.bGunUp = 6;
SamcoPreferences::pins.bGunDown = 7;
SamcoPreferences::pins.bGunLeft = 8;
SamcoPreferences::pins.bGunRight = 9;
SamcoPreferences::pins.bPedal = 14;
SamcoPreferences::pins.bPump = 13;
SamcoPreferences::pins.bHome = 5;

#endif // ARDUINO_ADAFRUIT_ITSYBITSY_RP2040

*/

#endif // _SAMCOPRESETS_H_
