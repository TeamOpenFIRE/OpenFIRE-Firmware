/*
 * This module (hastily) combines Mike Lynch's modified "AbsMouse5" library, and
 * Chris Young's TinyUSB Mouse and Keyboard library (the Keyboard half, anyways),
 * which in itself uses pieces of Arduino's basic Keyboard library.
 */
#ifdef USE_TINYUSB
  #include <Adafruit_TinyUSB.h>
#elifdef CFG_TUSB_MCU
  #error Incompatible USB stack. Use Adafruit TinyUSB.
#else
  #include <HID.h>
#endif

#include "TinyUSB_Devices.h"
#if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
#include <HID_Bluetooth.h>
#include <PicoBluetoothHID.h>
#endif // ARDUINO_RASPBERRY_PI_PICO_W

/*****************************
 *   GLOBAL SECTION
 *****************************/
Adafruit_USBD_HID usbHid;

enum HID_RID_e{
    HID_RID_KEYBOARD = 1,
    HID_RID_MOUSE,
    HID_RID_GAMEPAD
};

uint8_t desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_ABSMOUSE5(HID_REPORT_ID(2)),
    TUD_HID_REPORT_DESC_GAMEPAD16(HID_REPORT_ID(3))
};

#if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
enum HID_BT_e {
    HID_BT_KEYBOARD = 1,
    HID_BT_CONSUMER,
    HID_BT_MOUSE,
    HID_BT_GAMEPAD
};

uint8_t desc_bt_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_BT_KEYBOARD)),
    TUD_HID_REPORT_DESC_ABSMOUSE5(HID_REPORT_ID(HID_BT_MOUSE))//,
    //TUD_HID_REPORT_DESC_GAMEPAD16(HID_REPORT_ID(2))
};
#endif // ARDUINO_RASPBERRY_PI_PICO_W

TinyUSBDevices_::TinyUSBDevices_(void) {
}

void TinyUSBDevices_::begin(byte polRate) {
    usbHid.setPollInterval(polRate);
    usbHid.setReportDescriptor(desc_hid_report, sizeof(desc_hid_report));
    usbHid.begin();
    onBattery = false;
}

#if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
void TinyUSBDevices_::beginBT(const char *localName, const char *hidName) {
    // third arg is the type of device that this is exposed as, i.e. the icon displayed on the PC.
    // for BLE: 0x03C2 is mouse, 0x03C1 is keyboard, 0x03C4 is gamepad, 0x03C0 is "generic" bluetooth icon
    // for BT: 0x2580 is mouse, 0x2540 is keyboard, 0x2508 is gamepad, 0x25C0 is "combo".
    // also bluetooth classic for some reason has a "subclass"?
    PicoBluetoothHID.startHID(localName, hidName, 0x2580, 33, desc_bt_report, sizeof(desc_bt_report));
    onBattery = true;
}
#endif // ARDUINO_RASPBERRY_PI_PICO_W

TinyUSBDevices_ TinyUSBDevices;
  
/*****************************
 *   MOUSE SECTION
 *****************************/

#if defined(_USING_HID)
static const uint8_t HID_REPORT_DESCRIPTOR5[] PROGMEM = {
	0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
	0x09, 0x02,        // Usage (Mouse)
	0xA1, 0x01,        // Collection (Application)
	0x09, 0x01,        //   Usage (Pointer)
	0xA1, 0x00,        //   Collection (Physical)
	0x85, 0x01,        //     Report ID (1)
	0x05, 0x09,        //     Usage Page (Button)
	0x19, 0x01,        //     Usage Minimum (0x01)
	0x29, 0x05,        //     Usage Maximum (0x05)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x95, 0x05,        //     Report Count (5)
	0x75, 0x01,        //     Report Size (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x95, 0x01,        //     Report Count (1)
	0x75, 0x03,        //     Report Size (3)
	0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x09, 0x30,        //     Usage (X)
	0x09, 0x31,        //     Usage (Y)
	0x16, 0x00, 0x00,  //     Logical Minimum (0)
	0x26, 0xFF, 0x7F,  //     Logical Maximum (32767)
	0x36, 0x00, 0x00,  //     Physical Minimum (0)
	0x46, 0xFF, 0x7F,  //     Physical Maximum (32767)
	0x75, 0x10,        //     Report Size (16)
	0x95, 0x02,        //     Report Count (2)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,              //   End Collection
	0xC0               // End Collection
};
#endif // _USING_HID

AbsMouse5_::AbsMouse5_(uint8_t reportId) : _reportId(reportId), _buttons(0), _x(0), _y(0), _autoReport(true)
{
#if defined(_USING_HID)
	static HIDSubDescriptor descriptorNode(HID_REPORT_DESCRIPTOR5, sizeof(HID_REPORT_DESCRIPTOR5));
	HID().AppendDescriptor(&descriptorNode);
#endif // _USING_HID
}

void AbsMouse5_::init(bool autoReport)
{
	_autoReport = autoReport;
}

void AbsMouse5_::report(void)
{
	uint8_t buffer[5];
	buffer[0] = _buttons;
	buffer[1] = _x & 0xFF;
	buffer[2] = (_x >> 8) & 0xFF;
	buffer[3] = _y & 0xFF;
	buffer[4] = (_y >> 8) & 0xFF;

#if defined(_USING_HID)
	HID().SendReport(_reportId, buffer, 5);
#endif // _USING_HID
#if defined(USE_TINYUSB)
    #if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
    if(TinyUSBDevices.onBattery) {
      PicoBluetoothHID.send(HID_BT_MOUSE, buffer, 5);
    } else {
      while(!usbHid.ready()) yield();
      usbHid.sendReport(HID_RID_MOUSE, buffer, 5);
    }
    #else
    while(!usbHid.ready()) yield();
    usbHid.sendReport(HID_RID_MOUSE, buffer, 5);
    #endif // ARDUINO_RASPBERRY_PI_PICO_W
#endif // USE_TINYUSB
}

void AbsMouse5_::move(uint16_t x, uint16_t y)
{
	if(x != _x || y != _y) {
		_x = x;
		_y = y;
		if(_autoReport) {
			report();
		}
	}
}

void AbsMouse5_::press(uint8_t button)
{
	_buttons |= button;

	if(_autoReport) {
		report();
	}
}

void AbsMouse5_::release(uint8_t button)
{
	_buttons &= ~button;

	if(_autoReport) {
		report();
	}
}
  
 /*****************************
 *   KEYBOARD SECTION
 *****************************/ 

  Keyboard_::Keyboard_(void) {
  }
  
  void Keyboard_::sendReport(KeyReport* keys)
  {
    #if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
    if(TinyUSBDevices.onBattery) {
      PicoBluetoothHID.send(HID_BT_KEYBOARD, keys, sizeof(keys));
    } else {
      if ( USBDevice.suspended() )  {
        USBDevice.remoteWakeup();
      }
      while(!usbHid.ready()) yield();
      usbHid.keyboardReport(HID_RID_KEYBOARD, keys->modifiers, keys->keys);
    }
    #else
    if ( USBDevice.suspended() )  {
      USBDevice.remoteWakeup();
    }
    while(!usbHid.ready()) yield();
    usbHid.keyboardReport(HID_RID_KEYBOARD, keys->modifiers, keys->keys);
    #endif // ARDUINO_RASPBERRY_PI_PICO_W
  }
  
  #define SHIFT 0x80
  const uint8_t _asciimap[128] =
  {
    0x00,             // NUL
    0x00,             // SOH
    0x00,             // STX
    0x00,             // ETX
    0x00,             // EOT
    0x00,             // ENQ
    0x00,             // ACK  
    0x00,             // BEL
    0x2a,     // BS Backspace
    0x2b,     // TAB  Tab
    0x28,     // LF Enter
    0x00,             // VT 
    0x00,             // FF 
    0x00,             // CR 
    0x00,             // SO 
    0x00,             // SI 
    0x00,             // DEL
    0x00,             // DC1
    0x00,             // DC2
    0x00,             // DC3
    0x00,             // DC4
    0x00,             // NAK
    0x00,             // SYN
    0x00,             // ETB
    0x00,             // CAN
    0x00,             // EM 
    0x00,             // SUB
    0x00,             // ESC
    0x00,             // FS 
    0x00,             // GS 
    0x00,             // RS 
    0x00,             // US 
  
    0x2c,      //  ' '
    0x1e|SHIFT,    // !
    0x34|SHIFT,    // "
    0x20|SHIFT,    // #
    0x21|SHIFT,    // $
    0x22|SHIFT,    // %
    0x24|SHIFT,    // &
    0x34,          // '
    0x26|SHIFT,    // (
    0x27|SHIFT,    // )
    0x25|SHIFT,    // *
    0x2e|SHIFT,    // +
    0x36,          // ,
    0x2d,          // -
    0x37,          // .
    0x38,          // /
    0x27,          // 0
    0x1e,          // 1
    0x1f,          // 2
    0x20,          // 3
    0x21,          // 4
    0x22,          // 5
    0x23,          // 6
    0x24,          // 7
    0x25,          // 8
    0x26,          // 9
    0x33|SHIFT,      // :
    0x33,          // ;
    0x36|SHIFT,      // <
    0x2e,          // =
    0x37|SHIFT,      // >
    0x38|SHIFT,      // ?
    0x1f|SHIFT,      // @
    0x04|SHIFT,      // A
    0x05|SHIFT,      // B
    0x06|SHIFT,      // C
    0x07|SHIFT,      // D
    0x08|SHIFT,      // E
    0x09|SHIFT,      // F
    0x0a|SHIFT,      // G
    0x0b|SHIFT,      // H
    0x0c|SHIFT,      // I
    0x0d|SHIFT,      // J
    0x0e|SHIFT,      // K
    0x0f|SHIFT,      // L
    0x10|SHIFT,      // M
    0x11|SHIFT,      // N
    0x12|SHIFT,      // O
    0x13|SHIFT,      // P
    0x14|SHIFT,      // Q
    0x15|SHIFT,      // R
    0x16|SHIFT,      // S
    0x17|SHIFT,      // T
    0x18|SHIFT,      // U
    0x19|SHIFT,      // V
    0x1a|SHIFT,      // W
    0x1b|SHIFT,      // X
    0x1c|SHIFT,      // Y
    0x1d|SHIFT,      // Z
    0x2f,          // [
    0x31,          // bslash
    0x30,          // ]
    0x23|SHIFT,    // ^
    0x2d|SHIFT,    // _
    0x35,          // `
    0x04,          // a
    0x05,          // b
    0x06,          // c
    0x07,          // d
    0x08,          // e
    0x09,          // f
    0x0a,          // g
    0x0b,          // h
    0x0c,          // i
    0x0d,          // j
    0x0e,          // k
    0x0f,          // l
    0x10,          // m
    0x11,          // n
    0x12,          // o
    0x13,          // p
    0x14,          // q
    0x15,          // r
    0x16,          // s
    0x17,          // t
    0x18,          // u
    0x19,          // v
    0x1a,          // w
    0x1b,          // x
    0x1c,          // y
    0x1d,          // z
    0x2f|SHIFT,    // {
    0x31|SHIFT,    // |
    0x30|SHIFT,    // }
    0x35|SHIFT,    // ~
    0       // DEL
  };
  
  // press() adds the specified key (printing, non-printing, or modifier)
  // to the persistent key report and sends the report.  Because of the way 
  // USB HID works, the host acts like the key remains pressed until we 
  // call release(), releaseAll(), or otherwise clear the report and resend.
  size_t Keyboard_::press(uint8_t k)
  {
    uint8_t i;
    if (k >= 136) {     // it's a non-printing key (not a modifier)
      k = k - 136;
    } else if (k >= 128) {  // it's a modifier key
      _keyReport.modifiers |= (1<<(k-128));
      k = 0;
    } else {        // it's a printing key
      k = pgm_read_byte(_asciimap + k);
      if (!k) {
        setWriteError();
        return 0;
      }
      if (k & 0x80) {           // it's a capital letter or other character reached with shift
        _keyReport.modifiers |= 0x02; // the left shift modifier
        k &= 0x7F;
      }
    }
    
    // Add k to the key report only if it's not already present
    // and if there is an empty slot.
    if (_keyReport.keys[0] != k && _keyReport.keys[1] != k && 
      _keyReport.keys[2] != k && _keyReport.keys[3] != k &&
      _keyReport.keys[4] != k && _keyReport.keys[5] != k) {
      
      for (i=0; i<6; i++) {
        if (_keyReport.keys[i] == 0x00) {
          _keyReport.keys[i] = k;
          break;
        }
      }
      if (i == 6) {
        setWriteError();
        return 0;
      } 
    }
    sendReport(&_keyReport);
    return 1;
  }
  
  // release() takes the specified key out of the persistent key report and
  // sends the report.  This tells the OS the key is no longer pressed and that
  // it shouldn't be repeated any more.
  size_t Keyboard_::release(uint8_t k)
  {
    uint8_t i;
    if (k >= 136) {     // it's a non-printing key (not a modifier)
      k = k - 136;
    } else if (k >= 128) {  // it's a modifier key
      _keyReport.modifiers &= ~(1<<(k-128));
      k = 0;
    } else {        // it's a printing key
      k = pgm_read_byte(_asciimap + k);
      if (!k) {
        return 0;
      }
      if (k & 0x80) {             // it's a capital letter or other character reached with shift
        _keyReport.modifiers &= ~(0x02);  // the left shift modifier
        k &= 0x7F;
      }
    }
    
    // Test the key report to see if k is present.  Clear it if it exists.
    // Check all positions in case the key is present more than once (which it shouldn't be)
    for (i=0; i<6; i++) {
      if (0 != k && _keyReport.keys[i] == k) {
        _keyReport.keys[i] = 0x00;
      }
    }
    sendReport(&_keyReport);
    return 1;
  }
  
  void Keyboard_::releaseAll(void)
  {
    _keyReport.keys[0] = 0;
    _keyReport.keys[1] = 0; 
    _keyReport.keys[2] = 0;
    _keyReport.keys[3] = 0; 
    _keyReport.keys[4] = 0;
    _keyReport.keys[5] = 0; 
    _keyReport.modifiers = 0;
    sendReport(&_keyReport);
  }
  
  size_t Keyboard_::write(uint8_t c)
  {
    uint8_t p = press(c);  // Keydown
    release(c);            // Keyup
    return p;              // just return the result of press() since release() almost always returns 1
  }
  
  size_t Keyboard_::write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    while (size--) {
      if (*buffer != '\r') {
        if (write(*buffer)) {
          n++;
        } else {
          break;
        }
      }
      buffer++;
    }
    return n;
  }
  Keyboard_ Keyboard;//create an instance of the Keyboard object

/*****************************
 *   GAMEPAD SECTION
 *****************************/
  Gamepad16_::Gamepad16_(void) {
  }

  void Gamepad16_::moveCam(uint16_t origX, uint16_t origY) {
    if(stickRight) {
        gamepad16Report.X = map(origX, 0, 32768, 0, 65535);
        gamepad16Report.Y = map(origY, 0, 32768, 0, 65535);
    } else {
        gamepad16Report.Rx = map(origX, 0, 32768, 0, 65535);
        gamepad16Report.Ry = map(origY, 0, 32768, 0, 65535);
    }
    if(_autoReport) {
        report();
    }
  }

  void Gamepad16_::moveStick(uint16_t origX, uint16_t origY) {
    // TODO: inverted output for Cabela's Top Shot Elite sticks, but it might be backwards for others.
    if(origX != _x || origY != _y) {
        _x = origX, _y = origY;
        if(stickRight) {
            gamepad16Report.Rx = map(_x, 0, 4095, 65535, 0);
            gamepad16Report.Ry = map(_y, 0, 4095, 65535, 0);
        } else {
            gamepad16Report.X = map(_x, 0, 4095, 65535, 0);
            gamepad16Report.Y = map(_y, 0, 4095, 65535, 0);
        }
        if(_autoReport) {
            report();
        }
    }
  }

  void Gamepad16_::press(uint8_t buttonNum) {
    bitSet(gamepad16Report.buttons, buttonNum);
    if(_autoReport) {
        report();
    }
  }

  void Gamepad16_::release(uint8_t buttonNum) {
    bitClear(gamepad16Report.buttons, buttonNum);
    if(_autoReport) {
        report();
    }
  }

  void Gamepad16_::padUpdate(uint8_t padMask) {
    gamepad16Report.hat = padMask;
    if(_autoReport) {
        report();
    }
  }

  void Gamepad16_::report() {
    #if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
    if(TinyUSBDevices.onBattery) {
      // this doesn't work for some reason :(
      //PicoBluetoothHID.send(2, &gamepad16Report, sizeof(gamepad16Report));
    } else {
      if ( USBDevice.suspended() )  {
        USBDevice.remoteWakeup();
      }
      while(!usbHid.ready()) yield();
      usbHid.sendReport(HID_RID_GAMEPAD, &gamepad16Report, sizeof(gamepad16Report));
    }
    #else
    if ( USBDevice.suspended() )  {
      USBDevice.remoteWakeup();
    }
    while(!usbHid.ready()) yield();
    usbHid.sendReport(HID_RID_GAMEPAD, &gamepad16Report, sizeof(gamepad16Report));
    #endif // ARDUINO_RASPBERRY_PI_PICO_W
  }

  void Gamepad16_::releaseAll() {
    gamepad16Report.buttons = 0;
    gamepad16Report.hat = 0;
    gamepad16Report.X = 32767;
    gamepad16Report.Y = 32767;
    gamepad16Report.Rx = 32767;
    gamepad16Report.Ry = 32767;
    report();
  }


  Gamepad16_ Gamepad16;
