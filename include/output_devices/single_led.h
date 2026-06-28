#ifndef OUTPUT_DEVICES_SINGLE_LED_H
#define OUTPUT_DEVICES_SINGLE_LED_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void singleLedSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    if (ch.routes[0].source == 1) {
        pcaManager.getOrCreateDriver(ch.routes[0].addr);
        pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, 0);
        return;
    }
    if (ch.routes[0].pin == 255) return;
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
        ledcAttachPin(ch.routes[0].pin, pwmChan);
        ledcWrite(pwmChan, 0);
        ch.dmxPort = pwmChan;
    }
}

inline void singleLedUpdate(OutputChannel& ch) {
    uint32_t max_val = getMaxValue(ch.mc_resolution);
    if (max_val == 0) return;
    uint32_t val = getDmxValue(ch);
    if (ch.routes[0].source == 1) {
        uint16_t duty = (uint32_t)((uint64_t)val * 4095) / max_val;
        pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, duty);
    } else if (ch.dmxPort != 255) {
        ledcWrite(ch.dmxPort, val);
    }
}

#endif
