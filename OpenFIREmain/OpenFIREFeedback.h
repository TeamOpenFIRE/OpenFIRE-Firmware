 /*!
 * @file OpenFIREFeedback.h
 * @brief Force feedback subsystems.
 *
 * @copyright That One Seong, 2024
 * @copyright GNU Lesser General Public License
 */

#ifndef _OPENFIREFEEDBACK_H_
#define _OPENFIREFEEDBACK_H_

#include <stdint.h>
#include "OpenFIREprefs.h"

class OF_FFB {
public:
    static void FFBOnScreen();

    static void FFBOffScreen();

    static void FFBRelease();

    /// @brief Manages solenoid state w/ temperature tempering
    /// @details Temp tempering is based on last poll of TemperatureUpdate()
    static void SolenoidActivation(const int &);

    /// @brief Updates current temperature (averaged), if available
    /// @details Only polls every 250ms, with updates committed to temperatureCurrent after four successful polling cycles
    static void TemperatureUpdate();

    /// @brief Subroutine managing rumble state
    static void RumbleActivation();

    /// @brief Subroutine for solenoid burst firing
    static void BurstFire();

    /// @brief Macro to shut down all force feedback
    static void FFBShutdown();

    // For rumble: for checking rumble state in main loop.
    static inline bool rumbleHappening = false;              // To keep track on if this is a rumble command or not.

    // For autofire:
    static inline bool triggerHeld = false;                  // Trigger SHOULDN'T be being pulled by default, right?
    static inline bool burstFireActive = false;
    static inline bool autofireDoubleLengthWait = false;

    // Current temperature as read from TMP36, in (approximate) Celsius
    static inline uint temperatureCurrent;

    enum TempStatuses_e {
        Temp_Safe = 0,
        Temp_Warning,
        Temp_Fatal
    };

    static inline TempStatuses_e tempStatus = Temp_Safe;     // Relative current state of the solenoid, according to TempStatuses_e

    #define TEMP_UPDATE_INTERVAL 250

private:
    // For solenoid:
    static inline bool solenoidFirstShot = false;            // default to off, but set this on the first time we shoot.

    // For rumble:
    static inline bool rumbleHappened = false;               // If we're holding, this marks we sent a rumble command already; is cleared when trigger is released
    
    static inline unsigned long previousMillisSol = 0;       // Timestamp of last time since unique solenoid state change

    // timer stuff
    static inline unsigned long currentMillis = 0;           // Current millis() value, which is globally updated/read across all functions in this class
    static inline unsigned long previousMillisTemp = 0;      // Timestamp of last time TMP36 was read

    static inline int temperatureGraph[4];                  // Table of collected (converted) TMP36 readings, to be averaged into temperatureCurrent on the fourth value.
    static inline uint tempGraphIndex = 0;                 // Current index of temperatureGraph to update; initiates temperatureCurrent update/averaging when = 3.

    // For burst firing stuff:
    static inline uint burstFireCount = 0;                   // What shot are we on?
    static inline uint burstFireCountLast = 0;               // What shot have we last processed?
    static inline bool burstFiring = false;                  // Are we in a burst fire command?

    // For rumble:
    static inline unsigned long previousMillisRumble = 0;    // our time since the rumble motor event started
    // We need the rumbleHappening because of the variable nature of the PWM controlling the motor.
};

#endif // _OPENFIREFEEDBACK_H_
