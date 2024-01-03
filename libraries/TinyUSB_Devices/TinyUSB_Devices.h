/*
 * This module simulates the standard Arduino "Mouse.h" and
 * "Keyboard.h" API for use with the TinyUSB HID API. Instead of doing
 *  #include <HID.h>
 *  #include <Mouse.h>
 *  #include <Keyboard.h>
 *  
 *  Simply do
 *  
 *  #include <TinyUSB_Mouse_Keyboard.h>
 *  
 *  and this module will automatically select whether or not to use the
 *  standard Arduino mouse and keyboard API or the TinyUSB API. We had to
 *  combine them into a single library because of the way TinyUSB handles
 *  descriptors.
 *  
 *  For details on Arduino Mouse.h see
 *   https://www.arduino.cc/reference/en/language/functions/usb/mouse/
 *  For details on Arduino Keyboard.h see
 *   https://www.arduino.cc/reference/en/language/functions/usb/keyboard/
 *
 *  NOTE: This code is derived from the standard Arduino Mouse.h, Mouse.cpp,
 *    Keyboard.h, and Keyboard.cpp code. The copyright on that original code
 *    is as follows.
 *   
 *  Copyright (c) 2015, Arduino LLC
 *  Original code (pre-library): Copyright (c) 2011, Peter Barrett
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h>

/*****************************
 *   GLOBAL SECTION
 *****************************/

class TinyUSBDevices_ {
public:
  TinyUSBDevices_(void);
  void begin(byte polRate);
};
extern TinyUSBDevices_ TinyUSBDevices;

/*****************************
 *   MOUSE SECTION
 *****************************/ 
#ifndef _ABSMOUSE5_H_
#define _ABSMOUSE5_H_

#include <stdint.h>

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04
#define MOUSE_BUTTON4 0x08
#define MOUSE_BUTTON5 0x10

#if defined(USE_TINYUSB)
#define TUD_HID_REPORT_DESC_ABSMOUSE5(...) \
	0x05, 0x01, \
	0x09, 0x02, \
	0xA1, 0x01, \
	__VA_ARGS__ \
	0x09, 0x01, \
	0xA1, 0x00, \
	0x05, 0x09, \
	0x19, 0x01, \
	0x29, 0x05, \
	0x15, 0x00, \
	0x25, 0x01, \
	0x95, 0x05, \
	0x75, 0x01, \
	0x81, 0x02, \
	0x95, 0x01, \
	0x75, 0x03, \
	0x81, 0x03, \
	0x05, 0x01, \
	0x09, 0x30, \
	0x09, 0x31, \
	0x16, 0x00, 0x00, \
	0x26, 0xFF, 0x7F, \
	0x36, 0x00, 0x00, \
	0x46, 0xFF, 0x7F, \
	0x75, 0x10, \
	0x95, 0x02, \
	0x81, 0x02, \
	0xC0, \
	0xC0

#define TUD_HID_REPORT_DESC_GAMEPAD16(...) \
          0x05, 0x01, \
			0x09, 0x05, \
			0xa1, 0x01, \
              0xa1, 0x00, \
              0x85, 0x03, \
              0x05, 0x09, \
              0x19, 0x01, \
              0x29, 0x08, \
              0x15, 0x00, \
              0x25, 0x01, \
              0x75, 0x01, \
              0x95, 0x10, \
              0x81, 0x02, \
			0x05, 0x01, \
              0x09, 0x30, \
              0x09, 0x31, \
              0x15, 0x00, \
              0x27, 0xFF, 0xFF, 0x00, 0x00, \
              0x75, 0x10, \
              0x95, 0x02, \
              0x81, 0x02, \
              0xc0,       \
			0xc0
#endif // USE_TINYUSB

// 5 button absolute mouse
class AbsMouse5_
{
private:
	const uint8_t _reportId;
	uint8_t _buttons;
	uint16_t _x;
	uint16_t _y;
	uint32_t _width;
	uint32_t _height;
	bool _autoReport;

public:
	AbsMouse5_(uint8_t reportId = 1);
	void init(uint16_t width = 32767, uint16_t height = 32767, bool autoReport = true);
	void report(void);
	void move(uint16_t x, uint16_t y);
	void press(uint8_t b = MOUSE_LEFT);
	void release(uint8_t b = MOUSE_LEFT);
};

// global singleton
extern AbsMouse5_ AbsMouse5;

#endif // _ABSMOUSE5_H_

/******************************
 *    KEYBOARD SECTION
 ******************************/
  //  Keyboard codes
  //  Note these are different in some respects to the TinyUSB codes but 
  //  are compatible with Arduino Keyboard.h API
  
  #define KEY_LEFT_CTRL   0x80
  #define KEY_LEFT_SHIFT    0x81
  #define KEY_LEFT_ALT    0x82
  #define KEY_LEFT_GUI    0x83
  #define KEY_RIGHT_CTRL    0x84
  #define KEY_RIGHT_SHIFT   0x85
  #define KEY_RIGHT_ALT   0x86
  #define KEY_RIGHT_GUI   0x87
  
  #define KEY_UP_ARROW    0xDA
  #define KEY_DOWN_ARROW    0xD9
  #define KEY_LEFT_ARROW    0xD8
  #define KEY_RIGHT_ARROW   0xD7
  #define KEY_BACKSPACE   0xB2
  #define KEY_TAB       0xB3
  #define KEY_RETURN      0xB0
  #define KEY_ESC       0xB1
  #define KEY_INSERT      0xD1
  #define KEY_DELETE      0xD4
  #define KEY_PAGE_UP     0xD3
  #define KEY_PAGE_DOWN   0xD6
  #define KEY_HOME      0xD2
  #define KEY_END       0xD5
  #define KEY_CAPS_LOCK   0xC1
  #define KEY_F1        0xC2
  #define KEY_F2        0xC3
  #define KEY_F3        0xC4
  #define KEY_F4        0xC5
  #define KEY_F5        0xC6
  #define KEY_F6        0xC7
  #define KEY_F7        0xC8
  #define KEY_F8        0xC9
  #define KEY_F9        0xCA
  #define KEY_F10       0xCB
  #define KEY_F11       0xCC
  #define KEY_F12       0xCD
  #define KEY_F13       0xF0
  #define KEY_F14       0xF1
  #define KEY_F15       0xF2
  #define KEY_F16       0xF3
  #define KEY_F17       0xF4
  #define KEY_F18       0xF5
  #define KEY_F19       0xF6
  #define KEY_F20       0xF7
  #define KEY_F21       0xF8
  #define KEY_F22       0xF9
  #define KEY_F23       0xFA
  #define KEY_F24       0xFB
  
  //  Low level key report: up to 6 keys and shift, ctrl etc at once
  typedef struct
  {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keys[6];
  } KeyReport;
  
  /*
   * This class contains the exact same methods as the Arduino Keyboard.h class.
   */
  
  class Keyboard_ : public Print
  {
  private:
    KeyReport _keyReport;
    void sendReport(KeyReport* keys);
  public:
    Keyboard_(void);
    size_t write(uint8_t k);
    size_t write(const uint8_t *buffer, size_t size);
    size_t press(uint8_t k);
    size_t release(uint8_t k);
    void releaseAll(void);
  };
extern Keyboard_ Keyboard;

/*****************************
 *   GAMEPAD SECTION
 *****************************/

typedef struct {
        uint16_t buttons;     // button bitmask
		uint16_t X = 32767;
        uint16_t Y = 32767;
} gamepad16Report_s;

class Gamepad16_ {
private:
  gamepad16Report_s gamepad16Report;
public:
  Gamepad16_(void);
  void move(uint16_t origX, uint16_t origY);
  void press(uint8_t buttonNum);
  void release(uint8_t buttonNum);
  void report(void);
  void releaseAll(void);
};
extern Gamepad16_ Gamepad16;
