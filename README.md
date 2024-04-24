###### If you enjoy or if my work's helped you in any way,
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/Z8Z5NNXWL)
# IR-GUN4ALL - An Expanded Arduino-powered Light Gun System 
##### Original/more accurate title: SAMCO Enhanced+ (Plus, now with added pew-pews!)

Based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO project](https://github.com/samuelballantyne/IR-Light-Gun)

###### (new video demonstration coming soon!)

### Looking for configuration? Check out the [GUN4ALL-GUI](https://github.com/SeongGino/GUN4ALL-GUI)!

## PLUS Enhancements!
- **Drag'n'drop installation!** One-minute initial setup with provided binaries for a number of *RP2040*-based boards; just plug the board in, drag the firmware over, and then configure using the [GUN4ALL-GUI](https://github.com/SeongGino/GUN4ALL-GUI), compatible with both Windows and flavors of Linux. *No license required,* and configurations are saved to the board for easier use across multiple systems!
- **Solenoid Support!** Get the authentic arcade feedback with every shot, in either single shot, three-shot burst, or rapid fire modes. Intelligently works when aiming *on-screen!*
- **Rumble Support!** Feel the subtle feedback of a motor, for those moments when you need to *shoot outside of the screen!*
- **Temperature Sensor Support!** With an optional TMP36 sensor, you can keep your solenoid working better for longer! Tempers feedback based on temperature readings with every shot.
- **Offscreen Button Support!** The gun can distinguish between on- and off-screen aiming and *report different buttons* depending on screen detection! An optional setting is also available for older games that allows the trigger to send left or right-clicks when shooting off-screen.
- **Gamepad Output Support!** GUN4ALL reports as a plug'n'play five-button mouse, keyboard, *and gamepad* with ability to output the position as either the left or right joystick (with the other reserved for the gun's physical analog stick, if available). The first open source lightgun system that's fully compatible with PCSX2 nightly on all platforms for multiplayer!
- **Toggleable Extras!** Aside from the option for **using hardware switches** baked in, the extras *can all be individually toggled mid-game* in Pause Mode (either via Hotkeys or a scrollable menu system)!
- **Supports any gun shell imaginable!** All original console and custom controller designs are compatible with careful consideration for all sorts of form-factors; from the one-button Sega Virtua Gun to the stick-toting behemoths of Cabela's Top Shot Rifles, to even planned support for [Blamcons](https://blamcon.com/) - if you can fit a Pico in it, *you can use it with GUN4ALL!*
- **RGB Support!** GUN4ALL supports both four-pin-style RGB LEDs (common anode & cathode supported), as well as internal *and* external [NeoPixels](https://www.adafruit.com/product/1938) for glowy-blinky feedback, for both navigating the gun's internal systems or reacting to RGB commands!
- **Mame Hooker Support!** Further your own goals with [Mame Hooker](http://dragonking.arcadecontrols.com/static.php?page=aboutmamehooker), compatible with Windows & Linux (thru Wine); the gun will automagically hand over control of offscreen button mode, peripherals and LEDs *for event aware feedbacks for even more immersive gameplay!*
- Dual Core Support; take advantage of the second core in the ever-popular *Raspberry Pi Pico's* **RP2040** chip for processing button inputs in parallel, (theoretically) reducing latency.
- Multiple Guns Support; easily set the gun to use binds for P1-4 with a single setting, swap player position on-the-fly, and change the USB identifier for each unique board without modifying deep rooted Arduino files!
- Fixed button responsiveness; no sticky inputs, and solid debouncing with no performance impact!
- All upgrades are *optional,* and can work as a drop-in upgrade and/or replacement for current GUN4IR *and* SAMCO builds.
- Expanded save support, allowing changes to *every facet* of the firmware to be pushed to the board; no more reflashing just to change that one setting!
- Plenty of safety checks, to ensure rock-solid functionality without parts sticking or overheating. Now you too can feel like a helicopter parent!
- Remains forever open source, with *compatibility for GUN4IR parts!* Can use the same community resources of parts and tutorials for easier assembly of a complete build.
- Clearer labeling in the sketch for custom builders, to streamline deployments as much as possible
- Made out of at least 49% passion and 49% stubbornness (and 2% friendly spite)!

### Original Prow's Fork Enhancements
- Increased precision for maths and mouse pointer position
- Glitch-free DFRobot positioning (`DFRobotIRPositionEx` library)
- IR camera sensitivity adjustment (`DFRobotIRPositionEx` library)
- Faster IIC clock option for IR camera (`DFRobotIRPositionEx` library)
- Optional averaging modes can be enabled to slightly reduce mouse position jitter
- Enhanced button debouncing and handling (`LightgunButtons` library)
- Modified AbsMouse to be a 5 button device (`AbsMouse5` library, now part of `TinyUSB_Devices`)
- Multiple calibration profiles
- Save settings and calibration profiles to flash memory (SAMD) or EEPROM (RP2040)
- Built in Processing mode for use with the SAMCO Processing sketch (now part of GUN4ALL-GUI)

## Requirements
- An Arduino-compatible microcontroller based on an **RP2040**.
  * Recommended boards for new builds would be the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) *(cheapest, most pins available),* Adafruit [Kee Boar KB2040](https://www.adafruit.com/product/5302) *(cheaper, Pro Micro formfactor, compatible with [GUN4IR boards](https://www.gun4ir.com/products/universal-gun4ir-diy-pcb)),* or [ItsyBitsy RP2040](https://www.adafruit.com/product/4888) *(compatible with [SAMCO boards](https://www.ebay.com/itm/184699412596))*
  * Keep in mind the differences in pinouts, as it will always be different between builds and boards!
- **DFRobot IR Positioning Camera SEN0158:** [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html) | [Mirrors list](https://octopart.com/sen0158-dfrobot-81833633)
- **4 IR LED emitters:** regular Wii sensor bars might work for small distances, but it's HIGHLY recommended to use [SFH 4547 LEDs](https://www.mouser.com/ProductDetail/720-SFH4547) w/ 5.6Ω *(ohm)* resistors. [Build tutorial here!](https://www.youtube.com/watch?v=dNoWT8CaGRc)
   * Optional: **Any 12/24V solenoid,** w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=4uWgqc8g1PM) [Easy driver board here](https://oshpark.com/shared_projects/bjY4d7Vo)
     * *Requires a DC power extension cable &/or DC pigtail, and a separate adjustable 12-24V power supply.*
   * Optional: **Any 5V gamepad rumble motor,** w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=LiJ5rE-MeHw) [Easy driver board here](https://oshpark.com/shared_projects/VdsmUaSm)
   * Optional: **Any 2-way SPDT switches,** to adjust state of rumble/solenoid/rapid fire in hardware *(can be adjusted in software from pause mode if not available!)*
 
## Installation:
Grab the latest *.UF2* binary for your board from the [releases page](https://github.com/SeongGino/ir-light-gun-plus/releases/latest), and drag'n'drop the file to your microcontroller while booted into Bootloader mode (RP2040 is automatically mounted like this when no program is loaded, or can be forced into this mode by holding BOOTSEL while plugging it into the computer - it will appear as a removable storage device called **RPI-RP2**).

## Additional information
[Check out the enclosed instruction book!](SamcoEnhanced/README.md) Also see the README files in `libraries` for more information on library functionality.

For reference, the default schematic and (general) layout for the build and its optional extras are attached.
![Guh](https://raw.githubusercontent.com/SeongGino/ir-light-gun-plus/plus/SamcoPlus%20Schematic-pico.png)
 * *Layouts can be customized after installing the firmware - the only pins that **must** match are Camera Data & Clock.*

## Known Issues (want to fix sooner rather than later):
- Calibrating while serial activity is ongoing has a chance of causing the gun to lock up (exact cause still being investigated).
- Camera failing initialization will cause the board to lock up
  * Add extra feedback in the initial docking message for the GUI to alert the user of the camera not working.?
- Temperature sensor *should* work, but haven't tested yet; there be ~~[elf goddesses](https://www.youtube.com/watch?v=DSgw9RKpaKY)~~ dargons.

> [!NOTE]
> Solenoid *may or may not* cause EMI disconnects with certain wiring. **This is not caused by GUN4ALL,** but is indicative of too-thin wiring on the cables going to/from the solenoid driver. Cables for this run specifically should be **22AWG** at its thinnest - or else the cables will become antennas under extended use, which will trip USB safety thresholds in your PC to protect the ports.

## TODO:
- (Re-)expose button function remapping.
- Should implement support for rumble as an alternative force-feedback system (`RUMBLE_FF`).
- Some code functions should be offloaded to classes (a lot of core functionality/variables are all shoved into `SamcoEnhanced.ino` atm).

## Thanks:
* Samuel Ballantyne, for his SAMCO project which inspired my madness in the first place.
* Prow7, for improving an already promising project and providing the basis for this madness.
* Jaybee, for a frame of reference on implementing the feedback additions, and to hopefully act as an OS-agnostic competition for the (I assume) great GUN4IR.
* The people that have donated and given their thoughts about this silly little obsession I've been working on.
* The testers that have had to deal with my MAMEHOOKER integration stability bullshit (seriously, it's super appreciated, and I'm so sorry).
* [My YouTube audience,](https://youtube.com/@ThatOneSeong) for their endless patience as I couldn't help but work on this instead of videos.
* Emm, for being there when I needed her.
* And Autism.

  *~<3*
  
  *That One Seong*
