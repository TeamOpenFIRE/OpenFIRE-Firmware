/*!
 * @file OpenFIREcalibration.h
 * @brief Maps the Perspective output to the screen with the calibration offsets.
 *
 * Kept apart from OpenFIREcommon.cpp, which needs the board, so that it can be tested on a PC.
 */

#ifndef _OPENFIRECALIBRATION_H_
#define _OPENFIRECALIBRATION_H_

#include <cstdint>
#include <limits>

#include <OpenFIREConst.h>

// Limit for the Perspective output before MapCalibratedAxis(), derived from 32-bit integer
// arithmetic rather than tuned: with |offset| <= res_x, (value - offset) * res stays within int32_t.
// 271940 is about 35 screen widths: beyond it the gun aims almost parallel to the screen,
// and the cursor is clamped to the screen edge later anyway.
constexpr int32_t CALI_MAP_VALUE_LIMIT = (std::numeric_limits<int32_t>::max() / res_x) - res_x;
static_assert(res_x >= res_y, "CALI_MAP_VALUE_LIMIT assumes res_x is the larger axis");
static_assert(CALI_MAP_VALUE_LIMIT > 0, "res_x too large for 32-bit calibration mapping");

// Maps one Perspective axis to the screen with the calibration offsets.
// lowOffset is the reading taken while aiming at the top (or left) edge, highOffset is res
// minus the reading taken at the bottom (or right) edge: the two readings land exactly on
// 0 and res, as aimed during calibration. With zero offsets the result is the plain
// Perspective value. Only 32-bit integer operations: one multiply and one divide, like map().
inline int32_t MapCalibratedAxis(int32_t value, int32_t res, int32_t lowOffset, int32_t highOffset)
{
    // Limited first, so every result stays small: the moving averages that follow
    // add up to four results without overflowing.
    if(value > CALI_MAP_VALUE_LIMIT)
        value = CALI_MAP_VALUE_LIMIT;
    else if(value < -CALI_MAP_VALUE_LIMIT)
        value = -CALI_MAP_VALUE_LIMIT;

    // A real calibration reads each screen edge within one screen of the LED range, and leaves
    // at least a quarter of the range to the screen. Anything else is a damaged calibration:
    // use the profile as uncalibrated.
    if(lowOffset < -res || lowOffset > res || highOffset < -res || highOffset > res)
        return value;

    const int32_t span = res - lowOffset - highOffset;   // between -res and 3 * res
    if(span < res / 4)
        return value;

    // map(value, lowOffset, res - highOffset, 0, res)
    return ((value - lowOffset) * res) / span;
}

#endif // _OPENFIRECALIBRATION_H_
