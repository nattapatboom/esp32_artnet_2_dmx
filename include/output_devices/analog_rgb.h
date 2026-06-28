#ifndef OUTPUT_DEVICES_ANALOG_RGB_H
#define OUTPUT_DEVICES_ANALOG_RGB_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void analogRgbSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    ch.dmxPort = 255;
    ch.ledc_chan2 = 255;
    ch.ledc_chan3 = 255;
    ch.ledc_chan4 = 255;

    uint8_t res = ledcResolution(ch);
    if (ch.routes[0].source == 0 && ch.routes[0].pin != 255) {
        uint8_t rChan = allocateLedc(ledcIdx);
        if (rChan != 255) {
            ledcSetup(rChan, ch.mc_freq, res);
            ledcAttachPin(ch.routes[0].pin, rChan);
            ledcWrite(rChan, 0);
            ch.dmxPort = rChan;
        }
    }
    if (ch.routes[1].source == 0 && ch.routes[1].pin != 255) {
        uint8_t gChan = allocateLedc(ledcIdx);
        if (gChan != 255) {
            ledcSetup(gChan, ch.mc_freq, res);
            ledcAttachPin(ch.routes[1].pin, gChan);
            ledcWrite(gChan, 0);
            ch.ledc_chan2 = gChan;
        }
    }
    if (ch.routes[2].source == 0 && ch.routes[2].pin != 255) {
        uint8_t bChan = allocateLedc(ledcIdx);
        if (bChan != 255) {
            ledcSetup(bChan, ch.mc_freq, res);
            ledcAttachPin(ch.routes[2].pin, bChan);
            ledcWrite(bChan, 0);
            ch.ledc_chan3 = bChan;
        }
    }
    if (ch.color_order >= 4 && ch.routes[3].source == 0 && ch.routes[3].pin != 255) {
        uint8_t wChan = allocateLedc(ledcIdx);
        if (wChan != 255) {
            ledcSetup(wChan, ch.mc_freq, res);
            ledcAttachPin(ch.routes[3].pin, wChan);
            ledcWrite(wChan, 0);
            ch.ledc_chan4 = wChan;
        }
    }
}

inline void analogRgbUpdate(OutputChannel& ch) {
    uint16_t r = ch.dmxBuffer[0];
    uint16_t g = ch.dmxBuffer[1];
    uint16_t b = ch.dmxBuffer[2];
    uint16_t w = (ch.color_order >= 4) ? ch.dmxBuffer[3] : 0;
    uint32_t max_val = getMaxValue(ch.mc_resolution);

    if (ch.routes[0].source == 0) {
        if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, (r * max_val) / 255);
    } else if (OutputDefs::isPwmExpanderSource(ch.routes[0].source)) {
        pwmExpanderManager.write(ch.routes[0].addr, ch.routes[0].channel, (r * 4095) / 255, false, ch.routes[0].source);
    }

    if (ch.routes[1].source == 0) {
        if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, (g * max_val) / 255);
    } else if (OutputDefs::isPwmExpanderSource(ch.routes[1].source)) {
        pwmExpanderManager.write(ch.routes[1].addr, ch.routes[1].channel, (g * 4095) / 255, false, ch.routes[1].source);
    }

    if (ch.routes[2].source == 0) {
        if (ch.ledc_chan3 != 255) ledcWrite(ch.ledc_chan3, (b * max_val) / 255);
    } else if (OutputDefs::isPwmExpanderSource(ch.routes[2].source)) {
        pwmExpanderManager.write(ch.routes[2].addr, ch.routes[2].channel, (b * 4095) / 255, false, ch.routes[2].source);
    }

    if (ch.color_order >= 4) {
        if (ch.routes[3].source == 0) {
            if (ch.ledc_chan4 != 255) ledcWrite(ch.ledc_chan4, (w * max_val) / 255);
        } else if (OutputDefs::isPwmExpanderSource(ch.routes[3].source)) {
            pwmExpanderManager.write(ch.routes[3].addr, ch.routes[3].channel, (w * 4095) / 255, false, ch.routes[3].source);
        }
    }
}

#endif
