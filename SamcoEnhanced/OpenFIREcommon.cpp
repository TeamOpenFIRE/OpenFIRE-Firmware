 /*!
 * @file OpenFIREcommon.h
 * @brief Shared methods used throughout the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>
#include <Wire.h>
#include "OpenFIREcommon.h"
#include "OpenFIRElights.h"
#include "OpenFIREserial.h"

// button object instance (defined in OpenFIREcommon.h)
LightgunButtons FW_Common::buttons(lgbData, ButtonCount);

void FW_Common::FeedbackSet()
{
    #ifdef USES_RUMBLE
        if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0)
            pinMode(SamcoPreferences::pins[OF_Const::rumblePin], OUTPUT);
        else SamcoPreferences::toggles[OF_Const::rumble] = false;
    #endif // USES_RUMBLE

    #ifdef USES_SOLENOID
        if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0)
            pinMode(SamcoPreferences::pins[OF_Const::solenoidPin], OUTPUT);
        else SamcoPreferences::toggles[OF_Const::solenoid] = false;
    #endif // USES_SOLENOID

    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(SamcoPreferences::pins[OF_Const::rumbleSwitch] >= 0)
                pinMode(SamcoPreferences::pins[OF_Const::rumbleSwitch], INPUT_PULLUP);
        #endif // USES_RUMBLE

        #ifdef USES_SOLENOID
            if(SamcoPreferences::pins[OF_Const::solenoidSwitch] >= 0)
                pinMode(SamcoPreferences::pins[OF_Const::solenoidSwitch], INPUT_PULLUP);
        #endif // USES_SOLENOID

        if(SamcoPreferences::pins[OF_Const::autofireSwitch] >= 0)
            pinMode(SamcoPreferences::pins[OF_Const::autofireSwitch], INPUT_PULLUP);
    #endif // USES_SWITCHES

    #ifdef USES_ANALOG
        analogReadResolution(12);
        #ifdef USES_TEMP
        if(SamcoPreferences::pins[OF_Const::analogX] >= 0 && SamcoPreferences::pins[OF_Const::analogY] >= 0 && SamcoPreferences::pins[OF_Const::analogX] != SamcoPreferences::pins[OF_Const::analogY] &&
           SamcoPreferences::pins[OF_Const::analogX] != SamcoPreferences::pins[OF_Const::tempPin] && SamcoPreferences::pins[OF_Const::analogY] != SamcoPreferences::pins[OF_Const::tempPin])
        #else
        if(SamcoPreferences::pins[OF_Const::analogX] >= 0 && SamcoPreferences::pins[OF_Const::analogY] >= 0 && SamcoPreferences::pins[OF_Const::analogX] != SamcoPreferences::pins[OF_Const::analogY])
        #endif // USES_TEMP
            //pinMode(analogPinX, INPUT);
            //pinMode(analogPinY, INPUT);
            analogIsValid = true;
        else analogIsValid = false;
    #endif // USES_ANALOG

    #if defined(LED_ENABLE) && defined(FOURPIN_LED)
    if(SamcoPreferences::pins[OF_Const::ledR] < 0 || SamcoPreferences::pins[OF_Const::ledG] < 0 || SamcoPreferences::pins[OF_Const::ledB] < 0)
        ledIsValid = false;
    else {
        pinMode(SamcoPreferences::pins[OF_Const::ledR], OUTPUT);
        pinMode(SamcoPreferences::pins[OF_Const::ledG], OUTPUT);
        pinMode(SamcoPreferences::pins[OF_Const::ledB], OUTPUT);
        ledIsValid = true;
    }
    #endif // FOURPIN_LED

    #ifdef CUSTOM_NEOPIXEL
    if(SamcoPreferences::pins[OF_Const::neoPixel] >= 0)
        OF_RGB::InitExternPixel(SamcoPreferences::pins[OF_Const::neoPixel]);
    #endif // CUSTOM_NEOPIXEL

    #ifdef USES_DISPLAY
    // wrapper will manage display validity
    if(SamcoPreferences::pins[OF_Const::periphSCL] >= 0 && SamcoPreferences::pins[OF_Const::periphSDA] >= 0 &&
      // check it's not using the camera's I2C line
       bitRead(SamcoPreferences::pins[OF_Const::camSCL], 1) != bitRead(SamcoPreferences::pins[OF_Const::periphSCL], 1) &&
       bitRead(SamcoPreferences::pins[OF_Const::camSDA], 1) != bitRead(SamcoPreferences::pins[OF_Const::periphSDA], 1))
        if(!OLED.Begin())
            if(OLED.display != nullptr) delete OLED.display;
    #endif // USES_DISPLAY
}

void FW_Common::PinsReset()
{
    if(dfrIRPos != nullptr) {
        delete dfrIRPos;
        dfrIRPos = nullptr;
    }

    #ifdef USES_RUMBLE
        if(SamcoPreferences::pins[OF_Const::rumblePin] >= 0)
            pinMode(SamcoPreferences::pins[OF_Const::rumblePin], INPUT);
    #endif // USES_RUMBLE

    #ifdef USES_SOLENOID
        if(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0)
            pinMode(SamcoPreferences::pins[OF_Const::solenoidPin], INPUT);
    #endif // USES_SOLENOID

    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(SamcoPreferences::pins[OF_Const::rumbleSwitch] >= 0)
                pinMode(SamcoPreferences::pins[OF_Const::rumbleSwitch], INPUT);
        #endif // USES_RUMBLE

        #ifdef USES_SOLENOID
            if(SamcoPreferences::pins[OF_Const::solenoidSwitch] >= 0)
                pinMode(SamcoPreferences::pins[OF_Const::solenoidSwitch], INPUT);
        #endif // USES_SOLENOID

        if(SamcoPreferences::pins[OF_Const::autofireSwitch] >= 0)
            pinMode(SamcoPreferences::pins[OF_Const::autofireSwitch], INPUT);
    #endif // USES_SWITCHES

    #ifdef LED_ENABLE
        OF_RGB::LedOff();

        #ifdef FOURPIN_LED
            if(ledIsValid) {
                pinMode(SamcoPreferences::pins[OF_Const::ledR], INPUT);
                pinMode(SamcoPreferences::pins[OF_Const::ledG], INPUT);
                pinMode(SamcoPreferences::pins[OF_Const::ledB], INPUT);
            }
        #endif // FOURPIN_LED

        #ifdef CUSTOM_NEOPIXEL
            if(OF_RGB::externPixel != nullptr) {
                OF_RGB::externPixel->clear();
                delete OF_RGB::externPixel;
                OF_RGB::externPixel = nullptr;
            }
        #endif // CUSTOM_NEOPIXEL
    #endif // LED_ENABLE

    #ifdef USES_DISPLAY
        if(OLED.display != nullptr)
            OLED.Stop();
    #endif // USES_DISPLAY
}

void FW_Common::CameraSet()
{
    // Sanity check: which channel do these pins correlate to?
    if(bitRead(SamcoPreferences::pins[OF_Const::camSCL], 1) && bitRead(SamcoPreferences::pins[OF_Const::camSDA], 1)) {
        // I2C1
        if(bitRead(SamcoPreferences::pins[OF_Const::camSCL], 0) && !bitRead(SamcoPreferences::pins[OF_Const::camSDA], 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire1.setSDA(SamcoPreferences::pins[OF_Const::camSDA]);
            Wire1.setSCL(SamcoPreferences::pins[OF_Const::camSCL]);
        }
        dfrIRPos = new DFRobotIRPositionEx(Wire1);
    } else if(!bitRead(SamcoPreferences::pins[OF_Const::camSCL], 1) && !bitRead(SamcoPreferences::pins[OF_Const::camSDA], 1)) {
        // I2C0
        if(bitRead(SamcoPreferences::pins[OF_Const::camSCL], 0) && !bitRead(SamcoPreferences::pins[OF_Const::camSDA], 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire.setSDA(SamcoPreferences::pins[OF_Const::camSDA]);
            Wire.setSCL(SamcoPreferences::pins[OF_Const::camSCL]);
        }
        dfrIRPos = new DFRobotIRPositionEx(Wire);
    }

    // Start IR Camera with basic data format
    dfrIRPos->begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity);
}

void FW_Common::SetMode(const GunMode_e &newMode)
{
    if(gunMode == newMode)
        return;
    
    // exit current mode
    switch(gunMode) {
    case GunMode_Run:
        stateFlags |= StateFlag_PrintPreferences;
        break;
    case GunMode_Pause:
        break;
    case GunMode_Docked:
        if(newMode != GunMode_Calibration)
            Serial.println("Undocking.");
        break;
    }
    
    // enter new mode
    gunMode = newMode;
    switch(newMode) {
    case GunMode_Run:
        // begin run mode with all 4 points seen
        lastSeen = 0x0F;

        #ifdef USES_DISPLAY
            if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both)
                OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
            else if(OF_Serial::serialMode)
                OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, buttons.analogOutput);
            else OLED.ScreenModeChange(ExtDisplay::Screen_Normal, buttons.analogOutput);

            OLED.TopPanelUpdate("Prof: ", profileData[profiles.selectedProfile].name);
        #endif // USES_DISPLAY

        break;
    case GunMode_Calibration:
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_Calibrating);
            OLED.TopPanelUpdate("Cali: ", profileData[profiles.selectedProfile].name);
        #endif // USES_DISPLAY
        break;
    case GunMode_Pause:
        stateFlags |= StateFlag_SavePreferencesEn | StateFlag_PrintSelectedProfile;

        #ifdef USES_DISPLAY
          OLED.ScreenModeChange(ExtDisplay::Screen_Pause);
          OLED.TopPanelUpdate("Using ", profileData[profiles.selectedProfile].name);

          if(SamcoPreferences::toggles[OF_Const::simplePause]) 
              OLED.PauseListUpdate(pauseModeSelection);
          else OLED.PauseScreenShow(profiles.selectedProfile, profileData[0].name, profileData[1].name, profileData[2].name, profileData[3].name);
        #endif // USES_DISPLAY

        break;
    case GunMode_Docked:
        stateFlags |= StateFlag_SavePreferencesEn;

        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
        #endif // USES_DISPLAY

        break;
    }

    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE
}

void FW_Common::SetRunMode(const RunMode_e &newMode)
{
    if(newMode >= RunMode_Count)
        return;

    // block Processing/test modes being applied to a profile
    if(newMode <= RunMode_ProfileMax && profileData[profiles.selectedProfile].runMode != newMode) {
        profileData[profiles.selectedProfile].runMode = newMode;
        stateFlags |= StateFlag_SavePreferencesEn;
    }
    
    if(runMode != newMode) {
        runMode = newMode;
        //if(!(stateFlags & StateFlag_PrintSelectedProfile))
            //PrintRunMode();
    }
}

// Dedicated calibration method
void FW_Common::ExecCalMode(const bool &fromDesktop)
{
    buttons.ReportDisable();

    uint8_t calStage = 0;

    // hold values in a buffer till calibration is complete
    int topOffset;
    int bottomOffset;
    int leftOffset;
    int rightOffset;

    // backup current values in case the user cancels
    int _topOffset = profileData[profiles.selectedProfile].topOffset;
    int _bottomOffset = profileData[profiles.selectedProfile].bottomOffset;
    int _leftOffset = profileData[profiles.selectedProfile].leftOffset;
    int _rightOffset = profileData[profiles.selectedProfile].rightOffset;
    float _TLled = profileData[profiles.selectedProfile].TLled;
    float _TRled = profileData[profiles.selectedProfile].TRled;
    float _adjX = profileData[profiles.selectedProfile].adjX;
    float _adjY = profileData[profiles.selectedProfile].adjY;

    // set current values to factory defaults
    profileData[profiles.selectedProfile].topOffset = 0;
    profileData[profiles.selectedProfile].bottomOffset = 0;
    profileData[profiles.selectedProfile].leftOffset = 0;
    profileData[profiles.selectedProfile].rightOffset = 0;

    // Force center mouse to center
    AbsMouse5.move(32768/2, 32768/2);

    // Initialize current mouse positions (local variables)
    int32_t mouseCurrentX = 32768 / 2;
    int32_t mouseCurrentY = 32768 / 2;

    // Initialize variables for incremental movement
    int32_t mouseTargetX = mouseCurrentX;
    int32_t mouseTargetY = mouseCurrentY;
    bool mouseMoving = false;

    // Jack in, CaliMan, execute!!!
    SetMode(GunMode_Calibration);
    Serial.printf("CalStage: %d\r\n", Cali_Init);

    while(gunMode == GunMode_Calibration) {
        buttons.Poll(1);

        if(irPosUpdateTick) {
            irPosUpdateTick = 0;
            GetPosition();
        }

        // Handle incremental mouse movement
        if (mouseMoving) {
            int32_t deltaX = mouseTargetX - mouseCurrentX;
            int32_t deltaY = mouseTargetY - mouseCurrentY;
            int32_t stepX = 30;
            int32_t stepY = 30;

            if (abs(deltaX) < stepX) stepX = abs(deltaX);
            if (abs(deltaY) < stepY) stepY = abs(deltaY);

            if (deltaX != 0)
                mouseCurrentX += (deltaX > 0) ? stepX : -stepX;

            if (deltaY != 0)
                mouseCurrentY += (deltaY > 0) ? stepY : -stepY;

            AbsMouse5.move(mouseCurrentX, mouseCurrentY);

            if (mouseCurrentX == mouseTargetX && mouseCurrentY == mouseTargetY) {
                mouseMoving = false;
                delay(5);  // Optional small delay
            }
        }

        // Handle button presses and calibration stages
        if((buttons.pressedReleased & (ExitPauseModeBtnMask | ExitPauseModeHoldBtnMask) || Serial.peek() == 'X') && !justBooted) {
            Serial.printf("CalStage: %d\r\n", Cali_Verify+1);

            // Reapplying backed up data
            profileData[profiles.selectedProfile].topOffset = _topOffset;
            profileData[profiles.selectedProfile].bottomOffset = _bottomOffset;
            profileData[profiles.selectedProfile].leftOffset = _leftOffset;
            profileData[profiles.selectedProfile].rightOffset = _rightOffset;
            profileData[profiles.selectedProfile].TLled = _TLled;
            profileData[profiles.selectedProfile].TRled = _TRled;
            profileData[profiles.selectedProfile].adjX = _adjX;
            profileData[profiles.selectedProfile].adjY = _adjY;

            // Re-print the profile
            stateFlags |= StateFlag_PrintSelectedProfile;

            // Exit back to docked mode or run mode, depending on if pinged from Desktop App
            if(fromDesktop)
                SetMode(GunMode_Docked);
            else SetMode(GunMode_Run);

            return;
        } else if(buttons.pressed == BtnMask_Trigger && !mouseMoving) {
            calStage++;
            Serial.printf("CalStage: %d\r\n", calStage);

            // Ensure our messages go through, or else the HID reports eat UART.
            Serial.flush();

            switch(calStage) {
                case Cali_Init:
                    // Initial state, nothing to do (but center cursor for desktop use)
                    if(fromDesktop)
                        AbsMouse5.move(32768/2, 32768/2);
                    break;
                case Cali_Top:
                    // Reset Offsets
                    topOffset = 0;
                    bottomOffset = 0;
                    leftOffset = 0;
                    rightOffset = 0;

                    // Set Cam center offsets
                    if(profileData[profiles.selectedProfile].irLayout) {
                        profileData[profiles.selectedProfile].adjX = (OpenFIREdiamond.testMedianX() - (512 << 2)) * cos(OpenFIREdiamond.Ang()) -
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * sin(OpenFIREdiamond.Ang()) + (512 << 2);
                        profileData[profiles.selectedProfile].adjY = (OpenFIREdiamond.testMedianX() - (512 << 2)) * sin(OpenFIREdiamond.Ang()) +
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * cos(OpenFIREdiamond.Ang()) + (384 << 2);
                    } else {
                        profileData[profiles.selectedProfile].adjX = (OpenFIREsquare.testMedianX() - (512 << 2)) * cos(OpenFIREsquare.Ang()) -
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * sin(OpenFIREsquare.Ang()) + (512 << 2);
                        profileData[profiles.selectedProfile].adjY = (OpenFIREsquare.testMedianX() - (512 << 2)) * sin(OpenFIREsquare.Ang()) +
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * cos(OpenFIREsquare.Ang()) + (384 << 2);
                        // Work out LED locations by assuming height is 100%
                        profileData[profiles.selectedProfile].TLled = (res_x / 2) - ((OpenFIREsquare.W() * (res_y  / OpenFIREsquare.H())) / 2);
                        profileData[profiles.selectedProfile].TRled = (res_x / 2) + ((OpenFIREsquare.W() * (res_y  / OpenFIREsquare.H())) / 2);
                    }

                    // Update Cam centre in perspective library
                    OpenFIREper.source(profileData[profiles.selectedProfile].adjX, profileData[profiles.selectedProfile].adjY);
                    OpenFIREper.deinit(0);

                    // Set mouse movement to top position
                    if(!fromDesktop) {
                        mouseTargetX = 32768 / 2;
                        mouseTargetY = 0;
                        mouseMoving = true;
                    }
                    break;
                case Cali_Bottom:
                    // Set Offset buffer
                    topOffset = mouseY;
                    Serial.printf("CalUpd: 1.%d\r\n", topOffset);

                    // Set mouse movement to bottom position
                    if(!fromDesktop) {
                        mouseTargetX = 32768 / 2;
                        mouseTargetY = 32767;
                        mouseMoving = true;
                    }
                    break;
                case Cali_Left:
                    // Set Offset buffer
                    bottomOffset = (res_y - mouseY);
                    Serial.printf("CalUpd: 2.%d\r\n", bottomOffset);

                    // Set mouse movement to left position
                    if(!fromDesktop) {
                        mouseTargetX = 0;
                        mouseTargetY = 32768 / 2;
                        mouseMoving = true;
                    }
                    break;
                case Cali_Right:
                    // Set Offset buffer
                    leftOffset = mouseX;
                    Serial.printf("CalUpd: 3.%d\r\n", leftOffset);

                    // Set mouse movement to right position
                    if(!fromDesktop) {
                        mouseTargetX = 32767;
                        mouseTargetY = 32768 / 2;
                        mouseMoving = true;
                    }
                    break;
                case Cali_Center:
                    // Set Offset buffer
                    rightOffset = (res_x - mouseX);
                    Serial.printf("CalUpd: 4.%d\r\n", rightOffset);

                    // Save Offset buffer to profile
                    profileData[profiles.selectedProfile].topOffset = topOffset;
                    profileData[profiles.selectedProfile].bottomOffset = bottomOffset;
                    profileData[profiles.selectedProfile].leftOffset = leftOffset;
                    profileData[profiles.selectedProfile].rightOffset = rightOffset;

                    // Move back to center calibration point
                    if(!fromDesktop) {
                        mouseTargetX = 32768 / 2;
                        mouseTargetY = 32768 / 2;
                        mouseMoving = true;
                    }
                    break;
                case Cali_Verify:
                    // Apply new Cam center offsets with Offsets applied
                    if(profileData[profiles.selectedProfile].irLayout) {
                        profileData[profiles.selectedProfile].adjX = (OpenFIREdiamond.testMedianX() - (512 << 2)) * cos(OpenFIREdiamond.Ang()) -
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * sin(OpenFIREdiamond.Ang()) + (512 << 2);
                        profileData[profiles.selectedProfile].adjY = (OpenFIREdiamond.testMedianX() - (512 << 2)) * sin(OpenFIREdiamond.Ang()) +
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * cos(OpenFIREdiamond.Ang()) + (384 << 2);
                    } else {
                        profileData[profiles.selectedProfile].adjX = (OpenFIREsquare.testMedianX() - (512 << 2)) * cos(OpenFIREsquare.Ang()) -
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * sin(OpenFIREsquare.Ang()) + (512 << 2);
                        profileData[profiles.selectedProfile].adjY = (OpenFIREsquare.testMedianX() - (512 << 2)) * sin(OpenFIREsquare.Ang()) +
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * cos(OpenFIREsquare.Ang()) + (384 << 2);
                    }

                    Serial.printf("CalUpd: 5.%f\r\n", profileData[profiles.selectedProfile].TLled);
                    Serial.printf("CalUpd: 6.%f\r\n", profileData[profiles.selectedProfile].TRled);
                    Serial.flush();

                    // Update Cam centre in perspective library
                    OpenFIREper.source(profileData[profiles.selectedProfile].adjX, profileData[profiles.selectedProfile].adjY);
                    OpenFIREper.deinit(0);

                    // Let the user test.
                    SetMode(GunMode_Verification);
                    while(gunMode == GunMode_Verification) {
                        buttons.Poll();

                        if(irPosUpdateTick) {
                            irPosUpdateTick = 0;
                            GetPosition();
                        }

                        // If it's good, move onto calibration finish.
                        if(buttons.pressed == BtnMask_Trigger) {
                            calStage++;
                            // Stay in Verification Mode; the code outside of the calibration loop will catch us.
                            break;
                        // Press A/B to restart calibration for current profile
                        } else if(buttons.pressedReleased & ExitPauseModeHoldBtnMask) {
                            calStage = 0;
                            Serial.printf("CalStage: %d\r\n", Cali_Init);
                            Serial.flush();

                            // (Re)set current values to factory defaults
                            profileData[profiles.selectedProfile].topOffset = 0;
                            profileData[profiles.selectedProfile].bottomOffset = 0;
                            profileData[profiles.selectedProfile].leftOffset = 0;
                            profileData[profiles.selectedProfile].rightOffset = 0;
                            profileData[profiles.selectedProfile].adjX = 512 << 2;
                            profileData[profiles.selectedProfile].adjY = 384 << 2;
                            SetMode(GunMode_Calibration);
                            AbsMouse5.move(32768/2, 32768/2);
                        // Press C/Home to exit without committing new calibration values
                        } else if(buttons.pressedReleased & ExitPauseModeBtnMask && !justBooted) {
                            Serial.printf("CalStage: %d\r\n", Cali_Verify+1);

                            // Reapply backed-up data
                            profileData[profiles.selectedProfile].topOffset = _topOffset;
                            profileData[profiles.selectedProfile].bottomOffset = _bottomOffset;
                            profileData[profiles.selectedProfile].leftOffset = _leftOffset;
                            profileData[profiles.selectedProfile].rightOffset = _rightOffset;
                            profileData[profiles.selectedProfile].TLled = _TLled;
                            profileData[profiles.selectedProfile].TRled = _TRled;
                            profileData[profiles.selectedProfile].adjX = _adjX;
                            profileData[profiles.selectedProfile].adjY = _adjY;

                            // Re-print the profile
                            stateFlags |= StateFlag_PrintSelectedProfile;

                            // Re-apply the calibration stored in the profile
                            if(fromDesktop)
                                SetMode(GunMode_Docked);
                            else SetMode(GunMode_Run);
                            return;
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    // Break calibration
    if(justBooted) {
        // If this is an initial calibration, save it immediately!
        stateFlags |= StateFlag_SavePreferencesEn;
        SavePreferences();
        if(fromDesktop)
            SetMode(GunMode_Docked);
    } else if(fromDesktop) {
        // TODO: won't be needed, as we send prof data with each cali stage.
        Serial.printf("UpdatedProf: %d\r\n", profiles.selectedProfile);
        Serial.println(profileData[profiles.selectedProfile].topOffset);
        Serial.println(profileData[profiles.selectedProfile].bottomOffset);
        Serial.println(profileData[profiles.selectedProfile].leftOffset);
        Serial.println(profileData[profiles.selectedProfile].rightOffset);
        Serial.println(profileData[profiles.selectedProfile].TLled);
        Serial.println(profileData[profiles.selectedProfile].TRled);
        SetMode(GunMode_Docked);
    } else SetMode(GunMode_Run);

    #ifdef USES_RUMBLE
        if(SamcoPreferences::toggles[OF_Const::rumble]) {
            analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
            delay(80);
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], false);
            delay(50);
            analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
            delay(125);
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], false);
        }
    #endif // USES_RUMBLE

    Serial.printf("CalStage: %d\r\n", Cali_Verify+1);
}

void FW_Common::GetPosition()
{
    int error = dfrIRPos->basicAtomic(DFRobotIRPositionEx::Retry_2);
    if(error == DFRobotIRPositionEx::Error_Success) {
        // if diamond layout, or square
        if(profileData[profiles.selectedProfile].irLayout) {
            OpenFIREdiamond.begin(dfrIRPos->xPositions(), dfrIRPos->yPositions(), dfrIRPos->seen());
            OpenFIREper.warp(OpenFIREdiamond.X(0), OpenFIREdiamond.Y(0),
                             OpenFIREdiamond.X(1), OpenFIREdiamond.Y(1),
                             OpenFIREdiamond.X(2), OpenFIREdiamond.Y(2),
                             OpenFIREdiamond.X(3), OpenFIREdiamond.Y(3),
                             res_x / 2, 0, 0,
                             res_y / 2, res_x / 2,
                             res_y, res_x, res_y / 2);
        } else {
            OpenFIREsquare.begin(dfrIRPos->xPositions(), dfrIRPos->yPositions(), dfrIRPos->seen());
            OpenFIREper.warp(OpenFIREsquare.X(0), OpenFIREsquare.Y(0),
                             OpenFIREsquare.X(1), OpenFIREsquare.Y(1),
                             OpenFIREsquare.X(2), OpenFIREsquare.Y(2),
                             OpenFIREsquare.X(3), OpenFIREsquare.Y(3),
                             profileData[profiles.selectedProfile].TLled, 0,
                             profileData[profiles.selectedProfile].TRled, 0,
                             profileData[profiles.selectedProfile].TLled, res_y,
                             profileData[profiles.selectedProfile].TRled, res_y);
        }

        // Output mapped to screen resolution because offsets are measured in pixels
        mouseX = map(OpenFIREper.getX(), 0, res_x, (0 - profileData[profiles.selectedProfile].leftOffset), (res_x + profileData[profiles.selectedProfile].rightOffset));                 
        mouseY = map(OpenFIREper.getY(), 0, res_y, (0 - profileData[profiles.selectedProfile].topOffset), (res_y + profileData[profiles.selectedProfile].bottomOffset));

        switch(runMode) {
            case RunMode_Average:
                // 2 position moving average
                moveIndex ^= 1;
                moveXAxisArr[moveIndex] = mouseX;
                moveYAxisArr[moveIndex] = mouseY;
                mouseX = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                mouseY = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                break;
            case RunMode_Average2:
                // weighted average of current position and previous 2
                if(moveIndex < 2)
                    ++moveIndex;
                else moveIndex = 0;

                moveXAxisArr[moveIndex] = mouseX;
                moveYAxisArr[moveIndex] = mouseY;
                mouseX = (mouseX + moveXAxisArr[0] + moveXAxisArr[1] + moveXAxisArr[2]) / 4;
                mouseY = (mouseY + moveYAxisArr[0] + moveYAxisArr[1] + moveYAxisArr[2]) / 4;
                break;
            default:
                break;
            }

        // Constrain that bisch so negatives don't cause underflow
        int32_t conMoveX = constrain(mouseX, 0, res_x);
        int32_t conMoveY = constrain(mouseY, 0, res_y);

        // Output mapped to Mouse resolution
        conMoveX = map(conMoveX, 0, res_x, 0, 32767);
        conMoveY = map(conMoveY, 0, res_y, 0, 32767);

        if(gunMode == GunMode_Run) {
            UpdateLastSeen();

            if(OF_Serial::serialARcorrection) {
                conMoveX = map(conMoveX, 4147, 28697, 0, 32767);
                conMoveX = constrain(conMoveX, 0, 32767);
            }

            bool offXAxis = false;
            bool offYAxis = false;

            if(conMoveX == 0 || conMoveX == 32767)
                offXAxis = true;
            
            if(conMoveY == 0 || conMoveY == 32767)
                offYAxis = true;

            if(offXAxis || offYAxis)
                buttons.offScreen = true;
            else buttons.offScreen = false;

            if(buttons.analogOutput)
                Gamepad16.moveCam(conMoveX, conMoveY);
            else AbsMouse5.move(conMoveX, conMoveY);

        } else if(gunMode == GunMode_Verification) {
            AbsMouse5.move(conMoveX, conMoveY);
        } else {
            if(millis() - testLastStamp > 50) {
                testLastStamp = millis();
                // RAW Camera Output mapped to screen res (1920x1080)
                int rawX[4];
                int rawY[4];
                // RAW Output for viewing in processing sketch mapped to 1920x1080 screen resolution
                for (int i = 0; i < 4; i++) {
                    if(profileData[profiles.selectedProfile].irLayout) {
                        rawX[i] = map(OpenFIREdiamond.X(i), 0, 1023 << 2, 1920, 0);
                        rawY[i] = map(OpenFIREdiamond.Y(i), 0, 768 << 2, 0, 1080);
                    } else {
                        rawX[i] = map(OpenFIREsquare.X(i), 0, 1023 << 2, 0, 1920);
                        rawY[i] = map(OpenFIREsquare.Y(i), 0, 768 << 2, 0, 1080);
                    }
                }

                if(runMode == RunMode_Processing) {
                    if(profileData[profiles.selectedProfile].irLayout) {
                        Serial.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
                                      rawX[0], rawY[0],
                                      rawX[1], rawY[1],
                                      rawX[2], rawY[2],
                                      rawX[3], rawY[3],
                                      mouseX / 4, mouseY / 4,
                                      map(OpenFIREdiamond.testMedianX(), 0, 1023 << 2, 1920, 0),
                                      map(OpenFIREdiamond.testMedianY(), 0, 768 << 2, 0, 1080));
                    } else {
                        Serial.printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
                                      rawX[0], rawY[0],
                                      rawX[1], rawY[1],
                                      rawX[2], rawY[2],
                                      rawX[3], rawY[3],
                                      mouseX / 4, mouseY / 4,
                                      map(OpenFIREsquare.testMedianX(), 0, 1023 << 2, 0, 1920),
                                      map(OpenFIREsquare.testMedianY(), 0, 768 << 2, 0, 1080));
                    }
                }

                #ifdef USES_DISPLAY
                    OLED.DrawVisibleIR(rawX, rawY);
                #endif // USES_DISPLAY
            }
        }
    } else if(error != DFRobotIRPositionEx::Error_DataMismatch) {
        // set flag to warn desktop app when docking
        if(!camNotAvailable)
            camNotAvailable = true;

        if(millis() - camWarningTimestamp > CAM_WARNING_INTERVAL) {
            Serial.println("CAMERROR: Not available");
            camWarningTimestamp = millis();
        }
    }
}

void FW_Common::UpdateLastSeen()
{
    if(profileData[profiles.selectedProfile].irLayout) {
        if(lastSeen != OpenFIREdiamond.seen()) {
            #ifdef MAMEHOOKER
            if(!OF_Serial::serialMode)
            #endif // MAMEHOOKER
                #ifdef LED_ENABLE
                if(!lastSeen && OpenFIREdiamond.seen())
                    OF_RGB::LedOff();
                else if(lastSeen && !OpenFIREdiamond.seen())
                    OF_RGB::SetLedPackedColor(OF_RGB::IRSeen0Color);
                #endif // LED_ENABLE

            lastSeen = OpenFIREdiamond.seen();
        }
    } else {
        if(lastSeen != OpenFIREsquare.seen()) {
            #ifdef MAMEHOOKER
            if(!OF_Serial::serialMode) {
            #endif // MAMEHOOKER
                #ifdef LED_ENABLE
                if(!lastSeen && OpenFIREsquare.seen())
                    OF_RGB::LedOff();
                else if(lastSeen && !OpenFIREsquare.seen())
                    OF_RGB::SetLedPackedColor(OF_RGB::IRSeen0Color);
                #endif // LED_ENABLE
            #ifdef MAMEHOOKER
            }
            #endif // MAMEHOOKER
            lastSeen = OpenFIREsquare.seen();
        }
    }
}

bool FW_Common::SelectCalProfile(const uint8_t &profile)
{
    if(profile >= PROFILE_COUNT)
        return false;

    if(profiles.selectedProfile != profile) {
        stateFlags |= StateFlag_PrintSelectedProfile;
        profiles.selectedProfile = profile;
    }

    OpenFIREper.source(profileData[profiles.selectedProfile].adjX, profileData[profiles.selectedProfile].adjY);                                                          
    OpenFIREper.deinit(0);

    // set IR sensitivity
    if(profileData[profile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max)
        SetIrSensitivity(profileData[profile].irSensitivity);

    // set run mode
    if(profileData[profile].runMode < RunMode_Count)
        SetRunMode((RunMode_e)profileData[profile].runMode);

    #ifdef USES_DISPLAY
        if(gunMode != GunMode_Docked)
            OLED.TopPanelUpdate("Using ", profileData[profiles.selectedProfile].name);
    #endif // USES_DISPLAY
 
    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE

    // enable save to allow setting new default profile
    stateFlags |= StateFlag_SavePreferencesEn;
    return true;
}

#ifdef LED_ENABLE
void FW_Common::SetLedColorFromMode()
{
    switch(gunMode) {
    case GunMode_Calibration:
        OF_RGB::SetLedPackedColor(OF_RGB::CalModeColor);
        break;
    case GunMode_Pause:
        OF_RGB::SetLedPackedColor(profileData[profiles.selectedProfile].color);
        break;
    case GunMode_Run:
        if(lastSeen)
             OF_RGB::LedOff();
        else OF_RGB::SetLedPackedColor(OF_RGB::IRSeen0Color);
        break;
    default:
        break;
    }
}
#endif // LED_ENABLE

void FW_Common::SetIrSensitivity(const uint8_t &sensitivity)
{
    if(sensitivity > DFRobotIRPositionEx::Sensitivity_Max)
        return;

    if(profileData[profiles.selectedProfile].irSensitivity != sensitivity) {
        profileData[profiles.selectedProfile].irSensitivity = sensitivity;
        stateFlags |= StateFlag_SavePreferencesEn;
    }

    if(irSensitivity != (DFRobotIRPositionEx::Sensitivity_e)sensitivity) {
        irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)sensitivity;
        dfrIRPos->sensitivityLevel(irSensitivity);
        //if(!(stateFlags & StateFlag_PrintSelectedProfile))
            //PrintIrSensitivity();
    }
}

void FW_Common::SetIrLayout(const uint8_t &layout)
{
    // TODO: we need an enum for layout types available
    if(layout > 1)
        return;

    if(profileData[profiles.selectedProfile].irLayout != layout) {
        profileData[profiles.selectedProfile].irLayout = layout;
        stateFlags |= StateFlag_SavePreferencesEn;
    }
}

void FW_Common::LoadPreferences()
{
    if(!nvAvailable)
        return;

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Load(flash);
#else
    nvPrefsError = SamcoPreferences::LoadProfiles();
#endif // SAMCO_FLASH_ENABLE
    
    // Profile sanity checks
    // resets offsets that are wayyyyy too unreasonably high
    for(unsigned int i = 0; i < PROFILE_COUNT; ++i) {
        if(profileData[i].rightOffset >= 32768 || profileData[i].bottomOffset >= 32768 ||
           profileData[i].topOffset >= 32768 || profileData[i].leftOffset >= 32768) {
            profileData[i].topOffset = 0;
            profileData[i].bottomOffset = 0;
            profileData[i].leftOffset = 0;
            profileData[i].rightOffset = 0;
        }
    
        if(profileData[i].irSensitivity > DFRobotIRPositionEx::Sensitivity_Max)
            profileData[i].irSensitivity = DFRobotIRPositionEx::Sensitivity_Default;

        if(profileData[i].runMode >= RunMode_Count)
            profileData[i].runMode = RunMode_Normal;
    }

    // if default profile is not valid, use current selected profile instead
    if(profiles.selectedProfile >= PROFILE_COUNT)
        profiles.selectedProfile = (uint8_t)profiles.selectedProfile;
}

void FW_Common::SavePreferences()
{
    // Unless the user's Docked,
    // Only allow one write per pause state until something changes.
    // Extra protection to ensure the same data can't write a bunch of times.
    if(gunMode != GunMode_Docked) {
        if(!nvAvailable || !(stateFlags & StateFlag_SavePreferencesEn))
            return;

        stateFlags &= ~StateFlag_SavePreferencesEn;

        #ifdef USES_DISPLAY
            if(OLED.display != nullptr)
                OLED.ScreenModeChange(ExtDisplay::Screen_Saving);
        #endif // USES_DISPLAY
    }
    
    // use selected profile as the default
    profiles.selectedProfile = (uint8_t)profiles.selectedProfile;

#ifdef SAMCO_FLASH_ENABLE
    nvPrefsError = SamcoPreferences::Save(flash);
#else
    nvPrefsError = SamcoPreferences::SaveProfiles();
#endif // SAMCO_FLASH_ENABLE

    if(nvPrefsError == SamcoPreferences::Error_Success) {
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_SaveSuccess);
        #endif // USES_DISPLAY

        Serial.print("Settings saved to ");
        Serial.println(NVRAMlabel);
        SamcoPreferences::SaveToggles();

        if(SamcoPreferences::toggles[OF_Const::customPins])
            SamcoPreferences::SavePins();

        SamcoPreferences::SaveSettings();
        SamcoPreferences::SaveUSBID();

        #ifdef LED_ENABLE
            for(byte i = 0; i < 3; i++) {
                OF_RGB::LedUpdate(25,25,255);
                delay(55);
                OF_RGB::LedOff();
                delay(40);
            }
        #endif // LED_ENABLE
    } else {
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_SaveError);
        #endif // USES_DISPLAY

        Serial.println("Error saving Preferences.");
        if(nvPrefsError != SamcoPreferences::Error_Success) {
            Serial.print(NVRAMlabel);
            Serial.print(" error: ");
        #ifdef SAMCO_FLASH_ENABLE
            Serial.println(SamcoPreferences::ErrorCodeToString(nvPrefsError));
        #else
            Serial.println(nvPrefsError);
        #endif // SAMCO_FLASH_ENABLE
        }

        #ifdef LED_ENABLE
            for(byte i = 0; i < 2; i++) {
                OF_RGB::LedUpdate(255,10,5);
                delay(145);
                OF_RGB::LedOff();
                delay(60);
            }
        #endif // LED_ENABLE
    }

    #ifdef USES_DISPLAY
        if(gunMode == GunMode_Docked)
            OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
        else if(gunMode == GunMode_Pause) {
            OLED.ScreenModeChange(ExtDisplay::Screen_Pause);
            if(SamcoPreferences::toggles[OF_Const::simplePause])
                OLED.PauseListUpdate(ExtDisplay::ScreenPause_Save);
            else OLED.PauseScreenShow(profiles.selectedProfile, profileData[0].name, profileData[1].name, profileData[2].name, profileData[3].name);
        }
    #endif // USES_DISPLAY
}

void FW_Common::UpdateBindings(const bool &lowButtons)
{
    // Updates pins
    LightgunButtons::ButtonDesc[BtnIdx_Trigger].pin = SamcoPreferences::pins[OF_Const::btnTrigger];
    LightgunButtons::ButtonDesc[BtnIdx_A].pin = SamcoPreferences::pins[OF_Const::btnGunA];
    LightgunButtons::ButtonDesc[BtnIdx_B].pin = SamcoPreferences::pins[OF_Const::btnGunB];
    LightgunButtons::ButtonDesc[BtnIdx_Reload].pin = SamcoPreferences::pins[OF_Const::btnGunC];
    LightgunButtons::ButtonDesc[BtnIdx_Start].pin = SamcoPreferences::pins[OF_Const::btnStart];
    LightgunButtons::ButtonDesc[BtnIdx_Select].pin = SamcoPreferences::pins[OF_Const::btnSelect];
    LightgunButtons::ButtonDesc[BtnIdx_Up].pin = SamcoPreferences::pins[OF_Const::btnGunUp];
    LightgunButtons::ButtonDesc[BtnIdx_Down].pin = SamcoPreferences::pins[OF_Const::btnGunDown];
    LightgunButtons::ButtonDesc[BtnIdx_Left].pin = SamcoPreferences::pins[OF_Const::btnGunLeft];
    LightgunButtons::ButtonDesc[BtnIdx_Right].pin = SamcoPreferences::pins[OF_Const::btnGunRight];
    LightgunButtons::ButtonDesc[BtnIdx_Pedal].pin = SamcoPreferences::pins[OF_Const::btnPedal];
    LightgunButtons::ButtonDesc[BtnIdx_Pedal2].pin = SamcoPreferences::pins[OF_Const::btnPedal2];
    LightgunButtons::ButtonDesc[BtnIdx_Pump].pin = SamcoPreferences::pins[OF_Const::btnPump];
    LightgunButtons::ButtonDesc[BtnIdx_Home].pin = SamcoPreferences::pins[OF_Const::btnHome];

    // Updates button functions for low-button mode
    if(lowButtons) {
        LightgunButtons::ButtonDesc[BtnIdx_A].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[BtnIdx_A].reportCode2 = playerStartBtn;
        LightgunButtons::ButtonDesc[BtnIdx_B].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[BtnIdx_B].reportCode2 = playerSelectBtn;
    } else {
        LightgunButtons::ButtonDesc[BtnIdx_A].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[BtnIdx_A].reportCode2 = MOUSE_RIGHT;
        LightgunButtons::ButtonDesc[BtnIdx_B].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[BtnIdx_B].reportCode2 = MOUSE_MIDDLE;
    }

    // update start/select button keyboard bindings
    LightgunButtons::ButtonDesc[BtnIdx_Start].reportCode = playerStartBtn;
    LightgunButtons::ButtonDesc[BtnIdx_Start].reportCode2 = playerStartBtn;
    LightgunButtons::ButtonDesc[BtnIdx_Select].reportCode = playerSelectBtn;
    LightgunButtons::ButtonDesc[BtnIdx_Select].reportCode2 = playerSelectBtn;
}