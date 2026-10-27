// A fake SSD1306 OLED: it doesn't draw, it records what was asked of it, so that a test can read what the screen would show.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Wire.h"

#define SSD1306_SWITCHCAPVCC 0x02
#define SSD1306_BLACK 0
#define SSD1306_WHITE 1
#define SSD1306_INVERSE 2

class Adafruit_SSD1306 {
public:
    // Whether begin() succeeds for the displays made from now on, as it does not when no screen answers on the bus.
    static inline bool beginSucceedsByDefault = true;

    Adafruit_SSD1306(uint8_t width, uint8_t height, TwoWire* wire, int8_t resetPin) : beginSucceeds(beginSucceedsByDefault), width(width), height(height), wire(wire) { (void)resetPin; }

    bool begin(uint8_t vccMode, uint8_t address) {
        beginCalls++;
        beginVcc = vccMode;
        beginAddress = address;
        return beginSucceeds;
    }
    void clearDisplay() { text.clear(); lines.clear(); bitmaps = 0; rects.clear(); circles.clear(); cleared++; cursorX = cursorY = 0; }
    void display() { displayed++; }
    void setCursor(int16_t x, int16_t y) { cursorX = x; cursorY = y; }
    void setTextSize(uint8_t size) { textSize = size; }
    void setTextColor(uint16_t color) { textColor = color; textBackground = color; }
    void setTextColor(uint16_t color, uint16_t background) { textColor = color; textBackground = background; }

    template <class T> size_t print(T value) {
        const std::string printed = toText(value);
        text += printed;
        lines.push_back({printed, cursorX, cursorY, textColor, textBackground});
        return printed.size();
    }
    template <class T> size_t println(T value) {
        print(value);
        text += "\n";
        return 1;
    }
    size_t println() { text += "\n"; return 1; }

    void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint16_t color) {
        (void)x; (void)y; (void)bitmap; (void)w; (void)h; (void)color;
        bitmaps++;
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { rects.push_back({x, y, w, h, color}); }
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { (void)x; (void)y; (void)w; (void)color; lineDraws++; }
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) { circles.push_back({x, y, r, color}); }

    // For the tests: what the code did.
    struct Line { std::string text; int16_t x, y; uint16_t foreground, background; };
    // Whether the text was printed highlighted (black on white), as the menus mark the selected entry.
    static bool highlighted(const Line& line) { return line.foreground == SSD1306_BLACK && line.background == SSD1306_WHITE; }
    struct Rect { int16_t x, y, w, h; uint16_t color; };
    struct Circle { int16_t x, y, r; uint16_t color; };
    bool beginSucceeds;
    int beginCalls = 0, displayed = 0, cleared = 0, bitmaps = 0, lineDraws = 0;
    uint8_t beginVcc = 0, beginAddress = 0;
    uint8_t width, height;
    TwoWire* wire;
    int16_t cursorX = 0, cursorY = 0;
    uint8_t textSize = 1;
    uint16_t textColor = 1, textBackground = 1;
    std::string text;                 // everything printed since the last clearDisplay()
    std::vector<Line> lines;          // every print, with where the cursor was
    std::vector<Rect> rects;
    std::vector<Circle> circles;

private:
    static std::string toText(const char* value) { return value ? value : ""; }
    static std::string toText(const std::string& value) { return value; }
    template <class T> static std::string toText(T value) { return std::to_string(value); }
};
