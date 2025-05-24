# OpenFIRE - The Enclosed Instruction Book!

## Table of Contents:
 - [IR Emitter Setup](#ir-emitter-setup)
 - [Board Configuration](#board-configuration)
 - [Operations Manual](#operations-manual)
   - [Run Modes](#run-modes)
   - [Default Buttons](#default-buttons)
   - [Default Buttons in Pause Mode](#default-buttons-in-pause-mode-hotkey)
   - [How to Calibrate](#how-to-calibrate)
   - [IR Camera Sensitivity](#ir-camera-sensitivity)
   - [Profiles](#profiles)
   - [Software Toggles](#software-toggles)
   - [Saving Settings to Flash](#saving-settings-to-flash)
   - [Test Mode](#test-mode)
 - [Technical Details & Assorted Errata](#technical-details--assorted-errata)
   - [Serial Handoff (Mame Hooker) Mode](#serial-handoff-mame-hooker-mode)
   - [Change USB ID for Multiple Guns](#change-usb-id-for-multiple-guns)

## IR Emitter setup
The IR emitters can be arranged in either two ways:
 - As dual lightbars, or "square" layout (Samco, preferred)
 - At four separate points on the display, or "diamond" layout (Xwiigun and other legacy lightgun systems)

if you're playing on a small PC monitor, you can use 2 Wii sensor bars; one on top of your screen and one below. However, if you're playing on a TV, you should consider building or buying a set of high power black IR LEDs and arranging them like (larger) sensor bars at the top and bottom of the display.

The OpenFIRE Desktop App has an alignment assistant that can be used to help align your emitters to the display (by selecting *Help->Open IR Emitter Alignment Assistant*) - alternatively, you can refer to this online alignment guide [here @ diylightgun.com](https://diylightgun.com/align/)

## Board Configuration
The gun is configured through the companion [OpenFIRE Desktop Application.](https://github.com/TeamOpenFIRE/OpenFIRE-App) The latest version of the App (v3.0 or later) can be opened without plugging in a microcontroller prior, and will regularly search for devices flashed with the latest Firmware.

When the board's COM port is selected in the app, the gun will go into a *Docked* state - this is what allows for real-time configuration of pin mappings, settings, changing between and renaming calibration profiles, and testing button inputs and force feedback devices. The camera will be disabled while in this mode, unless the gun is set to IR test mode.

## First-time Setup
When flashing a new board with OpenFIRE, or after clearing the flash, the first time it's plugged in will prompt for the user to pull the trigger button to start initial calibration - this can be accomplished from the App using any of the *Calibrate Profile* buttons, or pressing the trigger for standalone calibration (see the [How to Calibrate](#how-to-calibrate) section for more information). If your build is using custom pins, or you would like to change any settings at this point, the gun can be docked to the OpenFIRE App and configured prior to starting initial calibration - at least *Trigger* and *Button A* should be mapped and confirmed working in the *Gun Tests* tab.

## Operations Manual
The light gun operates as an absolute positioning mouse (like a stylus!) until the button/combination is pressed to enter pause mode. Alternatively, the gun can be signaled to output using its corresponding HID Gamepad device using a Serial Feedback Distributor program such as MAMEHOOKER - see the [Serial Handoff](#serial-handoff-mame-hooker-mode) section for more info.

Any serial terminal (Arduino IDE's Serial Monitor, *PuTTY,* *screen,* etc.) can be used to see information while the gun is paused and during standalone calibration.

Note that the buttons in pause mode (and to enter pause mode) activate when the last button of the combination releases. This is used to detect and differentiate button combinations vs a single button press.

* Note: At its peak, the mouse position updates at 209Hz, or roughly every ~4.8ms, so it is extremely responsive.

### Run modes
The gun has the following modes of operation:
1. Normal - The mouse position updates from each frame from the IR positioning camera (no averaging)
2. Averaging - The position is calculated from a 2 frame moving average (current + previous position)
3. Averaging2 - The position is calculated from a weighted average of the current frame and 2 previous frames
4. Processing - Test mode for use with the Desktop App (this mode is prevented from being assigned to a profile)

The averaging modes are subtle but do reduce the motion jitter a bit without adding much if any noticeable lag.

### Default Buttons
- Trigger: Left mouse button
- A: Right mouse button (In low buttons mode, Start if pressed offscreen)
- B: Middle mouse button (In low buttons mode, Select if pressed offscreen)
- C/Reload: Mouse button 4/Side Button 1/Back
- Pump Action (Cabela's or alike): Right mouse button
- Start: 1 key
- Select: 5 key
- Up/Down/Left/Right: Keyboard arrow keys
- Pedal Main: Mouse button 4/Side Button 1/Back
- Alt Pedal: Mouse button 5/Side Button 2/Forward
- C + Start: Esc key

Pause mode can be entered by either pressing C + Select by default, pressing the *Home Button* if used in current pin layout, or *holding the trigger plus the A Button with **no IR points in sight*** if hold-to-pause is enabled - pointing the gun towards the ground is recommended here.

#### Default Buttons in Pause mode (Hotkey)
- A, B, Start, Select: select a profile
- Start + Down: Normal gun mode (averaging disabled)
- Start + Up: Normal gun with averaging, switch between the 2 averaging modes (use serial monitor to see the setting)
- B + Down: Decrease IR camera sensitivity (use a serial monitor to see the setting)
- B + Up: Increase IR camera sensitivity (use a serial monitor to see the setting)
- C/Reload: Exit pause mode
- Left: Toggle Rumble *(when no rumble switch is detected)*
- Right: Toggle Solenoid *(when no solenoid switch is detected)*
- Trigger: Begin calibration
- Start + Select: save settings to non-volatile flash storage space

#### Controls for Simple Pause Menu
- A: Navigate Cursor Up
- B: Navigate Cursor Down
- Trigger: Select option
- C: Exit pause mode
  - Holding A or B for half the duration of the hold-to-pause time (so ~2s by default) will also exit the simple pause menu.
Available options in simple pause menu are as follows, from first option to last before rolling back:
* Calibrate current profile (always the initial option)
* Switch profiles (submenu)
  * Select from profile 1-4 using the navigation buttons/trigger to select, or press C to back out.
* Save settings to non-volatile memory
* Toggle Rumble *(when rumble is enabled & no switch is detected)*
* Toggle Solenoid *(when solenoid is enabled & no switch is detected)*
* Send escape key signal to the PC

### How to calibrate
##### These instructions apply to the standalone on-board calibration process; the Calibration screens in the OpenFIRE App has a similar procedure with more info to guide the user through the process.
1. Select the profile to calibrate - either through pressing A/B/Start/Select in the Hotkey Pause Mode, or selecting in the Simple Pause Menu - and pull the trigger to begin calibration. Alternatively, calibration can be initialized from the OpenFIRE App. 
2. Shoot the pointer at center of the screen and press the trigger while keeping a steady aim.
3. The mouse should move to the four edges of the screen; first topmost, bottommost, leftmost, rightmost. Shoot the edge of the screen where the cursor is at.
4. When the cursor returns to the center, shoot the cursor to finish calibration.
5. The new calibration profile will be applied and you'll be able to test the tracking. A good sign of a good calibration is maintaining as close to line-of-sight accuracy as possible when aiming at the screen edges and corners.
   - If the calibration is good, pull the trigger to confirm.
   - If you want to start calibration over, press the A or B button in cali verification to restart from the center point in Step 2.
   - Calibration can be canceled outright by pressing C/Reload at any time, or the A/B buttons any time before cali verification.
Remember to save your calibration and current profile afterwards, either by sending the save signal from the app, pressing Start+Select in Hotkey Pause Mode, or selecting the third "Save Settings" option in the Simple Pause Menu.

### IR Camera Sensitivity
The IR camera sensitivity can be adjusted. It is recommended to adjust the sensitivity as high as possible. If the IR sensitivity is too low then the pointer precision can suffer. However, too high of a sensitivity can cause the camera to pick up unwanted reflections that will cause the pointer to jump around. It is impossible to know which setting will work best since it is dependent on the specific setup. It depends on how bright the IR emitters are, the distance, camera lens, and if shiny surfaces may cause reflections.

A sign that the IR sensitivity is too low is if the pointer moves in noticeable coarse steps, as if it has a low resolution to it. If you have the sensitivity level set to max and you notice this, the IR emitters may not be bright enough.

A sign that the IR sensitivity is too high is if the pointer jumps around erratically. If this happens only while aiming at certain areas of the screen, this is a good indication that a reflection is being detected by the camera. If the sensitivity is at max, step it down to high or minimum. Obviously, the best solution is to eliminate the reflective surface. The Desktop App's Test Mode can help diagnose this problem, since it will visually display the 4 IR points.

### Profiles
The main OpenFIRE builds are configured with 4 calibration profiles available. Each profile has its own calibration data, run mode, and IR camera sensitivity settings. Each profile can be selected from pause mode by pressing the associated button (A/B/Start/Select), or selecting them via the profiles submenu in simple pause menu.

### Software Toggles
Hardware features can be toggled at runtime, even without hardware switches defined!

While in pause mode, the toggles are as follows (color indicating what the board's builtin LED lights up with):
- Left D-Pad: **Rumble Toggle** (Salmon) - Enables/disables the rumble functionality. When enabled, the motor will engage for a short period.
- Right D-Pad: **Solenoid Toggle** (Yellow) - Enables/disables the solenoid force feedback. When enabled, the solenoid will engage for a short period.
These can also be done from the respective setting in the Simple Pause Menu.

The current state of these settings (except Offscreen Button Mode) are saved when committed to, and pulled from flash storage space at boot.

#### Saving Settings to Flash
The calibration data, profile settings, and extended gun options like custom pins mapping and rumble intensity, can be saved in non-volatile memory by pressing Start + Select from the gun itself, or whenever new settings are committed from the Desktop App. The currently selected calibration profile is saved as the default for when the light gun is plugged in - gun settings (pins mapping, force feedback, etc.) applies to *all profiles.*

#### Test Mode
Test Mode lets you visually see the IR points as seen by the camera in the Desktop App's IR Test Mode screen. This is very useful for aligning the camera when building your light gun, and for testing that the camera tracks all 4 points properly, as well as observing possible reflections. The validity of the test points shape (square in default IR layout, diamond in alt IR layout) depends on the current profile used and its IR layout setting.

## Technical Details & Assorted Errata

### Serial Handoff (Mame Hooker) Mode
The gun will automatically hand off control to an instance of Mame Hooker that's connected once a start code has been detected! If available, the onboard LED and any *non-static* external NeoPixels will change to a mid-intensity white to signal serial handoff mode (unless any LED events trigger it to change, which will follow those thereafter).

If you aren't already familiar with Mame Hooker, **you'll need compatible inis for each game you play** and **the gun's COM port should be set to match the player number** (COM1 for P1, COM2 for P2, etc.)! COM port assignment can be done in Windows via the Device Manager, or Linux via settings in the Wine registry of the prefix your game/Mame Hooker is started in. [Consult the wiki page on MAMEHOOKER for more information!](https://github.com/TeamOpenFIRE/OpenFIRE-Firmware/wiki/MAMEHOOKER-Documentation) For Linux users wanting to use their gun with native emulators' force feedback (currently MAME, Flycast, or their RetroArch ports), consider trying [QMamehook](https://github.com/SeongGino/QMamehook).

### Change USB ID for Multiple Guns
If you intend to use multiple OpenFIRE guns, you'll want to change what the board reports itself as.

These are known as the **USB Implementer's Forum (USB-IF) identifiers**, and if multiple devices share a common display name and/or Product/Vendor ID, apps like RetroArch and TeknoParrot that read individual mouse devices will get VERY confused.

These parameters can be saved to and loaded from flash storage space if a customized Device Product ID (PID) & Device Name are detected, which can be committed from the Desktop App.
