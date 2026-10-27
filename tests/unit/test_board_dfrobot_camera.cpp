// Tests for DFRobotIRPositionEx, the driver of the camera, against a fake I2C bus (../stub/Wire.h) that records what the
// driver sends and answers with the frames a test has queued.
//
// These tests are built for the RP2040 (ARDUINO_ARCH_RP2040 is defined, see run.sh), which is the board
// that orders the driver's start-up calls differently from the others.
#include "framework.h"

#include <vector>

#include <Arduino.h>
#include <Wire.h>
#include <DFRobotIRPositionEx.h>

namespace {

using Camera = DFRobotIRPositionEx;

// The camera reports a point that isn't there as the largest value of both axes.
constexpr int EMPTY = 1023;

struct Point {
    int x, y, size;
};

// The camera's answer in the basic format: a header byte, then two frames of two points each, with the two highest
// bits of each coordinate packed into one byte per frame.
std::vector<uint8_t> basicFrame(const Point (&points)[4], uint8_t header = 0) {
    std::vector<uint8_t> bytes = {header};
    for (int frame = 0; frame < 2; frame++) {
        const Point& first = points[frame * 2];
        const Point& second = points[frame * 2 + 1];
        bytes.push_back(first.x & 0xFF);
        bytes.push_back(first.y & 0xFF);
        bytes.push_back(((first.y >> 8) & 3) << 6 | ((first.x >> 8) & 3) << 4 | ((second.y >> 8) & 3) << 2 | ((second.x >> 8) & 3));
        bytes.push_back(second.x & 0xFF);
        bytes.push_back(second.y & 0xFF);
    }
    return bytes;
}

// The extended format: a header byte, then three bytes for each of four points, with the sizes too.
std::vector<uint8_t> extendedFrame(const Point (&points)[4], uint8_t header = 0) {
    std::vector<uint8_t> bytes = {header};
    for (const Point& point : points) {
        bytes.push_back(point.x & 0xFF);
        bytes.push_back(point.y & 0xFF);
        bytes.push_back(((point.y >> 8) & 3) << 6 | ((point.x >> 8) & 3) << 4 | (point.size & 0xF));
    }
    return bytes;
}

const Point FOUR_LEDS[4] = {{100, 200, 5}, {900, 150, 6}, {300, 700, 7}, {1000, 767, 8}};
const Point THREE_LEDS[4] = {{100, 200, 5}, {EMPTY, EMPTY, 15}, {300, 700, 7}, {1000, 767, 8}};

// The writes the driver made, as "register=value" pairs, in order.
std::vector<std::pair<int, int>> registerWrites(const TwoWire& wire) {
    std::vector<std::pair<int, int>> writes;
    for (const TwoWire::Event& event : wire.events) {
        if (event.kind == TwoWire::Event::Transmission && event.bytes.size() == 2) writes.push_back({event.bytes[0], event.bytes[1]});
    }
    return writes;
}

int requestsMade(const TwoWire& wire) {
    int requests = 0;
    for (const TwoWire::Event& event : wire.events) requests += event.kind == TwoWire::Event::Request;
    return requests;
}

} // namespace

TEST(DfrobotCamera, StartsTheCameraInTheRightOrderAndTheRightClock) {
    TwoWire wire;
    Camera camera(wire);
    CHECK(camera.begin(1000000, Camera::DataFormat_Extended, Camera::Sensitivity_High));

    // On the RP2040 the clock is set before the bus is started.
    REQUIRE(wire.events.size() >= 2);
    CHECK(wire.events[0].kind == TwoWire::Event::SetClock);
    CHECK_EQ(wire.events[0].value, 1000000);
    CHECK(wire.events[1].kind == TwoWire::Event::Begin);

    // Stop the camera, set the sensitivity (three registers), the format, then start it.
    const std::vector<std::pair<int, int>> expected = {{0x30, 0x01}, {0x06, 0x90}, {0x08, 0x41}, {0x1A, 0x40}, {0x33, 0x33}, {0x30, 0x08}};
    CHECK(registerWrites(wire) == expected);

    // Every transmission goes to the camera's address.
    for (const TwoWire::Event& event : wire.events) {
        if (event.kind == TwoWire::Event::Transmission) CHECK_EQ(event.value, 0xB0 >> 1);
    }
}

TEST(DfrobotCamera, DefaultsAre400kHzBasicFormatAndDefaultSensitivity) {
    TwoWire wire;
    Camera camera(wire);
    CHECK(camera.begin());
    CHECK_EQ(wire.events[0].value, 400000);
    const std::vector<std::pair<int, int>> expected = {{0x30, 0x01}, {0x06, 0x90}, {0x08, 0xC0}, {0x1A, 0x40}, {0x33, 0x11}, {0x30, 0x08}};
    CHECK(registerWrites(wire) == expected);
}

TEST(DfrobotCamera, TheDriverWaitsForTheCameraBetweenWrites) {
    TwoWire wire;
    Camera camera(wire);
    const unsigned long before = testMillis;
    camera.begin();
    // 10 ms after each of the five writes that are followed by a pause, and 100 ms at the end.
    CHECK(testMillis - before >= 5 * 10 + 100);
}

TEST(DfrobotCamera, SensitivityLevelsWriteTheirRegisters) {
    const int expectedFor[3][3] = {{0x90, 0xC0, 0x40}, {0x90, 0x41, 0x40}, {0xFF, 0x0C, 0x00}};
    for (int level = 0; level < 3; level++) {
        TwoWire wire;
        Camera camera(wire);
        camera.sensitivityLevel((Camera::Sensitivity_e)level);
        const std::vector<std::pair<int, int>> expected = {{0x06, expectedFor[level][0]}, {0x08, expectedFor[level][1]}, {0x1A, expectedFor[level][2]}};
        INFO("level %d", level);
        CHECK(registerWrites(wire) == expected);
    }

    // A level beyond the highest is the highest.
    TwoWire wire;
    Camera camera(wire);
    camera.sensitivityLevel((Camera::Sensitivity_e)7);
    const std::vector<std::pair<int, int>> highest = {{0x06, 0xFF}, {0x08, 0x0C}, {0x1A, 0x00}};
    CHECK(registerWrites(wire) == highest);
}

TEST(DfrobotCamera, DataFormatSelectsTheModeRegister) {
    TwoWire wire;
    Camera camera(wire);
    camera.dataFormat(Camera::DataFormat_Basic);
    camera.dataFormat(Camera::DataFormat_Extended);
    const std::vector<std::pair<int, int>> expected = {{0x33, 0x11}, {0x33, 0x33}};
    CHECK(registerWrites(wire) == expected);
}

TEST(DfrobotCamera, BeginFailsWhenTheCameraDoesNotAnswer) {
    TwoWire wire;
    wire.transmissionResults = {2};     // the first transmission is not acknowledged
    Camera camera(wire);
    CHECK(!camera.begin());
    // Nothing else is tried.
    CHECK_EQ(registerWrites(wire).size(), 1);
}

TEST(DfrobotCamera, RequestsAskForTheRightNumberOfBytes) {
    TwoWire wire;
    Camera camera(wire);
    camera.requestPositionBasic();
    camera.requestPositionExtended();

    // Each request first writes the data register (0x36), then asks for 11 (basic) or 13 (extended) bytes.
    REQUIRE(wire.events.size() == 4);
    CHECK(wire.events[0].kind == TwoWire::Event::Transmission);
    CHECK(wire.events[0].bytes == std::vector<uint8_t>({0x36}));
    CHECK(wire.events[1].kind == TwoWire::Event::Request);
    CHECK_EQ(wire.events[1].value, 11);
    CHECK_EQ(wire.events[3].value, 13);
}

TEST(DfrobotCamera, BasicFrameIsUnpackedWithItsSeenFlags) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses.push_back(basicFrame(FOUR_LEDS));
    camera.requestPositionBasic();
    CHECK(camera.availableBasic());

    for (int led = 0; led < 4; led++) {
        INFO("LED %d", led);
        CHECK_EQ(camera.x(led), FOUR_LEDS[led].x);
        CHECK_EQ(camera.y(led), FOUR_LEDS[led].y);
    }
    CHECK_EQ(camera.seen(), 0x0F);
    CHECK(camera.xPositions() == &camera.xPositions()[0]);
    CHECK_EQ(camera.xPositions()[3], 1000);
    CHECK_EQ(camera.yPositions()[2], 700);
}

TEST(DfrobotCamera, APointOutsideTheSensorIsNotSeenAndKeepsItsLastPosition) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses.push_back(basicFrame(FOUR_LEDS));
    camera.requestPositionBasic();
    camera.availableBasic();

    // The second point is reported as empty (y of 1023, above the sensor's last row of 767).
    wire.responses.push_back(basicFrame(THREE_LEDS));
    camera.requestPositionBasic();
    CHECK(camera.availableBasic());
    CHECK_EQ(camera.seen(), 0x0D);
    CHECK_EQ(camera.x(1), FOUR_LEDS[1].x);      // still the last seen position
    CHECK_EQ(camera.y(1), FOUR_LEDS[1].y);
    CHECK_EQ(camera.x(0), 100);

    // A row of exactly 767 is still on the sensor, 768 isn't.
    const Point edge[4] = {{10, 767, 0}, {20, 768, 0}, {30, 0, 0}, {40, 1023, 0}};
    wire.responses.push_back(basicFrame(edge));
    camera.requestPositionBasic();
    camera.availableBasic();
    CHECK_EQ(camera.seen(), 0x05);
}

TEST(DfrobotCamera, TheNoSeenVariantUnpacksEverythingAndLeavesTheFlags) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses.push_back(basicFrame(FOUR_LEDS));
    camera.requestPositionBasic();
    camera.availableBasic();
    CHECK_EQ(camera.seen(), 0x0F);

    wire.responses.push_back(basicFrame(THREE_LEDS));
    camera.requestPositionBasic();
    CHECK(camera.availableBasicNoSeen());
    CHECK_EQ(camera.seen(), 0x0F);          // not updated
    CHECK_EQ(camera.x(1), EMPTY);           // the empty point is unpacked as it is
    CHECK_EQ(camera.y(1), EMPTY);
}

TEST(DfrobotCamera, ExtendedFrameCarriesTheSizes) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses.push_back(extendedFrame(FOUR_LEDS));
    camera.requestPositionExtended();
    CHECK(camera.availableExtended());
    for (int led = 0; led < 4; led++) {
        INFO("LED %d", led);
        CHECK_EQ(camera.x(led), FOUR_LEDS[led].x);
        CHECK_EQ(camera.y(led), FOUR_LEDS[led].y);
        CHECK_EQ(camera.size(led), FOUR_LEDS[led].size);
    }
    CHECK_EQ(camera.seen(), 0x0F);
    CHECK_EQ(camera.sizes()[2], 7);

    // An empty point keeps its position and size from before, and isn't seen.
    wire.responses.push_back(extendedFrame(THREE_LEDS));
    camera.requestPositionExtended();
    camera.availableExtended();
    CHECK_EQ(camera.seen(), 0x0D);
    CHECK_EQ(camera.x(1), 900);
    CHECK_EQ(camera.size(1), 6);

    // The no-seen variant unpacks all, empty points with their size of 15.
    wire.responses.push_back(extendedFrame(THREE_LEDS));
    camera.requestPositionExtended();
    CHECK(camera.availableExtendedNoSeen());
    CHECK_EQ(camera.seen(), 0x0D);
    CHECK_EQ(camera.x(1), EMPTY);
    CHECK_EQ(camera.size(1), 15);
}

TEST(DfrobotCamera, AFrameOfTheWrongLengthIsRefusedAndFlushed) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses.push_back(std::vector<uint8_t>(7, 0));
    camera.requestPositionBasic();
    CHECK(!camera.availableBasic());
    CHECK_EQ(wire.available(), 0);      // what was there has been read away

    wire.responses.push_back(std::vector<uint8_t>(11, 0));
    camera.requestPositionExtended();
    CHECK(!camera.availableExtended());
    CHECK(!camera.availableBasicNoSeen());
    CHECK(!camera.availableExtendedNoSeen());
}

TEST(DfrobotCamera, AtomicBasicReadAcceptsTwoMatchingReads) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses = {basicFrame(FOUR_LEDS), basicFrame(FOUR_LEDS)};
    CHECK_EQ(camera.basicAtomic(Camera::Retry_0), Camera::Error_Success);
    CHECK_EQ(camera.seen(), 0x0F);
    CHECK_EQ(camera.x(3), 1000);
    CHECK_EQ(requestsMade(wire), 2);
}

TEST(DfrobotCamera, TheHeaderByteIsNotCompared) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses = {basicFrame(FOUR_LEDS, 0), basicFrame(FOUR_LEDS, 0x55)};
    CHECK_EQ(camera.basicAtomic(Camera::Retry_0), Camera::Error_Success);
}

TEST(DfrobotCamera, AtomicReadRetriesAsOftenAsAsked) {
    // The camera updates its registers while they are read; a read that differs from the one before is a torn
    // frame. The two readings are compared, and the driver tries again as many times as the retry option says.
    const Point other[4] = {{101, 200, 5}, {900, 150, 6}, {300, 700, 7}, {1000, 767, 8}};
    struct Case {
        Camera::Retry_e retry;
        int differingReads;     // how many reads at the start differ from the next one
        int expectedResult;
        int expectedRequests;
    };
    const Case cases[] = {
        {Camera::Retry_0, 0, Camera::Error_Success, 2},
        {Camera::Retry_0, 1, Camera::Error_DataMismatch, 2},
        {Camera::Retry_1, 1, Camera::Error_Success, 3},
        {Camera::Retry_1, 2, Camera::Error_DataMismatch, 3},
        {Camera::Retry_2, 2, Camera::Error_Success, 4},
        {Camera::Retry_2, 3, Camera::Error_DataMismatch, 4},
        {Camera::Retry_0s, 1, Camera::Error_SuccessMismatch, 2},
        {Camera::Retry_1s, 2, Camera::Error_SuccessMismatch, 3},
        {Camera::Retry_2s, 3, Camera::Error_SuccessMismatch, 4},
    };
    for (const Case& test : cases) {
        TwoWire wire;
        Camera camera(wire);
        // Alternating frames that differ, then (if the case has one) one that matches the one before.
        for (int read = 0; read < 4; read++) {
            wire.responses.push_back(basicFrame(read < test.differingReads ? (read % 2 ? FOUR_LEDS : other) : (test.differingReads % 2 ? FOUR_LEDS : other)));
        }
        INFO("retry %d, %d differing", (int)test.retry, test.differingReads);
        CHECK_EQ(camera.basicAtomic(test.retry), test.expectedResult);
        CHECK_EQ(requestsMade(wire), test.expectedRequests);
    }
}

TEST(DfrobotCamera, AMismatchAcceptedWithTheSuffixUsesTheLastFrame) {
    TwoWire wire;
    Camera camera(wire);
    const Point first[4] = {{100, 200, 1}, {900, 150, 1}, {300, 700, 1}, {1000, 767, 1}};
    const Point second[4] = {{110, 210, 1}, {910, 160, 1}, {310, 710, 1}, {990, 757, 1}};
    wire.responses = {basicFrame(first), basicFrame(second)};
    CHECK_EQ(camera.basicAtomic(Camera::Retry_0s), Camera::Error_SuccessMismatch);
    CHECK_EQ(camera.x(0), 110);     // the second reading
    CHECK_EQ(camera.y(3), 757);
}

TEST(DfrobotCamera, ABusErrorEndsTheAtomicReadWithAnError) {
    for (int silentRead : {0, 1}) {
        TwoWire wire;
        Camera camera(wire);
        // The camera answers the first read, and then not.
        if (silentRead == 1) wire.responses.push_back(basicFrame(FOUR_LEDS));
        INFO("answers up to read %d", silentRead);
        CHECK_EQ(camera.basicAtomic(Camera::Retry_2), Camera::Error_IICerror);
    }
}

TEST(DfrobotCamera, AtomicExtendedReadWorksLikeTheBasicOne) {
    TwoWire wire;
    Camera camera(wire);
    wire.responses = {extendedFrame(FOUR_LEDS), extendedFrame(FOUR_LEDS)};
    CHECK_EQ(camera.extendedAtomic(Camera::Retry_0), Camera::Error_Success);
    CHECK_EQ(camera.seen(), 0x0F);
    CHECK_EQ(camera.size(2), 7);
    CHECK_EQ(requestsMade(wire), 2);

    // A torn frame is retried once with Retry_1 and then taken.
    TwoWire second;
    Camera other(second);
    const Point torn[4] = {{100, 201, 5}, {900, 150, 6}, {300, 700, 7}, {1000, 767, 8}};
    second.responses = {extendedFrame(torn), extendedFrame(FOUR_LEDS), extendedFrame(FOUR_LEDS)};
    CHECK_EQ(other.extendedAtomic(Camera::Retry_1), Camera::Error_Success);
    CHECK_EQ(other.y(0), 200);

    // Mismatching to the end: refused, or taken with the suffix.
    TwoWire third;
    Camera refusing(third);
    third.responses = {extendedFrame(torn), extendedFrame(FOUR_LEDS)};
    CHECK_EQ(refusing.extendedAtomic(Camera::Retry_0), Camera::Error_DataMismatch);
    TwoWire fourth;
    Camera accepting(fourth);
    fourth.responses = {extendedFrame(torn), extendedFrame(FOUR_LEDS)};
    CHECK_EQ(accepting.extendedAtomic(Camera::Retry_0s), Camera::Error_SuccessMismatch);
    CHECK_EQ(accepting.y(0), 200);

    // No answer from the bus.
    TwoWire silent;
    Camera deaf(silent);
    CHECK_EQ(deaf.extendedAtomic(), Camera::Error_IICerror);
    silent.responses.push_back(extendedFrame(FOUR_LEDS));
    CHECK_EQ(deaf.extendedAtomic(), Camera::Error_IICerror);
}

TEST(DfrobotCamera, TheHighestCoordinatesAreDecodedFromTheirPackedBits) {
    // x and y have 10 bits: the low eight in their own byte, the top two packed with the others.
    const Point corners[4] = {{1023, 0, 0}, {0, 767, 0}, {256, 512, 0}, {767, 255, 0}};
    TwoWire wire;
    Camera camera(wire);
    wire.responses.push_back(basicFrame(corners));
    camera.requestPositionBasic();
    camera.availableBasic();
    for (int led = 0; led < 4; led++) {
        INFO("LED %d", led);
        CHECK_EQ(camera.x(led), corners[led].x);
        CHECK_EQ(camera.y(led), corners[led].y);
    }
}
