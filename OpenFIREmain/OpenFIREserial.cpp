 /*!
 * @file OpenFIREserial.cpp
 * @brief Serial RX buffer reading routines.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>

#include "OpenFIREserial.h"
#include "OpenFIREprefs.h"
#include "OpenFIREFeedback.h"
#include "OpenFIRElights.h"
#include "boards/OpenFIREshared.h"
#include "OpenFIREcommon.h"

#ifdef MAMEHOOKER
void OF_Serial::SerialProcessing()
{
    // For more info about Serial commands, see the OpenFIRE repo wiki.

    switch(Serial.read()) {
        // Start Signal
        case 'S':
          if(serialMode)
              Serial.println("SERIALREAD: Detected Serial Start command while already in Serial handoff mode!");
          // TODO: handle the other Start line bits? this assume equiv of `S6`
          else {
              serialMode = true;
              OF_FFB::FFBShutdown();

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
                  if(FW_Common::gunMode == FW_Const::GunMode_Run)
                      FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, FW_Common::buttons.analogOutput);
              #endif // USES_DISPLAY
          }
          break;
        // Modesetting Signal
        case 'M':
          switch(Serial.read()) {
              // input mode
              case '0':
                Serial.read(); // nomf
                switch(Serial.read()) {
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
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportCode3 = PAD_A,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode3       = PAD_B,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportCode3       = PAD_X,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Reload].reportCode3  = PAD_Y,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].reportCode3   = PAD_START,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Select].reportCode3  = PAD_SELECT,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Up].reportCode3      = PAD_UP,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Down].reportCode3    = PAD_DOWN,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Left].reportCode3    = PAD_LEFT,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Right].reportCode3   = PAD_RIGHT,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].reportCode3   = PAD_LB,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal2].reportCode3  = PAD_RB,
                      LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pump].reportCode3    = PAD_C;
                      #ifdef USES_DISPLAY
                          FW_Common::OLED.mister = true;
                      #endif // USES_DISPLAY
                      break;
                }
                FW_Common::buttons.ReleaseAll();
                #ifdef USES_DISPLAY
                    if(!serialMode && FW_Common::gunMode == FW_Const::GunMode_Run)
                        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Normal, FW_Common::buttons.analogOutput);
                    else if(serialMode && FW_Common::gunMode == FW_Const::GunMode_Run &&
                            FW_Common::OLED.serialDisplayType > ExtDisplay::ScreenSerial_None &&
                            FW_Common::OLED.serialDisplayType < ExtDisplay::ScreenSerial_Both) {
                        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, FW_Common::buttons.analogOutput);
                    }
                #endif // USES_DISPLAY
                break;
              // offscreen button mode
              case '1':
                Serial.read(); // nomf
                switch(Serial.read()) {
                    // cursor in bottom left - just use disabled
                    case '1':
                    // "true offscreen shot" mode - just use disabled for now
                    case '3':
                    // disabled
                    case '0':
                      FW_Common::UpdateBindings(OF_Prefs::toggles[OF_Const::lowButtonsMode]);
                      break;
                    // offscreen button
                    case '2':
                      memcpy(&LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportType2,
                             OF_Prefs::backupButtonDesc[FW_Const::BtnIdx_A],
                             sizeof(LightgunButtons::Desc_s::reportType)*2);
                      // remap bindings for low button users to make e.g. VCop 3 playable with 1 btn + pedal
                      if(OF_Prefs::toggles[OF_Const::lowButtonsMode])
                          memcpy(&LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportType,
                                 OF_Prefs::backupButtonDesc[FW_Const::BtnIdx_Reload],
                                 sizeof(LightgunButtons::Desc_s::reportType)*2);
                      break;
                }
                break;
              // pedal functionality
              case '2':
                Serial.read();                                         // nomf
                switch(Serial.read()) {
                    // separate button (default to original binds)
                    case '0':
                      FW_Common::UpdateBindings(OF_Prefs::toggles[OF_Const::lowButtonsMode]);
                      break;
                    // make reload button (mapping of Button A)
                    case '1':
                      memcpy(&LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].reportType,
                             OF_Prefs::backupButtonDesc[FW_Const::BtnIdx_A],
                             sizeof(OF_Prefs::backupButtonDesc[0]));
                      break;
                    // make middle mouse button (mapping of Button B, useful for low buttons mode & e.g. using VCop3 ES mode)
                    case '2':
                      memcpy(&LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].reportType,
                             OF_Prefs::backupButtonDesc[FW_Const::BtnIdx_B],
                             sizeof(OF_Prefs::backupButtonDesc[0]));
                      break;
                }
                break;
              // aspect ratio correction
              case '3':
                Serial.read(); // nomf
                serialARcorrection = Serial.read() - '0';
                if(!serialMode) {
                    if(serialARcorrection) { Serial.println("Setting 4:3 correction on!"); }
                    else { Serial.println("Setting 4:3 correction off!"); }
                }
                break;
              #ifdef USES_TEMP
              // temp sensor disabling (why?)
              case '4':
                Serial.read(); // nomf
                Serial.read();
                break;
              #endif // USES_TEMP
              // autoreload (TODO: maybe?)
              case '5':
                Serial.read(); // nomf
                Serial.read();
                break;
              // rumble only mode (enable Rumble FF)
              case '6':
                Serial.read(); // nomf
                switch(Serial.read()) {
                    // disable
                    case '0':
                      if(OF_Prefs::pins[OF_Const::solenoidSwitch] == -1 && OF_Prefs::pins[OF_Const::solenoidPin] >= 0)
                          OF_Prefs::toggles[OF_Const::solenoid] = true;
                      if(OF_Prefs::pins[OF_Const::rumblePin] >= 0) 
                          OF_Prefs::toggles[OF_Const::rumbleFF] = false;
                      break;
                    // enable
                    case '1':
                      if(OF_Prefs::pins[OF_Const::rumbleSwitch] == -1 && OF_Prefs::pins[OF_Const::rumblePin] >= 0) { OF_Prefs::toggles[OF_Const::rumble] = true; }
                      if(OF_Prefs::pins[OF_Const::solenoidSwitch] == -1 && OF_Prefs::pins[OF_Const::solenoidPin] >= 0) { OF_Prefs::toggles[OF_Const::solenoid] = false; }
                      if(OF_Prefs::pins[OF_Const::rumblePin] >= 0) { OF_Prefs::toggles[OF_Const::rumbleFF] = true; }
                      break;
                }
                OF_FFB::FFBShutdown();
                break;
              #ifdef USES_SOLENOID
              // solenoid automatic mode
              case '8':
                Serial.read(); // Nomf the padding bit.
                switch(Serial.read()) {
                // "auto"
                case '1':
                    OF_FFB::burstFireActive = true;
                    OF_Prefs::toggles[OF_Const::autofire] = false;
                    break;
                // "always on"
                case '2':
                    OF_Prefs::toggles[OF_Const::autofire] = true;
                    OF_FFB::burstFireActive = false;
                    break;
                // disabled
                case '0':
                    OF_Prefs::toggles[OF_Const::autofire] = false;
                    OF_FFB::burstFireActive = false;
                    break;
                }
                break;
              #endif // USES_SOLENOID
              #ifdef USES_DISPLAY
              case 'D':
                Serial.read(); // Nomf padding byte
                switch(Serial.read()) {
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
                if(FW_Common::gunMode == FW_Const::GunMode_Run) {
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
                  serialMode = false;
                  memset(serialQueue, false, sizeof(serialQueue));
                  serialARcorrection = false;
                  #ifdef USES_DISPLAY
                      FW_Common::OLED.serialDisplayType = ExtDisplay::ScreenSerial_None;
                      if(FW_Common::gunMode == FW_Const::GunMode_Run) FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Normal, FW_Common::buttons.analogOutput);
                  #endif // USES_DISPLAY
                  #ifdef LED_ENABLE
                      // Clear any stale serial LED pulses
                      serialLEDPulseColorMap = 0b00000000;
                      serialLEDPulses = 0;
                      serialLEDPulsesLast = 0;
                      serialLEDPulseRising = true;
                      serialLEDR = 0;
                      serialLEDG = 0;
                      serialLEDB = 0;
                      serialLEDChange = false;
                      if(FW_Common::gunMode == FW_Const::GunMode_Run)
                        OF_RGB::LedOff(); // Turn it off, and let lastSeen handle it from here.
                  #endif // LED_ENABLE
                  #ifdef USES_RUMBLE
                      digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
                      serialRumbPulseStage = 0;
                      serialRumbPulses = 0;
                      serialRumbPulsesLast = 0;
                      serialRumbCustomHoldLength = 0;
                      serialRumbCustomPauseLength = 0;
                  #endif // USES_RUMBLE
                  #ifdef USES_SOLENOID
                      digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);
                      serialSolPulses = 0;
                      serialSolPulsesLast = 0;
                      serialSolCustomHoldLength = 0;
                      serialSolCustomPauseLength = 0;
                  #endif // USES_SOLENOID
                  FW_Common::buttons.ReleaseAll();
                  // remap back to defaults, in case they were changed
                  FW_Common::UpdateBindings(OF_Prefs::toggles[OF_Const::lowButtonsMode]);
                  Serial.println("Received end serial pulse, releasing FF override.");
              }
              break;
          }
          break;
        // owo SPECIAL SETUP EH?
        case 'X':
          switch(Serial.read()) {
              // Set Autofire Interval Length
              case 'I':
                OF_FFB::autofireDoubleLengthWait = Serial.read() - '0';
                break;
              // Remap player numbers
              case 'R':
              {
                char serialInput = Serial.read();
                if(serialInput >= '1' && serialInput <= '4') {
                    playerStartBtn = serialInput;
                    playerSelectBtn = serialInput + 4;
                    FW_Common::UpdateBindings(OF_Prefs::toggles[OF_Const::lowButtonsMode]);
                } else Serial.println("SERIALREAD: Player remap command called, but an invalid or no slot number was declared!");
                break;
              }
              default:
                Serial.println("SERIALREAD: Internal setting cmd detected, but not valid!");
                Serial.println("Internally recognized commands are:");
                Serial.println("I(nterval Autofire)2/3/4 / R(emap)1/2/3/4");
                break;
          }
          // End of 'X'
          break;
        // Enter Docked Mode
        case OF_Const::sDock1:
          if(Serial.read() == OF_Const::sDock2) {
            #ifdef DUAL_CORE // This may be being run from Core 1, so signal if running in main Run Mode.
            if(FW_Common::gunMode == FW_Const::GunMode_Run)
                rp2040.fifo.push(FW_Const::GunMode_Docked);
            else FW_Common::SetMode(FW_Const::GunMode_Docked);
            #else
            FW_Common::SetMode(FW_Const::GunMode_Docked);
            #endif // DUAL_CORE
          }
          break;
        // Force Feedback
        case 'F':
          switch(Serial.read()) {
              #ifdef USES_SOLENOID
              // Solenoid bits
              case '0':
                Serial.read(); // nomf the padding
                switch(Serial.read()) {
                // Solenoid "on" command
                case '1':
                    #ifdef USES_TEMP
                    // block every other signal if temp is at warning threshold
                    if(OF_Prefs::pins[OF_Const::tempPin] > -1) {
                        switch(OF_FFB::tempStatus) {
                        case OF_FFB::Temp_Safe:
                            serialQueue[SerialQueue_Solenoid] = true;
                            break;
                        case OF_FFB::Temp_Warning:
                            if(serialSolTempBuffer) serialQueue[SerialQueue_Solenoid] = true;
                            serialSolTempBuffer = !serialSolTempBuffer;
                            break;
                        case OF_FFB::Temp_Fatal:
                        default:
                            break;
                        }
                    } else 
                    #endif // USES_TEMP
                    serialQueue[SerialQueue_Solenoid] = true;
                    break;
                // Solenoid "pulse" command (only if not already pulsing)
                case '2':
                    if(!serialQueue[SerialQueue_SolPulse]) {
                        Serial.read(); // nomf the padding bit.
                        if(Serial.peek() >= '0' & Serial.peek() <= '9') {
                            serialQueue[SerialQueue_SolPulse] = true;
                            char serialInputS[4] = {0,0,0,0};
                            for(uint n = 0; n < 3; ++n) {
                                serialInputS[n] = Serial.read();
                                if(Serial.peek() < '0' || Serial.peek() > '9')
                                    break;
                            }
                            serialSolPulses = atoi(serialInputS);
                            if(!serialSolPulses) serialSolPulses++;
                            serialSolPulsesLast = 0;
                        }
                    }
                    break;
                // Solenoid "off" command
                case '0':
                    serialQueue[SerialQueue_Solenoid] = false, serialQueue[SerialQueue_SolPulse] = false;
                    break;
                }
                break;
              #endif // USES_SOLENOID
              #ifdef USES_RUMBLE
              // Rumble bits
              case '1':
                Serial.read(); // nomf the padding
                switch(Serial.read()) {
                // Rumble "on" command
                case '1':
                    serialQueue[SerialQueue_Rumble] = true;
                    break;
                // Rumble "pulse" command (only if not already pulsing)
                case '2':
                    if(!serialQueue[SerialQueue_RumbPulse]) {
                        Serial.read(); // nomf the padding
                        if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                            serialQueue[SerialQueue_RumbPulse] = true;
                            char serialInputS[4] = {0,0,0,0};
                            for(uint n = 0; n < 3; ++n) {
                                serialInputS[n] = Serial.read();
                                if(Serial.peek() < '0' || Serial.peek() > '9')
                                    break;
                            }
                            serialRumbPulses = atoi(serialInputS);
                            if(!serialRumbPulses) serialRumbPulses++;
                            serialRumbPulsesLast = 0;
                        }
                    }
                    break;
                // Rumble "off" command
                case '0':
                    serialQueue[SerialQueue_Rumble] = false, serialQueue[SerialQueue_RumbPulse] = false;
                    break;
                }
                break;
              #endif // USES_RUMBLE
              #ifdef LED_ENABLE
              // LED Red bits
              case '2':
                Serial.read(); // nomf the padding
                switch(Serial.read()) {
                // LED Red "static on" command
                case '1':
                    Serial.read(); // nomf
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        serialLEDChange = true;
                        serialQueue[SerialQueue_Red] = true;
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }
                        // Static emitting overrides pulse bits
                        serialLEDR = atoi(serialInputS);
                        serialQueue[SerialQueue_LEDPulse] = false;
                        serialLEDPulseColorMap = 0;
                    }
                    break;
                // LED Red "pulse" command (only if not already pulsing)
                case '2':
                    if(!serialQueue[SerialQueue_LEDPulse]) {
                        Serial.read(); // nomf
                        if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                            serialLEDChange = true, serialQueue[SerialQueue_LEDPulse] = true,
                            serialLEDPulseColorMap = 0b00000001; // Set the R LED as the one pulsing only (overwrites the others).
                            char serialInputS[4] = {0,0,0,0};
                            for(uint n = 0; n < 3; ++n) {
                                serialInputS[n] = Serial.read();
                                if(Serial.peek() < '0' || Serial.peek() > '9')
                                    break;
                            }
                            serialLEDPulses = atoi(serialInputS);
                            serialLEDPulsesLast = 0;
                        }
                    }
                    break;
                // LED Red "off" command
                case '0':
                    serialLEDChange = true, serialQueue[SerialQueue_Red] = false, serialQueue[SerialQueue_LEDPulse] = false,
                    serialLEDR = 0, serialLEDPulseColorMap = 0;
                    break;
                }
                break;
              // LED Green bits
              case '3':
                Serial.read(); // nomf the padding
                switch(Serial.read()) {
                // LED Green "static on" command
                case '1':
                    Serial.read(); // nomf
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_Green] = true;
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }
                        serialLEDG = atoi(serialInputS);
                        serialQueue[SerialQueue_LEDPulse] = false, serialLEDPulseColorMap = 0;
                    }
                    break;
                // LED Green "pulse" command (only if not already pulsing)
                case '2':
                    if(!serialQueue[SerialQueue_LEDPulse]) {
                        Serial.read(); // nomf
                        if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                            serialLEDChange = true, serialQueue[SerialQueue_LEDPulse] = true,
                            serialLEDPulseColorMap = 0b00000010; // Set the G LED as the one pulsing only (overwrites the others).
                            char serialInputS[4] = {0,0,0,0};
                            for(uint n = 0; n < 3; ++n) {
                                serialInputS[n] = Serial.read();
                                if(Serial.peek() < '0' || Serial.peek() > '9')
                                    break;
                            }
                            serialLEDPulses = atoi(serialInputS);
                            serialLEDPulsesLast = 0;
                        }
                    }
                    break;
                // LED Green "off" command
                case '0':
                    serialLEDChange = true,
                    serialQueue[SerialQueue_Green] = false, serialQueue[SerialQueue_LEDPulse] = false,
                    serialLEDG = 0, serialLEDPulseColorMap = 0;
                    break;
                }
                break;
              // LED Blue bits
              case '4':
                Serial.read(); // nomf the padding
                switch(Serial.read()) {
                // LED Blue "static on" command
                case '1':
                    Serial.read(); // nomf
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        serialLEDChange = true, serialQueue[SerialQueue_Blue] = true;
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }
                        serialLEDB = atoi(serialInputS);
                        serialQueue[SerialQueue_LEDPulse] = false;
                        serialLEDPulseColorMap = 0;
                    }
                    break;
                // LED Blue "pulse" command (only if not already pulsing)
                case '2':
                    if(!serialQueue[SerialQueue_LEDPulse]) {
                        Serial.read(); // nomf
                        if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                            serialLEDChange = true, serialQueue[SerialQueue_LEDPulse] = true,
                            serialLEDPulseColorMap = 0b00000100; // Set the B LED as the one pulsing only (overwrites the others).
                            char serialInputS[4] = {0,0,0,0};
                            for(uint n = 0; n < 3; ++n) {
                                serialInputS[n] = Serial.read();
                                if(Serial.peek() < '0' || Serial.peek() > '9')
                                    break;
                            }
                            serialLEDPulses = atoi(serialInputS);
                            serialLEDPulsesLast = 0;
                        }
                    }
                    break;
                // LED Blue "off" command
                case '0':
                    serialLEDChange = true,
                    serialQueue[SerialQueue_Blue] = false, serialQueue[SerialQueue_LEDPulse] = false,
                    serialLEDB = 0, serialLEDPulseColorMap = 0;
                    break;
                }
                break;
              #endif // LED_ENABLE
              #ifdef USES_DISPLAY
              case 'D':
                switch(Serial.read()) {
                case 'A':
                    Serial.read(); // nomf the padding
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }
                        serialAmmoCount = atoi(serialInputS);
                        serialAmmoCount = constrain(serialAmmoCount, 0, 99);
                        serialDisplayChange = true;
                    }
                    break;
                case 'L':
                    Serial.read(); // nomf the padding
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
                            serialInputS[n] = Serial.read();
                            if(Serial.peek() < '0' || Serial.peek() > '9')
                                break;
                        }

                        serialLifeCount = atoi(serialInputS);
                        if(FW_Common::OLED.lifeBar) {
                            if(serialLifeCount > FW_Common::dispMaxLife)
                                FW_Common::dispMaxLife = serialLifeCount;
                            FW_Common::dispLifePercentage = (100 * serialLifeCount) / FW_Common::dispMaxLife; // Calculate the Life % to show 
                        }
                        serialDisplayChange = true;
                    }
                    break;
                }
                break;
              #endif // USES_DISPLAY
              #if !defined(USES_SOLENOID) && !defined(USES_RUMBLE) && !defined(LED_ENABLE)
              default:
                //Serial.println("SERIALREAD: Feedback command detected, but no feedback devices are built into this firmware!");
                break;
              #endif
          }
          // End of 'F'
          break;
        // Custom Pulse Overrides
        case 'R':
          switch(Serial.read()) {
              // Solenoid
              case '0':
                Serial.read(); // nomf
                if(Serial.peek() >= '0' && Serial.peek() <= '2') {
                    char serialInput = Serial.read();
                    Serial.read(); // nomf
                    if(Serial.peek() >= '0' && Serial.peek() <='9') {
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
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
                if(Serial.peek() >= '0' && Serial.peek() <= '2') {
                    char serialInput = Serial.read();
                    Serial.read(); // nomf
                    if(Serial.peek() >= '0' && Serial.peek() <= '9') {
                        char serialInputS[4] = {0,0,0,0};
                        for(uint n = 0; n < 3; ++n) {
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
      if(OF_Prefs::toggles[OF_Const::solenoid]) {
          // Solenoid "on" command
          if(serialQueue[SerialQueue_Solenoid]) {
              if(digitalRead(OF_Prefs::pins[OF_Const::solenoidPin])) {
                  if(millis() - serialSolTimestamp > SERIAL_SOLENOID_MAXSHUTOFF) {
                      digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);
                      serialQueue[SerialQueue_Solenoid] = false;
                  }
              } else {
                  digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], HIGH);
                  serialSolTimestamp = millis();
              }
          // Solenoid "pulse" command
          } else if(serialQueue[SerialQueue_SolPulse]) {
              if(!serialSolPulsesLast) {                            // Have we started pulsing?
                  digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], HIGH);  // Start pulsing it on!
                  serialSolPulsesLast++;                                 // Start the sequence.
                  serialSolPulsesLastUpdate = millis();                  // timestamp
              } else if(serialSolPulsesLast <= serialSolPulses) {   // Have we met the pulses quota?
                  if(digitalRead(OF_Prefs::pins[OF_Const::solenoidPin])) {
                      // custom hold length
                      if(serialSolCustomHoldLength) {
                          if(millis() - serialSolPulsesLastUpdate >= serialSolCustomHoldLength) {
                              digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);  // Start pulsing it off.
                              if(serialSolPulsesLast >= serialSolPulses)
                                  serialQueue[SerialQueue_SolPulse] = false;
                              else serialSolPulsesLast++, serialSolPulsesLastUpdate = millis();  // Timestamp our last pulse event.
                          }
                      // current settings hold length
                      } else if(millis() - serialSolPulsesLastUpdate >= OF_Prefs::settings[OF_Const::solenoidOnLength]) {
                          digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);  // Start pulsing it off.
                          if(serialSolPulsesLast >= serialSolPulses)
                              serialQueue[SerialQueue_SolPulse] = false;
                          else serialSolPulsesLast++, serialSolPulsesLastUpdate = millis();  // Timestamp our last pulse event.
                      }
                  } else {
                      // custom pause length
                      if(serialSolCustomPauseLength) {
                          if(millis() - serialSolPulsesLastUpdate >= serialSolCustomPauseLength) {
                              digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], HIGH); // Start pulsing it on.
                              serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                          }
                      // current settings pause length
                      } else if(millis() - serialSolPulsesLastUpdate >=
                                OF_Prefs::settings[OF_Const::solenoidOffLength] << OF_FFB::autofireDoubleLengthWait ? 1 : 0) {
                          digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], HIGH); // Start pulsing it on.
                          serialSolPulsesLastUpdate = millis();          // Timestamp our last pulse event.
                      }
                  }
              }
          // Solenoid "off" command
          } else digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);
      // solenoid toggle not allowed, just force it off.
      } else digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);
  #endif // USES_SOLENOID

  #ifdef USES_RUMBLE
      if(OF_Prefs::toggles[OF_Const::rumble]) {
          // Rumble "on" command
          if(serialQueue[SerialQueue_Rumble]) {
              analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]); // turn/keep it on.
          // Rumble "pulse" command
          } else if(serialQueue[SerialQueue_RumbPulse]) {
              // Pulses start
              if(!serialRumbPulsesLast) {
                  if(serialRumbCustomHoldLength && serialRumbCustomPauseLength)
                       analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]);
                  else analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength] / 3);
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
                              digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
                              if(serialRumbPulsesLast >= serialRumbPulses)
                                  serialQueue[SerialQueue_RumbPulse] = false;
                          }
                      } else if(millis() - serialRumbPulsesLastUpdate > serialRumbCustomPauseLength) {
                          serialRumbPulseStage++;
                          analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]);
                      }
                  // OF-style analog ramping
                  } else if(millis() - serialRumbPulsesLastUpdate > serialRumbPulsesLength) { // have we waited enough time between pulse stages?
                      switch(serialRumbPulseStage) {
                          // Rising to Sustain
                          case 0:
                              analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]);
                              serialRumbPulseStage++;                    // Increments the stage of the pulse.
                              serialRumbPulsesLastUpdate = millis();     // and timestamps when we've had updated this last.
                              break;                                     // Then quits until next pulse stage
                          // Sustain to Falling
                          case 1:
                              analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength] / 2);
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          // Falloff
                          case 2:
                              analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength] / 3);
                              serialRumbPulseStage++;
                              serialRumbPulsesLastUpdate = millis();
                              break;
                          // Check
                          case 3:
                              if(serialRumbPulsesLast >= serialRumbPulses) {
                                  digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
                                  serialQueue[SerialQueue_RumbPulse] = false;
                              } else serialRumbPulsesLast++, serialRumbPulseStage = 0;
                              break;
                      }
                  }
              }
          // Rumble "off"
          } else digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
      // Rumble disabled, not allowed to be on
      } else digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
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

// Serial Buffer in Docked Mode should always be being read by the main core on multicore systems
void OF_Serial::SerialProcessingDocked()
{
    switch(Serial.read()) {
    // Enter Docked Mode (in case running from first boot)
    case OF_Const::sDock1:
        if(Serial.read() == OF_Const::sDock2) FW_Common::SetMode(FW_Const::GunMode_Docked);
        break;
    // Common terminator
    case OF_Const::serialTerminator:
        if(!FW_Common::justBooted)
            FW_Common::SetMode(FW_Const::GunMode_Run);
        else FW_Common::SetMode(FW_Const::GunMode_Init);
        FW_Common::SetRunMode((FW_Const::RunMode_e)OF_Prefs::profiles[OF_Prefs::currentProfile].runMode);
        break;
        
    //// Prefs senders
    //
    case OF_Const::sGetToggles:  SerialBatchSend(&OF_Prefs::toggles,  OF_Prefs::OFPresets.boolTypes_Strings,     sizeof(OF_Prefs::toggles)  / OF_Const::boolTypesCount    ); break;
    case OF_Const::sGetPins:     SerialBatchSend(&OF_Prefs::pins,     OF_Prefs::OFPresets.boardInputs_Strings,   sizeof(OF_Prefs::pins)     / OF_Const::boardInputsCount  ); break;
    case OF_Const::sGetSettings: SerialBatchSend(&OF_Prefs::settings, OF_Prefs::OFPresets.settingsTypes_Strings, sizeof(OF_Prefs::settings) / OF_Const::settingsTypesCount); break;
    case OF_Const::sGetProfile:
        for(int prof = 0; prof < PROFILE_COUNT; ++prof)
            SerialBatchSend(&OF_Prefs::profiles[prof], OF_Prefs::OFPresets.profSettingTypes_Strings, sizeof(uint32_t), prof);
        break;

    //// State changes/direct control methods
    //
    case OF_Const::sIRTest:
        if(FW_Common::camNotAvailable) {
            Serial.read(); // nomf
            char message[2] = { OF_Const::sError, OF_Const::sErrCam };
            Serial.write(message, sizeof(message));
        } else if(FW_Common::runMode == FW_Const::RunMode_Processing && Serial.read() == false) {
            switch(OF_Prefs::profiles[OF_Prefs::currentProfile].runMode) {
            case FW_Const::RunMode_Normal:
                FW_Common::SetRunMode(FW_Const::RunMode_Normal);
                break;
            case FW_Const::RunMode_Average:
                FW_Common::SetRunMode(FW_Const::RunMode_Average);
                break;
            case FW_Const::RunMode_Average2:
                FW_Common::SetRunMode(FW_Const::RunMode_Average2);
                break;
            }
        } else if(Serial.read() == true)
            FW_Common::SetRunMode(FW_Const::RunMode_Processing);
        break;
    case OF_Const::sCaliProfile:
    {
        if(Serial.peek() < PROFILE_COUNT) {
            FW_Common::SelectCalProfile(Serial.read());
            char buf[2] = {OF_Const::sCurrentProf, (uint8_t)OF_Prefs::currentProfile};
            Serial.write(buf, 2);
            if(Serial.read() == OF_Const::sCaliStart) {
                if(FW_Common::camNotAvailable) Serial.write(OF_Const::sError);
                else {
                  // sensitivity/layout preset
                  if(Serial.peek() != -1) {
                    FW_Common::SetIrSensitivity(Serial.peek() & 0b11110000);
                    FW_Common::SetIrLayout(Serial.read() >> 4);
                  }

                  FW_Common::SetMode(FW_Const::GunMode_Calibration);
                  FW_Common::ExecCalMode(true);
                }
            }
        }
        break;
    }
    #ifdef USES_SOLENOID
    case OF_Const::sTestSolenoid:
        if(Serial.read() == true) {
            digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], HIGH);
            delay(OF_Prefs::settings[OF_Const::solenoidOnLength]);
            digitalWrite(OF_Prefs::pins[OF_Const::solenoidPin], LOW);
        }
        break;
    #endif // USES_SOLENOID
    #ifdef USES_RUMBLE
    case OF_Const::sTestRumble:
        if(Serial.read() == true) {
            analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]);
            delay(OF_Prefs::settings[OF_Const::rumbleInterval]);
            digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
        }
        break;
    #endif // USES_RUMBLE
    #ifdef LED_ENABLE // meant to be for 4pins, but will update all LED devices anyways.
    case OF_Const::sTestLEDR:
        if(Serial.read() == true) OF_RGB::LedUpdate(255, 0, 0);
        break;
    case OF_Const::sTestLEDG:
        if(Serial.read() == true) OF_RGB::LedUpdate(0, 255, 0);
        break;
    case OF_Const::sTestLEDB:
        if(Serial.read() == true) OF_RGB::LedUpdate(0, 0, 255);
        break;
    #endif // LED_ENABLE

    case OF_Const::sCommitStart:
    {
        if(Serial.available() == 1 && Serial.read() == true) {
            FW_Common::buttons.Unset();
            bool exit = false;
            size_t type, rxLen, datSize, profNum;
            Serial.write(OF_Const::sCommitStart), Serial.flush();
            while(!exit) {
                if(Serial.available()) {
                    rxLen = 0;
                    type = Serial.read();
                    switch(type) {
                    //// Commands
                    case OF_Const::sSave:
                        if(FW_Common::SavePreferences() == OF_Prefs::Error_Success) {
                            // For updating pin data for buttons, cams and periphs
                            FW_Common::PinsReset();
                            FW_Common::CameraSet();
                            FW_Common::FeedbackSet();
                            
                            // Update bindings so LED/Pixel changes are reflected immediately
                            if(OF_Prefs::usb.devicePID >= 1 && OF_Prefs::usb.devicePID <= 4) {
                                playerStartBtn = OF_Prefs::usb.devicePID + '0';
                                playerSelectBtn = OF_Prefs::usb.devicePID + '0' + 4;
                            }
                            FW_Common::UpdateBindings(OF_Prefs::toggles[OF_Const::lowButtonsMode]);

                        #ifdef LED_ENABLE
                            // Save op above resets color, so re-set it back to docked idle color
                            if(FW_Common::gunMode == FW_Const::GunMode_Docked)
                                OF_RGB::LedUpdate(127, 127, 255);
                            else if(FW_Common::gunMode == FW_Const::GunMode_Pause)
                                OF_RGB::SetLedPackedColor(OF_Prefs::profiles[OF_Prefs::currentProfile].color);
                        #endif // LED_ENABLE
                        // unlikely, but attempt to reload settings if save failed
                        // though this might just load corrupt data instead. :shrug:
                        } else OF_Prefs::Load();
                        FW_Common::buttons.Begin();
                        exit = true;
                        break;
                    case OF_Const::serialTerminator:
                        // Assumed failed/aborting save, so roll back to what's in flash.
                        OF_Prefs::Load();
                        exit = true;
                        break;

                    //// Saving ops
                    case OF_Const::sCommitID:
                        rxLen = Serial.readBytes(RXbuf, Serial.available());
                        if(rxLen == 18) memcpy(&OF_Prefs::usb, RXbuf, rxLen);
                        break;
                    default:
                        rxLen = Serial.readBytesUntil('\0', RXbuf, 32);
                        RXbuf[rxLen++] = '\0';
                        datSize = Serial.read();
                        RXbuf[rxLen++] = datSize;
                        if(type == OF_Const::sCommitProfile && OF_Prefs::OFPresets.profSettingTypes_Strings.at(RXbuf) != OF_Const::profCurrent) {
                            profNum = Serial.read();
                            RXbuf[rxLen++] = profNum;
                        }
                        rxLen += Serial.readBytes(&RXbuf[rxLen], datSize);
                        switch(type) {
                            case OF_Const::sCommitToggles:  SerialBatchRecv(RXbuf, OF_Prefs::toggles,  OF_Prefs::OFPresets.boolTypes_Strings,     sizeof(OF_Prefs::toggles)  / OF_Const::boolTypesCount,     datSize, rxLen); break;
                            case OF_Const::sCommitPins:     SerialBatchRecv(RXbuf, OF_Prefs::pins,     OF_Prefs::OFPresets.boardInputs_Strings,   sizeof(OF_Prefs::pins)     / OF_Const::boardInputsCount,   datSize, rxLen); break;
                            case OF_Const::sCommitSettings: SerialBatchRecv(RXbuf, OF_Prefs::settings, OF_Prefs::OFPresets.settingsTypes_Strings, sizeof(OF_Prefs::settings) / OF_Const::settingsTypesCount, datSize, rxLen); break;
                            case OF_Const::sCommitProfile:
                                if(profNum < PROFILE_COUNT) SerialBatchRecv(RXbuf,
                                                                            &OF_Prefs::profiles[profNum],
                                                                            OF_Prefs::OFPresets.profSettingTypes_Strings,
                                                                            sizeof(uint32_t),
                                                                            datSize,
                                                                            rxLen);
                                break;
                            default:
                                break;
                        }
                        break;
                    }
                    
                    if(rxLen > 0) { Serial.write(RXbuf, rxLen); Serial.flush(); }
                }
            }
        }
        break;
    }

    case OF_Const::sClearFlash:
        if(Serial.available() == 1 && Serial.read() == OF_Const::sClearFlash) {
            OF_Prefs::ResetPreferences();
            rp2040.reboot();
        } else while(Serial.available()) Serial.read();
        break;
    }
}

void OF_Serial::SerialBatchSend(void *dataPtr, const std::unordered_map<std::string, int> &mapPtr, const size_t &dataSize, const int &profNum)
{
    size_t pos;
    bool profNumSent = false;
    for(auto &pair : mapPtr) {
        if(pair.second >= 0) {
            pos = 0;
            strcpy(&TXbuf[pos], pair.first.c_str());
            pos += pair.first.length()+1;
            if(&mapPtr == &OF_Prefs::OFPresets.profSettingTypes_Strings) {
                if(pair.second == OF_Const::profCurrent) {
                    if(profNumSent) continue;
                    else {
                        TXbuf[pos++] = 1;
                        TXbuf[pos++] = OF_Prefs::currentProfile;
                        profNumSent = true;
                    }
                } else {
                    if(pair.second == OF_Const::profName) {
                        TXbuf[pos++] = sizeof(OF_Prefs::ProfileData_s::name);
                        TXbuf[pos++] = profNum;
                        memcpy(&TXbuf[pos], (uint8_t*)dataPtr + (dataSize * pair.second), sizeof(OF_Prefs::ProfileData_s::name));
                        pos += sizeof(OF_Prefs::ProfileData_s::name);
                    } else {
                        TXbuf[pos++] = dataSize;
                        TXbuf[pos++] = profNum;
                        memcpy(&TXbuf[pos], (uint8_t*)dataPtr + (dataSize * pair.second), dataSize);
                        pos += dataSize;
                    }
                }
            } else {
                TXbuf[pos++] = dataSize;
                memcpy(&TXbuf[pos], (uint8_t*)dataPtr + (dataSize * pair.second), dataSize);
                pos += dataSize;
            }
            
            for(int sendTry = 0; sendTry < 3; ++sendTry) {
                Serial.write(TXbuf, pos);
                Serial.flush();
                while(Serial.available() < pos) {}
                Serial.readBytes(RXbuf, Serial.available());

                if(!memcmp(RXbuf, TXbuf, pos)) break;
                if(sendTry >= 2) {
                    Serial.write(OF_Const::sError);
                    Serial.flush();
                    return;
                }
            }
        }
    }

    if(profNum == -1 || profNum >= PROFILE_COUNT-1) {
        Serial.write(OF_Const::serialTerminator);
        Serial.flush();
    }
}

void OF_Serial::SerialBatchRecv(const char *bufPtr, void *dataPtr, const std::unordered_map<std::string, int> &mapPtr, const size_t &dataSize, const size_t &rxDatSize, const size_t &rxBufSize)
{
    if(mapPtr.count(bufPtr)) {
        if(&mapPtr == &OF_Prefs::OFPresets.profSettingTypes_Strings && mapPtr.count(bufPtr) == OF_Const::profCurrent) {
            memcpy(&OF_Prefs::currentProfile, &bufPtr[rxBufSize-rxDatSize], rxDatSize);
        } else memcpy((uint8_t*)dataPtr + (dataSize * mapPtr.at(bufPtr)), &bufPtr[rxBufSize-rxDatSize], rxDatSize);
    }
}

void OF_Serial::PrintResults()
{
    if(millis() - lastPrintMillis < 100)
        return;

    if(!Serial) {
        FW_Common::stateFlags |= FW_Const::StateFlagsDtrReset;
        return;
    }

    if(FW_Common::stateFlags & FW_Const::StateFlag_PrintPreferences) {
        FW_Common::stateFlags &= ~FW_Const::StateFlag_PrintPreferences;

        // Prints basic storage device information
        // an estimation of storage used, though doesn't take extended prefs into account.
        if(FW_Common::stateFlags & FW_Const::StateFlag_PrintPreferencesStorage) {
            FW_Common::stateFlags &= ~FW_Const::StateFlag_PrintPreferencesStorage;
            
            #ifdef SAMCO_FLASH_ENABLE
                unsigned int required = OF_Prefs::Size();

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
                Serial.print((unsigned int)sizeof(OF_Prefs::profileData_t));
                Serial.print(", Profile data array size: ");
                Serial.println((unsigned int)sizeof(OF_Prefs::profiles));
            #endif

            #endif // SAMCO_FLASH_ENABLE
        }

        // prints all stored preferences information in a table
        Serial.print("Default Profile: ");
        Serial.println(OF_Prefs::profiles[OF_Prefs::currentProfile].name);
        
        Serial.println("Profiles:");
        for(uint i = 0; i < PROFILE_COUNT; ++i) {
            // report if a profile has been cal'd
            if(OF_Prefs::profiles[i].topOffset && OF_Prefs::profiles[i].bottomOffset &&
              OF_Prefs::profiles[i].leftOffset && OF_Prefs::profiles[i].rightOffset) {
                size_t len = strlen(OF_Prefs::profiles[i].name);
                Serial.print(OF_Prefs::profiles[i].name);
                while(len < 18) {
                    Serial.print(' ');
                    ++len;
                }
                Serial.print("Top: ");
                Serial.print(OF_Prefs::profiles[i].topOffset);
                Serial.print(", Bottom: ");
                Serial.print(OF_Prefs::profiles[i].bottomOffset);
                Serial.print(", Left: ");
                Serial.print(OF_Prefs::profiles[i].leftOffset);
                Serial.print(", Right: ");
                Serial.print(OF_Prefs::profiles[i].rightOffset);
                Serial.print(", TLled: ");
                Serial.print(OF_Prefs::profiles[i].TLled);
                Serial.print(", TRled: ");
                Serial.print(OF_Prefs::profiles[i].TRled);
                //Serial.print(", AdjX: ");
                //Serial.print(OF_Prefs::profiles[i].adjX);
                //Serial.print(", AdjY: ");
                //Serial.print(OF_Prefs::profiles[i].adjY);
                Serial.print(" IR: ");
                Serial.print((unsigned int)OF_Prefs::profiles[i].irSens);
                Serial.print(" Mode: ");
                Serial.print((unsigned int)OF_Prefs::profiles[i].runMode);
                Serial.print(" Layout: ");
                if(OF_Prefs::profiles[i].irLayout)
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
        
        if(FW_Common::stateFlags & FW_Const::StateFlag_PrintSelectedProfile) {
            FW_Common::stateFlags &= ~FW_Const::StateFlag_PrintSelectedProfile;

            // Print selected profile
            Serial.print("Profile: ");
            Serial.println(OF_Prefs::profiles[OF_Prefs::currentProfile].name);

            // Print current sensitivity
            Serial.print("IR Camera Sensitivity: ");
            Serial.println((int)OF_Prefs::profiles[OF_Prefs::currentProfile].irSens);

            // Subroutine that prints current runmode
            if(FW_Common::runMode < FW_Const::RunMode_Count) {
                Serial.print("Mode: ");
                Serial.println(FW_Const::RunModeLabels[FW_Common::runMode]);
            }

            #ifdef USES_RUMBLE
                Serial.print("Rumble enabled: ");
                if(OF_Prefs::toggles[OF_Const::rumble])
                    Serial.println("True");
                else Serial.println("False");
            #endif // USES_RUMBLE

            #ifdef USES_SOLENOID
                Serial.print("Solenoid enabled: ");
                if(OF_Prefs::toggles[OF_Const::solenoid]) {
                    Serial.println("True");
                    Serial.print("Rapid fire enabled: ");
                    if(OF_Prefs::toggles[OF_Const::autofire])
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

            Serial.printf("Firmware version: v%.1f"
                          #ifdef GIT_HASH
                          "-%s"
                          #endif // GIT_ HASH);
                          , OPENFIRE_VERSION
                          #ifdef GIT_HASH
                          , GIT_HASH
                          #endif // GIT_HASH
                          );
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
