 /*!
 * @file OpenFIREFeedback.cpp
 * @brief Force feedback subsystems.
 *
 * @copyright That One Seong, 2024
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>
#include "OpenFIREFeedback.h"
#include "SamcoPreferences.h"

void OF_FFB::FFBOnScreen()
{
    if(SamcoPreferences::toggles[OF_Const::solenoid]) {                             // (Only activate when the solenoid switch is on!)
        if(!triggerHeld) {  // If this is the first time we're firing,
            if(burstFireActive && !burstFiring) {  // Are we in burst firing mode?
                solenoidFirstShot = true;               // Set this so we use the instant solenoid fire path,
                SolenoidActivation(0);                  // Engage it,
                solenoidFirstShot = false;              // And disable the flag to mitigate confusion.
                burstFiring = true;                     // Set that we're in a burst fire event.
                burstFireCount = 1;                     // Set this as the first shot in a burst fire sequence,
                burstFireCountLast = 1;                 // And reset the stored counter,
            } else if(!burstFireActive) {
                solenoidFirstShot = true;
                SolenoidActivation(0);
                if(SamcoPreferences::toggles[OF_Const::autofire])
                    solenoidFirstShot = false;
            }
        // Else, these below are all if we've been holding the trigger.
        } else if(burstFiring) {  // If we're in a burst firing sequence,
            BurstFire();                                // Process it.
        } else if(SamcoPreferences::toggles[OF_Const::autofire] &&  // Else, if we've been holding the trigger, is the autofire switch active?
                  !burstFireActive) {                          // (WITHOUT burst firing enabled)
            if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {              // Is the solenoid engaged?
                SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidFastInterval]); // If so, immediately pass the autofire faster interval to solenoid method
            } else {                                    // Or if it's not,
                SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidFastInterval] * SamcoPreferences::settings[OF_Const::autofireWaitFactor]); // We're holding it for longer.
            }
        } else if(solenoidFirstShot) {                  // If we aren't in autofire mode, are we waiting for the initial shot timer still?
            if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {              // If so, are we still engaged? We need to let it go normally, but maintain the single shot flag.
                currentMillis = millis();
                if(currentMillis - previousMillisSol >= SamcoPreferences::settings[OF_Const::solenoidNormalInterval]) { // If we finally surpassed the wait threshold...
                    digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);     // Let it go.
                }
            } else {                                    // We're waiting on the extended wait before repeating in single shot mode.
                currentMillis = millis();
                if(currentMillis - previousMillisSol >= SamcoPreferences::settings[OF_Const::solenoidHoldLength]) { // If we finally surpassed the LONGER wait threshold...
                    solenoidFirstShot = false;          // We're gonna turn this off so we don't have to pass through this check anymore.
                    SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidNormalInterval]); // Process it now.
                }
            }
        } else if(!burstFireActive) {                   // if we don't have the single shot wait flag on (holding the trigger w/out autofire)
            if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {              // Are we engaged right now?
                SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidNormalInterval]); // Turn it off with this timer.
            } else {                                    // Or we're not engaged.
                SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidNormalInterval] * 2); // So hold it that way for twice the normal timer.
            }
        }
    // only activate rumbleFF as a fallback if Solenoid is explicitly disabled
    } else if(SamcoPreferences::toggles[OF_Const::rumble] &&
              SamcoPreferences::toggles[OF_Const::rumbleFF] && !rumbleHappened && !triggerHeld) {
        RumbleActivation();
    }
    if(SamcoPreferences::toggles[OF_Const::rumble] &&  // Is rumble activated,
       rumbleHappening && triggerHeld) {  // AND we're in a rumbling command WHILE the trigger's held?
        RumbleActivation();                    // Continue processing the rumble command, to prevent infinite rumble while going from on-screen to off mid-command.
    }
}

void OF_FFB::FFBOffScreen()
{
    if(SamcoPreferences::toggles[OF_Const::rumble]) {  // Only activate if the rumble switch is enabled!
        if(!SamcoPreferences::toggles[OF_Const::rumbleFF] &&
           !rumbleHappened && !triggerHeld) {  // Is this the first time we're rumbling AND only started pulling the trigger (to prevent starting a rumble w/ trigger hold)?
            RumbleActivation();                        // Start a rumble command.
        } else if(rumbleHappening) {  // We are currently processing a rumble command.
            RumbleActivation();                        // Keep processing that command then.
        }  // Else, we rumbled already, so don't do anything to prevent infinite rumbling.
    }
    if(burstFiring) {                                  // If we're in a burst firing sequence,
        BurstFire();
    } else if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin]) && !burstFireActive) { // If the solenoid is engaged, since we're not shooting the screen, shut off the solenoid a'la an idle cycle
        currentMillis = millis();                      // Calibrate current time
        if(currentMillis - previousMillisSol >= SamcoPreferences::settings[OF_Const::solenoidFastInterval]) { // I guess if we're not firing, may as well use the fastest shutoff.
            previousMillisSol = currentMillis;
            digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
        }
    }
}

void OF_FFB::FFBRelease()
{
    if(SamcoPreferences::toggles[OF_Const::solenoid]) {  // Has the solenoid remain engaged this cycle?
        if(burstFiring) {    // Are we in a burst fire command?
            BurstFire();                                    // Continue processing it.
        } else if(!burstFireActive) { // Else, we're just processing a normal/rapid fire shot.
            solenoidFirstShot = false;                      // Make sure this is unset to prevent "sticking" in single shot mode!
            currentMillis = millis();
            if(currentMillis - previousMillisSol >= SamcoPreferences::settings[OF_Const::solenoidFastInterval]) { // I guess if we're not firing, may as well use the fastest shutoff.
                previousMillisSol = currentMillis;
                digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);             // Make sure to turn it off.
            }
        }
    }
    
    // If Rumble FF is enabled and Autofire is enabled, the motor needs to be disabled when the trigger is released. Otherwise, allow RumbleActivation to deal with the activation timer
    if(SamcoPreferences::toggles[OF_Const::rumbleFF] && SamcoPreferences::toggles[OF_Const::autofire]) {
        if(rumbleHappening || rumbleHappened) {
            analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], 0);      // Make sure the rumble is OFF.
            rumbleHappening = false;                                // This rumble command is done now.
            rumbleHappened = false;                                 // Make it clear we've stopped holding.
        }
    } else {
        if(rumbleHappening) {                                   // Are we currently in a rumble command? (Implicitly needs SamcoPreferences::toggles[OF_Const::rumble])
            RumbleActivation();                                 // Continue processing our rumble command.
            // (This is to prevent making the lack of trigger pull actually activate a rumble command instead of skipping it like we should.)
        } else if(rumbleHappened) {                             // If rumble has happened,
            rumbleHappened = false;                             // well we're clear now that we've stopped holding.
        }
    }
}

void OF_FFB::SolenoidActivation(const int &solenoidFinalInterval)
{
    if(solenoidFirstShot) {                                       // If this is the first time we're shooting, it's probably safe to shoot regardless of temps.
        previousMillisSol = millis();                             // Calibrate the timer for future calcs.
        digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH);     // Since we're shooting the first time, just turn it on aaaaand fire.
    } else {
        if(SamcoPreferences::pins[OF_Const::tempPin] >= 0) { // If a temp sensor is installed and enabled,
            TemperatureUpdate();

            if(tempStatus < Temp_Fatal) {
                if(tempStatus == Temp_Warning) {
                    if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {    // Is the valve being pulled now?
                        if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                            previousMillisSol = currentMillis;
                            digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], !digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])); // Flip, flop.
                        }
                    } else { // The solenoid's probably off, not on right now. So that means we should wait a bit longer to fire again.
                        if(currentMillis - previousMillisSol >= solenoidWarningInterval) { // We're keeping it low for a bit longer, to keep temps stable. Try to give it a bit of time to cool down before we go again.
                            previousMillisSol = currentMillis;
                            digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], !digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin]));
                        }
                    }
                } else {
                    if(currentMillis - previousMillisSol >= solenoidFinalInterval) {
                        previousMillisSol = currentMillis;
                        digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], !digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])); // run the solenoid into the state we've just inverted it to.
                    }
                }
            } else {
                #ifdef PRINT_VERBOSE
                    Serial.println("Solenoid over safety threshold; not activating!");
                #endif
                digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);                       // Make sure it's off if we're this dangerously close to the sun.
            }
        } else { // No temp sensor, so just go ahead.
            currentMillis = millis();
            if(currentMillis - previousMillisSol >= solenoidFinalInterval) { // If we've waited long enough for this interval,
                previousMillisSol = currentMillis;                    // Since we've waited long enough, calibrate the timer
                digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], !digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])); // run the solenoid into the state we've just inverted it to.
            }
        }
    }
}

void OF_FFB::TemperatureUpdate()
{
    currentMillis = millis();
    if(currentMillis - previousMillisTemp > 2) {
        previousMillisTemp = currentMillis;
        temperatureGraph[temperatureIndex] = (((analogRead(SamcoPreferences::pins[OF_Const::tempPin]) * 3.3) / 4096) - 0.5) * 100; // Convert reading from mV->3.3->12-bit->Celsius
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

void OF_FFB::RumbleActivation()
{
    if(rumbleHappening) {                                         // Are we in a rumble command rn?
        currentMillis = millis();                                 // Calibrate a timer to set how long we've been rumbling.
        if(SamcoPreferences::toggles[OF_Const::rumbleFF]) {
            if(!SamcoPreferences::toggles[OF_Const::autofire]) {       // We only want to use the rumble timer if Autofire is not active. Otherwise, keep it going
                if(currentMillis - previousMillisRumble >= SamcoPreferences::settings[OF_Const::rumbleInterval] / 2) { // If we've been waiting long enough for this whole rumble command,
                    analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], 0);                         // Make sure the rumble is OFF.
                    rumbleHappening = false;                              // This rumble command is done now.
                    rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
                }
            }
        } else {
            if(currentMillis - previousMillisRumble >= SamcoPreferences::settings[OF_Const::rumbleInterval]) { // If we've been waiting long enough for this whole rumble command,
                analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], 0);                         // Make sure the rumble is OFF.
                rumbleHappening = false;                              // This rumble command is done now.
                rumbleHappened = true;                                // And just to make sure, to prevent holding == repeat rumble commands.
            }
        }
    } else {                                                      // OR, we're rumbling for the first time.
        previousMillisRumble = millis();                          // Mark this as the start of this rumble command.
        analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
        rumbleHappening = true;                                   // Mark that we're in a rumble command rn.
    }
}

void OF_FFB::BurstFire()
{
    if(burstFireCount < 4) {  // Are we within the three shots alotted to a burst fire command?
        #ifdef USES_SOLENOID
            if(!digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin]) &&  // Is the solenoid NOT on right now, and the counter hasn't matched?
            (burstFireCount == burstFireCountLast)) {
                burstFireCount++;                                 // Increment the counter.
            }
            if(!digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {  // Now, is the solenoid NOT on right now?
                SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidFastInterval] * 2);     // Hold it off a bit longer,
            } else {                         // or if it IS on,
                burstFireCountLast = burstFireCount;              // sync the counters since we completed one bullet cycle,
                SolenoidActivation(SamcoPreferences::settings[OF_Const::solenoidFastInterval]);         // And start trying to activate the dingus.
            }
        #endif // USES_SOLENOID
        return;
    } else {  // If we're at three bullets fired,
        burstFiring = false;                                      // Disable the currently firing tag,
        burstFireCount = 0;                                       // And set the count off.
        return;                                                   // Let's go back.
    }
}

void OF_FFB::FFBShutdown()
{
    digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
    analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], 0);
    solenoidFirstShot = false;
    rumbleHappening = false;
    rumbleHappened = false;
    triggerHeld = false;
    burstFiring = false;
    burstFireCount = 0;
}
