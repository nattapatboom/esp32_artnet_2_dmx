#ifndef OUTPUT_DEVICES_MOTOR_H
#define OUTPUT_DEVICES_MOTOR_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void motorSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    if (ch.pin == 255) return;
    if (ch.mc_mode == 0) {
        if (ch.source == 0) {
            uint8_t pwmFwd = allocateLedc(ledcIdx);
            uint8_t pwmRev = allocateLedc(ledcIdx);
            if (pwmFwd != 255 && pwmRev != 255) {
                ledcSetup(pwmFwd, ch.mc_freq, ledcResolution(ch));
                ledcSetup(pwmRev, ch.mc_freq, ledcResolution(ch));
                ledcAttachPin(ch.pin, pwmFwd);
                ledcAttachPin(ch.pin2, pwmRev);
                ledcWrite(pwmFwd, 0);
                ledcWrite(pwmRev, 0);
                ch.dmxPort = pwmFwd;
                ch.ledc_chan2 = pwmRev;
            }
        } else {
            ch.dmxPort = 255;
            ch.ledc_chan2 = 255;
            pcaManager.getOrCreateDriver(ch.pca_addr);
        }
    } else if (ch.mc_mode == 1) {
        if (ch.source == 0) {
            uint8_t pwmChan = allocateLedc(ledcIdx);
            if (pwmChan != 255) {
                ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                ledcAttachPin(ch.pin, pwmChan);
                if (ch.pin2_source == 0) {
                    pinMode(ch.pin2, OUTPUT);
                    digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
                } else {
                    writeOutputPin(ch, 2, false);
                }
                ledcWrite(pwmChan, 0);
                ch.dmxPort = pwmChan;
            }
        } else {
            ch.dmxPort = 255;
            if (ch.pin2_source == 0) {
                pinMode(ch.pin2, OUTPUT);
                digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
            } else {
                writeOutputPin(ch, 2, false);
            }
            pcaManager.getOrCreateDriver(ch.pca_addr);
            pcaManager.write(ch.pca_addr, ch.pca_channel, 0);
        }
    } else if (ch.mc_mode == 2) {
        uint8_t pwmChan = ch.pin3_source == 1 ? 255 : allocateLedc(ledcIdx);
        if (ch.pin3_source == 1 || pwmChan != 255) {
            if (ch.pin3_source == 1) {
                pcaManager.getOrCreateDriver(ch.pin3_addr);
                pcaManager.setFrequency(ch.pin3_addr, outputCtrl.sharedPcaFrequency(ch.pin3_addr));
                if (ch.pin3_channel != 255) pcaManager.write(ch.pin3_addr, ch.pin3_channel, 0);
            } else {
                ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                ledcAttachPin(ch.pin3, pwmChan);
                ledcWrite(pwmChan, 0);
                ch.dmxPort = pwmChan;
            }
            if (ch.source == 1) {
                writeOutputPin(ch, 1, false);
            } else {
                pinMode(ch.pin, OUTPUT);
                digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
            }
            if (ch.pin2_source == 0) {
                pinMode(ch.pin2, OUTPUT);
                digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
            } else {
                writeOutputPin(ch, 2, false);
            }
        }
    }
}

inline void motorUpdate(OutputChannel& ch) {
    uint32_t val = getDmxValue(ch);
    uint32_t max_val = getMaxValue(ch.mc_resolution);
    if (max_val == 0) return;
    int32_t center = max_val / 2;
    if (center == 0) center = 1;
    int32_t offset = (int32_t)val - center;
    bool is_forward = ch.mc_invert ? (offset < 0) : (offset > 0);
    uint32_t abs_offset = abs(offset);

    if (abs_offset < ch.mc_deadband) {
        abs_offset = 0;
    }

    if (ch.source == 1) {
        uint32_t duty = (abs_offset * 4095) / center;
        if (duty > 4095) duty = 4095;

        if (abs_offset == 0) {
            if (ch.mc_mode == 0) {
                pcaManager.write(ch.pca_addr, ch.pca_channel, ch.mc_brake ? 4095 : 0);
                if (ch.pin2_source == 1 && ch.pin2_channel != 255) {
                    pcaManager.write(ch.pin2_addr, ch.pin2_channel, ch.mc_brake ? 4095 : 0);
                } else if (ch.pin2_source == 0 && ch.pca_channel2 != 255) {
                    pcaManager.write(ch.pca_addr, ch.pca_channel2, ch.mc_brake ? 4095 : 0);
                } else {
                    writeOutputPin(ch, 2, ch.mc_brake);
                }
            } else if (ch.mc_mode == 1) {
                pcaManager.write(ch.pca_addr, ch.pca_channel, 0);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, ch.mc_brake);
                writeOutputPin(ch, 2, ch.mc_brake);
                if (ch.pin3_source == 1 && ch.pin3_channel != 255) {
                    pcaManager.write(ch.pin3_addr, ch.pin3_channel, ch.mc_brake ? 4095 : 0);
                } else if (ch.pin3_source == 0 && ch.pca_channel3 != 255) {
                    pcaManager.write(ch.pca_addr, ch.pca_channel3, ch.mc_brake ? 4095 : 0);
                } else {
                    writeOutputPin(ch, 3, ch.mc_brake);
                }
            }
        } else {
            if (ch.mc_mode == 0) {
                if (is_forward) {
                    pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
                    if (ch.pin2_source == 1 && ch.pin2_channel != 255) {
                        pcaManager.write(ch.pin2_addr, ch.pin2_channel, 0);
                    } else if (ch.pin2_source == 0 && ch.pca_channel2 != 255) {
                        pcaManager.write(ch.pca_addr, ch.pca_channel2, 0);
                    } else {
                        writeOutputPin(ch, 2, false);
                    }
                } else {
                    pcaManager.write(ch.pca_addr, ch.pca_channel, 0);
                    if (ch.pin2_source == 1 && ch.pin2_channel != 255) {
                        pcaManager.write(ch.pin2_addr, ch.pin2_channel, duty);
                    } else if (ch.pin2_source == 0 && ch.pca_channel2 != 255) {
                        pcaManager.write(ch.pca_addr, ch.pca_channel2, duty);
                    } else {
                        writeOutputPin(ch, 2, true);
                    }
                }
            } else if (ch.mc_mode == 1) {
                writeOutputPin(ch, 2, is_forward);
                pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, is_forward);
                writeOutputPin(ch, 2, !is_forward);
                if (ch.pin3_source == 1 && ch.pin3_channel != 255) {
                    pcaManager.write(ch.pin3_addr, ch.pin3_channel, duty);
                } else if (ch.pin3_source == 0 && ch.pca_channel3 != 255) {
                    pcaManager.write(ch.pca_addr, ch.pca_channel3, duty);
                } else {
                    writeOutputPin(ch, 3, duty > 2048);
                }
            }
        }
    } else {
        uint32_t duty = (abs_offset * max_val) / center;
        if (duty > max_val) duty = max_val;

        if (abs_offset == 0) {
            if (ch.mc_mode == 0) {
                ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
                ledcWrite(ch.ledc_chan2, ch.mc_brake ? max_val : 0);
            } else if (ch.mc_mode == 1) {
                ledcWrite(ch.dmxPort, 0);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, ch.mc_brake);
                writeOutputPin(ch, 2, ch.mc_brake);
                if (ch.pin3_source == 1) {
                    if (ch.pin3_channel != 255) pcaManager.write(ch.pin3_addr, ch.pin3_channel, ch.mc_brake ? 4095 : 0);
                } else {
                    ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
                }
            }
        } else {
            if (ch.mc_mode == 0) {
                if (is_forward) {
                    ledcWrite(ch.dmxPort, duty);
                    ledcWrite(ch.ledc_chan2, 0);
                } else {
                    ledcWrite(ch.dmxPort, 0);
                    ledcWrite(ch.ledc_chan2, duty);
                }
            } else if (ch.mc_mode == 1) {
                writeOutputPin(ch, 2, is_forward);
                ledcWrite(ch.dmxPort, duty);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, is_forward);
                writeOutputPin(ch, 2, !is_forward);
                if (ch.pin3_source == 1) {
                    if (ch.pin3_channel != 255) pcaManager.write(ch.pin3_addr, ch.pin3_channel, (uint32_t)duty * 4095 / max_val);
                } else {
                    ledcWrite(ch.dmxPort, duty);
                }
            }
        }
    }
}

#endif
