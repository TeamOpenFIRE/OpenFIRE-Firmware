// Tests for OpenFIRElights: the RGB LEDs (the board's NeoPixel and plain LED, the four-pin LED, and an external
// NeoPixel strand), against the fake pins and strands.
#include "framework.h"

#include "firmware_support.h"
#include "OpenFIREcommon.h"

#define private public
#include "OpenFIRElights.h"
#undef private

namespace {

constexpr int RED_PIN = 10, GREEN_PIN = 11, BLUE_PIN = 12, STRAND_PIN = 13;

void setUp() {
    firmware::restorePowerUpState();
    OF_Prefs::pins[OF_Const::ledR] = RED_PIN;
    OF_Prefs::pins[OF_Const::ledG] = GREEN_PIN;
    OF_Prefs::pins[OF_Const::ledB] = BLUE_PIN;
    OF_Prefs::toggles[OF_Const::commonAnode] = false;
    OF_Prefs::toggles[OF_Const::invertStaticPixels] = false;
    OF_Prefs::settings[OF_Const::customLEDcount] = 8;
    OF_Prefs::settings[OF_Const::customLEDstatic] = 0;
    FW_Common::ledIsValid = true;
    delete OF_RGB::externPixel;
    OF_RGB::externPixel = nullptr;
    OF_RGB::neopixel = Adafruit_NeoPixel(1, 16, NEO_GRB + NEO_KHZ800);
}

// The colour of the board's NeoPixel, as it was last sent out.
uint32_t boardPixel() { return OF_RGB::neopixel.shown.empty() ? 0xFFFFFFFF : OF_RGB::neopixel.shown[0]; }

} // namespace

TEST(Lights, InvertFlipsEveryBit) {
    CHECK_EQ(OF_RGB::Invert(0), 255);
    CHECK_EQ(OF_RGB::Invert(255), 0);
    CHECK_EQ(OF_RGB::Invert(0x0F), 0xF0);
}

TEST(Lights, InitStartsTheBoardsLedsAndShowsRed) {
    setUp();
    OF_RGB::LedInit();
    CHECK_EQ(testPinMode[PIN_LED], OUTPUT);             // the Pico's plain LED
    CHECK_EQ(testPinMode[NEOPIXEL_POWER], OUTPUT);      // a NeoPixel with a power pin: switched on
    CHECK_EQ(testPinOut[NEOPIXEL_POWER], HIGH);
    CHECK(OF_RGB::neopixel.began);
    CHECK_EQ(boardPixel(), 0xFF0000);                   // red during start-up
    CHECK_EQ(testPinOut[PIN_LED], HIGH);
    CHECK_EQ(testAnalogOut[RED_PIN], 255);
    CHECK_EQ(testAnalogOut[GREEN_PIN], 0);
    CHECK_EQ(testAnalogOut[BLUE_PIN], 0);
}

TEST(Lights, PackedColorGoesToEveryLed) {
    setUp();
    OF_RGB::SetLedPackedColor(0x123456);
    CHECK_EQ(boardPixel(), 0x123456);
    CHECK_EQ(testAnalogOut[RED_PIN], 0x12);
    CHECK_EQ(testAnalogOut[GREEN_PIN], 0x34);
    CHECK_EQ(testAnalogOut[BLUE_PIN], 0x56);
}

TEST(Lights, SeparateValuesGoToEveryLed) {
    setUp();
    OF_RGB::LedUpdate(10, 20, 30);
    CHECK_EQ(boardPixel(), 0x0A141E);
    CHECK_EQ(testAnalogOut[RED_PIN], 10);
    CHECK_EQ(testAnalogOut[GREEN_PIN], 20);
    CHECK_EQ(testAnalogOut[BLUE_PIN], 30);
}

TEST(Lights, TheFourPinLedIsInvertedForACommonAnode) {
    setUp();
    OF_Prefs::toggles[OF_Const::commonAnode] = true;
    OF_RGB::LedUpdate(10, 20, 30);
    CHECK_EQ(testAnalogOut[RED_PIN], 245);
    CHECK_EQ(testAnalogOut[GREEN_PIN], 235);
    CHECK_EQ(testAnalogOut[BLUE_PIN], 225);
    OF_RGB::SetLedPackedColor(0x0A141E);
    CHECK_EQ(testAnalogOut[RED_PIN], 245);
    CHECK_EQ(testAnalogOut[GREEN_PIN], 235);
    CHECK_EQ(testAnalogOut[BLUE_PIN], 225);

    // The board's NeoPixel doesn't care about the anode.
    CHECK_EQ(boardPixel(), 0x0A141E);
}

TEST(Lights, TheFourPinLedIsLeftAloneWhenItsPinsAreNotMapped) {
    setUp();
    FW_Common::ledIsValid = false;
    OF_RGB::LedUpdate(10, 20, 30);
    OF_RGB::SetLedPackedColor(0x010203);
    CHECK_EQ(testAnalogOut[RED_PIN], 0);
    CHECK_EQ(testAnalogOut[GREEN_PIN], 0);
    CHECK_EQ(testAnalogOut[BLUE_PIN], 0);
    CHECK_EQ(boardPixel(), 0x010203);
}

TEST(Lights, ThePicosPlainLedIsOnForAnythingBrightEnough) {
    setUp();
    OF_RGB::LedUpdate(99, 99, 99);
    CHECK_EQ(testPinOut[PIN_LED], LOW);
    OF_RGB::LedUpdate(100, 0, 0);
    CHECK_EQ(testPinOut[PIN_LED], HIGH);
    OF_RGB::LedUpdate(0, 100, 0);
    CHECK_EQ(testPinOut[PIN_LED], HIGH);
    OF_RGB::LedUpdate(0, 0, 100);
    CHECK_EQ(testPinOut[PIN_LED], HIGH);
    OF_RGB::SetLedPackedColor(0x636363);
    CHECK_EQ(testPinOut[PIN_LED], LOW);
    OF_RGB::SetLedPackedColor(0x640000);
    CHECK_EQ(testPinOut[PIN_LED], HIGH);
}

TEST(Lights, OffIsBlack) {
    setUp();
    OF_RGB::LedUpdate(255, 255, 255);
    OF_RGB::LedOff();
    CHECK_EQ(boardPixel(), 0);
    CHECK_EQ(testAnalogOut[RED_PIN], 0);
    CHECK_EQ(testPinOut[PIN_LED], LOW);
}

TEST(Lights, TheExternalStrandIsMadeWithTheConfiguredLength) {
    setUp();
    OF_Prefs::settings[OF_Const::customLEDcount] = 12;
    OF_RGB::InitExternPixel(STRAND_PIN);
    REQUIRE(OF_RGB::externPixel != nullptr);
    CHECK_EQ(OF_RGB::externPixel->numPixels(), 12);
    CHECK_EQ(OF_RGB::externPixel->pin, STRAND_PIN);
    CHECK(OF_RGB::externPixel->began);
    CHECK_EQ(OF_RGB::externPixel->shows, 0);        // no static pixels: nothing to show yet
}

TEST(Lights, StaticPixelsKeepTheirOwnColors) {
    setUp();
    OF_Prefs::settings[OF_Const::customLEDstatic] = 3;
    OF_Prefs::settings[OF_Const::customLEDcolor1] = 0x111111;
    OF_Prefs::settings[OF_Const::customLEDcolor2] = 0x222222;
    OF_Prefs::settings[OF_Const::customLEDcolor3] = 0x333333;
    OF_RGB::InitExternPixel(STRAND_PIN);
    const auto& pixels = OF_RGB::externPixel->shown;
    REQUIRE(pixels.size() == 8);
    CHECK_EQ(pixels[0], 0x111111);
    CHECK_EQ(pixels[1], 0x222222);
    CHECK_EQ(pixels[2], 0x333333);
    CHECK_EQ(pixels[3], 0);
}

TEST(Lights, StaticPixelsCanBeAtTheOtherEnd) {
    setUp();
    OF_Prefs::toggles[OF_Const::invertStaticPixels] = true;
    OF_Prefs::settings[OF_Const::customLEDstatic] = 2;
    OF_Prefs::settings[OF_Const::customLEDcolor1] = 0xAA0000;
    OF_Prefs::settings[OF_Const::customLEDcolor2] = 0x00BB00;
    OF_RGB::InitExternPixel(STRAND_PIN);
    const auto& pixels = OF_RGB::externPixel->shown;
    REQUIRE(pixels.size() == 8);
    CHECK_EQ(pixels[7], 0xAA0000);
    CHECK_EQ(pixels[6], 0x00BB00);
    CHECK_EQ(pixels[0], 0);
}

TEST(Lights, MoreStaticPixelsThanThePixelsThereAreIsIgnored) {
    setUp();
    OF_Prefs::settings[OF_Const::customLEDstatic] = 9;      // of 8
    OF_RGB::InitExternPixel(STRAND_PIN);
    CHECK_EQ(OF_RGB::externPixel->shows, 0);
}

TEST(Lights, StaticPixelsBeyondTheThirdAreOff) {
    // Only three colours can be set: the fourth and later static pixels are off.
    setUp();
    OF_Prefs::settings[OF_Const::customLEDstatic] = 5;
    OF_RGB::InitExternPixel(STRAND_PIN);
    CHECK_EQ(OF_RGB::externPixel->shown[3], 0);
    CHECK_EQ(OF_RGB::externPixel->shown[4], 0);
}

TEST(Lights, TheStrandShowsTheStatusColorOnTheRestOfItsPixels) {
    setUp();
    OF_Prefs::settings[OF_Const::customLEDstatic] = 2;
    OF_Prefs::settings[OF_Const::customLEDcolor1] = 0x111111;
    OF_Prefs::settings[OF_Const::customLEDcolor2] = 0x222222;
    OF_RGB::InitExternPixel(STRAND_PIN);

    OF_RGB::LedUpdate(1, 2, 3);
    const auto& pixels = OF_RGB::externPixel->shown;
    CHECK_EQ(pixels[0], 0x111111);      // static
    CHECK_EQ(pixels[1], 0x222222);
    for (int i = 2; i < 8; i++) CHECK_EQ(pixels[i], 0x010203);

    OF_RGB::SetLedPackedColor(0x445566);
    for (int i = 2; i < 8; i++) CHECK_EQ(OF_RGB::externPixel->shown[i], 0x445566);
    CHECK_EQ(OF_RGB::externPixel->shown[0], 0x111111);
}

TEST(Lights, TheStatusColorIsAtTheFrontWhenTheStaticPixelsAreAtTheBack) {
    setUp();
    OF_Prefs::toggles[OF_Const::invertStaticPixels] = true;
    OF_Prefs::settings[OF_Const::customLEDstatic] = 2;
    OF_Prefs::settings[OF_Const::customLEDcolor1] = 0x111111;
    OF_Prefs::settings[OF_Const::customLEDcolor2] = 0x222222;
    OF_RGB::InitExternPixel(STRAND_PIN);

    OF_RGB::LedUpdate(1, 2, 3);
    const auto& pixels = OF_RGB::externPixel->shown;
    for (int i = 0; i < 6; i++) CHECK_EQ(pixels[i], 0x010203);
    CHECK_EQ(pixels[6], 0x222222);
    CHECK_EQ(pixels[7], 0x111111);
}

TEST(Lights, AStrandOfOnlyStaticPixelsIsNeverOverwritten) {
    setUp();
    OF_Prefs::settings[OF_Const::customLEDcount] = 3;
    OF_Prefs::settings[OF_Const::customLEDstatic] = 3;
    OF_Prefs::settings[OF_Const::customLEDcolor1] = 0x111111;
    OF_Prefs::settings[OF_Const::customLEDcolor2] = 0x222222;
    OF_Prefs::settings[OF_Const::customLEDcolor3] = 0x333333;
    OF_RGB::InitExternPixel(STRAND_PIN);
    const int shows = OF_RGB::externPixel->shows;
    OF_RGB::LedUpdate(9, 9, 9);
    OF_RGB::SetLedPackedColor(0x999999);
    CHECK_EQ(OF_RGB::externPixel->shows, shows);
    CHECK_EQ(OF_RGB::externPixel->shown[0], 0x111111);
}

TEST(Lights, WithoutAStrandNothingIsSentToOne) {
    setUp();
    OF_RGB::LedUpdate(9, 9, 9);
    OF_RGB::SetLedPackedColor(0x999999);
    CHECK(OF_RGB::externPixel == nullptr);
}

TEST(Lights, TheDefaultColorsAreTheDocumentedOnes) {
    CHECK_EQ(OF_RGB::IRSeen0Color, WikiColor::Amber);
    CHECK_EQ(OF_RGB::CalModeColor, WikiColor::Red);
}
