// A fake I2C bus for the camera library: it records what is sent, and answers requests with frames the test has queued.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

class TwoWire {
public:
    // What the library did, in order: begin(), setClock(), and each transmission with its bytes.
    struct Event {
        enum Kind { Begin, SetClock, Transmission, Request } kind;
        uint32_t value = 0;                 // the clock, the address, or the number of bytes requested
        std::vector<uint8_t> bytes;         // the bytes of a transmission
    };
    std::vector<Event> events;

    // The camera's answers: each requestFrom() takes the next one. With none left, the answer is empty.
    std::deque<std::vector<uint8_t>> responses;
    // If set, the answer to every request instead of the queued ones: the camera's frame for the moment.
    std::function<std::vector<uint8_t>()> responder;
    // The result of each endTransmission(), taken in turn; 0 (success) once they are used up.
    std::deque<uint8_t> transmissionResults;

    void begin() { events.push_back({Event::Begin, 0, {}}); }
    void end() { ended++; }
    void setSDA(int pin) { sdaPin = pin; }
    void setSCL(int pin) { sclPin = pin; }
    int sdaPin = -1, sclPin = -1, ended = 0;
    void setClock(uint32_t clock) { events.push_back({Event::SetClock, clock, {}}); }

    void beginTransmission(int address) {
        pending = {Event::Transmission, (uint32_t)address, {}};
    }
    size_t write(uint8_t byte) {
        pending.bytes.push_back(byte);
        return 1;
    }
    uint8_t endTransmission() {
        events.push_back(pending);
        if (transmissionResults.empty()) return 0;
        const uint8_t result = transmissionResults.front();
        transmissionResults.pop_front();
        return result;
    }

    size_t requestFrom(int address, unsigned int quantity) {
        (void)address;
        events.push_back({Event::Request, quantity, {}});
        received.clear();
        if (responder) {
            for (uint8_t byte : responder()) received.push_back(byte);
        } else if (!responses.empty()) {
            for (uint8_t byte : responses.front()) received.push_back(byte);
            responses.pop_front();
        }
        return received.size();
    }
    int available() { return (int)received.size(); }
    int read() {
        if (received.empty()) return -1;
        const int byte = received.front();
        received.pop_front();
        return byte;
    }

private:
    Event pending;
    std::deque<uint8_t> received;
};

// The two I2C buses of the RP2040.
inline TwoWire Wire;
inline TwoWire Wire1;
