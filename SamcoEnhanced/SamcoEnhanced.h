/*!
 * @file SamcoEnhanced.h
 * @brief OpenFIRE main control program.
 *
 * @copyright Samco, https://github.com/samuelballantyne, June 2020
 * @copyright Mike Lynch, July 2021
 * @copyright That One Seong, https://github.com/SeongGino, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @date 2025
 */

#ifndef _SAMCOENHANCED_H_
#define _SAMCOENHANCED_H_

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

#include <Wire.h>
#ifdef SAMCO_FLASH_ENABLE
    #include <Adafruit_SPIFlashBase.h>
#elif SAMCO_EEPROM_ENABLE
    #include <EEPROM.h>
#endif // SAMCO_FLASH_ENABLE/EEPROM_ENABLE

#include <DFRobotIRPositionEx.h>
#include "OpenFIREDefines.h"
#include "OpenFIREcommon.h"

#ifdef ARDUINO_ARCH_RP2040
  #include <hardware/pwm.h>
  #include <hardware/irq.h>
  // declare PWM ISR
  void rp2040pwmIrq(void);
#endif

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

// TinyUSB devices interface object that's initialized in MainCoreSetup
TinyUSBDevices_ TUSBDeviceSetup;

// Selector for which profile in the profile selector of the simple pause menu you're picking.
uint8_t profileModeSelection;
// Flag to tell if we're in the profile selector submenu of the simple pause menu.
bool pauseModeSelectingProfile = false;

// Timestamp of when we started holding a buttons combo.
unsigned long pauseHoldStartstamp;
bool pauseHoldStarted = false;
bool pauseExitHoldStarted = false;

#endif // _SAMCOENHANCED_H_