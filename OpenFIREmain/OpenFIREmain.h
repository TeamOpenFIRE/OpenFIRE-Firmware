/*!
 * @file OpenFIREmain.h
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

#ifndef _OPENFIREMAIN_H_
#define _OPENFIREMAIN_H_

#include <Arduino.h>
#include <Wire.h>
// include TinyUSB or HID depending on USB stack option
#if defined(USE_TINYUSB)
#include <Adafruit_TinyUSB.h>
#elif defined(CFG_TUSB_MCU)
#error Incompatible USB stack. Use Adafruit TinyUSB.
#endif

#include <DFRobotIRPositionEx.h>

#include <OpenFIREBoard.h>
#include "OpenFIREDefines.h"
#include "OpenFIREcommon.h"

#ifdef ARDUINO_ARCH_RP2040
  #include <hardware/pwm.h>
  #include <hardware/irq.h>
  // declare PWM ISR
  void rp2040pwmIrq(void);
#endif

// TinyUSB devices interface object that's initialized in MainCoreSetup
TinyUSBDevices_ TUSBDeviceSetup;

// Selector for which profile in the profile selector of the simple pause menu you're picking.
uint8_t profileModeSelection = 0;
// Flag to tell if we're in the profile selector submenu of the simple pause menu.
bool pauseModeSelectingProfile = false;

// Timestamp of when we started holding a buttons combo.
unsigned long pauseHoldStartstamp;
bool pauseHoldStarted = false;
bool pauseExitHoldStarted = false;

uint32_t fifoData = 0;

#endif // _SAMCOENHANCED_H_