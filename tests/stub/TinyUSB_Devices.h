// A fake of the USB HID devices (mouse, keyboard, gamepad) that LightgunButtons reports to. It keeps what the host
// would see: which buttons and keys are down, the gamepad's hat, and how many reports were sent. Like the real
// devices, it marks a report as due only when something changed.
#pragma once

#include <bitset>
#include <cstdint>

#define PAD_A      0
#define PAD_B      1
#define PAD_C      2
#define PAD_X      3
#define PAD_Y      4
#define PAD_Z      5
#define PAD_LB     6
#define PAD_RB     7
#define PAD_LT     8
#define PAD_RT     9
#define PAD_SELECT 10
#define PAD_START  11
#define PAD_HOME   12
#define PAD_LS     13
#define PAD_RS     14
#define PAD_UP     15
#define PAD_DOWN   16
#define PAD_LEFT   17
#define PAD_RIGHT  18

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04
#define MOUSE_BUTTON4 0x08
#define MOUSE_BUTTON5 0x10

class TinyUSBDevices_ {
public:
    enum reportChannels_e { reportMouse = 0, reportKeyboard, reportGamepad };
    bool newReport[3] = {false, false, false};
    bool onBattery = false;
};
inline TinyUSBDevices_ TinyUSBDevices;

class AbsMouse5_ {
public:
    AbsMouse5_(uint8_t reportId = 1) : reportId(reportId) {}
    uint8_t reportId;
    uint16_t x = 0, y = 0;
    void move(uint16_t newX, uint16_t newY) { x = newX; y = newY; TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = true; }
    uint8_t buttons = 0;
    int reports = 0;
    void press(uint8_t button) {
        if (!(buttons & button)) { buttons |= button; TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = true; }
    }
    void release(uint8_t button) {
        if (buttons & button) { buttons &= ~button; TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = true; }
    }
    void releaseAll() { buttons = 0; TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = true; }
    void report() { reports++; TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = false; }
};
// Like the real header, this only declares the mouse: the firmware defines it (OpenFIREcommon.h), and the tests
// of the buttons library define it themselves.
extern AbsMouse5_ AbsMouse5;

class Keyboard_ {
public:
    std::bitset<256> keys;
    int reports = 0;
    bool press(uint8_t key) {
        if (!keys[key]) { keys[key] = true; TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = true; }
        return true;
    }
    bool release(uint8_t key) {
        if (keys[key]) { keys[key] = false; TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = true; }
        return true;
    }
    void releaseAll() { keys.reset(); TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = true; }
    void report() { reports++; TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = false; }
};
inline Keyboard_ Keyboard;

class Gamepad16_ {
public:
    uint32_t buttons = 0;
    uint8_t hat = 0;
    int reports = 0;
    bool stickRight = false;
    uint16_t camX = 0, camY = 0, stickX = 2048, stickY = 2048;
    void moveCam(uint16_t x, uint16_t y) { camX = x; camY = y; TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true; }
    void moveStick(uint16_t x, uint16_t y) {
        if (x != stickX || y != stickY) { stickX = x; stickY = y; TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true; }
    }
    void press(int button) {
        if (!(buttons & (1u << button))) { buttons |= 1u << button; TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true; }
    }
    void release(int button) {
        if (buttons & (1u << button)) { buttons &= ~(1u << button); TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true; }
    }
    void padUpdate(uint8_t newHat) {
        if (hat != newHat) { hat = newHat; TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true; }
    }
    void releaseAll() { buttons = 0; hat = 0; TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true; }
    void report() { reports++; TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = false; }
};
inline Gamepad16_ Gamepad16;

// Puts all three devices back to nothing pressed and nothing due.
inline void resetUsbDevices() {
    AbsMouse5 = AbsMouse5_(AbsMouse5.reportId);
    Keyboard = Keyboard_();
    Gamepad16 = Gamepad16_();
    TinyUSBDevices = TinyUSBDevices_();
}
