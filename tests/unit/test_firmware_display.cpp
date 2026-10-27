// Tests for OpenFIREdisplay: the OLED screens. The display is the recording fake in ../stub/Adafruit_SSD1306.h, which
// keeps what was printed (and in which colours), the shapes and the bitmaps drawn.
#include "framework.h"

#include <algorithm>
#include <string>

#include "firmware_support.h"

#define private public
#include "OpenFIREdisplay.h"
#undef private
#include "OpenFIREFeedback.h"

namespace {

// I2C1 on GPIO 18 and 19, as the Pico's presets map them.
constexpr int SDA_PIN = 18, SCL_PIN = 19;

void setUp() {
    firmware::restorePowerUpState();
    OF_Prefs::pins[OF_Const::periphSDA] = SDA_PIN;
    OF_Prefs::pins[OF_Const::periphSCL] = SCL_PIN;
    OF_Prefs::pins[OF_Const::tempPin] = -1;
    OF_Prefs::pins[OF_Const::rumblePin] = -1;
    OF_Prefs::pins[OF_Const::solenoidPin] = -1;
    OF_Prefs::pins[OF_Const::rumbleSwitch] = -1;
    OF_Prefs::pins[OF_Const::solenoidSwitch] = -1;
    OF_Prefs::toggles[OF_Const::i2cOLEDaltAddr] = false;
    Adafruit_SSD1306::beginSucceedsByDefault = true;
    Wire = TwoWire();
    Wire1 = TwoWire();
    TinyUSBDevices.onBattery = false;
    OF_FFB::temperatureCurrent = 25;
}

// A display that has started.
struct Started {
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    Adafruit_SSD1306*& screen = oled.display;

    Started() {
        setUp();
        REQUIRE_STARTED();
    }
    void REQUIRE_STARTED() {
        if (!oled.Begin()) unit::fail(__FILE__, __LINE__, "the display did not start");
    }
    ~Started() { oled.Stop(); }
};

// The lines the display printed, with the highlighted one (black on white) marked.
std::string printedText(const Adafruit_SSD1306& screen) { return screen.text; }

const Adafruit_SSD1306::Line* highlightedLine(const Adafruit_SSD1306& screen) {
    const Adafruit_SSD1306::Line* found = nullptr;
    for (const auto& line : screen.lines) {
        if (Adafruit_SSD1306::highlighted(line)) found = &line;
    }
    return found;
}

bool mentions(const Adafruit_SSD1306& screen, const char* text) { return screen.text.find(text) != std::string::npos; }

// The text of the line printed highlighted, without its spaces around.
std::string highlightedText(const Adafruit_SSD1306& screen) {
    const auto* line = highlightedLine(screen);
    if (!line) return "";
    std::string text = line->text;
    text.erase(0, text.find_first_not_of(' '));
    text.erase(text.find_last_not_of(' ') + 1);
    return text;
}

} // namespace

TEST(Display, DoesNothingWithoutADisplay) {
    setUp();
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    int pointX[4] = {0, 1, 2, 3}, pointY[4] = {0, 1, 2, 3};
    oled.TopPanelUpdate("a", "b");
    oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    oled.IdleOps();
    oled.DrawVisibleIR(pointX, pointY);
    oled.PauseScreenShow(0, "a", "b", "c", "d");
    oled.PauseListUpdate(0);
    oled.PauseProfileUpdate(0, "a", "b", "c", "d");
    oled.SaveScreen(0);
    oled.PrintAmmo(5);
    oled.PrintLife(5);
    oled.Stop();
    CHECK(oled.display == nullptr);
    CHECK_EQ(pointX[1], 1);     // and the arrays were left as they were
}

TEST(Display, StartsOnTheI2cBusOfItsPins) {
    setUp();
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    CHECK(oled.Begin());
    CHECK(oled.display->wire == &Wire1);
    CHECK_EQ(Wire1.sdaPin, SDA_PIN);
    CHECK_EQ(Wire1.sclPin, SCL_PIN);
    CHECK_EQ(Wire1.ended, 1);
    CHECK_EQ(oled.display->beginVcc, SSD1306_SWITCHCAPVCC);
    CHECK_EQ(oled.display->beginAddress, 0x3C);
    oled.Stop();

    OF_Prefs::pins[OF_Const::periphSDA] = 4;
    OF_Prefs::pins[OF_Const::periphSCL] = 5;
    CHECK(oled.Begin());
    CHECK(oled.display->wire == &Wire);
    CHECK_EQ(Wire.sdaPin, 4);
    CHECK_EQ(Wire.sclPin, 5);
    oled.Stop();
}

TEST(Display, TheAlternateAddressIsUsedWhenSelected) {
    setUp();
    OF_Prefs::toggles[OF_Const::i2cOLEDaltAddr] = true;
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    CHECK(oled.Begin());
    CHECK_EQ(oled.display->beginAddress, 0x3D);
    oled.Stop();
}

TEST(Display, DoesNotStartOnPinsThatAreNotAnI2cPair) {
    setUp();
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    struct Pins { int sda, scl; };
    // Unmapped, a pin of each bus, the two swapped, two clocks, and two data pins.
    const Pins notPairs[] = {{-1, -1}, {-1, 19}, {18, -1}, {4, 19}, {18, 5}, {19, 18}, {5, 5}, {18, 18}, {4, 4}, {5, 4}};
    for (const Pins& pins : notPairs) {
        OF_Prefs::pins[OF_Const::periphSDA] = pins.sda;
        OF_Prefs::pins[OF_Const::periphSCL] = pins.scl;
        INFO("SDA %d, SCL %d", pins.sda, pins.scl);
        CHECK(!oled.Begin());
        CHECK(oled.display == nullptr);
    }
}

TEST(Display, DoesNotStartWhenTheScreenDoesNotAnswer) {
    setUp();
    Adafruit_SSD1306::beginSucceedsByDefault = false;
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    CHECK(!oled.Begin());
    oled.Stop();
}

TEST(Display, StartingAgainReplacesTheOldDisplay) {
    setUp();
    ExtDisplay oled{};      // zeroed, as the firmware's display is (it has static storage)
    CHECK(oled.Begin());
    Adafruit_SSD1306* first = oled.display;
    first->print("marker");
    CHECK(oled.Begin());
    CHECK(oled.display != nullptr);
    CHECK(!mentions(*oled.display, "marker"));          // a fresh display
    oled.Stop();
    CHECK(oled.display == nullptr);
    oled.Stop();                                        // stopping twice is fine
}

TEST(Display, StartingShowsTheSplashScreen) {
    Started started;
    CHECK_EQ(started.screen->bitmaps, 2);               // the banner and the logo
    CHECK(started.screen->displayed > 0);
}

TEST(Display, TheTopPanelShowsAPrefixAndAProfileName) {
    Started started;
    started.screen->clearDisplay();
    started.oled.TopPanelUpdate("Using ", "Profile A");
    CHECK_STREQ(printedText(*started.screen), "Using Profile A\n");
    CHECK_EQ(started.screen->cursorX, 2);
    CHECK_EQ(started.screen->cursorY, 2);

    started.screen->clearDisplay();
    started.oled.TopPanelUpdate("A text of its own");
    CHECK_STREQ(printedText(*started.screen), "A text of its own");
}

TEST(Display, EachScreenModeShowsItsOwnText) {
    struct Case { int mode; const char* text; };
    const Case cases[] = {
        {ExtDisplay::Screen_Init, "Welcome!"},
        {ExtDisplay::Screen_Init, "start calibration!"},
        {ExtDisplay::Screen_IRTest, "IR Test"},
        {ExtDisplay::Screen_Saving, "Saving Profiles"},
        {ExtDisplay::Screen_Saving, "Saving..."},
        {ExtDisplay::Screen_SaveSuccess, "successful"},
        {ExtDisplay::Screen_SaveError, "failed"},
    };
    for (const Case& test : cases) {
        Started started;
        started.screen->clearDisplay();
        started.oled.ScreenModeChange(test.mode);
        INFO("mode %d", test.mode);
        CHECK(mentions(*started.screen, test.text));
    }
}

TEST(Display, TheSaveErrorIsInvertedAndTheSuccessIsNot) {
    Started started;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_SaveError);
    CHECK(highlightedLine(*started.screen) != nullptr);
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_SaveSuccess);
    CHECK(highlightedLine(*started.screen) == nullptr);
}

TEST(Display, TheNormalScreenShowsTheConnectionAndTheControllerIcons) {
    Started started;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    CHECK_EQ(started.screen->bitmaps, 2);               // USB, and the mouse

    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal, true);
    CHECK_EQ(started.screen->bitmaps, 2);               // USB, and the gamepad
    CHECK_EQ(started.screen->bitmaps, 2);

    // Mister mode has a picture of its own.
    started.oled.mister = true;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    CHECK_EQ(started.screen->bitmaps, 1);
}

TEST(Display, TheConnectionIconTellsBluetoothFromUsb) {
    Started started;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    const int usbBitmaps = started.screen->bitmaps;
    TinyUSBDevices.onBattery = true;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    CHECK_EQ(started.screen->bitmaps, usbBitmaps);      // another icon in the same place
    TinyUSBDevices.onBattery = false;
}

TEST(Display, DockedShowsTheSplashAgain) {
    Started started;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Docked);
    CHECK_EQ(started.screen->bitmaps, 2);
}

TEST(Display, TemperatureIsShownInTheTopPanelFromTimeToTime) {
    Started started;
    OF_Prefs::pins[OF_Const::tempPin] = 28;
    OF_FFB::temperatureCurrent = 31;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    started.screen->clearDisplay();

    testMillis += 4000;
    started.oled.IdleOps();
    CHECK_STREQ(printedText(*started.screen), "");          // not yet

    testMillis += 1500;
    started.oled.IdleOps();
    CHECK(mentions(*started.screen, "Current Temp: 31 C"));

    // After the next interval, back to the profile's name.
    started.screen->clearDisplay();
    testMillis += 5100;
    started.oled.IdleOps();
    CHECK(mentions(*started.screen, "Prof: Profile A"));
}

TEST(Display, ChangedTemperatureIsUpdatedWhileItIsShown) {
    Started started;
    OF_Prefs::pins[OF_Const::tempPin] = 28;
    OF_FFB::temperatureCurrent = 31;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    testMillis += 5100;
    started.oled.IdleOps();
    started.screen->clearDisplay();

    OF_FFB::temperatureCurrent = 32;
    testMillis += 300;
    started.oled.IdleOps();
    CHECK_STREQ(printedText(*started.screen), "");          // a quarter second is not enough
    testMillis += 500;
    started.oled.IdleOps();
    CHECK(mentions(*started.screen, "32 C"));

    // Unchanged: not drawn again.
    started.screen->clearDisplay();
    testMillis += 800;
    started.oled.IdleOps();
    CHECK_STREQ(printedText(*started.screen), "");
}

TEST(Display, SingleDigitAndFaultTemperatures) {
    Started started;
    OF_Prefs::pins[OF_Const::tempPin] = 28;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);

    OF_FFB::temperatureCurrent = 7;
    started.screen->clearDisplay();
    testMillis += 5100;
    started.oled.IdleOps();
    CHECK(mentions(*started.screen, "7 C"));

    OF_FFB::temperatureCurrent = OF_Const::TEMPERATURE_SENSOR_ERROR_VALUE;
    started.oled.ShowTemp();
    CHECK(mentions(*started.screen, "Temp sensor: Fault!"));
}

TEST(Display, TemperatureIsNotShownWithoutASensorOrOnOtherScreens) {
    Started started;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    started.screen->clearDisplay();
    testMillis += 20000;
    started.oled.IdleOps();                                 // no sensor mapped
    CHECK_STREQ(printedText(*started.screen), "");

    OF_Prefs::pins[OF_Const::tempPin] = 28;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Pause);
    started.screen->clearDisplay();
    testMillis += 20000;
    started.oled.IdleOps();
    CHECK_STREQ(printedText(*started.screen), "");
}

TEST(Display, ATemperatureOfAHundredOrMoreIsShownAsNumbers) {
    Started started;
    for (unsigned temperature : {0u, 9u, 10u, 99u, 100u, 105u, 124u}) {
        OF_FFB::temperatureCurrent = temperature;
        started.screen->clearDisplay();
        started.oled.ShowTemp();
        INFO("%u degrees", temperature);
        CHECK(mentions(*started.screen, (std::to_string(temperature) + " C").c_str()));
    }
}

TEST(Display, TheIrPointsAreDrawnScaledToTheScreen) {
    Started started;
    int pointX[4] = {0, 1920, 960, 480};
    int pointY[4] = {0, 1080, 540, 5000};
    started.screen->clearDisplay();
    started.oled.DrawVisibleIR(pointX, pointY);
    REQUIRE(started.screen->circles.size() == 4);
    CHECK_EQ(started.screen->circles[0].x, 0);
    CHECK_EQ(started.screen->circles[1].x, 128);
    CHECK_EQ(started.screen->circles[2].x, 64);
    CHECK_EQ(started.screen->circles[3].x, 32);
    CHECK_EQ(started.screen->circles[0].y, 16);             // the screen's first rows belong to the top panel
    CHECK_EQ(started.screen->circles[1].y, 64);
    CHECK_EQ(started.screen->circles[2].y, 40);
    CHECK_EQ(started.screen->circles[3].y, 64);             // beyond the bottom: held at the last row

    // The arrays hold the screen positions afterwards.
    CHECK_EQ(pointX[1], 128);
    CHECK_EQ(pointY[3], 64);
}

TEST(Display, TheHotkeyPauseScreenListsTheProfiles) {
    Started started;
    started.screen->clearDisplay();
    started.oled.PauseScreenShow(1, "One", "Two", "Three", "Four");
    CHECK(mentions(*started.screen, "Using Two"));
    CHECK(mentions(*started.screen, " A > One"));
    CHECK(mentions(*started.screen, " B > Two"));
    CHECK(mentions(*started.screen, "Str> Three"));
    CHECK(mentions(*started.screen, "Sel> Four"));
}

TEST(Display, TheProfileListHighlightsTheSelectedAndShowsItsNeighbours) {
    Started started;
    struct Case { int selection; const char* above; const char* selected; const char* below; };
    const Case cases[] = {
        {0, "Four", "One", "Two"}, {1, "One", "Two", "Three"}, {2, "Two", "Three", "Four"}, {3, "Three", "Four", "One"},
    };
    for (const Case& test : cases) {
        started.screen->clearDisplay();
        started.oled.PauseProfileUpdate(test.selection, "One", "Two", "Three", "Four");
        INFO("selection %d", test.selection);
        CHECK_STREQ(highlightedText(*started.screen), test.selected);
        CHECK_STREQ(printedText(*started.screen), std::string(test.above) + "\n" + test.selected + "\n" + test.below + "\n");
    }

    // A selection that isn't one of the four shows no list.
    started.screen->clearDisplay();
    started.oled.PauseProfileUpdate(7, "One", "Two", "Three", "Four");
    CHECK_STREQ(printedText(*started.screen), "");
    CHECK_EQ(started.screen->bitmaps, 2);               // only the two arrows
}

TEST(Display, ThePauseListHighlightsTheSelectedEntryOfAGunWithoutFeedbackPins) {
    Started started;
    struct Case { int selection; const char* selected; };
    const Case cases[] = {
        {ExtDisplay::ScreenPause_Calibrate, "Calibrate"},
        {ExtDisplay::ScreenPause_ProfileSelect, "Profile Select"},
        {ExtDisplay::ScreenPause_Save, "Save Gun Settings"},
        {ExtDisplay::ScreenPause_Rumble, "Send Escape Keypress"},
        {ExtDisplay::ScreenPause_Solenoid, "Calibrate"},
        {ExtDisplay::ScreenPause_EscapeKey, "Send Escape Keypress"},
    };
    for (const Case& test : cases) {
        started.screen->clearDisplay();
        started.oled.PauseListUpdate(test.selection);
        INFO("selection %d", test.selection);
        CHECK_STREQ(highlightedText(*started.screen), test.selected);
        CHECK_EQ(started.screen->bitmaps, 2);           // the up and down arrows
    }
}

TEST(Display, ThePauseListOffersTheTogglesOfTheFeedbackThatIsWiredWithoutASwitch) {
    Started started;
    OF_Prefs::pins[OF_Const::rumblePin] = 17;
    OF_Prefs::pins[OF_Const::solenoidPin] = 16;
    started.screen->clearDisplay();
    started.oled.PauseListUpdate(ExtDisplay::ScreenPause_Rumble);
    CHECK_STREQ(highlightedText(*started.screen), "Rumble Toggle");
    CHECK(mentions(*started.screen, "Solenoid Toggle"));

    started.screen->clearDisplay();
    started.oled.PauseListUpdate(ExtDisplay::ScreenPause_Solenoid);
    CHECK_STREQ(highlightedText(*started.screen), "Solenoid Toggle");
    CHECK(mentions(*started.screen, "Rumble Toggle"));
    CHECK(mentions(*started.screen, "Send Escape Keypress"));

    started.screen->clearDisplay();
    started.oled.PauseListUpdate(ExtDisplay::ScreenPause_Save);
    CHECK_STREQ(highlightedText(*started.screen), "Save Gun Settings");
    CHECK(mentions(*started.screen, "Rumble Toggle"));
}

TEST(Display, ThePauseListLeavesOutATogglePairedWithAHardwareSwitch) {
    Started started;
    OF_Prefs::pins[OF_Const::rumblePin] = 17;
    OF_Prefs::pins[OF_Const::rumbleSwitch] = 3;         // the rumble has a switch
    OF_Prefs::pins[OF_Const::solenoidPin] = 16;
    for (int selection = 0; selection <= ExtDisplay::ScreenPause_EscapeKey; selection++) {
        started.screen->clearDisplay();
        started.oled.PauseListUpdate(selection);
        INFO("selection %d", selection);
        CHECK(!mentions(*started.screen, "Rumble Toggle"));
    }

    OF_Prefs::pins[OF_Const::solenoidSwitch] = 4;
    for (int selection = 0; selection <= ExtDisplay::ScreenPause_EscapeKey; selection++) {
        started.screen->clearDisplay();
        started.oled.PauseListUpdate(selection);
        INFO("selection %d", selection);
        CHECK(!mentions(*started.screen, "Solenoid Toggle"));
    }
}

TEST(Display, EveryPauseSelectionOfEveryPinCombinationHighlightsExactlyOneEntry) {
    // Whatever is wired, the menu shows one selected entry, and the selected one is in the middle.
    Started started;
    for (int rumble : {-1, 17}) {
        for (int rumbleSwitch : {-1, 3}) {
            for (int solenoid : {-1, 16}) {
                for (int solenoidSwitch : {-1, 4}) {
                    OF_Prefs::pins[OF_Const::rumblePin] = rumble;
                    OF_Prefs::pins[OF_Const::rumbleSwitch] = rumbleSwitch;
                    OF_Prefs::pins[OF_Const::solenoidPin] = solenoid;
                    OF_Prefs::pins[OF_Const::solenoidSwitch] = solenoidSwitch;
                    for (int selection = 0; selection <= ExtDisplay::ScreenPause_EscapeKey; selection++) {
                        started.screen->clearDisplay();
                        started.oled.PauseListUpdate(selection);
                        int highlighted = 0;
                        for (const auto& line : started.screen->lines) {
                            if (Adafruit_SSD1306::highlighted(line)) {
                                highlighted++;
                                CHECK_EQ(line.y, 36);
                            }
                        }
                        INFO("selection %d, rumble %d/%d, solenoid %d/%d", selection, rumble, rumbleSwitch, solenoid, solenoidSwitch);
                        CHECK_EQ(highlighted, 1);
                    }
                }
            }
        }
    }
}

TEST(Display, TheSaveScreenSaysSaving) {
    Started started;
    started.screen->clearDisplay();
    started.oled.SaveScreen(0);
    CHECK_STREQ(printedText(*started.screen), "Saving...\n");
}

TEST(Display, AmmoIsShownAsTwoDigitsOnTheSerialScreens) {
    for (int mode : {ExtDisplay::Screen_Mamehook_Single, ExtDisplay::Screen_Mamehook_Dual}) {
        Started started;
        started.oled.serialDisplayType = ExtDisplay::ScreenSerial_Ammo;
        started.oled.ScreenModeChange(mode);
        started.screen->clearDisplay();
        started.oled.PrintAmmo(42);
        INFO("mode %d", mode);
        CHECK_EQ(started.screen->bitmaps, 2);
        CHECK(started.oled.currentAmmo == 42);
        CHECK(!started.oled.ammoEmpty);
        started.oled.PrintAmmo(0);
        CHECK(started.oled.ammoEmpty);
    }

    // On any other screen only the number is remembered.
    Started started;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    started.screen->clearDisplay();
    started.oled.PrintAmmo(12);
    CHECK_EQ(started.screen->bitmaps, 0);
    CHECK(started.oled.currentAmmo == 12);
}

TEST(Display, LifeIsShownAsHeartsAndTheirCountFollowsTheLife) {
    for (int mode : {ExtDisplay::Screen_Mamehook_Single, ExtDisplay::Screen_Mamehook_Dual}) {
        Started started;
        started.oled.serialDisplayType = ExtDisplay::ScreenSerial_Life;
        started.oled.lifeBar = false;
        started.oled.ScreenModeChange(mode);
        for (int life = 0; life <= 12; life++) {
            started.screen->clearDisplay();
            started.oled.PrintLife(life);
            INFO("mode %d, life %d", mode, life);
            CHECK_EQ(started.screen->bitmaps, std::min(life, 10));
            CHECK_EQ(started.oled.lifeEmpty, life == 0);
        }
    }
}

TEST(Display, LifeAsABarFillsInProportionAndPrintsThePercentage) {
    Started started;
    started.oled.serialDisplayType = ExtDisplay::ScreenSerial_Life;
    started.oled.lifeBar = true;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single);
    started.screen->clearDisplay();
    started.oled.PrintLife(60);
    const auto& bar = started.screen->rects.back();
    CHECK_EQ(bar.w, 60);
    CHECK_EQ(bar.color, WHITE);
    CHECK_STREQ(printedText(*started.screen), "60 %\n");

    started.screen->clearDisplay();
    started.oled.PrintLife(0);
    CHECK_STREQ(printedText(*started.screen), "");       // an empty bar has no number

    // The dual screen has a shorter bar: 55 pixels for 100 percent.
    Started dual;
    dual.oled.lifeBar = true;
    dual.oled.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
    dual.screen->clearDisplay();
    dual.oled.PrintLife(100);
    CHECK_EQ(dual.screen->rects.back().w, 55);
    dual.screen->clearDisplay();
    dual.oled.PrintLife(50);
    CHECK_EQ(dual.screen->rects.back().w, 27);
    CHECK(mentions(*dual.screen, "50 %"));
}

TEST(Display, LeavingTheSerialScreensForTheNormalOneForgetsAmmoAndLife) {
    Started started;
    started.oled.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
    started.oled.PrintAmmo(30);
    started.oled.PrintLife(5);
    started.oled.ScreenModeChange(ExtDisplay::Screen_Normal);
    CHECK(started.oled.currentAmmo == 0);
    CHECK(started.oled.currentLife == 0);
}

TEST(Display, TheSerialScreenFallsBackToTheMameHookIcon) {
    Started started;
    started.oled.serialDisplayType = ExtDisplay::ScreenSerial_None;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single);
    CHECK_EQ(started.screen->bitmaps, 3);               // USB, the mouse, and the icon

    started.oled.serialDisplayType = ExtDisplay::ScreenSerial_Life;
    started.oled.lifeBar = true;
    started.screen->clearDisplay();
    started.oled.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single);
    CHECK_EQ(started.screen->bitmaps, 4);               // USB, the mouse, the life bar's banner and the bar itself
}
