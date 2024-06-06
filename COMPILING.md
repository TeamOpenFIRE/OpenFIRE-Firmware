> [!NOTE]
> If you discover issues with custom builds or are not using the provided binaries in the releases page, **make sure you inform in the issue of what you modified in the sketch.** If it's a general firmware issue, see if it happens in the precompiled builds first.

## OpenFIRE Build Manual
 - [Arduino Setup & Libraries](#arduino-setup--libraries)
 - [Sketch Configuration](#sketch-configuration)
 - [Define Buttons & Timers](#define-buttons--timers)

### Arduino Setup & Libraries
If you're using Arduino IDE for the first time, the setup is relatively simple:
 1. [Install the Arduino IDE for your system](https://www.arduino.cc/en/software) (or for Linux users, from your system's package manager)
    * *Windows users, install the USB drivers when prompted.*
 2. Once installed, open Arduino, and from the top bar, click on __*File -> Preferences.*__
 3. In the Preferences window, the *Additional Boards Manager URLs* path should be empty. Copy and paste this string:
    `https://adafruit.github.io/arduino-board-index/package_adafruit_index.json,https://github.com/SeongGino/arduino-pico/releases/download/3.6.1-fix/package_rp2040_fix_index.json`
    and paste it in there. Click OK to confirm.
 4. Back in the main window, go to __*Tools -> Board: {some board name} -> Boards Manager*__ (IDE 1.8.x) || select __*Board Manager*__ from the sidebar (IDE 2.x) and install the latest version of `Raspberry Pi Pico/RP2040 (Fix)`
> [!WARNING]
> If you're building for RP2040 (and thus are using the provided "fixed" core above), ***remove the `Adafruit TinyUSB Library`*** if it's installed as a dependency! Having the TinyUSB library will override the fixed version provided in the core, which will cause board freezes when serial communication is happening!
 5. Under __*Tools*__, make sure to select your board of choice, and set the compiler optimize level to `Faster (-O3)`/`Optimize Even More (-O3)` and set the USB stack to `(Adafruit) TinyUSB`.
    * *Linux users should have their user in the `uucp` group. If not, add oneself to it (`sudo usermod -a -G uucp username`), then relogin to see the board's serial port.*
   
Extract the `SamcoEnhanced` and `libraries` folders from the source repository/releases into your Arduino sketches folder. Defaults are:
* Windows: `Documents\Arduino`
* Linux: `~/Arduino`

Now just go to File -> Open, and load `SamcoEnhanced.ino`.

### Sketch Configuration
Per-board build configurations for various microcontrollers are located in `SamcoPreferences.cpp - SamcoPreferences::LoadPresets()`, and the board report strings to identify the board in the OpenFIRE App can be found in `libraries/OpenFIREPosition/OpenFIREBoard.h`

### Define Buttons & Timers
Tactile extras can be defined/unset by simply (un)commenting the respective defines - though each one of these can be simply disabled at runtime even when the firmware is "fully kitted".

If your gun is going to be hardset to player 1/2/3/4 e.g. for an arcade build, change `#define PLAYER_NUMBER` to 1, 2, 3, or 4 depending on what keys you want the Start/Select buttons to correlate to. Remember that guns can be remapped to any player number arrangement at any time if needed by sending an `XR#` command over Serial - where # is the player number.

These parameters are easily found and can be redefined in the sketch configuration area - just above the `PLAYER_NUMBER` indicator!

```
#define MANUFACTURER_NAME "OpenFIRE"
#define DEVICE_NAME "FIRECon"
#define DEVICE_VID 0xF143
#define DEVICE_PID 0x1998
```

You may change these to suit whatever your heart desires - though the only parts *necessary to change for multiplayer* are the `DEVICE_VID` and `DEVICE_PID` pairs. Then, just reflash the board!

Remember that the sketch uses the Arduino GPIO pin numbers; on many boards, including the Raspberry Pi Pico and the Adafruit Itsybitsy RP2040, these are the silkscreen labels on the **underside** of the microcontroller (marked GP00-29). Note that this does not apply to the analog pins (A0-A3), which are macros for GP26-29.
Refer to [this interactive webpage](https://pico.pinout.xyz/) for detailed information on the Pico/W's layout, or your board vendor's documentation

The default button:pins layout used will be reflected by default in the OpenFIRE App, which can be used as reference or can be changed to any custom pins layout to suit your needs - custom settings will take priority over board defaults if enabled & detected.

Once the sketch is configured to your liking, plug the board into a USB port, click Upload (the arrow button next to the checkmark) and your board will reconnect a few times until it's recognized as a combined mouse/keyboard/gamepad device!
