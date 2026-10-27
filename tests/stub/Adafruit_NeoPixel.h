// A fake NeoPixel strand: it keeps the colour of each pixel, and counts how often show() sent them out.
#pragma once

#include <cstdint>
#include <vector>

#define NEO_GRB 0x0052
#define NEO_KHZ800 0x0000

class Adafruit_NeoPixel {
public:
    Adafruit_NeoPixel(uint16_t count, int16_t pin, uint16_t type) : pin(pin), type(type), pixels(count, 0) {}

    void begin() { began = true; }
    void clear() { for (uint32_t& pixel : pixels) pixel = 0; }
    void show() { shows++; shown = pixels; }
    void setPixelColor(uint16_t index, uint32_t color) {
        if (index < pixels.size()) pixels[index] = color;
    }
    void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) { setPixelColor(index, Color(r, g, b)); }
    // Fills `count` pixels from `first`; a count of 0 means up to the end, as in the real library.
    void fill(uint32_t color, uint16_t first = 0, uint16_t count = 0) {
        if (first >= pixels.size()) return;
        const uint16_t end = (count == 0 || (uint32_t)first + count > pixels.size()) ? (uint16_t)pixels.size() : first + count;
        for (uint16_t i = first; i < end; i++) pixels[i] = color;
    }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) { return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b; }
    uint16_t numPixels() const { return (uint16_t)pixels.size(); }

    // For the tests: what was set, what was last shown, and how the strand was made.
    int16_t pin;
    uint16_t type;
    bool began = false;
    int shows = 0;
    std::vector<uint32_t> pixels;
    std::vector<uint32_t> shown;
};
