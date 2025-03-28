/*!
 * @file SamcoDisplay.cpp
 * @brief Macros for lightgun HUD display (primarily for SSD1306 OLED modules).
 *
 * @copyright That One Seong, 2024
 * @copyright GNU Lesser General Public License
 */

// we're using our own splash screen kthx ada
#define SSD1306_NO_SPLASH

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Wire.h>
#include "SamcoDisplay.h"
#include "SamcoPreferences.h"
#include <TinyUSB_Devices.h>

bool ExtDisplay::Begin()
{
    if(display != nullptr) {
        delete display;
        display = nullptr;
    }

    // TODO: for some reason, doing this AFTER saving updated pins settings (even when doing it from defaults and there's no default mappings for peripheral pins)
    // causes the board to hang. Even though this is all correct (and any display objects should get deleted from the above, so don't think it can be a new object thing)...
    if(SamcoPreferences::pins[OF_Const::periphSCL] >= 0 && SamcoPreferences::pins[OF_Const::periphSDA] >= 0) {
        if(bitRead(SamcoPreferences::pins[OF_Const::periphSCL], 1) && bitRead(SamcoPreferences::pins[OF_Const::periphSDA], 1)) {
            // I2C1
            if(bitRead(SamcoPreferences::pins[OF_Const::periphSCL], 0) && !bitRead(SamcoPreferences::pins[OF_Const::periphSDA], 0)) {
                Wire1.end();
                // SDA/SCL are indeed on verified correct pins
                Wire1.setSDA(SamcoPreferences::pins[OF_Const::periphSDA]);
                Wire1.setSCL(SamcoPreferences::pins[OF_Const::periphSCL]);
                display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, -1);
            } else return false;
        } else if(!bitRead(SamcoPreferences::pins[OF_Const::periphSCL], 1) && !bitRead(SamcoPreferences::pins[OF_Const::periphSDA], 1)) {
            // I2C0
            if(bitRead(SamcoPreferences::pins[OF_Const::periphSCL], 0) && !bitRead(SamcoPreferences::pins[OF_Const::periphSDA], 0)) {
                Wire.end();
                // SDA/SCL are indeed on verified correct pins
                Wire.setSDA(SamcoPreferences::pins[OF_Const::periphSDA]);
                Wire.setSCL(SamcoPreferences::pins[OF_Const::periphSCL]);
                display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
            } else return false;
        } else return false;
    } else return false;

    if(display->begin(SSD1306_SWITCHCAPVCC, SamcoPreferences::oledPrefs[OF_Const::oledAltAddr] ? 0x3C : 0x3D)) {
        display->clearDisplay();
        ScreenModeChange(Screen_None);
        return true;
    } else return false;
}

void ExtDisplay::Stop()
{
    if(display != nullptr) {
        delete display;
        display = nullptr;
    }
}

void ExtDisplay::TopPanelUpdate(const char *textPrefix, const char *profText)
{
    if(display != nullptr) {
        display->fillRect(0, 0, 128, 16, BLACK);
        display->drawFastHLine(0, 15, 128, WHITE);
        display->setCursor(2, 2);
        display->setTextSize(1);
        display->setTextColor(WHITE, BLACK);
        display->print(textPrefix);
        if(profText != nullptr)
            display->println(profText);
        display->display();
    }
}

void ExtDisplay::ScreenModeChange(int8_t screenMode, bool isAnalog)
{
    if(display != nullptr) {
        display->fillRect(0, 16, 128, 48, BLACK);
        if(screenState >= Screen_Mamehook_Single &&
           screenMode == Screen_Normal) {
            currentAmmo = 0, currentLife = 0;
        }
        screenState = screenMode;
        display->setTextColor(WHITE, BLACK);
        switch(screenMode) {
          case Screen_Normal:
            if(mister) display->drawBitmap(48, 23, misterIco, MISTERKUN_WIDTH, MISTERKUN_HEIGHT, WHITE);
            else {
                if(TinyUSBDevices.onBattery) { display->drawBitmap(2, 46, btConnectIco, CONNECTION_WIDTH, CONNECTION_HEIGHT, WHITE); }
                else { display->drawBitmap(2, 46, usbConnectIco, CONNECTION_WIDTH, CONNECTION_HEIGHT, WHITE); }
                if(isAnalog) { display->drawBitmap(108, 49, gamepadIco, GAMEPAD_WIDTH, GAMEPAD_HEIGHT, WHITE); }
                else { display->drawBitmap(109, 48, mouseIco, MOUSE_WIDTH, MOUSE_HEIGHT, WHITE); }
            }
            break;
          case Screen_None:
          case Screen_Docked:
            display->fillRect(0, 0, 128, 16, BLACK);
            display->drawBitmap(24, 0, customSplashBanner, CUSTSPLASHBANN_WIDTH, CUSTSPLASHBANN_HEIGHT, WHITE);
            display->drawBitmap(40, 16, customSplash, CUSTSPLASH_WIDTH, CUSTSPLASH_HEIGHT, WHITE);
            display->display();
            break;
          case Screen_Init:
            display->setTextSize(2);
            display->setCursor(20, 18);
            display->println("Welcome!");
            display->setTextSize(1);
            display->setCursor(12, 40);
            display->println(" Pull trigger to");
            display->setCursor(12, 52);
            display->println("start calibration!");
            break;
          case Screen_IRTest:
            TopPanelUpdate("", "IR Test");
            break;
          case Screen_Saving:
            TopPanelUpdate("", "Saving Profiles");
            display->setTextSize(2);
            display->setCursor(16, 18);
            display->println("Saving...");
            break;
          case Screen_SaveSuccess:
            display->setTextSize(2);
            display->setCursor(30, 18);
            display->println("Save");
            display->setCursor(4, 40);
            display->println("successful");
            break;
          case Screen_SaveError:
            display->setTextSize(2);
            display->setCursor(30, 18);
            display->setTextColor(BLACK, WHITE);
            display->println("Save");
            display->setCursor(22, 40);
            display->println("failed");
            break;
          case Screen_Mamehook_Single:
            if(TinyUSBDevices.onBattery) { display->drawBitmap(2, 46, btConnectIco, CONNECTION_WIDTH, CONNECTION_HEIGHT, WHITE); }
            else { display->drawBitmap(2, 46, usbConnectIco, CONNECTION_WIDTH, CONNECTION_HEIGHT, WHITE); }
            if(isAnalog) { display->drawBitmap(108, 49, gamepadIco, GAMEPAD_WIDTH, GAMEPAD_HEIGHT, WHITE); }
            else { display->drawBitmap(109, 48, mouseIco, MOUSE_WIDTH, MOUSE_HEIGHT, WHITE); }
            if(serialDisplayType == ScreenSerial_Life && lifeBar) {
              display->drawBitmap(52, 23, lifeBarBanner, LIFEBAR_BANNER_WIDTH, LIFEBAR_BANNER_HEIGHT, WHITE);
              display->drawBitmap(11, 35, lifeBarLarge, LIFEBAR_LARGE_WIDTH, LIFEBAR_LARGE_HEIGHT, WHITE);
              PrintLife(currentLife);
            } else if(serialDisplayType == ScreenSerial_Ammo) {
              PrintAmmo(currentAmmo);
            } else {
              display->drawBitmap(0, 17, mamehookIco, MAMEHOOK_WIDTH, MAMEHOOK_HEIGHT, WHITE);
            }
            break;
          case Screen_Mamehook_Dual:
            display->drawBitmap(63, 16, dividerLine, DIVIDER_WIDTH, DIVIDER_HEIGHT, WHITE);
            if(lifeBar) {
              display->drawBitmap(20, 23, lifeBarBanner, LIFEBAR_BANNER_WIDTH, LIFEBAR_BANNER_HEIGHT, WHITE);
              display->drawBitmap(2, 37, lifeBarSmall, LIFEBAR_SMALL_WIDTH, LIFEBAR_SMALL_HEIGHT, WHITE);
            }
            PrintAmmo(currentAmmo);
            PrintLife(currentLife);
            break;
        }
        display->display();
    }
}

void ExtDisplay::IdleOps()
{
    if(display != nullptr) {
        switch(screenState) {
          case Screen_Normal:
            break;
          case Screen_Pause:
            break;
          case Screen_Profile:
            break;
          case Screen_Saving:
            break;
          case Screen_Calibrating:
            break;
          case Screen_Mamehook_Single:
            break;
          case Screen_Mamehook_Dual:
            break;
        }
    }
}

// Warning: SLOOOOW, should only be used in cali/where the mouse isn't being updated.
// Use at your own discression.
void ExtDisplay::DrawVisibleIR(int pointX[4], int pointY[4])
{
    if(display != nullptr) {
        display->fillRect(0, 16, 128, 48, BLACK);
        for(uint8_t i = 0; i < 4; i++) {
          pointX[i] = map(pointX[i], 0, 1920, 0, 128);
          pointY[i] = map(pointY[i], 0, 1080, 16, 64);
          pointY[i] = constrain(pointY[i], 16, 64);
          display->fillCircle(pointX[i], pointY[i], 1, WHITE);
        }
        display->display();
    }
}

void ExtDisplay::PauseScreenShow(uint8_t currentProf, char name1[16], char name2[16], char name3[16], char name4[16])
{
    if(display != nullptr) {
        char* namesList[16] = { name1, name2, name3, name4 };
        TopPanelUpdate("Using ", namesList[currentProf]); // names are placeholder
        display->fillRect(0, 16, 128, 48, BLACK);
        display->setTextSize(1);
        display->setCursor(0, 17);
        display->print(" A > ");
        display->println(name1);
        display->setCursor(0, 17+11);
        display->print(" B > ");
        display->println(name2);
        display->setCursor(0, 17+(11*2));
        display->print("Str> ");
        display->println(name3);
        display->setCursor(0, 17+(11*3));
        display->print("Sel> ");
        display->println(name4);
        display->display();
    }
}

void ExtDisplay::PauseListUpdate(uint8_t selection)
{
    if(display != nullptr) {
        display->fillRect(0, 16, 128, 48, BLACK);
        display->drawBitmap(60, 18, upArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
        display->drawBitmap(60, 59, downArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
        display->setTextSize(1);
        // Seong Note: Yeah, some of these are pretty out-of-bounds-esque behavior,
        // but pause mode selection in actual use would prevent some of these extremes from happening.
        // Just covering our asses.
        switch(selection) {
          case ScreenPause_Calibrate:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 25);
            display->println(" Send Escape Keypress");
            display->setTextColor(BLACK, WHITE);
            display->setCursor(0, 36);
            display->println(" Calibrate ");
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 47);
            display->println(" Profile Select ");
            break;
          case ScreenPause_ProfileSelect:
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
          case ScreenPause_Save:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 25);
            display->println(" Profile Select ");
            display->setTextColor(BLACK, WHITE);
            display->setCursor(0, 36);
            display->println(" Save Gun Settings ");
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 47);
            if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0 && SamcoPreferences::pins[OF_Const::rumbleSwitch] == -1) {
              display->println(" Rumble Toggle ");
            } else if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0 && SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1) {
              display->println(" Solenoid Toggle ");
            } else {
              display->println(" Send Escape Keypress");
            }
            break;
          case ScreenPause_Rumble:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 25);
            display->println(" Save Gun Settings ");
            display->setTextColor(BLACK, WHITE);
            display->setCursor(0, 36);
            if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0 && SamcoPreferences::pins[OF_Const::rumbleSwitch] == -1) {
              display->println(" Rumble Toggle ");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0 && SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1) {
                display->println(" Solenoid Toggle ");
              } else {
                display->println(" Send Escape Keypress");
              }
            } else if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0 && SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1) {
              display->println(" Solenoid Toggle ");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Send Escape Keypress");
            } else {
              display->println(" Send Escape Keypress");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Calibrate ");
            }
            break;
          case ScreenPause_Solenoid:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 25);
            if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0 && SamcoPreferences::pins[OF_Const::rumbleSwitch] == -1) {
              display->println(" Rumble Toggle ");
              display->setTextColor(BLACK, WHITE);
              display->setCursor(0, 36);
              if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0 && SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1) {
                display->println(" Solenoid Toggle ");
                display->setTextColor(WHITE, BLACK);
                display->setCursor(0, 47);
                display->println(" Send Escape Keypress");
              } else {
                display->println(" Send Escape Keypress");
                display->setTextColor(WHITE, BLACK);
                display->setCursor(0, 47);
                display->println("Calibrate");
              }
            } else if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0 && SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1) {
              display->println(" Save Gun Settings");
              display->setTextColor(BLACK, WHITE);
              display->setCursor(0, 36);
              display->println(" Solenoid Toggle ");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Send Escape Keypress");
            } else {
              display->println(" Send Escape Keypress");
              display->setTextColor(BLACK, WHITE);
              display->setCursor(0, 36);
              display->println(" Calibrate ");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Profile Select ");
            }
            break;
          case ScreenPause_EscapeKey:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(0, 25);
            if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0 && SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1) {
              display->println(" Solenoid Toggle ");
              display->setTextColor(BLACK, WHITE);
              display->setCursor(0, 36);
              display->println(" Send Escape Keypress");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Calibrate ");
            } else if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0 && SamcoPreferences::pins[OF_Const::rumbleSwitch] == -1) {
              display->println(" Rumble Toggle ");
              display->setTextColor(BLACK, WHITE);
              display->setCursor(0, 36);
              display->println(" Send Escape Keypress");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Calibrate ");
            } else {
              display->println(" Save Gun Settings ");
              display->setTextColor(BLACK, WHITE);
              display->setCursor(0, 36);
              display->println(" Send Escape Keypress");
              display->setTextColor(WHITE, BLACK);
              display->setCursor(0, 47);
              display->println(" Calibrate ");
            }
            break;
        }
        display->display();
    }
}

void ExtDisplay::PauseProfileUpdate(uint8_t selection, char name1[16], char name2[16], char name3[16], char name4[16])
{
    if(display != nullptr) {
        display->fillRect(0, 16, 128, 48, BLACK);
        display->drawBitmap(60, 18, upArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
        display->drawBitmap(60, 59, downArrowGlyph, ARROW_WIDTH, ARROW_HEIGHT, WHITE);
        display->setTextSize(1);
        switch(selection) {
          case 0: // Profile #0, etc.
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 25);
            display->println(name4);
            display->setTextColor(BLACK, WHITE);
            display->setCursor(4, 36);
            display->println(name1);
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 47);
            display->println(name2);
            break;
          case 1:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 25);
            display->println(name1);
            display->setTextColor(BLACK, WHITE);
            display->setCursor(4, 36);
            display->println(name2);
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 47);
            display->println(name3);
            break;
          case 2:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 25);
            display->println(name2);
            display->setTextColor(BLACK, WHITE);
            display->setCursor(4, 36);
            display->println(name3);
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 47);
            display->println(name4);
            break;
          case 3:
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 25);
            display->println(name3);
            display->setTextColor(BLACK, WHITE);
            display->setCursor(4, 36);
            display->println(name4);
            display->setTextColor(WHITE, BLACK);
            display->setCursor(4, 47);
            display->println(name1);
            break;
        }
        display->display();
    }
}

void ExtDisplay::SaveScreen(uint8_t status)
{
    if(display != nullptr) {
        display->fillRect(0, 16, 128, 48, BLACK);
        display->setTextColor(WHITE, BLACK);
        display->setTextSize(2);
        display->setCursor(24, 24);
        display->println("Saving...");
        display->display();
    }
}

void ExtDisplay::PrintAmmo(uint8_t ammo)
{
    if(display != nullptr) {
        currentAmmo = ammo;

        // use the rounding error to get the left & right digits
        uint8_t ammoLeft = ammo / 10;
        uint8_t ammoRight = ammo - ammoLeft * 10;

        if(!ammo) { ammoEmpty = true; } else { ammoEmpty = false; }

        if(screenState == Screen_Mamehook_Single) {
            display->fillRect(40, 22, (NUMBER_GLYPH_WIDTH*2)+6, NUMBER_GLYPH_HEIGHT, BLACK);
            display->drawBitmap(40,                      22, numbers[ammoLeft],  NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
            display->drawBitmap(40+6+NUMBER_GLYPH_WIDTH, 22, numbers[ammoRight], NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
        } else if(screenState == Screen_Mamehook_Dual) {
            display->fillRect(72, 22, (NUMBER_GLYPH_WIDTH*2)+6, NUMBER_GLYPH_HEIGHT, BLACK);
            display->drawBitmap(72,                      22, numbers[ammoLeft],  NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
            display->drawBitmap(72+6+NUMBER_GLYPH_WIDTH, 22, numbers[ammoRight], NUMBER_GLYPH_WIDTH, NUMBER_GLYPH_HEIGHT, WHITE);
        }

        display->display();
    }
}

void ExtDisplay::PrintLife(uint8_t life)
{
    if(display != nullptr) {
        currentLife = life;
        if(!life) { lifeEmpty = true; } else { lifeEmpty = false; }
        if(screenState == Screen_Mamehook_Single) {
            if(lifeBar) {
                display->fillRect(14, 37, 100, 9, BLACK);
                display->fillRect(52, 51, 30, 8, BLACK);
                display->fillRect(14, 37, life, 9, WHITE);

                if(life) {
                  display->setTextSize(1);
                  display->setCursor(52, 51);
                  display->setTextColor(WHITE, BLACK);
                  display->print(life);
                  display->println(" %");
                }

                display->display();
            } else {
                display->fillRect(22, 19, HEART_LARGE_WIDTH*5+4, HEART_LARGE_HEIGHT+22+HEART_LARGE_HEIGHT, BLACK);
                switch(life) {
                  case 9:
                    display->drawBitmap(22, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+1+HEART_LARGE_WIDTH, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+2+HEART_LARGE_WIDTH*2, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+3+HEART_LARGE_WIDTH*3, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+4+HEART_LARGE_WIDTH*4, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);

                    display->drawBitmap(30, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(30+1+HEART_LARGE_WIDTH, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(30+2+HEART_LARGE_WIDTH*2, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(30+3+HEART_LARGE_WIDTH*3, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 8:
                    display->drawBitmap(22, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+1+HEART_LARGE_WIDTH, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+2+HEART_LARGE_WIDTH*2, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+3+HEART_LARGE_WIDTH*3, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+4+HEART_LARGE_WIDTH*4, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);

                    display->drawBitmap(39, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(39+1+HEART_LARGE_WIDTH, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(39+2+HEART_LARGE_WIDTH*2, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 7:
                    display->drawBitmap(22, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+1+HEART_LARGE_WIDTH, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+2+HEART_LARGE_WIDTH*2, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+3+HEART_LARGE_WIDTH*3, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+4+HEART_LARGE_WIDTH*4, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);

                    display->drawBitmap(48, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(48+1+HEART_LARGE_WIDTH, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 6:
                    display->drawBitmap(22, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+1+HEART_LARGE_WIDTH, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+2+HEART_LARGE_WIDTH*2, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+3+HEART_LARGE_WIDTH*3, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+4+HEART_LARGE_WIDTH*4, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);

                    display->drawBitmap(56, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 5:
                    display->drawBitmap(22, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+HEART_LARGE_WIDTH, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+HEART_LARGE_WIDTH*2, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+HEART_LARGE_WIDTH*3, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+HEART_LARGE_WIDTH*4, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 4:
                    display->drawBitmap(30, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(30+1+HEART_LARGE_WIDTH, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(30+2+HEART_LARGE_WIDTH*2, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(30+3+HEART_LARGE_WIDTH*3, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 3:
                    display->drawBitmap(39, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(39+1+HEART_LARGE_WIDTH, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(39+2+HEART_LARGE_WIDTH*2, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 2:
                    display->drawBitmap(48, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(48+1+HEART_LARGE_WIDTH, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 1:
                    display->drawBitmap(56, 30, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                  case 0:
                    break;
                  default: // 10+
                    display->drawBitmap(22, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+1+HEART_LARGE_WIDTH, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+2+HEART_LARGE_WIDTH*2, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+3+HEART_LARGE_WIDTH*3, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+4+HEART_LARGE_WIDTH*4, 19, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);

                    display->drawBitmap(22, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+1+HEART_LARGE_WIDTH, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+2+HEART_LARGE_WIDTH*2, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+3+HEART_LARGE_WIDTH*3, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    display->drawBitmap(22+4+HEART_LARGE_WIDTH*4, 41, lifeIcoLarge, HEART_LARGE_WIDTH, HEART_LARGE_HEIGHT, WHITE);
                    break;
                }
                display->display();
            }
        } else if(screenState == Screen_Mamehook_Dual) {
            if(lifeBar) {
                display->fillRect(4, 39, 55, 5, BLACK);
                display->fillRect(20, 51, 30, 8, BLACK);
                display->fillRect(4, 39, map(life, 0, 100, 0, 55), 5, WHITE);

                if(life) {
                  display->setTextSize(1);
                  display->setCursor(20, 51);
                  display->setTextColor(WHITE, BLACK);
                  display->print(life);
                  display->println(" %");
                }

                display->display();
            } else {
                display->fillRect(1, 22, HEART_SMALL_WIDTH*5, HEART_SMALL_HEIGHT+20+HEART_SMALL_HEIGHT, BLACK);
                switch(life) {
                  case 9:
                    display->drawBitmap(1, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);

                    display->drawBitmap(7, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(7+HEART_SMALL_WIDTH, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(7+HEART_SMALL_WIDTH*2, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(7+HEART_SMALL_WIDTH*3, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 8:
                    display->drawBitmap(1, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);

                    display->drawBitmap(13, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(13+HEART_SMALL_WIDTH, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(13+HEART_SMALL_WIDTH*2, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 7:
                    display->drawBitmap(1, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);

                    display->drawBitmap(19, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(19+HEART_SMALL_WIDTH, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 6:
                    display->drawBitmap(1, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);

                    display->drawBitmap(25, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 5:
                    display->drawBitmap(1, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 4:
                    display->drawBitmap(7, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(7+HEART_SMALL_WIDTH, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(7+HEART_SMALL_WIDTH*2, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(7+HEART_SMALL_WIDTH*3, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 3:
                    display->drawBitmap(13, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(13+HEART_SMALL_WIDTH, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(13+HEART_SMALL_WIDTH*2, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 2:
                    display->drawBitmap(19, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(19+HEART_SMALL_WIDTH, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 1:
                    display->drawBitmap(25, 32, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                  case 0:
                    break;
                  default: // 10+
                    display->drawBitmap(1, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 22, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);

                    display->drawBitmap(1, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*2, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*3, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    display->drawBitmap(1+HEART_SMALL_WIDTH*4, 42, lifeIcoSmall, HEART_SMALL_WIDTH, HEART_SMALL_HEIGHT, WHITE);
                    break;
                }
                display->display();
            }
        }
    }
}
