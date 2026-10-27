// Tests for OpenFIRE_Diamond (the advanced tracker): LEDs in the middle of the four sides of the screen.
//
// Slots are numbered round the diamond: 0 top, 1 right, 2 bottom, 3 left (right being the larger X in the camera's
// image, which the Diamond tracker is given as it is). In these tests LED i of a scene is the LED that belongs in
// slot i, so "the tracker has kept the LEDs' identities" means slot i is at LED i's position.
#include "framework.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <Arduino.h>
#include <OpenFIRE_Diamond_Advanced.h>

#include "support.h"

using namespace scene;

namespace {

// The diamond's size in camera pixels: wider than tall, like the monitor.
constexpr double DIAMOND_WIDTH = 700, DIAMOND_HEIGHT = 400;

void feed(OpenFIRE_Diamond& tracker, const Leds& leds, unsigned visibleLeds = ALL_LEDS, const int* ledOfBlob = IN_ORDER) {
    const Blobs blobs = camera(leds, visibleLeds, ledOfBlob, false);
    tracker.begin(blobs.x, blobs.y, blobs.mask);
}

void feedFrames(OpenFIRE_Diamond& tracker, const Leds& leds, int frames, unsigned visibleLeds = ALL_LEDS) {
    for (int i = 0; i < frames; i++) feed(tracker, leds, visibleLeds);
}

Leds shape(double centerX, double centerY, double roll) {
    return diamond(centerX, centerY, DIAMOND_WIDTH, DIAMOND_HEIGHT, roll);
}

OpenFIRE_Diamond lockedOn(const Leds& leds) {
    OpenFIRE_Diamond tracker;
    tracker.configure();
    feedFrames(tracker, leds, 5);
    return tracker;
}

// Rolls the diamond from one angle to another in small steps, with all four LEDs visible all the time, and
// returns the first angle at which a slot is not at its LED any more, or NAN if that never happens.
double firstRollWithWrongIdentity(OpenFIRE_Diamond& tracker, double from, double to, double step,
                                  const int* ledOfBlob = IN_ORDER) {
    for (double roll = from; step > 0 ? roll <= to : roll >= to; roll += step) {
        const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
        feed(tracker, leds, ALL_LEDS, ledOfBlob);
        if (!slotsHoldTheirLeds(tracker, leds)) return roll;
    }
    return NAN;
}

} // namespace

TEST(DiamondAdvanced, StartsWithTheDefaultGeometry) {
    OpenFIRE_Diamond tracker;
    tracker.configure();

    // The default diamond is centred and wider than tall, and nothing has been seen.
    CHECK(tracker.Y(0) < tracker.Y(2));
    CHECK(tracker.X(3) < tracker.X(1));
    CHECK(tracker.X(1) - tracker.X(3) > tracker.Y(2) - tracker.Y(0));
    CHECK(perimeterIsClockwise(tracker, DIAMOND_PERIMETER));
    CHECK_NEAR(tracker.testMedianX(), MouseMaxX / 2, 2);
    CHECK_NEAR(tracker.testMedianY(), MouseMaxY / 2, 2);
    CHECK_EQ(tracker.seen(), 0);
    for (int slot = 0; slot < 4; slot++) CHECK_EQ(tracker.testSee(slot), 0);
}

TEST(DiamondAdvanced, FullViewPublishesTheLedsInTheirSlots) {
    OpenFIRE_Diamond tracker;
    tracker.configure();
    const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    feed(tracker, leds);

    CHECK(slotsHoldTheirLeds(tracker, leds));
    CHECK_EQ(tracker.seen(), ALL_LEDS);
    for (int slot = 0; slot < 4; slot++) CHECK_EQ(tracker.testSee(slot) & 1, 1);
    CHECK_NEAR(tracker.testMedianX(), mouse(CAMERA_CENTER_X), 1);
    CHECK_NEAR(tracker.testMedianY(), mouse(CAMERA_CENTER_Y), 1);
    CHECK_NEAR(tracker.H(), DIAMOND_HEIGHT * CamToMouseMult, 1);    // top to bottom
    CHECK_NEAR(tracker.W(), DIAMOND_WIDTH * CamToMouseMult, 1);     // left to right
    CHECK_NEAR(tracker.Ang(), 0.0, 1e-6);
}

TEST(DiamondAdvanced, SlotsDontDependOnTheOrderTheCameraReportsItsBlobs) {
    const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    int ordersTried = 0;
    forEachBlobOrder([&](const int* order) {
        OpenFIRE_Diamond tracker;
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

TEST(DiamondAdvanced, ThreeLedsAreNotEnoughToStart) {
    OpenFIRE_Diamond tracker;
    tracker.configure();
    const int topX = tracker.X(0), topY = tracker.Y(0);
    feedFrames(tracker, shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 5, 0x07);

    CHECK_EQ(tracker.X(0), topX);
    CHECK_EQ(tracker.Y(0), topY);
    CHECK_EQ(tracker.seen(), 0x07);
}

TEST(DiamondAdvanced, KeepsEachLedInItsSlotWhileTheGunRolls) {
    // Rolling the gun in either direction up to 120 degrees, with the blobs reported in a scrambled order.
    const int scrambled[4] = {2, 0, 3, 1};
    for (double direction : {1.0, -1.0}) {
        OpenFIRE_Diamond tracker;
        tracker.configure();
        const double wrongAt = firstRollWithWrongIdentity(tracker, 0, 120 * direction, 2 * direction, scrambled);
        INFO("direction %.0f", direction);
        CHECK(std::isnan(wrongAt));
    }
}

TEST(DiamondAdvanced, AlwaysPublishesTheFourLedsAsAnUnmirroredDiamond) {
    // Even past the roll the tracker can follow, where it falls back to reading the diamond as upright, what it
    // publishes is the four real LEDs, in an order that goes round the diamond: a mirror image can't happen.
    for (double direction : {1.0, -1.0}) {
        OpenFIRE_Diamond tracker;
        tracker.configure();
        int framesChecked = 0;
        for (double roll = 0; std::fabs(roll) <= 360; roll += 2 * direction) {
            const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
            feed(tracker, leds);
            framesChecked++;
            if (!publishesTheLeds(tracker, leds) || !perimeterIsClockwise(tracker, DIAMOND_PERIMETER)) {
                INFO("roll %.0f", roll);
                CHECK(false);
                break;
            }
        }
        CHECK(framesChecked > 150);
    }
}

TEST(DiamondAdvanced, ReportsTheRoll) {
    // The angle of the line from the left LED to the right one: positive for a clockwise roll on the screen.
    for (double roll : {-30.0, -10.0, 10.0, 30.0}) {
        OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll));
        INFO("roll %.0f", roll);
        CHECK_NEAR(tracker.Ang(), roll * M_PI / 180.0, 0.01);
    }
}

TEST(DiamondAdvanced, ColdStartReadsTheDiamondAsUpright) {
    // With no history, the top is the higher end of the shorter diagonal. Rolled by a quarter turn or less
    // it is the real identity; upside down it is the diamond read the right way up.
    for (double roll : {-30.0, 0.0, 30.0}) {
        const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
        OpenFIRE_Diamond tracker;
        tracker.configure();
        feed(tracker, leds);
        INFO("roll %.0f", roll);
        CHECK(slotsHoldTheirLeds(tracker, leds));
    }
    for (double roll : {150.0, 180.0, -150.0}) {
        const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
        OpenFIRE_Diamond tracker;
        tracker.configure();
        feed(tracker, leds);
        INFO("roll %.0f", roll);
        // Slot 0 holds the LED that is physically the bottom one, slot 1 the physical left one, and so on.
        CHECK_EQ(tracker.X(0), mouse(leds[2].x));
        CHECK_EQ(tracker.Y(0), mouse(leds[2].y));
        CHECK_EQ(tracker.X(1), mouse(leds[3].x));
        CHECK_EQ(tracker.Y(1), mouse(leds[3].y));
        CHECK(tracker.X(1) > tracker.X(3));
        CHECK(perimeterIsClockwise(tracker, DIAMOND_PERIMETER));
    }
}

TEST(DiamondAdvanced, HiddenLedStaysWhereItWasWhileTheGunIsStill) {
    const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    for (int hidden = 0; hidden < 4; hidden++) {
        OpenFIRE_Diamond tracker = lockedOn(leds);
        feedFrames(tracker, leds, 20, ALL_LEDS & ~(1u << hidden));
        INFO("LED %d hidden", hidden);
        CHECK(slotsHoldTheirLeds(tracker, leds));
        CHECK_EQ(tracker.testSee(hidden) & 1, 0);
        CHECK_EQ(tracker.testSee((hidden + 1) % 4) & 1, 1);
    }
}

TEST(DiamondAdvanced, HiddenLedFollowsTheOthersWhenTheGunMoves) {
    for (int hidden = 0; hidden < 4; hidden++) {
        OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        for (int frame = 1; frame <= 40; frame++) {
            leds = shape(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y + frame / 2.0, 0);
            feed(tracker, leds, ALL_LEDS & ~(1u << hidden));
        }
        // The gun moved 40 pixels (160 mouse units); the hidden LED may be a few pixels off.
        INFO("LED %d hidden", hidden);
        CHECK(worstError(tracker, leds) <= 16);
    }
}

TEST(DiamondAdvanced, HiddenLedFollowsTheRollOfTheOthers) {
    for (int hidden : {0, 3}) {
        OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        for (int frame = 1; frame <= 60; frame++) {
            leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0.5 * frame);
            feed(tracker, leds, ALL_LEDS & ~(1u << hidden));
        }
        INFO("LED %d hidden", hidden);
        CHECK(worstError(tracker, leds) <= 8);
    }
}

TEST(DiamondAdvanced, TwoVisibleLedsCarryTheDiamondWithThem) {
    // Every pair of LEDs that can be left visible, first with the gun still and then moving.
    for (unsigned visible = 1; visible < 15; visible++) {
        if (__builtin_popcount(visible) != 2) continue;
        OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);

        feedFrames(tracker, leds, 20, visible);
        INFO("LEDs 0x%X visible, gun still", visible);
        CHECK(slotsHoldTheirLeds(tracker, leds));

        for (int frame = 1; frame <= 40; frame++) {
            leds = shape(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y + frame, 0);
            feed(tracker, leds, visible);
        }
        INFO("LEDs 0x%X visible, gun moving", visible);
        CHECK(worstError(tracker, leds) <= 16);
    }
}

TEST(DiamondAdvanced, SingleVisibleLedMovesTheWholeDiamondWithIt) {
    for (int visible = 0; visible < 4; visible++) {
        OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        for (int frame = 1; frame <= 30; frame++) {
            leds = shape(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y - frame, 0);
            feed(tracker, leds, 1u << visible);
        }
        INFO("only LED %d visible", visible);
        CHECK(worstError(tracker, leds) <= 4);
    }
}

TEST(DiamondAdvanced, AfterBlackoutSingleLedIsNotTrustedButThreeAre) {
    OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
    feedFrames(tracker, shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 1, 0x00);
    const int x = tracker.X(0), y = tracker.Y(0);

    // The gun may have moved anywhere in the dark: one LED could be any of the four.
    const Leds elsewhere = shape(CAMERA_CENTER_X + 150, CAMERA_CENTER_Y + 100, 0);
    feedFrames(tracker, elsewhere, 3, 0x01);
    CHECK_EQ(tracker.X(0), x);
    CHECK_EQ(tracker.Y(0), y);

    // Three are enough to tell.
    feedFrames(tracker, elsewhere, 3, 0x07);
    CHECK(tracker.X(0) != x);
    CHECK(worstError(tracker, elsewhere) <= 16);
}

TEST(DiamondAdvanced, BlackoutKeepsTheLastGeometryAndReportsNothingSeen) {
    const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 20);
    OpenFIRE_Diamond tracker = lockedOn(leds);
    const int x = tracker.X(0), y = tracker.Y(0);
    feedFrames(tracker, leds, 30, 0x00);

    CHECK_EQ(tracker.X(0), x);
    CHECK_EQ(tracker.Y(0), y);
    CHECK_EQ(tracker.seen(), 0);
}

TEST(DiamondAdvanced, ShortBlackoutResumesWithTheSameLabels) {
    // The gun is rolled a long way, so the labels come from following the roll, not from the positions alone. After
    // a short blackout they must come back as they were; a long one forgets them.
    auto afterBlackout = [](int darkFrames, bool* identityKept) {
        OpenFIRE_Diamond tracker;
        tracker.configure();
        for (double roll = 0; roll <= 100; roll += 2) feed(tracker, shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll));
        feedFrames(tracker, shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 100), darkFrames, 0x00);
        const Leds back = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 105);
        feed(tracker, back);
        *identityKept = slotsHoldTheirLeds(tracker, back, 4);
        CHECK(publishesTheLeds(tracker, back));
        CHECK(perimeterIsClockwise(tracker, DIAMOND_PERIMETER));
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

TEST(DiamondAdvanced, RejectsCameraPositionsOutsideTheSensor) {
    const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    OpenFIRE_Diamond tracker = lockedOn(leds);

    Blobs blobs = camera(leds, ALL_LEDS, IN_ORDER, false);
    blobs.x[1] = 2000;
    tracker.begin(blobs.x, blobs.y, blobs.mask);
    CHECK(slotsHoldTheirLeds(tracker, leds));

    blobs = camera(leds, ALL_LEDS, IN_ORDER, false);
    blobs.y[2] = -3;
    tracker.begin(blobs.x, blobs.y, blobs.mask);
    CHECK(slotsHoldTheirLeds(tracker, leds));
}

TEST(DiamondAdvanced, RejectsLedsOnALine) {
    // Four LEDs in a row can't be a diamond seen by a camera, whatever caused it.
    const Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
    OpenFIRE_Diamond tracker = lockedOn(leds);

    const Leds line = {{{300, 384}, {400, 384}, {500, 384}, {600, 384}}};
    feed(tracker, line);
    CHECK(slotsHoldTheirLeds(tracker, leds));
}

TEST(DiamondAdvanced, ConfigureForgetsEverything) {
    OpenFIRE_Diamond tracker;
    tracker.configure();
    const int defaultX = tracker.X(0), defaultY = tracker.Y(0);
    feedFrames(tracker, shape(CAMERA_CENTER_X + 80, CAMERA_CENTER_Y - 60, 0), 5);
    CHECK(tracker.X(0) != defaultX);

    tracker.configure();
    CHECK_EQ(tracker.X(0), defaultX);
    CHECK_EQ(tracker.Y(0), defaultY);
    CHECK_EQ(tracker.testSee(0), 0);

    // It is back to waiting for a full view, too.
    feedFrames(tracker, shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 3, 0x07);
    CHECK_EQ(tracker.X(0), defaultX);
}

TEST(DiamondAdvanced, WhatHidingAnLedLeftOffIsReleasedFasterTheFasterTheGunMoves) {
    // A hidden LED is rebuilt as the geometry predicts. When it shows up somewhere else, the shown point
    // doesn't jump there: the difference is released gradually, slowly while the gun is still (a shift would be
    // noticed) and quickly while it moves (it can't be seen). Here the left LED comes back 60 pixels from
    // where the rebuild had it, and the gun moves down by different amounts per frame.
    auto errorAfterTwelveFrames = [](double pixelsPerFrame) {
        OpenFIRE_Diamond tracker;
        tracker.configure();
        Leds leds = shape(CAMERA_CENTER_X, CAMERA_CENTER_Y - 100, 0);
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
    CHECK(still > 0.5 * 60 * CamToMouseMult);   // most of the 240 units of difference is still there
    CHECK(still > slow);
    CHECK(slow > medium);
    CHECK(medium > quick);
    CHECK(quick > fast || quick == 0);
    CHECK(fast <= 4);
}

namespace {

double distanceBetweenSlots(const OpenFIRE_Diamond& tracker, int first, int second) {
    return std::hypot((double)tracker.X(first) - tracker.X(second), (double)tracker.Y(first) - tracker.Y(second));
}

} // namespace

TEST(DiamondAdvanced, NearlySquareDiamondColdStartTakesTheHighestLedAsTop) {
    // When the two diagonals are too alike to tell apart, the top is simply the LED that is highest in the image.
    for (double roll : {0.0, 20.0, -20.0, 60.0, -60.0, 100.0, 200.0}) {
        const Leds leds = diamond(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 380, roll);
        OpenFIRE_Diamond tracker;
        tracker.configure();
        feed(tracker, leds);
        INFO("roll %.0f", roll);
        int highestY = 1 << 30;
        for (const Point& led : leds) highestY = std::min(highestY, mouse(led.y));
        CHECK_EQ(tracker.Y(0), highestY);
        CHECK(publishesTheLeds(tracker, leds));
        CHECK(perimeterIsClockwise(tracker, DIAMOND_PERIMETER));
    }
}

TEST(DiamondAdvanced, AQuarterTurnInOneFrameDoesNotSwapTheDiagonals) {
    // 700 x 400: the diagonal from slot 1 to slot 3 is the long one. A lock a quarter turn off would make
    // the long diagonal run between slots 0 and 2, so after any sudden roll they must still be the right way round,
    // however the continuity would like to label the LEDs.
    for (double jump : {60.0, 75.0, 85.0, 90.0, 95.0, 105.0, 120.0, -75.0, -90.0, -105.0}) {
        OpenFIRE_Diamond tracker = lockedOn(shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0));
        feedFrames(tracker, shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, jump), 3);
        INFO("jump %.0f", jump);
        CHECK(distanceBetweenSlots(tracker, 1, 3) > distanceBetweenSlots(tracker, 0, 2));
        CHECK(perimeterIsClockwise(tracker, DIAMOND_PERIMETER));
    }
}
