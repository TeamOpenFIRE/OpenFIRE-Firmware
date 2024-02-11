# IR-GUN4ALL - The Enclosed Instruction Book!

## Table of Contents:
 - [IR Emitter Setup](#ir-emitter-setup)
 - [Operations Manual](#operations-manual)
   - [Run Modes](#run-modes)
   - [Default Buttons](#default-buttons)
   - [Default Buttons in Pause Mode](#default-buttons-in-pause-mode-hotkey)
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

## IR Emitter setup
The IR emitters must be arranged with 2 emitters on opposite edges of your screen/monitor forming a rectangle or square. For example, if you're playing on a small PC monitor, you can use 2 Wii sensor bars; one on top of your screen and one below. However, if you're playing on a TV, you should consider building a set of high power black IR LEDs and arranging them like (larger) sensor bars at the top and bottom of the display.

For visual reference, the purple/pinkish trapezoids are how your emitters should be placed on the display:
![Emitters](https://raw.githubusercontent.com/SeongGino/ir-light-gun-plus/plus/Emitters%20Layout-monitor.png)

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
- A: Right mouse button (In low buttons mode, Start if pressed offscreen)
- B: Middle mouse button (In low buttons mode, Select if pressed offscreen)
- C/Reload: Mouse button 4
- Pump Action (Cabela's or alike): Right mouse button
- Start: 1 key
- Select: 5 key
- Up/Down/Left/Right: Keyboard arrow keys
- Pedal: Mouse button 5
- C + Start: Esc key

Pause mode can be entered by either pressing C + Select by default, or *holding the trigger plus the A Button with **no IR points in sight*** if hold-to-pause is enabled (pointing the gun towards the ground is recommended).

#### Default Buttons in Pause mode (Hotkey)
- A, B, Start, Select: select a profile
- Start + Down: Normal gun mode (averaging disabled)
- Start + Up: Normal gun with averaging, switch between the 2 averaging modes (use serial monitor to see the setting)
- Start + A: Processing mode for use with the Processing sketch
- B + Down: Decrease IR camera sensitivity (use a serial monitor to see the setting)
- B + Up: Increase IR camera sensitivity (use a serial monitor to see the setting)
- C/Reload: Exit pause mode
- C/Reload + A: Toggle Offscreen Button Mode
- Left: Toggle Rumble *(when no rumble switch is detected)*
- Right: Toggle Solenoid *(when no solenoid switch is detected)*
- Trigger: Begin calibration
- Start + Select: save settings to non-volatile memory (EEPROM or Flash depending on the board configuration)

#### Controls for Simple Pause Menu
- A: Navigate Left
- B: Navigate Right
- Trigger: Select option
- C: Exit pause mode
  - Holding A or B for half the duration of the hold-to-pause time (so ~2s by default) will also exit the simple pause menu.
Available options in simple pause menu are as follows, from first option to last before rolling back:
* Calibrate current profile (always the initial option)
* Switch profiles (submenu)
  * Select from profile 1-4 using the navigation buttons/trigger to select, or press C to back out.
* Save settings to non-volatile memory
* Toggle Rumble *(when no rumble switch is detected)*
* Toggle Solenoid *(when no solenoid switch is detected)*
* Send escape key signal to the PC

### How to calibrate
1. Press the **C Button/Reload** (or hold **Trigger + A** while no IR points are in sight if hold-to-pause is enabled) to enter pause mode.
2. In Hotkey mode, press a button to select a profile unless you want to calibrate the current profile.
3. Pull the **Trigger** (or select "Calibrate current profile" in simple pause) to begin calibration.
4. Shoot the pointer at center of the screen and press the trigger while keeping a steady aim.
5. The mouse should lock to the vertical axis. Use the **A**/**B** buttons (can be held down) to adjust the mouse vertical range. **A** will increase and **B** will decrease. Track the pointer at the top and bottom edges of the screen while adjusting.
  * If using a gun with only one button (i.e. a Sega Virtua Gun/Saturn Stunner), pressing **A**/the not-trigger button while the mouse is at the edge of the display will decrease the vertical range.
7. Pull the **Trigger** for horizontal calibration. The mouse should lock to the horizontal axis. Use the **A**/**B** buttons (can be held down) to adjust the mouse horizontal range. **A** will increase and **B** will decrease. Track the pointer at the left and right edges of the screen while adjusting.
  * Same logic for single-button guns applies here; pressing **A** while the pointer is at the edge of the screen will decrease the range. 
8. Pull the **Trigger** to finish and return to run mode. Values will apply to the currently selected profile in memory.
9. After confirming the calibration is good, enter pause mode and press Start and Select to save the calibration to non-volatile memory. These will be saved, as well as the active profile, to be restored on replug/reboot.
 
Calibration can be cancelled during any step by pressing **Button C/Reload** or **Start** or **Select**. The gun will return to pause mode without saving if you cancel the calibration.

#### Advanced calibration
- During center calibration, press **A** to skip this step and proceed to the vertical calibration
- During vertical calibration, tap **Up** or **Down** to manually fine tune the vertical offset
- During horizontal calibration, tap **Left** or **Right** to manually fine tune the horizontal offset

### IR Camera Sensitivity
The IR camera sensitivity can be adjusted. It is recommended to adjust the sensitivity as high as possible. If the IR sensitivity is too low then the pointer precision can suffer. However, too high of a sensitivity can cause the camera to pick up unwanted reflections that will cause the pointer to jump around. It is impossible to know which setting will work best since it is dependent on the specific setup. It depends on how bright the IR emitters are, the distance, camera lens, and if shiny surfaces may cause reflections.

A sign that the IR sensitivity is too low is if the pointer moves in noticeable coarse steps, as if it has a low resolution to it. If you have the sensitivity level set to max and you notice this then the IR emitters may not be bright enough.

A sign that the IR sensitivity is too high is if the pointer jumps around erratically. If this happens only while aiming at certain areas of the screen then this is a good indication a reflection is being detected by the camera. If the sensitivity is at max, step it down to high or minimum. Obviously the best solution is to eliminate the reflective surface. The Processing sketch can help daignose this problem since it will visually display the 4 IR points.

### Profiles
The sketch is configured with 4 profiles available (with up to 8 possible if desired, correlating to the d-pad). Each profile has its own calibration data, run mode, and IR camera sensitivity settings. Each profile can be selected from pause mode by pressing the associated button (A/B/Start/Select), or selecting them via the profiles submenu in simple pause menu.

### Software Toggles
Introduced since v1.3 *(Serious Intensity)* is the ability to toggle hardware features at runtime, even without hardware switches!

While in pause mode, the toggles are as follows (color indicating what the board's builtin LED lights up with):
- Button C/Reload + Button A: **Offscreen Button Mode** (White) - For older games that only activate a reload function with a button press, this enables offscreen shots to send a right mouse click instead of a left click. If a working motor is installed, it will pulse on and off when enabled.
- Button C/Reload + Button B: **Rapid Fire Speed** (Magenta) - Sets the speed of the rapid fire (when autofire is enabled) by cycling between three different levels. When toggled, the solenoid will fire five times with the selected setting.
- Left D-Pad: **Rumble Toggle** (Salmon) - Enables/disables the rumble functionality. When enabled, the motor will engage for a short period.
- Right D-Pad: **Solenoid Toggle** (Yellow) - Enables/disables the solenoid force feedback. When enabled, the solenoid will engage for a short period.

These settings are not saved in profiles stored in Non-Volatile Memory, but the default state of each function can be set in the sketch hardware options section.

#### Saving Settings to Non-Volatile Memory
The calibration data, profile settings, and extended gun options like custom pins mapping and rumble intensity, can be saved in non-volatile memory by pressing Start + Select (by default). The currently selected profile is saved as the default for when the light gun is plugged in - gun settings applies to *all profiles.*

For ItsyBitsy M0 and M4 boards, the external on-board SPI flash memory is used; for ATmega32U4 and RP2040, the EEPROM is used (in the 2040's case, a simulated EEPROM provided by the USB stack that actually writes to program flash space, but is considered EEPROM nonetheless).

#### Processing Mode
The Processing mode is intended for use with the [SAMCO Processing sketch](https://github.com/samuelballantyne/IR-Light-Gun/tree/master/Samco_4IR_Beta/Samco_4IR_Processing_Sketch_BETA). Download [Processing](processing.org) and the 4IR processing sketch. The Processing sketch lets you visually see the IR points as seen by the camera. This is very useful aligning the camera when building your light gun and for testing that the camera tracks all 4 points properly, as well as observing possible reflections.

## Technical Details & Assorted Errata

### Serial Handoff (Mame Hooker) Mode
Introduced in v2.0 *(That Heart, That Power)*, when enabled, the gun will automatically hand off control to an instance of Mame Hooker that's connected once a start code has been detected! If available, the onboard LED will change to a mid-intensity white to signal serial handoff mode (unless any LED events trigger it to change, which will follow those thereafter).

If you aren't already familiar with Mame Hooker, **you'll need compatible inis for each game you play** and **the gun's COM port should be set to match the player number** (COM1 for P1, COM2 for P2, etc.)! COM port assignment can be done in Windows via the Device Manager, or Linux via settings in the Wine registry of the prefix your game/Mame Hooker is started in. [Consult the wiki page on MAMEHOOKER for more information!](https://github.com/SeongGino/ir-light-gun-plus/wiki/MAMEHOOKER-for-Light-Guns-%E2%80%90-The-Secret-Files#mamehooker-setup-guide)

> [!WARNING]
> Serial Handoff mode will cause INSTABILITY if GUN4ALL is built using an Arduino core without a patched TinyUSB - as of writing, upstream has a fatal bug where a large amount of serial data causes the board to unpredictably lock up, requiring physically unplugging from the PC. This has been fixed using a forked RP2040 core and is used in official binaries, but be mindful of this if you're deploying customized builds. For more information, see https://github.com/adafruit/Adafruit_TinyUSB_Arduino/issues/293

### Change USB ID for Multiple Guns
If you intend to use multiple GUN4ALLs, you'll want to change what the board reports itself as.

These are known as the **USB Implementer's Forum (USB-IF) identifiers**, and if multiple devices share a common display name and/or Product/Vendor ID, apps like RetroArch and TeknoParrot that read individual mouse devices will get VERY confused.

Since v3.0 *(Rinko)*, these parameters can be saved to and loaded from EEPROM if a customized Device Product ID (PID) & Device Name are detected. Refer to the builtin settings category by calling `Xm` in a serial monitor/terminal for more information.
