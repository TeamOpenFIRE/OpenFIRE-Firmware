 /*!
 * @file OpenFIREcommon.h
 * @brief Shared methods used throughout the OpenFIRE project.
 *
 * @copyright That One Seong, 2025
 * @copyright GNU Lesser General Public License
 */ 

#include <Arduino.h>
#include <Wire.h>
#ifdef ARDUINO_ARCH_RP2040
  // for RP2040 Wii Clock Gen
  #include <pico/stdlib.h>
  #include <hardware/clocks.h>
  #include <hardware/pwm.h>
#endif // ARDUINO_ARCH_RP2040

#include "OpenFIREcommon.h"
#include "OpenFIRElights.h"
#include "OpenFIREserial.h"

// button object instance (defined in OpenFIREcommon.h)
LightgunButtons FW_Common::buttons(lgbData, ButtonCount);

void FW_Common::FeedbackSet()
{
    #ifdef USES_RUMBLE
        if(OF_Prefs::pins[OF_Const::rumblePin] >= 0)
            pinMode(OF_Prefs::pins[OF_Const::rumblePin], OUTPUT);
        else OF_Prefs::toggles[OF_Const::rumble] = false;
    #endif // USES_RUMBLE

    #ifdef USES_SOLENOID
        if(OF_Prefs::pins[OF_Const::solenoidPin] >= 0)
            pinMode(OF_Prefs::pins[OF_Const::solenoidPin], OUTPUT);
        else OF_Prefs::toggles[OF_Const::solenoid] = false;
    #endif // USES_SOLENOID

    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(OF_Prefs::pins[OF_Const::rumbleSwitch] >= 0)
                pinMode(OF_Prefs::pins[OF_Const::rumbleSwitch], INPUT_PULLUP);
        #endif // USES_RUMBLE

        #ifdef USES_SOLENOID
            if(OF_Prefs::pins[OF_Const::solenoidSwitch] >= 0)
                pinMode(OF_Prefs::pins[OF_Const::solenoidSwitch], INPUT_PULLUP);
        #endif // USES_SOLENOID

        if(OF_Prefs::pins[OF_Const::autofireSwitch] >= 0)
            pinMode(OF_Prefs::pins[OF_Const::autofireSwitch], INPUT_PULLUP);
    #endif // USES_SWITCHES

    #ifdef USES_ANALOG
        analogReadResolution(12);
        #ifdef USES_TEMP
        if(OF_Prefs::pins[OF_Const::analogX] >= 0 && OF_Prefs::pins[OF_Const::analogY] >= 0 &&
           OF_Prefs::pins[OF_Const::analogX] != OF_Prefs::pins[OF_Const::analogY] &&
           OF_Prefs::pins[OF_Const::analogX] != OF_Prefs::pins[OF_Const::tempPin] &&
           OF_Prefs::pins[OF_Const::analogY] != OF_Prefs::pins[OF_Const::tempPin])
        #else
        if(OF_Prefs::pins[OF_Const::analogX] >= 0 && OF_Prefs::pins[OF_Const::analogY] >= 0 &&
           OF_Prefs::pins[OF_Const::analogX] != OF_Prefs::pins[OF_Const::analogY])
        #endif // USES_TEMP
            //pinMode(analogPinX, INPUT);
            //pinMode(analogPinY, INPUT);
            analogIsValid = true;
        else analogIsValid = false;
    #endif // USES_ANALOG

    #if defined(LED_ENABLE) && defined(FOURPIN_LED)
    if(OF_Prefs::pins[OF_Const::ledR] < 0 || OF_Prefs::pins[OF_Const::ledG] < 0 || OF_Prefs::pins[OF_Const::ledB] < 0)
        ledIsValid = false;
    else {
        pinMode(OF_Prefs::pins[OF_Const::ledR], OUTPUT);
        pinMode(OF_Prefs::pins[OF_Const::ledG], OUTPUT);
        pinMode(OF_Prefs::pins[OF_Const::ledB], OUTPUT);
        ledIsValid = true;
    }
    #endif // FOURPIN_LED

    #ifdef CUSTOM_NEOPIXEL
    if(OF_Prefs::pins[OF_Const::neoPixel] >= 0)
        OF_RGB::InitExternPixel(OF_Prefs::pins[OF_Const::neoPixel]);
    #endif // CUSTOM_NEOPIXEL

    if(OF_Prefs::pins[OF_Const::periphSCL] >= 0 && OF_Prefs::pins[OF_Const::periphSDA] >= 0 &&
       bitRead(OF_Prefs::pins[OF_Const::camSCL], 1) != bitRead(OF_Prefs::pins[OF_Const::periphSCL], 1) &&
       bitRead(OF_Prefs::pins[OF_Const::camSDA], 1) != bitRead(OF_Prefs::pins[OF_Const::periphSDA], 1)) {
    #ifdef USES_DISPLAY
        // wrapper will manage display validity
        // check it's not using the camera's I2C line
        if(OF_Prefs::i2cPeriphs[OF_Const::i2cOLED]) {
            if(!OLED.Begin()) { if(OLED.display != nullptr) delete OLED.display; }
        }
    #endif // USES_DISPLAY
    }
}

void FW_Common::PinsReset()
{
    if(dfrIRPos != nullptr) {
        delete dfrIRPos;
        dfrIRPos = nullptr;
    }

    #ifdef USES_RUMBLE
        if(OF_Prefs::pins[OF_Const::rumblePin] >= 0)
            pinMode(OF_Prefs::pins[OF_Const::rumblePin], INPUT);
    #endif // USES_RUMBLE

    #ifdef USES_SOLENOID
        if(OF_Prefs::pins[OF_Const::solenoidPin] >= 0)
            pinMode(OF_Prefs::pins[OF_Const::solenoidPin], INPUT);
    #endif // USES_SOLENOID

    #ifdef USES_SWITCHES
        #ifdef USES_RUMBLE
            if(OF_Prefs::pins[OF_Const::rumbleSwitch] >= 0)
                pinMode(OF_Prefs::pins[OF_Const::rumbleSwitch], INPUT);
        #endif // USES_RUMBLE

        #ifdef USES_SOLENOID
            if(OF_Prefs::pins[OF_Const::solenoidSwitch] >= 0)
                pinMode(OF_Prefs::pins[OF_Const::solenoidSwitch], INPUT);
        #endif // USES_SOLENOID

        if(OF_Prefs::pins[OF_Const::autofireSwitch] >= 0)
            pinMode(OF_Prefs::pins[OF_Const::autofireSwitch], INPUT);
    #endif // USES_SWITCHES

    #ifdef LED_ENABLE
        OF_RGB::LedOff();

        #ifdef FOURPIN_LED
            if(ledIsValid) {
                pinMode(OF_Prefs::pins[OF_Const::ledR], INPUT);
                pinMode(OF_Prefs::pins[OF_Const::ledG], INPUT);
                pinMode(OF_Prefs::pins[OF_Const::ledB], INPUT);
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
    if(bitRead(OF_Prefs::pins[OF_Const::camSCL], 1) && bitRead(OF_Prefs::pins[OF_Const::camSDA], 1)) {
        // I2C1
        if(bitRead(OF_Prefs::pins[OF_Const::camSCL], 0) && !bitRead(OF_Prefs::pins[OF_Const::camSDA], 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire1.setSDA(OF_Prefs::pins[OF_Const::camSDA]);
            Wire1.setSCL(OF_Prefs::pins[OF_Const::camSCL]);
            dfrIRPos = new DFRobotIRPositionEx(Wire1);
        }
        
    } else if(!bitRead(OF_Prefs::pins[OF_Const::camSCL], 1) && !bitRead(OF_Prefs::pins[OF_Const::camSDA], 1)) {
        // I2C0
        if(bitRead(OF_Prefs::pins[OF_Const::camSCL], 0) && !bitRead(OF_Prefs::pins[OF_Const::camSDA], 0)) {
            // SDA/SCL are indeed on verified correct pins
            Wire.setSDA(OF_Prefs::pins[OF_Const::camSDA]);
            Wire.setSCL(OF_Prefs::pins[OF_Const::camSCL]);
            dfrIRPos = new DFRobotIRPositionEx(Wire);
        }
    }

    #ifdef ARDUINO_ARCH_RP2040
    if(OF_Prefs::pins[OF_Const::wiiClockGen] > -1) {
        set_sys_clock_khz(125000, true);
        gpio_set_function(OF_Prefs::pins[OF_Const::wiiClockGen], GPIO_FUNC_PWM);
        uint slice_num = pwm_gpio_to_slice_num(OF_Prefs::pins[OF_Const::wiiClockGen]);
        pwm_set_clkdiv(slice_num, 1.0f);  // for sys_clock = 125MHz
        //pwm_set_clkdiv(slice_num, 3.0f); // for sys_clock = 150MHz
        pwm_set_wrap(slice_num, 4);
        pwm_set_chan_level(slice_num, PWM_CHAN_A, 2);
        pwm_set_enabled(slice_num, true);
    } else {
        set_sys_clock_khz(133000, true);
        pwm_set_enabled(pwm_gpio_to_slice_num(OF_Prefs::pins[OF_Const::wiiClockGen]), false);
        gpio_set_function(OF_Prefs::pins[OF_Const::wiiClockGen], GPIO_FUNC_SIO);
        gpio_put(OF_Prefs::pins[OF_Const::wiiClockGen], 0);
        gpio_set_dir(OF_Prefs::pins[OF_Const::wiiClockGen], GPIO_IN);
    }
    #endif // ARDUINO_ARCH_RP2040

    // Start IR Camera with basic data format
    if(dfrIRPos != nullptr) {
        if(!dfrIRPos->begin(DFROBOT_IR_IIC_CLOCK, DFRobotIRPositionEx::DataFormat_Basic, irSensitivity)) {
            delete dfrIRPos;
            dfrIRPos = nullptr;
            PrintIrError();
        } else camNotAvailable = false;
    } else PrintIrError();
}

void FW_Common::SetMode(const FW_Const::GunMode_e &newMode)
{
    if(gunMode == newMode)
        return;
    
    // exit current mode
    switch(gunMode) {
    case FW_Const::GunMode_Run:
        stateFlags |= FW_Const::StateFlag_PrintPreferences;
        break;
    case FW_Const::GunMode_Pause:
        break;
    case FW_Const::GunMode_Docked:
        if(newMode != FW_Const::GunMode_Calibration)
            Serial.println("Undocking.");
        break;
    }
    
    // enter new mode
    gunMode = newMode;
    switch(newMode) {
    case FW_Const::GunMode_Run:
        // begin run mode with all 4 points seen
        lastSeen = 0x0F;

        #ifdef USES_DISPLAY
            if(OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both)
                OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Dual);
            else if(OF_Serial::serialMode)
                OLED.ScreenModeChange(ExtDisplay::Screen_Mamehook_Single, buttons.analogOutput);
            else OLED.ScreenModeChange(ExtDisplay::Screen_Normal, buttons.analogOutput);

            OLED.TopPanelUpdate("Prof: ", OF_Prefs::profiles[OF_Prefs::currentProfile].name);
        #endif // USES_DISPLAY

        break;
    case FW_Const::GunMode_Calibration:
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_Calibrating);
            OLED.TopPanelUpdate("Cali: ", OF_Prefs::profiles[OF_Prefs::currentProfile].name);
        #endif // USES_DISPLAY
        break;
    case FW_Const::GunMode_Pause:
        stateFlags |= FW_Const::StateFlag_SavePreferencesEn | FW_Const::StateFlag_PrintSelectedProfile;

        #ifdef USES_DISPLAY
          OLED.ScreenModeChange(ExtDisplay::Screen_Pause);
          OLED.TopPanelUpdate("Using ", OF_Prefs::profiles[OF_Prefs::currentProfile].name);

          if(OF_Prefs::toggles[OF_Const::simplePause]) 
              OLED.PauseListUpdate(pauseModeSelection);
          else OLED.PauseScreenShow(OF_Prefs::currentProfile, OF_Prefs::profiles[0].name, OF_Prefs::profiles[1].name, OF_Prefs::profiles[2].name, OF_Prefs::profiles[3].name);
        #endif // USES_DISPLAY

        break;
    case FW_Const::GunMode_Docked:
        stateFlags |= FW_Const::StateFlag_SavePreferencesEn;

        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
        #endif // USES_DISPLAY

        break;
    }

    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE
}

void FW_Common::SetRunMode(const FW_Const::RunMode_e &newMode)
{
    if(newMode >= FW_Const::RunMode_Count)
        return;

    // block Processing/test modes being applied to a profile
    if(newMode <= FW_Const::RunMode_ProfileMax && OF_Prefs::profiles[OF_Prefs::currentProfile].runMode != newMode) {
        OF_Prefs::profiles[OF_Prefs::currentProfile].runMode = newMode;
        stateFlags |= FW_Const::StateFlag_SavePreferencesEn;
    }
    
    if(runMode != newMode) {
        runMode = newMode;
        //if(!(stateFlags & FW_Const::StateFlag_PrintSelectedProfile))
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
    int _topOffset = OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset;
    int _bottomOffset = OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset;
    int _leftOffset = OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset;
    int _rightOffset = OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset;
    float _TLled = OF_Prefs::profiles[OF_Prefs::currentProfile].TLled;
    float _TRled = OF_Prefs::profiles[OF_Prefs::currentProfile].TRled;
    float _adjX = OF_Prefs::profiles[OF_Prefs::currentProfile].adjX;
    float _adjY = OF_Prefs::profiles[OF_Prefs::currentProfile].adjY;

    // set current values to factory defaults
    OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset = 0;
    OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset = 0;
    OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset = 0;
    OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset = 0;

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
    SetMode(FW_Const::GunMode_Calibration);
    Serial.printf("%c%c", OF_Const::sCaliStageUpd, FW_Const::Cali_Init);

    while(gunMode == FW_Const::GunMode_Calibration) {
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
        if((buttons.pressedReleased & (FW_Const::ExitPauseModeBtnMask | FW_Const::ExitPauseModeHoldBtnMask) || Serial.read() == OF_Const::serialTerminator) && !justBooted) {
            Serial.printf("%c%c", OF_Const::sCaliStageUpd, FW_Const::Cali_Verify+1);

            // Reapplying backed up data
            OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset = _topOffset;
            OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset = _bottomOffset;
            OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset = _leftOffset;
            OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset = _rightOffset;
            OF_Prefs::profiles[OF_Prefs::currentProfile].TLled = _TLled;
            OF_Prefs::profiles[OF_Prefs::currentProfile].TRled = _TRled;
            OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = _adjX;
            OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = _adjY;

            // Re-print the profile
            stateFlags |= FW_Const::StateFlag_PrintSelectedProfile;

            // Exit back to docked mode or run mode, depending on if pinged from Desktop App
            if(fromDesktop)
                SetMode(FW_Const::GunMode_Docked);
            else SetMode(FW_Const::GunMode_Run);

            return;
        } else if(buttons.pressed == FW_Const::BtnMask_Trigger && !mouseMoving) {
            Serial.printf("%c%c", OF_Const::sCaliStageUpd, ++calStage);

            // Ensure our messages go through, or else the HID reports eat UART.
            Serial.flush();

            switch(calStage) {
                case FW_Const::Cali_Init:
                    // Initial state, nothing to do (but center cursor for desktop use)
                    if(fromDesktop)
                        AbsMouse5.move(32768/2, 32768/2);
                    break;
                case FW_Const::Cali_Top:
                    // Reset Offsets
                    topOffset = 0;
                    bottomOffset = 0;
                    leftOffset = 0;
                    rightOffset = 0;

                    // Set Cam center offsets
                    if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout) {
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = (OpenFIREdiamond.testMedianX() - (512 << 2)) * cos(OpenFIREdiamond.Ang()) -
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * sin(OpenFIREdiamond.Ang()) + (512 << 2);
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = (OpenFIREdiamond.testMedianX() - (512 << 2)) * sin(OpenFIREdiamond.Ang()) +
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * cos(OpenFIREdiamond.Ang()) + (384 << 2);
                    } else {
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = (OpenFIREsquare.testMedianX() - (512 << 2)) * cos(OpenFIREsquare.Ang()) -
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * sin(OpenFIREsquare.Ang()) + (512 << 2);
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = (OpenFIREsquare.testMedianX() - (512 << 2)) * sin(OpenFIREsquare.Ang()) +
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * cos(OpenFIREsquare.Ang()) + (384 << 2);
                        // Work out LED locations by assuming height is 100%
                        OF_Prefs::profiles[OF_Prefs::currentProfile].TLled = (res_x / 2) - ((OpenFIREsquare.W() * (res_y  / OpenFIREsquare.H())) / 2);
                        OF_Prefs::profiles[OF_Prefs::currentProfile].TRled = (res_x / 2) + ((OpenFIREsquare.W() * (res_y  / OpenFIREsquare.H())) / 2);
                    }

                    // Update Cam centre in perspective library
                    OpenFIREper.source(OF_Prefs::profiles[OF_Prefs::currentProfile].adjX, OF_Prefs::profiles[OF_Prefs::currentProfile].adjY);
                    OpenFIREper.deinit(0);

                    // Set mouse movement to top position
                    if(!fromDesktop) {
                        mouseTargetX = 32768 / 2;
                        mouseTargetY = 0;
                        mouseMoving = true;
                    }
                    break;
                case FW_Const::Cali_Bottom:
                    // Set Offset buffer
                    topOffset = mouseY;
                    {
                      char buf[6];
                      buf[0] = OF_Const::sCaliInfoUpd;
                      buf[1] = 1;
                      memcpy(&buf[2], &topOffset, sizeof(int));
                      Serial.write(buf, sizeof(buf));
                      Serial.flush();
                    }

                    // Set mouse movement to bottom position
                    if(!fromDesktop) {
                        mouseTargetX = 32768 / 2;
                        mouseTargetY = 32767;
                        mouseMoving = true;
                    }
                    break;
                case FW_Const::Cali_Left:
                    // Set Offset buffer
                    bottomOffset = (res_y - mouseY);
                    {
                      char buf[6];
                      buf[0] = OF_Const::sCaliInfoUpd;
                      buf[1] = 2;
                      memcpy(&buf[2], &bottomOffset, sizeof(int));
                      Serial.write(buf, sizeof(buf));
                      Serial.flush();
                    }

                    // Set mouse movement to left position
                    if(!fromDesktop) {
                        mouseTargetX = 0;
                        mouseTargetY = 32768 / 2;
                        mouseMoving = true;
                    }
                    break;
                case FW_Const::Cali_Right:
                    // Set Offset buffer
                    leftOffset = mouseX;
                    {
                      char buf[6];
                      buf[0] = OF_Const::sCaliInfoUpd;
                      buf[1] = 3;
                      memcpy(&buf[2], &leftOffset, sizeof(int));
                      Serial.write(buf, sizeof(buf));
                      Serial.flush();
                    }

                    // Set mouse movement to right position
                    if(!fromDesktop) {
                        mouseTargetX = 32767;
                        mouseTargetY = 32768 / 2;
                        mouseMoving = true;
                    }
                    break;
                case FW_Const::Cali_Center:
                    // Set Offset buffer
                    rightOffset = (res_x - mouseX);
                    {
                      char buf[6];
                      buf[0] = OF_Const::sCaliInfoUpd;
                      buf[1] = 4;
                      memcpy(&buf[2], &rightOffset, sizeof(int));
                      Serial.write(buf, sizeof(buf));
                      Serial.flush();
                    }

                    // Save Offset buffer to profile
                    OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset = topOffset;
                    OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset = bottomOffset;
                    OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset = leftOffset;
                    OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset = rightOffset;

                    // Move back to center calibration point
                    if(!fromDesktop) {
                        mouseTargetX = 32768 / 2;
                        mouseTargetY = 32768 / 2;
                        mouseMoving = true;
                    }
                    break;
                case FW_Const::Cali_Verify:
                    // Apply new Cam center offsets with Offsets applied
                    if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout) {
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = (OpenFIREdiamond.testMedianX() - (512 << 2)) * cos(OpenFIREdiamond.Ang()) -
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * sin(OpenFIREdiamond.Ang()) + (512 << 2);
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = (OpenFIREdiamond.testMedianX() - (512 << 2)) * sin(OpenFIREdiamond.Ang()) +
                                                                     (OpenFIREdiamond.testMedianY() - (384 << 2)) * cos(OpenFIREdiamond.Ang()) + (384 << 2);
                    } else {
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = (OpenFIREsquare.testMedianX() - (512 << 2)) * cos(OpenFIREsquare.Ang()) -
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * sin(OpenFIREsquare.Ang()) + (512 << 2);
                        OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = (OpenFIREsquare.testMedianX() - (512 << 2)) * sin(OpenFIREsquare.Ang()) +
                                                                     (OpenFIREsquare.testMedianY() - (384 << 2)) * cos(OpenFIREsquare.Ang()) + (384 << 2);
                    }

                    {
                      char buf[6];
                      buf[0] = OF_Const::sCaliInfoUpd;
                      buf[1] = 5;
                      memcpy(&buf[2], &OF_Prefs::profiles[OF_Prefs::currentProfile].TLled, sizeof(float));
                      Serial.write(buf, sizeof(buf));
                    }
                    {
                      char buf[6];
                      buf[0] = OF_Const::sCaliInfoUpd;
                      buf[1] = 6;
                      memcpy(&buf[2], &OF_Prefs::profiles[OF_Prefs::currentProfile].TRled, sizeof(float));
                      Serial.write(buf, sizeof(buf));
                    }
                    Serial.flush();

                    // Update Cam centre in perspective library
                    OpenFIREper.source(OF_Prefs::profiles[OF_Prefs::currentProfile].adjX, OF_Prefs::profiles[OF_Prefs::currentProfile].adjY);
                    OpenFIREper.deinit(0);

                    // Let the user test.
                    SetMode(FW_Const::GunMode_Verification);
                    while(gunMode == FW_Const::GunMode_Verification) {
                        buttons.Poll();

                        if(irPosUpdateTick) {
                            irPosUpdateTick = 0;
                            GetPosition();
                        }

                        // If it's good, move onto calibration finish.
                        if(buttons.pressed == FW_Const::BtnMask_Trigger) {
                            calStage++;
                            // Stay in Verification Mode; the code outside of the calibration loop will catch us.
                            break;
                        // Press A/B to restart calibration for current profile
                        } else if(buttons.pressedReleased & FW_Const::ExitPauseModeHoldBtnMask) {
                            calStage = 0;
                            Serial.printf("%c%c", OF_Const::sCaliStageUpd, FW_Const::Cali_Init);
                            Serial.flush();

                            // (Re)set current values to factory defaults
                            OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset = 0;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset = 0;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset = 0;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset = 0;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = 512 << 2;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = 384 << 2;
                            SetMode(FW_Const::GunMode_Calibration);
                            AbsMouse5.move(32768/2, 32768/2);
                        // Press C/Home to exit without committing new calibration values
                        } else if(buttons.pressedReleased & FW_Const::ExitPauseModeBtnMask && !justBooted) {
                            Serial.printf("%c%c", OF_Const::sCaliStageUpd, FW_Const::Cali_Verify+1);
                            Serial.flush();

                            // Reapply backed-up data
                            OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset = _topOffset;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset = _bottomOffset;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset = _leftOffset;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset = _rightOffset;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].TLled = _TLled;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].TRled = _TRled;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].adjX = _adjX;
                            OF_Prefs::profiles[OF_Prefs::currentProfile].adjY = _adjY;

                            // Re-print the profile
                            stateFlags |= FW_Const::StateFlag_PrintSelectedProfile;

                            // Re-apply the calibration stored in the profile
                            if(fromDesktop)
                                SetMode(FW_Const::GunMode_Docked);
                            else SetMode(FW_Const::GunMode_Run);
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
        stateFlags |= FW_Const::StateFlag_SavePreferencesEn;
        SavePreferences();
        if(fromDesktop)
            SetMode(FW_Const::GunMode_Docked);
    } else if(fromDesktop) {
        SetMode(FW_Const::GunMode_Docked);
    } else SetMode(FW_Const::GunMode_Run);

    #ifdef USES_RUMBLE
        if(OF_Prefs::toggles[OF_Const::rumble]) {
            analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]);
            delay(80);
            digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
            delay(50);
            analogWrite(OF_Prefs::pins[OF_Const::rumblePin], OF_Prefs::settings[OF_Const::rumbleStrength]);
            delay(125);
            digitalWrite(OF_Prefs::pins[OF_Const::rumblePin], LOW);
        }
    #endif // USES_RUMBLE

    Serial.printf("%c%c", OF_Const::sCaliStageUpd, FW_Const::Cali_Verify+1);
    Serial.flush();
}

void FW_Common::GetPosition()
{
    if(dfrIRPos != nullptr) {
        int error = dfrIRPos->basicAtomic(DFRobotIRPositionEx::Retry_2);
        if(error == DFRobotIRPositionEx::Error_Success) {
            // if diamond layout, or square
            if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout) {
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
                                OF_Prefs::profiles[OF_Prefs::currentProfile].TLled, 0,
                                OF_Prefs::profiles[OF_Prefs::currentProfile].TRled, 0,
                                OF_Prefs::profiles[OF_Prefs::currentProfile].TLled, res_y,
                                OF_Prefs::profiles[OF_Prefs::currentProfile].TRled, res_y);
            }

            // Output mapped to screen resolution because offsets are measured in pixels
            mouseX = map(OpenFIREper.getX(), 0, res_x, (0 - OF_Prefs::profiles[OF_Prefs::currentProfile].leftOffset), (res_x + OF_Prefs::profiles[OF_Prefs::currentProfile].rightOffset));                 
            mouseY = map(OpenFIREper.getY(), 0, res_y, (0 - OF_Prefs::profiles[OF_Prefs::currentProfile].topOffset), (res_y + OF_Prefs::profiles[OF_Prefs::currentProfile].bottomOffset));

            switch(runMode) {
                case FW_Const::RunMode_Average:
                    // 2 position moving average
                    moveIndex ^= 1;
                    moveXAxisArr[moveIndex] = mouseX;
                    moveYAxisArr[moveIndex] = mouseY;
                    mouseX = (moveXAxisArr[0] + moveXAxisArr[1]) / 2;
                    mouseY = (moveYAxisArr[0] + moveYAxisArr[1]) / 2;
                    break;
                case FW_Const::RunMode_Average2:
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

            if(gunMode == FW_Const::GunMode_Run) {
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

            } else if(gunMode == FW_Const::GunMode_Verification) {
                AbsMouse5.move(conMoveX, conMoveY);
            } else {
                if(millis() - testLastStamp > 50) {
                    testLastStamp = millis();
                    // RAW Camera Output mapped to screen res (1920x1080)
                    int rawX[4];
                    int rawY[4];
                    // RAW Output for viewing in processing sketch mapped to 1920x1080 screen resolution
                    for (int i = 0; i < 4; i++) {
                        if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout) {
                            rawX[i] = map(OpenFIREdiamond.X(i), 0, 1023 << 2, 1920, 0);
                            rawY[i] = map(OpenFIREdiamond.Y(i), 0, 768 << 2, 0, 1080);
                        } else {
                            rawX[i] = map(OpenFIREsquare.X(i), 0, 1023 << 2, 0, 1920);
                            rawY[i] = map(OpenFIREsquare.Y(i), 0, 768 << 2, 0, 1080);
                        }
                    }

                    if(runMode == FW_Const::RunMode_Processing) {
                        int mouseXscaled = mouseX / 4;
                        int mouseYscaled = mouseY / 4;

                        if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout) {
                            int testMedianX = map(OpenFIREdiamond.testMedianX(), 0, 1023 << 2, 1920, 0);
                            int testMedianY = map(OpenFIREdiamond.testMedianY(), 0, 768 << 2, 0, 1080);
                            char buf[49];
                            buf[0] = OF_Const::sTestCoords;
                            memcpy(&buf[1],  &rawX[0],      sizeof(int));
                            memcpy(&buf[5],  &rawY[0],      sizeof(int));
                            memcpy(&buf[9],  &rawX[1],      sizeof(int));
                            memcpy(&buf[13], &rawY[1],      sizeof(int));
                            memcpy(&buf[17], &rawX[2],      sizeof(int));
                            memcpy(&buf[21], &rawY[2],      sizeof(int));
                            memcpy(&buf[25], &rawX[3],      sizeof(int));
                            memcpy(&buf[29], &rawY[3],      sizeof(int));
                            memcpy(&buf[33], &mouseXscaled, sizeof(int));
                            memcpy(&buf[37], &mouseYscaled, sizeof(int));
                            memcpy(&buf[41], &testMedianX,  sizeof(int));
                            memcpy(&buf[45], &testMedianY,  sizeof(int));
                            Serial.write(buf, sizeof(buf));
                        } else {
                            int testMedianX = map(OpenFIREsquare.testMedianX(), 0, 1023 << 2, 0, 1920);
                            int testMedianY = map(OpenFIREsquare.testMedianY(), 0, 768 << 2, 0, 1080);
                            char buf[49];
                            buf[0] = OF_Const::sTestCoords;
                            memcpy(&buf[1],  &rawX[0],      sizeof(int));
                            memcpy(&buf[5],  &rawY[0],      sizeof(int));
                            memcpy(&buf[9],  &rawX[1],      sizeof(int));
                            memcpy(&buf[13], &rawY[1],      sizeof(int));
                            memcpy(&buf[17], &rawX[2],      sizeof(int));
                            memcpy(&buf[21], &rawY[2],      sizeof(int));
                            memcpy(&buf[25], &rawX[3],      sizeof(int));
                            memcpy(&buf[29], &rawY[3],      sizeof(int));
                            memcpy(&buf[33], &mouseXscaled, sizeof(int));
                            memcpy(&buf[37], &mouseYscaled, sizeof(int));
                            memcpy(&buf[41], &testMedianX,  sizeof(int));
                            memcpy(&buf[45], &testMedianY,  sizeof(int));
                            Serial.write(buf, sizeof(buf));
                        }
                    }

                    #ifdef USES_DISPLAY
                        OLED.DrawVisibleIR(rawX, rawY);
                    #endif // USES_DISPLAY
                }
            }
        } else if(error != DFRobotIRPositionEx::Error_DataMismatch)
            PrintIrError();
    } else  PrintIrError();
}

void FW_Common::PrintIrError()
{
    // set flag to warn desktop app when docking
    if(!camNotAvailable)
        camNotAvailable = true;

    if(dockedSaving) {
        char buf[2] = { OF_Const::sError, OF_Const::sErrCam };
        Serial.write(buf, 2);
    } else if(millis() - camWarningTimestamp > CAM_WARNING_INTERVAL) {
        Serial.println("CAMERROR: Not available");
        camWarningTimestamp = millis();
    }
}

void FW_Common::UpdateLastSeen()
{
    if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout) {
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

    if(OF_Prefs::currentProfile != profile) {
        stateFlags |= FW_Const::StateFlag_PrintSelectedProfile;
        OF_Prefs::currentProfile = profile;
    }

    OpenFIREper.source(OF_Prefs::profiles[OF_Prefs::currentProfile].adjX, OF_Prefs::profiles[OF_Prefs::currentProfile].adjY);                                                          
    OpenFIREper.deinit(0);

    // set IR sensitivity
    if(OF_Prefs::profiles[profile].irSens <= DFRobotIRPositionEx::Sensitivity_Max)
        SetIrSensitivity(OF_Prefs::profiles[profile].irSens);

    // set run mode
    if(OF_Prefs::profiles[profile].runMode < FW_Const::RunMode_Count)
        SetRunMode((FW_Const::RunMode_e)OF_Prefs::profiles[profile].runMode);

    #ifdef USES_DISPLAY
        if(gunMode != FW_Const::GunMode_Docked)
            OLED.TopPanelUpdate("Using ", OF_Prefs::profiles[OF_Prefs::currentProfile].name);
    #endif // USES_DISPLAY
 
    #ifdef LED_ENABLE
        SetLedColorFromMode();
    #endif // LED_ENABLE

    // enable save to allow setting new default profile
    stateFlags |= FW_Const::StateFlag_SavePreferencesEn;
    return true;
}

#ifdef LED_ENABLE
void FW_Common::SetLedColorFromMode()
{
    switch(gunMode) {
    case FW_Const::GunMode_Calibration:
        OF_RGB::SetLedPackedColor(OF_RGB::CalModeColor);
        break;
    case FW_Const::GunMode_Pause:
        OF_RGB::SetLedPackedColor(OF_Prefs::profiles[OF_Prefs::currentProfile].color);
        break;
    case FW_Const::GunMode_Run:
        if(lastSeen)
             OF_RGB::LedOff();
        else OF_RGB::SetLedPackedColor(OF_RGB::IRSeen0Color);
        break;
    default:
        break;
    }
}
#endif // LED_ENABLE

#ifdef USES_DISPLAY
void FW_Common::RedrawDisplay()
{
    if(gunMode == FW_Const::GunMode_Docked)
        OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
    else if(gunMode == FW_Const::GunMode_Pause) {
        OLED.ScreenModeChange(ExtDisplay::Screen_Pause);
        if(OF_Prefs::toggles[OF_Const::simplePause])
            OLED.PauseListUpdate(ExtDisplay::ScreenPause_Save);
        else OLED.PauseScreenShow(OF_Prefs::currentProfile,
                                  OF_Prefs::profiles[0].name,
                                  OF_Prefs::profiles[1].name,
                                  OF_Prefs::profiles[2].name,
                                  OF_Prefs::profiles[3].name);
    }
}
#endif // USES_DISPLAY

void FW_Common::SetIrSensitivity(const uint8_t &sensitivity)
{
    if(sensitivity > DFRobotIRPositionEx::Sensitivity_Max)
        return;

    if(OF_Prefs::profiles[OF_Prefs::currentProfile].irSens != sensitivity) {
        OF_Prefs::profiles[OF_Prefs::currentProfile].irSens = sensitivity;
        stateFlags |= FW_Const::StateFlag_SavePreferencesEn;
    }

    if(irSensitivity != (DFRobotIRPositionEx::Sensitivity_e)sensitivity) {
        irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)sensitivity;
        dfrIRPos->sensitivityLevel(irSensitivity);
        //if(!(stateFlags & FW_Const::StateFlag_PrintSelectedProfile))
            //PrintIrSensitivity();
    }
}

void FW_Common::SetIrLayout(const uint8_t &layout)
{
    // TODO: we need an enum for layout types available
    if(layout > 1)
        return;

    if(OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout != layout) {
        OF_Prefs::profiles[OF_Prefs::currentProfile].irLayout = layout;
        stateFlags |= FW_Const::StateFlag_SavePreferencesEn;
    }
}

int FW_Common::SavePreferences()
{
    // Unless the user's Docked,
    // Only allow one write per pause state until something changes.
    // Extra protection to ensure the same data can't write a bunch of times.
    if(gunMode != FW_Const::GunMode_Docked) {
        if(!(stateFlags & FW_Const::StateFlag_SavePreferencesEn))
            return OF_Prefs::Error_Success;

        stateFlags &= ~FW_Const::StateFlag_SavePreferencesEn;

        #ifdef USES_DISPLAY
            if(OLED.display != nullptr)
                OLED.ScreenModeChange(ExtDisplay::Screen_Saving);
        #endif // USES_DISPLAY
    }

    if(OF_Prefs::SaveProfiles() == OF_Prefs::Error_Success) {
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_SaveSuccess);
        #endif // USES_DISPLAY

        if(gunMode != FW_Const::GunMode_Docked) Serial.println("Settings saved to Flash"), Serial.flush();
        else Serial.printf("%c%c (Successfully saved to LittleFS Storage)", OF_Const::sSave, true), Serial.flush();
        OF_Prefs::SaveToggles();

        if(OF_Prefs::toggles[OF_Const::customPins])
            OF_Prefs::SavePins();

        OF_Prefs::SaveSettings();
        OF_Prefs::SavePeriphs();
        OF_Prefs::SaveUSBID();

        #ifdef LED_ENABLE
            for(uint i = 0; i < 3; ++i) {
                OF_RGB::LedUpdate(25,25,255);
                delay(55);
                OF_RGB::LedOff();
                delay(40);
            }
        #endif // LED_ENABLE

        #ifdef USES_DISPLAY
            RedrawDisplay();
        #endif // USES_DISPLAY

        return OF_Prefs::Error_Success;
    } else {
        #ifdef USES_DISPLAY
            OLED.ScreenModeChange(ExtDisplay::Screen_SaveError);
        #endif // USES_DISPLAY

        // TODO: reimpl a detailed error string
        if(gunMode != FW_Const::GunMode_Docked) Serial.println("Error saving Preferences to Flash.");
        else Serial.printf("%c%c (Failed to save to LittleFS Storage)", OF_Const::sSave, false), Serial.flush();

        /*
        if(nvPrefsError != OF_Prefs::Error_Success) {
            Serial.print(NVRAMlabel);
            Serial.print(" error: ");
        #ifdef SAMCO_FLASH_ENABLE
            Serial.println(OF_Prefs::ErrorCodeToString(nvPrefsError));
        #else
            Serial.println(nvPrefsError);
        #endif // SAMCO_FLASH_ENABLE
        }*/

        #ifdef LED_ENABLE
            for(uint i = 0; i < 2; ++i) {
                OF_RGB::LedUpdate(255,10,5);
                delay(145);
                OF_RGB::LedOff();
                delay(60);
            }
        #endif // LED_ENABLE

        #ifdef USES_DISPLAY
            RedrawDisplay();
        #endif // USES_DISPLAY

        return OF_Prefs::Error_Write;
    }
}

void FW_Common::UpdateBindings(const bool &lowButtons)
{
    if(gunMode != FW_Const::GunMode_Run) {
        // Updates pins
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].pin = OF_Prefs::pins[OF_Const::btnTrigger];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].pin       = OF_Prefs::pins[OF_Const::btnGunA];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].pin       = OF_Prefs::pins[OF_Const::btnGunB];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Reload].pin  = OF_Prefs::pins[OF_Const::btnGunC];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].pin   = OF_Prefs::pins[OF_Const::btnStart];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Select].pin  = OF_Prefs::pins[OF_Const::btnSelect];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Up].pin      = OF_Prefs::pins[OF_Const::btnGunUp];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Down].pin    = OF_Prefs::pins[OF_Const::btnGunDown];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Left].pin    = OF_Prefs::pins[OF_Const::btnGunLeft];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Right].pin   = OF_Prefs::pins[OF_Const::btnGunRight];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal].pin   = OF_Prefs::pins[OF_Const::btnPedal];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pedal2].pin  = OF_Prefs::pins[OF_Const::btnPedal2];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Pump].pin    = OF_Prefs::pins[OF_Const::btnPump];
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Home].pin    = OF_Prefs::pins[OF_Const::btnHome];
    }

    // Updates button functions for low-button mode
    if(lowButtons) {
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode2 = playerStartBtn;
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportType2 = LightgunButtons::ReportType_Keyboard;
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportCode2 = playerSelectBtn;
    } else { // TODO: we should just reload btn config table from flash instead
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_A].reportCode2 = MOUSE_RIGHT;
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportType2 = LightgunButtons::ReportType_Mouse;
        LightgunButtons::ButtonDesc[FW_Const::BtnIdx_B].reportCode2 = MOUSE_MIDDLE;
    }

    // update start/select button keyboard bindings
    LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].reportCode   = playerStartBtn;
    LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Start].reportCode2  = playerStartBtn;
    LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Select].reportCode  = playerSelectBtn;
    LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Select].reportCode2 = playerSelectBtn;

    LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportType2 = LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportType;
    LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportCode2 = LightgunButtons::ButtonDesc[FW_Const::BtnIdx_Trigger].reportCode;
}
