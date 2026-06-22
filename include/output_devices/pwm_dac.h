#ifndef OUTPUT_DEVICES_PWM_DAC_H
#define OUTPUT_DEVICES_PWM_DAC_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void pwmDacSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
        ledcAttachPin(ch.pin, pwmChan);
        ledcWrite(pwmChan, 0);
        ch.dmxPort = pwmChan;
    }
}

inline void pwmDacUpdate(OutputChannel& ch) {
    uint32_t max_val = getMaxValue(ch.mc_resolution);
    uint32_t val = getDmxValue(ch);
    if (ch.source == 1) {
        uint16_t duty = calibratedPwmDacDuty(ch, val, max_val, 4095);
        pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
    } else if (ch.dmxPort != 255) {
        ledcWrite(ch.dmxPort, calibratedPwmDacDuty(ch, val, max_val, max_val));
    }
}

#endif
