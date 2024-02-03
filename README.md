# IR-GUN4ALL - An Expanded Arduino-powered Light Gun System 
##### Original/more accurate title: SAMCO Enhanced+ (Plus, now with added pew-pews!)

Based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO project](https://github.com/samuelballantyne/IR-Light-Gun)

###### (new video demonstration coming soon!)

## PLUS Enhancements!
- **Solenoid Support!** Get the authentic arcade feedback with every shot, in either single shot, three-shot burst, or rapid fire modes. Intelligently works when aiming *on-screen!*
- **Rumble Support!** Feel the subtle feedback of a motor, for those moments when you need to *shoot outside of the screen!*
- **Temperature Sensor Support!** With an optional TMP36 sensor, you can keep your solenoid working better for longer! Tempers feedback based on temperature readings with every shot.
- **Offscreen Button Support!** The gun can distinguish between on- and off-screen aiming and *report different buttons* depending on screen detection! An optional setting is also available for older games that allows the trigger to send left or right-clicks when shooting off-screen.
- **Gamepad Output Support!** GUN4ALL reports as a five-button mouse, keyboard, *and gamepad* with ability to output the position as a joystick. The first open source lightgun system that's fully compatible with PCSX2 nightly on all platforms for multiplayer!
- **Toggleable Extras!** Aside from the option for **using hardware switches** baked in, the extras *can all be individually toggled mid-game* in Pause Mode (either via Hotkeys or a scrollable menu system)!
- **Supports any gun shell imaginable!** All original console and custom controller designs are compatible with careful consideration for all sorts of form-factors; from the one-button Sega Virtua Gun to the stick-toting behemoths of Cabela's Top Shot Rifles, to even planned support for [Blamcons](https://blamcon.com/) - if you can fit a Pico in it, *you can use it with GUN4ALL!*
- **RGB Support!** GUN4ALL supports both four-pin-style RGB LEDs (common anode & cathode supported), as well as internal *and* external [NeoPixels](https://www.adafruit.com/product/1938) for glowy-blinky feedback, for both navigating the gun's internal systems or reacting to RGB commands!
- **Mame Hooker Support!** Further your own goals with [Mame Hooker](http://dragonking.arcadecontrols.com/static.php?page=aboutmamehooker), compatible with Windows & Linux (thru Wine); the gun will automagically hand over control of offscreen button mode, peripherals and LEDs *for event aware feedbacks for even more immersive gameplay!*
- Dual Core Support; take advantage of the second core in the ever-popular *Raspberry Pi Pico's* **RP2040** chip for processing button inputs in parallel, (theoretically) reducing latency.
- Multiple Guns Support; easily set the gun to use binds for P1-4 with a single setting, swap player position on-the-fly, and change the USB identifier for each unique board without modifying deep rooted Arduino files!
- Fixed button responsiveness; no sticky inputs, and solid debouncing with no performance impact!
- All upgrades are *optional,* and can work as a drop-in upgrade for current SAMCO builds (with minor changes).
- Plenty of safety checks, to ensure rock-solid functionality without parts sticking or overheating. Now you too can feel like a helicopter parent!
- Remains forever open source, with *compatibility for GUN4IR parts!* Can use the same community resources of parts and tutorials for easier assembly of a complete build.
- Clearer labeling in the sketch for user readability, to streamline configuration as much as possible (with simpler automated installation/binary distribution and graphical configuration options coming soon)!
- Fully cross-platform solution; all initial hardware configuration done using the open-source Arduino IDE, profiles are saved on the board, and persist across updates!
- Made out of at least 49% passion and 49% stubbornness (and 2% friendly spite)!

## Original Prow's Fork Enhancements
- Increased precision for maths and mouse pointer position
- Glitch-free DFRobot positioning (`DFRobotIRPositionEx` library)
- IR camera sensitivity adjustment (`DFRobotIRPositionEx` library)
- Faster IIC clock option for IR camera (`DFRobotIRPositionEx` library)
- Optional averaging modes can be enabled to slightly reduce mouse position jitter
- Enhanced button debouncing and handling (`LightgunButtons` library)
- Modified AbsMouse to be a 5 button device (`AbsMouse5` library, now part of `TinyUSB_Devices`)
- Multiple calibration profiles
- Save settings and calibration profiles to flash memory (SAMD) or EEPROM (RP2040)
- Built in Processing mode for use with the SAMCO Processing sketch

## Requirements
- An Arduino-compatible microcontroller based on an **RP2040**.
  * Recommended boards for new builds would be the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) *(cheapest, most pins available)* Adafruit [Keeboar KB2040](https://www.adafruit.com/product/5302) *(cheaper, Pro Micro formfactor)* or [ItsyBitsy RP2040](https://www.adafruit.com/product/4888) *(compatible with [SAMCO boards](https://www.ebay.com/itm/184699412596) for drop-in compatibility with Namco Guncon hardware!)*
  * Keep in mind the differences in pinouts, as it will always be different between builds and boards!
- **DFRobot IR Positioning Camera SEN0158:** [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html) | [Mirrors list](https://octopart.com/sen0158-dfrobot-81833633)
- **4 IR LED emitters:** regular Wii sensor bars might work for small distances, but it's HIGHLY recommended to use [SFH 4547 LEDs](https://www.mouser.com/ProductDetail/720-SFH4547) w/ 5.6Ω *(ohm)* resistors. [Build tutorial here!](https://www.youtube.com/watch?v=dNoWT8CaGRc)
   * Optional: **Any 12/24V solenoid,** w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=4uWgqc8g1PM)
     * *Requires a DC power extension cable and a separate adjustable 12-24V power supply.*
   * Optional: **Any 5V gamepad rumble motor,** w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=LiJ5rE-MeHw)
   * Optional: **Any 2-way SPDT switches,** to adjust state of rumble/solenoid/rapid fire in hardware *(can be adjusted in software from pause mode if not available!)*

## Additional information
[Check out the enclosed instruction book!](SamcoEnhanced/README.md) Also see the README files in `libraries` for more information on library functionality.

For reference, the default schematic and (general) layout for the build and its optional extras are attached:
![Weh](https://raw.githubusercontent.com/SeongGino/ir-light-gun-plus/plus/SamcoPlus%20Schematic.png)
 * *Clarification: Rumble power can go to either the pin marked `VHi` (board decides power delivery) or `USB` (directly powered from the USB interface).*

## Known Issues (want to fix sooner rather than later):
- Serial communication (Mamehook or debug output) can randomly lock up operation due to complications with HID packets (Mouse/Keyboard/Pad commands) and Serial I/O clashing. See https://github.com/adafruit/Adafruit_TinyUSB_Arduino/issues/293
  * If having issues, reducing the baud rate of the serial port in the sketch and the config files might help. This is being actively investigated!
- MAMEHooker supports the main force feedback/lamp outputs and the offscreen button modeset, but is missing and will not react to the screen ratio or other modesets.
  * Are there games that have issues with this? It really should be resolved by the game/emulator, not the gun.
  * LED pulses sent rapidly may reset at the ON-falling position, but the effect looks kind of good actually. Is this really a bug?
- Temperature sensor *should* work, but haven't tested yet; there be ~~[elf goddesses](https://www.youtube.com/watch?v=DSgw9RKpaKY)~~ dargons.

> [!NOTE]
> Solenoid *may or may not* cause EMI disconnects depending on the build, the input voltage, and the disarray of wiring in tight gun builds. **This is not caused by the sketch,** but something that theoretically applies to most custom gun builds (just happened to happen to me and didn't find many consistent search results involving this, so be forewarned!) ***Make sure you use thick enough wiring!*** I replaced my jumper cables with 18AWG wires, as well as reduced freely floating ground daisy chain clumps, and my build seems to hold up to sustained solenoid use now.

## TODO (can and will implement, just not now):
- Implement expanded preferences saving/loading to/from EEPROM/flash. Should allow setting variables at runtime through the eventual GUI/serial and overriding pins layout with customs.
- Should implement support for rumble as an alternative force-feedback system (`RUMBLE_FF`); able to do so now, just have to do it.
- Code is still kind of a mess, so I should clean things up at some point maybe kinda.

## Wishlist (things I want to but don't know how/can't do yet):
- A more streamlined graphical interface specific for IR-GUN4ALL that doesn't require the full Arduino IDE setup.
  * The idea is to have an interface for both flashing the board (like GUN4IR, but supporting Linux as well as Windows) and configuring the gun (ideally in real-time, without having to re-flash to adjust e.g. solenoid settings or mappings).
    - Should use a native toolkit i.e. QT for Linux, though someone else would have to work on Windows support.
    - Should we use pre-defined pins for buttons/force feedback devices? Possible on a Pico at least, but less so on an Adafruit (would mean being able to hard-code pins, so only mapping profiles need to be added/saved).
  * Failing this, maybe have a Web-based UI to configure board settings (pin numbers, peripherals, etc.) at runtime, a'la [GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE).
    * This should be web browser agnostic, so WebUSB is off the table - consider RNDIS or alike (need examples)? Seong isn't downloading Chrome just to configure a gun :/
    * Preferably should be done by setting a runmode at boot by holding a button (trigger depressed at boot) to start webserver mode, which can't be accessed by Pause Mode otherwise.
  * Either one of these GUI options will necessitate moving away from pre-processor definitions/constants in-sketch to variables and stored in/pulled from EEPROM/SPI storage.
- Console support? [It's definitely possible!](https://github.com/88hcsif/IR-Light-Gun)
  * May be redundant, since PCs can emulate the consoles that this would be able to support anyways (GCon 2)...
- RP2040 has dual core support, currently handles input polling in parallel; any other boards that have dual cores?
  * ESP boards seem to have some support, but they're much more a pain in the butt to implement than the simple setup1()/loop1() the RP2040 needs.
- Support for diamonds LED arrangement to be setup-compatible with GUN4IR (**help wanted here!** [Discussed here](https://github.com/SeongGino/ir-light-gun-plus/issues/6)).

## Thanks:
* Samuel Ballantyne, for his SAMCO project which inspired my madness in the first place.
* Prow7, for improving an already promising project and providing the basis for this madness.
* Jaybee, for a frame of reference on implementing the feedback additions, and to hopefully act as an OS-agnostic competition for the (I assume) great GUN4IR.
* The people that have donated and given their thoughts about this silly little obsession I've been working on.
* The testers that have had to deal with my MAMEHOOKER integration stability bullshit (seriously, it's super appreciated, and I'm so sorry).
* [My YouTube audience,](https://youtube.com/@ThatOneSeong) for their endless patience as I couldn't help but work on this instead of videos.
* Emm, for being there when I needed her.
* And Autism.

If you enjoy my work, or somehow found entertainment in this, [please support my endeavors on Ko-fi!](https://ko-fi.com/ThatOneSeong)

  *~<3*
  
  *That One Seong*
