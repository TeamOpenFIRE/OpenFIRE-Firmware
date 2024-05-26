/*!
 * @file SamcoDisplay.cpp
 * @brief Macros for lightgun HUD display.
 *
 * @copyright That One Seong, 2024
 *
 *  SamcoDisplay is free software: you can redistribute it and/or modify
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
#include <Adafruit_SSD1306.h>
#include "SamcoDisplay.h"
#include "SamcoPreferences.h"

// include heuristics for determining Wire or Wire1 SDA/SCL pins, ref'd from SamcoPreferences::pins

Adafruit_SSD1306 display(128, 64, Wire);



#endif // _LIGHTGUNDISPLAY_H_
