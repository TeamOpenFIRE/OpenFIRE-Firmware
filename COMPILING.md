> [!NOTE]
> If you discover issues with custom builds or are not using the provided binaries in the releases page, **make sure you inform in the issue of what you modified in the code.** If it's a general firmware issue, see if it happens in the precompiled builds first.

## OpenFIRE Build Manual
 - [Arduino IDE Setup](#arduino--ide-setup)
 - [Arduino (-cli) Setup](#arduino--cli-setup)
 - [Sketch Configuration](#sketch-configuration)
 - [Define Buttons & Timers](#define-buttons--timers)

### Arduino IDE Setup
For most people, you may prefer editing, testing and using the Arduino IDE. This applies to Arduino IDE 2.x, though 1.x can also be made to work.
 1. [Download and install the Arduino IDE for your system]() (or for Linux users, install from your system's package manager).
 2. After opening the IDE, go to File->Preferences and paste the following into the *Additional boards manager URLs* field:
 ```
 https://github.com/SeongGino/arduino-pico/releases/download/3.9.2-fix/package_rp2040_fix_index_orig.json
 ```
 3. Open the Boards Manager (second button from the top on the left sidebar), search and install "Raspberry Pi Pico/RP2040 (Fix)" version `3.9.2-fix`.
    - Using this fork is REQUIRED as it includes a fixed version of Adafruit's TinyUSB library. Until adafruit/Adafruit_TinyUSB_Arduino#293 is resolved, do not use the upstream *Arduino-Pico* core or install the separate Adafruit TinyUSB library.
 4. Clone/extract the contents of the source repository into your system's `Arduino` folder, so that the `SamcoEnhanced` folder is next to `libraries`. Open the `SamcoEnhanced` sketch.
 5. From the app menu up top, set the current microcontroller to your current board under *Tools->Board:->Raspberry Pi Pico/RP2040 (Fix),* set CPU speed to *133 MHz,* set Optimize to *Optimize Even More (-O3),* and set USB Stack to *Adafruit TinyUSB* (not *Host*).
 6. When you're ready to build, click Verify to check for compilation errors, Upload to directly upload the binary to the microcontroller (if connected), or go to *Sketch->Export Compiled Binary* to generate a .uf2 file under `SamcoEnhanced/build/rp2040.rp2040.board_name` that you can drag and drop onto your microcontroller when it's in bootloader mode (either by holding BOOTSEL on poweron, or resetting to bootloader from the OpenFIRE App).

### Arduino (-cli) Setup
Compiling from the cli can be used to automate the build process, and is used by the GitHub actions deployments for release builds.
 1. [Download the Arduino-cli tool for your system](https://github.com/arduino/arduino-cli/releases/latest) and install it to where it's most convenient (or for Linux users, install from your system's package manager).
    - These instructions are tailored to Linux, but Windows users can use `arduino-cli.exe` whenever the tool is referenced.
 3. Install the patched RP2040 core for OpenFIRE:
    ```bash
    $ arduino-cli core install rp2040:rp2040 --additional-urls https://github.com/SeongGino/arduino-pico/releases/download/3.9.2-fix/package_rp2040_fix_index_orig.json
    ```
    - Optional: for *Arduino Nano RP2040 Connect*, also install its WiFiNINA library:
      ```bash
      $ arduino-cli lib install WiFiNINA
      ```
 4. Clone the repository, making sure to also download its submodules:
    ```bash
    $ git clone --recursive https://github.com/TeamOpenFIRE/OpenFIRE-Firmware
    ```
 5. Find the proto name of the board to build for:
    ```bash
    $ arduino-cli board listall rp2040
    
    Board Name                           FQBN
    0xCB Helios                          rp2040:rp2040:0xcb_helios
    Adafruit Feather RP2040              rp2040:rp2040:adafruit_feather
    Adafruit Feather RP2040 CAN          rp2040:rp2040:adafruit_feather_can
    Adafruit Feather RP2040 DVI          rp2040:rp2040:adafruit_feather_dvi
    Adafruit Feather RP2040 Prop-Maker   rp2040:rp2040:adafruit_feather_prop_maker
    Adafruit Feather RP2040 RFM          rp2040:rp2040:adafruit_feather_rfm
    ...
    ```
 6. Build OpenFIRE Firmware (replacing `{BOARD}` with your desired microcontroller's fqbn name:
    ```bash
    $ arduino-cli compile -e --fqbn rp2040:rp2040:{BOARD}:usbstack=tinyusb,opt=Optimize3 /path/to/OpenFIRE-Firmware/SamcoEnhanced --libraries /path/to/repo/libraries
    ```
    *for custom builds, feel free to configure the above build flags to your needs to enable/disable certain OpenFIREfw features.*
    
When successful, you will find the exported binary at `/path/to/OpenFIRE-Firmware/SamcoEnhanced/build/rp2040.rp2040.{BOARD}/SamcoEnhanced.ino.uf2`

### Sketch Configuration
Per-board build configurations for various microcontrollers are located in `SamcoPreferences.cpp - SamcoPreferences::LoadPresets()`, and the board report strings to identify the board in the OpenFIRE App can be found in `boards/OpenFIREshared.h`

### Define Buttons & Timers
Tactile extras can be defined/unset by simply (un)commenting the respective defines in `OpenFIREDefines.h` - though each one of these can be simply disabled at runtime even when the firmware is "fully kitted".

If your gun is going to be hardset to player 1/2/3/4 e.g. for an arcade build, change `#define PLAYER_NUMBER` to 1, 2, 3, or 4 depending on what keys you want the Start/Select buttons to correlate to. Remember that guns can be remapped to any player number arrangement at any time if needed by sending an `XR#` command over Serial - where # is the player number.

These parameters are easily found and can be redefined in `OpenFIREDefines.h`:

```c++
#define MANUFACTURER_NAME "OpenFIRE"
#define DEVICE_NAME "FIRECon"
#define DEVICE_VID 0xF143
```

You may change these to suit whatever your heart desires - though the only parts *necessary to change for multiplayer* is the Device Vendor ID and/or Product ID (the latter is determined by either loaded preferences or Player Number, in that order). Then, just reflash the board!

Remember that the sketch uses the Arduino GPIO pin numbers; on many boards, including the Raspberry Pi Pico and the Adafruit Itsybitsy RP2040, these are the silkscreen labels on the **underside** of the microcontroller (marked GP00-29). Note that this does not apply to the analog pins (A0-A3), which are macros for GP26-29.
Refer to [this interactive webpage](https://pico.pinout.xyz/) for detailed information on the Pico/W's layout, or your board vendor's documentation

The default button:pins layout used will be reflected by default in the OpenFIRE App, which can be used as reference or can be changed to any custom pins layout to suit your needs - custom settings will take priority over board defaults if enabled & detected.
