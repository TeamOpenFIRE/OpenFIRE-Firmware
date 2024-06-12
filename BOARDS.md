# Supported Boards Layouts:
 - [Raspberry Pi Pico (Non/W)](#raspberry-pi-pico-nonw)
 - [Adafruit ItsyBitsy RP2040](#adafruit-itsybitsy-rp2040)
 - [Adafruit Keeboar KB2040](#adafruit-keeboar-kb2040)
 - [Arduino Nano RP2040 Connect](#arduino-nano-rp2040-connect)
 - [Waveshare RP2040 Zero](#waveshare-rp2040-zero)
 - [VCC-GND YD RP2040 (Not Implemented)](#vcc-gnd-yd-rp2040)

```
                                        Symbol Legend:
                       (x) = GND/No Connect | (-) = GPIO | (p) = Power
```
## Raspberry Pi Pico (Non/W)
```

                                        (_____)
                     A Button     0  |-) *USB* (-| VBUS (USB 5V Out)
                     B Button     1  |-)       (-| VSYS (Input from Battery/Output to NeoPixels)
                                 GND |x)       (x| GND
                     C Button     2  |-)       (x| 3V3 En
                     Start        3  |-)       (p| 3V3 Out (to Display/Cam/Analog Inputs)
                     Select       4  |-)       (x| ADCVREF
                     Home Button  5  |-)       (-|  A2 *Unmapped*
                                 GND |x)       (x| AGND (for ADC VREF)
                     D-Pad Up     6  |-)       (-|  A1 *Unmapped*
                     D-Pad Down   7  |-)       (-|  A0 *Unmapped*
                     D-Pad Left   8  |-)       (x| RUN
                     D-Pad Right  9  |-)       (-|  22 *Unmapped*
                                 GND |x)       (x| GND
                     RGB Red     10  |-)       (-|  21 Camera SCL
                     RGB Green   11  |-)       (-|  20 Camera SDA
                     RGB Blue    12  |-)       (-|  19 *Unmapped*
                     Pump Action 13  |-)       (-|  18 *Unmapped*
                                 GND |x)       (x| GND
                     Pedal       14  |-)       (-|  17 Rumble Signal
                     Trigger     15  |-) _|_|_ (-|  16 Solenoid Signal

```
## Adafruit ItsyBitsy RP2040
```

                                        (_____)
                               RST |x)   *USB*   (p| BAT (Battery Input)
                               3V3 |p)           (x| GND
   (Display/Cam/Rumble/Analog) 3V3 |p)           (p| USB (5V Out)
                               VHi |p)           (-|  11 C Button
               B Button        A0  |-)           (-|  10 D-Pad Right
               A Button        A1  |-)           (-|  9  D-Pad Up
               Start           A2  |-)           (-|  8  D-Pad Left
               Select          A3  |-)           (-|  7  D-Pad Down
               Rumble Signal   24  |-)           (-|  6  Trigger
               Solenoid Signal 25  |-)           (x| !5  (Output Only)
               *Unmapped*      18  |-)           (-|  3  Camera SCL
               *Unmapped*      19  |-)           (-|  2  Camera SDA
               *Unmapped*      20  |-)           (-|  0  *Unmapped*
               *Unmapped*      12  |-) x|x|x|-|- (-|  1  *Unmapped*
                                             4 5
                                     4 - *Unmapped*
                                     5 - Pedal

```
## Adafruit Keeboar KB2040
```
                                        (_____)
                   (USB Data+)  D+ |x)   *USB*   (x| D-  (USB Data-)
               *Unmapped*       0  |-)           (p| RAW (5V, to NeoPixels)
               *Unmapped*       1  |-)           (x| GND
                               GND |x)           (x| RST
                               GND |x)           (p| 3V3 (Display/Cam/Rumble/Analog)
               Camera SDA       2  |-)           (-|  A3 A Button
               Camera SCL       3  |-)           (-|  A2 Trigger
               B Button         4  |-)           (-|  A1 Home Button
               Rumble Signal    5  |-)           (-|  A0 Temp Sensor
               Button C         6  |-)           (-|  18 D-Pad Up
               Solenoid Signal  7  |-)           (-|  20 D-Pad Down
               Select           8  |-)           (-|  19 D-Pad Left
               Start            9  |-)___________(-|  10 D-Pad Right
```
## Arduino Nano RP2040 Connect
##### *Note: A4/A5/A6/A7 are handled by the NiNa WiFi chip, but are not yet exposed to OpenFIRE.
```
                                         (_____)
                                    |     *USB*     |
                *Unmapped*       6  |-)           (-|  4  A Button
   (Display/Cam/Rumble/Analog)  3V3 |p)           (-|  7  B Button
                               AREF |x)           (-|  5  C Button
                *Unmapped*      A0  |-)           (-|  21 *Unmapped*
                *Unmapped*      A1  |-)           (-|  20 *Unmapped*
                *Unmapped*      A2  |-)           (-|  19 *Unmapped*
                *Unmapped*      A3  |-)           (-|  18 *Unmapped*
                Camera SDA      12  |x)           (-|  17 *Unmapped*
                Camera SCL      13  |x)           (-|  16 *Unmapped*
                *N/C*           A6  |x)           (-|  15 *Unmapped*
                *N/C*           A7  |x)           (-|  25 *Unmapped*
                 (to NeoPixels)  5V |p)           (x| GND
                *Unmapped*      REC |x)           (x| RST
                *Unmapped*      GND |x)           (-|  1  Pedal
                *Unmapped*      VIN |p)           (-|  0  Trigger
                                    |_______________|
```

## Waveshare RP2040 Zero
##### *Note: The underside GPIO pads *17-25* are not yet exposed to the OpenFIRE App.
```

                                         (_____)
                (to NeoPixels)  5V |p)    *USB*    (-|  0  Trigger
                               GND |x)             (-|  1  A Button
   (Display/Cam/Rumble/Analog) 3V3 |p)             (-|  2  B Button
               Temp Sensor     A3  |-)             (-|  3  C Button
               *Unmapped*      A2  |-)             (-|  4  Start
               Camera SCL      A1  |-)             (-|  5  Select
               Camera SDA      A0  |-)             (-|  6  *Unmapped*
               *Unmapped*      15  |-)             (-|  7  *Unmapped*
               *Unmapped*      14  |-)-| -| - |- |-(-|  8  *Unmapped*
                                     13 12 11  10 9
                                      9 - *Unmapped*
                                     10 - *Unmapped*
                                     11 - *Unmapped*
                                     12 - *Unmapped*
                                     13 - *Unmapped*

```
## VCC-GND YD RP2040
```

                          *no default layout yet, to be added*

```
