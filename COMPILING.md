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
 https://github.com/TeamOpenFIRE/arduino-pico/releases/download/global/package_rp2040_index.json
 ```
 3. Open the Boards Manager (second button from the top on the left sidebar), search and install the latest version of *"Raspberry Pi Pico/RP2040/RP2350 (TUSB Fix)"*.
    - Using this fork is REQUIRED as it includes a fixed version of Adafruit's TinyUSB library. Until adafruit/Adafruit_TinyUSB_Arduino#293 is resolved, do not use the upstream *Arduino-Pico* core or install the separate Adafruit TinyUSB library.
 4. Clone/extract the contents of the source repository into your system's `Arduino` folder, so that the `OpenFIREmain` folder is next to `libraries`. Open the `OpenFIREmain` sketch.
 5. From the app menu up top, set the current microcontroller to your current board under *Tools->Board:->Raspberry Pi Pico/RP2040/RP2350 (TUSB Fix),* set CPU speed to *133 MHz,* set Optimize to *Optimize Even More (-O3),* set Flash Size to include at least a 64KB file system,* and set USB Stack to *Adafruit TinyUSB* (not *Host*). For RP2350 boards, *CPU Architecture* must be set to *ARM*.
 6. When you're ready to build, click Verify to check for compilation errors, Upload to directly upload the binary to the microcontroller (if connected), or go to *Sketch->Export Compiled Binary* to generate a .uf2 file under `OpenFIREmain/build/rp2040.rp2040.board_name` that you can drag and drop onto your microcontroller when it's in bootloader mode (either by holding BOOTSEL on poweron, or resetting to bootloader from the OpenFIRE App).

### Arduino (-cli) Setup
Compiling from the cli can be used to automate the build process, and is used by the GitHub actions deployments for release builds.
 1. [Download the Arduino-cli tool for your system](https://github.com/arduino/arduino-cli/releases/latest) and install it to where it's most convenient (or for Linux users, install from your system's package manager).
    - These instructions are tailored to Linux, but Windows users can use `arduino-cli.exe` whenever the tool is referenced.
 2. Install the latest version of the patched RP2040/RP2350 core for OpenFIRE:
    ```bash
    $ arduino-cli core install rp2040:rp2040 --additional-urls https://github.com/TeamOpenFIRE/arduino-pico/releases/download/global/package_rp2040_index.json
    ```
    - Optional: for *Arduino Nano RP2040 Connect*, also install its WiFiNINA library:
      ```bash
      $ arduino-cli lib install WiFiNINA
      ```
 3. Clone the repository, making sure to also download its submodules:
    ```bash
    $ git clone --recursive https://github.com/TeamOpenFIRE/OpenFIRE-Firmware
    ```
 4. Find the proto name of the board to build for:
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
 5. Find the flash value that allocates at least 64KB for the File System (replacing `{BOARD}` with your desired microcontroller's FQBN name). The example output below would be seen if `{BOARD}` was set to `rp2040:rp2040:rpipico`:
    ```bash
    $ arduino-cli board details -b {BOARD}

    ...
    Option:       Flash Size                                       flash
                  2MB (no FS)                                      flash=2097152_0
                  2MB (Sketch: 1984KB, FS: 64KB)                   flash=2097152_65536
                  2MB (Sketch: 1920KB, FS: 128KB)                  flash=2097152_131072
                  2MB (Sketch: 1792KB, FS: 256KB)                  flash=2097152_262144
                  2MB (Sketch: 1536KB, FS: 512KB)                  flash=2097152_524288
                  2MB (Sketch: 1MB, FS: 1MB)                       flash=2097152_1048576
    ...
    ```
 6. Build OpenFIRE Firmware (replacing `{BOARD}` with your desired microcontroller's FBQN name and `{FLASH}` with the flash value found above):
    ```bash
    $ arduino-cli compile -e --fqbn rp2040:rp2040:{BOARD}:usbstack=tinyusb,opt=Optimize3,flash={FLASH} /path/to/OpenFIRE-Firmware/OpenFIREmain --libraries /path/to/repo/libraries
    ```
    
When successful, you will find the exported binary at `/path/to/OpenFIRE-Firmware/OpenFIREmain/build/rp2040.rp2040.{BOARD}/OpenFIREmain.ino.uf2`

### Sketch Configuration
Per-board build configurations for various microcontrollers, and the strings to identify which board is for what, can be found in `boards/OpenFIREshared.h`

### Define Buttons & Timers
Tactile extras can be defined/unset by simply (un)commenting the respective defines in `OpenFIREDefines.h` - though each one of these can be simply disabled at runtime even when the firmware is "fully kitted".

If your gun is going to be hardset to player 1/2/3/4 e.g. for an arcade build, uncomment and set `#define PLAYER_NUMBER` to 1, 2, 3, or 4 depending on what keys you want the Start/Select buttons to correlate to. Remember that, when this variable is unset, guns can be remapped to any player number arrangement at any time if needed by sending an `XR#` command over Serial - where # is the player number.

To change the default USB ID, these parameters are easily found and can be redefined in `OpenFIREDefines.h`:

```c++
#define MANUFACTURER_NAME "OpenFIRE"
#define DEVICE_NAME "FIRECon"
#define DEVICE_VID 0xF143
```

You may change these to suit whatever your heart desires - though the only parts *necessary to change for multiplayer* is the Device Vendor ID and/or Product ID (the latter is determined by either loaded preferences or Player Number, in that order). Then, just reflash the board!
Keep in mind that for App and most distros' compatibility, the Vendor ID (`DEVICE_VID`) **MUST** be kept at the default `0xF143` identifier.

Remember that the sketch uses the Arduino GPIO pin numbers; on many boards, including the Raspberry Pi Pico and the Adafruit Itsybitsy RP2040, these are the silkscreen labels on the **underside** of the microcontroller (marked GP00-29). Note that this does not apply to the analog pins (A0-A3), which are macros for GP26-29.
For boards already implemented, the OpenFIRE Desktop App has an embedded interactive *boards previewer* to view the default layout, location, and extra capabilities of GPIO for supported microcontrollers. You can also refer to [this interactive webpage](https://pico.pinout.xyz/) for detailed information on the Pico/W's layout, or your board vendor's documentation for more information about your particular microcontroller.

The default button:pins layout used will be reflected by default in the OpenFIRE App, which can be used as reference or can be changed to any custom pins layout to suit your needs - custom settings will take priority over board defaults if enabled & detected.
