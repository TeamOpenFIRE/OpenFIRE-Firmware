// Tests for OpenFIRE_OneEuro_Filter: the adaptive smoothing of the four LED positions.
//
// The camera delivers a frame every 4785 microseconds (209 Hz); the tests drive the filter's clock themselves, so
// the results don't depend on the machine they run on.
#include "framework.h"

#include <climits>
#include <cmath>

#include <Arduino.h>
#include <OpenFIRE_OneEuro_Filter.h>

namespace {

constexpr unsigned long FRAME_MICROSECONDS = 4785;

// Four LEDs close to the centre of the image, where the filter trusts the camera the most.
struct Leds {
    int x[4];
    int y[4];
};

Leds centralLeds() {
    return {{2040, 2056, 2040, 2056}, {1528, 1528, 1544, 1544}};
}

Leds shifted(Leds leds, int dx, int dy) {
    for (int i = 0; i < 4; i++) {
        leds.x[i] += dx;
        leds.y[i] += dy;
    }
    return leds;
}

struct Output {
    float x[4];
    float y[4];
};

// One camera frame: the clock moves on, the filter sees the LEDs.
Output frame(OpenFIRE_OneEuro_Filter& filter, const Leds& leds, unsigned long microseconds = FRAME_MICROSECONDS) {
    testMicros += microseconds;
    Output output;
    filter.process(leds.x, leds.y, output.x, output.y);
    return output;
}

// Frames of the same LEDs, the last output returned.
Output settle(OpenFIRE_OneEuro_Filter& filter, const Leds& leds, int frames) {
    Output output = frame(filter, leds);
    for (int i = 1; i < frames; i++) output = frame(filter, leds);
    return output;
}

} // namespace

TEST(OneEuroFilter, FirstFrameIsPassedThroughUnchanged) {
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    const Output output = frame(filter, leds);
    for (int i = 0; i < 4; i++) {
        CHECK_EQ(output.x[i], leds.x[i]);
        CHECK_EQ(output.y[i], leds.y[i]);
    }
}

TEST(OneEuroFilter, ConstantInputGivesExactlyThatOutput) {
    // The input is whole numbers; the output must settle on them exactly, and not creep towards them forever.
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    const Output output = settle(filter, leds, 500);
    for (int i = 0; i < 4; i++) {
        CHECK(output.x[i] == (float)leds.x[i]);
        CHECK(output.y[i] == (float)leds.y[i]);
    }
}

TEST(OneEuroFilter, StepIsFollowedGraduallyWithoutOvershoot) {
    OpenFIRE_OneEuro_Filter filter;
    const Leds before = centralLeds();
    const Leds after = shifted(before, 8, 0);
    settle(filter, before, 100);

    float previous = (float)before.x[0];
    for (int i = 0; i < 600; i++) {
        const Output output = frame(filter, after);
        if (i == 0) {
            // A first reaction, but nowhere near the whole step: that is the smoothing.
            CHECK(output.x[0] > previous);
            CHECK(output.x[0] < previous + 4.0f);
        }
        CHECK(output.x[0] >= previous);                 // never goes back...
        CHECK(output.x[0] <= (float)after.x[0]);        // ...and never goes past the target
        previous = output.x[0];
    }
    CHECK(previous == (float)after.x[0]);               // and arrives exactly
}

TEST(OneEuroFilter, JitterIsStronglyReduced) {
    // The camera's position flickers by one unit either way from frame to frame, with no real movement.
    OpenFIRE_OneEuro_Filter filter;
    Leds leds = centralLeds();
    float lowest = 1e9f, highest = -1e9f;
    for (int i = 0; i < 700; i++) {
        const int jitter = (i & 1) ? 1 : -1;
        const Output output = frame(filter, shifted(leds, jitter, jitter));
        if (i >= 300) {
            lowest = std::fmin(lowest, output.x[0] - (float)leds.x[0]);
            highest = std::fmax(highest, output.x[0] - (float)leds.x[0]);
        }
    }
    // The input spans 2 units; the output must span a small fraction of that.
    CHECK(highest - lowest < 0.5f);
}

TEST(OneEuroFilter, FastMovementIsFollowedWithShortLagAndSlowMovementIsSmoothedMore) {
    // The point of the 1€ filter: the cutoff grows with the speed. The lag of a steady movement, in frames, is
    // therefore long when slow and short when fast. Both movements stay near the centre of the image.
    auto lagInFrames = [](int unitsPerFrameTimes10, int frames) {
        OpenFIRE_OneEuro_Filter filter;
        Leds leds = centralLeds();
        settle(filter, leds, 50);
        float lag = 0.0f;
        for (int i = 1; i <= frames; i++) {
            const Leds moving = shifted(leds, unitsPerFrameTimes10 * i / 10, 0);
            const Output output = frame(filter, moving);
            lag = (float)moving.x[0] - output.x[0];
        }
        return lag / (unitsPerFrameTimes10 / 10.0f);
    };

    const float fastLag = lagInFrames(200, 60);     // 20 units per frame: 4000 units per second
    const float slowLag = lagInFrames(10, 400);     // 1 unit per frame
    INFO("fast lag %.2f frames, slow lag %.2f frames", fastLag, slowLag);
    CHECK(fastLag > 0.0f);          // it is a filter: there is some lag
    CHECK(fastLag < 2.0f);          // but a fast movement is hardly delayed
    CHECK(slowLag > 5.0f * fastLag);
}

TEST(OneEuroFilter, AllLedsAreSmoothedWithTheCutoffOfTheFastestOne) {
    // The four LEDs are one rigid shape: if one of them moves fast, the others are smoothed less too, or the shape
    // would bend while they lag behind it. LED 1 takes the same small step in both runs; only LED 0 differs.
    auto responseOfLed1 = [](bool led0MovesFast) {
        OpenFIRE_OneEuro_Filter filter;
        const Leds leds = centralLeds();
        settle(filter, leds, 50);
        Leds next = leds;
        next.x[1] += 10;
        if (led0MovesFast) next.x[0] += 200;
        return frame(filter, next).x[1] - (float)leds.x[1];
    };

    const float alone = responseOfLed1(false);
    const float withFastNeighbour = responseOfLed1(true);
    INFO("alone %.3f, with a fast neighbour %.3f", alone, withFastNeighbour);
    CHECK(alone > 0.0f);
    CHECK(withFastNeighbour > 1.5f * alone);
}

TEST(OneEuroFilter, LedsThatDontMoveStayWhereTheyAre) {
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    settle(filter, leds, 50);
    Leds moved = leds;
    moved.x[2] += 300;
    moved.y[2] -= 120;
    for (int i = 0; i < 20; i++) {
        const Output output = frame(filter, moved);
        for (int led : {0, 1, 3}) {
            CHECK(output.x[led] == (float)leds.x[led]);
            CHECK(output.y[led] == (float)leds.y[led]);
        }
    }
}

TEST(OneEuroFilter, NearTheEdgeOfTheImageTheFilterIsLessResponsive) {
    // The lens distorts the edges of the image the most, so a small change there counts for less than in the centre.
    auto responseToAStep = [](const Leds& leds) {
        OpenFIRE_OneEuro_Filter filter;
        settle(filter, leds, 50);
        return frame(filter, shifted(leds, 3, 0)).x[0] - (float)leds.x[0];
    };

    const Leds centre = centralLeds();
    const Leds corners = {{5, MouseMaxX - 5, 5, MouseMaxX - 5}, {5, 5, MouseMaxY - 5, MouseMaxY - 5}};
    const float centreResponse = responseToAStep(centre);
    const float edgeResponse = responseToAStep(corners);
    INFO("centre %.5f, edge %.5f", centreResponse, edgeResponse);
    CHECK(edgeResponse > 0.0f);
    CHECK(centreResponse > 1.5f * edgeResponse);
}

TEST(OneEuroFilter, LedsOutsideTheImageAreTreatedLikeTheEdge) {
    // LEDs rebuilt by the trackers can lie outside the image. Their position says nothing about the camera's
    // accuracy, so they must count no better than the image's edge.
    auto responseToAStep = [](const Leds& leds) {
        OpenFIRE_OneEuro_Filter filter;
        settle(filter, leds, 50);
        return frame(filter, shifted(leds, 3, 0)).x[0] - (float)leds.x[0];
    };

    const Leds atTheEdge = {{5, MouseMaxX - 5, 5, MouseMaxX - 5}, {5, 5, MouseMaxY - 5, MouseMaxY - 5}};
    const Leds outside = {{-500, MouseMaxX + 500, -500, MouseMaxX + 500}, {-500, -500, MouseMaxY + 500, MouseMaxY + 500}};
    // The two are the same up to float rounding: a float near 500 resolves about 3e-5, a few thousandths of the
    // response being measured (about 0.0115). So compare to within 1% of it.
    const float edgeResponse = responseToAStep(atTheEdge);
    CHECK(edgeResponse > 0.0f);
    CHECK_NEAR(responseToAStep(outside), edgeResponse, 0.01 * edgeResponse);
}

TEST(OneEuroFilter, LongPauseRestartsTheFilter) {
    // After tracking was lost, or the filter wasn't in use, the old positions are meaningless: the next frame
    // is taken as it comes.
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    settle(filter, leds, 50);
    const Leds far = shifted(leds, 900, 400);
    const Output output = frame(filter, far, 200000);
    for (int i = 0; i < 4; i++) {
        CHECK(output.x[i] == (float)far.x[i]);
        CHECK(output.y[i] == (float)far.y[i]);
    }
}

TEST(OneEuroFilter, ShortPauseDoesNotRestartTheFilter) {
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    settle(filter, leds, 50);
    const Leds far = shifted(leds, 900, 400);
    const Output output = frame(filter, far, 90000);   // 0.09 s: under the limit
    CHECK(output.x[0] < (float)far.x[0]);
    CHECK(output.x[0] > (float)leds.x[0]);
}

TEST(OneEuroFilter, ResetMakesTheNextFrameUnfiltered) {
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    settle(filter, leds, 50);
    filter.reset();
    const Leds far = shifted(leds, 900, 400);
    const Output output = frame(filter, far);
    for (int i = 0; i < 4; i++) {
        CHECK(output.x[i] == (float)far.x[i]);
        CHECK(output.y[i] == (float)far.y[i]);
    }
}

TEST(OneEuroFilter, ClockWrapAroundIsHandled) {
    // micros() starts again from zero now and then (every 71 minutes on the board). The frame that crosses it
    // must take a normal amount of time, not look like a pause of hours.
    OpenFIRE_OneEuro_Filter filter;
    const Leds leds = centralLeds();
    testMicros = ULONG_MAX - 3 * FRAME_MICROSECONDS;
    settle(filter, leds, 3);
    const Output output = frame(filter, shifted(leds, 10, 0));    // this frame crosses the wrap
    CHECK(testMicros < 10 * FRAME_MICROSECONDS);                  // the clock really did wrap
    CHECK(output.x[0] > (float)leds.x[0]);
    CHECK(output.x[0] < (float)leds.x[0] + 10.0f);                // filtered, not restarted
}

TEST(OneEuroFilter, FrameWithoutTimePassingIsTreatedAsAnOrdinaryFrame) {
    // Two frames with the same timestamp would divide by zero. They count as a frame of the nominal duration.
    auto outputAfterTheLastFrame = [](unsigned long lastFrameMicroseconds) {
        OpenFIRE_OneEuro_Filter filter;
        const Leds leds = centralLeds();
        settle(filter, leds, 50);
        return frame(filter, shifted(leds, 30, 0), lastFrameMicroseconds).x[0];
    };

    const float withoutTime = outputAfterTheLastFrame(0);
    const float nominal = outputAfterTheLastFrame(FRAME_MICROSECONDS);
    CHECK(std::isfinite(withoutTime));
    CHECK_NEAR(withoutTime, nominal, 0.05);
}

TEST(OneEuroFilter, OutputStaysFiniteWhateverItIsGiven) {
    // Positions far outside the image (rebuilt LEDs), jumps, and frames at odd times: the output must never
    // become infinite or not-a-number, which would stay for good.
    OpenFIRE_OneEuro_Filter filter;
    unsigned int state = 12345;
    auto random = [&state]() { state = state * 1664525u + 1013904223u; return state >> 8; };

    bool allFinite = true;
    for (int i = 0; i < 20000; i++) {
        Leds leds;
        for (int led = 0; led < 4; led++) {
            leds.x[led] = (int)(random() % 40000) - 16383;
            leds.y[led] = (int)(random() % 40000) - 16383;
        }
        const unsigned long microseconds = (random() % 5 == 0) ? 0 : random() % 300000;
        const Output output = frame(filter, leds, microseconds);
        for (int led = 0; led < 4; led++)
            allFinite = allFinite && std::isfinite(output.x[led]) && std::isfinite(output.y[led]);
    }
    CHECK(allFinite);
}
