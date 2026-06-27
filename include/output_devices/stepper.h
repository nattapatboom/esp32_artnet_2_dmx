#ifndef OUTPUT_DEVICES_STEPPER_H
#define OUTPUT_DEVICES_STEPPER_H

#include <Arduino.h>
#include <FastAccelStepper.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void setStepperDirection(OutputChannel& ch, bool forward) {
    if (ch.pin2_source == 0) return;
    writeOutputPin(ch, 2, forward);
}


inline void stepperSetup(OutputChannel& ch, FastAccelStepperEngine& engine, FastAccelStepper** steppers, uint8_t& stepperCount) {
    if (ch.source != 0) return;
    if (ch.pin == 255) return;
    if (stepperCount >= 8) return;

    FastAccelStepper* stepper = engine.stepperConnectToPin(ch.pin);
    if (stepper) {
        if (ch.pin2_source == 0 && ch.pin2 != 255) {
            stepper->setDirectionPin(ch.pin2, ch.pin2_invert);
        } else {
            setStepperDirection(ch, true);
        }
        if (ch.pin3_source == 0 && ch.pin3 != 255) {
            stepper->setEnablePin(ch.pin3, !ch.pin3_invert);
            stepper->setAutoEnable(true);
        } else if (ch.pin3_source != 0) {
            writeOutputPin(ch, 3, true);
        }
        stepper->setSpeedInHz(ch.mc_freq);
        stepper->setAcceleration(ch.mc_freq * 2);

        steppers[stepperCount] = stepper;
        ch.dmxPort = stepperCount;
        stepperCount++;
    }
}

inline void stepperUpdate(OutputChannel& ch, FastAccelStepper** steppers, uint8_t stepperCount) {
    if (ch.dmxPort >= stepperCount) return;
    FastAccelStepper* stepper = steppers[ch.dmxPort];
    if (!stepper) return;

    uint8_t pos_bytes = getValueByteCount(ch.mc_resolution);
    uint32_t speed_val = ch.dmxBuffer[pos_bytes];
    uint8_t cmd_val = ch.dmxBuffer[pos_bytes + 1];

    uint32_t target_speed = (speed_val * ch.mc_freq) / 255;
    if (target_speed < 50) target_speed = 50;

    unsigned long now = millis();

    if (cmd_val >= 100 && cmd_val <= 199) {
        if (ch.stepper_cmd_state != 2) {
            if (ch.stepper_cmd_state != 1) {
                ch.stepper_cmd_state = 1;
                ch.stepper_cmd_start_time = now;
            } else if (now - ch.stepper_cmd_start_time >= 1000) {
                stepper->stopMove();
                ch.stepper_cmd_state = 2;
            }
        }
    } else if (cmd_val >= 200 && cmd_val <= 255) {
        if (ch.stepper_cmd_state != 4 && ch.stepper_cmd_state != 5) {
            if (ch.stepper_cmd_state != 3) {
                ch.stepper_cmd_state = 3;
                ch.stepper_cmd_start_time = now;
            } else if (now - ch.stepper_cmd_start_time >= 1000) {
                ch.stepper_cmd_state = 4;
                ch.stepper_homing_start_time = now;

                uint32_t h_speed = ch.mc_homing_speed;
                if (h_speed == 0) h_speed = ch.mc_freq / 2;
                stepper->setSpeedInHz(h_speed);

                if (ch.mc_homing_mode == 0 && ch.pin4 != 255) {
                    if (ch.pin4_source == 0) {
                        pinMode(ch.pin4, INPUT_PULLUP);
                    } else {
                        digitalExpanderManager.configureInput(ch.pin4_source, ch.pin4_addr, ch.pin4_channel, true);
                    }
                }

                if (ch.mc_homing_dir == 0) {
                    setStepperDirection(ch, true);
                    stepper->runForward();
                } else {
                    setStepperDirection(ch, false);
                    stepper->runBackward();
                }
            }
        }
    } else {
        if (ch.stepper_cmd_state != 0) {
            ch.stepper_cmd_state = 0;
            stepper->setSpeedInHz(target_speed);
        }
    }

    if (ch.stepper_cmd_state == 4) {
        bool zero_reached = false;
        if (ch.mc_homing_mode == 0) {
            if (ch.pin4 != 255 && readOutputPin(ch, 4) == false) {
                zero_reached = true;
            }
        } else {
            if (now - ch.stepper_homing_start_time >= (unsigned long)ch.mc_homing_timeout * 1000) {
                zero_reached = true;
            }
        }

        if (zero_reached) {
            stepper->forceStopAndNewPosition(0);
            ch.stepper_cmd_state = 5;
            stepper->setSpeedInHz(target_speed);
        }
    }

    if (ch.stepper_cmd_state == 0) {
        stepper->setSpeedInHz(target_speed);
        uint32_t val = getDmxValue(ch);
        uint32_t max_val = getMaxValue(ch.mc_resolution);
        if (max_val == 0) return;
        int32_t targetPos;
        if (ch.mc_scale_factor > 0.0f) {
            targetPos = (int32_t)round(val * ch.mc_scale_factor);
        } else {
            targetPos = (int32_t)(((uint64_t)val * ch.mc_steps_per_rev) / max_val);
        }
        if (ch.mc_invert) targetPos = -targetPos;
        if (ch.pin2_source != 0) {
            setStepperDirection(ch, targetPos >= stepper->getCurrentPosition());
        }
        writeOutputPin(ch, 3, true);
        stepper->moveTo(targetPos);
    }
}

#endif
