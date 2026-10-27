// Tests for OpenFIRE_Square (the advanced tracker): LEDs at the corners of a rectangle around the screen.
//
// Slots are numbered row by row: 0 top left, 1 top right, 2 bottom left, 3 bottom right. In these tests LED i of
// a scene is the LED that belongs in slot i, so "the tracker has kept the LEDs' identities" means slot i is at
// LED i's position. A rolled gun sees the rectangle turned, but the physical LEDs stay the same ones.
#include "framework.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <Arduino.h>
#include <OpenFIRE_Square_Advanced.h>

#include "support.h"

using namespace scene;

namespace {

// The rectangle's size in camera pixels: taller than wide, as the normal layout is, and a wide one for the layout
// with the LEDs at the screen's corners.
constexpr double TALL_WIDTH = 284, TALL_HEIGHT = 400;
constexpr double WIDE_WIDTH = 600, WIDE_HEIGHT = 330;

// The Square tracker is given the camera's image mirrored left to right.
void feed(OpenFIRE_Square& tracker, const Leds& leds, unsigned visibleLeds = ALL_LEDS, const int* ledOfBlob = IN_ORDER) {
    const Blobs blobs = camera(leds, visibleLeds, ledOfBlob, true);
    tracker.begin(blobs.x, blobs.y, blobs.mask);
}

void feedFrames(OpenFIRE_Square& tracker, const Leds& leds, int frames, unsigned visibleLeds = ALL_LEDS) {
    for (int i = 0; i < frames; i++) feed(tracker, leds, visibleLeds);
}

Leds tall(double centerX, double centerY, double roll) { return rectangle(centerX, centerY, TALL_WIDTH, TALL_HEIGHT, roll); }
Leds wide(double centerX, double centerY, double roll) { return rectangle(centerX, centerY, WIDE_WIDTH, WIDE_HEIGHT, roll); }

OpenFIRE_Square lockedOn(const Leds& leds) {
    OpenFIRE_Square tracker;
    tracker.configure();
    feedFrames(tracker, leds, 5);
    return tracker;
}

// Rolls the rectangle from one angle to another in small steps, with all four LEDs visible all the time, and
// returns the first angle at which a slot is not at its LED any more, or NAN if that never happens.
template <class MakeRectangle>
double firstRollWithWrongIdentity(OpenFIRE_Square& tracker, MakeRectangle make, double from, double to, double step,
                                  const int* ledOfBlob = IN_ORDER) {
    for (double roll = from; step > 0 ? roll <= to : roll >= to; roll += step) {
        const Leds leds = make(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
        feed(tracker, leds, ALL_LEDS, ledOfBlob);
        if (!slotsHoldTheirLeds(tracker, leds)) return roll;
    }
    return NAN;
}

} // namespace

TEST(SquareAdvanced, StartsWithTheDefaultGeometry) {
    OpenFIRE_Square tracker;
    tracker.configure();

    // The default rectangle is centred, taller than wide, and nothing has been seen.
    CHECK(tracker.X(0) < tracker.X(1));
    CHECK(tracker.Y(0) < tracker.Y(2));
    CHECK(perimeterIsClockwise(tracker, SQUARE_PERIMETER));
    CHECK(tracker.H() > tracker.W());
    CHECK_NEAR(tracker.testMedianX(), MouseMaxX / 2, 2);
    CHECK_NEAR(tracker.testMedianY(), MouseMaxY / 2, 2);
    CHECK_EQ(tracker.seen(), 0);
    for (int slot = 0; slot < 4; slot++) CHECK_EQ(tracker.testSee(slot), 0);
}

TEST(SquareAdvanced, FullViewPublishesTheLedsInTheirSlots) {
    OpenFIRE_Square tracker;
    tracker.configure();
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    feed(tracker, leds);

    CHECK(slotsHoldTheirLeds(tracker, leds));
    CHECK_EQ(tracker.seen(), ALL_LEDS);
    for (int slot = 0; slot < 4; slot++) CHECK_EQ(tracker.testSee(slot) & 1, 1);
    CHECK_NEAR(tracker.testMedianX(), mouse(CAMERA_CENTER_X), 1);
    CHECK_NEAR(tracker.testMedianY(), mouse(CAMERA_CENTER_Y), 1);
    CHECK_NEAR(tracker.H(), TALL_HEIGHT * CamToMouseMult, 1);
    CHECK_NEAR(tracker.W(), TALL_WIDTH * CamToMouseMult, 1);
    CHECK_NEAR(tracker.Ang(), 0.0, 1e-6);
}

TEST(SquareAdvanced, SlotsDontDependOnTheOrderTheCameraReportsItsBlobs) {
    // The camera lists its blobs in an order that has nothing to do with where the LEDs are.
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    int ordersTried = 0;
    forEachBlobOrder([&](const int* order) {
        OpenFIRE_Square tracker;
        tracker.configure();
        feed(tracker, leds, ALL_LEDS, order);
        ordersTried++;
        if (!slotsHoldTheirLeds(tracker, leds)) {
            INFO("blob order %d %d %d %d", order[0], order[1], order[2], order[3]);
            CHECK(false);
        }
    });
    CHECK_EQ(ordersTried, 24);
}

TEST(SquareAdvanced, ThreeLedsAreNotEnoughToStart) {
    // Until all four LEDs have been seen together, the first measurements aren't trusted.
    OpenFIRE_Square tracker;
    tracker.configure();
    const int topLeftX = tracker.X(0), topLeftY = tracker.Y(0);
    feedFrames(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 5, 0x07);

    CHECK_EQ(tracker.X(0), topLeftX);
    CHECK_EQ(tracker.Y(0), topLeftY);
    CHECK_EQ(tracker.seen(), 0x07);    // what the camera saw is still reported
}

TEST(SquareAdvanced, KeepsEachLedInItsSlotWhileTheGunRolls) {
    // Rolling the gun in either direction up to 120 degrees, with the blobs reported in a scrambled order.
    const int scrambled[4] = {2, 0, 3, 1};
    for (double direction : {1.0, -1.0}) {
        OpenFIRE_Square tracker;
        tracker.configure();
        const double wrongAt = firstRollWithWrongIdentity(
            tracker, tall, 0, 120 * direction, 2 * direction, scrambled);
        INFO("direction %.0f", direction);
        CHECK(std::isnan(wrongAt));
    }
}

TEST(SquareAdvanced, KeepsEachLedInItsSlotWhileTheGunRollsInTheWideLayout) {
    for (double direction : {1.0, -1.0}) {
        OpenFIRE_Square tracker;
        tracker.configure();
        tracker.setWideLayout(true);
        const double wrongAt = firstRollWithWrongIdentity(tracker, wide, 0, 120 * direction, 2 * direction);
        INFO("direction %.0f", direction);
        CHECK(std::isnan(wrongAt));
    }
}

TEST(SquareAdvanced, AlwaysPublishesTheFourLedsAsAnUnmirroredRectangle) {
    // Even past the roll the tracker can follow, where it falls back to reading the rectangle as upright, what it
    // publishes is the four real LEDs, in an order that goes round the rectangle: a mirror image can't happen.
    for (double direction : {1.0, -1.0}) {
        OpenFIRE_Square tracker;
        tracker.configure();
        int framesChecked = 0;
        for (double roll = 0; std::fabs(roll) <= 360; roll += 2 * direction) {
            const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
            feed(tracker, leds);
            framesChecked++;
            if (!publishesTheLeds(tracker, leds) || !perimeterIsClockwise(tracker, SQUARE_PERIMETER)) {
                INFO("roll %.0f", roll);
                CHECK(false);
                break;
            }
        }
        CHECK(framesChecked > 150);
    }
}

TEST(SquareAdvanced, ReportsTheRoll) {
    // Positive when the right side of the rectangle is higher than the left one, so a clockwise roll is negative.
    for (double roll : {-30.0, -10.0, 10.0, 30.0}) {
        OpenFIRE_Square tracker = lockedOn(tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll));
        INFO("roll %.0f", roll);
        CHECK_NEAR(tracker.Ang(), -roll * M_PI / 180.0, 0.01);
    }
}

TEST(SquareAdvanced, CalibrationLabelsLedsByTheirHeight) {
    // Calibration happens facing the screen: the two highest LEDs are the top ones, left to right, whatever the roll.
    for (double roll : {0.0, 20.0, -20.0, 150.0, 180.0}) {
        OpenFIRE_Square tracker;
        tracker.configure();
        tracker.setCalibrationMode(true);
        feed(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll));
        INFO("roll %.0f", roll);

        const int highestOfTheBottom = std::min(tracker.Y(2), tracker.Y(3));
        const int lowestOfTheTop = std::max(tracker.Y(0), tracker.Y(1));
        CHECK(lowestOfTheTop <= highestOfTheBottom);
        CHECK(tracker.X(0) < tracker.X(1));
        CHECK(tracker.X(2) < tracker.X(3));
    }
}

TEST(SquareAdvanced, ColdStartReadsTheRectangleAsUpright) {
    // With no history, the labelling comes from the positions alone: the top slots are the higher LEDs. Rolled
    // by a quarter turn or less it is the real identity; upside down it is the rectangle read the right way up.
    for (double roll : {-30.0, 0.0, 30.0}) {
        OpenFIRE_Square tracker;
        tracker.configure();
        feed(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll));
        INFO("roll %.0f", roll);
        CHECK(slotsHoldTheirLeds(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll)));
    }
    for (double roll : {150.0, 180.0, -150.0}) {
        OpenFIRE_Square tracker;
        tracker.configure();
        const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
        feed(tracker, leds);
        INFO("roll %.0f", roll);
        // Slot 0 holds the LED that is physically the bottom right one, and so on: the labelling is turned by 180.
        CHECK_EQ(tracker.X(0), mouse(leds[3].x));
        CHECK_EQ(tracker.Y(0), mouse(leds[3].y));
        CHECK_EQ(tracker.X(3), mouse(leds[0].x));
        CHECK_EQ(tracker.Y(3), mouse(leds[0].y));
        CHECK(perimeterIsClockwise(tracker, SQUARE_PERIMETER));
    }
}

TEST(SquareAdvanced, HiddenLedStaysWhereItWasWhileTheGunIsStill) {
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    for (int hidden = 0; hidden < 4; hidden++) {
        OpenFIRE_Square tracker = lockedOn(leds);
        feedFrames(tracker, leds, 20, ALL_LEDS & ~(1u << hidden));
        INFO("LED %d hidden", hidden);
        CHECK(slotsHoldTheirLeds(tracker, leds));
        CHECK_EQ(tracker.testSee(hidden) & 1, 0);                   // reported as not seen...
        CHECK_EQ(tracker.testSee((hidden + 1) % 4) & 1, 1);         // ...unlike the others
    }
}

TEST(SquareAdvanced, HiddenLedFollowsTheOthersWhenTheGunMoves) {
    for (int hidden = 0; hidden < 4; hidden++) {
        OpenFIRE_Square tracker = lockedOn(tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        for (int frame = 1; frame <= 40; frame++) {
            leds = tall(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y + frame / 2.0, 0);
            feed(tracker, leds, ALL_LEDS & ~(1u << hidden));
        }
        // The gun moved 40 pixels (160 mouse units); the hidden LED may be a few pixels off.
        INFO("LED %d hidden", hidden);
        CHECK(worstError(tracker, leds) <= 16);
    }
}

TEST(SquareAdvanced, HiddenLedFollowsTheRollOfTheOthers) {
    for (int hidden : {0, 3}) {
        OpenFIRE_Square tracker = lockedOn(tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        for (int frame = 1; frame <= 60; frame++) {
            leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0.5 * frame);
            feed(tracker, leds, ALL_LEDS & ~(1u << hidden));
        }
        INFO("LED %d hidden", hidden);
        CHECK(worstError(tracker, leds) <= 8);
    }
}

TEST(SquareAdvanced, TwoVisibleLedsCarryTheRectangleWithThem) {
    // Every pair of LEDs that can be left visible, first with the gun still and then moving.
    for (unsigned visible = 1; visible < 15; visible++) {
        if (__builtin_popcount(visible) != 2) continue;
        OpenFIRE_Square tracker = lockedOn(tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);

        feedFrames(tracker, leds, 20, visible);
        INFO("LEDs 0x%X visible, gun still", visible);
        CHECK(slotsHoldTheirLeds(tracker, leds));

        for (int frame = 1; frame <= 40; frame++) {
            leds = tall(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y + frame, 0);
            feed(tracker, leds, visible);
        }
        INFO("LEDs 0x%X visible, gun moving", visible);
        CHECK(worstError(tracker, leds) <= 16);
    }
}

TEST(SquareAdvanced, SingleVisibleLedMovesTheWholeRectangleWithIt) {
    // One LED says nothing about roll or size, so the rectangle is moved as it was, with that LED.
    for (int visible = 0; visible < 4; visible++) {
        OpenFIRE_Square tracker = lockedOn(tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        for (int frame = 1; frame <= 30; frame++) {
            leds = tall(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y - frame, 0);
            feed(tracker, leds, 1u << visible);
        }
        INFO("only LED %d visible", visible);
        CHECK(worstError(tracker, leds) <= 4);
    }
}

TEST(SquareAdvanced, AfterBlackoutSingleLedIsNotTrustedButThreeAre) {
    OpenFIRE_Square tracker = lockedOn(tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
    feedFrames(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 1, 0x00);
    const int x = tracker.X(0), y = tracker.Y(0);

    // The gun may have moved anywhere in the dark: one LED could be any of the four.
    const Leds elsewhere = tall(CAMERA_CENTER_X + 150, CAMERA_CENTER_Y + 100, 0);
    feedFrames(tracker, elsewhere, 3, 0x01);
    CHECK_EQ(tracker.X(0), x);
    CHECK_EQ(tracker.Y(0), y);

    // Three are enough to tell.
    feedFrames(tracker, elsewhere, 3, 0x07);
    CHECK(tracker.X(0) != x);
    CHECK(worstError(tracker, elsewhere) <= 16);
}

TEST(SquareAdvanced, BlackoutKeepsTheLastGeometryAndReportsNothingSeen) {
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 20);
    OpenFIRE_Square tracker = lockedOn(leds);
    const int x = tracker.X(0), y = tracker.Y(0);
    feedFrames(tracker, leds, 30, 0x00);

    CHECK_EQ(tracker.X(0), x);
    CHECK_EQ(tracker.Y(0), y);
    CHECK_EQ(tracker.seen(), 0);
}

TEST(SquareAdvanced, ShortBlackoutResumesWithTheSameLabels) {
    // The gun is rolled a long way, so the labels come from following the roll, not from the positions alone. After
    // a short blackout they must come back as they were; a long one forgets them.
    auto afterBlackout = [](int darkFrames, bool* identityKept) {
        OpenFIRE_Square tracker;
        tracker.configure();
        for (double roll = 0; roll <= 100; roll += 2) feed(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll));
        feedFrames(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 100), darkFrames, 0x00);
        const Leds back = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 105);
        feed(tracker, back);
        *identityKept = slotsHoldTheirLeds(tracker, back, 4);
        // Whatever the labels, what comes back is the four LEDs as a proper rectangle.
        CHECK(publishesTheLeds(tracker, back));
        CHECK(perimeterIsClockwise(tracker, SQUARE_PERIMETER));
    };

    for (int darkFrames : {1, 5, 8}) {
        bool kept = false;
        afterBlackout(darkFrames, &kept);
        INFO("%d dark frames", darkFrames);
        CHECK(kept);
    }
    bool kept = true;
    afterBlackout(9, &kept);
    CHECK(!kept);       // after 9 frames the old labels are given up
}

TEST(SquareAdvanced, RejectsCameraPositionsOutsideTheSensor) {
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    OpenFIRE_Square tracker = lockedOn(leds);

    Blobs blobs = camera(leds, ALL_LEDS, IN_ORDER, true);
    blobs.x[1] = 2000;
    tracker.begin(blobs.x, blobs.y, blobs.mask);
    CHECK(slotsHoldTheirLeds(tracker, leds));

    blobs = camera(leds, ALL_LEDS, IN_ORDER, true);
    blobs.y[2] = -3;
    tracker.begin(blobs.x, blobs.y, blobs.mask);
    CHECK(slotsHoldTheirLeds(tracker, leds));
}

TEST(SquareAdvanced, RejectsLedsOnALine) {
    // Four LEDs in a row can't be a rectangle seen by a camera, whatever caused it.
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    OpenFIRE_Square tracker = lockedOn(leds);

    const Leds line = {{{300, 384}, {400, 384}, {500, 384}, {600, 384}}};
    feed(tracker, line);
    CHECK(slotsHoldTheirLeds(tracker, leds));
}

TEST(SquareAdvanced, ConfigureForgetsEverything) {
    OpenFIRE_Square tracker;
    tracker.configure();
    const int defaultX = tracker.X(0), defaultY = tracker.Y(0);
    const float defaultHeight = tracker.H();
    feedFrames(tracker, tall(CAMERA_CENTER_X + 80, CAMERA_CENTER_Y - 60, 0), 5);
    CHECK(tracker.X(0) != defaultX);

    tracker.configure();
    CHECK_EQ(tracker.X(0), defaultX);
    CHECK_EQ(tracker.Y(0), defaultY);
    CHECK_NEAR(tracker.H(), defaultHeight, 0);
    CHECK_EQ(tracker.testSee(0), 0);

    // It is back to waiting for a full view, too.
    feedFrames(tracker, tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 3, 0x07);
    CHECK_EQ(tracker.X(0), defaultX);
}

TEST(SquareAdvanced, ChangingTheLayoutStartsTheTrackingOver) {
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    const Leds moved = tall(CAMERA_CENTER_X + 60, CAMERA_CENTER_Y, 0);

    // Switching to the wide layout drops the history, so three LEDs aren't accepted again until all four were seen.
    OpenFIRE_Square tracker = lockedOn(leds);
    tracker.setWideLayout(true);
    feedFrames(tracker, moved, 3, 0x07);
    CHECK(slotsHoldTheirLeds(tracker, leds));

    // Setting the layout it already has changes nothing: the tracker keeps going.
    OpenFIRE_Square same = lockedOn(leds);
    same.setWideLayout(false);
    feedFrames(same, moved, 3, 0x07);
    CHECK(worstError(same, moved) <= 16);
}

namespace {

// What a tracker publishes at some moment, to compare later frames with.
struct Published {
    int x[4], y[4];
};

Published publishedBy(const OpenFIRE_Square& tracker) {
    Published published;
    for (int slot = 0; slot < 4; slot++) {
        published.x[slot] = tracker.X(slot);
        published.y[slot] = tracker.Y(slot);
    }
    return published;
}

int largestDifference(const OpenFIRE_Square& tracker, const Published& before) {
    int largest = 0;
    for (int slot = 0; slot < 4; slot++) {
        largest = std::max(largest, std::abs(tracker.X(slot) - before.x[slot]));
        largest = std::max(largest, std::abs(tracker.Y(slot) - before.y[slot]));
    }
    return largest;
}

// Whether the tracker's four points are the published ones, in whatever slots: each of its points is within the
// tolerance of one of them, and no two are near the same one.
bool sameCorners(const OpenFIRE_Square& tracker, const Published& before, int tolerance) {
    bool taken[4] = {false, false, false, false};
    for (int slot = 0; slot < 4; slot++) {
        bool found = false;
        for (int other = 0; other < 4 && !found; other++) {
            if (!taken[other] && std::abs(tracker.X(slot) - before.x[other]) <= tolerance &&
                std::abs(tracker.Y(slot) - before.y[other]) <= tolerance) {
                taken[other] = found = true;
            }
        }
        if (!found) return false;
    }
    return true;
}

} // namespace

TEST(SquareAdvanced, WithoutAnchorTwoVisibleLedsCompleteAFlatRectangle) {
    // All six pairs of LEDs: two on the top, bottom, left or right side, and two on a diagonal. The gun is
    // rolled enough in some cases that a side's "left" LED is on the right of the image, which the rebuild
    // has to allow for. The scene is flat, so the rectangle comes out as it is.
    for (bool wideLayout : {false, true}) {
        for (double roll : {0.0, 25.0, -25.0, 70.0, 88.0, 91.0, 93.0, -91.0, -93.0, 100.0, -100.0, 115.0, -115.0}) {
            const Leds leds = wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
            for (unsigned visible = 1; visible < 15; visible++) {
                if (__builtin_popcount(visible) != 2) continue;
                // The gun is rolled up to its angle in steps, as a player would, so that the tracker follows the
                // roll instead of reading the rectangle cold. Past a quarter turn the three LEDs seen after the
                // dark are labelled again from their positions, and the labels can come out differently, so the
                // result is compared as a set of corners with what the tracker published for the full view.
                OpenFIRE_Square tracker;
                tracker.configure();
                tracker.setWideLayout(wideLayout);
                for (double step = 0; std::fabs(step) < std::fabs(roll); step += (roll < 0 ? -5 : 5)) {
                    feed(tracker, wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, step) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, step));
                }
                feedFrames(tracker, leds, 5);
                const Published fullView = publishedBy(tracker);
                feedFrames(tracker, leds, 1, 0x00);
                feedFrames(tracker, leds, 3, 0x07);
                feedFrames(tracker, leds, 15, visible);
                INFO("%s layout, roll %.0f, LEDs 0x%X visible", wideLayout ? "wide" : "tall", roll, visible);
                // The rebuilt corners use the proportions the tracker has learned, not the real ones: a few pixels off, and
                // in the wide layout about 8 pixels with the gun rolled a quarter turn (observed, not designed).
                const int tolerance = wideLayout && std::fabs(std::fabs(roll) - 90) < 5 ? 40 : 16;
                CHECK(sameCorners(tracker, fullView, tolerance));
            }
        }
    }
}

TEST(SquareAdvanced, WithoutAnchorAHiddenLedIsCompletedAsAParallelogram) {
    for (bool wideLayout : {false, true}) {
        for (double roll : {0.0, 25.0, -40.0}) {
            for (int hidden = 0; hidden < 4; hidden++) {
                const Leds leds = wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
                OpenFIRE_Square tracker;
                tracker.configure();
                tracker.setWideLayout(wideLayout);
                feedFrames(tracker, leds, 5);
                feedFrames(tracker, leds, 1, 0x00);
                feedFrames(tracker, leds, 10, ALL_LEDS & ~(1u << hidden));
                INFO("%s layout, roll %.0f, LED %d hidden", wideLayout ? "wide" : "tall", roll, hidden);
                CHECK(slotsHoldTheirLeds(tracker, leds, 4));

                // And it goes on following the three that are seen.
                const Leds rolled = wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll + 10) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll + 10);
                feedFrames(tracker, rolled, 10, ALL_LEDS & ~(1u << hidden));
                CHECK(worstError(tracker, rolled) <= 8);
            }
        }
    }
}

TEST(SquareAdvanced, SingleLedFarFromEveryCornerIsDropped) {
    // A corner can't move more than three quarters of the rectangle's shorter side in a frame, so a lone LED
    // farther than that from all four corners is not one of ours (a reflection, say).
    const Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    OpenFIRE_Square tracker = lockedOn(leds);
    const Published before = publishedBy(tracker);

    Leds stray = leds;
    stray[0] = {50, 50};    // far from the corners, which are within 200 pixels of the centre
    feedFrames(tracker, stray, 3, 0x01);
    CHECK_EQ(largestDifference(tracker, before), 0);

    // Having dropped it, the tracker has nothing to continue a lone LED from, even one at a corner.
    feedFrames(tracker, leds, 3, 0x01);
    CHECK_EQ(largestDifference(tracker, before), 0);

    // Three LEDs bring it back.
    feedFrames(tracker, leds, 3, 0x07);
    CHECK(slotsHoldTheirLeds(tracker, leds));
}

TEST(SquareAdvanced, WhatHidingAnLedLeftOffIsReleasedFasterTheFasterTheGunMoves) {
    // A hidden LED is rebuilt as the geometry predicts. When it shows up somewhere else, the shown point
    // doesn't jump there: the difference is released gradually, slowly while the gun is still (a shift would be
    // noticed) and quickly while it moves (it can't be seen). Here the bottom right LED comes back 60 pixels from
    // where the rebuild had it, and the gun moves down by different amounts per frame.
    auto errorAfterTwelveFrames = [](double pixelsPerFrame) {
        OpenFIRE_Square tracker;
        tracker.configure();
        Leds leds = tall(CAMERA_CENTER_X, CAMERA_CENTER_Y - 100, 0);
        feedFrames(tracker, leds, 5);
        feedFrames(tracker, leds, 10, ALL_LEDS & ~8u);
        leds[3].x -= 60;
        int error = 0;
        for (int frame = 1; frame <= 12; frame++) {
            Leds moved = leds;
            for (Point& led : moved) led.y += pixelsPerFrame * frame;
            feed(tracker, moved);
            error = std::abs(tracker.X(3) - mouse(moved[3].x)) + std::abs(tracker.Y(3) - mouse(moved[3].y));
        }
        return error;
    };

    const int still = errorAfterTwelveFrames(0), slow = errorAfterTwelveFrames(1), medium = errorAfterTwelveFrames(2),
              quick = errorAfterTwelveFrames(4), fast = errorAfterTwelveFrames(10);
    INFO("errors %d %d %d %d %d", still, slow, medium, quick, fast);
    CHECK(still > 0.5 * 60 * CamToMouseMult);
    CHECK(still > slow);
    CHECK(slow > medium);
    CHECK(medium > quick);
    CHECK(quick > fast || quick == 0);
    CHECK(fast <= 4);
}

TEST(SquareAdvanced, StartsRightAtAnyRollUpToAQuarterTurnWithThreeOrFourLeds) {
    // Read cold, a rectangle rolled by up to 85 degrees either way still gets the LEDs' own labels, in both layouts,
    // whether the start is from a full view or from three LEDs after the first full view was lost.
    for (bool wideLayout : {false, true}) {
        for (double roll : {20.0, 30.0, 45.0, 60.0, 75.0, 85.0, -45.0, -60.0, -75.0, -85.0}) {
            for (unsigned visible : {0x0Fu, 0x07u, 0x0Bu}) {
                const Leds leds = wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
                OpenFIRE_Square tracker;
                tracker.configure();
                tracker.setWideLayout(wideLayout);
                feedFrames(tracker, leds, 3);
                feedFrames(tracker, leds, 3, visible);
                INFO("%s layout, roll %.0f, LEDs 0x%X", wideLayout ? "wide" : "tall", roll, visible);
                CHECK(slotsHoldTheirLeds(tracker, leds, 4));
            }
        }
    }
}

TEST(SquareAdvanced, KeepsTheLabelsThroughSuddenRollsOfUpToFortyFiveDegrees) {
    // Far faster than a hand can roll a gun (45 degrees in one frame), but the continuity must hold up when
    // the camera misses frames.
    for (bool wideLayout : {false, true}) {
        for (double jump : {15.0, 30.0, 45.0, -30.0, -45.0}) {
            for (unsigned visible : {0x0Fu, 0x07u, 0x0Bu, 0x0Du, 0x0Eu}) {
                OpenFIRE_Square tracker;
                tracker.configure();
                tracker.setWideLayout(wideLayout);
                const Leds upright = wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
                const Leds rolled = wideLayout ? wide(CAMERA_CENTER_X, CAMERA_CENTER_Y, jump) : tall(CAMERA_CENTER_X, CAMERA_CENTER_Y, jump);
                feedFrames(tracker, upright, 5);
                feedFrames(tracker, rolled, 5, visible);
                INFO("%s layout, jump %.0f, LEDs 0x%X", wideLayout ? "wide" : "tall", jump, visible);
                // The corners that are seen are where they are; one that is hidden is only an estimate, which
                // the jump leaves behind for a while, so it isn't compared.
                for (int slot = 0; slot < 4; slot++) {
                    if (!(visible & (1u << slot))) continue;
                    CHECK(std::abs(tracker.X(slot) - mouse(rolled[slot].x)) <= 8);
                    CHECK(std::abs(tracker.Y(slot) - mouse(rolled[slot].y)) <= 8);
                }
            }
        }
    }
}

TEST(SquareAdvanced, KeepsTheLabelsInObliqueViews) {
    // Seen from far to the side, or from far above or below, perspective shrinks one side of the rectangle so much
    // that its shape no longer says which side is which. The labels must follow the gun's roll all the same, with
    // all four LEDs and with one hidden. Not covered: views from 65 degrees or more to the side (where the
    // rectangle is only 40% as wide as it is tall), in which the tracker labels the three LEDs wrongly when the top left one
    // is hidden, and those more than 40 degrees above or below, or tilted both ways, where the labels of three LEDs are lost too.
    struct View { double yaw, pitch; };
    const View views[] = {{0, 0}, {45, 0}, {-45, 0}, {0, 40}, {0, -40}};
    for (bool wideLayout : {false, true}) {
        for (const View& view : views) {
            for (double roll : {0.0, 40.0, 80.0, -80.0}) {
                for (unsigned visible : {0x0Fu, 0x07u, 0x0Eu}) {
                    auto make = [&](double rollNow) {
                        return wideLayout ? viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 220, view.yaw, view.pitch, rollNow)
                                          : viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 200, 280, view.yaw, view.pitch, rollNow);
                    };
                    OpenFIRE_Square tracker;
                    tracker.configure();
                    tracker.setWideLayout(wideLayout);
                    // For its first 200 frames after a cold start the tracker doesn't trust the labels of three LEDs
                    // seen from an angle (the shape can't confirm them), so it first has a full view for a second.
                    feedFrames(tracker, make(0), 250);
                    for (double step = 0; std::fabs(step) <= std::fabs(roll); step += (roll < 0 ? -5 : 5)) feed(tracker, make(step));
                    feedFrames(tracker, make(roll), 5, visible);
                    INFO("%s layout, yaw %.0f, pitch %.0f, roll %.0f, LEDs 0x%X", wideLayout ? "wide" : "tall", view.yaw, view.pitch, roll, visible);
                    // Only the LEDs that are seen are compared: where a hidden one is estimated depends on the
                    // perspective, but a wrong label would show on the seen ones.
                    const Leds leds = make(roll);
                    for (int slot = 0; slot < 4; slot++) {
                        if (!(visible & (1u << slot))) continue;
                        CHECK(std::abs(tracker.X(slot) - mouse(leds[slot].x)) <= 8);
                        CHECK(std::abs(tracker.Y(slot) - mouse(leds[slot].y)) <= 8);
                    }
                }
            }
        }
    }
}
