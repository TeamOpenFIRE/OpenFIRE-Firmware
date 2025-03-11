 /*!
 * @file OpenFIREserial.h
 * @brief Serial RX buffer reading routines.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#ifndef _OPENFIRESERIAL_H_
#define _OPENFIRESERIAL_H_

#include <Arduino.h>
#include "OpenFIREDefines.h"

class OF_Serial
{
public:

    /// @brief    Method for processing the Serial buffer when docked to the Desktop App
    /// @details  Only method that allows for reading/writing to system settings.
    static void SerialProcessingDocked();

    // Main routine that prints information to connected serial monitor when the gun enters Pause Mode.
    static void PrintResults();

    #ifdef DEBUG_SERIAL
    static void PrintDebugSerial();
    #endif // DEBUG_SERIAL

    #ifdef MAMEHOOKER
    /// @brief    Main method processing the Serial buffer.
    /// @details  asd
    static void SerialProcessing();

    /// @brief    Handling gun events that may have been processed in SerialProcessing
    static void SerialHandling();

    // For serial mode:
    enum SerialQueueBits {
        SerialQueue_Solenoid = 0,
        SerialQueue_SolPulse,
        SerialQueue_Rumble,
        SerialQueue_RumbPulse,
        SerialQueue_Red,
        SerialQueue_Green,
        SerialQueue_Blue,
        SerialQueue_LEDPulse,
        SerialQueueBitsCount
    };

    static inline bool serialMode = false;                         // Set if we're prioritizing force feedback over serial commands or not.
    static inline bool offscreenButtonSerial = false;              // Serial-only version of offscreenButton toggle.
    static inline bool serialQueue[SerialQueueBitsCount] = {false};// Array of events we've queued from the serial receipt.
    static inline bool serialARcorrection = false;                 // 4:3 AR correction mode flag
    // from least to most significant bit: solenoid digital, solenoid pulse, rumble digital, rumble pulse, R/G/B direct, RGB (any) pulse.

    // These do get addressed by the main code
    #ifdef USES_DISPLAY
    static inline bool serialDisplayChange = false;                // Signal of pending display update, sent by Core 2 to be used by Core 1 in dual core configs
    static inline uint16_t serialLifeCount = 0;		                 // Changed from uint16_t for games with life values > 255
    static inline uint8_t serialAmmoCount = 0;
    #endif // USES_DISPLAY

    #endif // MAMEHOOKER

private:
    #ifdef MAMEHOOKER

    #ifdef LED_ENABLE
    static inline unsigned long serialLEDPulsesLastUpdate = 0;     // The timestamp of the last serial-invoked LED pulse update we iterated.
    static inline unsigned int serialLEDPulsesLength = 2;          // How long each stage of a serial-invoked pulse rumble is, in ms.
    static inline bool serialLEDChange = false;                    // Set on if we set an LED command this cycle.
    static inline bool serialLEDPulseRising = true;                // In LED pulse events, is it rising now? True to indicate rising, false to indicate falling; default to on for very first pulse.
    static inline uint8_t serialLEDPulses = 0;                     // How many LED pulses are we being told to do?
    static inline uint8_t serialLEDPulsesLast = 0;                 // What LED pulse we've processed last.
    static inline uint8_t serialLEDR = 0;                          // For the LED, how strong should it be?
    static inline uint8_t serialLEDG = 0;                          // Each channel is defined as three brightness values
    static inline uint8_t serialLEDB = 0;                          // So yeah.
    static inline uint8_t serialLEDPulseColorMap = 0b00000000;     // The map of what LEDs should be pulsing (we use the rightmost three of this bitmask for R, G, or B).
    #endif // LED_ENABLE

    #ifdef USES_RUMBLE
    static inline unsigned long serialRumbPulsesLastUpdate = 0;    // The timestamp of the last serial-invoked pulse rumble we updated.
    static inline unsigned int serialRumbPulsesLength = 60;        // How long each stage of a serial-invoked pulse rumble is, in ms.
    static inline uint8_t serialRumbPulseStage = 0;                // 0 = start/rising, 1 = peak, 2 = falling, 3 = final check/reset to start
    static inline uint8_t serialRumbPulses = 0;                    // If rumble is commanded to do pulse responses, how many?
    static inline uint8_t serialRumbPulsesLast = 0;                // Counter of how many pulse rumbles we did so far.
    static inline uint16_t serialRumbCustomHoldLength = 0;         // Determines custom solenoid ON state length for sol "pulse" commands - 0 = use system settings
    static inline uint16_t serialRumbCustomPauseLength = 0;        // Determines custom solenoid OFF state length for sol "pulse" commands - 0 = use system settings
    #endif // USES_RUMBLE

    #ifdef USES_SOLENOID
    static inline unsigned long serialSolPulsesLastUpdate = 0;     // The timestamp of the last serial-invoked pulse solenoid event we updated.
    static inline uint8_t serialSolPulses = 0;                     // How many solenoid pulses are we being told to do?
    static inline uint8_t serialSolPulsesLast = 0;                 // What solenoid pulse we've processed last.
    static inline uint32_t serialSolTimestamp = 0;                 // Timestamp of the last solenoid static on command (for safety)
    static inline uint8_t serialSolCustomHoldLength = 0;           // Determines custom solenoid ON state length for sol "pulse" commands - 0 = use system settings
    static inline uint16_t serialSolCustomPauseLength = 0;         // Determines custom solenoid OFF state length for sol "pulse" commands - 0 = use system settings
    #define SERIAL_SOLENOID_MAXSHUTOFF 2000
    #endif // USES_SOLENOID

    #endif // MAMEHOOKER

    //// Printing
    // used for periodic serial prints
    static inline unsigned long lastPrintMillis = 0;

    // used for debug prints
    #ifdef DEBUG_SERIAL
    static inline unsigned long serialDbMs = 0;
    static inline unsigned long frameCount = 0;
    static inline unsigned long irPosCount = 0;
    #endif
};

#endif // _OPENFIRESERIAL_H_