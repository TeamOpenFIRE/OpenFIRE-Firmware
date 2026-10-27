// Tests for the firmware's mapping of the Perspective output to the screen with the calibration offsets
// (OpenFIREmain/OpenFIREcalibration.h), and for the run mode check in the firmware's constants.
#include "framework.h"

#include <cstdint>
#include <cstdlib>

#include <OpenFIREcalibration.h>
#include <OpenFIREconstant.h>

namespace {

// What the mapping is meant to compute, with room for the products: the screen axis' range stretched from the
// calibrated readings to 0..res.
int64_t reference(int64_t value, int64_t res, int64_t lowOffset, int64_t highOffset) {
    return ((value - lowOffset) * res) / (res - lowOffset - highOffset);
}

} // namespace

TEST(Calibration, WithoutOffsetsTheValueIsKept) {
    for (int32_t value : {-5000, -1, 0, 1, 960, res_x / 2, res_x, res_x + 700, 123456}) {
        CHECK_EQ(MapCalibratedAxis(value, res_x, 0, 0), value);
        CHECK_EQ(MapCalibratedAxis(value, res_y, 0, 0), value);
    }
}

TEST(Calibration, TheCalibratedEdgesLandExactlyOnTheScreenEdges) {
    // The offsets are the readings taken while aiming at the edges: the left/top reading, and the screen size minus
    // the right/bottom reading. Those readings must come out as exactly 0 and the screen size.
    struct Offsets { int32_t low, high; };
    const Offsets calibrations[] = {{0, 0}, {120, 80}, {-200, 150}, {300, -250}, {-res_y / 4, -res_y / 4}, {res_y / 4, res_y / 4}};   // valid for both axes
    for (const Offsets& calibration : calibrations) {
        INFO("offsets %d %d", (int)calibration.low, (int)calibration.high);
        CHECK_EQ(MapCalibratedAxis(calibration.low, res_x, calibration.low, calibration.high), 0);
        CHECK_EQ(MapCalibratedAxis(res_x - calibration.high, res_x, calibration.low, calibration.high), res_x);
        CHECK_EQ(MapCalibratedAxis(calibration.low, res_y, calibration.low, calibration.high), 0);
        CHECK_EQ(MapCalibratedAxis(res_y - calibration.high, res_y, calibration.low, calibration.high), res_y);
    }
}

TEST(Calibration, TheMiddleOfTheCalibratedRangeIsTheMiddleOfTheScreen) {
    const int32_t low = 400, high = 600;
    const int32_t middle = (low + (res_x - high)) / 2;
    CHECK_NEAR(MapCalibratedAxis(middle, res_x, low, high), res_x / 2, 1);
}

TEST(Calibration, MapsLikeTheReferenceFormulaAndNeverOverflows) {
    // Across the whole legal range of offsets and values, including the largest values that are let through.
    // A 32-bit overflow would show as a difference from the 64-bit reference.
    const int32_t offsetsX[] = {-res_x, -res_x / 2, -1, 0, 1, res_x / 3, res_x / 2, res_x};
    const int32_t values[] = {-CALI_MAP_VALUE_LIMIT, -res_x, -1, 0, 1, res_x / 2, res_x, CALI_MAP_VALUE_LIMIT};
    int mapped = 0;
    for (int32_t low : offsetsX) {
        for (int32_t high : offsetsX) {
            const int32_t span = res_x - low - high;
            if (span < res_x / 4) continue;     // a damaged calibration: tested below
            for (int32_t value : values) {
                INFO("value %d, offsets %d %d", (int)value, (int)low, (int)high);
                CHECK_EQ(MapCalibratedAxis(value, res_x, low, high), reference(value, res_x, low, high));
                mapped++;
            }
        }
    }
    CHECK(mapped > 40);
}

TEST(Calibration, ValuesBeyondTheLimitAreClamped) {
    // Aiming almost parallel to the screen gives huge values; they are limited first, in either direction.
    CHECK_EQ(MapCalibratedAxis(std::numeric_limits<int32_t>::max(), res_x, 0, 0), CALI_MAP_VALUE_LIMIT);
    CHECK_EQ(MapCalibratedAxis(std::numeric_limits<int32_t>::min(), res_x, 0, 0), -CALI_MAP_VALUE_LIMIT);
    CHECK_EQ(MapCalibratedAxis(CALI_MAP_VALUE_LIMIT + 1, res_x, 0, 0), CALI_MAP_VALUE_LIMIT);
    CHECK_EQ(MapCalibratedAxis(CALI_MAP_VALUE_LIMIT, res_x, 0, 0), CALI_MAP_VALUE_LIMIT);

    // The clamped value is then mapped like any other, still inside int32_t.
    const int64_t expected = reference(CALI_MAP_VALUE_LIMIT, res_x, 100, 100);
    CHECK_EQ(MapCalibratedAxis(std::numeric_limits<int32_t>::max(), res_x, 100, 100), expected);
}

TEST(Calibration, ADamagedCalibrationIsIgnored) {
    // An offset beyond one screen, or offsets that leave less than a quarter of the screen, can't come from aiming at
    // the edges: the value is used as it is (clamped).
    const int32_t value = 1234;
    CHECK_EQ(MapCalibratedAxis(value, res_x, -res_x - 1, 0), value);
    CHECK_EQ(MapCalibratedAxis(value, res_x, res_x + 1, 0), value);
    CHECK_EQ(MapCalibratedAxis(value, res_x, 0, -res_x - 1), value);
    CHECK_EQ(MapCalibratedAxis(value, res_x, 0, res_x + 1), value);
    CHECK_EQ(MapCalibratedAxis(value, res_x, res_x, res_x), value);                      // span: -res_x
    CHECK_EQ(MapCalibratedAxis(value, res_x, res_x / 2, res_x / 2 + 1), value);          // span: -1
    CHECK_EQ(MapCalibratedAxis(value, res_x, res_x / 2, res_x / 2 - 1), value);          // span 1: far below a quarter
    CHECK_EQ(MapCalibratedAxis(value, res_x, 0, res_x - res_x / 4 + 1), value);          // just under a quarter

    // Just a quarter is the narrowest calibration that is accepted.
    CHECK_EQ(MapCalibratedAxis(value, res_x, 0, res_x - res_x / 4), (int32_t)((int64_t)value * res_x / (res_x / 4)));
}

TEST(Calibration, TheLimitFitsTheArithmetic) {
    // The product in the mapping stays inside int32_t for the largest value and offset: this is what the limit is for.
    const int64_t largest = ((int64_t)CALI_MAP_VALUE_LIMIT + res_x) * res_x;
    CHECK(largest <= std::numeric_limits<int32_t>::max());
}

TEST(RunModes, OnlyRealModesCanBeStoredInAProfile) {
    CHECK(FW_Const::IsProfileRunMode(FW_Const::RunMode_Normal));
    CHECK(FW_Const::IsProfileRunMode(FW_Const::RunMode_Average));
    CHECK(FW_Const::IsProfileRunMode(FW_Const::RunMode_Average2));
    CHECK(FW_Const::IsProfileRunMode(FW_Const::RunMode_OneEuro));

    // The Processing test mode is never stored, and nothing beyond the last mode or below the first is a mode.
    CHECK(!FW_Const::IsProfileRunMode(FW_Const::RunMode_Processing));
    CHECK(!FW_Const::IsProfileRunMode(-1));
    CHECK(!FW_Const::IsProfileRunMode(FW_Const::RunMode_Count));
    CHECK(!FW_Const::IsProfileRunMode(255));
}

TEST(RunModes, TheDesktopAppsModesKeepTheirNumbers) {
    // The modes up to Processing are known to the Desktop App by number, so they must never be renumbered.
    CHECK_EQ(FW_Const::RunMode_Normal, 0);
    CHECK_EQ(FW_Const::RunMode_Average, 1);
    CHECK_EQ(FW_Const::RunMode_Average2, 2);
    CHECK_EQ(FW_Const::RunMode_Processing, 3);
    CHECK_EQ(FW_Const::RunMode_OneEuro, 4);

    // A profile stores the run mode in 5 bits.
    CHECK(FW_Const::RunMode_Count <= 32);
}
