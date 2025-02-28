 /*!
 * @file OpenFIRElights.cpp
 * @brief Implementations of RGB lighting for the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>
#include "OpenFIRElights.h"
#include "SamcoPreferences.h"
#include "OpenFIREcommon.h"
#include "boards/OpenFIREshared.h"

#ifdef LED_ENABLE
void OF_RGB::LedInit()
{
    #ifdef ARDUINO_RASPBERRY_PI_PICO
        // this only needs to be set for rpipico, as Pico W's LED is tied to the WiFi chip,
        // and thus doesn't take pin direction statements.
        pinMode(PIN_LED, OUTPUT);
    #endif // ARDUINO_RASPBERRY_PI_PICO

    // init DotStar and/or NeoPixel to red during setup()
    // For the onboard NEOPIXEL, if any; it needs to be enabled.
    #ifdef NEOPIXEL_ENABLEPIN
        pinMode(NEOPIXEL_ENABLEPIN, OUTPUT);
        digitalWrite(NEOPIXEL_ENABLEPIN, HIGH);
    #endif // NEOPIXEL_ENABLEPIN
 
    #ifdef DOTSTAR_ENABLE
        dotstar.begin();
    #endif // DOTSTAR_ENABLE

    #ifdef NEOPIXEL_PIN
        neopixel.begin();
    #endif // NEOPIXEL_PIN
 
    /* Arduino Nano LED support disabled due to instability.
    #ifdef ARDUINO_NANO_RP2040_CONNECT
    pinMode(LEDR, OUTPUT);
    pinMode(LEDG, OUTPUT);
    pinMode(LEDB, OUTPUT);
    #endif // NANO_RP2040 */
    LedUpdate(255, 0, 0);
}

#ifdef CUSTOM_NEOPIXEL
void OF_RGB::InitExternPixel(const int8_t &pin)
{
    externPixel = new Adafruit_NeoPixel(SamcoPreferences::settings[OF_Const::customLEDcount], pin, NEO_GRB + NEO_KHZ800);
    externPixel->begin();
    if(SamcoPreferences::settings[OF_Const::customLEDstatic] > 0 &&
       SamcoPreferences::settings[OF_Const::customLEDstatic] <= SamcoPreferences::settings[OF_Const::customLEDcount]) {
        for(byte i = 0; i < SamcoPreferences::settings[OF_Const::customLEDstatic]; i++) {
            uint32_t color;
            switch(i) {
              case 0:
                color = SamcoPreferences::settings[OF_Const::customLEDcolor1];
                break;
              case 1:
                color = SamcoPreferences::settings[OF_Const::customLEDcolor2];
                break;
              case 2:
                color = SamcoPreferences::settings[OF_Const::customLEDcolor3];
                break;
            }
            externPixel->setPixelColor(i, color);
        }
        externPixel->show();
    }
}
#endif // CUSTOM_NEOPIXEL

// 32-bit packed color value update across all LED units
void OF_RGB::SetLedPackedColor(const uint32_t &color)
{
#ifdef DOTSTAR_ENABLE
    dotstar.setPixelColor(0, color);
    dotstar.show();
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    neopixel.setPixelColor(0, color);
    neopixel.show();
#endif // NEOPIXEL_PIN

#ifdef CUSTOM_NEOPIXEL
    if(externPixel != nullptr) {
        if(SamcoPreferences::settings[OF_Const::customLEDstatic] < SamcoPreferences::settings[OF_Const::customLEDcount]) {
            externPixel->fill(color, SamcoPreferences::settings[OF_Const::customLEDstatic]);
            externPixel->show();
        }
    }
#endif // CUSTOM_NEOPIXEL

    // separate r/g/b values for the following three pin output devices.
    byte r = highByte(color >> 8);
    byte g = highByte(color);
    byte b = lowByte(color);

#ifdef ARDUINO_RASPBERRY_PI_PICO
    // since Pico LED is a simple on/off, round down and average the total color.
    // TODO: Pico W will lock up when addressing its LED (WiFi module problems?)
    if(r < 100 && g < 100 && b < 100)
        digitalWrite(PIN_LED, LOW);
    else digitalWrite(PIN_LED, HIGH);
#endif // ARDUINO_RASPBERRY_PI_PICO/W

#ifdef FOURPIN_LED
    if(FW_Common::ledIsValid) {
        if(SamcoPreferences::toggles[OF_Const::commonAnode]) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        analogWrite(SamcoPreferences::pins[OF_Const::ledR], r);
        analogWrite(SamcoPreferences::pins[OF_Const::ledG], g);
        analogWrite(SamcoPreferences::pins[OF_Const::ledB], b);
    }
#endif // FOURPIN_LED

/* Arduino Nano LED support disabled due to instability.
#ifdef ARDUINO_NANO_RP2040_CONNECT
    // in case the color bytes were already flipped before, as Arduino Nano also uses power sink pins i.e. common anode
    if(FW_Common::ledIsValid && !SamcoPreferences::toggles.commonAnode) {
        r = ~r;
        g = ~g;
        b = ~b;
    }
    analogWrite(LEDR, r);
    analogWrite(LEDG, g);
    analogWrite(LEDB, b);
#endif // NANO_RP2040 */
}

void OF_RGB::LedOff()
{
    LedUpdate(0, 0, 0);
}

// Generic R/G/B value update across all LED units
void OF_RGB::LedUpdate(const byte &r, const byte &g, const byte &b)
{
    #ifdef DOTSTAR_ENABLE
        dotstar.setPixelColor(0, r, g, b);
        dotstar.show();
    #endif // DOTSTAR_ENABLE
    #ifdef NEOPIXEL_PIN
        neopixel.setPixelColor(0, r, g, b);
        neopixel.show();
    #endif // NEOPIXEL_PIN

    #ifdef CUSTOM_NEOPIXEL
        if(externPixel != nullptr) {
            if(SamcoPreferences::settings[OF_Const::customLEDstatic] < SamcoPreferences::settings[OF_Const::customLEDcount]) {
                externPixel->fill(Adafruit_NeoPixel::Color(r, g, b), SamcoPreferences::settings[OF_Const::customLEDstatic]);
                externPixel->show();
            }
        }
    #endif // CUSTOM_NEOPIXEL

    #ifdef ARDUINO_RASPBERRY_PI_PICO
    // TODO: Pico W will lock up when addressing its LED (WiFi module problems?)
        if(r < 100 && g < 100 && b < 100) digitalWrite(PIN_LED, LOW);
        else digitalWrite(PIN_LED, HIGH);
    #endif // ARDUINO_RASPBERRY_PI_PICO

    #ifdef FOURPIN_LED
        if(FW_Common::ledIsValid) {
            if(SamcoPreferences::toggles[OF_Const::commonAnode]) {
                analogWrite(SamcoPreferences::pins[OF_Const::ledR], ~r);
                analogWrite(SamcoPreferences::pins[OF_Const::ledG], ~g);
                analogWrite(SamcoPreferences::pins[OF_Const::ledB], ~b);
            } else {
                analogWrite(SamcoPreferences::pins[OF_Const::ledR], r);
                analogWrite(SamcoPreferences::pins[OF_Const::ledG], g);
                analogWrite(SamcoPreferences::pins[OF_Const::ledB], b);
            }
        }
    #endif // FOURPIN_LED

    /* Arduino Nano LED support disabled due to instability.
    #ifdef ARDUINO_NANO_RP2040_CONNECT
        #ifdef FOURPIN_LED
        // Nano's builtin is a common anode, so we use that logic by default if it's enabled on the external 4-pin;
        // otherwise, invert the values.
        if((FW_Common::ledIsValid && !SamcoPreferences::toggles.commonAnode) || !FW_Common::ledIsValid) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        #else
            r = ~r;
            g = ~g;
            b = ~b;
        #endif // FOURPIN_LED
        analogWrite(LEDR, r);
        analogWrite(LEDG, g);
        analogWrite(LEDB, b);
    #endif // NANO_RP2040 */
}
#endif // LED_ENABLE