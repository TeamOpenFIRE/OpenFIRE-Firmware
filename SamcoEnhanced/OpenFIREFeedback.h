 /*!
 * @file OpenFIREFeedback.h
 * @brief Force feedback subsystems.
 *
 * @copyright That One Seong, 2024
 *
 *  OpenFIREFeedback is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _OPENFIREFEEDBACK_H_
#define _OPENFIREFEEDBACK_H_

#include <stdint.h>
#include "SamcoPreferences.h"

class FFB {
public:
    /// @brief Constructor
    FFB();

    void FFBOnScreen();

    void FFBOffScreen();

    void FFBRelease();

    /// @brief Manages solenoid state w/ temperature tempering
    /// @details Temp tempering is based on last poll of TemperatureUpdate()
    void SolenoidActivation(int solenoidFinalInterval);

    /// @brief Updates current temperature (averaged), if available
    /// @details Only polls every 3ms, with updates committed to temperatureCurrent after four successful polling cycles
    void TemperatureUpdate();

    /// @brief Subroutine managing rumble state
    void RumbleActivation();

    /// @brief Subroutine for solenoid burst firing
    void BurstFire();

    /// @brief Macro to shut down all force feedback
    void FFBShutdown();

    // For autofire:
    bool triggerHeld = false;                  // Trigger SHOULDN'T be being pulled by default, right?

    bool burstFireActive = false;

    // Current temperature as read from TMP36, in (approximate) Celsius
    uint8_t temperatureCurrent;

private:
    // For solenoid:
    bool solenoidFirstShot = false;            // default to off, but is set this the first time we shoot.

    // For rumble:
    bool rumbleHappening = false;              // To keep track on if this is a rumble command or not.
    bool rumbleHappened = false;               // If we're holding, this marks we sent a rumble command already; is cleared when trigger is released
    
    unsigned long previousMillisSol = 0;       // Timestamp of last time since unique solenoid state change

    enum TempStatuses_e {
        Temp_Safe = 0,
        Temp_Warning,
        Temp_Fatal
    };

    uint8_t tempNormal = 35;                   // Solenoid: Anything below this value is "normal" operating temperature for the solenoid, in Celsius.
    uint8_t tempWarning = 42;                  // Solenoid: Above normal temps, this is the value up to where we throttle solenoid activation, in Celsius.
    uint8_t tempStatus = Temp_Safe;            // Current state of the solenoid,

    // timer stuff
    unsigned long currentMillis = 0;           // Current millis() value, which is globally updated/read across all functions in this class
    unsigned long previousMillisTemp = 0;      // Timestamp of last time TMP36 was read

    unsigned int temperatureGraph[4];          // Table of collected (converted) TMP36 readings, to be averaged into temperatureCurrent on the fourth value.
    uint8_t temperatureIndex = 0;              // Current index of temperatureGraph to update; initiates temperatureCurrent update/averaging when = 3.

    const unsigned int solenoidWarningInterval = SamcoPreferences::settings.solenoidFastInterval * 5; // for if solenoid is getting toasty.

    // For burst firing stuff:
    byte burstFireCount = 0;                   // What shot are we on?
    byte burstFireCountLast = 0;               // What shot have we last processed?
    bool burstFiring = false;                  // Are we in a burst fire command?

    // For rumble:
    unsigned long previousMillisRumble = 0;    // our time since the rumble motor event started
    // We need the rumbleHappening because of the variable nature of the PWM controlling the motor.
};

#endif // _OPENFIREFEEDBACK_H_
