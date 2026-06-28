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
    void begin();
    void update();
};

extern MotionControl motionCtrl;

#endif
