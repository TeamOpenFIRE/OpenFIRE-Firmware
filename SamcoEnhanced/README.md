# IR-GUN4ALL - The Enclosed Instruction Book!

## Table of Contents:
 - [Setup Guide](#setup-guide)
   - [IR Emitter Setup](#ir-emitter-setup)
   - [Arduino Setup & Libraries](#arduino-setup--libraries)
   - [Sketch Configuration](#sketch-configuration)
   - [Define Buttons & Timers](#define-buttons--timers)
 - [Operations Manual](#operations-manual)
   - [Run Modes](#run-modes)
   - [Default Buttons](#default-buttons)
   - [Default Buttons in Pause Mode](#default-buttons-in-pause-mode)
   - [How to Calibrate](#how-to-calibrate)
   - [Advanced Calibration](#advanced-calibration)
   - [IR Camera Sensitivity](#ir-camera-sensitivity)
   - [Profiles](#profiles)
   - [Software Toggles](#software-toggles)
   - [Saving settings to non-volatile memory](#saving-settings-to-non-volatile-memory)
   - [Processing Mode](#processing-mode)
 - [Technical Details & Assorted Errata](#technical-details--assorted-errata)
   - [Serial Handoff (Mame Hooker) Mode](#serial-handoff-mame-hooker-mode)
   - [Change USB ID for Multiple Guns](#change-usb-id-for-multiple-guns)
   - [Dual Core Mode](#dual-core-mode)
   - [Button Combo Masks](#button-combo-masks)
   - [Other Constants](#other-constants)
   - [Profile Array](#profile-array)

## Setup Guide

### IR Emitter setup
The IR emitters must be arranged with 2 emitters on opposite edges of your screen/monitor forming a rectangle or square. For example, if you're playing on a small PC monitor, you can use 2 Wii sensor bars; one on top of your screen and one below. However, if you're playing on a TV, you should consider building a set of high power black IR LEDs and arranging them like (larger) sensor bars at the top and bottom of the display.

### Arduino Setup & Libraries
If you're using Arduino IDE for the first time, the setup is relatively simple:
 1. [Install the Arduino IDE for your system](https://www.arduino.cc/en/software) (or for Linux users, from your system's package manager)
    * *Windows users, install the USB drivers when prompted.*
 3. Once installed, open Arduino, and from the top bar, click on __*File -> Preferences.*__
 4. In the Preferences window, the *Additional Boards Manager URLs* path should be empty. Copy and paste this string:
    `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json,https://github.com/SeongGino/arduino-pico/releases/download/3.6.1-fix/package_rp2040_fix_index.json`
    and paste it in there. Click OK to confirm.
 5. Back in the main window, go to __*Tools -> Board: {some board name} -> Boards Manager*__ (IDE 1.8.x) || select __*Board Manager*__ from the sidebar (IDE 2.x) and install the latest version of `Adafruit SAMD Boards` (for M0/M4) and `Raspberry Pi Pico/RP2040 (Fix)`
 7. **For Adafruit users:** go to __*Tools -> Manage Libraries...*__ (IDE 1.8.x) || select __*Library Manager*__ (IDE 2.x), and install `Adafruit DotStar`, `Adafruit NeoPixel`, & `Adafruit SPI Flash`.
 8. Under __*Tools*__, make sure to select your board of choice, and set the compiler optimize level to `Faster (-O3)`/`Optimize Even More (-O3)` and set the USB stack to `(Adafruit) TinyUSB`.
    * *Linux users should have their user in the `uucp` group. If not, add oneself to it (`sudo usermod -a -G uucp username`), then relogin to see the board's serial port.*
   
Extract the `SamcoEnhanced` and `libraries` folders from the source repository/releases into your Arduino sketches folder. Defaults are:
* Windows: `Documents\Arduino`
* Linux: `~/Arduino`

Now just go to File -> Open, and load `SamcoEnhanced.ino`.

### Sketch Configuration
The sketch is configured for a [SAMCO 2.0](https://www.ebay.com/itm/184699412596) (GunCon 2) build... *ish.* Because of the additions added (that may or may not conflict with a SAMCO 2.0 board's pin mapping), you'll probably have to adjust what pins are arranged to what buttons manually, and which pins are reserved for the Rumble and Solenoid signal wires (if any). You can still use a loose board for a custom non-Guncon build, just change the pinout as you see fit - see section below on [Defining Buttons & Timers](#define-buttons--timers).

A0 is reserved for an optional temperature sensor if you plan to use a solenoid - it is recommended if you use a weaker/smaller solenoid or intend to run it in autofire mode for extended periods of time, as the sketch will temper activation at higher temperature thresholds (that are configurable!), but will function without.

### Define Buttons & Timers
Refer to Line 79 for the pins of the tactile extras (rumble, solenoid, hardware switches if need be) and the adjustable settings (solenoid activation, rumble event length, etc.), and Line 127-onwards for the button constants.

If your gun is going to be player 1/2/3/4, change `#define PLAYER_NUMBER` to 1, 2, 3, or 4 depending on what keys you want the Start/Select buttons to correlate to. See [Change USB ID for Multiple Guns](#change-usb-id-for-multiple-guns) for more info on multiplayer!

Remember that the sketch uses the Arduino GPIO pin numbers; on the Adafruit Itsybitsy RP2040, these are the silkscreen labels on the **underside** of the microcontroller (marked GP00-29). Also note that this does not apply to the analog pins (A0-A3), which does work and map as expected if used in lieu of GPIO numbers. All the other Adafruit boards don't have this discrepancy.
![Itsybitsy RP2040 Back](https://cdn-learn.adafruit.com/assets/assets/000/101/909/original/adafruit_products_ItsyRP_pinouts_back.jpg)
The default button:pins layout used is as follows (aside from the power pins and the camera, you may deviate from this as much as desired so long as that's reflected in the sketch!):
![Schematic](https://raw.githubusercontent.com/SeongGino/ir-light-gun-plus/plus/SamcoPlus%20Schematic.png)

Once the sketch is configured to your liking, plug the board into a USB port, click Upload (the arrow button next to the checkmark) and your board will reconnect a few times until it's recognized as a combined mouse and keyboard device!

## Operations Manual
The light gun operates as an absolute positioning mouse (like a stylus!) until the button/combination is pressed to enter pause mode. The Arduino serial monitor (or any serial terminal) can be used to see information while the gun is paused and during the calibration procedure.

Note that the buttons in pause mode (and to enter pause mode) activate when the last button of the combination releases. This is used to detect and differentiate button combinations vs a single button press.

* Note: At its peak, the mouse position updates at 209Hz, or roughly every ~4.8ms, so it is extremely responsive.

### Run modes
The gun has the following modes of operation:
1. Normal - The mouse position updates from each frame from the IR positioning camera (no averaging)
2. Averaging - The position is calculated from a 2 frame moving average (current + previous position)
3. Averaging2 - The position is calculated from a weighted average of the current frame and 2 previous frames
4. Processing - Test mode for use with the Processing sketch (this mode is prevented from being assigned to a profile)

The averaging modes are subtle but do reduce the motion jitter a bit without adding much if any noticeable lag.

### Default Buttons
- Trigger: Left mouse button
- A: Right mouse button
- B: Middle mouse button
- C/Reload: Mouse button 4
- Start: 1 key
- Select: 5 key
- Up/Down/Left/Right: Keyboard arrow keys
- Pedal: Mouse button 5
- C + Start: Esc key
- C + Select: Enter Pause mode

#### Default Buttons in Pause mode
- A, B, Start, Select: select a profile
- Start + Down: Normal gun mode (averaging disabled)
- Start + Up: Normal gun with averaging, switch between the 2 averaging modes (use serial monitor to see the setting)
- Start + A: Processing mode for use with the Processing sketch
- B + Down: Decrease IR camera sensitivity (use serial monitor to see the setting)
- B + Up: Increase IR camera sensitivity (use serial monitor to see the setting)
- C/Reload: Exit pause mode
- C/Reload + A: Toggle Offscreen Button Mode
- C/Reload + B: Toggle Solenoid Rapid Fire speed, between 3 levels.
- Left: Toggle Rumble *(when hardware switches are unavailable)*
- Right: Toggle Solenoid *(when hardware switches are unavailable)*
- Trigger: Begin calibration
- Start + Select: save settings to non-volatile memory (EEPROM or Flash depending on the board configuration)

### How to calibrate
1. Press the **C Button/Reload** to enter pause mode.
2. Press a button to select a profile unless you want to calibration the current profile.
3. Pull the **Trigger** to begin calibration.
4. Shoot the pointer at center of the screen and hold the trigger down for 1/3 of a second while keeping a steady aim.
5. The mouse should lock to the vertical axis. Use the **A**/**B** buttons (can be held down) to adjust the mouse vertical range. **A** will increase and **B** will decrease. Track the pointer at the top and bottom edges of the screen while adjusting.
6. Pull the **Trigger** for horizontal calibration.
7. The mouse should lock to the horizontal axis. Use the **A**/**B** buttons (can be held down) to adjust the mouse horizontal range. **A** will increase and **B** will decrease. Track the pointer at the left and right edges of the screen while adjusting.
8. Pull the **Trigger** to finish and return to run mode. Values will apply to the currently selected profile in memory.
9. After confirming the calibration is good, enter pause mode and press Start and Select to save the calibration to non-volatile memory. These will be saved, as well as the active profile, to be restored on replug/reboot.
 
Calibration can be cancelled during any step by pressing **Button C/Reload** or **Start** or **Select**. The gun will return to pause mode if you cancel the calibration.

#### Advanced calibration
- During center calibration, press **A** to skip this step and proceed to the vertical calibration
- During vertical calibration, tap **Up** or **Down** to manually fine tune the vertical offset
- During horizontal calibration, tap **Left** or **Right** to manually fine tune the horizontal offset

### IR Camera Sensitivity
The IR camera sensitivity can be adjusted. It is recommended to adjust the sensitivity as high as possible. If the IR sensitivity is too low then the pointer precision can suffer. However, too high of a sensitivity can cause the camera to pick up unwanted reflections that will cause the pointer to jump around. It is impossible to know which setting will work best since it is dependent on the specific setup. It depends on how bright the IR emitters are, the distance, camera lens, and if shiny surfaces may cause reflections.

A sign that the IR sensitivity is too low is if the pointer moves in noticeable coarse steps, as if it has a low resolution to it. If you have the sensitivity level set to max and you notice this then the IR emitters may not be bright enough.

A sign that the IR sensitivity is too high is if the pointer jumps around erratically. If this happens only while aiming at certain areas of the screen then this is a good indication a reflection is being detected by the camera. If the sensitivity is at max, step it down to high or minimum. Obviously the best solution is to eliminate the reflective surface. The Processing sketch can help daignose this problem since it will visually display the 4 IR points.

### Profiles
The sketch is configured with 4 profiles available (with up to 8 possible if desired, correlating to the d-pad). Each profile has its own calibration data, run mode, and IR camera sensitivity settings. Each profile can be selected from pause mode by pressing the associated button (A/B/Start/Select).

### Software Toggles
Introduced since v1.3 *(Serious Intensity)* is the ability to toggle hardware features at runtime, even without hardware switches!

While in pause mode, the toggles are as follows (color indicating what the board's builtin LED lights up with):
- Button C/Reload + Button A: **Offscreen Button Mode** (White) - For older games that only activate a reload function with a button press, this enables offscreen shots to send a right mouse click instead of a left click. If a working motor is installed, it will pulse on and off when enabled.
- Button C/Reload + Button B: **Rapid Fire Speed** (Magenta) - Sets the speed of the rapid fire (when autofire is enabled) by cycling between three different levels. When toggled, the solenoid will fire five times with the selected setting.
- Button C/Reload + Button A + Button B: **Burst Fire Mode** (Orange) - Sets the solenoid to fire in three-shot bursts for every trigger pull. When toggled, the solenoid will quickly fire three times when on, or held once for off.
- Left D-Pad: **Rumble Toggle** (Salmon) - Enables/disables the rumble functionality. When enabled, the motor will engage for a short period.
- Right D-Pad: **Solenoid Toggle** (Yellow) - Enables/disables the solenoid force feedback. When enabled, the solenoid will engage for a short period.

These settings are not saved in profiles stored in Non-Volatile Memory, but the default state of each function can be set in the sketch hardware options section.

#### Saving Settings to Non-Volatile Memory
The calibration data and profile settings can be saved in non-volatile memory by pressing Start + Select (by default). The currently selected profile is saved as the default for when the light gun is plugged in.

For ItsyBitsy M0 and M4 boards, the external on-board SPI flash memory is used; for ATmega32U4 and RP2040, the EEPROM is used (in the 2040's case, a simulated EEPROM provided by the USB stack that actually writes to program flash space, but EEPROM nonetheless).

#### Processing Mode
The Processing mode is intended for use with the [SAMCO Processing sketch](https://github.com/samuelballantyne/IR-Light-Gun/tree/master/Samco_4IR_Beta/Samco_4IR_Processing_Sketch_BETA). Download [Processing](processing.org) and the 4IR processing sketch. The Processing sketch lets you visually see the IR points as seen by the camera. This is very useful aligning the camera when building your light gun and for testing that the camera tracks all 4 points properly, as well as observing possible reflections.

## Technical Details & Assorted Errata

### Serial Handoff (Mame Hooker) Mode
Introduced in v2.0 *(That Heart, That Power)*, when enabled, the gun will automatically hand off control to an instance of Mame Hooker that's connected once a start code has been detected! If available, the onboard LED will change to a mid-intensity white to signal serial handoff mode (unless any LED events trigger it to change, which will follow those thereafter). As if now, it reacts to M1x2 (offscreen button toggle for that game), F0 solenoid feedback, F1 rumble feedback, and F2/3/4 LED feedback.

If you aren't already familiar with Mame Hooker, you'll **need compatible inis for each game you play** and **the gun's COM port should be set to match the player number** (COM1 for P1, COM2 for P2, etc.)! COM port assignment can be done in Windows via the Device Manager, or Linux via settings in the Wine registry of the prefix your game/Mame Hooker is started in ([instructions from WineHQ here](https://wiki.winehq.org/Wine_User%27s_Guide#Serial_and_Parallel_Ports), note that Arduino COM ports are indeed `ttyACM#`).

> [!WARNING]
> Serial Handoff mode will cause INSTABILITY if using an Arduino core without a patched TinyUSB - as of writing, upstream has a fatal bug where a large amount of serial data causes the board to unpredictably lock up, requiring physically unplugging from the PC. This has been fixed in the RP2040 core linked in this manual, but can't speak to others. For more information, see https://github.com/adafruit/Adafruit_TinyUSB_Arduino/issues/293

This can be disabled (and any other references to serial handoff mode) by commenting out `#define MAMEHOOKER` in the beginning of the sketch.

### Change USB ID for Multiple Guns
If you intend to use multiple GUN4ALLs, you'll want to change what the board reports itself as.

These are known as the **USB Implementer's Forum (USB-IF) identifiers**, and if multiple devices share a common display name and/or Product/Vendor ID, apps like RetroArch and TeknoParrot that read individual mouse devices will get VERY confused.

Thankfully, since v2.0-rev1 *(That Heart, That Power)*, these parameters are easily found and can be redefined in the sketch configuration area - just above the `PLAYER_NUMBER` indicator!

```
#define MANUFACTURER_NAME "3DP"
#define DEVICE_NAME "GUN4ALL-Con"
#define DEVICE_VID 0x0920
#define DEVICE_PID 0x1998
```

You may change these to suit whatever your heart desires - though the only parts *necessary to change for multiplayer* are the `DEVICE_VID` and `DEVICE_PID` pairs. Then, just reflash the board!

### Dual Core Mode
On compatible boards (atm, only the RP2040 provides two cores), the sketch will automatically split the processing load of button inputs onto the second core.

As of now, the performance of single and dual core modes on the RP2040 are similar, with the possibility of reducing button signal latency when input polling is on its own thread. Note:
- Since it's a separate thread, we have to mark the gunmode from core 2 while the thread in core 1 has to check if it's changed every cycle to ensure that pause mode and etc. works.
- ^ an extension to this, we only use the second core for inputs when in the main `runMode` - i.e., the gun is actually tracking and sending mouse input. Really don't think we need to use it for everything.

### Button Combo Masks
Below the button definitions are a bunch of constants that configure the buttons to control the gun. For example, enter and exit pause mode, and changing various settings. See the comments above each value for details.

Most button behaviours can be assigned a combination. If the comment says a button combination is used then it activates when the last button of the combination releases.

### Other Constants
- `IRSeen0Color` colour for the RGB LED when no IR points are seen
- `CalModeColor` colour for the RGB LED while calibrating

### Profile Array
There is a `ProfileCount` constant that defines the number of profiles. The `profileData` array has the default values for the profile. If there are no settings saved in non-volatile memory, then these values are used. There is no need to change these values if settings are saved to non-volatile memory, as that will always override these (even across board reflashes, unless you nuke the program flash by other means).

Find the `profileDesc` array to configure each profile. The profile descriptor allows you to specify:
- A button/combination to select the profile while in pause mode
- Colour used to light up an RGB LED (ItsyBitsy M0 and M4 include a DotStar, ItsyBitsy RP2040 includes a NeoPixel) when paused
- A text description for the button/combination
- An optional text label or brief description
