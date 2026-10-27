// Tests for the classic OpenFIRE_Perspective, the warp the RP2040 runs. It shares its name with the advanced one, so
// these tests are built on their own, with NO_ADVANCED_TRACKING (see run.sh). The classic warp has no lens
// correction and no protection against degenerate input: it is meant to be given four LEDs that form a quadrilateral.
#include "framework.h"

#include <cmath>

#include <Arduino.h>
#include <OpenFIRE_Perspective.h>

#include "support.h"

using namespace scene;

#ifdef ADVANCED_TRACKING
#error "The classic tracker tests must be built with NO_ADVANCED_TRACKING"
#endif

namespace {

constexpr float SCREEN_WIDTH = 1920, SCREEN_HEIGHT = 1080;

struct Quad {
    float x[4], y[4];   // top left, top right, bottom left, bottom right
};

Quad inMouseSpace(const Leds& leds) {
    Quad quad;
    for (int i = 0; i < 4; i++) {
        quad.x[i] = (float)(leds[i].x * CamToMouseMult);
        quad.y[i] = (float)(leds[i].y * CamToMouseMult);
    }
    return quad;
}

void warp(OpenFIRE_Perspective& perspective, const Quad& quad) {
    perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                     0, 0, SCREEN_WIDTH, 0, 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
}

} // namespace

TEST(PerspectiveClassic, EachLedLandsOnItsCornerOfTheScreen) {
    struct Scene {
        const char* name;
        Leds leds;
    };
    const Scene scenes[] = {
        {"front view", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 0)},
        {"off centre", rectangle(CAMERA_CENTER_X + 120, CAMERA_CENTER_Y - 90, 300, 220, 0)},
        {"rolled", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 30)},
        {"upside down", rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 180)},
        {"from the side", viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 60, 0, 0)},
        {"from below, rolled", viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, -50, 30, 25)},
    };
    const float cornersX[4] = {0, SCREEN_WIDTH, 0, SCREEN_WIDTH};
    const float cornersY[4] = {0, 0, SCREEN_HEIGHT, SCREEN_HEIGHT};

    for (const Scene& scene : scenes) {
        const Quad quad = inMouseSpace(scene.leds);
        for (int corner = 0; corner < 4; corner++) {
            OpenFIRE_Perspective perspective;
            perspective.source(quad.x[corner], quad.y[corner]);
            warp(perspective, quad);
            INFO("%s, LED %d", scene.name, corner);
            CHECK_NEAR(perspective.getX(), cornersX[corner], 2);
            CHECK_NEAR(perspective.getY(), cornersY[corner], 2);
        }
    }
}

TEST(PerspectiveClassic, TheMiddleOfTheScreenIsWhereTheDiagonalsCross) {
    // Without lens correction a projective map keeps straight lines straight exactly, so the crossing of the
    // diagonals is the middle of the screen in every view, to within the rounding of single-precision floats.
    const double views[][3] = {{0, 0, 0}, {30, 0, 0}, {60, 0, 0}, {-60, 0, 20}, {0, -45, 0}, {45, 30, -40}};
    for (const auto& view : views) {
        const Quad quad = inMouseSpace(viewedRectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, view[0], view[1], view[2]));
        const double firstDirectionX = quad.x[3] - quad.x[0], firstDirectionY = quad.y[3] - quad.y[0];
        const double secondDirectionX = quad.x[1] - quad.x[2], secondDirectionY = quad.y[1] - quad.y[2];
        const double determinant = firstDirectionX * secondDirectionY - firstDirectionY * secondDirectionX;
        const double along = ((quad.x[2] - quad.x[0]) * secondDirectionY - (quad.y[2] - quad.y[0]) * secondDirectionX) / determinant;
        OpenFIRE_Perspective perspective;
        perspective.source((float)(quad.x[0] + along * firstDirectionX), (float)(quad.y[0] + along * firstDirectionY));
        warp(perspective, quad);
        INFO("yaw %.0f, pitch %.0f, roll %.0f", view[0], view[1], view[2]);
        CHECK_NEAR(perspective.getX(), SCREEN_WIDTH / 2, 0.005 * SCREEN_WIDTH);
        CHECK_NEAR(perspective.getY(), SCREEN_HEIGHT / 2, 0.005 * SCREEN_HEIGHT);
    }
}

TEST(PerspectiveClassic, FrontViewIsALinearMapAndGoesOnOutsideTheLeds) {
    const Quad quad = inMouseSpace(rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 0));
    const float centerX = (quad.x[0] + quad.x[3]) / 2, centerY = (quad.y[0] + quad.y[3]) / 2;
    const float width = quad.x[1] - quad.x[0], height = quad.y[2] - quad.y[0];
    OpenFIRE_Perspective perspective;

    perspective.source(centerX + width / 4, centerY - height / 4);
    warp(perspective, quad);
    CHECK_NEAR(perspective.getX(), SCREEN_WIDTH * 0.75, 1);
    CHECK_NEAR(perspective.getY(), SCREEN_HEIGHT * 0.25, 1);

    perspective.source(centerX + width, centerY);
    warp(perspective, quad);
    CHECK(perspective.getX() > SCREEN_WIDTH);
}

TEST(PerspectiveClassic, TheScreenIsOnlyLearnedAgainAfterDeinit) {
    const Quad quad = inMouseSpace(rectangle(CAMERA_CENTER_X, CAMERA_CENTER_Y, 400, 300, 0));
    const float centerX = (quad.x[0] + quad.x[3]) / 2, centerY = (quad.y[0] + quad.y[3]) / 2;
    OpenFIRE_Perspective perspective;
    perspective.source(centerX, centerY);

    warp(perspective, quad);
    CHECK_NEAR(perspective.getX(), 960, 1);

    perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                     0, 0, 800, 0, 0, 600, 800, 600);
    CHECK_NEAR(perspective.getX(), 960, 1);     // still the old screen

    perspective.deinit(false);
    perspective.warp(quad.x[0], quad.y[0], quad.x[1], quad.y[1], quad.x[2], quad.y[2], quad.x[3], quad.y[3],
                     0, 0, 800, 0, 0, 600, 800, 600);
    CHECK_NEAR(perspective.getX(), 400, 1);
    CHECK_NEAR(perspective.getY(), 300, 1);
}
