// A fake of the Pico SDK's PWM slices: it records the wrap, the clock divider and the channel levels the firmware set.
#pragma once

#include <cstdint>

struct PwmSlice {
    uint16_t wrap = 0;
    float divider = 1;
    uint16_t level[2] = {0, 0};
    bool enabled = false;
};
extern PwmSlice testPwmSlice[16];
extern int testGpioInvalidCalls;
inline unsigned pwm_gpio_to_slice_num(unsigned pin) { if (pin >= 64) testGpioInvalidCalls++; return (pin >> 1) & 7; }
inline unsigned pwm_gpio_to_channel(unsigned pin) { return pin & 1; }
inline void pwm_set_wrap(unsigned slice, uint16_t wrap) { testPwmSlice[slice].wrap = wrap; }
inline void pwm_set_clkdiv(unsigned slice, float divider) { testPwmSlice[slice].divider = divider; }
inline void pwm_set_chan_level(unsigned slice, unsigned channel, uint16_t level) { testPwmSlice[slice].level[channel] = level; }
inline void pwm_set_enabled(unsigned slice, bool enabled) { testPwmSlice[slice].enabled = enabled; }
