 /*!
 * @file OpenFIRElights.cpp
 * @brief Implementations of RGB lighting for the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>

#include "OpenFIRElights.h"
#include "OpenFIREprefs.h"
#include "OpenFIREcommon.h"
#include "OpenFIREserial.h"
#include "boards/OpenFIREshared.h"

OF_RGB::NeoPixelEffect OF_RGB::currentEffect = OF_RGB::EFFECT_NONE;
char OF_RGB::effectColorChar = 'R';
uint8_t OF_RGB::fire_heat[150];

//  Knight Rider effect
int OF_RGB::riderPosition = 0;
bool OF_RGB::riderDirection = true;
unsigned long OF_RGB::lastRiderUpdate = 0;
char OF_RGB::knightRiderColor = 'R';

// Utilities for effects
// Sum with saturation for 8 bits (prevents overflow)
uint8_t qadd8(uint8_t i, uint8_t j) {
    unsigned int t = i + j;
    if (t > 255) t = 255;
    return t;
}

// Subtract with saturation for 8 bits (avoids negative values)
uint8_t qsub8(uint8_t i, uint8_t j) {
    int t = i - j;
    if (t < 0) t = 0;
    return t;
}

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
void OF_RGB::InitExternPixel(const int &pin)
{
    externPixel = new Adafruit_NeoPixel(OF_Prefs::settings[OF_Const::customLEDcount], pin, NEO_GRB + NEO_KHZ800);
    externPixel->begin();
    if(OF_Prefs::settings[OF_Const::customLEDstatic] > 0 &&
       OF_Prefs::settings[OF_Const::customLEDstatic] <= OF_Prefs::settings[OF_Const::customLEDcount]) {
        for(uint i = 0; i < OF_Prefs::settings[OF_Const::customLEDstatic]; ++i) {
            uint32_t color;
            switch(i) {
              case 0:
                color = OF_Prefs::settings[OF_Const::customLEDcolor1];
                break;
              case 1:
                color = OF_Prefs::settings[OF_Const::customLEDcolor2];
                break;
              case 2:
                color = OF_Prefs::settings[OF_Const::customLEDcolor3];
                break;
            }

            if(OF_Prefs::toggles[OF_Const::invertStaticPixels])
                 externPixel->setPixelColor(OF_Prefs::settings[OF_Const::customLEDcount]-1 - i, color);
            else externPixel->setPixelColor(i, color);
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
        if(OF_Prefs::settings[OF_Const::customLEDstatic] < OF_Prefs::settings[OF_Const::customLEDcount]) {
            if(OF_Prefs::toggles[OF_Const::invertStaticPixels])
                 externPixel->fill(color, 0, OF_Prefs::settings[OF_Const::customLEDcount] - OF_Prefs::settings[OF_Const::customLEDstatic]);
            else externPixel->fill(color, OF_Prefs::settings[OF_Const::customLEDstatic]);
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
        if(OF_Prefs::toggles[OF_Const::commonAnode]) {
            r = ~r;
            g = ~g;
            b = ~b;
        }
        analogWrite(OF_Prefs::pins[OF_Const::ledR], r);
        analogWrite(OF_Prefs::pins[OF_Const::ledG], g);
        analogWrite(OF_Prefs::pins[OF_Const::ledB], b);
    }
#endif // FOURPIN_LED

/* Arduino Nano LED support disabled due to instability.
#ifdef ARDUINO_NANO_RP2040_CONNECT
    // in case the color bytes were already flipped before, as Arduino Nano also uses power sink pins i.e. common anode
    if(FW_Common::ledIsValid && !OF_Prefs::toggles.commonAnode) {
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
void OF_RGB::LedUpdate(const uint8_t &r, const uint8_t &g, const uint8_t &b)
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
            if(OF_Prefs::settings[OF_Const::customLEDstatic] < OF_Prefs::settings[OF_Const::customLEDcount]) {
                if(OF_Prefs::toggles[OF_Const::invertStaticPixels])
                     externPixel->fill(Adafruit_NeoPixel::Color(r, g, b), 0, OF_Prefs::settings[OF_Const::customLEDcount] - OF_Prefs::settings[OF_Const::customLEDstatic]);
                else externPixel->fill(Adafruit_NeoPixel::Color(r, g, b), OF_Prefs::settings[OF_Const::customLEDstatic]);
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
            if(OF_Prefs::toggles[OF_Const::commonAnode]) {
                analogWrite(OF_Prefs::pins[OF_Const::ledR], Invert(r));
                analogWrite(OF_Prefs::pins[OF_Const::ledG], Invert(g));
                analogWrite(OF_Prefs::pins[OF_Const::ledB], Invert(b));
            } else {
                analogWrite(OF_Prefs::pins[OF_Const::ledR], r);
                analogWrite(OF_Prefs::pins[OF_Const::ledG], g);
                analogWrite(OF_Prefs::pins[OF_Const::ledB], b);
            }
        }
    #endif // FOURPIN_LED

    /* Arduino Nano LED support disabled due to instability.
    #ifdef ARDUINO_NANO_RP2040_CONNECT
        #ifdef FOURPIN_LED
        // Nano's builtin is a common anode, so we use that logic by default if it's enabled on the external 4-pin;
        // otherwise, invert the values.
        if((FW_Common::ledIsValid && !OF_Prefs::toggles.commonAnode) || !FW_Common::ledIsValid) {
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
void OF_RGB::updateNeoPixelBar(uint16_t currentValue, uint16_t maxValue, uint16_t startLed, uint16_t ledCount, uint32_t colorFull, uint32_t colorEmpty) {
    if (externPixel == nullptr || ledCount == 0) return;
    if (maxValue == 0) maxValue = 1;

    uint16_t ledsToShow = map(currentValue, 0, maxValue, 0, ledCount);

    uint8_t r1 = (colorFull >> 16) & 0xFF, g1 = (colorFull >> 8) & 0xFF, b1 = colorFull & 0xFF;
    uint8_t r2 = (colorEmpty >> 16) & 0xFF, g2 = (colorEmpty >> 8) & 0xFF, b2 = colorEmpty & 0xFF;

    for (uint16_t i = 0; i < ledCount; i++) {
        uint16_t physicalLed = i + startLed;
        if (i < ledsToShow) {
            float ratio = (ledCount > 1) ? ((float)i / (float)(ledCount - 1)) : 0.0f;
            uint8_t r = r2 + ratio * (r1 - r2);
            uint8_t g = g2 + ratio * (g1 - g2);
            uint8_t b = b2 + ratio * (b1 - b2);
            externPixel->setPixelColor(physicalLed, externPixel->Color(r, g, b));
        } else {
            externPixel->setPixelColor(physicalLed, 0);
        }
    }
    externPixel->show();
}

void OF_RGB::setEffect(NeoPixelEffect effect, char color) {
    currentEffect = effect;
    effectColorChar = toupper(color);

    if (effect == EFFECT_KNIGHT_RIDER) {
        riderPosition = 0;
        riderDirection = true;
    }

    if (effect == EFFECT_NONE) {
        if (externPixel != nullptr) {
            uint16_t startLed = OF_Prefs::settings[OF_Const::effectsStartLed];
            uint16_t ledCount = OF_Prefs::settings[OF_Const::effectsLedCount];
            for (int i = startLed; i < startLed + ledCount; i++) {
                externPixel->setPixelColor(i, 0);
            }
            externPixel->show();
        }
    }
}

//LED effects
void OF_RGB::updateEffects() {
    if (externPixel == nullptr || currentEffect == EFFECT_NONE) return;

    switch (currentEffect) {
        case EFFECT_FIRE:
            fireEffect();
            break;
        case EFFECT_ICE:
            iceEffect();
            break;
        case EFFECT_PLASMA:
            plasmaEffect();
            break;
        case EFFECT_BEAM:
            beamEffect();
            break;
        case EFFECT_KNIGHT_RIDER:
            knightRiderEffect();
            break;
        default:
            break;
    }
}

uint32_t OF_RGB::getColorFromChar(char colorChar) {
    switch(colorChar) {
        case 'R': return externPixel->Color(255, 0, 0);   // Rojo
        case 'G': return externPixel->Color(0, 255, 0);   // Verde
        case 'B': return externPixel->Color(0, 0, 255);   // Azul
        case 'O': return externPixel->Color(255, 165, 0); // Naranja
        case 'P': return externPixel->Color(128, 0, 128); // Púrpura
        case 'Y': return externPixel->Color(255, 255, 0); // Amarillo
        case 'C': return externPixel->Color(0, 255, 255); // Cian
        case 'M': return externPixel->Color(255, 0, 255); // Magenta
        case 'W': return externPixel->Color(255, 255, 255); // Blanco
        case 'L': return externPixel->Color(180, 255, 0); // Lima
        default:  return externPixel->Color(255, 0, 0);   // Rojo por defecto
    }
}

void OF_RGB::fireEffect() {
    uint16_t startLed = OF_Prefs::settings[OF_Const::effectsStartLed];
    uint16_t numLeds = OF_Prefs::settings[OF_Const::effectsLedCount];
    if (numLeds == 0) return;

    int Cooling = 55;
    int Sparks = 120;

    for (int i = 0; i < numLeds; i++) {
        fire_heat[i] = qsub8(fire_heat[i], random(0, ((Cooling * 10) / numLeds) + 2));
    }
    for (int k = (numLeds - 1); k >= 2; k--) {
        fire_heat[k] = (fire_heat[k - 1] + fire_heat[k - 2] + fire_heat[k - 2]) / 3;
    }
    if (random(255) < Sparks) {
        int y = random(7);
        fire_heat[y] = qadd8(fire_heat[y], random(160, 255));
    }

    for (int j = 0; j < numLeds; j++) {
        byte temperature = fire_heat[j];
        byte t192 = round((temperature / 255.0) * 191);
        byte heatramp = t192 & 0x3F;
        heatramp <<= 2;
        uint8_t r, g, b;

        // --- SWITCH AMPLIADO PARA TODOS LOS COLORES ---
        switch(effectColorChar) {
            case 'G': // Verde
                if (t192 > 0x80) { r = heatramp; g = 255; b = heatramp; }
                else if (t192 > 0x40) { r = 0; g = 255; b = heatramp / 2; }
                else { r = 0; g = heatramp; b = 0; }
                break;
            case 'B': // Azul
                if (t192 > 0x80) { r = heatramp; g = heatramp; b = 255; }
                else if (t192 > 0x40) { r = 0; g = heatramp; b = 255; }
                else { r = 0; g = 0; b = heatramp; }
                break;
            case 'O': // Naranja
                if (t192 > 0x80) { r = 255; g = 255; b = heatramp; }
                else if (t192 > 0x40) { r = 255; g = heatramp; b = 0; }
                else { r = heatramp; g = heatramp / 2; b = 0; }
                break;
            case 'P': // Púrpura
                if (t192 > 0x80) { r = 255; g = heatramp; b = 255; }
                else if (t192 > 0x40) { r = 255; g = 0; b = heatramp; }
                else { r = heatramp; g = 0; b = heatramp; }
                break;
            case 'Y': // Amarillo
                if (t192 > 0x80) { r = 255; g = 255; b = heatramp; }
                else if (t192 > 0x40) { r = 255; g = 255; b = 0; }
                else { r = heatramp; g = heatramp; b = 0; }
                break;
            case 'C': // Cian
                if (t192 > 0x80) { r = heatramp; g = 255; b = 255; }
                else if (t192 > 0x40) { r = 0; g = 255; b = 255; }
                else { r = 0; g = heatramp; b = heatramp; }
                break;
            case 'M': // Magenta
                if (t192 > 0x80) { r = 255; g = heatramp; b = 255; }
                else if (t192 > 0x40) { r = 255; g = 0; b = 255; }
                else { r = heatramp; g = 0; b = heatramp; }
                break;
            case 'W': // Blanco
                if (t192 > 0x80) { r = 255; g = 255; b = 255; }
                else if (t192 > 0x40) { r = heatramp; g = heatramp; b = 255; }
                else { r = heatramp; g = heatramp; b = heatramp; }
                break;
            case 'L': // Lima
                if (t192 > 0x80) { r = 255; g = 255; b = heatramp; }
                else if (t192 > 0x40) { r = heatramp; g = 255; b = 0; }
                else { r = heatramp / 2; g = heatramp; b = 0; }
                break;
            case 'R': default: // Rojo
                if (t192 > 0x80) { r = 255; g = 255; b = heatramp; }
                else if (t192 > 0x40) { r = 255; g = heatramp; b = 0; }
                else { r = heatramp; g = 0; b = 0; }
                break;
        }
        externPixel->setPixelColor(j + startLed, externPixel->Color(r, g, b));
    }
    externPixel->show();
}

void OF_RGB::iceEffect() {
    uint16_t startLed = OF_Prefs::settings[OF_Const::effectsStartLed];
    uint16_t numLeds = OF_Prefs::settings[OF_Const::effectsLedCount];
    if (numLeds == 0) return;

    uint32_t color = getColorFromChar(effectColorChar);

    //sparkles
    if (random(255) < 80) {
        int led = random(numLeds);
        externPixel->setPixelColor(led + startLed, color);
    }
    for (int i = startLed; i < startLed + numLeds; i++) {
        uint32_t currentColor = externPixel->getPixelColor(i);
        uint8_t r = ((currentColor >> 16) & 0xFF) / 2;
        uint8_t g = ((currentColor >> 8) & 0xFF) / 2;
        uint8_t b = (currentColor & 0xFF) / 2;
        externPixel->setPixelColor(i, externPixel->Color(r, g, b));
    }
    externPixel->show();
}

void OF_RGB::plasmaEffect() {
    uint16_t startLed = OF_Prefs::settings[OF_Const::effectsStartLed];
    uint16_t numLeds = OF_Prefs::settings[OF_Const::effectsLedCount];
    if (numLeds == 0) return;

    uint32_t baseColor = getColorFromChar(effectColorChar);
    uint8_t base_r = (baseColor >> 16) & 0xFF;
    uint8_t base_g = (baseColor >> 8) & 0xFF;
    uint8_t base_b = baseColor & 0xFF;

    for (int i = 0; i < numLeds; i++) {
        uint8_t r = (uint8_t)((base_r / 2.0) + (base_r / 2.0) * sin(i / 8.0 + millis() / 500.0));
        uint8_t g = (uint8_t)((base_g / 2.0) + (base_g / 2.0) * sin(i / 7.0 + millis() / 400.0));
        uint8_t b = (uint8_t)((base_b / 2.0) + (base_b / 2.0) * sin(i / 6.0 + millis() / 600.0));
        externPixel->setPixelColor(i + startLed, externPixel->Color(r, g, b));
    }
    externPixel->show();
}

void OF_RGB::beamEffect() {
    uint16_t startLed = OF_Prefs::settings[OF_Const::effectsStartLed];
    uint16_t numLeds = OF_Prefs::settings[OF_Const::effectsLedCount];
    if (numLeds == 0) return;

    if ((millis() / 80) % 2 == 0) {
        uint32_t color = getColorFromChar(effectColorChar);
        for (int i = startLed; i < startLed + numLeds; i++) {
            externPixel->setPixelColor(i, color);
        }
    } else {
        for (int i = startLed; i < startLed + numLeds; i++) {
            externPixel->setPixelColor(i, 0);
        }
    }
    externPixel->show();
}

void OF_RGB::knightRiderEffect() {
    uint16_t startLed = OF_Prefs::settings[OF_Const::effectsStartLed];
    uint16_t numLeds = OF_Prefs::settings[OF_Const::effectsLedCount];
    if (numLeds == 0) return;

    int ridingWidth = 4;
    int delayDuration = 50;

    if (millis() - lastRiderUpdate < delayDuration) {
        return;
    }
    lastRiderUpdate = millis();

    for (int i = startLed; i < startLed + numLeds; i++) { externPixel->setPixelColor(i, 0); }

    uint32_t color = getColorFromChar(effectColorChar);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    for (int j = 0; j < ridingWidth; j++) {
        int ledIndex = riderPosition + j;
        if (ledIndex >= 0 && ledIndex < numLeds) {
            externPixel->setPixelColor(ledIndex + startLed, externPixel->Color(r, g, b));
        }
    }
    if (riderPosition - 1 >= 0) {
        externPixel->setPixelColor(riderPosition - 1 + startLed, externPixel->Color(r / 4, g / 4, b / 4));
    }
    if (riderPosition + ridingWidth < numLeds) {
        externPixel->setPixelColor(riderPosition + ridingWidth + startLed, externPixel->Color(r / 4, g / 4, b / 4));
    }
    externPixel->show();

    if (riderDirection) {
        riderPosition++;
        if (riderPosition + ridingWidth >= numLeds) {
            riderDirection = false;
        }
    } else {
        riderPosition--;
        if (riderPosition <= 0) {
            riderDirection = true;
        }
    }
}

#endif // LED_ENABLE
