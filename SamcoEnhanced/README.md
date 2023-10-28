# SAMCO Prow Enhanced+ - The Enclosed Instruction Book!

## Table of Contents:
 - [Setup Guide](#setup-guide)
   - [IR Emitter Setup](#ir-emitter-setup)
   - [Arduino Library Dependencies](#arduino-library-dependencies)
   - [Compiling & Configuration Options](#compiling--configuration-options)
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
   - [Saving settings to non-volatile memory](#saving-settings-to-non-volatile-memory)
   - [Processing Mode](#processing-mode)
 - [Technical Details & Assorted Errata](#technical-details--assorted-errata)
   - [Button Combo Masks](#button-combo-masks)
   - [Other Constants](#other-constants)
   - [Profile Array](#profile-array)

## Setup Guide

### IR Emitter setup
The IR emitters must be arranged with 2 emitters on opposite edges of your screen/monitor forming a rectangle or square. For example, if you're playing on a small PC monitor, you can use 2 Wii sensor bars; one on top of your screen and one below. However, if you're playing on a TV, you should consider building a set of high power black IR LEDs and arranging them like (larger) sensor bars at the top and bottom of the display.

### Arduino Library Dependencies
Be sure to have the following libraries installed depending on the board you are using (or just install them all).
- Adafruit DotStar (for ItsyBitsy M0 and M4)
- Adafruit SPI Flash (for ItsyBitsy M0 and M4)
- Adafruit NeoPixel (for ItsyBitsy RP2040)
- Adafruit TinyUSB (for ItsyBitsy RP2040)

### Compiling & Configuration Options

Under the "Tools" dropdown in the Arduino IDE, set the compiler to use `Optimize Even More (-O3)`. Use the default clock speeds on M4 or RP2040-based boards.

The USB Stack option to should be set to `Arduino` for **most boards** (SAMD, ATmega), and `Adafruit TinyUSB` for **RP2040**s.

### Sketch Configuration
The sketch is configured for a [SAMCO 2.0](https://www.ebay.com/itm/184699412596) (GunCon 2) build... *ish.* Because of the additions added (that may or may not conflict with a SAMCO 2.0 board's pin mapping), you'll probably have to adjust what pins are arranged to what buttons manually, and which pins are reserved for the Rumble and Solenoid signal wires (if any). You can still use a loose board for a custom non-Guncon build, just change the pinout as you see fit - see section below on [Defining Buttons & Timers](#define-buttons--timers).

A0 is reserved for an optional temperature sensor if you plan to use a solenoid - it is recommended if you use a weaker/smaller solenoid or intend to run it in autofire mode for extended periods of time, as the sketch will temper activation at higher temperature thresholds (that are configurable!), but will function without.

### Define Buttons & Timers
Refer to Line 119 for the pins of the tactile extras (rumble, solenoid, hardware switches if need be) and the adjustable settings (solenoid activation, rumble event length, etc.), and Line 140-onwards for the button constants.

Remember that the sketch uses the Arduino GPIO pin numbers; on the Adafruit Itsybitsy RP2040, these are the silkscreen labels on the **underside** of the microcontroller (marked GP00-29). Also note that this does not apply to the analog pins (A0-A3), which does work. All the other Adafruit boards don't have this discrepancy.
![Itsybitsy RP2040 Back](https://cdn-learn.adafruit.com/assets/assets/000/101/909/original/adafruit_products_ItsyRP_pinouts_back.jpg)

## Operations Manual
The light gun operates as an absolute positioning mouse (like a stylus!) until the button/combination is pressed to enter pause mode. The Arduino serial monitor (or any serial terminal) can be used to see information while the gun is paused and during the calibration procedure.

Note that the buttons in pause mode (and to enter pause mode) activate when the last button of the combination releases. This is used to detect and differentiate button combinations vs a single button press.

* Note: The mouse position updates at 209Hz, so it is extremely responsive.

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
9. Recommended (for ATmega users): After confirming the calibration is good, enter pause mode and press Start and Select to save the calibration to non-volatile memory.
   * Else, for **RP2040 users:** Open serial monitor and update the `xCenter`, `yCenter`, `xScale`, and `yScale` values in the profile data array in the sketch.
 
Calibration can be cancelled during any step by pressing **Reload** or **Start** or **Select**. The gun will return to pause mode if you cancel the calibration.

#### Advanced calibration
- During center calibration, press **A** to skip this step and proceed to the vertical calibration
- During vertical calibration, tap **Up** or **Down** to manually fine tune the vertical offset
- During horizontal calibration, tap **Left** or **Right** to manually fine tune the horizontal offset

### IR Camera Sensitivity
The IR camera sensitivity can be adjusted. It is recommended to adjust the sensitivity as high as possible. If the IR sensitivity is too low then the pointer precision can suffer. However, too high of a sensitivity can cause the camera to pick up unwanted reflections that will cause the pointer to jump around. It is impossible to know which setting will work best since it is dependent on the specific setup. It depends on how bright the IR emitters are, the distance, camera lens, and if shiny surfaces may cause reflections.

A sign that the IR sensitivity is too low is if the pointer moves in noticeable coarse steps, as if it has a low resolution to it. If you have the sensitivity level set to max and you notice this then the IR emitters may not be bright enough.

A sign that the IR sensitivity is too high is if the pointer jumps around erratically. If this happens only while aiming at certain areas of the screen then this is a good indication a reflection is being detected by the camera. If the sensitivity is at max, step it down to high or minimum. Obviously the best solution is to eliminate the reflective surface. The Processing sketch can help daignose this problem since it will visually display the 4 IR points.

### Profiles
The sketch is configured with 8 profiles available. Each profile has its own calibration data, run mode, and IR camera sensitivity settings. Each profile can be selected from pause mode by assigning a unique button or combination.

#### Saving Settings to Non-Volatile Memory
The calibration data and profile settings can be saved in non-volatile memory. The currently selected profile is saved as the default for when the light gun is plugged in.

For ItsyBitsy M0 and M4 boards the external on-board SPI flash memory is used. For ATmega32U4 the EEPROM is used. The RP2040 *should* have flash memory reserved too, but doesn't seem to work unfortunately. :(

#### Processing Mode
The Processing mode is intended for use with the [SAMCO Processing sketch](https://github.com/samuelballantyne/IR-Light-Gun/tree/master/Samco_4IR_Beta/Samco_4IR_Processing_Sketch_BETA). Download [Processing](processing.org) and the 4IR processing sketch. The Processing sketch lets you visually see the IR points as seen by the camera. This is very useful aligning the camera when building your light gun and for testing that the camera tracks all 4 points properly, as well as observing possible reflections.

## Technical Details & Assorted Errata

### Button Combo Masks
Below the button definitions are a bunch of constants that configure the buttons to control the gun. For example, enter and exit pause mode, and changing various settings. See the comments above each value for details.

Most button behaviours can be assigned a combination. If the comment says a button combination is used then it activates when the last button of the combination releases.

### Other Constants
- `IRSeen0Color` colour for the RGB LED when no IR points are seen
- `CalModeColor` colour for the RGB LED while calibrating

### Profile Array
There is a `ProfileCount` constant that defines the number of profiles. The `profileData` array has the default values for the profile. If there are no settings saved in non-volatile memory, then these values are used. There is no need to change these values if settings are saved to non-volatile memory.

Find the `profileDesc` array to configure each profile. The profile descriptor allows you to specify:
- A button/combination to select the profile while in pause mode
- Colour used to light up an RGB LED (ItsyBitsy M0 and M4 include a DotStar) when paused
- A text description for the button/combination
- An optional text label or brief description
