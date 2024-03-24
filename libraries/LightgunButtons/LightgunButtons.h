/*!
 * @file LightgunButtons.h
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

#ifndef _LIGHTGUNBUTTONS_H_
#define _LIGHTGUNBUTTONS_H_

#include <stdint.h>

/// @brief Relatively simple buttons with some decent per-button confirgurable debouncing.
/// @details While intended for a Light gun, can be used for any HID using AbsMouse5 and/or Keyboard.
/// Basic usage is to periodically call Poll() and then check the various bit mask values.
/// This assumes a logical high value for released (0 for pressed).
/// The only limitation is 32 buttons since a uint32_t is used for the button bitmask.
/// If your light gun needs more than 32 buttons then I wanna see pics.
class LightgunButtons {
public:
    /// @brief Report type.
    enum ReportType_e {
        ReportType_Mouse = 0,
        ReportType_Keyboard = 1,
        ReportType_Internal = 2,
        ReportType_Gamepad = 3
    };

    /// @brief Descriptor.
    typedef struct Desc_s {
        int8_t pin;                   ///< Arduino defined pin to read.
        uint8_t reportType;           ///< Report type. See ReportType_e.
        uint8_t reportCode;           ///< Report code. Mouse or key press depending on report type.
        uint8_t reportType2;          ///< Report type 2, for offscreen presses
        uint8_t reportCode2;          ///< Report code type 2, for offscreen presses
        uint8_t reportType3;
        uint8_t reportCode3;
        uint8_t debounceTicks;        ///< Number of millis() to wait after the button state changes.
        uint32_t debounceFifoMask;    ///< Mask checked to ensure button state is consistent (0 to disable)
    } Desc_t;

    /// @brief Runtime debouncing state data.
    /// The arrays must be the same length as the ButtonDesc[] descriptor array.
    typedef struct Data_s {
        uint32_t* pArrFifo;             ///< Pointer to button fifo array.
        uint8_t* pArrDebounceCount;     ///< Pointer to button debounce counters.
    } Data_t;
    
    /// @brief Constructor.
    LightgunButtons(Data_t data, unsigned int count);

    /// @brief Initialize the buttons.
    void Begin();

    /// @brief De-initialize the buttons.
    void Unset();

    /// @brief Poll button state.
    /// @details This will reset pressed, released, and pressedReleased.
    /// @param[in] minTicks Minimum number of ticks for poll to update.
    /// @return The pressed value.
    uint32_t Poll(unsigned long minTicks = 0);

    /// @brief Update the internal repeat value.
    /// @details Call after Poll() if the repeat value is required.
    /// @return The repeat value.
    uint32_t Repeat();

    /// @brief The buttons that must be defined in the sketch.
    static Desc_t ButtonDesc[];

    /// @brief Bit mask of newly pressed buttons from last poll, 1 if pressed.
    /// @details Resets on each Poll().
    uint32_t pressed;

    /// @brief Bit mask of newly released buttons from last poll, 1 if released.
    /// @details Resets on each Poll().
    uint32_t released;

    /// @brief Debounced buttons that internally repeat (pulse) at the specified interval.
    /// @details This is for internal use, not related to reporting HID events to the host.
    /// This only updates when calling Repeat().
    uint32_t repeat;

    /// @brief Bit mask of debounced buttons, 1 if pressed.
    uint32_t debounced;
    
    /// @brief Bit mask of buttons currently debouncing.
    /// @details Buttons can be debouncing after being pressed or released.
    uint32_t debouncing;
    
    /// @brief Bit mask of debounced buttons pressed and released since last poll.
    /// @details Track all pressed buttons and set only when all buttons release.
    /// Resets on each Poll().
    uint32_t pressedReleased;

    /// @brief Interval for pulsing the repeat value while buttons are pressed for Repeat().
    unsigned int interval;

    /// @brief Bit mask of buttons to enable reporting HID events to host.
    uint32_t report;

    /// @brief Enable reporting for all buttons. Set report to 0xFFFFFFFF.
    void ReportEnable() { report = 0xFFFFFFFF; }

    /// @brief Disable reporting for all buttons. Clear report to 0.
    void ReportDisable() { report = 0; }

    /// @brief Flag that determines if we're shooting off-screen.
    bool offScreen;

    /// @brief Flag that determines analog output mode.
    bool analogOutput;

    /// @brief Test if pressed button(s) in comibination with already held buttons match given values.
    /// @details Test the pressed buttons equals a given value along with a modifer bit mask
    /// match with the debounced value.
    /// @param[in] pressedMask Bit mask newly pressed buttons to match with pressed.
    /// @param[in] modifierMask Bit mask of buttons already held down to match with debounced.
    /// @return true if pressedMask equals pressed and the modifierMask is debounced.
    bool ModifierPressed(uint32_t pressedMask, uint32_t modifierMask) {
        // note that since pressedMask is expected to pressed, it will also be debounced
        return ((pressedMask == pressed) && ((modifierMask | pressedMask) == debounced)) ? true : false;
    }

    /// @brief Get the button index from a mask or -1 if a single button is not matched
    static int MaskToIndex(uint32_t mask);

private:
    /// @brief millis() value from last Poll
    unsigned long lastMillis;
    
    /// @brief millis() value from last Repeat()
    unsigned long lastRepeatMillis;
    
    /// @brief button pin states
    uint32_t pinState;

    /// @brief Internal tracked buttons for the final pressedReleased value.
    uint32_t internalPressedReleased;

    /// @brief Bit mask of reported pressed buttons.
    uint32_t reportedPressed;

    /// @brief Button state FIFO array.
    uint32_t* stateFifo;

    /// @brief Button debounce count array.
    uint8_t* debounceCount;

    /// @brief Internal bit mask of directional buttons pressed.
    /// @details Only the four rightmost bits are used to track state of gamepad directional buttons pressed.
    uint8_t padMask = 0x00000000;

    /// @brief Bit mask of directional buttons pressed after conversion.
    /// @details Converted from padMask before being passed over to TinyUSB_Devices' due to how HID POV works.
    uint8_t padMaskConv = 0x00000000;

    /// @brief Tracked buttons that are offscreen.
    uint32_t internalOffscreenMask;

    /// @brief Number of buttons.
    const unsigned int count;
};

/// @brief Helper to allocate button data arrays.
template<unsigned int count>
class LightgunButtonsStatic {
private:
    uint32_t stateFifoArr[count];
    uint8_t debounceCountArr[count];

public:
    operator LightgunButtons::Data_t() { 
        LightgunButtons::Data_t d = {stateFifoArr, debounceCountArr};
        return d; 
    }
};

#endif // _LIGHTGUNBUTTONS_H_
