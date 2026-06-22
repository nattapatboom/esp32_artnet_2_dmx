#ifndef OUTPUT_DEVICES_SERVO_H
#define OUTPUT_DEVICES_SERVO_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void servoSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ledcSetup(pwmChan, 50, 16);
        ledcAttachPin(ch.pin, pwmChan);
        ledcWrite(pwmChan, 0);
        ch.dmxPort = pwmChan;
    }
}

inline void servoUpdate(OutputChannel& ch) {
    uint32_t max_val = getMaxValue(ch.mc_resolution);
    uint32_t val = getDmxValue(ch);
    uint32_t pulse_us = ch.mc_min_us + (val * (ch.mc_max_us - ch.mc_min_us)) / max_val;
    if (ch.source == 1) {
        uint32_t ticks = (pulse_us * 4096) / 20000;
        pcaManager.write(ch.pca_addr, ch.pca_channel, ticks > 4095 ? 4095 : ticks);
    } else if (ch.dmxPort != 255) {
        uint32_t duty = (pulse_us * 65535) / 20000;
        ledcWrite(ch.dmxPort, duty);
    }
}

#endif
