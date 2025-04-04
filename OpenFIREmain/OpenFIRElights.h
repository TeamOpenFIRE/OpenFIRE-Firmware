 /*!
 * @file OpenFIRElights.h
 * @brief Implementations of RGB lighting for the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#ifndef _OPENFIRELIGHTS_H_
#define _OPENFIRELIGHTS_H_

#include <OpenFIREBoard.h>

#include "OpenFIREDefines.h"
#include "OpenFIREcolors.h"

#ifdef DOTSTAR_ENABLE
    #define LED_ENABLE
    #include <Adafruit_DotStar.h>
#endif // DOTSTAR_ENABLE
#ifdef NEOPIXEL_PIN
    #define LED_ENABLE
#endif
#ifdef LED_ENABLE
    #include <Adafruit_NeoPixel.h>
#endif // LED_ENABLE
/* TODO: Arduino Nano LED support disabled due to instability.
#ifdef ARDUINO_NANO_RP2040_CONNECT
    #define LED_ENABLE
    // Apparently an LED is included, but has to be communicated with through the WiFi chip (or else it throws compiler errors)
    // That said, LEDs are attached to Pins 25(G), 26(B), 27(R).
    #include <WiFiNINA.h>
#endif // ARDUINO_NANO */

class OF_RGB
{
public:
    // initializes system and 4pin RGB LEDs.
    // ONLY to be used from setup()
    static void LedInit();

    #ifdef CUSTOM_NEOPIXEL
    static void InitExternPixel(const int &);
    #endif // CUSTOM_NEOPIXEL

    static void SetLedPackedColor(const uint32_t &);

    static void LedOff();

    static void LedUpdate(const uint8_t &, const uint8_t &, const uint8_t &);

    static void SetLedColorFromMode();

    static uint8_t Invert(const uint8_t &orig) { return ~orig; }

    #ifdef CUSTOM_NEOPIXEL
    static inline Adafruit_NeoPixel* externPixel = nullptr;
    #endif // CUSTOM_NEOPIXEL

    // colour when no IR points are seen
    static inline uint32_t IRSeen0Color = WikiColor::Amber;

    // colour when calibrating
    static inline uint32_t CalModeColor = WikiColor::Red;

private:
    // internal addressable LEDs inits
    #ifdef DOTSTAR_ENABLE
    // note if the colours don't match then change the colour format from BGR
    // apparently different lots of DotStars may have different colour ordering ¯\_(ツ)_/¯
    static inline Adafruit_DotStar dotstar = Adafruit_DotStar(1, DOTSTAR_DATAPIN, DOTSTAR_CLOCKPIN, DOTSTAR_BGR);
    #endif // DOTSTAR_ENABLE

    #ifdef NEOPIXEL_PIN
    static inline Adafruit_NeoPixel neopixel = Adafruit_NeoPixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    #endif // CUSTOM_NEOPIXEL
};

#endif // _OPENFIRELIGHTS_H_