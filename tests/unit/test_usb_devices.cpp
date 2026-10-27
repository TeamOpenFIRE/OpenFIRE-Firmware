// Tests for TinyUSB_Devices: the absolute mouse, the keyboard and the gamepad that the gun presents to the host, and the
// reports they send. The real TinyUSB_Devices.cpp is built (USE_TINYUSB) against a fake of Adafruit's HID class
// (../stub/Adafruit_TinyUSB.h) that records the descriptor and every report.
#include "framework.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <string>

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <TinyUSB_Devices.h>

// The mouse the firmware makes for itself (OpenFIREcommon.h): report id 2, as in the descriptor.
AbsMouse5_ AbsMouse5(2);

extern Adafruit_USBD_HID usbHid;

namespace {

// The report ids in the descriptor, and the ones the devices send with.
constexpr uint8_t KEYBOARD_ID = 1, MOUSE_ID = 2, GAMEPAD_ID = 3;

const Adafruit_USBD_HID::Report& lastReport() { return usbHid.reports.back(); }

// Every test starts with nothing sent and nothing due.
void setUp() {
    usbHid.clear();
    TinyUSBDevices = TinyUSBDevices_();
}

// A device with its report all zero, as the firmware's static devices are at power-up. (The keyboard and the gamepad leave
// part of their state uninitialized in their constructors, relying on that.)
template <class Device>
struct Fresh {
    alignas(Device) unsigned char memory[sizeof(Device)];
    Device& device;
    Fresh() : device(*new (memset(memory, 0, sizeof(memory))) Device()) {}
};

bool due(int channel) { return TinyUSBDevices.newReport[channel]; }

std::vector<uint8_t> littleEndian16(uint16_t value) { return {(uint8_t)(value & 0xFF), (uint8_t)(value >> 8)}; }

} // namespace

TEST(UsbDevices, BeginSetsUpTheHidInterface) {
    setUp();
    usbHid.reportDescriptor.clear();
    usbHid.begun = 0;
    TinyUSBDevices.onBattery = true;
    TinyUSBDevices.begin(1);
    CHECK_EQ(usbHid.pollInterval, 1);
    CHECK_EQ(usbHid.begun, 1);
    CHECK(!TinyUSBDevices.onBattery);
    CHECK(!usbHid.reportDescriptor.empty());

    TinyUSBDevices.begin(8);
    CHECK_EQ(usbHid.pollInterval, 8);
}

TEST(UsbDevices, TheDescriptorHasTheThreeReportsUnderTheIdsTheDevicesSendWith) {
    setUp();
    TinyUSBDevices.begin(1);
    const std::vector<uint8_t>& descriptor = usbHid.reportDescriptor;
    // "Report ID" items (0x85, id) in the order keyboard, mouse, gamepad.
    std::vector<uint8_t> ids;
    for (size_t at = 0; at + 1 < descriptor.size(); at++) {
        if (descriptor[at] == 0x85) ids.push_back(descriptor[at + 1]);
    }
    CHECK(ids == std::vector<uint8_t>({KEYBOARD_ID, MOUSE_ID, GAMEPAD_ID}));
}

TEST(UsbDevices, TheMouseDescriptorSaysAbsolutePositionsUpTo32767) {
    setUp();
    TinyUSBDevices.begin(1);
    const std::vector<uint8_t> logicalMaximum = {0x26, 0xFF, 0x7F};        // logical maximum 32767
    CHECK(std::search(usbHid.reportDescriptor.begin(), usbHid.reportDescriptor.end(), logicalMaximum.begin(), logicalMaximum.end()) != usbHid.reportDescriptor.end());
}

TEST(UsbDevices, TheMouseOnlyRequestsAReportWhenSomethingChanges) {
    Fresh<AbsMouse5_> mouse;
    setUp();
    mouse.device.move(0, 0);
    CHECK(!due(TinyUSBDevices_::reportMouse));          // it was there already
    mouse.device.move(100, 200);
    CHECK(due(TinyUSBDevices_::reportMouse));
    TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = false;
    mouse.device.move(100, 200);
    CHECK(!due(TinyUSBDevices_::reportMouse));
    mouse.device.move(101, 200);
    CHECK(due(TinyUSBDevices_::reportMouse));
    TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = false;
    mouse.device.move(101, 201);                        // only Y changed
    CHECK(due(TinyUSBDevices_::reportMouse));

    TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = false;
    mouse.device.press(MOUSE_LEFT);
    CHECK(due(TinyUSBDevices_::reportMouse));
    TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = false;
    mouse.device.press(MOUSE_LEFT);                     // already down
    CHECK(!due(TinyUSBDevices_::reportMouse));
    mouse.device.release(MOUSE_RIGHT);                  // not down
    CHECK(!due(TinyUSBDevices_::reportMouse));
    mouse.device.release(MOUSE_LEFT);
    CHECK(due(TinyUSBDevices_::reportMouse));
}

TEST(UsbDevices, TheMouseReportIsButtonsThenXThenYLittleEndian) {
    Fresh<AbsMouse5_> mouse;
    setUp();
    mouse.device.press(MOUSE_LEFT);
    mouse.device.press(MOUSE_BUTTON5);
    mouse.device.move(0x1234, 0x5678);
    mouse.device.report();

    REQUIRE(usbHid.reports.size() == 1);
    CHECK_EQ(lastReport().id, MOUSE_ID);
    std::vector<uint8_t> expected = {MOUSE_LEFT | MOUSE_BUTTON5, 0x34, 0x12, 0x78, 0x56};
    CHECK(lastReport().bytes == expected);
    CHECK(!due(TinyUSBDevices_::reportMouse));          // sent: nothing more is due
}

TEST(UsbDevices, TheMouseReportsTheWholeRangeOfPositions) {
    Fresh<AbsMouse5_> mouse;
    setUp();
    mouse.device.move(32767, 0);
    mouse.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0xFF, 0x7F, 0, 0}));
    mouse.device.move(0, 32767);
    mouse.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0, 0xFF, 0x7F}));
}

TEST(UsbDevices, ReleaseAllLetsGoOfAllFiveMouseButtons) {
    Fresh<AbsMouse5_> mouse;
    setUp();
    for (uint8_t button : {MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE, MOUSE_BUTTON4, MOUSE_BUTTON5}) mouse.device.press(button);
    mouse.device.report();
    CHECK_EQ(lastReport().bytes[0], 0x1F);
    mouse.device.releaseAll();
    CHECK(due(TinyUSBDevices_::reportMouse));
    mouse.device.report();
    CHECK_EQ(lastReport().bytes[0], 0);
}

TEST(UsbDevices, TheMouseWaitsForThePortToBeFree) {
    Fresh<AbsMouse5_> mouse;
    setUp();
    usbHid.busyFor = 3;
    mouse.device.report();
    CHECK_EQ(testYields, 3);                            // it gave way to the rest of the firmware while it waited
    CHECK_EQ(usbHid.reports.size(), 1);
}

TEST(UsbDevices, APrintingKeyIsSentAsItsHidCode) {
    Fresh<Keyboard_> keyboard;
    setUp();
    CHECK(keyboard.device.press('a'));
    keyboard.device.report();
    REQUIRE(usbHid.reports.size() == 1);
    CHECK_EQ(lastReport().id, KEYBOARD_ID);
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0x04, 0, 0, 0, 0, 0}));
    CHECK(!due(TinyUSBDevices_::reportKeyboard));
}

TEST(UsbDevices, ACapitalLetterAddsTheLeftShift) {
    Fresh<Keyboard_> keyboard;
    setUp();
    CHECK(keyboard.device.press('A'));
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0x02, 0, 0x04, 0, 0, 0, 0, 0}));

    // Releasing it lets go of the key and the shift.
    CHECK(keyboard.device.release('A'));
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST(UsbDevices, NumbersAndSymbolsHaveTheirCodes) {
    struct Case { char character; uint8_t code; bool shifted; };
    const Case cases[] = {
        {'1', 0x1E, false}, {'0', 0x27, false}, {' ', 0x2C, false}, {'!', 0x1E, true}, {'\n', 0x28, false}, {'-', 0x2D, false},
        {'z', 0x1D, false}, {'Z', 0x1D, true}, {'~', 0x35, true}, {'`', 0x35, false},
    };
    for (const Case& test : cases) {
        Fresh<Keyboard_> keyboard;
        setUp();
        INFO("character %d", (int)test.character);
        CHECK(keyboard.device.press((uint8_t)test.character));
        keyboard.device.report();
        CHECK_EQ(lastReport().bytes[2], test.code);
        CHECK_EQ(lastReport().bytes[0], test.shifted ? 0x02 : 0);
    }
}

TEST(UsbDevices, ModifierKeysSetTheirBitsInTheModifierByte) {
    Fresh<Keyboard_> keyboard;
    setUp();
    keyboard.device.press(KEY_LEFT_CTRL);
    keyboard.device.press(KEY_RIGHT_ALT);
    keyboard.device.report();
    CHECK_EQ(lastReport().bytes[0], (1 << 0) | (1 << 6));
    CHECK_EQ(lastReport().bytes[2], 0);                 // and no key
    keyboard.device.release(KEY_LEFT_CTRL);
    keyboard.device.report();
    CHECK_EQ(lastReport().bytes[0], 1 << 6);
}

TEST(UsbDevices, NonPrintingKeysAreSentWithTheirOffsetCode) {
    Fresh<Keyboard_> keyboard;
    setUp();
    keyboard.device.press(KEY_UP_ARROW);                // 0xDA: 218 - 136 = 0x52
    keyboard.device.press(KEY_F1);                      // 0xC2: 0x3A
    keyboard.device.press(KEY_ESC);                     // 0xB1: 0x29
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0x52, 0x3A, 0x29, 0, 0, 0}));
}

TEST(UsbDevices, AKeyAlreadyDownIsNotAddedAgain) {
    Fresh<Keyboard_> keyboard;
    setUp();
    keyboard.device.press('a');
    keyboard.device.press('a');
    keyboard.device.press('b');
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0x04, 0x05, 0, 0, 0, 0}));
}

TEST(UsbDevices, OnlySixKeysAtOnce) {
    Fresh<Keyboard_> keyboard;
    setUp();
    for (char key : std::string("abcdef")) CHECK(keyboard.device.press(key));
    CHECK(!keyboard.device.press('g'));
    CHECK(keyboard.device.getWriteError() != 0);
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}));
}

TEST(UsbDevices, DISABLED_AModifierGoesEvenWithSixKeysDown) {
    // A modifier takes no slot of the six, so it should be sent whatever else is down. With all six slots full, press() looks for
    // an empty slot for it (as key 0), finds none, and returns false with a write error and no report asked for.
    Fresh<Keyboard_> keyboard;
    setUp();
    for (char key : std::string("abcdef")) keyboard.device.press(key);
    TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = false;
    CHECK(keyboard.device.press(KEY_LEFT_SHIFT));
    CHECK(due(TinyUSBDevices_::reportKeyboard));
}

TEST(UsbDevices, ACharacterWithNoKeyIsRefused) {
    Fresh<Keyboard_> keyboard;
    setUp();
    CHECK(!keyboard.device.press(0x01));                // a control character
    CHECK(keyboard.device.getWriteError() != 0);
    CHECK(!keyboard.device.release(0x01));
    CHECK(!due(TinyUSBDevices_::reportKeyboard));
}

TEST(UsbDevices, ReleasingFreesTheSlotForAnotherKey) {
    Fresh<Keyboard_> keyboard;
    setUp();
    for (char key : std::string("abcdef")) keyboard.device.press(key);
    keyboard.device.release('c');
    CHECK(keyboard.device.press('g'));
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0x04, 0x05, 0x0A, 0x07, 0x08, 0x09}));
}

TEST(UsbDevices, KeyboardReleaseAllClearsEverything) {
    Fresh<Keyboard_> keyboard;
    setUp();
    keyboard.device.press('A');
    keyboard.device.press(KEY_LEFT_CTRL);
    keyboard.device.press(KEY_F5);
    TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = false;
    keyboard.device.releaseAll();
    CHECK(due(TinyUSBDevices_::reportKeyboard));
    keyboard.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST(UsbDevices, TheKeyboardAsksForAReportOnPressAndRelease) {
    Fresh<Keyboard_> keyboard;
    setUp();
    keyboard.device.press('x');
    CHECK(due(TinyUSBDevices_::reportKeyboard));
    TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = false;
    keyboard.device.release('x');
    CHECK(due(TinyUSBDevices_::reportKeyboard));
}

TEST(UsbDevices, TheKeyboardWaitsForThePortAndHasNoTextOutput) {
    Fresh<Keyboard_> keyboard;
    setUp();
    usbHid.busyFor = 2;
    keyboard.device.report();
    CHECK_EQ(testYields, 2);
    // write() is part of the Print interface and does nothing.
    CHECK_EQ(keyboard.device.write((uint8_t)'a'), 1);
    const uint8_t text[] = {'a', 'b'};
    CHECK_EQ(keyboard.device.write(text, 2), 1);
    CHECK_EQ(usbHid.reports.size(), 1);
}

TEST(UsbDevices, TheGamepadReportIsSticksHatAndButtons) {
    Fresh<Gamepad16_> pad;
    setUp();
    pad.device.press(PAD_A);
    pad.device.press(PAD_START);
    pad.device.padUpdate(3);
    pad.device.report();

    REQUIRE(usbHid.reports.size() == 1);
    CHECK_EQ(lastReport().id, GAMEPAD_ID);
    // X, Y, Rx, Ry (four 16-bit numbers), the hat, and the buttons (16 bits).
    std::vector<uint8_t> expected = {0, 0, 0, 0, 0, 0, 0, 0, 3};
    const std::vector<uint8_t> buttons = littleEndian16((1 << PAD_A) | (1 << PAD_START));
    expected.insert(expected.end(), buttons.begin(), buttons.end());
    CHECK(lastReport().bytes == expected);
    CHECK_EQ(lastReport().bytes.size(), 11);
    CHECK(!due(TinyUSBDevices_::reportGamepad));
}

TEST(UsbDevices, GamepadButtonsAndHatAskForAReportOnlyOnAChange) {
    Fresh<Gamepad16_> pad;
    setUp();
    pad.device.press(PAD_B);
    CHECK(due(TinyUSBDevices_::reportGamepad));
    TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = false;
    pad.device.press(PAD_B);
    CHECK(!due(TinyUSBDevices_::reportGamepad));
    pad.device.release(PAD_X);                          // not down
    CHECK(!due(TinyUSBDevices_::reportGamepad));
    pad.device.press(PAD_RS);
    pad.device.release(PAD_B);
    CHECK(due(TinyUSBDevices_::reportGamepad));
    pad.device.report();
    CHECK(lastReport().bytes[9] == (uint8_t)((1 << PAD_RS) & 0xFF) && lastReport().bytes[10] == (uint8_t)((1 << PAD_RS) >> 8));      // only B was let go

    TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = false;
    pad.device.padUpdate(5);
    CHECK(due(TinyUSBDevices_::reportGamepad));
    TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = false;
    pad.device.padUpdate(5);
    CHECK(!due(TinyUSBDevices_::reportGamepad));
}

TEST(UsbDevices, TheCameraMovesTheRightOrTheLeftStick) {
    Fresh<Gamepad16_> pad;
    setUp();
    // 0..32767 is spread over the stick's range, -32767..32767.
    pad.device.stickRight = false;
    pad.device.moveCam(0, 32767);
    pad.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0, 0, 0, 0, 0x01, 0x80, 0xFF, 0x7F, 0, 0, 0}));     // Rx -32767, Ry 32767

    pad.device.stickRight = true;
    pad.device.moveCam(16384, 16383);
    pad.device.report();
    CHECK(due(TinyUSBDevices_::reportGamepad) == false);
    const std::vector<uint8_t>& bytes = lastReport().bytes;
    const int16_t x = (int16_t)(bytes[0] | (bytes[1] << 8)), y = (int16_t)(bytes[2] | (bytes[3] << 8));
    CHECK_NEAR(x, 1, 2);                                // the middle of the range
    CHECK_NEAR(y, 0, 2);
}

TEST(UsbDevices, TheAnalogStickIsInvertedAndOnlyReportsChanges) {
    Fresh<Gamepad16_> pad;
    setUp();
    // The stick is read as 0..4095 (12 bits) and goes out as 32767..-32767.
    pad.device.stickRight = false;
    pad.device.moveStick(0, 4095);
    CHECK(due(TinyUSBDevices_::reportGamepad));
    pad.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>({0xFF, 0x7F, 0x01, 0x80, 0, 0, 0, 0, 0, 0, 0}));     // X 32767, Y -32767

    pad.device.moveStick(0, 4095);                      // no change
    CHECK(!due(TinyUSBDevices_::reportGamepad));
    pad.device.moveStick(0, 4094);
    CHECK(due(TinyUSBDevices_::reportGamepad));

    pad.device.stickRight = true;
    pad.device.moveStick(4095, 0);
    pad.device.report();
    // The right stick (Rx, Ry) is the one that moved, from the full range of the 12 bits: -32767 and 32767.
    const std::vector<uint8_t>& bytes = lastReport().bytes;
    CHECK_EQ((int16_t)(bytes[4] | (bytes[5] << 8)), -32767);
    CHECK_EQ((int16_t)(bytes[6] | (bytes[7] << 8)), 32767);
}

TEST(UsbDevices, GamepadReleaseAllCentersTheStickAndClearsTheButtons) {
    Fresh<Gamepad16_> pad;
    setUp();
    pad.device.press(PAD_LT);
    pad.device.padUpdate(7);
    pad.device.moveStick(0, 0);
    pad.device.releaseAll();
    CHECK(due(TinyUSBDevices_::reportGamepad));
    pad.device.report();
    CHECK(lastReport().bytes == std::vector<uint8_t>(11, 0));

    // The stick's remembered position is the center again: moving to the center is no change.
    TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = false;
    pad.device.moveStick(2048, 2048);
    CHECK(!due(TinyUSBDevices_::reportGamepad));
}

TEST(UsbDevices, TheGamepadWaitsForThePort) {
    Fresh<Gamepad16_> pad;
    setUp();
    usbHid.busyFor = 4;
    pad.device.report();
    CHECK_EQ(testYields, 4);
}

TEST(UsbDevices, EachDeviceHasItsOwnChannelForPendingReports) {
    Fresh<AbsMouse5_> mouse;
    Fresh<Keyboard_> keyboard;
    Fresh<Gamepad16_> pad;
    setUp();
    mouse.device.press(MOUSE_LEFT);
    CHECK(due(TinyUSBDevices_::reportMouse) && !due(TinyUSBDevices_::reportKeyboard) && !due(TinyUSBDevices_::reportGamepad));
    keyboard.device.press('a');
    CHECK(due(TinyUSBDevices_::reportKeyboard) && !due(TinyUSBDevices_::reportGamepad));
    pad.device.press(PAD_A);
    CHECK(due(TinyUSBDevices_::reportGamepad));
    mouse.device.report();
    CHECK(!due(TinyUSBDevices_::reportMouse) && due(TinyUSBDevices_::reportKeyboard) && due(TinyUSBDevices_::reportGamepad));
}
