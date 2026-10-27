// A fake of Adafruit's TinyUSB HID class, for the real TinyUSB_Devices.cpp: it records the descriptor the devices set up and every
// report they send, and can be made busy for a while so that the code has to wait for it.
#pragma once

#include <cstdint>
#include <vector>

// The HID descriptor items that TinyUSB_Devices.h and .cpp use, with the bytes the real macros give. The keyboard's
// descriptor is not modelled: only its report id matters here.
#define HID_REPORT_ID(x) 0x85, x,
#define TUD_HID_REPORT_DESC_KEYBOARD(...) 0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, __VA_ARGS__ 0xC0
#define HID_USAGE_PAGE(x) 0x05, x
#define HID_USAGE(x) 0x09, x
#define HID_USAGE_MIN(x) 0x19, x
#define HID_USAGE_MAX(x) 0x29, x
#define HID_COLLECTION(x) 0xA1, x
#define HID_COLLECTION_END 0xC0
#define HID_REPORT_COUNT(x) 0x95, x
#define HID_REPORT_SIZE(x) 0x75, x
#define HID_LOGICAL_MIN(x) 0x15, x
#define HID_LOGICAL_MAX(x) 0x25, x
#define HID_PHYSICAL_MIN(x) 0x35, x
// Items of two bytes only (the only size used here), least significant byte first.
#define HID_LOGICAL_MIN_N(x, n) (0x14 | (n)), ((x) & 0xFF), (((x) >> 8) & 0xFF)
#define HID_LOGICAL_MAX_N(x, n) (0x24 | (n)), ((x) & 0xFF), (((x) >> 8) & 0xFF)
#define HID_PHYSICAL_MAX_N(x, n) (0x44 | (n)), ((x) & 0xFF), (((x) >> 8) & 0xFF)
#define HID_INPUT(x) 0x81, x
#define HID_DATA 0x00
#define HID_VARIABLE 0x02
#define HID_ABSOLUTE 0x00
#define HID_USAGE_PAGE_DESKTOP 0x01
#define HID_USAGE_PAGE_BUTTON 0x09
#define HID_USAGE_DESKTOP_GAMEPAD 0x05
#define HID_USAGE_DESKTOP_X 0x30
#define HID_USAGE_DESKTOP_Y 0x31
#define HID_USAGE_DESKTOP_RX 0x33
#define HID_USAGE_DESKTOP_RY 0x34
#define HID_USAGE_DESKTOP_HAT_SWITCH 0x39
#define HID_COLLECTION_APPLICATION 0x01

class Adafruit_USBD_HID {
public:
    struct Report {
        uint8_t id;
        std::vector<uint8_t> bytes;
    };

    // What the devices set up.
    uint8_t pollInterval = 0;
    std::vector<uint8_t> reportDescriptor;
    int begun = 0;
    // What they sent.
    std::vector<Report> reports;
    // The port is not ready for this many more asks, as when the previous report has not been taken by the host yet.
    int busyFor = 0;
    int readyAsks = 0;

    void setPollInterval(uint8_t interval) { pollInterval = interval; }
    void setReportDescriptor(const uint8_t* descriptor, uint16_t length) { reportDescriptor.assign(descriptor, descriptor + length); }
    bool begin() { begun++; return true; }
    bool ready() {
        readyAsks++;
        if (busyFor > 0) { busyFor--; return false; }
        return true;
    }
    bool sendReport(uint8_t id, const void* report, uint8_t length) {
        const uint8_t* bytes = (const uint8_t*)report;
        reports.push_back({id, std::vector<uint8_t>(bytes, bytes + length)});
        return true;
    }
    // A keyboard report: the modifiers, a reserved byte, and the six keys.
    bool keyboardReport(uint8_t id, uint8_t modifiers, const uint8_t keys[6]) {
        std::vector<uint8_t> bytes = {modifiers, 0};
        bytes.insert(bytes.end(), keys, keys + 6);
        reports.push_back({id, bytes});
        return true;
    }
    void clear() { reports.clear(); busyFor = 0; readyAsks = 0; }
};
