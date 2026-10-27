// Tests for the classic trackers OpenFIRE_Square and OpenFIRE_Diamond, the ones the RP2040 runs. They share their
// class names with the advanced ones, so these tests are built on their own, with NO_ADVANCED_TRACKING (see run.sh).
//
// The classic trackers keep no memory of which LED is which: they sort the points into the four quadrants round
// the middle of the shape. That works for a gun held nearly level, and these tests check that range and what the
// trackers do when LEDs are hidden or leave the camera's image, not the roll tracking of the advanced ones.
#include "framework.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <Arduino.h>
#include <OpenFIRE_Diamond.h>
#include <OpenFIRE_Square.h>

#include "support.h"

using namespace scene;

#ifdef ADVANCED_TRACKING
#error "The classic tracker tests must be built with NO_ADVANCED_TRACKING"
#endif

namespace {

constexpr double TALL_WIDTH = 284, TALL_HEIGHT = 400;
constexpr double DIAMOND_WIDTH = 700, DIAMOND_HEIGHT = 400;

// The classic trackers expect their members to start as zero, as they do in the firmware (static storage). A
// default-initialized local variable would hold garbage in those, so the trackers are value-initialized.
struct SquareFixture {
    static constexpr bool MIRROR_X = true;
    using Tracker = OpenFIRE_Square;
    static Leds shape(double x, double y, double roll) { return rectangle(x, y, TALL_WIDTH, TALL_HEIGHT, roll); }
    static constexpr double MAX_ROLL = 35;
    // The Square leaves a hidden LED where it was, however the gun moves; it keeps only the seen ones up to date.
    static constexpr bool HIDDEN_LEDS_FOLLOW_THE_GUN = false;
    // The Diamond takes the blobs in the order the camera reports them as they are.
    static constexpr bool BLOB_ORDER_DOESNT_MATTER = true;
};

struct DiamondFixture {
    static constexpr bool MIRROR_X = false;
    using Tracker = OpenFIRE_Diamond;
    static Leds shape(double x, double y, double roll) { return diamond(x, y, DIAMOND_WIDTH, DIAMOND_HEIGHT, roll); }
    static constexpr double MAX_ROLL = 10;
    static constexpr bool HIDDEN_LEDS_FOLLOW_THE_GUN = true;
    static constexpr bool BLOB_ORDER_DOESNT_MATTER = false;
};

template <class Fixture>
void feed(typename Fixture::Tracker& tracker, const Leds& leds, unsigned visibleLeds = ALL_LEDS, const int* ledOfBlob = IN_ORDER) {
    const Blobs blobs = camera(leds, visibleLeds, ledOfBlob, Fixture::MIRROR_X);
    tracker.begin(blobs.x, blobs.y, blobs.mask);
}

template <class Fixture>
void feedFrames(typename Fixture::Tracker& tracker, const Leds& leds, int frames, unsigned visibleLeds = ALL_LEDS) {
    for (int i = 0; i < frames; i++) feed<Fixture>(tracker, leds, visibleLeds);
}

bool insideImage(const Point& led) {
    return led.x >= 0 && led.x <= CamMaxX && led.y >= 0 && led.y <= CamMaxY;
}

// The LEDs of the scene that the camera can see: the ones inside its image.
unsigned visibleIn(const Leds& leds) {
    unsigned visible = 0;
    for (int led = 0; led < 4; led++) {
        if (insideImage(leds[led])) visible |= 1u << led;
    }
    return visible;
}

// The largest distance between the LEDs the camera sees and their slots.
template <class Tracker>
int worstErrorOfSeen(const Tracker& tracker, const Leds& leds, unsigned visible) {
    int worst = 0;
    for (int slot = 0; slot < 4; slot++) {
        if (!(visible & (1u << slot))) continue;
        worst = std::max(worst, std::abs(tracker.X(slot) - mouse(leds[slot].x)));
        worst = std::max(worst, std::abs(tracker.Y(slot) - mouse(leds[slot].y)));
    }
    return worst;
}

// Every test is run for both trackers.
template <class Fixture>
struct ClassicTests {
    using Tracker = typename Fixture::Tracker;

    static void waitsForAFullViewBeforeStarting() {
        Tracker tracker{};
        const int x = tracker.X(0), y = tracker.Y(0);
        feedFrames<Fixture>(tracker, Fixture::shape(CAMERA_CENTER_X + 40, CAMERA_CENTER_Y, 0), 5, 0x07);
        CHECK_EQ(tracker.X(0), x);
        CHECK_EQ(tracker.Y(0), y);
        CHECK_EQ(tracker.seen(), 0x07);
    }

    static void fullViewPublishesTheLedsInTheirSlots() {
        for (double roll = -Fixture::MAX_ROLL; roll <= Fixture::MAX_ROLL; roll += 5) {
            const Leds leds = Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
            Tracker tracker{};
            feedFrames<Fixture>(tracker, leds, 10);
            INFO("roll %.0f", roll);
            CHECK(worstError(tracker, leds) <= 4);
            CHECK_EQ(tracker.seen(), ALL_LEDS);
            for (int slot = 0; slot < 4; slot++) CHECK(tracker.testSee(slot) & 1);
        }
    }

    static void slotsDontDependOnTheOrderTheCameraReportsItsBlobs() {
        if (!Fixture::BLOB_ORDER_DOESNT_MATTER) return;
        const Leds leds = Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
        int ordersTried = 0;
        forEachBlobOrder([&](const int* order) {
            Tracker tracker{};
            for (int i = 0; i < 10; i++) feed<Fixture>(tracker, leds, ALL_LEDS, order);
            ordersTried++;
            if (worstError(tracker, leds) > 4) {
                INFO("blob order %d %d %d %d", order[0], order[1], order[2], order[3]);
                CHECK(false);
            }
        });
        CHECK_EQ(ordersTried, 24);
    }

    static void centerAndSizeFollowTheShape() {
        const Leds leds = Fixture::shape(CAMERA_CENTER_X + 30, CAMERA_CENTER_Y - 20, 0);
        Tracker tracker{};
        feedFrames<Fixture>(tracker, leds, 10);
        CHECK_NEAR(tracker.testMedianX(), mouse(CAMERA_CENTER_X + 30), 8);
        CHECK_NEAR(tracker.testMedianY(), mouse(CAMERA_CENTER_Y - 20), 8);
        CHECK_NEAR(tracker.H(), TALL_HEIGHT * CamToMouseMult, 0.05 * TALL_HEIGHT * CamToMouseMult);
        CHECK_NEAR(tracker.Ang(), 0.0, 0.02);
    }

    static void aHiddenLedStaysWhereItWas() {
        for (double roll : {-Fixture::MAX_ROLL / 2, 0.0, Fixture::MAX_ROLL / 2}) {
            const Leds leds = Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
            for (int hidden = 0; hidden < 4; hidden++) {
                Tracker tracker{};
                feedFrames<Fixture>(tracker, leds, 10);
                feedFrames<Fixture>(tracker, leds, 20, ALL_LEDS & ~(1u << hidden));
                INFO("roll %.0f, LED %d hidden", roll, hidden);
                CHECK(worstError(tracker, leds) <= 8);
            }
        }
    }

    static void seenLedsFollowTheGunWhileOthersAreHidden() {
        // One to three LEDs hidden while the gun is moved 40 pixels. The seen ones are where they are; the Diamond also
        // rebuilds the hidden ones from the shape.
        for (unsigned visible : {0x0Eu, 0x0Du, 0x0Bu, 0x07u, 0x03u, 0x05u, 0x0Au, 0x0Cu, 0x06u, 0x09u}) {
            Tracker tracker{};
            feedFrames<Fixture>(tracker, Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 10);
            Leds leds = Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0);
            for (int frame = 1; frame <= 40; frame++) {
                leds = Fixture::shape(CAMERA_CENTER_X + frame, CAMERA_CENTER_Y + frame, 0);
                feed<Fixture>(tracker, leds, visible);
            }
            INFO("LEDs 0x%X visible", visible);
            CHECK(worstErrorOfSeen(tracker, leds, visible) <= 8);
            if (Fixture::HIDDEN_LEDS_FOLLOW_THE_GUN) CHECK(worstError(tracker, leds) <= 16);
        }
    }

    static void ledsThatLeaveTheImageAreExtrapolated() {
        // The camera only sees LEDs that are inside its image. Moving the gun until some of them are outside has
        // to leave the tracker with the whole shape, from the ones still seen.
        const double directions[8][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}};
        for (const auto& direction : directions) {
            for (double roll : {0.0, Fixture::MAX_ROLL / 2, -Fixture::MAX_ROLL / 2}) {
                Tracker tracker{};
                feedFrames<Fixture>(tracker, Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll), 10);
                int framesWithHiddenLeds = 0;
                for (int step = 1; step <= 140; step++) {
                    const Leds leds = Fixture::shape(CAMERA_CENTER_X + direction[0] * step * 5,
                                                     CAMERA_CENTER_Y + direction[1] * step * 5, roll);
                    const unsigned visible = visibleIn(leds);
                    feed<Fixture>(tracker, leds, visible);
                    if (visible == 0) break;
                    if (visible != ALL_LEDS) framesWithHiddenLeds++;
                    if (worstErrorOfSeen(tracker, leds, visible) > 24) {
                        INFO("direction %.0f %.0f, roll %.0f, step %d, visible 0x%X", direction[0], direction[1], roll, step, visible);
                        CHECK(false);
                        break;
                    }
                }
                CHECK(framesWithHiddenLeds > 10);
            }
        }
    }

    static void rollingWithAnLedHiddenKeepsFollowingTheSeenOnes() {
        for (int hidden = 0; hidden < 4; hidden++) {
            Tracker tracker{};
            feedFrames<Fixture>(tracker, Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 10);
            Leds leds;
            for (double roll = 0; roll <= Fixture::MAX_ROLL / 2; roll += 1) {
                leds = Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, roll);
                feed<Fixture>(tracker, leds, ALL_LEDS & ~(1u << hidden));
            }
            INFO("LED %d hidden", hidden);
            CHECK(worstErrorOfSeen(tracker, leds, ALL_LEDS & ~(1u << hidden)) <= 8);
            if (Fixture::HIDDEN_LEDS_FOLLOW_THE_GUN) CHECK(worstError(tracker, leds) <= 40);
        }
    }

    static void seenFlagsAreReported() {
        Tracker tracker{};
        feedFrames<Fixture>(tracker, Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 10);
        // Three LEDs seen: the camera reports them in its first three blobs.
        feedFrames<Fixture>(tracker, Fixture::shape(CAMERA_CENTER_X, CAMERA_CENTER_Y, 0), 1, 0x0B);
        CHECK_EQ(tracker.seen(), 0x07);
    }
};

} // namespace

#define CLASSIC_TEST(name) \
    TEST(SquareClassic, name) { ClassicTests<SquareFixture>::name(); } \
    TEST(DiamondClassic, name) { ClassicTests<DiamondFixture>::name(); }

CLASSIC_TEST(waitsForAFullViewBeforeStarting)
CLASSIC_TEST(fullViewPublishesTheLedsInTheirSlots)
CLASSIC_TEST(slotsDontDependOnTheOrderTheCameraReportsItsBlobs)
CLASSIC_TEST(centerAndSizeFollowTheShape)
CLASSIC_TEST(aHiddenLedStaysWhereItWas)
CLASSIC_TEST(seenLedsFollowTheGunWhileOthersAreHidden)
CLASSIC_TEST(ledsThatLeaveTheImageAreExtrapolated)
CLASSIC_TEST(rollingWithAnLedHiddenKeepsFollowingTheSeenOnes)
CLASSIC_TEST(seenFlagsAreReported)
