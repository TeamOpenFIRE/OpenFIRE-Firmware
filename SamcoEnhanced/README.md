# SAMCO Prow Enhanced - Arduino Powered Light Gun

Based on the 4IR Beta "Big Code Update" SAMCO project from https://github.com/samuelballantyne/IR-Light-Gun

## Enhancements
- Increased precision for maths and mouse pointer position
- Glitch-free DFRobot positioning (DFRobotIRPositionEx library)
- IR camera sensitivity adjustment (DFRobotIRPositionEx library)
- Faster IIC clock option for IR camera (DFRobotIRPositionEx library)
- Optional averaging modes can be enabled to slightly reduce mouse position jitter
- Enhanced button debouncing and handling (LightgunButtons library)
- Modified AbsMouse to be a 5 button device (AbsMouse5 library)
- Multiple calibration profiles
- Save settings and calibration profiles to flash memory (SAMD) or EEPROM (ATmega32U4)
- Built in Processing mode for use with the SAMCO Processing sketch

## Requirements
- Adafruit ItsyBitsy M0, M4, ATmega32U4 5V 16MHz or Pro Micro ATmega32U4 5V 16MHz
- DFRobot IR Positioning Camera (SEN0158)
- 4 IR LED emitters

With minor modifications it should work with any SAMD21, SAMD51, or ATmega32U4 16MHz boards. See the SAMCO project for build details: https://github.com/samuelballantyne/IR-Light-Gun

I recommend using an ItsyBitsy M0 or M4 to future proof your build since there is way more memory, they are much faster, and the build is super easy if you use a SAMCO PCB. The ATmega32U4 builds are almost out of code space.

## IR Emitter setup
The IR emitters must be arranged with 2 emitters on opposite edges of your screen/monitor forming a rectangle or square. For example, you can use 2 Wii sensor bars; one on top of your screen and one below.

## Compiling
If you are using an ItsyBitsy M0 or M4 then it is recommended to set the Optimize option to -Ofast "Here be dragons" (or Fastest). If you are using an ItsyBitsy M4 (or any other SAMD51 board) then set the CPU Speed to 120 MHz standard. There is no need for overclocking.

## Operation
The light gun operates as a mouse until the button/combination is pressed to enter pause mode. The Arduino serial monitor (or any serial terminal) can be used to see information while the gun is paused and during the calibration procedure.

Note that the buttons in pause mode (and to enter pause mode) activate when the last button of the combination releases. This is used to detect and differentiate button combinations vs a single button press.

The mouse position updates at 209Hz so it is extremely responsive.

## Run modes
The gun has the following modes of operation:
1. Normal - The mouse position updates from each frame from the IR positioning camera (no averaging)
2. Averaging - The position is calculated from a 2 frame moving average (current + previous position)
3. Averaging2 - The position is calculated from a weighted average of the current frame and 2 previous frames
4. Processing - Test mode for use with the Processing sketch (this mode is prevented from being assigned to a profile)

The averaging modes are subtle but do reduce the motion jitter a bit without adding much if any noticeable lag.

## IR camera sensitivity
The IR camera sensitivity can be adjusted. It is recommended to adjust the sensitivity to as high as possible. If the IR sensitivity is too low then the pointer precision can suffer. However, too high of a sensitivity can cause the camera to pick up unwanted reflections that will cause the pointer to jump around. It is impossible to know which setting will work best since it is all dependent the specific setup. It depends on how bright the IR emitters are, the distance, camera lens, and if shiny surfaces may cause reflections.

A sign that the IR sensitivity is too low is if the pointer moves in noticeable steps acts as if it has a low resolution to it. The movement should look fluid and fine on an HD or even QHD screen. If you have the sensitivity level set to max and you notice this then the IR emitters may not be bright enough.

A sign that the IR sensitivity is too high is if the pointer jumps around erratically, especially when you try and aim at specific locations. This typically means a reflection is being picked up and used for positioning instead of one of the intended IR emitters. If the sensitivity is at max, step it down to high or minimum. Ideally eliminate the reflective surface.

## Profiles
By default there are 8 profiles available. Each profile has its own calibration data, run mode, and IR camera sensitivity settings.

## Default Buttons (normal running mode)
- Trigger: left mouse button
- A: left mouse button
- B: middle mouse button
- Start: 1 key
- Select: 5 key
- Up/Down/Left/Right: keyboard arrow keys
- Reload: Enter pause mode
- Pedal: Mouse button 5

## Default Buttons in Pause mode
- A, B, Start, Select, Up, Down, Left, Right: select a profile
- Start + Up: Normal gun with averaging, switch between the 2 averaging modes
- Start + Down: Normal gun mode (averaging disabled)
- Start + A: Processing mode for use with the Processing sketch
- B + Up: Cycle IR camera sensitivity (use serial monitor to see the setting)
- Reload: Exit pause mode
- Trigger: Begin calibration
- Start + Select: save settings to non-volatile memory (EEPROM or Flash depending on the board configuration)

## How to calibrate
1. Press Reload to enter pause mode.
2. Press a button to select a profile unless you want to calibration the current profile selection.
3. Pull Trigger to begin calibration.
4. Shoot cursor at center of the screen and hold the trigger down for 1/3 of a second. Release the trigger when you see the cursor move.
5. Mouse should lock to vertical axis. Use A/B buttons (can be held down) buttons to adjust mouse vertical range. A will increase, B will decrease. Track the top and bottom edges of the screen while adjusting.
6. Pull Trigger for horizontal calibration.
7. Mouse should lock to horizontal axis. Use A/B buttons (can be held down) to adjust mouse horizontal range. A will increase, B will decrease. Track the left and right edges of the screen while adjusting.
8. Pull Trigger to finish and return to run mode. Values will apply to the currently selected profile.
9. Recommended: After confirming the calibration is good, enter pause mode and press Start and Select to save the calibration to non-volatile memory.
10. Optional: Open serial monitor and update xCenter, yCenter, xOffset & yOffset values in the profile data array (no need with step 9).
 
Calibration can be cancelled during any step by pressing Reload or Start or Select. The gun will return to pause mode if you cancel the calibration.

### Advanced calibration
- During center calibration step press A to skip to the vertical offset
- During vertical offset calibration, tap up or down to manually fine tune the vertical offset
- During horizontal offset calibration, tap left or right to manually fine tune the horizontal offset

## Saving settings to non-volatile memory
The calibration data and profile settings can be saved in non-volatile memory. The currently selected profile is saved as the default when the light gun is plugged in.

For ItsyBitsy M0 and M4 boards the external on-board flash memory is used. For ATmega32U4 the EEPROM is used.

## Sketch Configuration
The sketch is configured for a SAMCO 2.0 (GunCon 2) build. If you are using a SAMCO 2.0 PCB or your build matches the SAMCO 2.0 button assignment then the sketch will work as is. If you are using a different set of buttons then the sketch will have to be modified.

### Define Buttons
Find the `LightgunButtons::ButtonDesc` array and define all of the buttons. The order of the buttons in the array represent a bit position. Define enum constants in the `ButtonMask_e` enum with the button bit mask values. There is also a `ButtonIndex_e` that defines the index positions in the array, but it is not currently not used except to define the bit mask values. See the `Desc_t` structure in the `LightgunButtons` for details on the structure.

### Behaviour buttons
Below the button definitions are a bunch of constants that configure the buttons to control the gun. For example, entering and exist pause mode, and changing various settings. See the comments above each value for details.

Most button behaviours can be assigned a combination. If the comment says a button combination is used then it activates when the last button of the combination releases. This

### Other constants
- `IRSeen0Color` colour for the RGB LED when no IR points are seen
- `CalModeColor` colour for the RGB LED while calibrating

### Profiles
There is a `ProfileCount` constant that defines the number of profiles. The `profileData` array has the default values for the profile. If there are no settings saved in non-volatile memory, then these values are used. There is no need to change these values if settings are saved to non-volatile memory.

Find the `profileDesc` array to configure each profile. The profile descriptor allows you to specify:
- A button/combination to select the profile while in pause mode
- Colour used to light up an RGB LED (ItsyBitsy M0 and M4 include a DotStar) when paused
- A text description for the button/combination
- An optional text label or brief description
