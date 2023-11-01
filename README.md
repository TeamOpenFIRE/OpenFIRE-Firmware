# SAMCO Enhanced+ - Arduino Powered Light Gun (now with added pew-pews!)
##### Alt working title: GUN4ALL

Based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO project](https://github.com/samuelballantyne/IR-Light-Gun)

[![YouTube Demo](https://i.ytimg.com/vi/Y_AKmZJIwDY/maxresdefault.jpg)](https://youtu.be/Y_AKmZJIwDY "YouTube Demo (Click to view!)")

## PLUS Enhancements!
- **Solenoid Support!** Get the authentic arcade feedback with every shot. Works when aiming *on-screen!*
- **Rumble Support!** Feel the subtle feedback of a motor, for those moments when you need to *shoot outside of the screen!*
- **Temperature Sensor Support!** With an optional TMP36 sensor, you can keep your solenoid working better for longer! Tempers feedback based on temperature readings with every shot.
- All upgrades are *optional,* and can work as a drop-in replacement for current SAMCO builds (with minor changes).
- Plenty of safety checks, to ensure rock-solid functionality without parts sticking or overheating. Now you too can feel like a helicopter parent!
- Remains forever open source, with *compatibility for GUN4IR parts!* Can use the same community resources of parts and tutorials for easier assembly of a complete build.
- Clearer labeling in the sketch for user readability. Ain't nobody got time to read!
- Fully cross-platform solution, all configuration done using the open-source Arduino IDE and adjustments done on the board!
- Made out of at least 49% passion and 49% stubbornness (and 2% spite)!

## Original Prow's Fork Enhancements
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
- Adafruit ItsyBitsy [M0](https://www.adafruit.com/product/3727), [M4](https://www.adafruit.com/product/3800), or [RP2040](https://www.adafruit.com/product/4888) (ATmega32U4 5V 16MHz or Pro Micro ATmega32U4 5V 16MHz might still work, but no guarantees on performance)
- DFRobot IR Positioning Camera SEN0158: [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html)
- 4 IR LED emitters: regular Wii sensor bars might work for small distances, but it's HIGHLY recommended to use [SFH 4547 LEDs](https://www.mouser.com/ProductDetail/720-SFH4547) w/ 5.6Ω *(ohm)* resistors. [Build tutorial here!](https://www.youtube.com/watch?v=dNoWT8CaGRc)
   * Optional: Any 12V solenoid, w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=4uWgqc8g1PM)
     * *Requires a DC power extension cable and a separate adjustable 12V power supply.*
   * Optional: Any 5V gamepad rumble motor, w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=LiJ5rE-MeHw)
   * Optional: Any 2-way SPDT switches, to adjust state of rumble/solenoid/rapid fire.

With minor modifications it should work with any SAMD21, SAMD51, or ATmega32U4 16MHz boards. [See the SAMCO project for build details.](https://github.com/samuelballantyne/IR-Light-Gun)

The RP2040 is the most performant board for the cheapest price, and future proofs your build (at the cost of no working EEPROM storage for profile saves), but the M0 and M4 should still work well! The ATmega-based boards have an EEPROM for saving settings, but might be out of code space by now with this build.

## Additional information
[Check out the enclosed instruction book!](SamcoEnhanced/README.md) Also see the README files in `libraries` for more information on library functionality.

For reference, the default schematic and (general) layout for the build and its optional extras are attached:
![Weh](https://raw.githubusercontent.com/SeongGino/ir-light-gun-plus/plus/SamcoPlus%20Schematic.png)
 * *Clarification: Rumble power can go to either the pin marked `VHi` (board decides power delivery) or `USB` (directly powered from the USB interface).*

## Known Issues (want to fix sooner rather than later):
- Start/Select/Dpad debouncing logic is weird and may cause buttons to stick or require a few tries to actuate properly. Still needs investigation (increasing the debounce time to 60 might help?).
- Temperature sensor *should* work, but haven't tested yet; there be ~~[elf goddesses](https://www.youtube.com/watch?v=DSgw9RKpaKY)~~ dargons.
- Code is still kind of a mess, so I should clean things up at some point maybe kinda.
- Solenoid *may or may not* cause EMI disconnects depending on the build, the input voltage, and the disarray of wiring in tight gun builds. **This is not caused by the sketch,** but something that theoretically applies to most custom gun builds (just happened to happen to me and didn't find many consistent search results involving this, so be forewarned!)
  * If, like me, you suffer from this, make sure you use thick enough wiring! I replaced my jumper cables with 18AWG wires, as well as reduced freely floating ground daisy chain clumps, and my build seems to hold up to sustained solenoid use now.

## TODO (can and will implement, just not now):
- Should implement support for rumble as an alternative force-feedback system; decouple rumble from off-screen exclusively at some point.
- Rumble probably should have better variability aside from just "full blast" and "half-blast". The two settings should be enough for most, but noting this regardless.
- Implement software solution for enabling the rumble or solenoid, or toggle rapid fire system; should probably make a new button mask combo to activate while in pause mode.

## Wishlist (things I want to but don't know how/can't do yet):
- Implement [MAMEHOOKER](http://dragonking.arcadecontrols.com/static.php?page=aboutmamehooker) support! Discussed in [#1](../../issues/1)
- Console support? [It's definitely possible!](https://github.com/88hcsif/IR-Light-Gun)
- Document and implement separate RGB LED support?
  * We currently use only a board's builtin DotStar or NeoPixel, but this is only for distinguishing between profiles and indicating camera state for now. Could make RGB LEDs react to events, i.e. trigger pulls.
- *Maybe* make an option for a true autofire that auto-reloads after a set amount of seconds or trigger pulls? Make the coordinates move to 0,0 and force a mouse unclick/click/unclick. Might be cheaty, but if someone wants it...


## Thanks:
* Samuel Ballantyne, for his SAMCO project which inspired my madness in the first place.
* Prow7, for improving an already promising project and providing the basis for this madness.
* Jaybee, for a frame of reference on implementing the feedback additions, and to hopefully act as an OS-agnostic competition for the (I assume) great GUN4IR.
* [My YouTube audience,](https://youtube.com/ThatOneSeong) for their endless patience as I couldn't help but work on this instead of videos.
* Emm, for being there when I needed her.
* And Autism.

If you enjoy my work, or somehow found entertainment in this, [please support my endeavors on Ko-fi!](https://ko-fi.com/ThatOneSeong)

  *~<3*
  
  *That One Seong*
