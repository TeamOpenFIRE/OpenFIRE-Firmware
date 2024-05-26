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

// we're using our own splash screen kthx ada
#define SSD1306_NO_SPLASH

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "SamcoDisplay.h"
#include "SamcoPreferences.h"

// include heuristics for determining Wire or Wire1 SDA/SCL pins, ref'd from SamcoPreferences::pins

Adafruit_SSD1306 *display;

ExtDisplay::ExtDisplay() {}

bool ExtDisplay::Begin()
{
    if(display != nullptr) { delete display; }
    if(bitRead(SamcoPreferences::pins.pPeriphSCL, 1) && bitRead(SamcoPreferences::pins.pPeriphSDA, 1)) {
        // I2C1
        if(bitRead(SamcoPreferences::pins.pPeriphSCL, 0) && !bitRead(SamcoPreferences::pins.pPeriphSDA, 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire1.setSDA(SamcoPreferences::pins.pPeriphSDA);
            Wire1.setSCL(SamcoPreferences::pins.pPeriphSCL);
        }
        display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);
    } else if(!bitRead(SamcoPreferences::pins.pPeriphSCL, 1) && !bitRead(SamcoPreferences::pins.pPeriphSDA, 1)) {
        // I2C0
        if(bitRead(SamcoPreferences::pins.pPeriphSCL, 0) && !bitRead(SamcoPreferences::pins.pPeriphSDA, 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire.setSDA(SamcoPreferences::pins.pPeriphSDA);
            Wire.setSCL(SamcoPreferences::pins.pPeriphSCL);
        }
        display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
    }

    if(display->begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        screenState = Screen_None;
        display->clearDisplay();
        display->setTextColor(WHITE, BLACK);
        display->drawBitmap(24, 0, customSplashBanner, CUSTSPLASHBANN_WIDTH, CUSTSPLASHBANN_HEIGHT, WHITE);
        display->drawBitmap(40, 16, customSplash, CUSTSPLASH_WIDTH, CUSTSPLASH_HEIGHT, WHITE);
        display->display();
        return true;
    } else { return false; }
}

void ExtDisplay::TopPanelUpdate(char textPrefix[7], char textInput[16])
{
    display->fillRect(0, 0, 128, 16, BLACK);
    display->drawFastHLine(0, 15, 128, WHITE);
    display->setCursor(2, 2);
    display->setTextSize(1);
    display->setTextColor(WHITE, BLACK);
    display->print(textPrefix);
    display->println(textInput);
    display->display();
}

void ExtDisplay::ScreenModeChange(int8_t screenMode)
{
    //screenState = screenMode;
    display->fillRect(0, 16, 128, 48, BLACK);
    screenState = screenMode;
    switch(screenMode) {
      case Screen_Mamehook_Single:
        lastAmmoLeft = 255;
        lastAmmoRight = 255;
        lastLife = 255;
        break;
      case Screen_Mamehook_Dual:
        lastAmmoLeft = 255;
        lastAmmoRight = 255;
        lastLife = 255;
        display->drawBitmap(63, 16, dividerLine, DIVIDER_WIDTH, DIVIDER_HEIGHT, WHITE);
        break;
    }
    display->display();
}

void ExtDisplay::IdleOps()
{
    // add stuff here later lol
}

void ExtDisplay::PauseScreenShow()
{
    TopPanelUpdate("Prof: ", "Profile Name"); // names are placeholder
    display->fillRect(0, 16, 128, 48, BLACK);
    display->setTextSize(1);
    display->setCursor(0, 17);
    display->print(" A > ");
    display->println("Bringus");
    display->setCursor(0, 17+11);
    display->print(" B > ");
    display->println("Brongus");
    display->setCursor(0, 17+(11*2));
    display->print("Str> ");
    display->println("Broongus");
    display->setCursor(0, 17+(11*3));
    display->print("Sel> ");
    display->println("Parace L'sia");
    display->display();
}

void ExtDisplay::PauseListUpdate(uint8_t selection)
{
    display->fillRect(0, 16, 128, 48, BLACK);
    display->drawBitmap(60, 18, upArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
    display->drawBitmap(60, 59, downArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
    display->setTextSize(1);
    // Seong Note: Yeah, some of these are pretty out-of-bounds-esque behavior,
    // but pause mode selection in actual use would prevent some of these extremes from happening.
    // Just covering our asses.
    switch(selection) {
      case 0: // Calibrate
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println(" Send Escape Keypress ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println(" Calibrate ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        display->println(" Profile Select ");
        break;
      case 1: // Profile Select
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println(" Calibrate ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println(" Profile Select ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        display->println(" Save Gun Settings ");
        break;
      case 2: // Save
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println(" Profile Select ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println(" Save Gun Settings ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        if(SamcoPreferences::pins.oRumble >= 0 && SamcoPreferences::pins.sRumble == -1) {
          display->println(" Rumble Toggle ");
        } else if(SamcoPreferences::pins.oSolenoid >= 0 && SamcoPreferences::pins.sSolenoid == -1) {
          display->println(" Solenoid Toggle ");
        } else {
          display->println(" Send Escape Keypress ");
        }
        break;
      case 3: // Rumble Toggle
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println(" Save Gun Settings ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        if(SamcoPreferences::pins.oRumble >= 0 && SamcoPreferences::pins.sRumble == -1) {
          display->println(" Rumble Toggle ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          if(SamcoPreferences::pins.oSolenoid >= 0 && SamcoPreferences::pins.sSolenoid == -1) {
            display->println(" Solenoid Toggle ");
          } else {
            display->println(" Send Escape Keypress ");
          }
        } else if(SamcoPreferences::pins.oSolenoid >= 0 && SamcoPreferences::pins.sSolenoid == -1) {
          display->println(" Solenoid Toggle ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Send Escape Keypress ");
        } else {
          display->println(" Send Escape Keypress ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Calibrate ");
        }
        break;
      case 4: // Solenoid Toggle
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        if(SamcoPreferences::pins.oRumble >= 0 && SamcoPreferences::pins.sRumble == -1) {
          display->println(" Rumble Toggle ");
          display->setTextColor(BLACK, WHITE);
          display->setCursor(0, 36);
          if(SamcoPreferences::pins.oSolenoid >= 0 && SamcoPreferences::pins.sSolenoid == -1) {
            display->println(" Solenoid Toggle ");
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 47);
            display->println(" Send Escape Keypress ");
          } else {
            display->println(" Send Escape Keypress ");
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 47);
            display->println("Calibrate");
          }
        } else if(SamcoPreferences::pins.oSolenoid >= 0 && SamcoPreferences::pins.sSolenoid == -1) {
          display->println(" Solenoid Toggle ");
          display->setTextColor(BLACK, WHITE);
          display->setCursor(0, 36);
          display->println(" Send Escape Keypress ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Calibrate ");
        } else {
          display->println(" Send Escape Keypress ");
          display->setTextColor(BLACK, WHITE);
          display->setCursor(0, 36);
          display->println(" Calibrate ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Profile Select ");
        }
        break;
      case 5: // Send Escape Key
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        if(SamcoPreferences::pins.oSolenoid >= 0) {
          display->println(" Solenoid Toggle ");
          display->setTextColor(BLACK, WHITE);
          display->setCursor(0, 36);
          display->println(" Send Escape Keypress ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Calibrate ");
        } else if(SamcoPreferences::pins.oRumble >= 0) {
          display->println(" Rumble Toggle ");
          display->setTextColor(BLACK, WHITE);
          display->setCursor(0, 36);
          display->println(" Send Escape Key ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Calibrate ");
        } else {
          display->println(" Save Gun Settings ");
          display->setTextColor(BLACK, WHITE);
          display->setCursor(0, 36);
          display->println(" Send Escape Keypress ");
          display->setTextColor(WHITE, BLACK);
          display->setCursor(0, 47);
          display->println(" Calibrate ");
        }
        break;
    }
    display->display();
}

void ExtDisplay::PauseProfileUpdate(uint8_t selection)
{
    display->fillRect(0, 16, 128, 48, BLACK);
    display->drawBitmap(60, 18, upArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
    display->drawBitmap(60, 59, downArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
    display->setTextSize(1);
    switch(selection) {
      case 0: // Profile #0, etc.
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println("  Parace L'sia  ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println("  Bringus  ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        display->println("  Brongus  ");
        break;
      case 1:
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println("  Bringus  ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println("  Brongus  ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        display->println("  Broongus  ");
        break;
      case 2:
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println("  Brongus  ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println("  Broongus  ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        display->println("  Parace L'sia  ");
      case 3:
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 25);
        display->println("  Broongus  ");
        display->setTextColor(BLACK, WHITE);
        display->setCursor(0, 36);
        display->println("  Parace L'sia  ");
        display->setTextColor(WHITE, BLACK);
        display->setCursor(0, 47);
        display->println("  Bringus  ");
        break;
    }
    display->display();
}

void ExtDisplay::SaveScreen(uint8_t status)
{
    display->fillRect(0, 16, 128, 48, BLACK);
    display->setTextColor(WHITE, BLACK);
    display->setTextSize(2);
    display->setCursor(24, 24);
    display->println("Saving...");
    display->display();
}

void ExtDisplay::PrintAmmo(uint8_t ammo)
{
    // use the rounding error to get the left & right digits
    uint8_t ammoLeft = ammo / 10;
    uint8_t ammoRight = ammo - ammoLeft * 10;
    if(!ammo) { ammoEmpty = true; } else { ammoEmpty = false; }
    if(screenState == Screen_Mamehook_Single) {
        if(ammoLeft != lastAmmoLeft) {
          lastAmmoLeft = ammoLeft;
          display->fillRect(40, 22, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, BLACK);
          switch(ammoLeft) {
            case 0:
              display->drawBitmap(40, 22, number_0, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 1:
              display->drawBitmap(40, 22, number_1, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 2:
              display->drawBitmap(40, 22, number_2, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 3:
              display->drawBitmap(40, 22, number_3, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 4:
              display->drawBitmap(40, 22, number_4, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 5:
              display->drawBitmap(40, 22, number_5, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 6:
              display->drawBitmap(40, 22, number_6, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 7:
              display->drawBitmap(40, 22, number_7, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 8:
              display->drawBitmap(40, 22, number_8, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 9:
              display->drawBitmap(40, 22, number_9, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
          }
        }
        if(ammoRight != lastAmmoRight) {
          lastAmmoRight = ammoRight;
          display->fillRect(40+NUMBER_GLYPH_WIDTH+6, 22, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, BLACK);
          switch(ammoRight) {
            case 0:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_0, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 1:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_1, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 2:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_2, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 3:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_3, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 4:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_4, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 5:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_5, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 6:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_6, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 7:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_7, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 8:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_8, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 9:
              display->drawBitmap(40+NUMBER_GLYPH_WIDTH+6, 22, number_9, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
          }
        }
        display->display();
    } else if(screenState == Screen_Mamehook_Dual) {
        if(ammoLeft != lastAmmoLeft) {
          lastAmmoLeft = ammoLeft;
          display->fillRect(72, 22, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, BLACK);
          switch(ammoLeft) {
            case 0:
              display->drawBitmap(72, 22, number_0, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 1:
              display->drawBitmap(72, 22, number_1, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 2:
              display->drawBitmap(72, 22, number_2, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 3:
              display->drawBitmap(72, 22, number_3, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 4:
              display->drawBitmap(72, 22, number_4, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 5:
              display->drawBitmap(72, 22, number_5, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 6:
              display->drawBitmap(72, 22, number_6, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 7:
              display->drawBitmap(72, 22, number_7, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 8:
              display->drawBitmap(72, 22, number_8, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 9:
              display->drawBitmap(72, 22, number_9, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
          }
        }
        if(ammoRight != lastAmmoRight) {
          lastAmmoRight = ammoRight;
          display->fillRect(72+NUMBER_GLYPH_WIDTH+6, 22, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, BLACK);
          switch(ammoRight) {
            case 0:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_0, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 1:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_1, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 2:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_2, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 3:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_3, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 4:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_4, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 5:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_5, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 6:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_6, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 7:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_7, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 8:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_8, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
            case 9:
              display->drawBitmap(72+NUMBER_GLYPH_WIDTH+6, 22, number_9, NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
              break;
          }
        }
        display->display();
    }
}

void ExtDisplay::PrintLife(uint8_t life)
{
    if(!life) { lifeEmpty = true; } else { lifeEmpty = false; }
    if(screenState == Screen_Mamehook_Single) {
        if(life != lastLife) {
          lastLife = life;
          display->fillRect(34, 22, HEART_GLYPH_WIDTH*5, HEART_GLYPH_HEIGHT+20+HEART_GLYPH_HEIGHT, BLACK);
          switch(life) {
            case 9:
              display->drawBitmap(34, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(40, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(40+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(40+HEART_GLYPH_WIDTH*2, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(40+HEART_GLYPH_WIDTH*3, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 8:
              display->drawBitmap(34, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(46, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(46+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(46+HEART_GLYPH_WIDTH*2, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 7:
              display->drawBitmap(34, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(52, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(52+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 6:
              display->drawBitmap(34, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(58, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 5:
              display->drawBitmap(34, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 4:
              display->drawBitmap(40, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(40+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(40+HEART_GLYPH_WIDTH*2, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(40+HEART_GLYPH_WIDTH*3, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 3:
              display->drawBitmap(46, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(46+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(46+HEART_GLYPH_WIDTH*2, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 2:
              display->drawBitmap(52, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(52+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 1:
              display->drawBitmap(58, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 0:
              break;
            default: // 10+
              display->drawBitmap(34, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(34, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*2, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*3, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(34+HEART_GLYPH_WIDTH*4, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
          }
          display->display();
        } 
    } else if(screenState == Screen_Mamehook_Dual) {
        if(life != lastLife) {
          lastLife = life;
          display->fillRect(1, 22, HEART_GLYPH_WIDTH*5, HEART_GLYPH_HEIGHT+20+HEART_GLYPH_HEIGHT, BLACK);
          switch(life) {
            case 9:
              display->drawBitmap(1, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(7, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(7+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(7+HEART_GLYPH_WIDTH*2, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(7+HEART_GLYPH_WIDTH*3, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 8:
              display->drawBitmap(1, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(13, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(13+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(13+HEART_GLYPH_WIDTH*2, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 7:
              display->drawBitmap(1, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(19, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(19+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 6:
              display->drawBitmap(1, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(25, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 5:
              display->drawBitmap(1, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 4:
              display->drawBitmap(7, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(7+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(7+HEART_GLYPH_WIDTH*2, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(7+HEART_GLYPH_WIDTH*3, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 3:
              display->drawBitmap(13, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(13+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(13+HEART_GLYPH_WIDTH*2, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 2:
              display->drawBitmap(19, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(19+HEART_GLYPH_WIDTH, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 1:
              display->drawBitmap(25, 32, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
            case 0:
              break;
            default: // 10+
              display->drawBitmap(1, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 22, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);

              display->drawBitmap(1, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*2, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*3, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              display->drawBitmap(1+HEART_GLYPH_WIDTH*4, 42, lifeIco, HEART_GLYPH_WIDTH, HEART_GLYPH_HEIGHT, WHITE);
              break;
          }
          display->display();
        }
    }
}