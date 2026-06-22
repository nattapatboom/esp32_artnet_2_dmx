#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <Arduino.h>
#include "output_control.h"
#include "i2c_devices/i2c_dac.h"
#include <FastAccelStepper.h>

#include "output_devices/ledc_helpers.h"
#include "output_devices/single_led.h"
#include "output_devices/pwm_dac.h"
#include "output_devices/servo.h"
#include "output_devices/motor.h"
#include "output_devices/analog_rgb.h"
#include "output_devices/stepper.h"
#include "output_devices/buzzer.h"
#include "output_devices/seven_seg.h"
#include "output_devices/dfplayer.h"
#include "output_devices/dac.h"
#include "output_devices/funcgen.h"

class MotionControl {
private:
    FastAccelStepperEngine engine;
    FastAccelStepper* steppers[8] = {nullptr};
    uint8_t stepperCount = 0;
    uint8_t ledcChannelIndex = 0;

public:
    void begin() {
        ledcChannelIndex = 0;
        stepperCount = 0;
        engine.init();

        for (auto& ch : outputCtrl.getChannels()) {
            switch (ch.type) {
                case OutputDefs::TYPE_SINGLE_LED:
                    singleLedSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_PWM_DAC:
                    pwmDacSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_SERVO:
                    servoSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_MOTOR:
                    motorSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_ANALOG_RGB:
                    analogRgbSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_STEPPER:
                    stepperSetup(ch, engine, steppers, stepperCount);
                    break;
                case OutputDefs::TYPE_BUZZER:
                    buzzerSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_TM1637:
                case OutputDefs::TYPE_7SEG_7PIN:
                case OutputDefs::TYPE_7SEG_8PIN:
                    sevenSegSetup(ch, ledcChannelIndex);
                    break;
                case OutputDefs::TYPE_FUNC_GEN:
                    funcGenSetup(ch, ledcChannelIndex);
                    break;
                default:
                    break;
            }
        }
    }

    void update() {
        for (auto& ch : outputCtrl.getChannels()) {
            if (ch.dmxBuffer == nullptr) continue;

            switch (ch.type) {
                case OutputDefs::TYPE_ANALOG_RGB:
                    analogRgbUpdate(ch);
                    break;
                case OutputDefs::TYPE_SINGLE_LED:
                    singleLedUpdate(ch);
                    break;
                case OutputDefs::TYPE_PWM_DAC:
                    pwmDacUpdate(ch);
                    break;
                case OutputDefs::TYPE_SERVO:
                    servoUpdate(ch);
                    break;
                case OutputDefs::TYPE_MOTOR:
                    motorUpdate(ch);
                    break;
                case OutputDefs::TYPE_STEPPER:
                    stepperUpdate(ch, steppers, stepperCount);
                    break;
                case OutputDefs::TYPE_BUZZER:
                    buzzerUpdate(ch);
                    break;
                case OutputDefs::TYPE_TM1637:
                case OutputDefs::TYPE_7SEG_7PIN:
                case OutputDefs::TYPE_7SEG_8PIN:
                    sevenSegUpdate(ch);
                    break;
                case OutputDefs::TYPE_DFPLAYER:
                    dfPlayerUpdate(ch);
                    break;
                case OutputDefs::TYPE_DAC:
                    dacUpdate(ch);
                    break;
                case OutputDefs::TYPE_FUNC_GEN:
                    funcGenUpdate(ch);
                    break;
                default:
                    break;
            }
        }
    }
};

extern MotionControl motionCtrl;

#endif
