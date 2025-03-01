/*!
 * @file SamcoEnhanced.ino
 * @brief OpenFIRE - 4IR LED Lightgun sketch w/ support for force feedback and other features.
 * Forked from IR-GUN4ALL v4.2, which is based on Prow's Enhanced Fork from https://github.com/Prow7/ir-light-gun,
 * which in itself is based on the 4IR Beta "Big Code Update" SAMCO project from https://github.com/samuelballantyne/IR-Light-Gun
 *
 * @copyright Samco, https://github.com/samuelballantyne, June 2020
 * @copyright Mike Lynch, July 2021
 * @copyright That One Seong, https://github.com/SeongGino, 2024
 * @copyright GNU Lesser General Public License
 *
 * @author [Sam Ballantyne](samuelballantyne@hotmail.com)
 * @author Mike Lynch
 * @author [That One Seong](SeongsSeongs@gmail.com)
 * @date 2025
 */

#include "SamcoEnhanced.h"
#include "boards/OpenFIREshared.h"
#include "SamcoColours.h"
#include "OpenFIRElights.h"
#include "OpenFIREserial.h"
#include "OpenFIREFeedback.h"
#include "SamcoPreferences.h"

// Sets up the environment
void setup() {
    // initialize EEPROM device. Arduino AVR has a 1k flash, so use that.
    EEPROM.begin(1024);

    #ifdef ARDUINO_ADAFRUIT_ITSYBITSY_RP2040
        // SAMCO 1.1 needs Pin 5 normally HIGH for the camera
        pinMode(14, OUTPUT);
        digitalWrite(14, HIGH);
    #endif // ARDUINO_ADAFRUIT_ITSYBITSY_RP2040

    SamcoPreferences::LoadPresets();
    
    if(FW_Common::nvAvailable) {
        FW_Common::LoadPreferences();

        if(FW_Common::nvPrefsError == SamcoPreferences::Error_NoData) {
            SamcoPreferences::ResetPreferences();
        } else if(FW_Common::nvPrefsError == SamcoPreferences::Error_Success) {
            // use values from preferences
            // if default profile is valid then use it
            if(FW_Common::profiles.selectedProfile < PROFILE_COUNT) {
                // set the current IR camera sensitivity
                if(FW_Common::profileData[FW_Common::profiles.selectedProfile].irSensitivity <= DFRobotIRPositionEx::Sensitivity_Max)
                    FW_Common::irSensitivity = (DFRobotIRPositionEx::Sensitivity_e)FW_Common::profileData[FW_Common::profiles.selectedProfile].irSensitivity;

                // set the run mode
                if(FW_Common::profileData[FW_Common::profiles.selectedProfile].runMode < RunMode_Count)
                    FW_Common::runMode = (RunMode_e)FW_Common::profileData[FW_Common::profiles.selectedProfile].runMode;
            }

            SamcoPreferences::LoadToggles();

            if(SamcoPreferences::toggles[OF_Const::customPins])
                SamcoPreferences::LoadPins();

            SamcoPreferences::LoadSettings();
            SamcoPreferences::LoadUSBID();
        }
    }
 
    // We're setting our custom USB identifiers, as defined in the configuration area!
    #ifdef USE_TINYUSB
        // Initializes TinyUSB identifier
        // Values are pulled from EEPROM values that were loaded earlier in setup()
        TinyUSBDevice.setManufacturerDescriptor(MANUFACTURER_NAME);

        if(SamcoPreferences::usb.devicePID) {
            TinyUSBDevice.setID(DEVICE_VID, SamcoPreferences::usb.devicePID);
            if(SamcoPreferences::usb.deviceName[0] == '\0')
                TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
            else TinyUSBDevice.setProductDescriptor(SamcoPreferences::usb.deviceName);
        } else {
            TinyUSBDevice.setProductDescriptor(DEVICE_NAME);
            TinyUSBDevice.setID(DEVICE_VID, DEVICE_PID);
        }
    #endif // USE_TINYUSB

    if(SamcoPreferences::usb.devicePID > 0 && SamcoPreferences::usb.devicePID < 5) {
        playerStartBtn = SamcoPreferences::usb.devicePID + '0';
        playerSelectBtn = SamcoPreferences::usb.devicePID + '4';
    }

    // this is needed for both customs and builtins, as defaults are all uninitialized
    FW_Common::UpdateBindings(SamcoPreferences::toggles[OF_Const::lowButtonsMode]);

    // Initialize DFRobot Camera Wires & Object
    FW_Common::CameraSet();

    // initialize buttons & feedback devices
    FW_Common::buttons.Begin();
    FW_Common::FeedbackSet();

    #ifdef LED_ENABLE
        OF_RGB::LedInit();
    #endif // LED_ENABLE
    
#ifdef USE_TINYUSB
    TUSBDeviceSetup;
    #if defined(ARDUINO_RASPBERRY_PI_PICO_W) && defined(ENABLE_CLASSIC)
    // is VBUS (USB voltage) detected?
    if(digitalRead(34)) {
        // If so, we're connected via USB, so initializing the USB devices chunk.
        TUSBDeviceSetup.begin(1);
        // wait until device mounted
        while(!USBDevice.mounted()) { yield(); }
        Serial.begin(9600);
        Serial.setTimeout(0);
    } else {
        // Else, we're on batt, so init the Bluetooth chunks.
        if(SamcoPreferences::usb.deviceName[0] == '\0')
            TinyUSBDevice.beginBT(DEVICE_NAME, DEVICE_NAME);
        else TinyUSBDevice.beginBT(SamcoPreferences::usb.deviceName, SamcoPreferences::usb.deviceName);
    }
    #else
    // Initializing the USB devices chunk.
    TinyUSBDevices.begin(1);
    // wait until device mounted
    while(!USBDevice.mounted()) { yield(); }
    Serial.begin(9600);   // 9600 = 1ms data transfer rates, default for MAMEHOOKER COM devices.
    Serial.setTimeout(0);
    #endif // ARDUINO_RASPBERRY_PI_PICO_W
#else
    // was getting weird hangups... maybe nothing, or maybe related to dragons, so wait a bit
    delay(100);
#endif
    
    AbsMouse5.init(true);

    // IR camera maxes out motion detection at ~300Hz, and millis() isn't good enough
    startIrCamTimer(209);

    FW_Common::OpenFIREper.source(FW_Common::profileData[FW_Common::profiles.selectedProfile].adjX,
                                  FW_Common::profileData[FW_Common::profiles.selectedProfile].adjY);
    FW_Common::OpenFIREper.deinit(0);

    // First boot sanity checks.
    // Check if loading has failde
    if((FW_Common::nvPrefsError != SamcoPreferences::Error_Success) ||
    (FW_Common::profileData[FW_Common::profiles.selectedProfile].topOffset == 0 &&
     FW_Common::profileData[FW_Common::profiles.selectedProfile].bottomOffset == 0 && 
     FW_Common::profileData[FW_Common::profiles.selectedProfile].leftOffset == 0 &&
     FW_Common::profileData[FW_Common::profiles.selectedProfile].rightOffset == 0)) {
        // This is a first boot! Prompt to start calibration.
        unsigned int timerIntervalShort = 600;
        unsigned int timerInterval = 1000;
        OF_RGB::LedOff();
        unsigned long lastT = millis();
        bool LEDisOn = false;
        #ifdef USES_DISPLAY
            FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Init);
        #endif // USES_DISPLAY
        while(!(FW_Common::buttons.pressedReleased == BtnMask_Trigger)) {
            // Check and process serial commands, in case user needs to change EEPROM settings.
            if(Serial.available())
                OF_Serial::SerialProcessingDocked();
            
            if(FW_Common::gunMode == GunMode_Docked) {
                ExecGunModeDocked();
                #ifdef USES_DISPLAY
                  FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Init);
                #endif // USES_DISPLAY
            }

            FW_Common::buttons.Poll(1);
            FW_Common::buttons.Repeat();

            // LED update:
            if(LEDisOn) {
                unsigned long t = millis();
                if(t - lastT > timerInterval) {
                    OF_RGB::LedOff();
                    LEDisOn = false;
                    lastT = millis();
                }
            } else {
                unsigned long t = millis();
                if(t - lastT > timerIntervalShort) {
                    OF_RGB::LedUpdate(255,125,0);
                    LEDisOn = true;
                    lastT = millis();
                } 
            }
        }
        
        // Because the app offers cali options, just check to make sure that
        // current prof wasn't cal'd from THERE for sanity.
        if((FW_Common::profileData[FW_Common::profiles.selectedProfile].topOffset == 0 &&
            FW_Common::profileData[FW_Common::profiles.selectedProfile].bottomOffset == 0 && 
            FW_Common::profileData[FW_Common::profiles.selectedProfile].leftOffset == 0 &&
            FW_Common::profileData[FW_Common::profiles.selectedProfile].rightOffset == 0))
              FW_Common::SetMode(GunMode_Calibration);
        else FW_Common::SetMode(GunMode_Run);
    } else {
        /* In case someone wants to implement functionality when user holds trigger on boot, here's the former "mister mode" template c:
        if(SamcoPreferences::pins[OF_Const::btnTrigger] >= 0 && !digitalRead(SamcoPreferences::pins[OF_Const::btnTrigger])) {
            
        }*/
        // this will turn off the DotStar/RGB LED and ensure proper transition to Run
        FW_Common::SetMode(GunMode_Run);
    }
}

#ifdef ARDUINO_ARCH_RP2040
void startIrCamTimer(const int &frequencyHz)
{
    rp2040EnablePWMTimer(0, frequencyHz);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, rp2040pwmIrq);
    irq_set_enabled(PWM_IRQ_WRAP, true);
}

void rp2040EnablePWMTimer(const unsigned int &slice_num, const unsigned int &frequency)
{
    pwm_config pwmcfg = pwm_get_default_config();
    float clkdiv = (float)clock_get_hz(clk_sys) / (float)(65535 * frequency);
    if(clkdiv < 1.0f) {
        clkdiv = 1.0f;
    } else {
        // really just need to round up 1 lsb
        clkdiv += 2.0f / (float)(1u << PWM_CH1_DIV_INT_LSB);
    }
    
    // set the clock divider in the config and fetch the actual value that is used
    pwm_config_set_clkdiv(&pwmcfg, clkdiv);
    clkdiv = (float)pwmcfg.div / (float)(1u << PWM_CH1_DIV_INT_LSB);
    
    // calculate wrap value that will trigger the IRQ for the target frequency
    pwm_config_set_wrap(&pwmcfg, (float)clock_get_hz(clk_sys) / (frequency * clkdiv));
    
    // initialize and start the slice and enable IRQ
    pwm_init(slice_num, &pwmcfg, true);
    pwm_set_irq_enabled(slice_num, true);
}

void rp2040pwmIrq(void)
{
    pwm_hw->intr = 0xff;
    FW_Common::irPosUpdateTick = 1;
}


#ifdef DUAL_CORE
// Second core setup
// does... nothing, since timing is kinda important.
void setup1()
{
    // i sleep
}

// Second core main loop
// currently handles all button & serial processing when Core 0 is in ExecRunMode()
void loop1()
{
    #ifdef USES_ANALOG
        unsigned long lastAnalogPoll = millis();
    #endif // USES_ANALOG

    while(FW_Common::gunMode == GunMode_Run) {
        // For processing the trigger specifically.
        // (FW_Common::buttons.debounced is a binary variable intended to be read 1 bit at a time, with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
        FW_Common::buttons.Poll(0);

        #ifdef MAMEHOOKER
            if(Serial.available())
                OF_Serial::SerialProcessing();

            if(!OF_Serial::serialMode) {   // Have we released a serial signal pulse? If not,
                if(bitRead(FW_Common::buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFire();                                      // Handle button events and feedback ourselves.
                } else {   // Or we haven't pressed the trigger.
                    TriggerNotFire();                                   // Releasing button inputs and sending stop signals to feedback devices.
                }
            } else {   // This is if we've received a serial signal pulse in the last n millis.
                // For safety reasons, we're just using the second core for polling, and the main core for sending signals entirely. Too much a headache otherwise. =w='
                if(bitRead(FW_Common::buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                                // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                             // Release button inputs.
                }
                OF_Serial::SerialHandling();                                       // Process the force feedback.
            }
        #else
            if(bitRead(FW_Common::buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                          // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                       // Releasing button inputs and sending stop signals to feedback devices.
            }
        #endif // MAMEHOOKER

        #ifdef USES_ANALOG
            if(FW_Common::analogIsValid && (millis() - lastAnalogPoll > 1)) {
                AnalogStickPoll();
                lastAnalogPoll = millis();
            }
        #endif // USES_ANALOG
        
        if(FW_Common::buttons.pressedReleased == EscapeKeyBtnMask)
            SendEscapeKey();

        if(SamcoPreferences::toggles[OF_Const::holdToPause]) {
            if((FW_Common::buttons.debounced == EnterPauseModeHoldBtnMask)
                && !FW_Common::lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                if(!OF_Serial::serialMode)
                    Serial.println("Started holding pause mode signal buttons!");

            } else if(pauseHoldStarted && (FW_Common::buttons.debounced != EnterPauseModeHoldBtnMask || FW_Common::lastSeen)) {
                pauseHoldStarted = false;
                if(!OF_Serial::serialMode)
                    Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");

            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > SamcoPreferences::settings[OF_Const::holdToPauseLength]) {
                    // MAKE SURE EVERYTHING IS DISENGAGED:
                    OF_FFB::FFBShutdown();
                    FW_Common::offscreenBShot = false;
                    FW_Common::buttonPressed = false;
                    FW_Common::pauseModeSelection = PauseMode_Calibrate;
                    FW_Common::buttons.ReportDisable();
                    FW_Common::SetMode(GunMode_Pause);
                }
            }
        } else {
            if(FW_Common::buttons.pressedReleased == EnterPauseModeBtnMask || FW_Common::buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                OF_FFB::FFBShutdown();
                FW_Common::offscreenBShot = false;
                FW_Common::buttonPressed = false;
                FW_Common::buttons.ReportDisable();
                FW_Common::SetMode(GunMode_Pause);
                // at this point, the other core should be stopping us now.
            }
        }
    }
}
#endif // DUAL_CORE
#endif // ARDUINO_ARCH_RP2040

// Main core events hub
// splits off into subsequent ExecModes depending on circumstances
void loop()
{
    // poll/update button states with 1ms interval so debounce mask is more effective
    FW_Common::buttons.Poll(1);
    FW_Common::buttons.Repeat();

    if(SamcoPreferences::toggles[OF_Const::holdToPause] && pauseHoldStarted) {
        #ifdef USES_RUMBLE
            analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
            delay(300);
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
        #endif // USES_RUMBLE
        while(FW_Common::buttons.debounced != 0) {
            // Should release the buttons to continue, pls.
            FW_Common::buttons.Poll(1);
        }
        pauseHoldStarted = false;
        pauseModeSelectingProfile = false;
    }

    #ifdef MAMEHOOKER
        if(Serial.available()) OF_Serial::SerialProcessing();
    #endif // MAMEHOOKER

    switch(FW_Common::gunMode) {
        case GunMode_Pause:
            if(SamcoPreferences::toggles[OF_Const::simplePause]) {
                if(pauseModeSelectingProfile) {
                    if(FW_Common::buttons.pressedReleased == BtnMask_A) {
                        SetProfileSelection(false);
                    } else if(FW_Common::buttons.pressedReleased == BtnMask_B) {
                        SetProfileSelection(true);
                    } else if(FW_Common::buttons.pressedReleased == BtnMask_Trigger) {
                        FW_Common::SelectCalProfile(profileModeSelection);
                        pauseModeSelectingProfile = false;
                        FW_Common::pauseModeSelection = PauseMode_Calibrate;

                        if(!OF_Serial::serialMode) {
                            Serial.print("Switched to profile: ");
                            Serial.println(FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
                            Serial.println("Going back to the main menu...");
                            Serial.println("Selecting: Calibrate current profile");
                        }

                        #ifdef USES_DISPLAY
                            FW_Common::OLED.PauseListUpdate(ExtDisplay::ScreenPause_Calibrate);
                        #endif // USES_DISPLAY

                    } else if(FW_Common::buttons.pressedReleased & ExitPauseModeBtnMask) {
                        if(!OF_Serial::serialMode)
                            Serial.println("Exiting profile selection.");

                        pauseModeSelectingProfile = false;

                        #ifdef LED_ENABLE
                            for(byte i = 0; i < 2; i++) {
                                OF_RGB::LedUpdate(180,180,180);
                                delay(125);
                                OF_RGB::LedOff();
                                delay(100);
                            }
                            OF_RGB::LedUpdate(255,0,0);
                        #endif // LED_ENABLE

                        FW_Common::pauseModeSelection = PauseMode_Calibrate;

                        #ifdef USES_DISPLAY
                            FW_Common::OLED.PauseListUpdate(ExtDisplay::ScreenPause_Calibrate);
                        #endif // USES_DISPLAY
                    }
                } else if(FW_Common::buttons.pressedReleased == BtnMask_A) {
                    SetPauseModeSelection(false);
                } else if(FW_Common::buttons.pressedReleased == BtnMask_B) {
                    SetPauseModeSelection(true);
                } else if(FW_Common::buttons.pressedReleased == BtnMask_Trigger) {
                    switch(FW_Common::pauseModeSelection) {
                        case PauseMode_Calibrate:
                          FW_Common::SetMode(GunMode_Calibration);
                          if(!OF_Serial::serialMode) {
                              Serial.print("Calibrating for current profile: ");
                              Serial.println(FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
                          }
                          break;
                        case PauseMode_ProfileSelect:
                          if(!OF_Serial::serialMode) {
                              Serial.println("Pick a profile!");
                              Serial.print("Current profile in use: ");
                              Serial.println(FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
                          }
                          pauseModeSelectingProfile = true;
                          profileModeSelection = FW_Common::profiles.selectedProfile;
                          #ifdef USES_DISPLAY
                              FW_Common::OLED.PauseProfileUpdate(profileModeSelection, FW_Common::profileData[0].name, FW_Common::profileData[1].name, FW_Common::profileData[2].name, FW_Common::profileData[3].name);
                          #endif // USES_DISPLAY
                          #ifdef LED_ENABLE
                              OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);
                          #endif // LED_ENABLE
                          break;
                        case PauseMode_Save:
                          if(!OF_Serial::serialMode)
                              Serial.println("Saving...");
                          FW_Common::SavePreferences();
                          break;
                        #ifdef USES_RUMBLE
                        case PauseMode_RumbleToggle:
                          if(!OF_Serial::serialMode)
                              Serial.println("Toggling rumble!");
                          RumbleToggle();
                          break;
                        #endif // USES_RUMBLE
                        #ifdef USES_SOLENOID
                        case PauseMode_SolenoidToggle:
                          if(!OF_Serial::serialMode)
                              Serial.println("Toggling solenoid!");
                          SolenoidToggle();
                          break;
                        #endif // USES_SOLENOID
                        /*
                        #ifdef USES_SOLENOID
                        case PauseMode_BurstFireToggle:
                          Serial.println("Toggling solenoid burst firing!");
                          BurstFireToggle();
                          break;
                        #endif // USES_SOLENOID
                        */
                        case PauseMode_EscapeSignal:
                          SendEscapeKey();

                          #ifdef USES_DISPLAY
                              FW_Common::OLED.TopPanelUpdate("Sent Escape Key!");
                          #endif // USES_DISPLAY

                          #ifdef LED_ENABLE
                              for(byte i = 0; i < 3; i++) {
                                  OF_RGB::LedUpdate(150,0,150);
                                  delay(55);
                                  OF_RGB::LedOff();
                                  delay(40);
                              }
                          #endif // LED_ENABLE

                          #ifdef USES_DISPLAY
                              FW_Common::OLED.TopPanelUpdate("Using ", FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
                          #endif // USES_DISPLAY
                          break;
                        /*case PauseMode_Exit:
                          Serial.println("Exiting pause mode...");
                          if(FW_Common::runMode == RunMode_Processing) {
                              switch(FW_Common::profileData[FW_Common::profiles.selectedProfile].FW_Common::runMode) {
                                  case RunMode_Normal:
                                    FW_Common::SetRunMode(RunMode_Normal);
                                    break;
                                  case RunMode_Average:
                                    FW_Common::SetRunMode(RunMode_Average);
                                    break;
                                  case RunMode_Average2:
                                    FW_Common::SetRunMode(RunMode_Average2);
                                    break;
                                  default:
                                    break;
                              }
                          }
                          FW_Common::SetMode(GunMode_Run);
                          break;
                        */
                        default:
                          Serial.println("Oops, somethnig went wrong.");
                          break;
                    }
                } else if(FW_Common::buttons.pressedReleased & ExitPauseModeBtnMask) {
                    if(!OF_Serial::serialMode)
                        Serial.println("Exiting pause mode...");
                    FW_Common::SetMode(GunMode_Run);
                }
                if(pauseExitHoldStarted &&
                (FW_Common::buttons.debounced & ExitPauseModeHoldBtnMask)) {
                    unsigned long t = millis();
                    if(t - pauseHoldStartstamp > (SamcoPreferences::settings[OF_Const::holdToPauseLength] / 2)) {
                        if(!OF_Serial::serialMode)
                            Serial.println("Exiting pause mode via hold...");

                        if(FW_Common::runMode == RunMode_Processing) {
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
                                default:
                                  break;
                            }
                        }

                        #ifdef USES_RUMBLE
                            for(byte i = 0; i < 3; i++) {
                                analogWrite(SamcoPreferences::pins[OF_Const::rumblePin], SamcoPreferences::settings[OF_Const::rumbleStrength]);
                                delay(80);
                                digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);
                                delay(50);
                            }
                        #endif // USES_RUMBLE

                        while(FW_Common::buttons.debounced != 0)
                            // keep polling until all buttons are debounced
                            FW_Common::buttons.Poll(1);

                        FW_Common::SetMode(GunMode_Run);
                        pauseExitHoldStarted = false;
                    }
                } else if(FW_Common::buttons.debounced & ExitPauseModeHoldBtnMask) {
                    pauseExitHoldStarted = true;
                    pauseHoldStartstamp = millis();
                } else if(FW_Common::buttons.pressedReleased & ExitPauseModeHoldBtnMask)
                    pauseExitHoldStarted = false;
            } else if(FW_Common::buttons.pressedReleased & ExitPauseModeBtnMask) {
                FW_Common::SetMode(GunMode_Run);
            } else if(FW_Common::buttons.pressedReleased == BtnMask_Trigger) {
                FW_Common::SetMode(GunMode_Calibration);
            } else if(FW_Common::buttons.pressedReleased == RunModeNormalBtnMask) {
                FW_Common::SetRunMode(RunMode_Normal);
            } else if(FW_Common::buttons.pressedReleased == RunModeAverageBtnMask) {
                FW_Common::SetRunMode(FW_Common::runMode == RunMode_Average ? RunMode_Average2 : RunMode_Average);
            } else if(FW_Common::buttons.pressedReleased == IRSensitivityUpBtnMask) {
                IncreaseIrSensitivity();
            } else if(FW_Common::buttons.pressedReleased == IRSensitivityDownBtnMask) {
                DecreaseIrSensitivity();
            } else if(FW_Common::buttons.pressedReleased == SaveBtnMask) {
                FW_Common::SavePreferences();
            } else if(FW_Common::buttons.pressedReleased == OffscreenButtonToggleBtnMask) {
                OffscreenToggle();
            } else if(FW_Common::buttons.pressedReleased == AutofireSpeedToggleBtnMask) {
                AutofireSpeedToggle();
            #ifdef USES_RUMBLE
                } else if(FW_Common::buttons.pressedReleased == RumbleToggleBtnMask && SamcoPreferences::pins[OF_Const::rumbleSwitch] >= 0) {
                    RumbleToggle();
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                } else if(FW_Common::buttons.pressedReleased == SolenoidToggleBtnMask && SamcoPreferences::pins[OF_Const::solenoidSwitch] >= 0) {
                    SolenoidToggle();
            #endif // USES_SOLENOID
            } else SelectCalProfileFromBtnMask(FW_Common::buttons.pressedReleased);

            if(!OF_Serial::serialMode)
                OF_Serial::PrintResults();
            
            break;
        case GunMode_Docked:
            ExecGunModeDocked();
            break;
        case GunMode_Calibration:
            FW_Common::ExecCalMode();
            break;
        default:
            /* ---------------------- LET'S GO --------------------------- */
            switch(FW_Common::runMode) {
            case RunMode_Processing:
                //ExecRunModeProcessing();
                //break;
            case RunMode_Average:
            case RunMode_Average2:
            case RunMode_Normal:
            default:
                ExecRunMode();
                break;
            }
            break;
    }

#ifdef DEBUG_SERIAL
    OF_Serial::PrintDebugSerial();
#endif // DEBUG_SERIAL
}

/*        -----------------------------------------------        */
/* --------------------------- METHODS ------------------------- */
/*        -----------------------------------------------        */

// Main core loop
void ExecRunMode()
{
#ifdef DEBUG_SERIAL
    Serial.print("exec run mode ");
    Serial.println(RunModeLabels[FW_Common::runMode]);
#endif

    FW_Common::buttons.ReportEnable();
    if(FW_Common::justBooted) {
        // center the joystick so RetroArch doesn't throw a hissy fit about uncentered joysticks
        delay(100);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
        FW_Common::justBooted = false;
    }

    #ifdef USES_ANALOG
        unsigned long lastAnalogPoll = millis();
    #endif // USES_ANALOG

    for(;;) {
        // Setting the state of our toggles, if used.
        // Only sets these values if the switches are mapped to valid pins.
        #ifdef USES_SWITCHES
            #ifdef USES_RUMBLE
                if(SamcoPreferences::pins[OF_Const::rumbleSwitch] >= 0) {
                    SamcoPreferences::toggles[OF_Const::rumble] = !digitalRead(SamcoPreferences::pins[OF_Const::rumbleSwitch]);
                    #ifdef MAMEHOOKER
                    if(!OF_Serial::serialMode) {
                    #endif // MAMEHOOKER
                        if(!SamcoPreferences::toggles[OF_Const::rumble] && OF_FFB::rumbleHappening)
                            OF_FFB::FFBShutdown();
                    #ifdef MAMEHOOKER
                    }
                    #endif // MAMEHOOKER
                }
            #endif // USES_RUMBLE
            #ifdef USES_SOLENOID
                if(SamcoPreferences::pins[OF_Const::solenoidSwitch] >= 0) {
                    SamcoPreferences::toggles[OF_Const::solenoid] = !digitalRead(SamcoPreferences::pins[OF_Const::solenoidSwitch]);
                    #ifdef MAMEHOOKER
                    if(!OF_Serial::serialMode) {
                    #endif // MAMEHOOKER
                        if(!SamcoPreferences::toggles[OF_Const::solenoid] && digitalRead(SamcoPreferences::pins[OF_Const::solenoidPin])) {
                            OF_FFB::FFBShutdown();
                        }
                    #ifdef MAMEHOOKER
                    }
                    #endif // MAMEHOOKER
                }
            #endif // USES_SOLENOID
            if(SamcoPreferences::pins[OF_Const::autofireSwitch] >= 0)
                SamcoPreferences::toggles[OF_Const::autofire] = !digitalRead(SamcoPreferences::pins[OF_Const::autofireSwitch]);
        #endif // USES_SWITCHES

        // If we're on RP2040, we offload the button polling to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)
        FW_Common::buttons.Poll(0);

        // The main FW_Common::gunMode loop: here it splits off to different paths,
        // depending on if we're in serial handoff (MAMEHOOK) or normal mode.
        #ifdef MAMEHOOKER
            // Run through serial receive buffer once this run, if it has contents.
            if(Serial.available())
                SerialProcessing();

            if(!OF_Serial::serialMode) {  // Normal (gun-handled) mode
                // For processing the trigger specifically.
                // (FW_Common::buttons.debounced is a binary variable intended to be read 1 bit at a time,
                // with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
                if(bitRead(FW_Common::buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFire();                                  // Handle button events and feedback ourselves.
                } else {   // Or we haven't pressed the trigger.
                    TriggerNotFire();                               // Releasing button inputs and sending stop signals to feedback devices.
                }
            } else {  // Serial handoff mode
                if(bitRead(FW_Common::buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                    TriggerFireSimple();                            // Since serial is handling our devices, we're just handling button events.
                } else {   // Or if we haven't pressed the trigger,
                    TriggerNotFireSimple();                         // Release button inputs.
                }
                SerialHandling();                                   // Process the force feedback from the current queue.
            }
        #else
            // For processing the trigger specifically.
            // (FW_Common::buttons.debounced is a binary variable intended to be read 1 bit at a time,
            // with the 0'th point == rightmost == decimal 1 == trigger, 3 = start, 4 = select)
            if(bitRead(FW_Common::buttons.debounced, 0)) {   // Check if we pressed the Trigger this run.
                TriggerFire();                                      // Handle button events and feedback ourselves.
            } else {   // Or we haven't pressed the trigger.
                TriggerNotFire();                                   // Releasing button inputs and sending stop signals to feedback devices.
            }
        #endif // MAMEHOOKER
        #endif // DUAL_CORE

        if(FW_Common::irPosUpdateTick) {
            FW_Common::irPosUpdateTick = 0;
            FW_Common::GetPosition();
        }

        #ifdef MAMEHOOKER
            #ifdef USES_DISPLAY
                // For some reason, solenoid feedback is hella wonky when ammo updates are performed on the second core,
                // so just do it here using the signal sent by it.
                if(OF_Serial::serialDisplayChange) {
                    if(FW_Common::OLED.serialDisplayType == ExtDisplay::ScreenSerial_Ammo) {
                        FW_Common::OLED.PrintAmmo(OF_Serial::serialAmmoCount);
                    } else if(FW_Common::OLED.serialDisplayType == ExtDisplay::ScreenSerial_Life && FW_Common::OLED.lifeBar) {
                        FW_Common::OLED.PrintLife(FW_Common::dispLifePercentage);
                    } else if(FW_Common::OLED.serialDisplayType == ExtDisplay::ScreenSerial_Life) {
                        FW_Common::OLED.PrintLife(OF_Serial::serialLifeCount);
                    } else if(FW_Common::OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both && FW_Common::OLED.lifeBar) {
                        FW_Common::OLED.PrintAmmo(OF_Serial::serialAmmoCount);
                        FW_Common::OLED.PrintLife(FW_Common::dispLifePercentage);
                    } else if(FW_Common::OLED.serialDisplayType == ExtDisplay::ScreenSerial_Both) {
                        FW_Common::OLED.PrintAmmo(OF_Serial::serialAmmoCount);
                        FW_Common::OLED.PrintLife(OF_Serial::serialLifeCount);
                    }

                    OF_Serial::serialDisplayChange = false;
                }
            #endif // USES_DISPLAY
        #endif // MAMEHOOKER

        // If using RP2040, we offload the button processing to the second core.
        #if !defined(ARDUINO_ARCH_RP2040) || !defined(DUAL_CORE)

        #ifdef USES_ANALOG
            if(FW_Common::analogIsValid && (millis() - lastAnalogPoll > 1)) {
                AnalogStickPoll();
                lastAnalogPoll = millis();
            }
        #endif // USES_ANALOG

        if(FW_Common::buttons.pressedReleased == EscapeKeyBtnMask)
            SendEscapeKey();

        if(SamcoPreferences::toggles[OF_Const::holdToPause]) {
            if((FW_Common::buttons.debounced == EnterPauseModeHoldBtnMask)
                && !FW_Common::lastSeen && !pauseHoldStarted) {
                pauseHoldStarted = true;
                pauseHoldStartstamp = millis();
                if(!OF_Serial::serialMode)
                    Serial.println("Started holding pause mode signal buttons!");

            } else if(pauseHoldStarted && (FW_Common::buttons.debounced != EnterPauseModeHoldBtnMask || FW_Common::lastSeen)) {
                pauseHoldStarted = false;
                if(!OF_Serial::serialMode)
                    Serial.println("Either stopped holding pause mode buttons, aimed onscreen, or pressed other buttons");

            } else if(pauseHoldStarted) {
                unsigned long t = millis();
                if(t - pauseHoldStartstamp > SamcoPreferences::settings[OF_Const::holdToPauseLength]) {
                    // MAKE SURE EVERYTHING IS DISENGAGED:
                    OF_FFB::FFBShutdown();
		                FW_Common::Keyboard.releaseAll();
                    FW_Common::AbsMouse5.releaseAll();
                    FW_Common::offscreenBShot = false;
                    buttonPressed = false;
	    	            FW_Common::pauseModeSelection = PauseMode_Calibrate;
                    FW_Common::SetMode(GunMode_Pause);
                    FW_Common::buttons.ReportDisable();
                    return;
                }
            }
        } else {
            if(FW_Common::buttons.pressedReleased == EnterPauseModeBtnMask || FW_Common::buttons.pressedReleased == BtnMask_Home) {
                // MAKE SURE EVERYTHING IS DISENGAGED:
                OF_FFB::FFBShutdown();
		            FW_Common::Keyboard.releaseAll();
                FW_Common::AbsMouse5.releaseAll();
                FW_Common::offscreenBShot = false;
                buttonPressed = false;
		            FW_Common::SetMode(GunMode_Pause);
                FW_Common::buttons.ReportDisable();
                return;
            }
        }
        #else  // if we're using dual cores, we just check if the gunmode has been changed by the other thread.
        if(FW_Common::gunMode != GunMode_Run) {
            Keyboard.releaseAll();
            AbsMouse5.releaseAll();
            return;
        }
        #endif // ARDUINO_ARCH_RP2040 || DUAL_CORE

#ifdef DEBUG_SERIAL
        ++frameCount;
        OF_Serial::PrintDebugSerial();
#endif // DEBUG_SERIAL
    }
}

// from Samco_4IR_Test_BETA sketch
// for use with the Samco_4IR_Processing_Sketch_BETA Processing sketch
void ExecRunModeProcessing()
{
    FW_Common::buttons.ReportDisable();

    #ifdef USES_DISPLAY
        FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_IRTest);
    #endif // USES_DISPLAY

    for(;;) {
        FW_Common::buttons.Poll(1);

        if(Serial.available()) {
            #ifdef USES_DISPLAY
                FW_Common::OLED.ScreenModeChange(ExtDisplay::Screen_Docked);
            #endif // USES_DISPLAY

            OF_Serial::SerialProcessingDocked();
        }

        if(FW_Common::runMode != RunMode_Processing)
            return;

        if(FW_Common::irPosUpdateTick) {
            FW_Common::irPosUpdateTick = 0;
            FW_Common::GetPosition();
        }
    }
}

// For use with the OpenFIRE app when it connects to this board.
void ExecGunModeDocked()
{
    FW_Common::buttons.ReportDisable();

    if(FW_Common::justBooted) {
        // center the joystick so RetroArch/Windows doesn't throw a hissy fit about uncentered joysticks
        delay(250);  // Exact time needed to wait seems to vary, so make a safe assumption here.
        Gamepad16.releaseAll();
    }

    #ifdef LED_ENABLE
        OF_RGB::LedUpdate(127, 127, 255);
    #endif // LED_ENABLE

    unsigned long tempChecked = millis();
    unsigned long aStickChecked = millis();
    uint8_t aStickDirPrev;

#ifdef GIT_HASH
    Serial.printf("OpenFIRE,%.1f-%s,%s,%s,%i\r\n",
    OPENFIRE_VERSION,
    GIT_HASH,
    OPENFIRE_CODENAME,
    OPENFIRE_BOARD,
    FW_Common::profiles.selectedProfile);
#else
    Serial.printf("OpenFIRE,%.1f,%s,%s,%i\r\n",
    OPENFIRE_VERSION,
    OPENFIRE_CODENAME,
    OPENFIRE_BOARD,
    FW_Common::profiles.selectedProfile);
#endif // GIT_HASH

    for(;;) {
        FW_Common::buttons.Poll(1);

        if(Serial.available())
            OF_Serial::SerialProcessingDocked();

        if(!FW_Common::dockedSaving) {
            if(FW_Common::buttons.pressed)
                Serial.printf("Pressed: %d\n", FW_Common::buttons.pressed);

            /*
            switch(FW_Common::buttons.pressed) {
                case BtnMask_Trigger:
                  Serial.println("Pressed: 1");
                  break;
                case BtnMask_A:
                  Serial.println("Pressed: 2");
                  break;
                case BtnMask_B:
                  Serial.println("Pressed: 3");
                  break;
                case BtnMask_Reload:
                  Serial.println("Pressed: 4");
                  break;
                case BtnMask_Start:
                  Serial.println("Pressed: 5");
                  break;
                case BtnMask_Select:
                  Serial.println("Pressed: 6");
                  break;
                case BtnMask_Up:
                  Serial.println("Pressed: 7");
                  break;
                case BtnMask_Down:
                  Serial.println("Pressed: 8");
                  break;
                case BtnMask_Left:
                  Serial.println("Pressed: 9");
                  break;
                case BtnMask_Right:
                  Serial.println("Pressed: 10");
                  break;
                case BtnMask_Pedal:
                  Serial.println("Pressed: 11");
                  break;
                case BtnMask_Pedal2:
                  Serial.println("Pressed: 12");
                  break;
                case BtnMask_Home:
                  Serial.println("Pressed: 13");
                  break;
                case BtnMask_Pump:
                  Serial.println("Pressed: 14");
                  break;
            }*/

            if(FW_Common::buttons.released)
                Serial.printf("Released: %d\n", FW_Common::buttons.released);

            /*
            switch(FW_Common::buttons.released) {
                case BtnMask_Trigger:
                  Serial.println("Released: 1");
                  break;
                case BtnMask_A:
                  Serial.println("Released: 2");
                  break;
                case BtnMask_B:
                  Serial.println("Released: 3");
                  break;
                case BtnMask_Reload:
                  Serial.println("Released: 4");
                  break;
                case BtnMask_Start:
                  Serial.println("Released: 5");
                  break;
                case BtnMask_Select:
                  Serial.println("Released: 6");
                  break;
                case BtnMask_Up:
                  Serial.println("Released: 7");
                  break;
                case BtnMask_Down:
                  Serial.println("Released: 8");
                  break;
                case BtnMask_Left:
                  Serial.println("Released: 9");
                  break;
                case BtnMask_Right:
                  Serial.println("Released: 10");
                  break;
                case BtnMask_Pedal:
                  Serial.println("Released: 11");
                  break;
                case BtnMask_Pedal2:
                  Serial.println("Released: 12");
                  break;
                case BtnMask_Home:
                  Serial.println("Released: 13");
                  break;
                case BtnMask_Pump:
                  Serial.println("Released: 14");
                  break;
            }*/

            OF_FFB::TemperatureUpdate();
            unsigned long currentMillis = millis();
            if(currentMillis - tempChecked >= 1000) {
                if(SamcoPreferences::pins[OF_Const::tempPin] >= 0)
                    Serial.printf("Temperature: %d\r\n", OF_FFB::temperatureCurrent);

                tempChecked = currentMillis;
            }
            
            if(FW_Common::analogIsValid) {
                if(currentMillis - aStickChecked >= 16) {
                    unsigned int analogValueX = analogRead(SamcoPreferences::pins[OF_Const::analogX]);
                    unsigned int analogValueY = analogRead(SamcoPreferences::pins[OF_Const::analogY]);
                    // Analog stick deadzone should help mitigate overwriting USB commands for the other input channels.
                    uint8_t aStickDir = 0;

                    if((analogValueX < 1900 || analogValueX > 2200) ||
                      (analogValueY < 1900 || analogValueY > 2200)) {
                        if(analogValueX > 2200) {
                            bitSet(aStickDir, 0), bitClear(aStickDir, 1);
                        } else if(analogValueX < 1900) {
                            bitSet(aStickDir, 1), bitClear(aStickDir, 0);
                        } else {
                            bitClear(aStickDir, 0), bitClear(aStickDir, 1);
                        }
                        if(analogValueY > 2200) { 
                            bitSet(aStickDir, 2), bitClear(aStickDir, 3);
                        } else if(analogValueY < 1900) {
                            bitSet(aStickDir, 3), bitClear(aStickDir, 2);
                        } else {
                            bitClear(aStickDir, 2), bitClear(aStickDir, 3);
                        }
                    }

                    if(aStickDir != aStickDirPrev) {
                        switch(aStickDir) {
                        case 0b00000100: // up
                          Serial.println("Analog: 1");
                          break;
                        case 0b00000101: // up-left
                          Serial.println("Analog: 2");
                          break;
                        case 0b00000001: // left
                          Serial.println("Analog: 3");
                          break;
                        case 0b00001001: // down-left
                          Serial.println("Analog: 4");
                          break;
                        case 0b00001000: // down
                          Serial.println("Analog: 5");
                          break;
                        case 0b00001010: // down-right
                          Serial.println("Analog: 6");
                          break;
                        case 0b00000010: // right
                          Serial.println("Analog: 7");
                          break;
                        case 0b00000110: // up-right
                          Serial.println("Analog: 8");
                          break;
                        default:         // center
                          Serial.println("Analog: 0");
                          break;
                        }

                        aStickDirPrev = aStickDir;
                    }
                }
            }
        }

        if(FW_Common::gunMode != GunMode_Docked)
            return;

        if(FW_Common::runMode == RunMode_Processing)
            ExecRunModeProcessing();
    }
}

// wait up to given amount of time for no buttons to be pressed before setting the mode
void SetModeWaitNoButtons(const GunMode_e &newMode, const unsigned long &maxWait)
{
    unsigned long ms = millis();
    while(FW_Common::buttons.debounced && (millis() - ms < maxWait))
        FW_Common::buttons.Poll(1);

    FW_Common::SetMode(newMode);
}

// Handles events when trigger is pulled/held
void TriggerFire()
{
    if(!FW_Common::buttons.offScreen &&                                     // Check if the X or Y axis is in the screen's boundaries, i.e. "off screen".
    !FW_Common::offscreenBShot) {                                           // And only as long as we haven't fired an off-screen shot,
        if(!FW_Common::buttonPressed) {
            if(FW_Common::buttons.analogOutput)
                Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3); // No reason to handle this ourselves here, but eh.
            else AbsMouse5.press(MOUSE_LEFT);                     // We're handling the trigger button press ourselves for a reason.

            FW_Common::buttonPressed = true;                                // Set this so we won't spam a repeat press event again.
        }

        if(!bitRead(FW_Common::buttons.debounced, 3) &&                     // Is the trigger being pulled WITHOUT pressing Start & Select?
        !bitRead(FW_Common::buttons.debounced, 4))
            OF_FFB::FFBOnScreen();
    } else {  // We're shooting outside of the screen boundaries!
        if(!FW_Common::buttonPressed) {  // If we haven't pressed a trigger key yet,
            if(!OF_FFB::triggerHeld && FW_Common::offscreenButton) {  // If we are in offscreen button mode (and aren't dragging a shot offscreen)
                if(FW_Common::buttons.analogOutput)
                    Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
                else AbsMouse5.press(MOUSE_RIGHT);

                FW_Common::offscreenBShot = true;                     // Mark we pressed the right button via offscreen shot mode,
            } else {  // Or if we're not in offscreen button mode,
                if(FW_Common::buttons.analogOutput)
                    Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
                else AbsMouse5.press(MOUSE_LEFT);
            }

            FW_Common::buttonPressed = true;                      // Mark so we're not spamming these press events.
        }
        OF_FFB::FFBOffScreen();
    }
    OF_FFB::triggerHeld = true;                                   // Signal that we've started pulling the trigger this poll cycle.
}

// Handles events when trigger is released
void TriggerNotFire()
{
    OF_FFB::triggerHeld = false;                                    // Disable the holding function
    if(FW_Common::buttonPressed) {
        if(FW_Common::offscreenBShot) {                                // If we fired off screen with the FW_Common::offscreenButton set,
            if(FW_Common::buttons.analogOutput)
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
            else AbsMouse5.release(MOUSE_RIGHT);             // We were pressing the right mouse, so release that.

            FW_Common::offscreenBShot = false;
        } else {                                            // Or if not,
            if(FW_Common::buttons.analogOutput)
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
            else AbsMouse5.release(MOUSE_LEFT);              // We were pressing the left mouse, so release that instead.
        }
        
        FW_Common::buttonPressed = false;
    }
    
    OF_FFB::FFBRelease();
}

#ifdef USES_ANALOG
void AnalogStickPoll()
{
    unsigned int analogValueX = analogRead(SamcoPreferences::pins[OF_Const::analogX]);
    unsigned int analogValueY = analogRead(SamcoPreferences::pins[OF_Const::analogY]);
    
    // Analog stick deadzone should help mitigate overwriting USB commands for the other input channels.
    if((analogValueX < 1900 || analogValueX > 2200) ||
       (analogValueY < 1900 || analogValueY > 2200)) {
          Gamepad16.moveStick(analogValueX, analogValueY);
    } else {
        // Duplicate coords won't be reported, so no worries.
        Gamepad16.moveStick(2048, 2048);
    }
}
#endif // USES_ANALOG

#ifdef MAMEHOOKER
// Trigger execution path while in Serial handoff mode - pulled
void TriggerFireSimple()
{
    if(!FW_Common::buttonPressed &&                             // Have we not fired the last cycle,
    OF_Serial::offscreenButtonSerial && FW_Common::buttons.offScreen) {    // and are pointing the gun off screen WITH the offScreen button mode set?    
        if(FW_Common::buttons.analogOutput)
            Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
	      else AbsMouse5.press(MOUSE_RIGHT);

        FW_Common::offscreenBShot = true;                       // Mark we pressed the right button via offscreen shot mode,
        FW_Common::buttonPressed = true;                        // Mark so we're not spamming these press events.
    } else if(!FW_Common::buttonPressed) {                      // Else, have we simply not fired the last cycle?
	      if(FW_Common::buttons.analogOutput) 
            Gamepad16.press(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
	      else AbsMouse5.press(MOUSE_LEFT);

        FW_Common::buttonPressed = true;                        // Set this so we won't spam a repeat press event again.
    }
}

// Trigger execution path while in Serial handoff mode - released
void TriggerNotFireSimple()
{
    if(FW_Common::buttonPressed) {                              // Just to make sure we aren't spamming mouse button events.
        if(FW_Common::offscreenBShot) {                         // if it was marked as an offscreen button shot,
            if(FW_Common::buttons.analogOutput)
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_A].reportCode3);
	          else AbsMouse5.release(MOUSE_RIGHT);
            FW_Common::offscreenBShot = false;                  // And set it off.
        } else {                                     // Else,
            if(FW_Common::buttons.analogOutput)
                Gamepad16.release(LightgunButtons::ButtonDesc[BtnIdx_Trigger].reportCode3);
	          else AbsMouse5.release(MOUSE_LEFT);
        }
        FW_Common::buttonPressed = false;                       // Unset the button pressed bit.
    }
}
#endif // MAMEHOOKER


void SendEscapeKey()
{
    Keyboard.press(KEY_ESC);
    delay(20);  // wait a bit so it registers on the PC.
    Keyboard.release(KEY_ESC);
}

// Simple Pause Menu scrolling function
// Bool determines if it's incrementing or decrementing the list
// LEDs update according to the setting being scrolled onto, if any.
void SetPauseModeSelection(const bool &isIncrement)
{
    if(isIncrement) {
        if(FW_Common::pauseModeSelection == PauseMode_EscapeSignal) {
            FW_Common::pauseModeSelection = PauseMode_Calibrate;
        } else {
            FW_Common::pauseModeSelection++;
            // If we use switches, and they ARE mapped to valid pins,
            // then skip over the manual toggle options.
            #ifdef USES_SWITCHES
                #ifdef USES_RUMBLE
                    if(FW_Common::pauseModeSelection == PauseMode_RumbleToggle &&
                    (SamcoPreferences::pins[OF_Const::rumbleSwitch] >= 0 || SamcoPreferences::pins[OF_Const::rumblePin] == -1)) {
                        FW_Common::pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(FW_Common::pauseModeSelection == PauseMode_SolenoidToggle &&
                    (SamcoPreferences::pins[OF_Const::solenoidSwitch] >= 0 || SamcoPreferences::pins[OF_Const::solenoidPin] == -1)) {
                        FW_Common::pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #else
                #ifdef USES_RUMBLE
                    if(FW_Common::pauseModeSelection == PauseMode_RumbleToggle &&
                    !(SamcoPreferences::pins[OF_Const::rumblePin] >= 0)) {
                        FW_Common::pauseModeSelection++;
                    }
                #endif // USES_RUMBLE
                #ifdef USES_SOLENOID
                    if(FW_Common::pauseModeSelection == PauseMode_SolenoidToggle &&
                    !(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0)) {
                        FW_Common::pauseModeSelection++;
                    }
                #endif // USES_SOLENOID
            #endif // USES_SWITCHES
        }
    } else {
        if(FW_Common::pauseModeSelection == PauseMode_Calibrate) {
            FW_Common::pauseModeSelection = PauseMode_EscapeSignal;
        } else {
            FW_Common::pauseModeSelection--;
            #ifdef USES_SWITCHES
                #ifdef USES_SOLENOID
                    if(FW_Common::pauseModeSelection == PauseMode_SolenoidToggle &&
                    (SamcoPreferences::pins[OF_Const::solenoidSwitch] >= 0 || SamcoPreferences::pins[OF_Const::solenoidPin] == -1)) {
                        FW_Common::pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(FW_Common::pauseModeSelection == PauseMode_RumbleToggle &&
                    (SamcoPreferences::pins[OF_Const::rumbleSwitch] >= 0 || SamcoPreferences::pins[OF_Const::rumblePin] == -1)) {
                        FW_Common::pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #else
                #ifdef USES_SOLENOID
                    if(FW_Common::pauseModeSelection == PauseMode_SolenoidToggle &&
                    !(SamcoPreferences::pins[OF_Const::solenoidPin] >= 0)) {
                        FW_Common::pauseModeSelection--;
                    }
                #endif // USES_SOLENOID
                #ifdef USES_RUMBLE
                    if(FW_Common::pauseModeSelection == PauseMode_RumbleToggle &&
                    !(SamcoPreferences::pins[OF_Const::rumblePin] >= 0)) {
                        FW_Common::pauseModeSelection--;
                    }
                #endif // USES_RUMBLE
            #endif // USES_SWITCHES
        }
    }

    switch(FW_Common::pauseModeSelection) {
        case PauseMode_Calibrate:
          Serial.println("Selecting: Calibrate current profile");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(255,0,0);
          #endif // LED_ENABLE
          break;
        case PauseMode_ProfileSelect:
          Serial.println("Selecting: Switch profile");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(200,50,0);
          #endif // LED_ENABLE
          break;
        case PauseMode_Save:
          Serial.println("Selecting: Save Settings");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(155,100,0);
          #endif // LED_ENABLE
          break;
        #ifdef USES_RUMBLE
        case PauseMode_RumbleToggle:
          Serial.println("Selecting: Toggle rumble On/Off");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(100,155,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_RUMBLE
        #ifdef USES_SOLENOID
        case PauseMode_SolenoidToggle:
          Serial.println("Selecting: Toggle solenoid On/Off");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(55,200,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
        /*#ifdef USES_SOLENOID
        case PauseMode_BurstFireToggle:
          Serial.println("Selecting: Toggle burst-firing mode");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(0,255,0);
          #endif // LED_ENABLE
          break;
        #endif // USES_SOLENOID
        */
        case PauseMode_EscapeSignal:
          Serial.println("Selecting: Send Escape key signal");
          #ifdef LED_ENABLE
              OF_RGB::LedUpdate(150,0,150);
          #endif // LED_ENABLE
          break;
        /*case PauseMode_Exit:
          Serial.println("Selecting: Exit pause mode");
          break;
        */
        default:
          Serial.println("YOU'RE NOT SUPPOSED TO BE SEEING THIS");
          break;
    }
    #ifdef USES_DISPLAY
        FW_Common::OLED.PauseListUpdate(FW_Common::pauseModeSelection);
    #endif // USES_DISPLAY
}

// Simple Pause Mode - scrolls up/down profiles list
// Bool determines if it's incrementing or decrementing the list
// LEDs update according to the profile being scrolled on, if any.
void SetProfileSelection(const bool &isIncrement)
{
    if(isIncrement) {
        if(profileModeSelection >= PROFILE_COUNT - 1)
            profileModeSelection = 0;
        else profileModeSelection++;
    } else {
        if(profileModeSelection <= 0)
            profileModeSelection = PROFILE_COUNT - 1;
        else profileModeSelection--;
    }

    #ifdef LED_ENABLE
        OF_RGB::SetLedPackedColor(FW_Common::profileData[profileModeSelection].color);
    #endif // LED_ENABLE

    #ifdef USES_DISPLAY
        FW_Common::OLED.PauseProfileUpdate(profileModeSelection, FW_Common::profileData[0].name, FW_Common::profileData[1].name, FW_Common::profileData[2].name, FW_Common::profileData[3].name);
    #endif // USES_DISPLAY

    Serial.print("Selecting profile: ");
    Serial.println(FW_Common::profileData[profileModeSelection].name);

    return;
}

void SelectCalProfileFromBtnMask(const uint32_t &mask)
{
    // only check if buttons are set in the mask
    if(!mask)
        return;

    for(unsigned int i = 0; i < PROFILE_COUNT; ++i) {
        if(FW_Common::profileData[i].buttonMask == mask) {
            FW_Common::SelectCalProfile(i);
            return;
        }
    }
}

void CycleIrSensitivity()
{
    uint8_t sens = FW_Common::irSensitivity;
    if(FW_Common::irSensitivity < DFRobotIRPositionEx::Sensitivity_Max)
        sens++;
    else sens = DFRobotIRPositionEx::Sensitivity_Min;

    FW_Common::SetIrSensitivity(sens);
}

void IncreaseIrSensitivity()
{
    uint8_t sens = FW_Common::irSensitivity;
    if(FW_Common::irSensitivity < DFRobotIRPositionEx::Sensitivity_Max) {
        sens++;
        FW_Common::SetIrSensitivity(sens);
    }
}

void DecreaseIrSensitivity()
{
    uint8_t sens = FW_Common::irSensitivity;
    if(FW_Common::irSensitivity > DFRobotIRPositionEx::Sensitivity_Min) {
        sens--;
        FW_Common::SetIrSensitivity(sens);
    }
}

/*
// applies loaded screen calibration profile
bool SelectCalPrefs(unsigned int profile)
{
    if(profile >= PROFILE_COUNT) {
        return false;
    }

    // if center values are set, assume profile is populated
    if(FW_Common::profileData[profile].xCenter && FW_Common::profileData[profile].yCenter) {
        xCenter = FW_Common::profileData[profile].xCenter;
        yCenter = FW_Common::profileData[profile].yCenter;
        
        // 0 scale will be ignored
        if(FW_Common::profileData[profile].xScale) {
            xScale = CalScalePrefToFloat(FW_Common::profileData[profile].xScale);
        }
        if(FW_Common::profileData[profile].yScale) {
            yScale = CalScalePrefToFloat(FW_Common::profileData[profile].yScale);
        }
        return true;
    }
    return false;
}
*/

// Pause mode offscreen trigger mode toggle widget
// Blinks LEDs (if any) according to enabling or disabling this obtuse setting for finnicky/old programs that need right click as an offscreen shot substitute.
void OffscreenToggle()
{
    FW_Common::FW_Common::offscreenButton = !FW_Common::FW_Common::offscreenButton;
    if(FW_Common::offscreenButton) {                                         // If we turned ON this mode,
        Serial.println("Enabled Offscreen Button!");

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Ghost_white);            // Set a color,
        #endif // LED_ENABLE

        #ifdef USES_RUMBLE
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], HIGH);                        // Set rumble on
            delay(125);                                           // For this long,
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);                         // Then flick it off,
            delay(150);                                           // wait a little,
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], HIGH);                        // Flick it back on
            delay(200);                                           // For a bit,
            digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);                         // and then turn it off,
        #else
            delay(450);
        #endif // USES_RUMBLE

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE

        return;
    } else {                                                      // Or we're turning this OFF,
        Serial.println("Disabled Offscreen Button!");

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Ghost_white);            // Just set a color,
            delay(150);                                           // Keep it on,
            OF_RGB::LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            OF_RGB::SetLedPackedColor(WikiColor::Ghost_white);            // Flicker it back on
            delay(150);                                           // for a bit,
            OF_RGB::LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE

        return;
    }
}

// Pause mode autofire factor toggle widget
// Does a test fire demonstrating the autofire speed being toggled
void AutofireSpeedToggle()
{
    switch (SamcoPreferences::settings[OF_Const::autofireWaitFactor]) {
        case 2:
            SamcoPreferences::settings[OF_Const::autofireWaitFactor] = 3;
            Serial.println("Autofire speed level 2.");
            break;
        case 3:
            SamcoPreferences::settings[OF_Const::autofireWaitFactor] = 4;
            Serial.println("Autofire speed level 3.");
            break;
        case 4:
            SamcoPreferences::settings[OF_Const::autofireWaitFactor] = 2;
            Serial.println("Autofire speed level 1.");
            break;
    }
    #ifdef LED_ENABLE
        OF_RGB::SetLedPackedColor(WikiColor::Magenta);                    // Set a color,
    #endif // LED_ENABLE

    #ifdef USES_SOLENOID
        for(byte i = 0; i < 5; i++) {                             // And demonstrate the new autofire factor five times!
            digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH);
            delay(SamcoPreferences::settings[OF_Const::solenoidFastInterval]);
            digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);
            delay(SamcoPreferences::settings[OF_Const::solenoidFastInterval] * SamcoPreferences::settings[OF_Const::autofireWaitFactor]);
        }
    #endif // USES_SOLENOID

    #ifdef LED_ENABLE
        OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);    // And reset the LED back to pause mode color
    #endif // LED_ENABLE
}

/*
// Unused since runtime burst fire toggle was removed from pause mode, and is accessed from the serial command M8x1
void BurstFireToggle()
{
    burstFireActive = !burstFireActive;                           // Toggle burst fire mode.
    if(burstFireActive) {  // Did we flick it on?
        Serial.println("Burst firing enabled!");
        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Orange);
        #endif
        #ifdef USES_SOLENOID
            for(byte i = 0; i < 4; i++) {
                digitalWrite(solenoidPin, HIGH);                  // Demonstrate it by flicking the solenoid on/off three times!
                delay(SamcoPreferences::settings[OF_Const::solenoidFastInterval]);                      // (at a fixed rate to distinguish it from autofire speed toggles)
                digitalWrite(solenoidPin, LOW);
                delay(SamcoPreferences::settings[OF_Const::solenoidFastInterval] * 2);
            }
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    } else {  // Or we flicked it off.
        Serial.println("Burst firing disabled!");
        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Orange);
        #endif // LED_ENABLE
        #ifdef USES_SOLENOID
            digitalWrite(solenoidPin, HIGH);                      // Just hold it on for a second.
            delay(300);
            digitalWrite(solenoidPin, LOW);                       // Then off.
        #endif // USES_SOLENOID
        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
        return;
    }
}
*/

#ifdef USES_RUMBLE
// Pause mode rumble enabling widget
// Does a cute rumble pattern when on, or blinks LEDs (if any)
void RumbleToggle()
{
    SamcoPreferences::toggles[OF_Const::rumble] = !SamcoPreferences::toggles[OF_Const::rumble];
    if(SamcoPreferences::toggles[OF_Const::rumble]) {
        if(!OF_Serial::serialMode) 
            Serial.println("Rumble enabled!");

        #ifdef USES_DISPLAY
            FW_Common::OLED.TopPanelUpdate("Toggling Rumble ON");
        #endif // USES_DISPLAY

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Salmon);
        #endif // LED_ENABLE

        digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], HIGH);       // Pulse the motor on to notify the user,
        delay(300);                                               // Hold that,
        digitalWrite(SamcoPreferences::pins[OF_Const::rumblePin], LOW);        // Then turn off,

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
    } else {                                                      // Or if we're turning it OFF,
        if(!OF_Serial::serialMode) 
            Serial.println("Rumble disabled!");

        #ifdef USES_DISPLAY
            FW_Common::OLED.TopPanelUpdate("Toggling Rumble OFF");
        #endif // USES_DISPLAY

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Salmon);                 // Set a color,
            delay(150);                                           // Keep it on,
            OF_RGB::LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            OF_RGB::SetLedPackedColor(WikiColor::Salmon);                 // Flicker it back on
            delay(150);                                           // for a bit,
            OF_RGB::LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
    }

    #ifdef USES_DISPLAY
        FW_Common::OLED.TopPanelUpdate("Using ", FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
    #endif // USES_DISPLAY
}
#endif // USES_RUMBLE

#ifdef USES_SOLENOID
// Pause mode solenoid enabling widget
// Does a cute solenoid engagement, or blinks LEDs (if any)
void SolenoidToggle()
{
    SamcoPreferences::toggles[OF_Const::solenoid] = !SamcoPreferences::toggles[OF_Const::solenoid];                             // Toggle
    if(SamcoPreferences::toggles[OF_Const::solenoid]) {                                          // If we turned ON this mode,
        if(!OF_Serial::serialMode)
            Serial.println("Solenoid enabled!");

        #ifdef USES_DISPLAY
            FW_Common::OLED.TopPanelUpdate("Toggling Solenoid ON");
        #endif // USES_DISPLAY

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
        #endif // LED_ENABLE

        digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], HIGH);                          // Engage the solenoid on to notify the user,
        delay(300);                                               // Hold it that way for a bit,
        digitalWrite(SamcoPreferences::pins[OF_Const::solenoidPin], LOW);                           // Release it,

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);    // And reset the LED back to pause mode color
        #endif // LED_ENABLE

    } else {                                                      // Or if we're turning it OFF,
        if(!OF_Serial::serialMode)
            Serial.println("Solenoid disabled!");

        #ifdef USES_DISPLAY
            FW_Common::OLED.TopPanelUpdate("Toggling Solenoid OFF");
        #endif // USES_DISPLAY

        #ifdef LED_ENABLE
            OF_RGB::SetLedPackedColor(WikiColor::Yellow);                 // Set a color,
            delay(150);                                           // Keep it on,
            OF_RGB::LedOff();                                             // Flicker it off
            delay(100);                                           // for a bit,
            OF_RGB::SetLedPackedColor(WikiColor::Yellow);                 // Flicker it back on
            delay(150);                                           // for a bit,
            OF_RGB::LedOff();                                             // And turn it back off
            delay(200);                                           // for a bit,
            OF_RGB::SetLedPackedColor(FW_Common::profileData[FW_Common::profiles.selectedProfile].color);// And reset the LED back to pause mode color
        #endif // LED_ENABLE
    }

    #ifdef USES_DISPLAY
        FW_Common::OLED.TopPanelUpdate("Using ", FW_Common::profileData[FW_Common::profiles.selectedProfile].name);
    #endif // USES_DISPLAY
}
#endif // USES_SOLENOID