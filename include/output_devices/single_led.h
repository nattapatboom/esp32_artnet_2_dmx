#ifndef OUTPUT_DEVICES_SINGLE_LED_H
#define OUTPUT_DEVICES_SINGLE_LED_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void singleLedSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
        ledcAttachPin(ch.pin, pwmChan);
        ledcWrite(pwmChan, 0);
        ch.dmxPort = pwmChan;
    }
}

inline void singleLedUpdate(OutputChannel& ch) {
    if (ch.dmxPort != 255) {
        uint32_t max_val = getMaxValue(ch.mc_resolution);
        if (max_val == 0) return;
        uint32_t val = getDmxValue(ch);
        if (ch.source == 1) {
            uint16_t duty = (uint32_t)((uint64_t)val * 4095) / max_val;
            pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
        } else {
            ledcWrite(ch.dmxPort, val);
        }
    }
}

#endif
