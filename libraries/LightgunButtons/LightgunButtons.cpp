/*!
 * @file LightgunButtons.cpp
 * @brief HID buttons originally intended for use with a light gun.
 *
 * @copyright Mike Lynch, 2021
 *
 *  LightgunButtons is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <Arduino.h>
#include <TinyUSB_Devices.h>
#include "LightgunButtons.h"

LightgunButtons::LightgunButtons(Data_t _data, unsigned int _count) :
    pressed(0),
    released(0),
    repeat(0),
    debounced(0),
    debouncing(0),
    pressedReleased(0),
    interval(33),
    report(0),
    lastMillis(0),
    lastRepeatMillis(0),
    pinState(0xFFFFFFFF),
    internalPressedReleased(0),
    reportedPressed(0),
    count(_count),
    stateFifo(_data.pArrFifo),
    debounceCount(_data.pArrDebounceCount)
{
}

void LightgunButtons::Begin()
{
    // set button pins to input with pullup
    for(unsigned int i = 0; i < count; ++i) {
        pinMode(ButtonDesc[i].pin, INPUT_PULLUP);
        stateFifo[i] = 0xFFFFFFFF;
        debounceCount[i] = 0;
    }
}

uint32_t LightgunButtons::Poll(unsigned long minTicks)
{
    unsigned long m = millis();
    unsigned long ticks = m - lastMillis;
    uint32_t bitMask;
    
    // reset pressed and released from last poll
    pressed = 0;
    released = 0;
    pressedReleased = 0;
    
    if(ticks < minTicks) {
        return 0;
    }
    lastMillis = m;

    if(debouncing && ticks) {
        bitMask = 1;
        for(unsigned int i = 0; i < count; ++i, bitMask <<= 1) {
            const Desc_t& btn = ButtonDesc[i];
            if(debounceCount[i]) {
                if(ticks < debounceCount[i]) {
                    debounceCount[i] -= ticks;
                } else {
                    debounceCount[i] = 0;
                    debouncing &= ~bitMask;
                }
            }
        }
    }

    bitMask = 1;
    for(unsigned int i = 0; i < count; ++i, bitMask <<= 1) {
        const Desc_t& btn = ButtonDesc[i];

        // if not debouncing
        if(!debounceCount[i]) {
            // read the pin, expected to return 0 or 1
            uint32_t state = digitalRead(btn.pin);
            
            // if a state fifo mask is defined
            if(btn.debounceFifoMask) {
                // add the state to the fifo
                stateFifo[i] <<= 1;
                stateFifo[i] |= state;

                // apply the mask and check the value
                uint32_t m = stateFifo[i] & btn.debounceFifoMask;
                if(!m) {
                    state = 0;
                } else if(m == btn.debounceFifoMask) {
                    // use the bit mask for this button
                    state = bitMask;
                } else {
                    // button is bouncing, continue to next button
                    continue;
                }
            } else {
                // no fifo, so change high state into bit mask
                if(state) {
                    state = bitMask;
                }
            }

            // if existing pin state does not match new state
            if((pinState & bitMask) != state) {
                // update the pin state
                pinState = (pinState & ~bitMask) | state;

                // set the debounce counter and set the flag
                debounceCount[i] = btn.debounceTicks;
                debouncing |= bitMask;

                if(!state) {
                    // state is low, button is pressed

                    // if reporting is enabled for the button
                    if(report & bitMask) {
                        reportedPressed |= bitMask;
                        if(analogOutput) {
                            if(btn.reportType3 == ReportType_Keyboard) {
                                Keyboard.press(btn.reportCode3);
                            } else if(btn.reportType3 == ReportType_Gamepad) {
                                Gamepad16.press(btn.reportCode3);
                            }
                        } else if(offScreen) {
                            bitWrite(internalOffscreenMask, i, 1);
                            if(btn.reportType2 == ReportType_Mouse) {
                                AbsMouse5.press(btn.reportCode2);
                            } else if(btn.reportType2 == ReportType_Keyboard) {
                                Keyboard.press(btn.reportCode2);
                            }
                        } else {
                            if(btn.reportType == ReportType_Mouse) {
                                AbsMouse5.press(btn.reportCode);
                            } else if(btn.reportType == ReportType_Keyboard) {
                                Keyboard.press(btn.reportCode);
                            }
                        }
                    }

                    // button is debounced pressed and add it to the pressed/released combo
                    debounced |= bitMask;
                    pressed |= bitMask;
                    internalPressedReleased |= bitMask;

                } else {
                    // state high, button is not pressed

                    // if the button press was reported then report the release
                    // note that the report flag is ignored here to avoid stuck buttons
                    // in case the reporting is disabled while button(s) are pressed
                    if(reportedPressed & bitMask) {
                        reportedPressed &= ~bitMask;
                        if(analogOutput) {
                            if(btn.reportType3 == ReportType_Keyboard) {
                                Keyboard.release(btn.reportCode3);
                            } else if(btn.reportType3 == ReportType_Gamepad) {
                                Gamepad16.release(btn.reportCode3);
                            }
                        } else if(bitRead(internalOffscreenMask, i)) {
                            bitWrite(internalOffscreenMask, i, 0);
                            if(btn.reportType2 == ReportType_Mouse) {
                                AbsMouse5.release(btn.reportCode2);
                            } else if(btn.reportType2 == ReportType_Keyboard) {
                                Keyboard.release(btn.reportCode2);
                            }
                        } else {
                            if(btn.reportType == ReportType_Mouse) {
                                AbsMouse5.release(btn.reportCode);
                            } else if(btn.reportType == ReportType_Keyboard) {
                                Keyboard.release(btn.reportCode);
                            }
                        }
                    }

                    // clear the debounced state and button is released
                    debounced &= ~bitMask;
                    released |= bitMask;                    

                    // if all buttons released
                    if(!debounced) {
                        // report the combination pressed/released state
                        pressedReleased = internalPressedReleased;
                        internalPressedReleased = 0;
                    }
                }
            }
        }
    }

    return pressed;
}

uint32_t LightgunButtons::Repeat()
{
    unsigned long m = millis();
    if(m - lastRepeatMillis >= interval) {
        lastRepeatMillis = m;
        repeat = debounced;
    } else {
        repeat = 0;
    }
    return repeat;
}

int LightgunButtons::MaskToIndex(uint32_t mask)
{
    uint32_t bitMask = 1;
    for(unsigned int i = 0; bitMask; ++i, bitMask <<= 1) {
        if(bitMask == mask) {
            return i;
        }
    }
    return -1;
}
