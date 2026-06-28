#ifndef OUTPUT_DEVICES_SERVO_H
#define OUTPUT_DEVICES_SERVO_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void servoSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    if (ch.routes[0].pin == 255) return;
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ledcSetup(pwmChan, 50, 16);
        ledcAttachPin(ch.routes[0].pin, pwmChan);
        ledcWrite(pwmChan, 0);
        ch.dmxPort = pwmChan;
    }
}

inline void servoUpdate(OutputChannel& ch) {
    uint32_t max_val = getMaxValue(ch.mc_resolution);
    if (max_val == 0) return;
    if (ch.mc_max_us <= ch.mc_min_us) return;
    uint32_t val = getDmxValue(ch);
    uint32_t pulse_us = ch.mc_min_us + (val * (ch.mc_max_us - ch.mc_min_us)) / max_val;
    if (OutputDefs::isPwmExpanderSource(ch.routes[0].source)) {
        uint32_t ticks = (pulse_us * 4096) / 20000;
        pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, ticks > 4095 ? 4095 : ticks, false, ch.routes[0].source);
    } else if (ch.dmxPort != 255) {
        uint32_t duty = (pulse_us * 65535) / 20000;
        ledcWrite(ch.dmxPort, duty);
    }
}

#endif
