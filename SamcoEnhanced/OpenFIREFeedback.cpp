 /*!
 * @file OpenFIREFeedback.cpp
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

#include <Arduino.h>
#include "OpenFIREFeedback.h"
#include "SamcoPreferences.h"

FFB::FFB() {}

void FFB::FFBOnScreen()
{
    if(SamcoPreferences::toggles.solenoidActive) {                             // (Only activate when the solenoid switch is on!)
        if(!triggerHeld) {  // If this is the first time we're firing,
            if(burstFireActive && !burstFiring) {  // Are we in burst firing mode?
                solenoidFirstShot = true;               // Set this so we use the instant solenoid fire path,
                SolenoidActivation(0);                  // Engage it,
                solenoidFirstShot = false;              // And disable the flag to mitigate confusion.
                burstFiring = true;                     // Set that we're in a burst fire event.
                burstFireCount = 1;                     // Set this as the first shot in a burst fire sequence,
                burstFireCountLast = 1;                 // And reset the stored counter,
            } else if(!burstFireActive) {  // Or, if we're in normal or rapid fire mode,
                solenoidFirstShot = true;               // Set the First Shot flag on.
                SolenoidActivation(0);                  // Just activate the Solenoid already!
                if(SamcoPreferences::toggles.autofireActive) {          // If we are in auto mode,
                    solenoidFirstShot = false;          // Immediately set this bit off!
                }
            }
        // Else, these below are all if we've been holding the trigger.
        } else if(burstFiring) {  // If we're in a burst firing sequence,
            BurstFire();                                // Process it.
        } else if(SamcoPreferences::toggles.autofireActive &&  // Else, if we've been holding the trigger, is the autofire switch active?
                  !burstFireActive) {                          // (WITHOUT burst firing enabled)
            if(digitalRead(SamcoPreferences::pins.oSolenoid)) {              // Is the solenoid engaged?
                SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval); // If so, immediately pass the autofire faster interval to solenoid method
            } else {                                    // Or if it's not,
                SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval * SamcoPreferences::settings.autofireWaitFactor); // We're holding it for longer.
            }
        } else if(solenoidFirstShot) {                  // If we aren't in autofire mode, are we waiting for the initial shot timer still?
            if(digitalRead(SamcoPreferences::pins.oSolenoid)) {              // If so, are we still engaged? We need to let it go normally, but maintain the single shot flag.
                currentMillis = millis();
                if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidNormalInterval) { // If we finally surpassed the wait threshold...
                    digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);     // Let it go.
                }
            } else {                                    // We're waiting on the extended wait before repeating in single shot mode.
                currentMillis = millis();
                if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidLongInterval) { // If we finally surpassed the LONGER wait threshold...
                    solenoidFirstShot = false;          // We're gonna turn this off so we don't have to pass through this check anymore.
                    SolenoidActivation(SamcoPreferences::settings.solenoidNormalInterval); // Process it now.
                }
            }
        } else if(!burstFireActive) {                   // if we don't have the single shot wait flag on (holding the trigger w/out autofire)
            if(digitalRead(SamcoPreferences::pins.oSolenoid)) {              // Are we engaged right now?
                SolenoidActivation(SamcoPreferences::settings.solenoidNormalInterval); // Turn it off with this timer.
            } else {                                    // Or we're not engaged.
                SolenoidActivation(SamcoPreferences::settings.solenoidNormalInterval * 2); // So hold it that way for twice the normal timer.
            }
        }
    // only activate rumbleFF as a fallback if Solenoid is explicitly disabled
    } else if(SamcoPreferences::toggles.rumbleActive &&
              SamcoPreferences::toggles.rumbleFF && !rumbleHappened && !triggerHeld) {
        RumbleActivation();
    }
    if(SamcoPreferences::toggles.rumbleActive &&  // Is rumble activated,
       rumbleHappening && triggerHeld) {  // AND we're in a rumbling command WHILE the trigger's held?
        RumbleActivation();                    // Continue processing the rumble command, to prevent infinite rumble while going from on-screen to off mid-command.
    }
}

void FFB::FFBOffScreen()
{
    if(SamcoPreferences::toggles.rumbleActive) {  // Only activate if the rumble switch is enabled!
        if(!SamcoPreferences::toggles.rumbleFF &&
           !rumbleHappened && !triggerHeld) {  // Is this the first time we're rumbling AND only started pulling the trigger (to prevent starting a rumble w/ trigger hold)?
            RumbleActivation();                        // Start a rumble command.
        } else if(rumbleHappening) {  // We are currently processing a rumble command.
            RumbleActivation();                        // Keep processing that command then.
        }  // Else, we rumbled already, so don't do anything to prevent infinite rumbling.
    }
    if(burstFiring) {                                  // If we're in a burst firing sequence,
        BurstFire();
    } else if(digitalRead(SamcoPreferences::pins.oSolenoid) && !burstFireActive) { // If the solenoid is engaged, since we're not shooting the screen, shut off the solenoid a'la an idle cycle
        currentMillis = millis();                      // Calibrate current time
        if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
            previousMillisSol = currentMillis;
            digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
        }
    }
}

void FFB::FFBRelease()
{
    if(SamcoPreferences::toggles.solenoidActive) {  // Has the solenoid remain engaged this cycle?
        if(burstFiring) {    // Are we in a burst fire command?
            BurstFire();                                    // Continue processing it.
        } else if(!burstFireActive) { // Else, we're just processing a normal/rapid fire shot.
            solenoidFirstShot = false;                      // Make sure this is unset to prevent "sticking" in single shot mode!
            currentMillis = millis();
            if(currentMillis - previousMillisSol >= SamcoPreferences::settings.solenoidFastInterval) { // I guess if we're not firing, may as well use the fastest shutoff.
                previousMillisSol = currentMillis;
                digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);             // Make sure to turn it off.
            }
        }
    }

    if(rumbleHappening) {                                   // Are we currently in a rumble command? (Implicitly needs SamcoPreferences::toggles.rumbleActive)
        RumbleActivation();                                 // Continue processing our rumble command.
        // (This is to prevent making the lack of trigger pull actually activate a rumble command instead of skipping it like we should.)
    } else if(rumbleHappened) {                             // If rumble has happened,
        rumbleHappened = false;                             // well we're clear now that we've stopped holding.
    }
}

void FFB::SolenoidActivation(int solenoidFinalInterval)
{
    if(solenoidFirstShot) {                                       // If this is the first time we're shooting, it's probably safe to shoot regardless of temps.
        previousMillisSol = millis();                             // Calibrate the timer for future calcs.
        digitalWrite(SamcoPreferences::pins.oSolenoid, HIGH);     // Since we're shooting the first time, just turn it on aaaaand fire.
    } else {
        if(SamcoPreferences::pins.aTMP36 >= 0) { // If a temp sensor is installed and enabled,
            TemperatureUpdate();

            if(tempStatus < Temp_Fatal) {
                if(tempStatus == Temp_Warning) {
                    if(digitalRead(SamcoPreferences::pins.oSolenoid)) {    // Is the valve being pulled now?
                        if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                            previousMillisSol = currentMillis;
                            digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // Flip, flop.
                        }
                    } else { // The solenoid's probably off, not on right now. So that means we should wait a bit longer to fire again.
                        if(currentMillis - previousMillisSol >= solenoidWarningInterval) { // We're keeping it low for a bit longer, to keep temps stable. Try to give it a bit of time to cool down before we go again.
                            previousMillisSol = currentMillis;
                            digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid));
                        }
                    }
                } else {
                    if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                        previousMillisSol = currentMillis;
                        digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // run the solenoid into the state we've just inverted it to.
                    }
                }
            } else {
                #ifdef PRINT_VERBOSE
                    Serial.println("Solenoid over safety threshold; not activating!");
                #endif
                digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);                       // Make sure it's off if we're this dangerously close to the sun.
            }
        } else { // No temp sensor, so just go ahead.
            currentMillis = millis();
            if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
                previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
                digitalWrite(SamcoPreferences::pins.oSolenoid, !digitalRead(SamcoPreferences::pins.oSolenoid)); // run the solenoid into the state we've just inverted it to.
            }
        }
    }
}

void FFB::TemperatureUpdate()
{
    currentMillis = millis();
    if(currentMillis - previousMillisTemp > 2) {
        previousMillisTemp = currentMillis;
        temperatureGraph[temperatureIndex] = (((analogRead(SamcoPreferences::pins.aTMP36) * 3.3) / 4096) - 0.5) * 100; // Convert reading from mV->3.3->12-bit->Celsius
        if(temperatureIndex < 3) {
            temperatureIndex++;
        } else {
            // average out temperature from four samples taken 3ms apart from each other
            temperatureIndex = 0;
            temperatureCurrent = (temperatureGraph[0] +
                                  temperatureGraph[1] +
                                  temperatureGraph[2] +
                                  temperatureGraph[3]) / 4;
            if(tempStatus == Temp_Fatal) {
                if(temperatureCurrent < tempWarning-5) {
                    tempStatus = Temp_Warning;
                }
            } else {
                if(temperatureCurrent >= tempWarning) {
                    tempStatus = Temp_Fatal;
                } else if(tempStatus == Temp_Warning) {
                    if(temperatureCurrent < tempNormal-5) {
                        tempStatus = Temp_Safe;
                    }
                } else {
                    if(temperatureCurrent >= tempNormal) {
                        tempStatus = Temp_Warning;
                    }
                }
            }
        }
    }
}

void FFB::RumbleActivation()
{
    if(rumbleHappening) {                                         // Are we in a rumble command rn?
        currentMillis = millis();                                 // Calibrate a timer to set how long we've been rumbling.
        if(SamcoPreferences::toggles.rumbleFF) {
            if(currentMillis - previousMillisRumble >= SamcoPreferences::settings.rumbleInterval / 2) { // If we've been waiting long enough for this whole rumble command,
                digitalWrite(SamcoPreferences::pins.oRumble, LOW);                         // Make sure the rumble is OFF.
                rumbleHappening = false;                              // This rumble command is done now.
                rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
            }
        } else {
            if(currentMillis - previousMillisRumble >= SamcoPreferences::settings.rumbleInterval) { // If we've been waiting long enough for this whole rumble command,
                digitalWrite(SamcoPreferences::pins.oRumble, LOW);                         // Make sure the rumble is OFF.
                rumbleHappening = false;                              // This rumble command is done now.
                rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
            }
        }
    } else {                                                      // OR, we're rumbling for the first time.
        previousMillisRumble = millis();                          // Mark this as the start of this rumble command.
        analogWrite(SamcoPreferences::pins.oRumble, SamcoPreferences::settings.rumbleIntensity);
        rumbleHappening = true;                                   // Mark that we're in a rumble command rn.
    }
}

void FFB::BurstFire()
{
    if(burstFireCount < 4) {  // Are we within the three shots alotted to a burst fire command?
        #ifdef USES_SOLENOID
            if(!digitalRead(SamcoPreferences::pins.oSolenoid) &&  // Is the solenoid NOT on right now, and the counter hasn't matched?
            (burstFireCount == burstFireCountLast)) {
                burstFireCount++;                                 // Increment the counter.
            }
            if(!digitalRead(SamcoPreferences::pins.oSolenoid)) {  // Now, is the solenoid NOT on right now?
                SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval * 2);     // Hold it off a bit longer,
            } else {                         // or if it IS on,
                burstFireCountLast = burstFireCount;              // sync the counters since we completed one bullet cycle,
                SolenoidActivation(SamcoPreferences::settings.solenoidFastInterval);         // And start trying to activate the dingus.
            }
        #endif // USES_SOLENOID
        return;
    } else {  // If we're at three bullets fired,
        burstFiring = false;                                      // Disable the currently firing tag,
        burstFireCount = 0;                                       // And set the count off.
        return;                                                   // Let's go back.
    }
}

void FFB::FFBShutdown()
{
    digitalWrite(SamcoPreferences::pins.oSolenoid, LOW);
    digitalWrite(SamcoPreferences::pins.oRumble, LOW);
    solenoidFirstShot = false;
    rumbleHappening = false;
    rumbleHappened = false;
    triggerHeld = false;
    burstFiring = false;
    burstFireCount = 0;
}