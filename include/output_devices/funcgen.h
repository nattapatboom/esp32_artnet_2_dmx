#ifndef OUTPUT_DEVICES_FUNCGEN_H
#define OUTPUT_DEVICES_FUNCGEN_H

#include <Arduino.h>
#include <new>
#include "output_control.h"
#include "funcgen_control.h"
#include "ledc_helpers.h"

inline void funcGenSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    if (ch.routes[0].pin == 255) return;
    uint8_t pwmChan = allocateLedc(ledcIdx);
    if (pwmChan != 255) {
        ch.funcGen = new (std::nothrow) FuncGenController();
        if (ch.funcGen == nullptr) {
            Serial.println("Function Generator allocation failed");
            return;
        }
        ch.funcGen->begin(ch.routes[0].pin, pwmChan, ch.mc_freq ? ch.mc_freq : 50000);
        ch.dmxPort = pwmChan;
    }
}

inline void funcGenUpdate(OutputChannel& ch) {
    if (ch.funcGen != nullptr && ch.dmxBuffer != nullptr) {
        uint16_t freq = ch.dmxBuffer[0] | (ch.dmxBuffer[1] << 8);
        uint8_t wfType = ch.dmxBuffer[2];
        uint8_t amp = ch.dmxBuffer[3];
        uint8_t offset = ch.dmxBuffer[4];
        ch.funcGen->update(wfType, freq, amp, offset);
    }
}

#endif
