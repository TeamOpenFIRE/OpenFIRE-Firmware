// Runs the registered unit tests: see framework.h.
#include "framework.h"

#include <cstdlib>
#include <ctime>
#include <random>
#include <vector>

#include <Arduino.h>
#include <hardware/pwm.h>
#include <pico/stdlib.h>

// The clock the library reads through micros(); tests advance it themselves.
unsigned long testMicros = 0;
unsigned long testMillis = 0;
void (*testTickHook)() = nullptr;
int testPinLevel[64];
int testPinMode[64];
int testPinOut[64];
int testAnalogOut[64];
int testAnalogIn[64];
int testPinWrites[64];
int testGpioLevel[64];
int testGpioFunction[64];
int testGpioDirection[64];
PwmSlice testPwmSlice[16];
int testGpioInvalidCalls = 0;
int testYields = 0;
unsigned testSysClockKhz = 0;

namespace {

// Puts the tests in an order that depends only on the seed. std::shuffle is not used because the standard doesn't
// say what order it produces, so the same seed could give another order with another compiler, and a failure
// reported with a seed has to be reproducible by whoever reads it.
void shuffle(std::vector<const unit::TestCase*>& tests, unsigned seed) {
    std::mt19937 random(seed);
    for (size_t i = tests.size(); i > 1; i--) {
        std::swap(tests[i - 1], tests[random() % i]);
    }
}

} // namespace

int main(int argc, char** argv) {
    const char* filter = "";
    bool list = false;
    bool ordered = false;
    bool seedGiven = false;
    unsigned seed = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--list") == 0) {
            list = true;
        } else if (strcmp(argv[i], "--ordered") == 0) {
            ordered = true;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (unsigned)strtoul(argv[++i], nullptr, 10);
            seedGiven = true;
        } else if (argv[i][0] == '-' && argv[i][1] == '-') {
            fprintf(stderr, "usage: %s [--list] [--ordered] [--seed <number>] [<text in Suite.name>]\n", argv[0]);
            return 2;
        } else {
            filter = argv[i];
        }
    }

    std::vector<const unit::TestCase*> tests;
    for (const auto& test : unit::registry()) {
        char fullName[256];
        snprintf(fullName, sizeof(fullName), "%s.%s", test.suite, test.name);
        if (strstr(fullName, filter)) tests.push_back(&test);
    }

    if (list) {
        for (const auto* test : tests) printf("%s.%s\n", test->suite, test->name);
        return 0;
    }

    // Tests that only pass after another one has run (through state they leave behind) show up when the order
    // changes. The seed is printed so that such a failure can be repeated with --seed.
    if (!ordered) {
        if (!seedGiven) seed = (unsigned)std::time(nullptr);
        shuffle(tests, seed);
        printf("Random order, seed %u (repeat it with --seed %u, or run in file order with --ordered)\n\n", seed, seed);
    }

    // A test whose name starts with DISABLED_ describes how the code ought to behave, but doesn't yet: it is
    // counted and listed as skipped, so that it is not forgotten, and not run.
    int failed = 0, skipped = 0;
    for (const auto* test : tests) {
        if (strncmp(test->name, "DISABLED_", 9) == 0) {
            printf("skipped %s.%s\n", test->suite, test->name);
            skipped++;
            continue;
        }
        unit::failuresInTest() = 0;
        unit::note()[0] = '\0';
        testMicros = 1000000;   // every test starts with the same clocks, and with all pins pulled high
        testMillis = 1000;
        testTickHook = nullptr;
        for (int pin = 0; pin < 64; pin++) {
            testPinLevel[pin] = 1;
            testPinMode[pin] = 0;
            testPinOut[pin] = 0;
            testPinWrites[pin] = 0;
            testAnalogOut[pin] = 0;
            testAnalogIn[pin] = 0;
            testGpioLevel[pin] = 0;
            testGpioFunction[pin] = 0;
            testGpioDirection[pin] = 0;
        }
        for (PwmSlice& slice : testPwmSlice) slice = PwmSlice();
        testGpioInvalidCalls = 0;
        testYields = 0;
        testSysClockKhz = 0;
        Serial.clear();
        test->function();

        if (unit::failuresInTest()) {
            printf("FAILED  %s.%s\n", test->suite, test->name);
            failed++;
        } else {
            printf("ok      %s.%s\n", test->suite, test->name);
        }
    }

    printf("\n%d tests, %d failed", (int)tests.size() - skipped, failed);
    if (skipped) printf(", %d skipped", skipped);
    printf("\n");
    if (failed && !ordered) printf("Repeat with: --seed %u\n", seed);
    return failed ? 1 : 0;
}
