/*!
 * @file SamcoColours.h
 * @brief Collection of unique colours for RGB LEDs such as Dot Stars or Neopixels.
 *
 * @copyright Mike Lynch, 2021
 * @copyright GNU Lesser General Public License
 *
 * @author Mike Lynch
 * @version V1.0
 * @date 2021
 */

#ifndef _SAMCOCOLOURS_H_
#define _SAMCOCOLOURS_H_

#include <stdint.h>

// macro to scale an 8 bit colour value by an 8 bit value
// as seen by the math, 255 means full value
#define COLOR_BRI_ADJ_COLOR(brightness, color) ((((brightness) * ((color) & 0xFF)) / 255) & 0xFF)
//#define COLOR_BRI_ADJ_COLOR(brightness, color) (color)

// macro to scale a 32-bit RGBW word with an 8 bit brightness value
#define COLOR_BRI_ADJ_RGB(brightness, rgb) COLOR_BRI_ADJ_COLOR(brightness, rgb) \
    | (COLOR_BRI_ADJ_COLOR(brightness, (rgb >> 8)) << 8) \
    | (COLOR_BRI_ADJ_COLOR(brightness, (rgb >> 16)) << 16) \
    | (COLOR_BRI_ADJ_COLOR(brightness, (rgb >> 24)) << 24)

// some distinct colours from Wikipedia https://en.wikipedia.org/wiki/Lists_of_colors
// also adjusted the brightness to make them look even more distinct on an ItsyBitsy DotStar
// ... yeah I spent too much time on this
namespace WikiColor {
    constexpr uint32_t Amber = COLOR_BRI_ADJ_RGB(130, 0xFFBF00);
    constexpr uint32_t Blue = COLOR_BRI_ADJ_RGB(225, 0x0000FF);
    constexpr uint32_t Carnation_pink = COLOR_BRI_ADJ_RGB(165, 0xFFA6C9);
    constexpr uint32_t Cerulean_blue = COLOR_BRI_ADJ_RGB(255, 0x2A52BE);
    constexpr uint32_t Cornflower_blue = COLOR_BRI_ADJ_RGB(175, 0x6495ED);
    constexpr uint32_t Cyan = COLOR_BRI_ADJ_RGB(145, 0x00FFFF);
    constexpr uint32_t Electric_indigo = COLOR_BRI_ADJ_RGB(235, 0x6F00FF);
    constexpr uint32_t Ghost_white = COLOR_BRI_ADJ_RGB(135, 0xF8F8FF);
    constexpr uint32_t Golden_yellow = COLOR_BRI_ADJ_RGB(135, 0xFFDF00);
    constexpr uint32_t Green = COLOR_BRI_ADJ_RGB(140, 0x00FF00);
    constexpr uint32_t Green_Lizard = COLOR_BRI_ADJ_RGB(145, 0xA7F432);
    constexpr uint32_t Magenta = COLOR_BRI_ADJ_RGB(140, 0xFF00FF);
    constexpr uint32_t Orange = COLOR_BRI_ADJ_RGB(150, 0xFF7F00);
    constexpr uint32_t Red = COLOR_BRI_ADJ_RGB(145, 0xFF0000);
    constexpr uint32_t Salmon = COLOR_BRI_ADJ_RGB(175, 0xFA8072);
    constexpr uint32_t Yellow = COLOR_BRI_ADJ_RGB(135, 0xFFFF00);
};

#endif // _SAMCOCOLOURS_H_
