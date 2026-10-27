// Tests for LightgunButtons: debouncing, the press and release events, and what is reported to the host as HID
// mouse, keyboard and gamepad input. The pins, the clock and the USB devices are the fakes in ../stub.
#include "framework.h"

#include <new>

#include <Arduino.h>
#include <TinyUSB_Devices.h>
#include <LightgunButtons.h>

namespace {

// The buttons of the test gun, in the order of the descriptor table below.
enum Button {
    TRIGGER = 0,        // pin 10: left mouse button, right when off screen, never reported
    BUTTON_A,           // pin 11: mouse button 2, off screen mouse button 8, analog output key 'a'
    BUTTON_KEY,         // pin 12: keyboard key 'k', off screen key 'o', analog output gamepad button
    BUTTON_PAD_BUTTON,  // pin 13: gamepad button B
    BUTTON_UP,          // pin 14: D-pad
    BUTTON_DOWN,        // pin 15
    BUTTON_LEFT,        // pin 16
    BUTTON_RIGHT,       // pin 17
    BUTTON_UNUSED,      // pin -1: not wired
    BUTTON_FIFO,        // pin 19: debounced with a 4-sample fifo
    BUTTON_COUNT
};

constexpr int PIN_OF[BUTTON_COUNT] = {10, 11, 12, 13, 14, 15, 16, 17, -1, 19};

constexpr uint8_t MOUSE = LightgunButtons::ReportType_Mouse, KEYBOARD = LightgunButtons::ReportType_Keyboard,
                  GAMEPAD = LightgunButtons::ReportType_Gamepad, INTERNAL = LightgunButtons::ReportType_Internal;

constexpr uint8_t MOUSE_BUTTON_8 = 128;     // the other mouse buttons are the fake USB header's

} // namespace

// The mouse device the buttons report to (the firmware defines it in OpenFIREcommon.h).
AbsMouse5_ AbsMouse5;

// pin, type, code, off screen type and code, analog output type and code, debounce ms, fifo mask
LightgunButtons::Desc_t LightgunButtons::ButtonDesc[] = {
    {10, MOUSE, MOUSE_LEFT, MOUSE, MOUSE_RIGHT, INTERNAL, 0, 20, 0},
    {11, MOUSE, MOUSE_MIDDLE, MOUSE, MOUSE_BUTTON_8, KEYBOARD, 'a', 20, 0},
    {12, KEYBOARD, 'k', KEYBOARD, 'o', GAMEPAD, PAD_X, 20, 0},
    {13, GAMEPAD, PAD_B, GAMEPAD, PAD_Y, GAMEPAD, PAD_Z, 20, 0},
    {14, GAMEPAD, PAD_UP, GAMEPAD, PAD_UP, GAMEPAD, PAD_UP, 20, 0},
    {15, GAMEPAD, PAD_DOWN, GAMEPAD, PAD_DOWN, GAMEPAD, PAD_DOWN, 20, 0},
    {16, GAMEPAD, PAD_LEFT, GAMEPAD, PAD_LEFT, GAMEPAD, PAD_LEFT, 20, 0},
    {17, GAMEPAD, PAD_RIGHT, GAMEPAD, PAD_RIGHT, GAMEPAD, PAD_RIGHT, 20, 0},
    {-1, MOUSE, MOUSE_LEFT, MOUSE, MOUSE_LEFT, MOUSE, MOUSE_LEFT, 20, 0},
    {19, KEYBOARD, 'f', KEYBOARD, 'f', KEYBOARD, 'f', 0, 0x0F},
};

namespace {

constexpr uint32_t maskOf(Button button) { return 1u << button; }

// The buttons object is built in zeroed memory, as it is in the firmware, where it has static storage: its constructor
// leaves offScreen, analogOutput and the off screen mask uninitialized, relying on that.
LightgunButtons& buttonsInZeroedMemory(LightgunButtons::Data_t data) {
    alignas(LightgunButtons) static unsigned char memory[sizeof(LightgunButtons)];
    memset(memory, 0, sizeof(memory));
    return *new (memory) LightgunButtons(data, BUTTON_COUNT);
}

// A gun with its buttons, its state arrays and a clock that starts at a fixed time.
struct Gun {
    LightgunButtonsStatic<BUTTON_COUNT> storage;
    LightgunButtons& buttons;

    Gun() : buttons(buttonsInZeroedMemory(storage)) {
        resetUsbDevices();
        buttons.Begin();
        buttons.ReportEnable();
    }

    void hold(Button button) { testPinLevel[PIN_OF[button]] = 0; }
    void let(Button button) { testPinLevel[PIN_OF[button]] = 1; }

    // Lets the given time pass and polls.
    uint32_t poll(unsigned long milliseconds = 1) {
        testMillis += milliseconds;
        return buttons.Poll();
    }
};

} // namespace

TEST(Buttons, BeginPullsTheWiredPinsUpAndIgnoresTheOthers) {
    Gun gun;
    for (int button = 0; button < BUTTON_COUNT; button++) {
        if (PIN_OF[button] >= 0) CHECK_EQ(testPinMode[PIN_OF[button]], INPUT_PULLUP);
    }
    CHECK_EQ(testPinMode[0], INPUT);        // nothing happened to pin 0 or any pin that isn't a button's

    gun.buttons.Unset();
    for (int button = 0; button < BUTTON_COUNT; button++) {
        if (PIN_OF[button] >= 0) CHECK_EQ(testPinMode[PIN_OF[button]], INPUT);
    }
}

TEST(Buttons, NothingHappensWhileNothingIsPressed) {
    Gun gun;
    for (int i = 0; i < 10; i++) CHECK_EQ(gun.poll(), 0);
    CHECK_EQ(gun.buttons.debounced, 0);
    CHECK_EQ(gun.buttons.pressed, 0);
    CHECK_EQ(gun.buttons.released, 0);
    CHECK_EQ(AbsMouse5.buttons, 0);
}

TEST(Buttons, PressAndReleaseAreEventsOfOnePollEach) {
    Gun gun;
    gun.hold(BUTTON_A);
    CHECK_EQ(gun.poll(), maskOf(BUTTON_A));
    CHECK_EQ(gun.buttons.pressed, maskOf(BUTTON_A));
    CHECK_EQ(gun.buttons.debounced, maskOf(BUTTON_A));

    // The event is gone at the next poll, the button stays pressed.
    gun.poll();
    CHECK_EQ(gun.buttons.pressed, 0);
    CHECK_EQ(gun.buttons.debounced, maskOf(BUTTON_A));

    gun.let(BUTTON_A);
    gun.poll(50);       // after the debounce time of 20 ms
    CHECK_EQ(gun.buttons.released, maskOf(BUTTON_A));
    CHECK_EQ(gun.buttons.debounced, 0);
    gun.poll();
    CHECK_EQ(gun.buttons.released, 0);
}

TEST(Buttons, ABouncingButtonIsIgnoredForTheDebounceTime) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll();
    CHECK(gun.buttons.debouncing & maskOf(BUTTON_A));

    // Released after 5 ms: still inside the 20 ms debounce time, so nothing is seen.
    gun.let(BUTTON_A);
    gun.poll(5);
    CHECK_EQ(gun.buttons.released, 0);
    CHECK_EQ(gun.buttons.debounced, maskOf(BUTTON_A));

    // The pin is looked at again after the debounce time, and the release is seen.
    gun.poll(30);
    CHECK_EQ(gun.buttons.released, maskOf(BUTTON_A));
    gun.poll(30);
    CHECK_EQ(gun.buttons.debouncing, 0);        // and the debouncing of the release is over, too
}

TEST(Buttons, TheDebounceEndsExactlyAfterItsTime) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll();
    CHECK(gun.buttons.debouncing & maskOf(BUTTON_A));
    gun.poll(19);
    CHECK(gun.buttons.debouncing & maskOf(BUTTON_A));       // 19 of 20 ms
    gun.poll(1);
    CHECK_EQ(gun.buttons.debouncing, 0);                    // 20 of 20 ms: over

    // A release exactly then is seen at once.
    gun.let(BUTTON_A);
    gun.poll(1);
    CHECK_EQ(gun.buttons.released, maskOf(BUTTON_A));
}

TEST(Buttons, TheDebounceCountdownIsKeptAcrossPolls) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll();
    gun.let(BUTTON_A);
    for (int i = 0; i < 4; i++) {
        gun.poll(4);        // 16 ms after the press
        CHECK_EQ(gun.buttons.released, 0);
    }
    gun.poll(10);           // 26 ms: debounced by now
    CHECK_EQ(gun.buttons.released, maskOf(BUTTON_A));
}

TEST(Buttons, MinTicksLimitsHowOftenThePinsAreRead) {
    Gun gun;
    gun.poll();
    gun.hold(BUTTON_A);
    testMillis += 3;
    CHECK_EQ(gun.buttons.Poll(10), 0);          // 3 ms since the last poll: too soon
    CHECK_EQ(gun.buttons.debounced, 0);
    testMillis += 7;
    CHECK_EQ(gun.buttons.Poll(10), maskOf(BUTTON_A));
}

TEST(Buttons, AButtonThatIsNotWiredIsNeverRead) {
    Gun gun;
    for (int pin = 0; pin < 64; pin++) testPinLevel[pin] = 0;       // everything pulled low, wired or not
    gun.poll();
    CHECK(!(gun.buttons.debounced & maskOf(BUTTON_UNUSED)));
    CHECK(gun.buttons.debounced & maskOf(TRIGGER));
}

TEST(Buttons, FifoDebouncingNeedsAStableRun) {
    Gun gun;
    // The fifo mask is 0x0F: four equal samples in a row decide. A bouncing pin gives none.
    gun.hold(BUTTON_FIFO);
    gun.poll(); gun.poll(); gun.poll();
    CHECK_EQ(gun.buttons.debounced, 0);
    gun.let(BUTTON_FIFO);
    gun.poll();
    gun.hold(BUTTON_FIFO);
    gun.poll(); gun.poll(); gun.poll();
    CHECK_EQ(gun.buttons.debounced, 0);

    // The fourth low sample in a row makes it a press.
    gun.poll();
    CHECK_EQ(gun.buttons.debounced, maskOf(BUTTON_FIFO));
    CHECK_EQ(gun.buttons.pressed, maskOf(BUTTON_FIFO));
    CHECK(Keyboard.keys['f']);

    // And four high ones in a row a release.
    gun.let(BUTTON_FIFO);
    gun.poll(); gun.poll(); gun.poll();
    CHECK_EQ(gun.buttons.released, 0);
    gun.poll();
    CHECK_EQ(gun.buttons.released, maskOf(BUTTON_FIFO));
    CHECK(!Keyboard.keys['f']);
}

TEST(Buttons, TheTriggerIsNeverReportedToTheHost) {
    Gun gun;
    gun.hold(TRIGGER);
    gun.poll();
    CHECK_EQ(gun.buttons.pressed, maskOf(TRIGGER));
    CHECK_EQ(AbsMouse5.buttons, 0);
    gun.let(TRIGGER);
    gun.poll(50);
    CHECK_EQ(gun.buttons.released, maskOf(TRIGGER));
    CHECK_EQ(AbsMouse5.buttons, 0);
}

TEST(Buttons, PressesAreReportedAsTheirMouseKeyboardOrGamepadCode) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.hold(BUTTON_KEY);
    gun.hold(BUTTON_PAD_BUTTON);
    gun.poll();
    CHECK_EQ(AbsMouse5.buttons, MOUSE_MIDDLE);
    CHECK(Keyboard.keys['k']);
    CHECK_EQ(Gamepad16.buttons, 1u << PAD_B);

    gun.let(BUTTON_A);
    gun.let(BUTTON_KEY);
    gun.let(BUTTON_PAD_BUTTON);
    gun.poll(50);
    CHECK_EQ(AbsMouse5.buttons, 0);
    CHECK(!Keyboard.keys['k']);
    CHECK_EQ(Gamepad16.buttons, 0);
}

TEST(Buttons, OffScreenPressesUseTheirOwnCodes) {
    Gun gun;
    gun.buttons.offScreen = true;
    gun.hold(BUTTON_A);
    gun.hold(BUTTON_KEY);
    gun.hold(BUTTON_PAD_BUTTON);
    gun.poll();
    CHECK_EQ(AbsMouse5.buttons, MOUSE_BUTTON_8);
    CHECK(Keyboard.keys['o']);
    CHECK(!Keyboard.keys['k']);
    CHECK_EQ(Gamepad16.buttons, 1u << PAD_Y);

    // Back on screen before the release: the buttons release what they pressed.
    gun.buttons.offScreen = false;
    gun.let(BUTTON_A);
    gun.let(BUTTON_KEY);
    gun.let(BUTTON_PAD_BUTTON);
    gun.poll(50);
    CHECK_EQ(AbsMouse5.buttons, 0);
    CHECK(!Keyboard.keys['o']);
    CHECK_EQ(Gamepad16.buttons, 0);
}

TEST(Buttons, AnalogOutputUsesTheThirdCodes) {
    Gun gun;
    gun.buttons.analogOutput = true;
    gun.buttons.offScreen = true;       // analog output wins over off screen
    gun.hold(BUTTON_A);
    gun.hold(BUTTON_KEY);
    gun.hold(BUTTON_PAD_BUTTON);
    gun.poll();
    CHECK_EQ(AbsMouse5.buttons, 0);
    CHECK(Keyboard.keys['a']);
    CHECK_EQ(Gamepad16.buttons, (1u << PAD_X) | (1u << PAD_Z));

    gun.let(BUTTON_A);
    gun.let(BUTTON_KEY);
    gun.let(BUTTON_PAD_BUTTON);
    gun.poll(50);
    CHECK(!Keyboard.keys['a']);
    CHECK_EQ(Gamepad16.buttons, 0);
}

TEST(Buttons, AnalogOutputCanReportToTheMouseToo) {
    // A button whose third code is a mouse button.
    const LightgunButtons::Desc_t saved = LightgunButtons::ButtonDesc[BUTTON_A];
    LightgunButtons::ButtonDesc[BUTTON_A].reportType3 = MOUSE;
    LightgunButtons::ButtonDesc[BUTTON_A].reportCode3 = MOUSE_RIGHT;
    Gun gun;
    gun.buttons.analogOutput = true;
    gun.hold(BUTTON_A);
    gun.poll();
    CHECK_EQ(AbsMouse5.buttons, MOUSE_RIGHT);
    gun.let(BUTTON_A);
    gun.poll(50);
    CHECK_EQ(AbsMouse5.buttons, 0);
    LightgunButtons::ButtonDesc[BUTTON_A] = saved;
}

TEST(Buttons, WithReportingOffNothingIsSentButAPressedButtonStillReleases) {
    Gun gun;
    gun.buttons.ReportDisable();
    gun.hold(BUTTON_A);
    gun.poll();
    CHECK_EQ(AbsMouse5.buttons, 0);                         // not reported
    CHECK_EQ(gun.buttons.pressed, maskOf(BUTTON_A));           // but seen by the firmware

    // Reporting is on for a press, then off again before its release: the release goes out all the same,
    // or the host would be left with a stuck button.
    gun.let(BUTTON_A);
    gun.poll(50);
    gun.buttons.ReportEnable();
    gun.hold(BUTTON_A);
    gun.poll(50);
    CHECK_EQ(AbsMouse5.buttons, MOUSE_MIDDLE);
    gun.buttons.ReportDisable();
    gun.let(BUTTON_A);
    gun.poll(50);
    CHECK_EQ(AbsMouse5.buttons, 0);
}

TEST(Buttons, TheDpadDirectionsMakeTheHatPosition) {
    Gun gun;
    struct Case { Button first, second; uint8_t hat; };
    const Case cases[] = {
        {BUTTON_UP, BUTTON_UNUSED, 1}, {BUTTON_RIGHT, BUTTON_UNUSED, 3}, {BUTTON_DOWN, BUTTON_UNUSED, 5}, {BUTTON_LEFT, BUTTON_UNUSED, 7},
        {BUTTON_UP, BUTTON_RIGHT, 2}, {BUTTON_DOWN, BUTTON_RIGHT, 4}, {BUTTON_DOWN, BUTTON_LEFT, 6}, {BUTTON_UP, BUTTON_LEFT, 8},
    };
    for (const Case& test : cases) {
        gun.hold(test.first);
        if (test.second != BUTTON_UNUSED) gun.hold(test.second);
        gun.poll(50);
        INFO("buttons %d and %d", (int)test.first, (int)test.second);
        CHECK_EQ(Gamepad16.hat, test.hat);
        gun.let(test.first);
        if (test.second != BUTTON_UNUSED) gun.let(test.second);
        gun.poll(50);
        CHECK_EQ(Gamepad16.hat, 0);
    }
}

TEST(Buttons, TheDpadWorksOffScreenAndInAnalogMode) {
    for (int mode = 0; mode < 2; mode++) {
        Gun gun;
        gun.buttons.offScreen = (mode == 0);
        gun.buttons.analogOutput = (mode == 1);
        gun.hold(BUTTON_LEFT);
        gun.poll(50);
        CHECK_EQ(Gamepad16.hat, 7);
        gun.let(BUTTON_LEFT);
        gun.poll(50);
        CHECK_EQ(Gamepad16.hat, 0);
    }
}

TEST(Buttons, PadMaskConversionCoversEveryCombination) {
    LightgunButtonsStatic<BUTTON_COUNT> storage;
    LightgunButtons buttons(storage, BUTTON_COUNT);
    // Up 1, down 2, left 4, right 8. Opposite directions at once give the first of the two.
    const struct { uint32_t mask, hat; } expected[] = {
        {0, 0}, {1, 1}, {2, 5}, {4, 7}, {8, 3}, {5, 8}, {9, 2}, {6, 6}, {10, 4}, {3, 1}, {12, 7}, {7, 0}, {15, 0}, {16, 0},
    };
    for (const auto& test : expected) {
        INFO("mask %u", (unsigned)test.mask);
        CHECK_EQ(buttons.PadMaskConvert(test.mask), test.hat);
    }
}

TEST(Buttons, PressedReleasedIsTheWholeComboOnceEverythingIsLetGo) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll(50);
    gun.hold(BUTTON_KEY);
    gun.poll(50);

    gun.let(BUTTON_A);
    gun.poll(50);
    CHECK_EQ(gun.buttons.pressedReleased, 0);       // another button is still held

    gun.let(BUTTON_KEY);
    gun.poll(50);
    CHECK_EQ(gun.buttons.pressedReleased, maskOf(BUTTON_A) | maskOf(BUTTON_KEY));
    gun.poll(50);
    CHECK_EQ(gun.buttons.pressedReleased, 0);       // an event of one poll
}

TEST(Buttons, ModifierPressedMatchesANewPressWithHeldModifiers) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll(50);
    gun.hold(BUTTON_KEY);
    gun.poll(50);
    CHECK(gun.buttons.ModifierPressed(maskOf(BUTTON_KEY), maskOf(BUTTON_A)));
    CHECK(!gun.buttons.ModifierPressed(maskOf(BUTTON_KEY), 0));                    // the modifier isn't what is held
    CHECK(!gun.buttons.ModifierPressed(maskOf(BUTTON_A), maskOf(BUTTON_KEY)));        // not the new press
    gun.poll(50);
    CHECK(!gun.buttons.ModifierPressed(maskOf(BUTTON_KEY), maskOf(BUTTON_A)));        // the press was one poll ago
}

TEST(Buttons, MaskToIndexFindsASingleButton) {
    for (int index = 0; index < 32; index++) CHECK_EQ(LightgunButtons::MaskToIndex(1u << index), index);
    CHECK_EQ(LightgunButtons::MaskToIndex(0), -1);
    CHECK_EQ(LightgunButtons::MaskToIndex(3), -1);
    CHECK_EQ(LightgunButtons::MaskToIndex(0x80000001u), -1);
}

TEST(Buttons, RepeatPulsesTheHeldButtonsAtTheInterval) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll(50);

    // The interval is 33 ms: held buttons come back as a pulse once per interval.
    testMillis += 40;
    CHECK_EQ(gun.buttons.Repeat(), maskOf(BUTTON_A));
    testMillis += 10;
    CHECK_EQ(gun.buttons.Repeat(), 0);
    testMillis += 30;
    CHECK_EQ(gun.buttons.Repeat(), maskOf(BUTTON_A));

    gun.let(BUTTON_A);
    gun.poll(50);
    testMillis += 40;
    CHECK_EQ(gun.buttons.Repeat(), 0);

    // Exactly one interval later is due.
    testMillis += 33;
    CHECK_EQ(gun.buttons.Repeat(), 0);      // (the last pulse was 30 ms ago)
    testMillis += 3;
    CHECK_EQ(gun.buttons.Repeat(), 0);      // the button is no longer held
    gun.hold(BUTTON_A);
    gun.poll(50);
    gun.buttons.Repeat();
    testMillis += 33;
    CHECK_EQ(gun.buttons.Repeat(), maskOf(BUTTON_A));

    gun.buttons.interval = 5;
    gun.hold(BUTTON_A);
    gun.poll(50);
    testMillis += 6;
    CHECK_EQ(gun.buttons.Repeat(), maskOf(BUTTON_A));
}

TEST(Buttons, SendReportsSendsOneDeviceOrAllOfThem) {
    Gun gun;
    TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = true;
    TinyUSBDevices.newReport[TinyUSBDevices_::reportKeyboard] = true;
    TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true;

    // Without forcing, only the first device that has a report due sends it.
    gun.buttons.SendReports();
    CHECK_EQ(AbsMouse5.reports, 1);
    CHECK_EQ(Keyboard.reports, 0);
    gun.buttons.SendReports();
    CHECK_EQ(Keyboard.reports, 1);
    gun.buttons.SendReports();
    CHECK_EQ(Gamepad16.reports, 1);
    gun.buttons.SendReports();      // nothing due now
    CHECK_EQ(AbsMouse5.reports + Keyboard.reports + Gamepad16.reports, 3);

    // Forced, every device with something due sends it.
    TinyUSBDevices.newReport[TinyUSBDevices_::reportMouse] = true;
    TinyUSBDevices.newReport[TinyUSBDevices_::reportGamepad] = true;
    gun.buttons.SendReports(true);
    CHECK_EQ(AbsMouse5.reports, 2);
    CHECK_EQ(Keyboard.reports, 1);
    CHECK_EQ(Gamepad16.reports, 2);
}

TEST(Buttons, ReleaseAllClearsEverythingTheHostSaw) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.hold(BUTTON_KEY);
    gun.hold(BUTTON_PAD_BUTTON);
    gun.hold(BUTTON_UP);
    gun.poll(50);
    CHECK(AbsMouse5.buttons != 0 && Gamepad16.hat != 0);

    CHECK_EQ(gun.buttons.releaseAllCount, 0);
    gun.buttons.ReleaseAll();
    CHECK_EQ(gun.buttons.releaseAllCount, 1);
    CHECK_EQ(AbsMouse5.buttons, 0);
    CHECK(Keyboard.keys.none());
    CHECK_EQ(Gamepad16.buttons, 0);
    CHECK_EQ(Gamepad16.hat, 0);
    CHECK(AbsMouse5.reports >= 1 && Keyboard.reports >= 1 && Gamepad16.reports >= 1);

    // A D-pad direction that was held doesn't come back with the next D-pad press: the direction mask was cleared too.
    // (Up is still held; pressing right gives right alone, not up-right.)
    gun.hold(BUTTON_RIGHT);
    gun.poll(50);
    CHECK_EQ(Gamepad16.hat, 3);
}

TEST(Buttons, UnsetForgetsTheStateOfAllButtons) {
    Gun gun;
    gun.hold(BUTTON_A);
    gun.poll(50);
    gun.buttons.Unset();
    CHECK_EQ(gun.buttons.debounced, 0);
    CHECK_EQ(gun.buttons.pressed, 0);
    CHECK_EQ(gun.buttons.debouncing, 0);
    CHECK_EQ(gun.buttons.pressedReleased, 0);
}
