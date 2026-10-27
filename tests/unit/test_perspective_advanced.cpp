// Tests for OpenFIRE_Perspective (the advanced warp): from the four LEDs as the camera sees them and the point the
// gun aims with, to where on the screen the gun points.
//
// The LEDs are given in mouse space (the camera's pixels times four), in the order top left, top right, bottom
// left, bottom right, with the screen's corners as their targets, as the firmware does it for the Square layout.
#include "framework.h"

#include <cmath>
#include <limits>

#include <Arduino.h>
#include <OpenFIRE_Perspective_Advanced.h>

#include "support.h"

using namespace scene;

namespace {

constexpr float SCREEN_WIDTH = 1920, SCREEN_HEIGHT = 1080;

struct Quad {
    float x[4], y[4];   // top left, top right, bottom left, bottom right
};

// The corners of a Leds scene (camera pixels) in mouse space.
Quad inMouseSpace(const Leds& leds) {
    Quad quad;
    for (int i = 0; i < 4; i++) {
        quad.x[i] = (float)(leds[i].x * CamToMouseMult);
        quad.y[i] = (float)(leds[i].y * CamToMouseMult);
    }
    return quad;
}

// A front view of the screen's rectangle: the LEDs' quadrilateral is a plain rectangle in the middle of the image.
Quad frontView() {
    return inMouseSpace(rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 0));
}

void warp(OpenFIRE_Perspective& perspective, const Quad& quad, float screenWidth = SCREEN_WIDTH, float screenHeight = SCREEN_HEIGHT) {
    perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                     0, 0, screenWidth, 0, 0, screenHeight, screenWidth, screenHeight);
}

// Where the gun aims on the screen when it aims at the given point of the camera image.
struct Aim {
    int x, y;
};

Aim aimAt(OpenFIRE_Perspective& perspective, const Quad& quad, float aimX, float aimY) {
    perspective.source(aimX, aimY);
    warp(perspective, quad);
    return {perspective.getX(), perspective.getY()};
}

// Where the diagonals of the quadrilateral cross: the image of the middle of the screen, however the screen is seen.
void diagonalsCross(const Quad& quad, float& crossX, float& crossY) {
    const double firstDirectionX = quad.x[3] - quad.x[0], firstDirectionY = quad.y[3] - quad.y[0];
    const double secondDirectionX = quad.x[1] - quad.x[2], secondDirectionY = quad.y[1] - quad.y[2];
    const double determinant = firstDirectionX * secondDirectionY - firstDirectionY * secondDirectionX;
    const double along = ((quad.x[2] - quad.x[0]) * secondDirectionY - (quad.y[2] - quad.y[0]) * secondDirectionX) / determinant;
    crossX = (float)(quad.x[0] + along * firstDirectionX);
    crossY = (float)(quad.y[0] + along * firstDirectionY);
}

} // namespace

TEST(PerspectiveAdvanced, NothingIsKnownBeforeTheFirstWarp) {
    OpenFIRE_Perspective perspective;
    CHECK_EQ(perspective.getX(), 0);
    CHECK_EQ(perspective.getY(), 0);
}

TEST(PerspectiveAdvanced, EachLedLandsOnItsCornerOfTheScreen) {
    // The aim point is moved to each LED in turn: it must come out at that LED's corner of the screen, whatever
    // way the screen is seen.
    struct Scene {
        const char* name;
        Leds leds;
    };
    const Scene scenes[] = {
        {"front view", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 0)},
        {"off centre", rectangle(CAMERA_CENTER_X + 120, CAMERA_CENTER_Y - 90, 300, 220, 0)},
        {"small and far", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 120, 90, 0)},
        {"rolled", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 30)},
        {"upside down", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 180)},
        {"from the side", viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 60, 0, 0)},
        {"from below", viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 0, -45, 0)},
        {"from the side and rolled", viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, -50, 30, 25)},
    };
    const float cornersX[4] = {0, SCREEN_WIDTH, 0, SCREEN_WIDTH};
    const float cornersY[4] = {0, 0, SCREEN_HEIGHT, SCREEN_HEIGHT};

    for (const Scene& scene : scenes) {
        const Quad quad = inMouseSpace(scene.leds);
        for (int corner = 0; corner < 4; corner++) {
            OpenFIRE_Perspective perspective;
            const Aim aim = aimAt(perspective, quad, quad.x[corner], quad.y[corner]);
            INFO("%s, LED %d", scene.name, corner);
            CHECK_NEAR(aim.x, cornersX[corner], 2);
            CHECK_NEAR(aim.y, cornersY[corner], 2);
        }
    }
}

TEST(PerspectiveAdvanced, TheMiddleOfTheScreenIsWhereTheDiagonalsCross) {
    // A perspective warp keeps straight lines straight, so the point where the diagonals of the LEDs' quadrilateral
    // cross is the middle of the screen even when the view is oblique and the quadrilateral far from a rectangle.
    // An affine map (parallel lines staying parallel) would be off in such views. The lens correction bends lines
    // a little towards the image's edges, so the tolerance is not zero.
    const double views[][3] = {{0, 0, 0}, {30, 0, 0}, {60, 0, 0}, {-60, 0, 20}, {0, -45, 0}, {45, 30, -40}};
    for (const auto& view : views) {
        const Quad quad = inMouseSpace(viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, view[0], view[1], view[2]));
        float crossX, crossY;
        diagonalsCross(quad, crossX, crossY);
        OpenFIRE_Perspective perspective;
        const Aim aim = aimAt(perspective, quad, crossX, crossY);
        INFO("yaw %.0f, pitch %.0f, roll %.0f", view[0], view[1], view[2]);
        CHECK_NEAR(aim.x, SCREEN_WIDTH / 2, 0.01 * SCREEN_WIDTH);
        CHECK_NEAR(aim.y, SCREEN_HEIGHT / 2, 0.01 * SCREEN_HEIGHT);
    }
}

TEST(PerspectiveAdvanced, FrontViewIsALinearMap) {
    // Seen head on, a point halfway along the LEDs' rectangle is halfway across the screen, and the aim point's offset
    // from the rectangle's centre scales with the screen.
    const Quad quad = frontView();
    OpenFIRE_Perspective perspective;
    const float centerX = (quad.x[0] + quad.x[3]) / 2, centerY = (quad.y[0] + quad.y[3]) / 2;
    const float width = quad.x[1] - quad.x[0], height = quad.y[2] - quad.y[0];

    Aim aim = aimAt(perspective, quad, centerX, centerY);
    CHECK_NEAR(aim.x, SCREEN_WIDTH / 2, 1);
    CHECK_NEAR(aim.y, SCREEN_HEIGHT / 2, 1);

    aim = aimAt(perspective, quad, centerX + width / 4, centerY - height / 4);
    CHECK_NEAR(aim.x, SCREEN_WIDTH * 0.75, 6);   // 6: the lens correction moves points away from the centre a little
    CHECK_NEAR(aim.y, SCREEN_HEIGHT * 0.25, 6);

    // Outside the rectangle the map goes on: the gun points off the screen, which is how the aim reaches
    // past the edges.
    aim = aimAt(perspective, quad, centerX + width, centerY);
    CHECK(aim.x > SCREEN_WIDTH);
}

TEST(PerspectiveAdvanced, TheAimPointIsWhatMovesTheResult) {
    const Quad quad = frontView();
    OpenFIRE_Perspective perspective;
    const Aim left = aimAt(perspective, quad, quad.x[0] + 100, (quad.y[0] + quad.y[2]) / 2);
    const Aim right = aimAt(perspective, quad, quad.x[0] + 800, (quad.y[0] + quad.y[2]) / 2);
    CHECK(right.x > left.x);
    CHECK_NEAR(right.y, left.y, 4);
}

TEST(PerspectiveAdvanced, TheLensCorrectionDoesNotAccumulate) {
    // The aim point is corrected on a copy: the same frame twice gives the same answer.
    const Quad quad = inMouseSpace(rectangle(CAMERA_CENTER_X + 150, CAMERA_CENTER_Y + 100, 300, 220, 0));
    OpenFIRE_Perspective perspective;
    perspective.source(quad.x[0] + 300, quad.y[0] + 200);
    warp(perspective, quad);
    const int firstX = perspective.getX(), firstY = perspective.getY();
    for (int i = 0; i < 20; i++) warp(perspective, quad);
    CHECK_EQ(perspective.getX(), firstX);
    CHECK_EQ(perspective.getY(), firstY);
}

TEST(PerspectiveAdvanced, DegenerateLedsKeepThePreviousResult) {
    const Quad good = frontView();
    OpenFIRE_Perspective perspective;
    const Aim before = aimAt(perspective, good, good.x[0] + 200, good.y[0] + 150);

    // Four LEDs on a line, on a point, and two pairs on top of each other (a line again).
    const Quad line = {{1000, 2000, 3000, 4000}, {1500, 1500, 1500, 1500}};
    const Quad point = {{2000, 2000, 2000, 2000}, {1500, 1500, 1500, 1500}};
    const Quad doubled = {{1000, 1000, 3000, 3000}, {1000, 1000, 2000, 2000}};
    for (const Quad* bad : {&line, &point, &doubled}) {
        warp(perspective, *bad);
        CHECK_EQ(perspective.getX(), before.x);
        CHECK_EQ(perspective.getY(), before.y);
    }

    // And the next good frame works as if nothing had happened.
    warp(perspective, good);
    CHECK_EQ(perspective.getX(), before.x);
    CHECK_EQ(perspective.getY(), before.y);
}

TEST(PerspectiveAdvanced, NotANumberOrInfinityKeepsThePreviousResult) {
    const Quad good = frontView();
    OpenFIRE_Perspective perspective;
    const Aim before = aimAt(perspective, good, good.x[0] + 200, good.y[0] + 150);

    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float bad : {notANumber, infinity, -infinity}) {
        Quad quad = good;
        quad.x[3] = bad;
        warp(perspective, quad);
        CHECK_EQ(perspective.getX(), before.x);
        CHECK_EQ(perspective.getY(), before.y);
    }

    // The bad frames left nothing behind.
    warp(perspective, good);
    CHECK_EQ(perspective.getX(), before.x);
    CHECK_EQ(perspective.getY(), before.y);
}

TEST(PerspectiveAdvanced, TheScreenIsOnlyLearnedAgainAfterDeinit) {
    // The LEDs' places on the screen never change, so the matrix built from them is kept. A new screen size is
    // taken over after deinit(false) (the firmware does that after a calibration).
    const Quad quad = frontView();
    OpenFIRE_Perspective perspective;
    const float centerX = (quad.x[0] + quad.x[3]) / 2, centerY = (quad.y[0] + quad.y[3]) / 2;

    perspective.source(centerX, centerY);
    warp(perspective, quad, 1920, 1080);
    CHECK_NEAR(perspective.getX(), 960, 1);

    warp(perspective, quad, 800, 600);
    CHECK_NEAR(perspective.getX(), 960, 1);     // still the old screen
    CHECK_NEAR(perspective.getY(), 540, 1);

    perspective.deinit(false);
    warp(perspective, quad, 800, 600);
    CHECK_NEAR(perspective.getX(), 400, 1);
    CHECK_NEAR(perspective.getY(), 300, 1);

    // deinit(true) says it is up to date without any change.
    perspective.deinit(true);
    warp(perspective, quad, 1920, 1080);
    CHECK_NEAR(perspective.getX(), 400, 1);
}

TEST(PerspectiveAdvanced, ScreenCornersThatMakeNoShapeAreNotLearned) {
    // Destinations on a line can't be a screen. Nothing is output, and a valid screen given afterwards is learned.
    const Quad quad = frontView();
    OpenFIRE_Perspective perspective;
    const float centerX = (quad.x[0] + quad.x[3]) / 2, centerY = (quad.y[0] + quad.y[3]) / 2;
    perspective.source(centerX, centerY);

    perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                     0, 0, 100, 0, 0, 0, 100, 0);
    CHECK_EQ(perspective.getX(), 0);
    CHECK_EQ(perspective.getY(), 0);

    warp(perspective, quad);
    CHECK_NEAR(perspective.getX(), SCREEN_WIDTH / 2, 1);
    CHECK_NEAR(perspective.getY(), SCREEN_HEIGHT / 2, 1);
}

TEST(PerspectiveAdvanced, ScreenCornersOfAParallelogramNeedNoPerspective) {
    // A screen whose corners form a parallelogram (a sheared one) is a legal target.
    const Quad quad = frontView();
    OpenFIRE_Perspective perspective;
    const float centerX = (quad.x[0] + quad.x[3]) / 2, centerY = (quad.y[0] + quad.y[3]) / 2;
    perspective.source(centerX, centerY);
    perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                     100, 0, 1100, 0, 0, 500, 1000, 500);
    CHECK_NEAR(perspective.getX(), 550, 1);
    CHECK_NEAR(perspective.getY(), 250, 1);
}

TEST(PerspectiveAdvanced, AimingTowardsTheHorizonGivesFarButLegalPositions) {
    // The screen's top and bottom edges are parallel in the image, its sides meet at a vanishing point below it. Aiming
    // at points near the horizon line sends the result towards infinity; it has to stay a number the integer can hold
    // and not wrap around or crash.
    const Quad trapezoid = {{1000, 3000, 1500, 2500}, {1000, 1000, 2000, 2000}};
    OpenFIRE_Perspective perspective;
    for (float y = 2800; y <= 3300; y += 0.25f) {
        perspective.source(2000, y);
        warp(perspective, trapezoid);
        const long long x = perspective.getX(), screenY = perspective.getY();
        if (std::llabs(x) > 2000000000LL || std::llabs(screenY) > 2000000000LL) {
            INFO("aim at y %.2f gives %lld, %lld", y, x, screenY);
            CHECK(false);
            break;
        }
    }
}

TEST(PerspectiveAdvanced, KeepsWorkingForTheDiamondOrderOfLeds) {
    // The firmware gives the Diamond's LEDs as top, right (as top right), left (as bottom left) and bottom,
    // against the screen's top, left, right and bottom edge middles. Each LED must land on its own edge's middle.
    const Leds leds = diamond(CAMERA_CENTER_X, CAMERA_CENTER_Y, 700, 400, 0);
    const Quad quad = inMouseSpace(leds);
    OpenFIRE_Perspective perspective;
    const float screenWidth = SCREEN_WIDTH, screenHeight = SCREEN_HEIGHT;
    // Slots 0 top, 1 right, 2 bottom, 3 left -> warp(TL=top, TR=right, BL=left, BR=bottom), with the destinations
    // the firmware gives: top (w/2, 0), right (0, h/2), left (w, h/2), bottom (w/2, h). The screen is read mirrored
    // here, as in the firmware's call, and what matters is that each LED lands on its own destination.
    struct Target { int led; float x, y; };
    const Target targets[] = {{0, screenWidth / 2, 0}, {1, 0, screenHeight / 2}, {3, screenWidth, screenHeight / 2}, {2, screenWidth / 2, screenHeight}};
    for (const Target& target : targets) {
        perspective.deinit(false);
        perspective.source(quad.x[target.led], quad.y[target.led]);
        perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[3], quad.y[3], quad.x[2], quad.y[2],
                         screenWidth / 2, 0, 0, screenHeight / 2, screenWidth, screenHeight / 2, screenWidth / 2, screenHeight);
        INFO("LED %d", target.led);
        CHECK_NEAR(perspective.getX(), target.x, 2);
        CHECK_NEAR(perspective.getY(), target.y, 2);
    }
}

TEST(PerspectiveAdvanced, ScreenCornersThatAreNotAParallelogramAreReached) {
    // The firmware's screens are rectangles and rhombuses, but the warp doesn't depend on that: each LED lands
    // on its own destination also when the destinations form a general quadrilateral (which needs both perspective terms).
    const Quad quad = frontView();
    const float destinationX[4] = {100, 1800, 0, 1920}, destinationY[4] = {50, 0, 1000, 1080};
    for (int led = 0; led < 4; led++) {
        OpenFIRE_Perspective perspective;
        perspective.source(quad.x[led], quad.y[led]);
        perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                         destinationX[0], destinationY[0], destinationX[1], destinationY[1],
                         destinationX[2], destinationY[2], destinationX[3], destinationY[3]);
        INFO("LED %d", led);
        CHECK_NEAR(perspective.getX(), destinationX[led], 2);
        CHECK_NEAR(perspective.getY(), destinationY[led], 2);
    }
}
