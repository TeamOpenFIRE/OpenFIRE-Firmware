// A minimal unit test framework: enough for the library's tests, with no dependencies to install.
//
//   TEST(Suite, name) { ... }            defines a test; it is registered automatically
//   CHECK(condition)                     records a failure if false, and goes on with the test
//   CHECK_EQ(actual, expected)           integers (or anything printable with %lld)
//   CHECK_NEAR(actual, expected, tol)    floating point: |actual - expected| <= tol
//   CHECK_STREQ(actual, expected)        strings (std::string or C strings)
//   REQUIRE(condition)                   like CHECK, but ends the test at once (use it when the rest depends on it)
//   INFO(...)                            a printf-style note that is printed with the next failure of the test
//
// Run the tests with main.cpp's main(): `unit_tests` runs all of them, `unit_tests <text>` those whose
// "Suite.name" contains the text, and `unit_tests --list` lists them. The order is random, to reveal tests that
// depend on each other; the seed is printed, `--seed <number>` repeats an order and `--ordered` keeps the file order.
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace unit {

struct TestCase {
    const char* suite;
    const char* name;
    void (*function)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> tests;
    return tests;
}

// State of the test that is running.
inline int& failuresInTest() { static int count = 0; return count; }
inline char* note() { static char text[256] = ""; return text; }

struct Registrar {
    Registrar(const char* suite, const char* name, void (*function)()) {
        registry().push_back({suite, name, function});
    }
};

inline void fail(const char* file, int line, const char* message) {
    printf("    %s:%d: %s\n", file, line, message);
    if (note()[0]) printf("      (%s)\n", note());
    failuresInTest()++;
}

inline void failCondition(const char* file, int line, const char* macro, const char* condition) {
    char message[200];
    snprintf(message, sizeof(message), "%s(%s) is false", macro, condition);
    fail(file, line, message);
}

} // namespace unit

#define TEST(suite, name) \
    static void test_##suite##_##name(); \
    static unit::Registrar registrar_##suite##_##name(#suite, #name, test_##suite##_##name); \
    static void test_##suite##_##name()

#define INFO(...) snprintf(unit::note(), 256, __VA_ARGS__)

#define CHECK(condition) \
    do { if (!(condition)) unit::failCondition(__FILE__, __LINE__, "CHECK", #condition); } while (0)

#define REQUIRE(condition) \
    do { if (!(condition)) { unit::failCondition(__FILE__, __LINE__, "REQUIRE", #condition); return; } } while (0)

#define CHECK_EQ(actual, expected) \
    do { \
        const long long actualValue = (long long)(actual), expectedValue = (long long)(expected); \
        if (actualValue != expectedValue) { \
            char message[200]; \
            snprintf(message, sizeof(message), "CHECK_EQ(%s, %s): %lld != %lld", #actual, #expected, actualValue, expectedValue); \
            unit::fail(__FILE__, __LINE__, message); \
        } \
    } while (0)

#define CHECK_NEAR(actual, expected, tolerance) \
    do { \
        const double actualValue = (double)(actual), expectedValue = (double)(expected), toleranceValue = (double)(tolerance); \
        if (!(std::fabs(actualValue - expectedValue) <= toleranceValue)) { \
            char message[200]; \
            snprintf(message, sizeof(message), "CHECK_NEAR(%s, %s, %s): %.6g vs %.6g (tolerance %.6g)", \
                     #actual, #expected, #tolerance, actualValue, expectedValue, toleranceValue); \
            unit::fail(__FILE__, __LINE__, message); \
        } \
    } while (0)

#define CHECK_STREQ(actual, expected) \
    do { \
        const std::string actualText = (actual), expectedText = (expected); \
        if (actualText != expectedText) { \
            char message[300]; \
            snprintf(message, sizeof(message), "CHECK_STREQ(%s, %s): \"%s\" != \"%s\"", #actual, #expected, actualText.c_str(), expectedText.c_str()); \
            unit::fail(__FILE__, __LINE__, message); \
        } \
    } while (0)
