# IR-GUN4ALL - An Expanded Arduino-powered Light Gun System 
##### Original/more accurate title: SAMCO Enhanced+ (Plus, now with added pew-pews!)

Based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO project](https://github.com/samuelballantyne/IR-Light-Gun)

###### *(older video demo, doesn't reflect all the current features)
[![YouTube Demo](https://i.ytimg.com/vi/Y_AKmZJIwDY/maxresdefault.jpg)](https://youtu.be/Y_AKmZJIwDY "YouTube Demo (Click to view!)")

## PLUS Enhancements!
- **Solenoid Support!** Get the authentic arcade feedback with every shot, in either single shot, three-shot burst, or rapid fire modes. Intelligently works when aiming *on-screen!*
- **Rumble Support!** Feel the subtle feedback of a motor, for those moments when you need to *shoot outside of the screen!*
- **Temperature Sensor Support!** With an optional TMP36 sensor, you can keep your solenoid working better for longer! Tempers feedback based on temperature readings with every shot.
- **Offscreen Button Support!** An optional setting for older games, the gun will send a different button input (right click) when shooting off-screen if enabled!
- **Toggleable Extras!** Aside from the option for **using hardware switches** baked in, the extras *can all be individually toggled mid-game* in pause mode (Button C + Select)!
- Dual Core Support; If using a board powered by the RP2040, it will take advantage of that second core for processing button inputs in parallel, (theoretically) reducing latency.
- Mame Hooker Support! **(Experimental!)** Further your own goals with [Mame Hooker](http://dragonking.arcadecontrols.com/static.php?page=aboutmamehooker), compatible with Windows & Linux (thru Wine); the gun will automagically hand over control of offscreen button mode, peripherals and LEDs (WIP) *for event aware feedbacks for even more immersive gameplay!*
- Fixed button responsiveness; no sticky inputs, and solid debouncing with no performance impact!
- All upgrades are *optional,* and can work as a drop-in upgrade for current SAMCO builds (with minor changes).
- Plenty of safety checks, to ensure rock-solid functionality without parts sticking or overheating. Now you too can feel like a helicopter parent!
- Remains forever open source, with *compatibility for GUN4IR parts!* Can use the same community resources of parts and tutorials for easier assembly of a complete build.
- Clearer labeling in the sketch for user readability, to streamline configuration as much as possible!
- Fully cross-platform solution, all initial hardware configuration done using the open-source Arduino IDE and profiles are saved on the board!
- Made out of at least 49% passion and 49% stubbornness (and 2% friendly spite)!

## Original Prow's Fork Enhancements
- Increased precision for maths and mouse pointer position
- Glitch-free DFRobot positioning (DFRobotIRPositionEx library)
- IR camera sensitivity adjustment (DFRobotIRPositionEx library)
- Faster IIC clock option for IR camera (DFRobotIRPositionEx library)
- Optional averaging modes can be enabled to slightly reduce mouse position jitter
- Enhanced button debouncing and handling (LightgunButtons library)
- Modified AbsMouse to be a 5 button device (`AbsMouse5` library, now part of `TinyUSB_Devices`)
- Multiple calibration profiles
- Save settings and calibration profiles to flash memory (SAMD) or EEPROM (RP2040)
- Built in Processing mode for use with the SAMCO Processing sketch

## Requirements
- An Arduino-compatible microcontroller based on a **Cortex-M0/M0+**, **Cortex-M4**, or **RP2040**. *(ATmega32U4 boards don't have enough codespace at this point.)*
  * Recommended boards for new builds would be the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) *(cheapest, globally available)* Adafruit [Keeboar KB2040](https://www.adafruit.com/product/5302) *(cheaper, Pro Micro formfactor)* or [ItsyBitsy RP2040](https://www.adafruit.com/product/4888) *(more pins, compatible with [SAMCO boards](https://www.ebay.com/itm/184699412596) for drop-in compatibility with Namco Guncon hardware!)*
  * Keep in mind the differences in pinouts, as it will always be different between builds and boards!
- **DFRobot IR Positioning Camera SEN0158:** [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html)
- **4 IR LED emitters:** regular Wii sensor bars might work for small distances, but it's HIGHLY recommended to use [SFH 4547 LEDs](https://www.mouser.com/ProductDetail/720-SFH4547) w/ 5.6Ω *(ohm)* resistors. [Build tutorial here!](https://www.youtube.com/watch?v=dNoWT8CaGRc)
   * Optional: **Any 12/24V solenoid,** w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=4uWgqc8g1PM)
     * *Requires a DC power extension cable and a separate adjustable 12-24V power supply.*
   * Optional: **Any 5V gamepad rumble motor,** w/ associated relay board. [Build tutorial here!](https://www.youtube.com/watch?v=LiJ5rE-MeHw)
   * Optional: **Any 2-way SPDT switches,** to adjust state of rumble/solenoid/rapid fire in hardware *(can be adjusted in software from pause mode if not available!)*

The Pico/RP2040 is the most performant microcontroller for the cheapest price, and future proofs builds significantly, but the M0 and M4 should still work well!

## Additional information
[Check out the enclosed instruction book!](SamcoEnhanced/README.md) Also see the README files in `libraries` for more information on library functionality.

For reference, the default schematic and (general) layout for the build and its optional extras are attached:
![Weh](https://raw.githubusercontent.com/SeongGino/ir-light-gun-plus/plus/SamcoPlus%20Schematic.png)
 * *Clarification: Rumble power can go to either the pin marked `VHi` (board decides power delivery) or `USB` (directly powered from the USB interface).*

## Known Issues (want to fix sooner rather than later):
- Serial communication (Mamehook or debug output) can randomly crash while camera is being tracked. (Still) being actively investigated.
- Temperature sensor *should* work, but haven't tested yet; there be ~~[elf goddesses](https://www.youtube.com/watch?v=DSgw9RKpaKY)~~ dargons.

***NOTE:*** Solenoid *may or may not* cause EMI disconnects depending on the build, the input voltage, and the disarray of wiring in tight gun builds. **This is not caused by the sketch,** but something that theoretically applies to most custom gun builds (just happened to happen to me and didn't find many consistent search results involving this, so be forewarned!) ***Make sure you use thick enough wiring!*** I replaced my jumper cables with 18AWG wires, as well as reduced freely floating ground daisy chain clumps, and my build seems to hold up to sustained solenoid use now.

## TODO (can and will implement, just not now):
- Finish MAMEHooker support.
  * If someone could help provide the needed INIs compatible with GUN4IR so I could make this easier for the rest of the community, *please* get in touch/raise an issue!
  * So far we only support `S` (start, ignoring the bit), `E` (end), `M1x3` (offscreen mode - offscreen button mode toggle only), `F0` (solenoid feedback w/ auto), `F1` (rumble feedback w/ pulse), and `F2`/`F3`/`F4` (R/G/B LED color off/set, missing pulse support)
  * How long should a rumble or solenoid "pulse" actually be?
  * Why the FLIP does it still boardcrash randomly???
  * Open a wiki page with real notes about this, so no one else has to suffer like I did.
- Should implement support for rumble as an alternative force-feedback system (`RUMBLE_FF`); able to do so now, just have to do it.
- Code is still kind of a mess, so I should clean things up at some point maybe kinda.

## Wishlist (things I want to but don't know how/can't do yet):
- Console support? [It's definitely possible!](https://github.com/88hcsif/IR-Light-Gun)
- Document and implement separate RGB LED support?
  * We currently use only a board's builtin DotStar or NeoPixel, but this is only for distinguishing between profiles and indicating camera state for now. These are also used in serial handoff mode, but we could perhaps also make RGB LEDs react to events, i.e. trigger pulls.
  * Maybe consider using [FastLED](https://github.com/FastLED/FastLED) instead, to simplify LEDs use?
  * Would be easier to use external NeoPixels since we're running out of available pins, but generic four-pin RGB leds would be nice too.
- RP2040 has dual core support, currently handles input polling in parallel; any other boards that have dual cores?
  * ESP boards seem to have some support, but they're much more a pain in the butt to implement than the simple setup1()/loop1() the RP2040 needs.
- Could the tracking be improved more? We have a slight but still somewhat noticeable amount of cursor drift when aiming from extreme angles at screen corners on a 50" display with fisheye lenses.
- *Maybe* make an option for a true autofire that auto-reloads after a set amount of seconds or trigger pulls? Make the coordinates move to 0,0 and force a mouse unclick/click/unclick. Might be cheaty, but if someone wants it...
  * A lot of games have problems with this, though. Not sure if that's desirable with the implied caveats.

## Thanks:
* Samuel Ballantyne, for his SAMCO project which inspired my madness in the first place.
* Prow7, for improving an already promising project and providing the basis for this madness.
* Jaybee, for a frame of reference on implementing the feedback additions, and to hopefully act as an OS-agnostic competition for the (I assume) great GUN4IR.
* [My YouTube audience,](https://youtube.com/@ThatOneSeong) for their endless patience as I couldn't help but work on this instead of videos.
* Emm, for being there when I needed her.
* And Autism.

If you enjoy my work, or somehow found entertainment in this, [please support my endeavors on Ko-fi!](https://ko-fi.com/ThatOneSeong)

  *~<3*
  
  *That One Seong*
