#ifndef OUTPUT_DEVICES_SEVEN_SEG_H
#define OUTPUT_DEVICES_SEVEN_SEG_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"
#include "tm1637.h"

inline void sevenSegSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    int8_t mode = (int8_t)ch.mc_mode;
    const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    bool isDirectDrive = def != nullptr && def->segmentCount > 0;
    bool isCommonDim = (mode == 2 || mode == 3);
    bool isCA = (mode == 0 || mode == 2);
    uint8_t numSeg = def != nullptr ? def->segmentCount : 0;

    if (isDirectDrive) {
        if (isCommonDim && ch.routes[0].source == 0 && ch.routes[0].pin != 255) {
            uint8_t pwmChan = allocateLedc(ledcIdx);
            if (pwmChan != 255) {
                ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                ledcAttachPin(ch.routes[0].pin, pwmChan);
                ledcWrite(pwmChan, isCA ? 0 : 255);
                ch.dmxPort = pwmChan;
            }
        }

        if (isCommonDim) {
            for (uint8_t s = 0; s < numSeg; s++) {
                setupSegmentOutput(ch, s, isCA);
            }
        } else {
            uint8_t baseChan = allocateLedc(ledcIdx);
            bool useLedc = (baseChan != 255);
            uint8_t usedLedc = 0;

            for (uint8_t s = 0; s < numSeg; s++) {
                uint8_t routeIdx = segmentRouteIndex(ch, s);
                if (useLedc && ch.routes[routeIdx].source == 0 && ch.routes[routeIdx].pin != 255 && baseChan + usedLedc <= 15) {
                    ledcSetup(baseChan + usedLedc, ch.mc_freq ? ch.mc_freq : 1000, 8);
                    ledcAttachPin(ch.routes[routeIdx].pin, baseChan + usedLedc);
                    ledcWrite(baseChan + usedLedc, 0);
                    usedLedc++;
                } else {
                    setupSegmentOutput(ch, s, false);
                }
            }
            if (useLedc) {
                ledcIdx = baseChan + usedLedc;
                ch.dmxPort = baseChan;
            }
        }
        ch.prev_7seg_val = 0;
        ch.prev_7seg_valid = false;
    }
}

inline void sevenSegUpdate(OutputChannel& ch) {
    int8_t mode = (int8_t)ch.mc_mode;
    const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    bool isDirectDrive = def != nullptr && def->segmentCount > 0;
    uint8_t bytes = 0;
    if (isDirectDrive) {
        bytes = (mode >= 0) ? 2 : 1;
    } else {
        bytes = (mode == 1) ? 4 : 2;
    }
    uint32_t val = 0;
    for (uint8_t i = 0; i < bytes; i++) {
        val = (val << 8) | ch.dmxBuffer[i];
    }
    if (ch.prev_7seg_valid && val == ch.prev_7seg_val) return;

    ch.prev_7seg_val = val;
    ch.prev_7seg_valid = true;

    bool isCommonDim = (mode == 2 || mode == 3);
    bool isCA = (mode == 0 || mode == 2);
    uint8_t numSeg = def != nullptr ? def->segmentCount : 0;
    uint8_t segByte = 0;
    if (ch.mc_resolution == 10) {
        uint8_t raw = ch.dmxBuffer[0];
        if (raw <= 9) {
            segByte = SEG_DIGITS[raw];
        } else if (raw >= 10 && raw <= 19) {
            segByte = SEG_DIGITS[raw - 10] | 0x80;
        }
    } else {
        segByte = asciiToSegment(ch.dmxBuffer[0]);
    }

    if (isDirectDrive) {
        if (isCommonDim) {
            uint8_t brightness = ch.dmxBuffer[1];
            uint8_t duty = isCA ? brightness : (255 - brightness);
            if (OutputDefs::isPwmExpanderSource(ch.routes[0].source)) {
                pwmExpanderManager.write(ch.routes[0].addr, ch.routes[0].channel, (duty * 4095) / 255, false, ch.routes[0].source);
            } else if (ch.routes[0].source == 0 && ch.dmxPort != 255) {
                ledcWrite(ch.dmxPort, duty);
            }

            for (uint8_t b = 0; b < numSeg; b++) {
                bool bitVal = (segByte >> b) & 1;
                writeSegmentOutput(ch, b, isCA ? !bitVal : bitVal);
            }
        } else {
            uint8_t brightness = 255;
            if (mode == 0 || mode == 1) {
                brightness = ch.dmxBuffer[1];
            }
            for (uint8_t b = 0; b < numSeg; b++) {
                uint8_t routeIdx = segmentRouteIndex(ch, b);
                uint32_t duty = ((segByte >> b) & 1) ? brightness : 0;

                if (ch.routes[routeIdx].source != 0) {
                    writeSegmentOutput(ch, b, duty > 0);
                } else if (ch.dmxPort != 255 && ch.dmxPort + b <= 15) {
                    bool inv = ch.routes[routeIdx].invert;
                    uint32_t active_duty = inv ? (brightness - duty) : duty;
                    ledcWrite(ch.dmxPort + b, active_duty);
                }
            }
        }
    } else {
        TM1637Driver tm(ch.routes[0].pin, ch.routes[1].pin);
        if (mode == 1) {
            uint8_t segments[4];
            for (uint8_t i = 0; i < 4; i++) {
                segments[i] = asciiToSegment(ch.dmxBuffer[i]);
            }
            tm.displayRaw(segments);
        } else {
            tm.displayNum((uint16_t)val);
        }
    }
}

#endif
