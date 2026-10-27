// Minimal stand-in for Arduino.h, to build the libraries on a desktop computer.
//
// It defines the same constant and function-like macros as the Arduino core (ArduinoCore-API/api/Common.h), so
// that a name in the library that clashes with one of them fails to build here too, and not only on the board.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cfloat>
#include <cstring>

using std::isfinite;
using std::isnan;

#define PI          3.1415926535897932384626433832795
#define HALF_PI     1.5707963267948966192313216916398
#define TWO_PI      6.283185307179586476925286766559
#define DEG_TO_RAD  0.017453292519943295769236907684886
#define RAD_TO_DEG  57.295779513082320876798154814105
#define EULER       2.718281828459045235360287471352

#define SERIAL      0x0
#define DISPLAY     0x1

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) ((x)*(x))
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet((value), (bit)) : bitClear((value), (bit)))
#define bit(b) (1UL << (b))

// The test driver advances this clock itself, so that the 1€ filter sees reproducible frame times.
extern unsigned long testMicros;
inline unsigned long micros() { return testMicros; }

// The Arduino core's integer map(), which rescales a value from one range to another.
inline long map(long x, long fromLow, long fromHigh, long toLow, long toHigh) {
    return (x - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
}

// Time and pins, for the libraries that read buttons and wait for the camera. The tests set these themselves:
// millis() is testMillis (delay() advances it), and digitalRead() returns testPinLevel for the pin, which
// the buttons' pull-ups hold high until a test pulls a pin low.
extern unsigned long testMillis;
extern int testPinLevel[64];
extern int testPinMode[64];
constexpr int INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
constexpr int LOW = 0, HIGH = 1;
// A test can set a hook that runs whenever the code asks for the time or waits, to play out the world around a loop that
// would otherwise wait for ever for a button or the camera (see unit/world.h). The hook advances the clock itself.
extern void (*testTickHook)();
inline unsigned long millis() { if (testTickHook) testTickHook(); return testMillis; }
inline void delay(unsigned long milliseconds) { testMillis += milliseconds; if (testTickHook) testTickHook(); }
inline void pinMode(int pin, int mode) { testPinMode[pin] = mode; }
inline int digitalRead(int pin) { return testPinLevel[pin]; }

typedef uint8_t byte;
typedef unsigned int uint;
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#ifdef ARDUINO_RASPBERRY_PI_PICO
#define PIN_LED 25
#endif
#define BLACK 0
#define WHITE 1

// Outputs: what the firmware drove each pin to, and what the analog inputs read. A test sets testAnalogIn, and reads
// testPinOut (digital) and testAnalogOut (PWM).
extern int testPinOut[64];
extern int testAnalogOut[64];
extern int testAnalogIn[64];
extern int testPinWrites[64];
inline void digitalWrite(int pin, int level) { testPinOut[pin] = level; testPinLevel[pin] = level; testPinWrites[pin]++; }
inline void analogWrite(int pin, int value) { testAnalogOut[pin] = value; }
inline int analogRead(int pin) { return testAnalogIn[pin]; }
inline void analogReadResolution(int bits) { (void)bits; }
// yield() is called while code waits for the USB port: the tests count the calls.
extern int testYields;
inline void yield() { testYields++; }

#define PROGMEM
#define pgm_read_byte(address) (*(const uint8_t*)(address))

// The base class of the Arduino streams, as far as the keyboard uses it: a write error flag.
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
    int getWriteError() const { return writeError; }
    void clearWriteError() { writeError = 0; }
protected:
    void setWriteError(int error = 1) { writeError = error; }
private:
    int writeError = 0;
};

#include <cstdarg>
#include <cstdio>
#include <string>

// The USB serial port. Output is collected as a string; input is what a test puts in `input`.
class SerialFake {
public:
    std::string output;
    std::string input;
    size_t inputAt = 0;

    void begin(unsigned long) {}
    void flush() {}
    explicit operator bool() const { return true; }
    int available() const { return (int)(input.size() - inputAt); }
    int read() { return inputAt < input.size() ? (unsigned char)input[inputAt++] : -1; }
    int peek() const { return inputAt < input.size() ? (unsigned char)input[inputAt] : -1; }
    size_t write(uint8_t byte) { output.push_back((char)byte); return 1; }
    size_t write(const uint8_t* buffer, size_t length) { output.append((const char*)buffer, length); return length; }
    size_t write(const char* text) { output += text; return strlen(text); }
    size_t write(const char* buffer, size_t length) { output.append(buffer, length); return length; }
    size_t print(const char* text) { output += text; return strlen(text); }
    size_t print(char letter) { output.push_back(letter); return 1; }
    size_t print(int value) { output += std::to_string(value); return 1; }
    size_t print(unsigned int value) { output += std::to_string(value); return 1; }
    size_t print(long value) { output += std::to_string(value); return 1; }
    size_t print(unsigned long value) { output += std::to_string(value); return 1; }
    size_t print(float value, int digits = 2) { char text[32]; snprintf(text, sizeof(text), "%.*f", digits, value); output += text; return 1; }
    size_t print(double value, int digits = 2) { return print((float)value, digits); }
    size_t println() { output += "\r\n"; return 2; }
    template <class T> size_t println(T value) { print(value); return println(); }
    template <class T> size_t println(T value, int digits) { print(value, digits); return println(); }
    int printf(const char* format, ...) __attribute__((format(printf, 2, 3))) {
        char text[512];
        va_list arguments;
        va_start(arguments, format);
        const int length = vsnprintf(text, sizeof(text), format, arguments);
        va_end(arguments);
        output += text;
        return length;
    }
    void clear() { output.clear(); input.clear(); inputAt = 0; }
};
inline SerialFake Serial;
