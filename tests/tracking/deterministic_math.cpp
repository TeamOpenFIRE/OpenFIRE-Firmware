// sin, cos, tan, sinf, cosf and atan2f that give the same results on every platform, for the tracking test.
//
// The C library's versions are only required to be accurate to within a few units of the last place, and they differ between
// versions of glibc and even between CPUs (it picks code for the CPU's fused multiply-add). A change in the last bit of an
// angle can flip a rounded position, so the output of the tracking could not be compared with a hash recorded elsewhere.
// These functions use nothing but the +, -, * and / and square root of IEEE arithmetic, which are the same everywhere (the test
// is built without fused multiply-add and without fast-math), so the output depends on the tracking code alone.
//
// They replace the C library's by their names: the tracking test is built with -fno-builtin, so that the compiler calls
// them instead of using its own, and the linker prefers them to the library's. They are accurate to about 1e-15, far
// more than the float functions that the tracking code asks for need.

#include <cstdint>

namespace {

constexpr double PI = 3.14159265358979323846;
constexpr double HALF_PI = 1.57079632679489661923;

// 1 / (2k+1)! and 1 / (2k)! as the Taylor series of the sine and the cosine need, worked out by the compiler.
struct Coefficients {
    double sine[14], cosine[14];
    constexpr Coefficients() : sine(), cosine() {
        double factorial = 1;
        for (int term = 0; term < 14; term++) {
            // term k: sine 1/(2k+1)!, cosine 1/(2k)!
            const int cosineOrder = 2 * term, sineOrder = 2 * term + 1;
            double cosineFactorial = 1, sineFactorial = 1;
            for (int i = 2; i <= cosineOrder; i++) cosineFactorial *= i;
            for (int i = 2; i <= sineOrder; i++) sineFactorial *= i;
            cosine[term] = 1.0 / cosineFactorial;
            sine[term] = 1.0 / sineFactorial;
        }
        (void)factorial;
    }
};
constexpr Coefficients COEFFICIENTS;

// The sine and the cosine of an angle of at most a quarter turn (pi/4 either way).
double sineOfSmall(double angle) {
    const double squared = angle * angle;
    double sum = 0;
    for (int term = 13; term >= 0; term--) {
        // the sign alternates: + - + - ...
        const double coefficient = (term & 1) ? -COEFFICIENTS.sine[term] : COEFFICIENTS.sine[term];
        sum = sum * squared + coefficient;
    }
    return sum * angle;
}

double cosineOfSmall(double angle) {
    const double squared = angle * angle;
    double sum = 0;
    for (int term = 13; term >= 0; term--) {
        const double coefficient = (term & 1) ? -COEFFICIENTS.cosine[term] : COEFFICIENTS.cosine[term];
        sum = sum * squared + coefficient;
    }
    return sum;
}

bool isNotANumber(double value) { return value != value; }
bool isInfinite(double value) { return value - value != 0 && value == value; }
bool isNegative(double value) { return value < 0 || (value == 0 && 1.0 / value < 0); }
double withSignOf(double magnitude, double sign) { return isNegative(sign) ? -magnitude : magnitude; }

// Splits an angle into a whole number of quarter turns and what is left (at most pi/4 either way). The quarter turn is
// split in three parts, each with few enough bits that its product with the number of quarter turns is exact.
double reduce(double angle, long& quarterTurns) {
    if (angle > 1e6 || angle < -1e6) {
        // Far beyond what the tracking uses; still a definite answer.
        const double turns = angle / (2 * PI);
        angle -= 2 * PI * (double)(long)turns;
    }
    const double scaled = angle * 0.63661977236758134308;       // 2 / pi
    const double rounded = (double)(long)(scaled + (scaled < 0 ? -0.5 : 0.5));
    quarterTurns = (long)rounded;
    double remainder = angle - rounded * 1.57079632673412561417e+00;
    remainder -= rounded * 6.07710050630396597660e-11;
    remainder -= rounded * 2.02226624871116645580e-21;
    remainder -= rounded * 8.47842766036889956997e-32;
    return remainder;
}

double sineOf(double angle) {
    if (isNotANumber(angle) || isInfinite(angle)) return angle - angle;
    long quarterTurns;
    const double remainder = reduce(angle, quarterTurns);
    switch (quarterTurns & 3) {
        case 0: return sineOfSmall(remainder);
        case 1: return cosineOfSmall(remainder);
        case 2: return -sineOfSmall(remainder);
        default: return -cosineOfSmall(remainder);
    }
}

double cosineOf(double angle) {
    if (isNotANumber(angle) || isInfinite(angle)) return angle - angle;
    long quarterTurns;
    const double remainder = reduce(angle, quarterTurns);
    switch (quarterTurns & 3) {
        case 0: return cosineOfSmall(remainder);
        case 1: return -sineOfSmall(remainder);
        case 2: return -cosineOfSmall(remainder);
        default: return sineOfSmall(remainder);
    }
}

// The arc tangent of a number of at least 0: the argument is halved three times (atan x = 2 atan(x / (1 + sqrt(1 + x^2)))) so that the
// series converges quickly, with the number at most about 0.1 then.
double arcTangent(double value) {
    if (value > 1) return HALF_PI - arcTangent(1.0 / value);
    for (int halving = 0; halving < 3; halving++) value = value / (1.0 + __builtin_sqrt(1.0 + value * value));
    const double squared = value * value;
    double sum = 0;
    for (int term = 14; term >= 0; term--) {
        const double coefficient = (term & 1) ? -1.0 / (2 * term + 1) : 1.0 / (2 * term + 1);
        sum = sum * squared + coefficient;
    }
    return 8.0 * (sum * value);
}

// atan2 as the C standard gives it, with its special cases for zeros, infinities and not-a-numbers.
double arcTangent2(double y, double x) {
    if (isNotANumber(y) || isNotANumber(x)) return y + x;
    if (y == 0) return (x > 0 || (x == 0 && !isNegative(x))) ? y : withSignOf(PI, y);
    if (x == 0) return withSignOf(HALF_PI, y);
    if (isInfinite(x)) {
        if (isInfinite(y)) return withSignOf(x > 0 ? PI / 4 : 3 * PI / 4, y);
        return x > 0 ? withSignOf(0.0, y) : withSignOf(PI, y);
    }
    if (isInfinite(y)) return withSignOf(HALF_PI, y);
    const double ratio = (y < 0 ? -y : y) / (x < 0 ? -x : x);
    const double angle = arcTangent(ratio);
    return withSignOf(x > 0 ? angle : PI - angle, y);
}

} // namespace

extern "C" {

double sin(double angle) { return sineOf(angle); }
double cos(double angle) { return cosineOf(angle); }
double tan(double angle) { return sineOf(angle) / cosineOf(angle); }
float sinf(float angle) { return (float)sineOf(angle); }
float cosf(float angle) { return (float)cosineOf(angle); }
float atan2f(float y, float x) { return (float)arcTangent2(y, x); }

}
