 /*!
 * @file OpenFIREserial.cpp
 * @brief Serial RX buffer reading routines.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>
#include "OpenFIREserial.h"
#include "SamcoPreferences.h"
#include "OpenFIREFeedback.h"
#include "OpenFIRElights.h"
#include "boards/OpenFIREshared.h"
#include "OpenFIREcommon.h"

#ifdef MAMEHOOKER
void OF_Serial::SerialProcessing()
{
    // For more info about Serial commands, see the OpenFIRE repo wiki.
    char serialInput = Serial.read();                              // Read the serial input one byte at a time (we'll read more later)

    switch(serialInput) {
        // Start Signal
        case 'S':
          if(serialMode)
              Serial.println("SERIALREAD: Detected Serial Start command while already in Serial handoff mode!");
          else {
              serialMode = true;                                         // Set it on, then!
              OF_FFB::FFBShutdown();
              FW_Common::offscreenBShot = false;

              #ifdef USES_SOLENOID
              serialSolCustomHoldLength = 0;
              serialSolCustomPauseLength = 0;
              #endif // USES_SOLENOID

              #ifdef USES_RUMBLE
              serialRumbCustomHoldLength = 0;
              serialRumbCustomPauseLength = 0;
              #endif // USES_RUMBLE

              #ifdef LED_ENABLE
                  // Set the LEDs to a mid-intense white.
                  OF_RGB::LedUpdate(127, 127, 127);
              #endif // LED_ENABLE

              #ifdef USES_DISPLAY
                  // init basic display to show mamehook icon
                  if(FW_Common::gunMode == GunMode_Run)
                      FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, FW_Common::buttons.analogOutput);
              #endif // USES_DISPLAY
          }
          break;
        // Modesetting Signal
        case 'M':
          serialInput = Serial.read();                               // Read the second bit.
          switch(serialInput) {
              // input mode
              case '0':
                Serial.read();
                serialInput = Serial.read();
                switch(serialInput) {
                    case '2': // "hybrid" - just use the default m&kb mode
                    case '0': // mouse & kb 
                      FW_Common::buttons.analogOutput = false;
                      break;
                    // gamepad
                    case '1':
                      FW_Common::buttons.analogOutput = true;
                      Gamepad16.stickRight = (Serial.peek() == 'L') ? true: false;
                      break;
                    // official "MiSTer optimized" mode
                    case '9':
                      FW_Common::buttons.analogOutput = true;
                      Gamepad16.stickRight = true;
                      // HACK SHACK - testing MiSTer-friendly default gamepad maps
                      LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3 = PAD_A,
                      LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3       = PAD_B,
                      LightgunButtons::ButtonDesc[BtnIdx_B].reportCode3       = PAD_X,
                      LightgunButtons::ButtonDesc[BtnIdx_Reload].reportCode3  = PAD_Y,
                      LightgunButtons::ButtonDesc[BtnIdx_Start].reportCode3   = PAD_START,
                      LightgunButtons::ButtonDesc[BtnIdx_Select].reportCode3  = PAD_SELECT,
                      LightgunButtons::ButtonDesc[BtnIdx_Up].reportCode3      = PAD_UP,
                      LightgunButtons::ButtonDesc[BtnIdx_Down].reportCode3    = PAD_DOWN,
                      LightgunButtons::ButtonDesc[BtnIdx_Left].reportCode3    = PAD_LEFT,
                      LightgunButtons::ButtonDesc[BtnIdx_Right].reportCode3   = PAD_RIGHT,
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportCode3   = PAD_LB,
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal2].reportCode3  = PAD_RB,
                      LightgunButtons::ButtonDesc[BtnIdx_Pump].reportCode3    = PAD_C;
                      #ifdef USES_DISPLAY
                          FW_Common::OLED.mister = true;
                      #endif // USES_DISPLAY
                      break;
                }
                AbsMouse5.releaseAll();
                Keyboard.releaseAll();
                Gamepad16.releaseAll();
                #ifdef USES_DISPLAY
                    if(!serialMode && FW_Common::gunMode == GunMode_Run)
                        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Normal, FW_Common::buttons.analogOutput);
                    else if(serialMode && FW_Common::gunMode == GunMode_Run &&
                            FW_Common::OLED.serialDisplayType > ExtDisplay::ScreenSerial_None &&
                            FW_Common::OLED.serialDisplayType < ExtDisplay::ScreenSerial_Both) {
                        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, FW_Common::buttons.analogOutput);
                    }
                #endif // USES_DISPLAY
                break;
              // offscreen button mode
              case '1':
                Serial.read();                                         // nomf
                serialInput = Serial.read();
                switch(serialInput) {
                    // cursor in bottom left - just use disabled
                    case '1':
                    // "true offscreen shot" mode - just use disabled for now
                    case '3':
                    // disabled
                    case '0':
                      if(serialMode)
                          offscreenButtonSerial = false;
                      else FW_Common::offscreenButton = false;
                      // reset bindings for low button users if offscreen button was enabled earlier.
                      if(SamcoPreferences::toggles[OF_Const::lowButtonsMode]) {
                          FW_Common::UpdateBindings();
                      }
                      break;
                    // offscreen button
                    case '2':
                      // Applies to both serial and non, as it might be useful for Linux Supermodel users.
                      if(serialMode) 
                          offscreenButtonSerial = true;
                      else FW_Common::offscreenButton = true;
                      // remap bindings for low button users to make e.g. VCop 3 playable with 1 btn + pedal
                      // TODO: make this its own dedicated method in OpenFIREinput?
                      if(SamcoPreferences::toggles[OF_Const::lowButtonsMode]) {
                          LightgunButtons::ButtonDesc[BtnIdx_A].reportType = LightgunButtons::ReportType_Mouse;
                          LightgunButtons::ButtonDesc[BtnIdx_A].reportCode = MOUSE_BUTTON4;
                          LightgunButtons::ButtonDesc[BtnIdx_B].reportType = LightgunButtons::ReportType_Mouse;
                          LightgunButtons::ButtonDesc[BtnIdx_B].reportCode = MOUSE_BUTTON5;
                      }
                      break;
                }
                break;
              // pedal functionality
              case '2':
                Serial.read();                                         // nomf
                serialInput = Serial.read();
                switch(serialInput) {
                    // separate button (default to original binds)
                    case '0':
                      FW_Common::UpdateBindings(SamcoPreferences::toggles[OF_Const::lowButtonsMode]);
                      break;
                    // make reload button (mouse right)
                    case '1':
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportType = LightgunButtons::ReportType_Mouse;
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportCode = MOUSE_RIGHT;
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportType2 = LightgunButtons::ReportType_Mouse;
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportCode2 = MOUSE_RIGHT;
                      break;
                    // make middle mouse button (for VCop3 EZ mode)
                    case '2':
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportType = LightgunButtons::ReportType_Mouse;
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportCode = MOUSE_MIDDLE;
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportType2 = LightgunButtons::ReportType_Mouse;
                      LightgunButtons::ButtonDesc[BtnIdx_Pedal].reportCode2 = MOUSE_MIDDLE;
                      break;
                }
                break;
              // aspect ratio correction
              case '3':
                Serial.read();                                         // nomf
                serialARcorrection = Serial.read() - '0';
                if(!serialMode) {
                    if(serialARcorrection) { Serial.println("Setting 4:3 correction on!"); }
                    else { Serial.println("Setting 4:3 correction off!"); }
                }
                break;
              #ifdef USES_TEMP
              // temp sensor disabling (why?)
              case '4':
                Serial.read();                                         // nomf
                serialInput = Serial.read();
                // TODO: implement
                break;
              #endif // USES_TEMP
              // autoreload (TODO: maybe?)
              case '5':
                Serial.read();                                         // nomf
                serialInput = Serial.read();
                // TODO: implement?
                break;
              // rumble only mode (enable Rumble FF)
              case '6':
                Serial.read();                                         // nomf
                serialInput = Serial.read();
                switch(serialInput) {
                    // disable
                    case '0':
                      if(SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1 && SamcoPreferences::pins[OF_Const::solenoidPin] >= 0) { SamcoPreferences::toggles[OF_Const::solenoid] = true; }
                      if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0) { SamcoPreferences::toggles[OF_Const::rumbleFF] = false; }
                      break;
                    // enable
                    case '1':
                      if(SamcoPreferences::pins[OF_Const::rumbleSwitch] == -1 && SamcoPreferences::pins[OF_Const::rumblePin] >= 0) { SamcoPreferences::toggles[OF_Const::rumble] = true; }
                      if(SamcoPreferences::pins[OF_Const::solenoidSwitch] == -1 && SamcoPreferences::pins[OF_Const::solenoidPin] >= 0) { SamcoPreferences::toggles[OF_Const::solenoid] = false; }
                      if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0) { SamcoPreferences::toggles[OF_Const::rumbleFF] = true; }
                      break;
                }
                OF_FFB::FFBShutdown();
                break;
              #ifdef USES_SOLENOID
              // solenoid automatic mode
              case '8':
                Serial.read();                                         // Nomf the padding bit.
                serialInput = Serial.read();                           // Read the next.
                if(serialInput == '1') {
                    OF_FFB::burstFireActive = true;
                    SamcoPreferences::toggles[OF_Const::autofire] = false;
                } else if(serialInput == '2') {
                    SamcoPreferences::toggles[OF_Const::autofire] = true;
                    OF_FFB::burstFireActive = false;
                } else if(serialInput == '0') {
                    SamcoPreferences::toggles[OF_Const::autofire] = false;
                    OF_FFB::burstFireActive = false;
                }
                break;
              #endif // USES_SOLENOID
              #ifdef USES_DISPLAY
              case 'D':
                Serial.read();                                         // Nomf padding byte
                serialInput = Serial.read();
                switch(serialInput) {
                    case '0':
                      FW_Common::OLED.serialDisplayType = ExtDisplay::ScreenSerial_None;
                      break;
                    case '1':
                      FW_Common::OLED.serialDisplayType = ExtDisplay::ScreenSerial_Life;
                      break;
                    case '2':
                      FW_Common::OLED.serialDisplayType = ExtDisplay::ScreenSerial_Ammo;
                      break;
                    case '3':
                      FW_Common::OLED.serialDisplayType = ExtDisplay::ScreenSerial_Both;
                      break;
                }
                
                if(Serial.read() == 'B') {
                    FW_Common::OLED.lifeBar = true;
		                FW_Common::dispMaxLife = 0;
                } else FW_Common::OLED.lifeBar = false;

                // prevent glitching if currently in pause mode
                if(FW_Common::gunMode == GunMode_Run) {
                    if(FW_Common::OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both)
                        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
                    else if(FW_Common::OLED.serialDisplayType > ExtDisplay::ScreenSerial_None)
                        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, FW_Common::buttons.analogOutput);
                }
                break;
              #endif // USES_DISPLAY
              default:
                if(!serialMode) Serial.println("SERIALREAD: Serial modesetting command found, but no valid set bit found!");
                break;
          }
          break;
        // End Signal
        // Check to make sure that 'E' is not actually a glitched command bit
        // by ensuring that there's no adjacent bit.
        case 'E':
          if(Serial.peek() == -1) {
              if(!serialMode) Serial.println("SERIALREAD: Detected Serial End command while Serial Handoff mode is already off!");
              else {
                  serialMode = false;                                    // Turn off serial mode then.
                  offscreenButtonSerial = false;                         // And clear the stale serial offscreen button mode flag.
                  serialQueue[SerialQueueBitsCount] = {false};
                  serialARcorrection = false;
                  #ifdef USES_DISPLAY
                      FW_Common::OLED.serialDisplayType = ExtDisplay::ScreenSerial_None;
                      if(FW_Common::gunMode == GunMode_Run) FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Normal, FW_Common::buttons.analogOutput);
                  #endif // USES_DISPLAY
                  #ifdef LED_ENABLE
                      serialLEDPulseColorMap = 0b00000000;               // Clear any stale serial LED pulses
                      serialLEDPulses = 0;
                      serialLEDPulsesLast = 0;
                      serialLEDPulseRising = true;
                      serialLEDR = 0;                                    // Clear stale serial LED values.
                      serialLEDG = 0;
                      serialLEDB = 0;
                      serialLEDChange = false;
                      if(FW_Common::gunMode == GunMode_Run) OF_RGB::LedOff();           // Turn it off, and let lastSeen handle it from here.
                  #endif // LED_ENABLE
                  #ifdef USES_RUMBLE
                      digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
                      serialRumbPulseStage = 0;
                      serialRumbPulses = 0;
                      serialRumbPulsesLast = 0;
                      serialRumbCustomHoldLength = 0;
                      serialRumbCustomPauseLength = 0;
                  #endif // USES_RUMBLE
                  #ifdef USES_SOLENOID
                      digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
                      serialSolPulses = 0;
                      serialSolPulsesLast = 0;
                      serialSolCustomHoldLength = 0;
                      serialSolCustomPauseLength = 0;
                  #endif // USES_SOLENOID
                  AbsMouse5.releaseAll();
                  Keyboard.releaseAll();
                  // remap
                  FW_Common::UpdateBindings(SamcoPreferences::toggles[OF_Const::lowButtonsMode]);
                  Serial.println("Received end serial pulse, releasing FF override.");
              }
              break;
          }
          break;
        // owo SPECIAL SETUP EH?
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Set Autofire Interval Length
              case 'I':
                serialInput = Serial.read();
                if(serialInput >= '2' && serialInput <= '4') {
                    uint8_t afSetting = serialInput - '0';
                    SamcoPreferences::settings[OF_Const::autofireWaitFactor] = afSetting;
                    Serial.print("Autofire speed level ");
                    Serial.println(afSetting);
                } else Serial.println("SERIALREAD: No valid interval set! (Expected 2 to 4)");
                break;
              // Remap player numbers
              case 'R':
                serialInput = Serial.read();
                if(serialInput >= '1' && serialInput <= '4') {
                    playerStartBtn = serialInput;
                    playerSelectBtn = serialInput + 4;
                    FW_Common::UpdateBindings(SamcoPreferences::toggles[OF_Const::lowButtonsMode]);
                } else {
                    Serial.println("SERIALREAD: Player remap command called, but an invalid or no slot number was declared!");
                }
                break;
              // Enter Docked Mode
              case 'P':
                FW_Common::SetMode(GunMode_Docked);
                break;
              default:
                Serial.println("SERIALREAD: Internal setting command detected, but no valid option found!");
                Serial.println("Internally recognized commands are:");
                Serial.println("I(nterval Autofire)2/3/4 / R(emap)1/2/3/4 / P(ause)");
                break;
          }
          // End of 'X'
          break;
        // Force Feedback
        case 'F':
          serialInput = Serial.read();
          switch(serialInput) {
              #ifdef USES_SOLENOID
              // Solenoid bits
              case '0':
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();
                // Solenoid "on" command
                if(serialInput == '1') {         
                    serialQueue[SerialQueue_Solenoid] = true;
                // Solenoud "pulse" command (only if not already pulsing)
                } else if(serialInput == '2' &&
                !serialQueue[SerialQueue_SolPulse]) {
                    Serial.read();                                     // nomf the padding bit.
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialQueue[SerialQueue_SolPulse] = true;
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }
                        serialSolPulses = atoi(serialInputS);
                        if(!serialSolPulses) serialSolPulses++;
                        serialSolPulsesLast = 0;
                    }
                // Solenoid "off" command
                } else if(serialInput == '0') serialQueue[SerialQueue_Solenoid] = false, serialQueue[SerialQueue_SolPulse] = false;
                break;
              #endif // USES_SOLENOID
              #ifdef USES_RUMBLE
              // Rumble bits
              case '1':
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();
                // Rumble "on" command
                if(serialInput == '1') {
                    serialQueue[SerialQueue_Rumble] = true;
                // Rumble "pulse" command (only if not already pulsing)
                } else if(serialInput == '2' &&
                !serialQueue[SerialQueue_RumbPulse]) {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        serialQueue[SerialQueue_RumbPulse] = true;
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialRumbPulses = atoi(serialInputS);
                        if(!serialRumbPulses) serialRumbPulses++;
                        serialRumbPulsesLast = 0;
                    }
                // Rumble "off" command
                } else if(serialInput == '0') serialQueue[SerialQueue_Rumble] = false, serialQueue[SerialQueue_RumbPulse] = false;
                break;
              #endif // USES_RUMBLE
              #ifdef LED_ENABLE
              // LED Red bits
              case '2':
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();
                // LED Red "on" command
                if(serialInput == '1') {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialLEDChange = true;
                        serialQueue[SerialQueue_Red] = true;
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialLEDR = atoi(serialInputS);                   // Set array as the strength of the red value that's requested!
                        serialQueue[SerialQueue_LEDPulse] = false;         // Static emitting overrides pulse bits
                        serialLEDPulseColorMap = 0;
                    }
                // LED Red "pulse" command (only if not already pulsing)
                } else if(serialInput == '2' &&
                !serialQueue[SerialQueue_LEDPulse]) {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_LEDPulse] = true,
                        serialLEDPulseColorMap = 0b00000001;               // Set the R LED as the one pulsing only (overwrites the others).
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialLEDPulses = atoi(serialInputS);
                        serialLEDPulsesLast = 0;
                    }
                // LED Red "off" command
                } else if(serialInput == '0') {
                    serialLEDChange = true,
                    serialQueue[SerialQueue_Red] = false, serialQueue[SerialQueue_LEDPulse] = false,
                    serialLEDR = 0, serialLEDPulseColorMap = 0;
                }
                break;
              // LED Green bits
              case '3':
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();
                // LED Green "on" command
                if(serialInput == '1') {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_Green] = true;
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialLEDG = atoi(serialInputS);
                        serialQueue[SerialQueue_LEDPulse] = false, serialLEDPulseColorMap = 0;
                    }
                // LED Green "pulse" command
                } else if(serialInput == '2' &&
                !serialQueue[SerialQueue_LEDPulse]) {  // (and we haven't already sent a pulse command?)
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_LEDPulse] = true,
                        serialLEDPulseColorMap = 0b00000010;               // Set the G LED as the one pulsing only (overwrites the others).
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialLEDPulses = atoi(serialInputS);
                        serialLEDPulsesLast = 0;
                    }
                // LED Green "off" command
                } else if(serialInput == '0') {
                    serialLEDChange = true,
                    serialQueue[SerialQueue_Green] = false, serialQueue[SerialQueue_LEDPulse] = false,
                    serialLEDG = 0, serialLEDPulseColorMap = 0;
                }
                break;
              // LED Blue bits
              case '4':
                Serial.read();                                         // nomf the padding
                serialInput = Serial.read();
                // LED Blue "on" command
                if(serialInput == '1') {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_Blue] = true;
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialLEDB = atoi(serialInputS);
                        serialQueue[SerialQueue_LEDPulse] = false;
                        serialLEDPulseColorMap = 0;
                    }
                // LED Blue "pulse" command
                } else if(serialInput == '2' &&
                !serialQueue[SerialQueue_LEDPulse]) {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_LEDPulse] = true,
                        serialLEDPulseColorMap = 0b00000100;               // Set the B LED as the one pulsing only (overwrites the others).
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9') {
                                break;
                            }
                        }
                        serialLEDPulses = atoi(serialInputS);
                        serialLEDPulsesLast = 0;
                    }
                // LED Blue "off" command
                } else if(serialInput == '0') {
                    serialLEDChange = true,
                    serialQueue[SerialQueue_Blue] = false, serialQueue[SerialQueue_LEDPulse] = false,
                    serialLEDB = 0, serialLEDPulseColorMap = 0;
                }
                break;
              #endif // LED_ENABLE
              #ifdef USES_DISPLAY
              case 'D':
                serialInput = Serial.read();
                switch(serialInput) {
                  case 'A':
                  {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }
                        serialAmmoCount = atoi(serialInputS);
                        serialAmmoCount = constrain(serialAmmoCount, 0, 99);
                        serialDisplayChange = true;
                    }
                    break;
                  }
                  case 'L':
                  {
                    Serial.read();                                     // nomf the padding
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {                      // For three runs,
                            serialInputS[n] = Serial.read();               // Read the value and fill it into the char array...
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }

                        serialLifeCount = atoi(serialInputS);
                        if (FW_Common::OLED.lifeBar) {
                            if(serialLifeCount > FW_Common::dispMaxLife)
                                FW_Common::dispMaxLife = serialLifeCount;
                            FW_Common::dispLifePercentage = (100 * serialLifeCount) / FW_Common::dispMaxLife; // Calculate the Life % to show 
                        }
                        serialDisplayChange = true;
                    }
                    break;
                  }
                }
                break;
              #endif // USES_DISPLAY
              #if !defined(USES_SOLENOID) && !defined(USES_RUMBLE) && !defined(LED_ENABLE)
              default:
                //Serial.println("SERIALREAD: Feedback command detected, but no feedback devices are built into this firmware!");
              #endif
          }
          // End of 'F'
          break;
        // Custom Pulse Overrides
        case 'R':
          serialInput = Serial.read();
          switch(serialInput) {
              // Solenoid
              case '0':
                Serial.read(); // nomf
                if(Serial.peek() >= '0' && Serial.peek() <= '2') {
                    serialInput = Serial.read();
                    Serial.read(); // nomf
                    if(Serial.peek() >= '0' && Serial.peek() <='9') {
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }

                        switch(serialInput) {
                            // hold length
                            case '0':
                              serialSolCustomHoldLength = atoi(serialInputS);
                              break;
                            // pause length
                            case '1':
                              serialSolCustomPauseLength = atoi(serialInputS);
                              break;
                            // analog(?)
                            case '2':
                            default:
                              break;
                        }
                    }
                }
                break;
              // Rumble
              case '1':
                Serial.read(); // nomf
                if(Serial.peek() >= '0' & Serial.peek() <= '2') {
                    serialInput = Serial.read();
                    Serial.read();
                    if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                        char serialInputS[4];
                        for(byte n = 0; n < 3; n++) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }

                        switch(serialInput) {
                            // hold length
                            case '0':
                              serialRumbCustomHoldLength = atoi(serialInputS);
                              break;
                            // pause length
                            case '1':
                              serialRumbCustomPauseLength = atoi(serialInputS);
                              break;
                            // analog(?)
                            case '2':
                            default:
                              break;
                        }
                    }
                }
                break;
              // LED Red
              case '2':
              // LED Green
              case '3':
              // LED Blue
              case '4':
              default:
                break;
          }
          // End of 'R'
          break;
    }
}

void OF_Serial::SerialHandling()
{
    // The Mamehook feedback system handles most of the timing for us.
    // For the most part, all we have to do is just read and process what it sends us at face value.
    // Solenoid "normal" enable bits need to be monitored to ensure it isn't on for too long, and force shutdown if it is.
    // Solenoid "pulse" bits will borrow from current force feedback settings.
    // Rumble pulse bits are also something we do need to calculate ourselves.
    // The display (if enabled) is handled in the normal Core 0 gunmode run method.

    #ifdef USES_SOLENOID
      if(SamcoPreferences::toggles[OF_Const::solenoid]) {
          // Solenoid "on" command
          if(serialQueue[SerialQueue_Solenoid]) {
              if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {
                  if(millis() - serialSolTimestamp > SERIAL_SOLENOID_MAXSHUTOFF) {
                      digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
                      serialQueue[SerialQueue_Solenoid] = false;
                  }
              } else {
                  digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH);
                  serialSolTimestamp = millis();
              }
          // Solenoid "pulse" command
          } else if(serialQueue[SerialQueue_SolPulse]) {
              if(!serialSolPulsesLast) {                            // Have we started pulsing?
                  digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH);  // Start pulsing it on!
                  serialSolPulsesLast++;                                 // Start the sequence.
                  serialSolPulsesLastUpdate = millis();                  // timestamp
              } else if(serialSolPulsesLast <= serialSolPulses) {   // Have we met the pulses quota?
                  if(digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {
                      // custom hold length
                      if(serialSolCustomHoldLength) {
                          if(millis() - serialSolPulsesLastUpdate >= serialSolCustomHoldLength) {
                              digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);  // Start pulsing it off.
                              if(serialSolPulsesLast >= serialSolPulses)
                                  serialQueue[SerialQueue_SolPulse] = false;
                              else serialSolPulsesLast++, serialSolPulsesLastUpdate = millis();  // Timestamp our last pulse event.
                          }
                      // current settings hold length
                      } else if(millis() - serialSolPulsesLastUpdate >= SamcoPreferences::settings[OF_Const::solenoidNormalInterval]) {
                          digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);  // Start pulsing it off.
                          if(serialSolPulsesLast >= serialSolPulses)
                              serialQueue[SerialQueue_SolPulse] = false;
                          else serialSolPulsesLast++, serialSolPulsesLastUpdate = millis();  // Timestamp our last pulse event.
                      }
                  } else {
                      // custom pause length
                      if(serialSolCustomPauseLength) {
                          if(millis() - serialSolPulsesLastUpdate >= serialSolCustomPauseLength) {
                              digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH); // Start pulsing it on.
                              serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                          }
                      // current settings pause length
                      } else if(millis() - serialSolPulsesLastUpdate >=
                                SamcoPreferences::settings[OF_Const::solenoidFastInterval] * SamcoPreferences::settings[OF_Const::autofireWaitFactor]) {
                          digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH); // Start pulsing it on.
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      }
                  }
              }
          // Solenoid "off" command
          } else digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
      // solenoid toggle not allowed, just force it off.
      } else digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
  #endif // USES_SOLENOID

  #ifdef USES_RUMBLE
      if(SamcoPreferences::toggles[OF_Const::rumble]) {
          // Rumble "on" command
          if(serialQueue[SerialQueue_Rumble]) {
              analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]); // turn/keep it on.
          // Rumble "pulse" command
          } else if(serialQueue[SerialQueue_RumbPulse]) {
              // Pulses start
              if(!serialRumbPulsesLast) {
                  if(serialRumbCustomHoldLength && serialRumbCustomPauseLength)
                       analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
                  else analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength] / 3);
                  serialRumbPulseStage = 0;                              // Set that we're at stage 0.
                  serialRumbPulsesLast++;
                  serialRumbPulsesLastUpdate = millis();
              // Pulses processing
              } else if(serialRumbPulsesLast <= serialRumbPulses) {
                  // G4IR-style on/off style ramping
                  if(serialRumbCustomHoldLength && serialRumbCustomPauseLength) {
                      if(!serialRumbPulseStage) {
                          if(millis() - serialRumbPulsesLastUpdate > serialRumbCustomHoldLength) {
                              serialRumbPulseStage = 0;
                              digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
                              if(serialRumbPulsesLast >= serialRumbPulses)
                                  serialQueue[SerialQueue_RumbPulse] = false;
                          }
                      } else if(millis() - serialRumbPulsesLastUpdate > serialRumbCustomPauseLength) {
                          serialRumbPulseStage++;
                          analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
                      }
                  // OF-style analog ramping
                  } else if(millis() - serialRumbPulsesLastUpdate > serialRumbPulsesLength) { // have we waited enough time between pulse stages?
                      switch(serialRumbPulseStage) {
                          // Rising to Sustain
                          case 0:
                              analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
                              serialRumbPulseStage++;                    // Increments the stage of the pulse.
                              serialRumbPulsesLastUpdate = millis();     // and timestamps when we've had updated this last.
                              break;                                     // Then quits until next pulse stage
                          // Sustain to Falling
                          case 1:
                              analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength] / 2);
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          // Falloff
                          case 2:
                              analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength] / 3);
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          // Check
                          case 3:
                              if(serialRumbPulsesLast >= serialRumbPulses) {
                                  digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
                                  serialQueue[SerialQueue_RumbPulse] = false;
                              } else serialRumbPulsesLast++, serialRumbPulseStage = 0;
                              break;
                      }
                  }
              }
          // Rumble "off"
          } else digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
      // Rumble disabled, not allowed to be on
      } else digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
  #endif // USES_RUMBLE

  #ifdef LED_ENABLE
    if(serialLEDChange) {                                     // Has the LED command state changed?
        // LED "pulse" command
        if(serialQueue[SerialQueue_LEDPulse]) {
            // LED pulsing start
            if(!serialLEDPulsesLast) {                        // Are we just starting?
                serialQueue[SerialQueue_Red] = false, serialQueue[SerialQueue_Green] = false, serialQueue[SerialQueue_Blue] = false;
                serialLEDPulsesLast = 1;                           // Set that we have started.
                serialLEDPulseRising = true;                       // Set the LED cycle to rising.
                // Reset all the LEDs to zero, the color map will tell us which one to focus on.
                serialLEDR = 0, serialLEDG = 0, serialLEDB = 0;
            // LED pulsing processing
            } else if(serialLEDPulsesLast <= serialLEDPulses) {
                if(millis() - serialLEDPulsesLastUpdate > serialLEDPulsesLength) { // have we waited enough time between pulse stages?
                    switch(serialLEDPulseColorMap) {           // Check the color map
                        case 0b00000001:                       // Basically for R, G, or B,
                            if(serialLEDPulseRising) {
                                serialLEDR += 3;                   // Set the LED value up by three (it's easiest to do blindly like this without over/underflowing tbh)
                                if(serialLEDR == 255)       // If we've reached the max value,
                                    serialLEDPulseRising = false;  // Set that we're in the falling state now.
                            } else {
                                serialLEDR -= 3;                   // Decrement the value.
                                if(serialLEDR == 0) {         // If the LED value has reached the lowest point,
                                    serialLEDPulseRising = true;   // Set that we should be in the rising part of a new cycle.
                                    serialLEDPulsesLast++;         // This was a pulse cycle, so increment that.
                                }
                            }
                            serialLEDPulsesLastUpdate = millis(); // Timestamp this event.
                            break;                             // And get out.
                        case 0b00000010:
                            if(serialLEDPulseRising) {
                                serialLEDG += 3;
                                if(serialLEDG == 255)
                                    serialLEDPulseRising = false;
                            } else {
                                serialLEDG -= 3;
                                if(serialLEDG == 0) {
                                    serialLEDPulseRising = true;
                                    serialLEDPulsesLast++;
                                }
                            }
                            serialLEDPulsesLastUpdate = millis();
                            break;
                        case 0b00000100:
                            if(serialLEDPulseRising) {
                                serialLEDB += 3;
                                if(serialLEDB == 255)
                                    serialLEDPulseRising = false;
                            } else {
                                serialLEDB -= 3;
                                if(serialLEDB == 0) {
                                    serialLEDPulseRising = true;
                                    serialLEDPulsesLast++;
                                }
                            }
                            serialLEDPulsesLastUpdate = millis();
                            break;
                    }
                    // Then, commit the changed value.
                    OF_RGB::LedUpdate(serialLEDR, serialLEDG, serialLEDB);
                }
            // LED pulsing finishing
            } else serialLEDPulseColorMap = 0b00000000, serialQueue[SerialQueue_LEDPulse] = false;
        // Any LED static bits
        } else if(serialQueue[SerialQueue_Red] ||           // Are either the R,
                  serialQueue[SerialQueue_Green] ||         // G,
                  serialQueue[SerialQueue_Blue]) {          // OR B digital bits set to on?
            // Command the LED to change/turn on with the values serialProcessing set for us.
            OF_RGB::LedUpdate(serialLEDR, serialLEDG, serialLEDB);
            serialLEDChange = false;                               // Set the bit to off.
        // LEDs off
        } else OF_RGB::LedOff(), serialLEDChange = false;     // We've done the change, so set it off to reduce redundant LED updates.
    }
    #endif // LED_ENABLE
}
#endif // MAMEHOOKER

void OF_Serial::SerialProcessingDocked()
{
    char serialInput = Serial.read();

    switch(serialInput) {
        case 'X':
          serialInput = Serial.read();
          switch(serialInput) {
              // Set IR Brightness
              case 'B':
              {
                byte lvl = Serial.read() - '0';
                if(lvl >= 0 && lvl <= 2) {
                    if(FW_Common::gunMode != GunMode_Pause || FW_Common::gunMode != GunMode_Docked) {
                        Serial.println("Can't set sensitivity in run mode! Please enter pause mode if you'd like to change IR sensitivity.");
                    } else FW_Common::SetIrSensitivity(lvl);
                } else Serial.println("SERIALREAD: No valid IR sensitivity level set! (Expected 0 to 2)");
                break;
              }
              // Toggle Test/Processing Mode
              case 'T':
                if(FW_Common::runMode == RunMode_Processing) {
                    Serial.println("Exiting processing mode...");
                    switch(FW_Common::profileData[FW_Common::profiles.selectedProfile].runMode) {
                        case RunMode_Normal:
                          FW_Common::SetRunMode(RunMode_Normal);
                          break;
                        case RunMode_Average:
                          FW_Common::SetRunMode(RunMode_Average);
                          break;
                        case RunMode_Average2:
                          FW_Common::SetRunMode(RunMode_Average2);
                          break;
                    }
                } else {
                    Serial.println("Entering Test Mode...");
                    FW_Common::SetRunMode(RunMode_Processing);
                }
                break;
              // Enter Docked Mode
              case 'P':
                FW_Common::SetMode(GunMode_Docked);
                break;
              // Exit Docked Mode
              case 'E':
                if(!FW_Common::justBooted)
                    FW_Common::SetMode(GunMode_Run);
                else FW_Common::SetMode(GunMode_Init);
                switch(FW_Common::profileData[FW_Common::profiles.selectedProfile].runMode) {
                    case RunMode_Normal:
                      FW_Common::SetRunMode(RunMode_Normal);
                      break;
                    case RunMode_Average:
                      FW_Common::SetRunMode(RunMode_Average);
                      break;
                    case RunMode_Average2:
                      FW_Common::SetRunMode(RunMode_Average2);
                      break;
                }
                break;
              // Enter Calibration mode (optional: switch to cal profile if detected)
              case 'C':
              {
                byte i = Serial.read() - '0';
                if(i >= 1 && i <= 4) {
                    FW_Common::SelectCalProfile(i-1);
                    Serial.print("Profile: ");
                    Serial.println(i-1);
                    if(Serial.peek() == 'C') {
                        Serial.read(); // nomf

                        // sensitivity preset
                        if(Serial.peek() == 'I') {
                            Serial.read(); // nomf
                            FW_Common::SetIrSensitivity(Serial.read() - '0');
                        }

                        // ir layout type preset
                        if(Serial.peek() == 'L') {
                            Serial.read(); // nomf
                            FW_Common::SetIrLayout(Serial.read() - '0');
                        }

                        FW_Common::SetMode(GunMode_Calibration);
                        FW_Common::ExecCalMode(true);
                    }
                }
                break;
              }
              // Save current profile
              case 'S':
                Serial.println("Saving preferences...");
                Serial.flush();
                // dockedSaving flag is set by Xm, since that's required anyways for this to make any sense.
                FW_Common::SavePreferences();
                // load everything back to commit custom pins setting to memory
                if(FW_Common::nvPrefsError == SamcoPreferences::Error_Success) {
                    FW_Common::PinsReset();
                    FW_Common::CameraSet();
                    FW_Common::FeedbackSet();

                    // Update bindings so LED/Pixel changes are reflected immediately
                    if(SamcoPreferences::usb.devicePID >= 1 && SamcoPreferences::usb.devicePID <= 5) {
                        playerStartBtn = SamcoPreferences::usb.devicePID + '0';
                        playerSelectBtn = SamcoPreferences::usb.devicePID + '0' + 4;
                    }
                    FW_Common::UpdateBindings(SamcoPreferences::toggles[OF_Const::lowButtonsMode]);

                    #ifdef LED_ENABLE
                    // Save op above resets color, so re-set it back to docked idle color
                    if(FW_Common::gunMode == GunMode_Docked) {
                        OF_RGB::LedUpdate(127, 127, 255);
                    } else if(FW_Common::gunMode == GunMode_Pause) {
                        OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);
                    }
                    #endif // LED_ENABLE
                }
                FW_Common::buttons.Begin();
                FW_Common::dockedSaving = false;
                break;
              // Clear EEPROM.
              case 'c':
                //Serial.println(EEPROM.length());
                FW_Common::dockedSaving = true;
                SamcoPreferences::ResetPreferences();
                Serial.println("Cleared! Please reset the board.");
                FW_Common::dockedSaving = false;
                break;
              // Mapping new values to commit to EEPROM.
              case 'm':
              {
                if(!FW_Common::dockedSaving) {
                    FW_Common::buttons.Unset();
                    FW_Common::dockedSaving = true; // mark so button presses won't interrupt this process.
                } else {
                    Serial.read(); // nomf
                    serialInput = Serial.read();
                    // bool change
                    if(serialInput == '0') {
                        Serial.read(); // nomf
                        int8_t sCase = Serial.parseInt();
                        Serial.read(); // nomf
                        SamcoPreferences::toggles[sCase] = Serial.read() - '0';
                        SamcoPreferences::toggles[sCase] = constrain(SamcoPreferences::toggles[sCase], 0, 1);
                        Serial.printf("OK: Toggled setting %d to %d.\r\n", sCase, SamcoPreferences::toggles[sCase]);
                    // Pins
                    } else if(serialInput == '1') {
                        Serial.read(); // nomf
                        int8_t sCase = Serial.parseInt();
                        Serial.read(); // nomf
                        SamcoPreferences::pins[sCase] = Serial.parseInt();
                        SamcoPreferences::pins[sCase] = constrain(SamcoPreferences::pins[sCase], -1, 29);
                        Serial.printf("OK: Set function %d to pin %d.\r\n", sCase, SamcoPreferences::pins[sCase]);
                    // Extended Settings
                    } else if(serialInput == '2') {
                        Serial.read(); // nomf
                        uint32_t sCase = Serial.parseInt();
                        Serial.read(); // nomf
                        SamcoPreferences::settings[sCase] = Serial.parseInt();
                        Serial.printf("OK: Set setting %d to %d.\r\n", sCase, SamcoPreferences::settings[sCase]);
                    #ifdef USE_TINYUSB
                    // TinyUSB Identifier Settings
                    } else if(serialInput == '3') {
                        Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          // Device PID
                          case '0':
                            {
                              Serial.read(); // nomf
                              SamcoPreferences::usb.devicePID = Serial.parseInt();
                              Serial.println("OK: Updated TinyUSB Device ID.");
                              break;
                            }
                          // Device name
                          case '1':
                            Serial.read(); // nomf
                            // clears name
                            for(byte i = 0; i < sizeof(SamcoPreferences::usb.deviceName); i++) {
                                SamcoPreferences::usb.deviceName[i] = '\0';
                            }
                            for(byte i = 0; i < sizeof(SamcoPreferences::usb.deviceName); i++) {
                                SamcoPreferences::usb.deviceName[i] = Serial.read();
                                if(!Serial.available()) {
                                    break;
                                }
                            }
                            Serial.println("OK: Updated TinyUSB Device String.");
                            break;
                        }
                    #endif // USE_TINYUSB
                    // Profile settings
                    } else if(serialInput == 'P') {
                        Serial.read(); // nomf
                        serialInput = Serial.read();
                        switch(serialInput) {
                          case 'i':
                          {
                            Serial.read(); // nomf
                            uint8_t i = Serial.read() - '0';
                            i = constrain(i, 0, PROFILE_COUNT - 1);
                            Serial.read(); // nomf
                            uint8_t v = Serial.read() - '0';
                            v = constrain(v, 0, 2);
                            FW_Common::profileData[i].irSensitivity = v;
                            if(i == FW_Common::profiles.selectedProfile)
                                FW_Common::SetIrSensitivity(v);
                            Serial.println("OK: Set IR sensitivity");
                            break;
                          }
                          case 'r':
                          {
                            Serial.read(); // nomf
                            uint8_t i = Serial.read() - '0';
                            i = constrain(i, 0, PROFILE_COUNT - 1);
                            Serial.read(); // nomf
                            uint8_t v = Serial.read() - '0';
                            v = constrain(v, 0, 2);
                            FW_Common::profileData[i].runMode = v;
                            if(i == FW_Common::profiles.selectedProfile) {
                                switch(v) {
                                  case 0:
                                    FW_Common::SetRunMode(RunMode_Normal);
                                    break;
                                  case 1:
                                    FW_Common::SetRunMode(RunMode_Average);
                                    break;
                                  case 2:
                                    FW_Common::SetRunMode(RunMode_Average2);
                                    break;
                                }
                            }
                            Serial.println("OK: Set Run Mode");
                            break;
                          }
                          case 'l':
                          {
                            Serial.read(); // nomf
                            uint8_t i = Serial.read() - '0';
                            i = constrain(i, 0, PROFILE_COUNT - 1);
                            Serial.read(); // nomf
                            uint8_t v = Serial.read() - '0';
                            v = constrain(v, 0, 1);
                            FW_Common::profileData[i].irLayout = v;
                            Serial.println("OK: Set IR layout type");
                            break;
                          }
                          case 'n':
                          {
                            Serial.read(); // nomf
                            uint8_t s = Serial.read() - '0';
                            s = constrain(s, 0, PROFILE_COUNT - 1);
                            Serial.read(); // nomf
                            for(byte i = 0; i < sizeof(FW_Common::profileData[s].name); i++)
                                FW_Common::profileData[s].name[i] = '\0';
                            for(byte i = 0; i < sizeof(FW_Common::profileData[s].name); i++) {
                                FW_Common::profileData[s].name[i] = Serial.read();
                                if(!Serial.available()) {
                                    break;
                                }
                            }
                            Serial.println("OK: Set Profile Name");
                            break;
                          }
                          case 'c':
                          {
                            Serial.read(); // nomf
                            uint8_t s = Serial.read() - '0';
                            s = constrain(s, 0, PROFILE_COUNT - 1);
                            Serial.read(); // nomf
                            FW_Common::profileData[s].color = Serial.parseInt();
                            Serial.println("OK: Set Profile Color");
                            break;
                          }
                        }
                    }
                }
                break;
              }
              // Print EEPROM values.
              case 'l':
              {
                //Serial.println("Printing values saved in EEPROM...");
                serialInput = Serial.read();
                switch(serialInput) {
                  case 'b':
                    Serial.printf("%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n",
                    SamcoPreferences::toggles[OF_Const::customPins],
                    SamcoPreferences::toggles[OF_Const::rumble],
                    SamcoPreferences::toggles[OF_Const::solenoid],
                    SamcoPreferences::toggles[OF_Const::autofire],
                    SamcoPreferences::toggles[OF_Const::simplePause],
                    SamcoPreferences::toggles[OF_Const::holdToPause],
                    SamcoPreferences::toggles[OF_Const::commonAnode],
                    SamcoPreferences::toggles[OF_Const::lowButtonsMode],
                    SamcoPreferences::toggles[OF_Const::rumbleFF]
                    );
                    break;
                  case 'p':
                    Serial.printf(
                    "%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n",
                    SamcoPreferences::pins[OF_Const::btnTrigger],
                    SamcoPreferences::pins[OF_Const::btnGunA],
                    SamcoPreferences::pins[OF_Const::btnGunB],
                    SamcoPreferences::pins[OF_Const::btnGunC],
                    SamcoPreferences::pins[OF_Const::btnStart],
                    SamcoPreferences::pins[OF_Const::btnSelect],
                    SamcoPreferences::pins[OF_Const::btnGunUp],
                    SamcoPreferences::pins[OF_Const::btnGunDown],
                    SamcoPreferences::pins[OF_Const::btnGunLeft],
                    SamcoPreferences::pins[OF_Const::btnGunRight],
                    SamcoPreferences::pins[OF_Const::btnPedal],
                    SamcoPreferences::pins[OF_Const::btnPedal2],
                    SamcoPreferences::pins[OF_Const::btnHome],
                    SamcoPreferences::pins[OF_Const::btnPump],
                    SamcoPreferences::pins[OF_Const::rumblePin],
                    SamcoPreferences::pins[OF_Const::solenoidPin],
                    SamcoPreferences::pins[OF_Const::rumbleSwitch],
                    SamcoPreferences::pins[OF_Const::solenoidSwitch],
                    SamcoPreferences::pins[OF_Const::autofireSwitch],
                    SamcoPreferences::pins[OF_Const::neoPixel],
                    SamcoPreferences::pins[OF_Const::ledR],
                    SamcoPreferences::pins[OF_Const::ledG],
                    SamcoPreferences::pins[OF_Const::ledB],
                    SamcoPreferences::pins[OF_Const::camSDA],
                    SamcoPreferences::pins[OF_Const::camSCL],
                    SamcoPreferences::pins[OF_Const::periphSDA],
                    SamcoPreferences::pins[OF_Const::periphSCL],
                    SamcoPreferences::pins[OF_Const::battery],
                    SamcoPreferences::pins[OF_Const::analogX],
                    SamcoPreferences::pins[OF_Const::analogY],
                    SamcoPreferences::pins[OF_Const::tempPin]
                    );
                    break;
                  case 's':
                    Serial.printf("%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n",
                    SamcoPreferences::settings[OF_Const::rumbleStrength],
                    SamcoPreferences::settings[OF_Const::rumbleInterval],
                    SamcoPreferences::settings[OF_Const::solenoidNormalInterval],
                    SamcoPreferences::settings[OF_Const::solenoidFastInterval],
                    SamcoPreferences::settings[OF_Const::solenoidHoldLength],
                    SamcoPreferences::settings[OF_Const::autofireWaitFactor],
                    SamcoPreferences::settings[OF_Const::holdToPauseLength],
                    SamcoPreferences::settings[OF_Const::customLEDcount],
                    SamcoPreferences::settings[OF_Const::customLEDstatic],
                    SamcoPreferences::settings[OF_Const::customLEDcolor1],
                    SamcoPreferences::settings[OF_Const::customLEDcolor2],
                    SamcoPreferences::settings[OF_Const::customLEDcolor3]
                    );
                    break;
                  case 'P':
                    serialInput = Serial.read();
                    if(serialInput >= '0' && serialInput <= '3') {
                        uint8_t i = serialInput - '0';
                        Serial.printf("%i,%i,%i,%i,%.2f,%.2f,%i,%i,%i,%i,",
                        FW_Common::profileData[i].topOffset,
                        FW_Common::profileData[i].bottomOffset,
                        FW_Common::profileData[i].leftOffset,
                        FW_Common::profileData[i].rightOffset,
                        FW_Common::profileData[i].TLled,
                        FW_Common::profileData[i].TRled,
                        FW_Common::profileData[i].irSensitivity,
                        FW_Common::profileData[i].runMode,
                        FW_Common::profileData[i].irLayout,
                        FW_Common::profileData[i].color
                        );
                        Serial.println(FW_Common::profileData[i].name);
                    }
                    break;
                  #ifdef USE_TINYUSB
                  case 'i':
                    Serial.printf("%i,",SamcoPreferences::usb.devicePID);
                    if(SamcoPreferences::usb.deviceName[0] == '\0')
                        Serial.println("SERIALREADERR01");
                    else Serial.println(SamcoPreferences::usb.deviceName);
                    break;
                  #endif // USE_TINYUSB
                }
                break;
              }
              // Testing feedback
              case 't':
                serialInput = Serial.read();
                switch(serialInput) {
                    #ifdef USES_SOLENOID
                    case 's':
                      digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH);
                      delay(SamcoPreferences::settings[OF_Const::solenoidNormalInterval]);
                      digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
                      break;
                    #endif // USES_SOLENOID
                    #ifdef USES_RUMBLE
                    case 'r':
                      analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
                      delay(SamcoPreferences::settings[OF_Const::rumbleInterval]);
                      digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
                      break;
                    #endif // USES_RUMBLE
                    #ifdef LED_ENABLE
                    // meant to be for 4pins, but will update all LED devices anyways.
                    case 'R':
                      OF_RGB::LedUpdate(255, 0, 0);
                      break;
                    case 'G':
                      OF_RGB::LedUpdate(0, 255, 0);
                      break;
                    case 'B':
                      OF_RGB::LedUpdate(0, 0, 255);
                      break;
                    #endif // LED_ENABLE
                    default:
                      break;
                }
                break;
              case 'x':
                if(Serial.peek() == 'x') { rp2040.rebootToBootloader(); }
                // we probably left the firmware by now, but eh.
                break;
          }
          break;
    }
}

void OF_Serial::PrintResults()
{
    if(millis() - lastPrintMillis < 100)
        return;

    if(!Serial) {
        FW_Common::stateFlags |= StateFlagsDtrReset;
        return;
    }

    if(FW_Common::stateFlags & StateFlag_PrintPreferences) {
        FW_Common::stateFlags &= ~StateFlag_PrintPreferences;

        // Prints basic storage device information
        // an estimation of storage used, though doesn't take extended prefs into account.
        if(FW_Common::stateFlags & StateFlag_PrintPreferencesStorage) {
            FW_Common::stateFlags &= ~StateFlag_PrintPreferencesStorage;
            
            #ifdef SAMCO_FLASH_ENABLE
                unsigned int required = SamcoPreferences::Size();

            #ifndef PRINT_VERBOSE
                if(required < flash.size())
                    return;
            #endif

                Serial.print("NV Storage capacity: ");
                Serial.print(flash.size());
                Serial.print(", required size: ");
                Serial.println(required);

            #ifdef PRINT_VERBOSE
                Serial.print("Profile struct size: ");
                Serial.print((unsigned int)sizeof(SamcoPreferences::profileData_t));
                Serial.print(", Profile data array size: ");
                Serial.println((unsigned int)sizeof(FW_Common::profileData));
            #endif

            #endif // SAMCO_FLASH_ENABLE
        }

        // prints all stored preferences information in a table
        Serial.print("Default Profile: ");
        Serial.println(FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
        
        Serial.println("Profiles:");
        for(unsigned int i = 0; i < PROFILE_COUNT; ++i) {
            // report if a profile has been cal'd
            if(FW_Common::profileData[i].topOffset && FW_Common::profileData[i].bottomOffset &&
              FW_Common::profileData[i].leftOffset && FW_Common::profileData[i].rightOffset) {
                size_t len = strlen(FW_Common::profileData[i].name);
                Serial.print(FW_Common::profileData[i].name);
                while(len < 18) {
                    Serial.print(' ');
                    ++len;
                }
                Serial.print("Top: ");
                Serial.print(FW_Common::profileData[i].topOffset);
                Serial.print(", Bottom: ");
                Serial.print(FW_Common::profileData[i].bottomOffset);
                Serial.print(", Left: ");
                Serial.print(FW_Common::profileData[i].leftOffset);
                Serial.print(", Right: ");
                Serial.print(FW_Common::profileData[i].rightOffset);
                Serial.print(", TLled: ");
                Serial.print(FW_Common::profileData[i].TLled);
                Serial.print(", TRled: ");
                Serial.print(FW_Common::profileData[i].TRled);
                //Serial.print(", AdjX: ");
                //Serial.print(FW_Common::profileData[i].adjX);
                //Serial.print(", AdjY: ");
                //Serial.print(FW_Common::profileData[i].adjY);
                Serial.print(" IR: ");
                Serial.print((unsigned int)FW_Common::profileData[i].irSensitivity);
                Serial.print(" Mode: ");
                Serial.print((unsigned int)FW_Common::profileData[i].runMode);
                Serial.print(" Layout: ");
                if(FW_Common::profileData[i].irLayout)
                    Serial.println("Diamond");
                else Serial.println("Square");
            }
        }
        /*
        Serial.print(finalX);
        Serial.print(" (");
        Serial.print(MoveXAxis);
        Serial.print("), ");
        Serial.print(finalY);
        Serial.print(" (");
        Serial.print(MoveYAxis);
        Serial.print("), H ");
        Serial.println(mySamco.H());*/

        //Serial.print("conMove ");
        //Serial.print(conMoveXAxis);
        //Serial.println(conMoveYAxis);
        
        if(FW_Common::stateFlags & StateFlag_PrintSelectedProfile) {
            FW_Common::stateFlags &= ~StateFlag_PrintSelectedProfile;

            // Print selected profile
            Serial.print("Profile: ");
            Serial.println(FW_Common::profileData[FW_Common::profiles.selectedProfile].name);

            // Print current sensitivity
            Serial.print("IR Camera Sensitivity: ");
            Serial.println((int)FW_Common::irSensitivity);

            // Subroutine that prints current runmode
            if(FW_Common::runMode < RunMode_Count) {
                Serial.print("Mode: ");
                Serial.println(RunModeLabels[FW_Common::runMode]);
            }

            // Extra settings to print to connected serial monitor when entering Pause Mode
            Serial.print("Offscreen button mode enabled: ");
            if(FW_Common::offscreenButton)
                Serial.println("True");
            else Serial.println("False");

            #ifdef USES_RUMBLE
                Serial.print("Rumble enabled: ");
                if(SamcoPreferences::toggles[OF_Const::rumble])
                    Serial.println("True");
                else Serial.println("False");
            #endif // USES_RUMBLE

            #ifdef USES_SOLENOID
                Serial.print("Solenoid enabled: ");
                if(SamcoPreferences::toggles[OF_Const::solenoid]) {
                    Serial.println("True");
                    Serial.print("Rapid fire enabled: ");
                    if(SamcoPreferences::toggles[OF_Const::autofire])
                        Serial.println("True");
                    else Serial.println("False");

                    Serial.print("Burst fire enabled: ");
                    if(OF_FFB::burstFireActive)
                        Serial.println("True");
                    else Serial.println("False");
                }
                else Serial.println("False");
            #endif // USES_SOLENOID

            #ifdef ARDUINO_ARCH_RP2040
            #ifdef DUAL_CORE
                Serial.println("Running on dual cores.");
            #else
                Serial.println("Running on one core.");
            #endif // DUAL_CORE
            #endif // ARDUINO_ARCH_RP2040

            Serial.print("Firmware version: v");
            Serial.print(OPENFIRE_VERSION, 1);
            Serial.print(" - ");
            Serial.println(OPENFIRE_CODENAME);
        }
                    
        lastPrintMillis = millis();
    }
}

#ifdef DEBUG_SERIAL
void OF_Serial::PrintDebugSerial()
{
    // only print every second
    if(millis() - serialDbMs >= 1000 && Serial) {
        Serial.print("mode ");
        Serial.print(FW_Common::gunMode);
        Serial.print(", IR pos fps ");
        Serial.print(irPosCount);
        Serial.print(", loop/sec ");
        Serial.print(frameCount);

        /*
        Serial.print(", Mouse X,Y ");
        Serial.print(FW_Common::conMoveXAxis);
        Serial.print(",");
        Serial.println(FW_Common::conMoveYAxis);
        */
        
        frameCount = 0;
        irPosCount = 0;
        serialDbMs = millis();
    }
}
#endif // DEBUG_SERIAL