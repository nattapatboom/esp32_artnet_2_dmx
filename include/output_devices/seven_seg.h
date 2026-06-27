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

    if (isDirectDrive && ch.pin2_source == 1) {
        pcaManager.getOrCreateDriver(ch.pin2_addr);
        pcaManager.setFrequency(ch.pin2_addr, outputCtrl.sharedPcaFrequency(ch.pin2_addr));
        for (uint8_t s = 0; s < numSeg; s++) {
            pcaManager.write(ch.pin2_addr, ch.pin2_channel + s, 0);
        }
        if (isCommonDim && ch.source == 0) {
            uint8_t pwmChan = allocateLedc(ledcIdx);
            if (pwmChan != 255) {
                ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                ledcAttachPin(ch.pin, pwmChan);
                ledcWrite(pwmChan, isCA ? 0 : 255);
                ch.dmxPort = pwmChan;
            }
        }
        ch.prev_7seg_val = 0;
        ch.prev_7seg_valid = false;
    } else if (isDirectDrive && ch.pin2_source >= 2 && ch.pin2_source <= 4) {
        for (uint8_t s = 0; s < numSeg; s++) {
            digitalExpanderManager.write(ch.pin2_source, ch.pin2_addr, ch.pin2_channel + s, false, true);
        }
        if (isCommonDim && ch.source == 0) {
            uint8_t pwmChan = allocateLedc(ledcIdx);
            if (pwmChan != 255) {
                ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                ledcAttachPin(ch.pin, pwmChan);
                ledcWrite(pwmChan, isCA ? 0 : 255);
                ch.dmxPort = pwmChan;
            }
        }
        ch.prev_7seg_val = 0;
        ch.prev_7seg_valid = false;
    } else if (isDirectDrive && ch.pin2_source == 0) {
        if (isCommonDim) {
            if (ch.source == 0) {
                uint8_t pwmChan = allocateLedc(ledcIdx);
                if (pwmChan != 255) {
                    ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                    ledcAttachPin(ch.pin, pwmChan);
                    ledcWrite(pwmChan, isCA ? 0 : 255);
                    ch.dmxPort = pwmChan;
                }
            }
            for (uint8_t s = 0; s < numSeg; s++) {
                setupSegmentOutput(ch, s, isCA);
            }
        } else {
            if (isDirectDrive) {
                uint8_t baseChan = allocateLedc(ledcIdx);
                if (baseChan != 255) {
                    uint8_t usedLedc = 0;
                    for (uint8_t s = 0; s < numSeg; s++) {
                        uint8_t segPin = segmentGpio(ch, s);
                        if (ch.seg_sources[s] == 0 && segPin != 255 && baseChan + usedLedc <= 15) {
                            ledcSetup(baseChan + usedLedc, ch.mc_freq ? ch.mc_freq : 1000, 8);
                            ledcAttachPin(segPin, baseChan + usedLedc);
                            ledcWrite(baseChan + usedLedc, 0);
                            usedLedc++;
                        } else if (ch.seg_sources[s] >= 1 && ch.seg_sources[s] <= 4) {
                            setupSegmentOutput(ch, s, false);
                        }
                    }
                    ledcIdx = baseChan + usedLedc;
                    ch.dmxPort = baseChan;
                }
            } else {
                for (uint8_t s = 0; s < numSeg; s++) {
                    setupSegmentOutput(ch, s, false);
                }
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

    if (isDirectDrive && ch.pin2_source == 1) {
        for (uint8_t b = 0; b < numSeg; b++) {
            bool bitVal = (segByte >> b) & 1;
            if (isCommonDim) bitVal = isCA ? !bitVal : bitVal;
            bool inv = (ch.seg_inverts >> b) & 1;
            pcaManager.write(ch.pin2_addr, ch.pin2_channel + b, (bitVal ^ inv) ? 4095 : 0);
        }
        if (isCommonDim) {
            uint8_t brightness = ch.dmxBuffer[1];
            uint8_t duty = isCA ? brightness : (255 - brightness);
            if (ch.source == 1) {
                pcaManager.write(ch.pca_addr, ch.pca_channel, (duty * 4095) / 255);
            } else if (ch.source == 0 && ch.dmxPort != 255) {
                ledcWrite(ch.dmxPort, duty);
            }
        }
    } else if (isDirectDrive && ch.pin2_source >= 2 && ch.pin2_source <= 4) {
        for (uint8_t b = 0; b < numSeg; b++) {
            bool bitVal = (segByte >> b) & 1;
            if (isCommonDim) bitVal = isCA ? !bitVal : bitVal;
            bool inv = (ch.seg_inverts >> b) & 1;
            digitalExpanderManager.write(ch.pin2_source, ch.pin2_addr, ch.pin2_channel + b, bitVal ^ inv);
        }
        if (isCommonDim) {
            uint8_t brightness = ch.dmxBuffer[1];
            uint8_t duty = isCA ? brightness : (255 - brightness);
            if (ch.source == 1) {
                pcaManager.write(ch.pca_addr, ch.pca_channel, (duty * 4095) / 255);
            } else if (ch.source == 0 && ch.dmxPort != 255) {
                ledcWrite(ch.dmxPort, duty);
            }
        }
    } else if (isDirectDrive && ch.pin2_source == 0) {
        if (isCommonDim) {
            uint8_t brightness = ch.dmxBuffer[1];
            uint8_t duty = isCA ? brightness : (255 - brightness);
            if (ch.source == 1) {
                pcaManager.write(ch.pca_addr, ch.pca_channel, (duty * 4095) / 255);
            } else if (ch.source == 0 && ch.dmxPort != 255) {
                ledcWrite(ch.dmxPort, duty);
            }
            for (uint8_t b = 0; b < numSeg; b++) {
                bool bitVal = (segByte >> b) & 1;
                writeSegmentOutput(ch, b, isCA ? !bitVal : bitVal);
            }
        } else {
            if (isDirectDrive) {
                uint8_t brightness = 255;
                if (mode == 0 || mode == 1) {
                    brightness = ch.dmxBuffer[1];
                }
                for (uint8_t b = 0; b < numSeg; b++) {
                    uint32_t duty = ((segByte >> b) & 1) ? brightness : 0;
                    if (ch.seg_sources[b] >= 1 && ch.seg_sources[b] <= 4) {
                        writeSegmentOutput(ch, b, duty > 0);
                    } else if (ch.dmxPort != 255 && ch.dmxPort + b <= 15) {
                        bool inv = (ch.seg_inverts >> b) & 1;
                        uint32_t active_duty = inv ? (brightness - duty) : duty;
                        ledcWrite(ch.dmxPort + b, active_duty);
                    }
                }
            } else {
                for (uint8_t b = 0; b < numSeg; b++) {
                    writeSegmentOutput(ch, b, (segByte >> b) & 1);
                }
            }
        }
    } else {
        TM1637Driver tm(ch.pin, ch.pin2);
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
