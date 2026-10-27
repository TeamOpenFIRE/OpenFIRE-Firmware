# Unit tests

Tests of single classes of the libraries, run on the PC. They need only `g++` (C++17).

    ./run.sh                    # build and run everything
    ./run.sh DiamondAdvanced    # only the tests whose "Suite.name" contains the text
    ./run.sh --list             # list the tests
    ./run.sh --seed 12345       # repeat the order of an earlier run
    ./run.sh --ordered          # file order instead of random

Coverage (which lines and branches of the library the tests run) is measured with gcov:

    ./coverage.sh                  # summary per file, with the lines that were not run
    ./coverage.sh --html <dir>     # also an HTML report with every line marked
    ./coverage.sh --min 90         # fail (exit status 3) if less than 90% of the lines are run

It needs `gcovr` (`apt install gcovr`). The missing lines of each file are a list of what to test next. What is left in the advanced Square are
checks that the camera's real limits can't reach, and the labelling of views that are both far to the side and
far above.

The tests run in a random order, so that a test that only passes after another one has run is noticed. A run
prints its seed at the start and, if a test failed, again at the end: `--seed` with that number repeats the order.

The exit status is 1 if a test fails. A failure prints the file and line of the check, and the `INFO(...)` note
the test had set (for example the roll angle that was being tried).

## What is tested

| File | Code | Covers |
|---|---|---|
| `test_oneeuro_filter.cpp` | `OpenFIRE_OneEuro_Filter` | smoothing, lag, shared cutoff, edge handling, restarts after pauses, clock wraparound |
| `test_square_advanced.cpp` | `OpenFIRE_Square` (advanced) | LED labelling, roll tracking, hidden LEDs, blackouts, oblique views, input validation |
| `test_diamond_advanced.cpp` | `OpenFIRE_Diamond` (advanced) | the same for the diamond layout |
| `test_perspective_advanced.cpp` | `OpenFIRE_Perspective` (advanced) | the warp from LEDs to screen, oblique views, degenerate input, learning the screen |
| `test_classic_trackers.cpp` | `OpenFIRE_Square`, `OpenFIRE_Diamond` (classic) | what the RP2040's trackers keep to: the roll range, hidden and off-image LEDs |
| `test_classic_perspective.cpp` | `OpenFIRE_Perspective` (classic) | the warp the RP2040 runs |
| `test_board_dfrobot_camera.cpp` | `DFRobotIRPositionEx` | the camera driver against a fake I2C bus: start-up writes, frame decoding, seen flags, the atomic reads and their retries |
| `test_board_lightgun_buttons.cpp` | `LightgunButtons` | debouncing, press and release events, what is reported to the host (mouse, keyboard, gamepad, D-pad, off screen, analog), release all |
| `test_firmware_prefs.cpp` | `OpenFIREprefs` | profiles, toggles, pins, settings, USB id and button map in flash: file formats, round trips, damaged files, board presets |
| `test_firmware_lights.cpp` | `OpenFIRElights` | the board's LEDs, the four-pin LED and the external NeoPixel strand |
| `test_firmware_feedback.cpp` | `OpenFIREFeedback` | the solenoid (single shot, autofire, burst), the rumble motor, temperature protection |
| `test_firmware_display.cpp` | `OpenFIREdisplay` | starting the OLED, every screen, the pause menus, the HUD |
| `test_firmware_common.cpp` | `OpenFIREcommon` | pins, camera and display setup, gun modes, profile and run mode selection, saving, button bindings |
| `test_firmware_aiming.cpp` | `OpenFIREcommon` (with the trackers, warp and buttons) | a simulated player calibrates and aims: accuracy, roll, off-axis, hidden LEDs, run modes, aspect ratio |
| `test_usb_devices.cpp` | `TinyUSB_Devices` | the USB mouse, keyboard and gamepad: report layouts and ids, key codes, change detection, waiting for the port |
| `test_calibration.cpp` | `OpenFIREmain/OpenFIREcalibration.h`, `FW_Const` | the calibration mapping to the screen, run mode numbers |

The classic classes have the same names as the advanced ones, so their tests (files with `_classic_` in the name) are
built on their own, with `NO_ADVANCED_TRACKING`, as the RP2040 builds them. `run.sh` and `coverage.sh` do both builds.
The classic code is upstream's and isn't ours to change, so its compiler warnings are not errors; the tests' and the
advanced classes' are.

The drivers of the camera and the buttons (files with `_board_` in the name) talk to the board, so they are built a third
time, as for the RP2040 (`ARDUINO_ARCH_RP2040`), against fakes of the board in `../stub`: `Wire.h` (an I2C bus that
records what is sent and answers with queued frames), the pins and clock in `Arduino.h` (`testPinLevel`, `testMillis`), and
`TinyUSB_Devices.h` (the HID devices, keeping what the host would see). `LightgunButtons` leaves `offScreen`,
`analogOutput` and its off screen mask uninitialized, relying on the static storage it has in the firmware, so the tests
build it in zeroed memory.

The USB devices (`test_usb_*`) are the real `TinyUSB_Devices.cpp`, built for Adafruit's TinyUSB (`USE_TINYUSB`) against a fake
of its HID class in `../stub/Adafruit_TinyUSB.h`, which records the report descriptor and every report sent, and can be made
busy so that the code has to wait for it. The keyboard and the gamepad leave part of their state uninitialized in their
constructors, relying on static storage, so the tests build them in zeroed memory.

The firmware's own files (`test_firmware_*`) are built with AddressSanitizer and UndefinedBehaviorSanitizer on (not in
`coverage.sh`), so that a write past a buffer or a read of an uninitialized value is an error of the test that does it, and they are built twice, as for a Raspberry Pi Pico (an RP2040, with the classic
trackers) and a Pico 2 (an RP2350, with the advanced ones), and every test runs in both. More fakes stand in for the board:
the file system (`FS.h`, `LittleFS.h`: files in memory, with a switch for a full or damaged volume), the serial port
(`Serial`, collecting what is sent and reading what a test puts in), the PWM and GPIO of the Pico SDK, NeoPixel strands
and the SSD1306 display (which record what was drawn and printed, and in which colours).

### The simulated player

`world.h` plays out the world around the firmware's loops, which would otherwise wait for ever for a button or a camera
frame. A test gives a script, a function from time to what the player does: where the gun aims on the screen, how it is rolled,
which buttons are held, which LEDs are covered. The world turns that into the pins' levels and into the camera's
frames (a pinhole camera looking along the gun at LEDs on the screen plane, mirrored like the DFRobot's), and sets the flag the
board's timer would set for each camera read. It is driven by a hook in `millis()` and `delay()` (`testTickHook`): every
look at the clock moves the world on by a millisecond. With it, the real `ExecCalMode()` can be run from the first trigger
pull to the end, and `GetPosition()` checked against where the gun was really aimed.

Not tested yet: the serial protocol (`OpenFIREserial.cpp`) and the sketch (`OpenFIREmain.ino`: the loop, the pause menu, the hotkeys).

## Writing tests

- `framework.h` is a small test framework (`TEST`, `CHECK`, `CHECK_EQ`, `CHECK_NEAR`, `REQUIRE`, `INFO`);
  its header comment lists them. A test file is added to the build by being named `test_*.cpp`, and its tests register
  themselves, so nothing else needs changing. A new library class needs its `.cpp` added to `run.sh`.
- `support.h` builds scenes: LEDs of a rectangle or diamond at a place and roll, and what the camera would
  report for them (`camera()`), with chosen LEDs hidden or the blobs in another order. It also has checks that
  work for any tracker with `X(slot)`/`Y(slot)`.
- The camera model is flat (no lens, no perspective): the scenes are in camera pixels.
- Time comes from `micros()`, which the stub in `../stub/Arduino.h` reads from `testMicros`. Every test starts at the same
  time, and a test that needs time to pass sets `testMicros` itself.
- Test what the class is supposed to do (its documented behaviour), not what it happens to output: the
  exact outputs of whole runs are in `../tracking`.

## Tests that describe what should be, but isn't

A test whose name starts with `DISABLED_` is listed as skipped and not run: it says how the code ought to behave, and
it doesn't yet. Enabling one shows the problem. At the moment:

- `UsbDevices.DISABLED_AModifierGoesEvenWithSixKeysDown`: with six keys down, pressing Shift, Ctrl or another modifier looks for
  a free key slot for it, finds none, returns false with a write error, and asks for no report, though a modifier needs no slot.
