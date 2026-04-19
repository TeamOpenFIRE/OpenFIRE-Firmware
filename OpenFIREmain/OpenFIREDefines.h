 /*!
 * @file OpenFIREdefines.h
 * @brief Global precompiler definitions & build options for the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#ifndef _OPENFIREDEFINES_H_
#define _OPENFIREDEFINES_H_

#ifndef OPENFIRE_VERSION
#define OPENFIRE_VERSION 6.2
#endif // OPENFIRE_VERSION

 // For custom builders, remember to check (COMPILING.md) for IDE instructions!
 // ISSUERS: REMEMBER TO SPECIFY YOUR USING A CUSTOM BUILD & WHAT CHANGES ARE MADE TO THE SKETCH; OTHERWISE YOUR ISSUE MAY BE CLOSED!

    // GLOBAL PREDEFINES ----------------------------------------------------------------------------------------------------------
// enable extra serial debug during run mode
//#define PRINT_VERBOSE 1
//#define DEBUG_SERIAL 1
//#define DEBUG_SERIAL 2

  // Enables input processing on the second core, if available. Currently exclusive to RP2040-based microcontrollers.
  // Isn't necessarily faster, but creates more headroom on the main core to dedicate to controlling I2C peripherals
  // while the second core exclusively handles USB input polling, serial UART (when MAMEHOOKER is defined) and force feedback processing.
  // If unsure, leave this uncommented - it only affects RP2040 anyways.
#define DUAL_CORE

  // Here we define the Manufacturer Name, Device Name, and Vendor ID of the gun as will be displayed by the operating system.
  // For multiplayer, different guns need different IDs!
  // If unsure, just leave these at their defaults, as Product ID is determined by what's saved in local storage, or Player Number as a fallback.
  // For compatibility with some whitelists' support for OpenFIRE, Manufacturer Name and Device VID must be kept at the default values "OpenFIRE"/OxF143
#define MANUFACTURER_NAME "OpenFIRE"
#define DEVICE_NAME "FIRECon"
#define DEVICE_VID 0xF143

  // Set what player this board is hardcoded to.
  // This will change the function of playerStartBtn/playerSelectBtn appropriate for the respective player, regardless of Product ID.
  // If unsure, just leave commented out - the mapping is adjusted dynamically based on Product ID,
  // and can be changed at runtime by sending an 'XR#' command over Serial, where # = player number.
//#define PLAYER_NUMBER 1

#ifdef PLAYER_NUMBER
#if PLAYER_NUMBER == 1
#define PLAYER_START '1'
#define PLAYER_SELECT '5'
#elif PLAYER_NUMBER == 2
#define PLAYER_START '2'
#define PLAYER_SELECT '6'
#elif PLAYER_NUMBER == 3
#define PLAYER_START '3'
#define PLAYER_SELECT '7'
#elif PLAYER_NUMBER == 4
#define PLAYER_START '4'
#define PLAYER_SELECT '8'
#endif // PLAYER_NUMBER
#endif // PLAYER_NUMBER

  // Leave this uncommented to enable MAMEHOOKER support, or comment out (//) to disable references to serial reading and only use it for debugging.
  // WARNING: Has a chance of making the board lock up if TinyUSB hasn't been patched to fix serial-related lockups.
  // If you're building this for RP2040, please make sure that you have NOT installed the TinyUSB library.
  // If unsure, leave uncommented - serial activity is used for configuration, and undefining this will cause errors.
#define MAMEHOOKER

  // Leave this uncommented to support use of hardware switches, or comment out to disable all references to hw switch functionality.
#define USES_SWITCHES

  // Leave this uncommented to support use of rumble motors, or comment out to disable any references to rumble functionality.
#define USES_RUMBLE

  // Leave this uncommented to support use of solenoids for force feedback, or comment out to disable any references to solenoid functionality.
#define USES_SOLENOID
#ifdef USES_SOLENOID
    // Leave this uncommented for TMP36 temperature sensor support for solenoid tempering, or comment out to disable references to temperature reading or throttling.
    #define USES_TEMP
#endif // USES_SOLENOID

  // Leave this uncommented to support use of analog sticks in analog pins.
#define USES_ANALOG

  // Leave this uncommented to support use of a four pin RGB LED.
#define FOURPIN_LED
#ifdef FOURPIN_LED
    #define LED_ENABLE
#endif // FOURPIN_LED

  // Leave this uncommented to support use of an external NeoPixel strand.
#define CUSTOM_NEOPIXEL
#ifdef CUSTOM_NEOPIXEL
    #define LED_ENABLE
#endif // CUSTOM_NEOPIXEL

  // Leave this uncommented to enable support for SSD1306 monochrome OLED displays as an I2C peripheral.
#define USES_DISPLAY

#endif // _OPENFIREDEFINES_H_
