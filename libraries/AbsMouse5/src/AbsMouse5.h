/*!
 * @file AbsMouse5.h
 * @brief 5 button absolute mouse.
 * @n Based on AbsMouse Arduino library. Modified to be a 5 button device.
 */

#ifndef _ABSMOUSE5_H_
#define _ABSMOUSE5_H_

#include <stdint.h>

#define MOUSE_LEFT 0x01
#define MOUSE_RIGHT 0x02
#define MOUSE_MIDDLE 0x04
#define MOUSE_BUTTON4 0x08
#define MOUSE_BUTTON5 0x10

#if defined(USE_TINYUSB)
#define TUD_HID_REPORT_DESC_ABSMOUSE5(...) \
	0x05, 0x01, \
	0x09, 0x02, \
	0xA1, 0x01, \
	__VA_ARGS__ \
	0x09, 0x01, \
	0xA1, 0x00, \
	0x05, 0x09, \
	0x19, 0x01, \
	0x29, 0x05, \
	0x15, 0x00, \
	0x25, 0x01, \
	0x95, 0x05, \
	0x75, 0x01, \
	0x81, 0x02, \
	0x95, 0x01, \
	0x75, 0x03, \
	0x81, 0x03, \
	0x05, 0x01, \
	0x09, 0x30, \
	0x09, 0x31, \
	0x16, 0x00, 0x00, \
	0x26, 0xFF, 0x7F, \
	0x36, 0x00, 0x00, \
	0x46, 0xFF, 0x7F, \
	0x75, 0x10, \
	0x95, 0x02, \
	0x81, 0x02, \
	0xC0, \
	0xC0
#endif // USE_TINYUSB

// 5 button absolute mouse
class AbsMouse5_
{
private:
	const uint8_t _reportId;
	uint8_t _buttons;
	uint16_t _x;
	uint16_t _y;
	uint32_t _width;
	uint32_t _height;
	bool _autoReport;

public:
	AbsMouse5_(uint8_t reportId = 1);
	void init(uint16_t width = 32767, uint16_t height = 32767, bool autoReport = true);
	void report(void);
	void move(uint16_t x, uint16_t y);
	void press(uint8_t b = MOUSE_LEFT);
	void release(uint8_t b = MOUSE_LEFT);
};

// global singleton
extern AbsMouse5_ AbsMouse5;

#endif // _ABSMOUSE5_H_
