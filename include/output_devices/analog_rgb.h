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

    if (ch.source == 0 && ch.pin != 255) {
        uint8_t rChan = allocateLedc(ledcIdx);
        if (rChan != 255) {
            ledcSetup(rChan, ch.mc_freq, 8);
            ledcAttachPin(ch.pin, rChan);
            ledcWrite(rChan, 0);
            ch.dmxPort = rChan;
        }
    }
    if (ch.pin2_source == 0 && ch.pin2 != 255) {
        uint8_t gChan = allocateLedc(ledcIdx);
        if (gChan != 255) {
            ledcSetup(gChan, ch.mc_freq, 8);
            ledcAttachPin(ch.pin2, gChan);
            ledcWrite(gChan, 0);
            ch.ledc_chan2 = gChan;
        }
    }
    if (ch.pin3_source == 0 && ch.pin3 != 255) {
        uint8_t bChan = allocateLedc(ledcIdx);
        if (bChan != 255) {
            ledcSetup(bChan, ch.mc_freq, 8);
            ledcAttachPin(ch.pin3, bChan);
            ledcWrite(bChan, 0);
            ch.ledc_chan3 = bChan;
        }
    }
    if (ch.color_order >= 4 && ch.pin4_source == 0 && ch.pin4 != 255) {
        uint8_t wChan = allocateLedc(ledcIdx);
        if (wChan != 255) {
            ledcSetup(wChan, ch.mc_freq, 8);
            ledcAttachPin(ch.pin4, wChan);
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

    if (ch.source == 0) {
        if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, r);
    } else if (ch.source == 1) {
        pcaManager.write(ch.pca_addr, ch.pca_channel, (r * 4095) / 255);
    }

    if (ch.pin2_source == 0) {
        if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, g);
    } else if (ch.pin2_source == 1) {
        pcaManager.write(ch.pin2_addr, ch.pin2_channel, (g * 4095) / 255);
    }

    if (ch.pin3_source == 0) {
        if (ch.ledc_chan3 != 255) ledcWrite(ch.ledc_chan3, b);
    } else if (ch.pin3_source == 1) {
        pcaManager.write(ch.pin3_addr, ch.pin3_channel, (b * 4095) / 255);
    }

    if (ch.color_order >= 4) {
        if (ch.pin4_source == 0) {
            if (ch.ledc_chan4 != 255) ledcWrite(ch.ledc_chan4, w);
        } else if (ch.pin4_source == 1) {
            pcaManager.write(ch.pin4_addr, ch.pin4_channel, (w * 4095) / 255);
        }
    }
}

#endif
