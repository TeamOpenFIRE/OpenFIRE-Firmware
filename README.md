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
- Arduino development environment

With minor modifications it should work with any SAMD21, SAMD51, or ATmega32U4 16MHz boards. See the SAMCO project for build details: https://github.com/samuelballantyne/IR-Light-Gun

I recommend using an ItsyBitsy M0 or M4 to future proof your build since there is way more memory, they are much faster, and the build is super easy if you use a SAMCO PCB. The ATmega32U4 builds are almost out of code space.

## Installation
1. Ensure you have the Arduino development environment installed with support for your board.
2. If you use an ItsyBitsy M0 or M4 then install the **Adafruit DotStar** and **Adafruit SPIFlash** libraries in the Library Manager.
3. Copy all the folders under **libraries** into your Arduino libraries folder.
4. Copy the **SamcoEnhanced** folder to your Arduino sketch folder.
5. Open the **SamcoEnhanced** sketch.

## Compiling
If you are using an ItsyBitsy M0 or M4 then it is recommended to set the Optimize option to -Ofast "Here be dragons" (or Fastest). If you are using an ItsyBitsy M4 (or any other SAMD51 board) then set the CPU Speed to 120 MHz standard. There is no need for overclocking.

## Sketch Configuration
The sketch is configured for a SAMCO 2.0 (GunCon 2) build. If you are using a SAMCO 2.0 PCB or your build matches the SAMCO 2.0 button assignment then the sketch will work as is. If you are using a different set of buttons then the sketch will have to be modified.

## Additional information
See the README file in the sketch folder for details on operation and configuration. Also see the README files in libraries for more information on library functionality.
