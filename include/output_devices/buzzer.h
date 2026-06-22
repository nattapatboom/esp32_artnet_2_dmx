#ifndef OUTPUT_DEVICES_BUZZER_H
#define OUTPUT_DEVICES_BUZZER_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void buzzerSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ledcSetup(pwmChan, 1000, 8);
        ledcAttachPin(ch.pin, pwmChan);
        ledcWrite(pwmChan, 0);
        ch.dmxPort = pwmChan;
    }
}

inline void buzzerUpdate(OutputChannel& ch) {
    if (ch.dmxPort != 255) {
        uint8_t freqDmx = ch.dmxBuffer[0];
        uint8_t volDmx = ch.dmxBuffer[1];
        if (freqDmx == 0 || volDmx == 0) {
            ledcWrite(ch.dmxPort, 0);
        } else {
            uint32_t freq = 100 + (freqDmx - 1) * (5000 - 100) / 254;
            uint32_t duty = (volDmx * 128) / 255;
            ledcChangeFrequency(ch.dmxPort, freq, 8);
            ledcWrite(ch.dmxPort, duty);
        }
    }
}

#endif
