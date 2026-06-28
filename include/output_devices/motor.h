#ifndef OUTPUT_DEVICES_MOTOR_H
#define OUTPUT_DEVICES_MOTOR_H

#include <Arduino.h>
#include "output_control.h"
#include "ledc_helpers.h"

inline void motorSetup(OutputChannel& ch, uint8_t& ledcIdx) {
    if (ch.routes[0].pin == 255) return;
    if (ch.mc_mode == 0) {
        if (ch.routes[0].source == 0) {
            uint8_t pwmFwd = allocateLedc(ledcIdx);
            uint8_t pwmRev = allocateLedc(ledcIdx);
            if (pwmFwd != 255 && pwmRev != 255) {
                ledcSetup(pwmFwd, ch.mc_freq, ledcResolution(ch));
                ledcSetup(pwmRev, ch.mc_freq, ledcResolution(ch));
                ledcAttachPin(ch.routes[0].pin, pwmFwd);
                ledcAttachPin(ch.routes[1].pin, pwmRev);
                ledcWrite(pwmFwd, 0);
                ledcWrite(pwmRev, 0);
                ch.dmxPort = pwmFwd;
                ch.ledc_chan2 = pwmRev;
            }
        } else {
            ch.dmxPort = 255;
            ch.ledc_chan2 = 255;
            pcaManager.getOrCreateDriver(ch.routes[0].addr, ch.routes[0].source);
        }
    } else if (ch.mc_mode == 1) {
        if (ch.routes[0].source == 0) {
            uint8_t pwmChan = allocateLedc(ledcIdx);
            if (pwmChan != 255) {
                ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                ledcAttachPin(ch.routes[0].pin, pwmChan);
                if (ch.routes[1].source == 0) {
                    pinMode(ch.routes[1].pin, OUTPUT);
                    digitalWrite(ch.routes[1].pin, ch.routes[1].invert ? HIGH : LOW);
                } else {
                    writeOutputPin(ch, 2, false);
                }
                ledcWrite(pwmChan, 0);
                ch.dmxPort = pwmChan;
            }
        } else {
            ch.dmxPort = 255;
            if (ch.routes[1].source == 0) {
                pinMode(ch.routes[1].pin, OUTPUT);
                digitalWrite(ch.routes[1].pin, ch.routes[1].invert ? HIGH : LOW);
            } else {
                writeOutputPin(ch, 2, false);
            }
            pcaManager.getOrCreateDriver(ch.routes[0].addr, ch.routes[0].source);
            pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, 0, false, ch.routes[0].source);
        }
    } else if (ch.mc_mode == 2) {
        uint8_t pwmChan = OutputDefs::isPwmExpanderSource(ch.routes[2].source) ? 255 : allocateLedc(ledcIdx);
        if (OutputDefs::isPwmExpanderSource(ch.routes[2].source) || pwmChan != 255) {
            if (OutputDefs::isPwmExpanderSource(ch.routes[2].source)) {
                pcaManager.getOrCreateDriver(ch.routes[2].addr, ch.routes[2].source);
                pcaManager.setFrequency(ch.routes[2].addr, outputCtrl.sharedPcaFrequency(ch.routes[2].addr), ch.routes[2].source);
                if (ch.routes[2].channel != 255) pcaManager.write(ch.routes[2].addr, ch.routes[2].channel, 0, false, ch.routes[2].source);
            } else {
                ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                ledcAttachPin(ch.routes[2].pin, pwmChan);
                ledcWrite(pwmChan, 0);
                ch.dmxPort = pwmChan;
            }
            if (OutputDefs::isPwmExpanderSource(ch.routes[0].source)) {
                writeOutputPin(ch, 1, false);
            } else {
                pinMode(ch.routes[0].pin, OUTPUT);
                digitalWrite(ch.routes[0].pin, ch.routes[0].invert ? HIGH : LOW);
            }
            if (ch.routes[1].source == 0) {
                pinMode(ch.routes[1].pin, OUTPUT);
                digitalWrite(ch.routes[1].pin, ch.routes[1].invert ? HIGH : LOW);
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

    if (OutputDefs::isPwmExpanderSource(ch.routes[0].source)) {
        uint32_t duty = (abs_offset * 4095) / center;
        if (duty > 4095) duty = 4095;

        if (abs_offset == 0) {
            uint16_t offVal = ch.mc_brake ? 4095 : 0;
            if (ch.mc_mode == 0) {
                if (ch.routes[0].channel != 255) pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, offVal, false, ch.routes[0].source);
                if (ch.routes[1].channel != 255) pcaManager.write(ch.routes[1].addr, ch.routes[1].channel, offVal, false, ch.routes[1].source);
            } else if (ch.mc_mode == 1) {
                if (ch.routes[0].channel != 255) pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, 0, false, ch.routes[0].source);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, ch.mc_brake);
                writeOutputPin(ch, 2, ch.mc_brake);
                if (OutputDefs::isPwmExpanderSource(ch.routes[2].source) && ch.routes[2].channel != 255) {
                    pcaManager.write(ch.routes[2].addr, ch.routes[2].channel, ch.mc_brake ? 4095 : 0, false, ch.routes[2].source);
                } else {
                    writeOutputPin(ch, 3, ch.mc_brake);
                }
            }
        } else {
            if (ch.mc_mode == 0) {
                if (is_forward) {
                    if (ch.routes[0].channel != 255) pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, duty, false, ch.routes[0].source);
                    if (ch.routes[1].channel != 255) pcaManager.write(ch.routes[1].addr, ch.routes[1].channel, 0, false, ch.routes[1].source);
                } else {
                    if (ch.routes[0].channel != 255) pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, 0, false, ch.routes[0].source);
                    if (ch.routes[1].channel != 255) pcaManager.write(ch.routes[1].addr, ch.routes[1].channel, duty, false, ch.routes[1].source);
                }
            } else if (ch.mc_mode == 1) {
                writeOutputPin(ch, 2, is_forward);
                if (ch.routes[0].channel != 255) pcaManager.write(ch.routes[0].addr, ch.routes[0].channel, duty, false, ch.routes[0].source);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, is_forward);
                writeOutputPin(ch, 2, !is_forward);
                if (OutputDefs::isPwmExpanderSource(ch.routes[2].source) && ch.routes[2].channel != 255) {
                    pcaManager.write(ch.routes[2].addr, ch.routes[2].channel, duty, false, ch.routes[2].source);
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
                if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
                if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, ch.mc_brake ? max_val : 0);
            } else if (ch.mc_mode == 1) {
                if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, 0);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, ch.mc_brake);
                writeOutputPin(ch, 2, ch.mc_brake);
                if (OutputDefs::isPwmExpanderSource(ch.routes[2].source)) {
                    if (ch.routes[2].channel != 255) pcaManager.write(ch.routes[2].addr, ch.routes[2].channel, ch.mc_brake ? 4095 : 0, false, ch.routes[2].source);
                } else {
                    if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
                }
            }
        } else {
            if (ch.mc_mode == 0) {
                if (is_forward) {
                    if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, duty);
                    if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, 0);
                } else {
                    if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, 0);
                    if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, duty);
                }
            } else if (ch.mc_mode == 1) {
                writeOutputPin(ch, 2, is_forward);
                if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, duty);
            } else if (ch.mc_mode == 2) {
                writeOutputPin(ch, 1, is_forward);
                writeOutputPin(ch, 2, !is_forward);
                if (OutputDefs::isPwmExpanderSource(ch.routes[2].source)) {
                    if (ch.routes[2].channel != 255) pcaManager.write(ch.routes[2].addr, ch.routes[2].channel, (uint32_t)duty * 4095 / max_val, false, ch.routes[2].source);
                } else {
                    if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, duty);
                }
            }
        }
    }
}

#endif
