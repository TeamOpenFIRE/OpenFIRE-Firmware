// A fake of the Raspberry Pi Pico SDK's GPIO functions: they record what the firmware asked for.
#pragma once

#include <cstdint>

#define GPIO_FUNC_PWM 4
#define GPIO_FUNC_SIO 5
#define GPIO_OUT 1
#define GPIO_IN 0

extern int testGpioLevel[64];
extern int testGpioFunction[64];
extern int testGpioDirection[64];
// A pin number that isn't one of the 64 the fake knows (-1 as unsigned, for one) would make the real functions write to
// memory that is not a pin's: it is counted here, and ignored.
extern int testGpioInvalidCalls;
inline void gpio_put(unsigned pin, bool level) { if (pin >= 64) { testGpioInvalidCalls++; return; } testGpioLevel[pin] = level; }
inline void gpio_set_dir(unsigned pin, bool out) { if (pin >= 64) { testGpioInvalidCalls++; return; } testGpioDirection[pin] = out; }
inline void gpio_set_function(unsigned pin, int function) { if (pin >= 64) { testGpioInvalidCalls++; return; } testGpioFunction[pin] = function; }

// The system clock the firmware asked for, in kHz.
extern unsigned testSysClockKhz;
inline bool set_sys_clock_khz(uint32_t khz, bool required) { (void)required; testSysClockKhz = khz; return true; }
