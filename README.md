### Like our work? [Remember to support the developers!](https://github.com/TeamOpenFIRE/.github/blob/main/profile/README.md)

![BannerDark](of_bannerLoD.png#gh-dark-mode-only)![BannerLight](of_bannerDoL.png#gh-light-mode-only)
# OpenFIRE - The Open *Four Infa-Red Emitter* Light Gun System
###### Successor to [IR-GUN4ALL](http://github.com/SeongGino/ir-light-gun-plus), which is based on the [Prow Enhanced fork](https://github.com/Prow7/ir-light-gun), which in itself is based on the 4IR Beta "Big Code Update" [SAMCO project](https://github.com/samuelballantyne/IR-Light-Gun)

## Features:
- **Fully featured peripherals**, from Solenoid & Rumble Force Feedback, to TMP36 Temperature Monitoring, and others to come.
- **Multiple IR layouts support**, with *realtime perspective-adjusted tracking* for both double lightbar (recommended!) and Xwiigun-like diamond layouts (compatible with other systems).
- **Flexible Input System**, with outputs to Keyboard, 5-button ABS(olute Positioning) Mouse, and dual-stick gamepad w/ d-pad support, with a **robust button mapping system** to suit your needs!
- **Easy installation** with simple *.UF2* binaries that can be drag'n'dropped directly onto an *RP2040*-based Microcontroller.
- **Portable on-board settings** to store calibration profiles, toggles, settings, USB identifier, and more.
- **Integrates with the [OpenFIRE App](https://github.com/TeamOpenFIRE/OpenFIRE-App)** for user-friendly, and cross-platform on-the-fly configuration.
- **Optimized for the RP2040**, using its second core for input polling & queuing and serial handling, and the main core for camera and peripherals processing, whenever possible.
- **Compatible with PC Force Feedback handlers** such as [Mame Hooker](https://dragonking.arcadecontrols.com/static.php?page=aboutmamehooker), [The Hook Of The Reaper](https://github.com/6Bolt/Hook-Of-The-Reaper), and [QMamehook](https://github.com/SeongGino/QMamehook).
- **Supports integrated OLED display output** for *SSD1306 I2C displays* for menu navigation and visual feedback of game elements such as life and current ammo counts.
- **Compatible with the MiSTer FPGA ecosystem**, with dedicated mappings to streamline the user experience as much as possible.
- **Forever free and open source to the lightgun community!**

## Requirements
- An **RP2040** microcontroller for running the OpenFIRE firmware.
  * Recommended boards for new builds would be the [Raspberry Pi Pico](https://www.raspberrypi.com/products/raspberry-pi-pico/) *(cheapest, most pins available),* Adafruit [Kee Boar KB2040](https://www.adafruit.com/product/5302) *(cheaper, Pro Micro formfactor, compatible with other carrier boards),* or [ItsyBitsy RP2040](https://www.adafruit.com/product/4888) *(compatible with [SAMCO carrier boards](https://www.ebay.com/itm/184699412596))*
- **DFRobot IR Positioning Camera [SEN0158]:** [Mouser (US Distributor)](https://www.mouser.com/ProductDetail/DFRobot/SEN0158?qs=lqAf%2FiVYw9hCccCG%2BpzjbQ%3D%3D) | [DF-Robot (International)](https://www.dfrobot.com/product-1088.html) | [Mirrors list](https://octopart.com/sen0158-dfrobot-81833633)
- **4 IR LED emitters:** regular Wii sensor bars might work for small distances, but it's HIGHLY recommended to use [SFH 4547 LEDs](https://www.mouser.com/ProductDetail/720-SFH4547) w/ 5.6Ω *(ohm)* resistors. [DIY build tutorial here!*](https://www.youtube.com/watch?v=dNoWT8CaGRc)
   * Optional: **Any 12/24V solenoid,** w/ associated driver board. [DIY build tutorial here!*](https://www.youtube.com/watch?v=4uWgqc8g1PM) [Easy driver board here](https://oshpark.com/shared_projects/bjY4d7Vo)
     * *Requires a DC power extension cable &/or DC pigtail, and a separate adjustable 12-24V power supply.*
   * Optional: **Any 5V gamepad rumble motor,** w/ associated driver board. [DIY build tutorial here!*](https://www.youtube.com/watch?v=LiJ5rE-MeHw) [Easy driver board here](https://oshpark.com/shared_projects/VdsmUaSm)
   * Optional: **Any 2-way SPDT switches,** to adjust state of rumble/solenoid/rapid fire in hardware *(can be adjusted in software if not available!)*
   * Optional: **Any WS2812B GRB NeoPixels,** or any four-pin RGB LED for realtime lighting and reactions. [Amazon](https://www.amazon.com/BTF-LIGHTING-WS2812B-Heatsink-10mm3mm-WS2811/dp/B01DC0J0WS) | [AliExpress (International)](https://www.aliexpress.us/item/3256801340809756.html)
   * Optional: **SSD1306-based I2C (2wire/4pin) 128x64 OLED display** for visual UI and life/ammo counter feedback support. [AliExpress (International)](https://www.aliexpress.us/item/3256806186748120.html)
###### *Dedicated tutorials for the OpenFIRE system to come eventually.
 
***Refer to [BOARDS.md](BOARDS.md) for default pinouts for various boards** - though keep in mind that every board supports completely custom pin layouts, configurable through the OpenFIRE Desktop App. These layouts can also be viewed from the new Desktop App.*

## Installation:
Grab the latest *.UF2* binary for your respective board [from the releases page](https://github.com/TeamOpenFIRE/OpenFIRE-Firmware/releases/latest), and drag'n'drop the file to your microcontroller while booted into Bootloader mode; the RP2040 is automatically mounted like this when no program is loaded, but it can be forced into this mode by holding BOOTSEL while plugging it into the computer - it will appear as a removable storage device called **RPI-RP2**.

## Additional information
[Check out the enclosed instruction book!](OpenFIREmain/README.md) For developers, consult [the documentation on setting up and building the OpenFIRE Firmware.](COMPILING.md)

## Known Issues:
- With Pico W & Bluetooth enabled, TinyUSB/Serial fails to initialize properly when connected via USB, so the firmware is deadlocked either sending serial or USB report data.
- Updating NeoPixels settings from the OF App might cause some Pixels to be locked or shut off after saving - this is only temporary and will resolve itself after unplugging the board.
- Some rare instances where a serial exit signal is received while sending an escape key signal in the Simple Pause Menu with (non-static) Pixels in use might cause the gun to freeze. The exact cause is being investigated.

## TODO:
- Come up with better jokes.

> [!NOTE]
> Solenoid *may* cause EMI disconnects with too thin of wiring. Cables for this run specifically should be **24AWG** at its thinnest - or else the cables will become antennas under extended use, which will trip USB safety thresholds in your PC to protect the ports.

___
## Regarding Contributions, Ports & Forks:
Being an open source project, OpenFIRE **invites anyone to contribute** new code or fixes, assuming the following guidelines are followed:
 - To expedite review, PRs modifying existing code **must** describe the reasoning behind the changes (e.g. *"this resolves an issue regarding building on..."*). Default or *no* description text in the PR is likely to prompt for more details, or your request may be closed without merging.
 - Any work that modifies more than a few lines or files at once **should be performed on a separate branch** on your local repository, so that PRs don't go out-of-scope from simultaneous ongoing development.

OpenFIRE uses the Arduino format for the main source file `OpenFIREmain.ino` which should make it easier to develop for other architectures, however the `arduino-pico` core does use some of its own keywords/syntax (e.g. for establishing multi core workloads) so it won't directly run on other platforms without modification. While this repo is primarily focused on development for RP2040/RP235X ARM platforms, **we do encourage those with the know-how to port OpenFIRE to other architectures or microcontrollers.** Ideally, any ports should maintain compatibility with the existing Desktop App so the user doesn't need multiple executables. Those who are interested in maintaining these forks are encourage to write in a new thread in the *Discussions* tab of this repository.

**Forks made for the purposes of rebranding are heavily DISCOURAGED.** All forks should maintain the OpenFIRE name as well as credits to its original authors (specifically *That One Seong,* *Samuel Ballantyne (Samco),* and *Mike Lynch (Prow7)*).

Products that wish to integrate OpenFIRE by using its firmware on a supported microcontroller board - for example, inside of a prebuilt light gun - should make its usage of OpenFIRE clear, as well as give appropriate credit to its original creators. While it is Free (Libre) Software, we do ask that anyone incorporating our volunteer work into a product to please consider contributing back to the project via donations or sponsorships. Usage of OpenFIRE in commercial environments (such as public arcade settings) is also allowed, with the same note about contributing back applying as well. Any inquiries regarding third-party commercial sales of OpenFIRE-enabled products (such as prebuilt light guns using its Firmware) should be discussed in a thread in the *Discussions* tab of this repository, or emailing *That One Seong (SeongsSeongs@gmail.com)* and/or *Samuel Ballantyne (samuelballantyne@hotmail.com).*

As written under the *GNU Lesser General Public License,* any entities that wish to incorporate part of or all of OpenFIRE's Firmware components **must make their modifications available to the public upon request.** OpenFIRE is ***Free Software,*** both as in *Libre* and as in *Coffee:* if you have been charged any amount to use OpenFIRE, **you have been scammed and should demand your money back!**

___
## Thanks:
* Samuel Ballantyne, for his original SAMCO project, the gorgeous OpenFIRE branding, and perspective-based tracking system.
* Prow7, for his enhanced SAMCO fork which provided the basis of pause mode and saving subsystems.
* Odwalla-J, mrkylegp, RG2020 & lemmingDev for prerelease consultation, bug testing and feedback.
* The IR-GUN4ALL testers for their early feedback and feature requests - this wouldn't have happened without you lot!
* Chris Young for his TinyUSB compatible library (now part of `TinyUSB_Devices`).
