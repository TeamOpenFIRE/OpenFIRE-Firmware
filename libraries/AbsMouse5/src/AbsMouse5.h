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

// 5 button absolute mouse
class AbsMouse5_
{
private:
	uint8_t _buttons;
	uint16_t _x;
	uint16_t _y;
	uint32_t _width;
	uint32_t _height;
	bool _autoReport;

public:
	AbsMouse5_();
	void init(uint16_t width = 32767, uint16_t height = 32767, bool autoReport = true);
	void report(void);
	void move(uint16_t x, uint16_t y);
	void press(uint8_t b = MOUSE_LEFT);
	void release(uint8_t b = MOUSE_LEFT);
};

// global singleton
extern AbsMouse5_ AbsMouse5;

#endif // _ABSMOUSE5_H_
