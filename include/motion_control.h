#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <Arduino.h>
#include <Wire.h>
#include "output_control.h"
#include <FastAccelStepper.h>

class TM1637Driver {
private:
    uint8_t clk_pin;
    uint8_t dio_pin;
    
    void start() {
        pinMode(dio_pin, OUTPUT);
        digitalWrite(dio_pin, LOW);
        delayMicroseconds(5);
        digitalWrite(clk_pin, LOW);
        delayMicroseconds(5);
    }
    
    void stop() {
        pinMode(dio_pin, OUTPUT);
        digitalWrite(dio_pin, LOW);
        delayMicroseconds(5);
        digitalWrite(clk_pin, HIGH);
        delayMicroseconds(5);
        digitalWrite(dio_pin, HIGH);
        delayMicroseconds(5);
    }
    
    bool writeByte(uint8_t b) {
        for (uint8_t i = 0; i < 8; i++) {
            digitalWrite(clk_pin, LOW);
            pinMode(dio_pin, OUTPUT);
            digitalWrite(dio_pin, (b & 0x01) ? HIGH : LOW);
            delayMicroseconds(5);
            digitalWrite(clk_pin, HIGH);
            delayMicroseconds(5);
            b >>= 1;
        }
        // Read ACK
        digitalWrite(clk_pin, LOW);
        pinMode(dio_pin, INPUT_PULLUP);
        delayMicroseconds(5);
        digitalWrite(clk_pin, HIGH);
        delayMicroseconds(5);
        bool ack = (digitalRead(dio_pin) == LOW);
        digitalWrite(clk_pin, LOW);
        return ack;
    }
    
public:
    TM1637Driver(uint8_t clk, uint8_t dio) : clk_pin(clk), dio_pin(dio) {}
    
    void begin() {
        pinMode(clk_pin, OUTPUT);
        pinMode(dio_pin, OUTPUT);
        digitalWrite(clk_pin, HIGH);
        digitalWrite(dio_pin, HIGH);
    }
    
    void displayNum(uint16_t num) {
        if (num > 9999) num = 9999;
        // Segment codes for digits 0-9
        const uint8_t segDigits[] = {
            0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
        };
        uint8_t data[4];
        data[0] = segDigits[num / 1000];
        data[1] = segDigits[(num % 1000) / 100];
        data[2] = segDigits[(num % 100) / 10];
        data[3] = segDigits[num % 10];
        
        // Command 1: Data setting
        start();
        writeByte(0x40); // write data to register, auto-increment address
        stop();
        
        // Command 2: Set Address
        start();
        writeByte(0xC0); // start address at 00H
        for (int i = 0; i < 4; i++) {
            writeByte(data[i]);
        }
        stop();
        
        // Command 3: Display control (ON, brightness 7)
        start();
        writeByte(0x88 + 7);
        stop();
    }

    void displayRaw(const uint8_t* segments) {
        if (segments == nullptr) return;

        start();
        writeByte(0x40);
        stop();

        start();
        writeByte(0xC0);
        for (int i = 0; i < 4; i++) {
            writeByte(segments[i]);
        }
        stop();

        start();
        writeByte(0x88 + 7);
        stop();
    }
};

class MotionControl {
private:
    FastAccelStepperEngine engine;
    FastAccelStepper* steppers[8] = {nullptr}; // Max 8 steppers
    uint8_t stepperCount = 0;
    
    uint8_t ledcChannelIndex = 0; // Tracks available LEDC channels (0-15)

    uint8_t allocateLedc() {
        if (ledcChannelIndex > 15) return 255;
        return ledcChannelIndex++;
    }

    uint8_t getValueByteCount(uint8_t resolution) {
        if (resolution <= 8) return 1;
        if (resolution <= 16) return 2;
        if (resolution <= 24) return 3;
        return 4;
    }

    uint32_t getMaxValue(uint8_t resolution) {
        if (resolution >= 32) return 0xFFFFFFFFUL;
        return ((uint32_t)1 << resolution) - 1;
    }

    uint32_t getDmxValue(OutputChannel& ch) {
        if (!ch.dmxBuffer) return 0;
        uint8_t bytes = getValueByteCount(ch.mc_resolution);
        uint32_t value = 0;
        for (uint8_t i = 0; i < bytes; i++) {
            value = (value << 8) | ch.dmxBuffer[i];
        }
        return value;
    }

    uint8_t ledcResolution(OutputChannel& ch) {
        return ch.mc_resolution > 16 ? 16 : ch.mc_resolution;
    }

    uint32_t calibratedPwmDacDuty(OutputChannel& ch, uint32_t value, uint32_t inputMax, uint32_t outputMax) {
        if (inputMax == 0) return 0;
        uint16_t minPct = ch.pwm_dac_min > 10000 ? 10000 : ch.pwm_dac_min;
        uint16_t maxPct = ch.pwm_dac_max > 10000 ? 10000 : ch.pwm_dac_max;
        uint32_t minDuty = ((uint64_t)outputMax * minPct) / 10000;
        uint32_t maxDuty = ((uint64_t)outputMax * maxPct) / 10000;
        if (maxDuty >= minDuty) return minDuty + ((uint64_t)value * (maxDuty - minDuty)) / inputMax;
        return minDuty - ((uint64_t)value * (minDuty - maxDuty)) / inputMax;
    }

    void writeMcp4725(uint8_t addr, uint8_t value) {
        uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        Wire.beginTransmission(addr);
        Wire.write(0x40);
        Wire.write((dac >> 4) & 0xFF);
        Wire.write((dac & 0x0F) << 4);
        Wire.endTransmission();
        if (i2cMutex) xSemaphoreGive(i2cMutex);
    }

    void writeDac7571(uint8_t addr, uint8_t value) {
        uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
        uint8_t ctrl = (dac >> 8) & 0x0F; // PD1=0, PD0=0, D11-D8
        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        Wire.beginTransmission(addr);
        Wire.write(ctrl);
        Wire.write(dac & 0xFF);
        Wire.endTransmission();
        if (i2cMutex) xSemaphoreGive(i2cMutex);
    }

    void writeDac7573(uint8_t addr, uint8_t channel, uint8_t value) {
        uint16_t dac = ((uint16_t)value << 4) | (value >> 4);
        uint8_t cmd = (channel & 0x03) << 1; // ch bits at pos 1-2, A3=A2=0, PD0=0
        if (i2cMutex && xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        Wire.beginTransmission(addr);
        Wire.write(cmd);
        Wire.write((dac >> 4) & 0xFF);
        Wire.write((dac & 0x0F) << 4);
        Wire.endTransmission();
        if (i2cMutex) xSemaphoreGive(i2cMutex);
    }

    uint8_t segmentGpio(OutputChannel& ch, uint8_t idx) {
        return (ch.seg_pins[idx] != 255) ? ch.seg_pins[idx] : (ch.pin + idx);
    }

    void setupSegmentOutput(OutputChannel& ch, uint8_t idx, bool offState) {
        uint8_t src = ch.seg_sources[idx];
        bool inv = (ch.seg_inverts >> idx) & 1;
        bool active_off = offState ^ inv;
        if (src == 1) { // PCA9685
            pcaManager.getOrCreateDriver(ch.seg_addrs[idx]);
            pcaManager.setFrequency(ch.seg_addrs[idx], ch.mc_freq ? ch.mc_freq : 1000);
            if (ch.seg_channels[idx] != 255) pcaManager.write(ch.seg_addrs[idx], ch.seg_channels[idx], active_off ? 4095 : 0);
            return;
        }
        if (src >= 2 && src <= 4) {
            if (ch.seg_channels[idx] != 255) digitalExpanderManager.write(src, ch.seg_addrs[idx], ch.seg_channels[idx], active_off, true);
            return;
        }
        uint8_t pin = segmentGpio(ch, idx);
        if (pin != 255) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, active_off ? HIGH : LOW);
        }
    }

    void writeSegmentOutput(OutputChannel& ch, uint8_t idx, bool state) {
        uint8_t src = ch.seg_sources[idx];
        bool inv = (ch.seg_inverts >> idx) & 1;
        bool active_state = state ^ inv;
        if (src == 1) { // PCA9685
            if (ch.seg_channels[idx] != 255) pcaManager.write(ch.seg_addrs[idx], ch.seg_channels[idx], active_state ? 4095 : 0);
            return;
        }
        if (src >= 2 && src <= 4) {
            if (ch.seg_channels[idx] != 255) digitalExpanderManager.write(src, ch.seg_addrs[idx], ch.seg_channels[idx], active_state);
            return;
        }
        uint8_t pin = segmentGpio(ch, idx);
        if (pin != 255) digitalWrite(pin, active_state ? HIGH : LOW);
    }

    void setStepperDirection(OutputChannel& ch, bool forward) {
        if (ch.pin2_source == 0) return;
        writeOutputPin(ch, 2, forward);
    }

    void setStepperEnable(OutputChannel& ch, bool enabled) {
        if (ch.pin3_source == 0) return;
        writeOutputPin(ch, 3, ch.mc_enable_active_high ? enabled : !enabled);
    }

public:
    void begin() {
        ledcChannelIndex = 0;
        stepperCount = 0;
        engine.init();

        for (auto& ch : outputCtrl.getChannels()) {
            if (ch.source != 0 && ch.type != 7 && ch.type != CHAN_TYPE_ANALOG_RGB) continue; // Stepper and Analog RGB may be hybrid/independent.
            if (ch.type == 4) { // CHAN_TYPE_PWM
                uint8_t pwmChan = allocateLedc();
                if (pwmChan != 255) {
                    ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                    ledcAttachPin(ch.pin, pwmChan);
                    ledcWrite(pwmChan, 0);
                    ch.dmxPort = pwmChan; // Reuse dmxPort variable to store LEDC channel
                    Serial.printf("PWM Output initialized on GPIO %d (LEDC %d)\n", ch.pin, pwmChan);
                }
            }
            else if (ch.type == 15) { // PWM DAC (GPIO-only, 2ch resolution, configurable carrier)
                uint8_t pwmChan = allocateLedc();
                if (pwmChan != 255) {
                    ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                    ledcAttachPin(ch.pin, pwmChan);
                    ledcWrite(pwmChan, 0);
                    ch.dmxPort = pwmChan;
                    Serial.printf("PWM DAC initialized on GPIO %d (LEDC %d, %d-bit, %dHz)\n", ch.pin, pwmChan, ch.mc_resolution, ch.mc_freq);
                }
            } 
            else if (ch.type == 8) { // CHAN_TYPE_SERVO (v3)
                uint8_t pwmChan = allocateLedc();
                if (pwmChan != 255) {
                    // RC Servo is exactly 50Hz, usually 16-bit for smooth control
                    ledcSetup(pwmChan, 50, 16);
                    ledcAttachPin(ch.pin, pwmChan);
                    ledcWrite(pwmChan, 0);
                    ch.dmxPort = pwmChan;
                    Serial.printf("Servo Output initialized on GPIO %d (LEDC %d)\n", ch.pin, pwmChan);
                }
            }
            else if (ch.type == 6) { // CHAN_TYPE_MOTOR_DC (v3)
                if (ch.mc_mode == 0) { // PWM + PWM
                    uint8_t pwmFwd = allocateLedc();
                    uint8_t pwmRev = allocateLedc();
                    if (pwmFwd != 255 && pwmRev != 255) {
                        ledcSetup(pwmFwd, ch.mc_freq, ledcResolution(ch));
                        ledcSetup(pwmRev, ch.mc_freq, ledcResolution(ch));
                        ledcAttachPin(ch.pin, pwmFwd);
                        ledcAttachPin(ch.pin2, pwmRev);
                        ledcWrite(pwmFwd, 0);
                        ledcWrite(pwmRev, 0);
                        ch.dmxPort = pwmFwd;
                        ch.ledc_chan2 = pwmRev; // Store 2nd LEDC channel for reverse PWM
                        Serial.printf("Motor (PWM+PWM) on GPIO %d, %d\n", ch.pin, ch.pin2);
                    }
                } 
                else if (ch.mc_mode == 1) { // PWM + DIR (v3)
                    uint8_t pwmChan = allocateLedc();
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
                        Serial.printf("Motor (PWM+DIR) src=%d GPIO=%d DIR src=%d\n", ch.source, ch.pin, ch.pin2_source);
                    }
                }
                else if (ch.mc_mode == 2) { // IN1 + IN2 + EN (v3)
                    uint8_t pwmChan = ch.pin3_source == 1 ? 255 : allocateLedc();
                    if (ch.pin3_source == 1 || pwmChan != 255) {
                        if (ch.pin3_source == 1) {
                            pcaManager.getOrCreateDriver(ch.pin3_addr);
                            pcaManager.setFrequency(ch.pin3_addr, ch.mc_freq ? ch.mc_freq : 1000);
                            if (ch.pin3_channel != 255) pcaManager.write(ch.pin3_addr, ch.pin3_channel, 0);
                        } else {
                            ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                            ledcAttachPin(ch.pin3, pwmChan); // EN pin
                            ledcWrite(pwmChan, 0);
                            ch.dmxPort = pwmChan;
                        }
                        pinMode(ch.pin, OUTPUT);  // IN1
                        digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
                        if (ch.pin2_source == 0) {
                            pinMode(ch.pin2, OUTPUT); // IN2
                            digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
                        } else {
                            writeOutputPin(ch, 2, false);
                        }
                        Serial.printf("Motor (IN1+IN2+EN) src=%d IN2 src=%d EN src=%d\n", ch.source, ch.pin2_source, ch.pin3_source);
                    }
                }
            }
            else if (ch.type == CHAN_TYPE_ANALOG_RGB) {
                ch.dmxPort = 255;
                ch.ledc_chan2 = 255;
                ch.ledc_chan3 = 255;
                ch.ledc_chan4 = 255;

                // Red
                if (ch.source == 0 && ch.pin != 255) {
                    uint8_t rChan = allocateLedc();
                    if (rChan != 255) {
                        ledcSetup(rChan, ch.mc_freq, 8);
                        ledcAttachPin(ch.pin, rChan);
                        ledcWrite(rChan, 0);
                        ch.dmxPort = rChan;
                        Serial.printf("Analog Red attached to GPIO %d (LEDC %d)\n", ch.pin, rChan);
                    }
                }
                // Green
                if (ch.pin2_source == 0 && ch.pin2 != 255) {
                    uint8_t gChan = allocateLedc();
                    if (gChan != 255) {
                        ledcSetup(gChan, ch.mc_freq, 8);
                        ledcAttachPin(ch.pin2, gChan);
                        ledcWrite(gChan, 0);
                        ch.ledc_chan2 = gChan;
                        Serial.printf("Analog Green attached to GPIO %d (LEDC %d)\n", ch.pin2, gChan);
                    }
                }
                // Blue
                if (ch.pin3_source == 0 && ch.pin3 != 255) {
                    uint8_t bChan = allocateLedc();
                    if (bChan != 255) {
                        ledcSetup(bChan, ch.mc_freq, 8);
                        ledcAttachPin(ch.pin3, bChan);
                        ledcWrite(bChan, 0);
                        ch.ledc_chan3 = bChan;
                        Serial.printf("Analog Blue attached to GPIO %d (LEDC %d)\n", ch.pin3, bChan);
                    }
                }
                // White
                if (ch.color_order >= 4 && ch.pin4_source == 0 && ch.pin4 != 255) {
                    uint8_t wChan = allocateLedc();
                    if (wChan != 255) {
                        ledcSetup(wChan, ch.mc_freq, 8);
                        ledcAttachPin(ch.pin4, wChan);
                        ledcWrite(wChan, 0);
                        ch.ledc_chan4 = wChan;
                        Serial.printf("Analog White attached to GPIO %d (LEDC %d)\n", ch.pin4, wChan);
                    }
                }
            }
            else if (ch.type == 7) { // CHAN_TYPE_STEPPER (v3)
                if (ch.source != 0) {
                    Serial.println("Stepper skipped: STEP pin must use ESP32 GPIO source");
                    continue;
                }
                FastAccelStepper* stepper = engine.stepperConnectToPin(ch.pin); // STEP pin
                if (stepper) {
                    if (ch.pin2_source == 0 && ch.pin2 != 255) {
                        stepper->setDirectionPin(ch.pin2, ch.pin2_invert);
                    } else {
                        setStepperDirection(ch, true);
                    }
                    if (ch.pin3_source == 0 && ch.pin3 != 255) {
                        stepper->setEnablePin(ch.pin3, !ch.pin3_invert);
                        stepper->setAutoEnable(true);
                    } else if (ch.pin3_source != 0) {
                        setStepperEnable(ch, true);
                    }
                    stepper->setSpeedInHz(ch.mc_freq);       // Default speed
                    stepper->setAcceleration(ch.mc_freq * 2); // Quick accel
                    
                    steppers[stepperCount] = stepper;
                    ch.dmxPort = stepperCount; // Store stepper index
                    stepperCount++;
                    Serial.printf("Stepper initialized on STEP:%d DIR:%s EN:%s\n",
                                  ch.pin,
                                  ch.pin2_source == 0 ? String(ch.pin2).c_str() : "expander",
                                  ch.pin3_source == 0 ? String(ch.pin3).c_str() : "expander");
                }
            }
            else if (ch.type == 9) { // Passive Buzzer (v3)
                uint8_t pwmChan = allocateLedc();
                if (pwmChan != 255) {
                    ledcSetup(pwmChan, 1000, 8); // start at 1000Hz, 8-bit
                    ledcAttachPin(ch.pin, pwmChan);
                    ledcWrite(pwmChan, 0);
                    ch.dmxPort = pwmChan;
                    Serial.printf("Passive Buzzer initialized: GPIO %d, LEDC %d\n", ch.pin, pwmChan);
                }
        } else if (ch.type == 11 || ch.type == 12 || ch.type == 13) { // 7-Segment Display (v3)
            bool isCommonDim = (ch.mc_mode >= 6 && ch.mc_mode <= 9);
            uint8_t numSeg = (ch.mc_mode == 3 || ch.mc_mode == 5 || ch.mc_mode == 8 || ch.mc_mode == 9) ? 8 : 7;

            if (ch.mc_mode >= 2 && ch.pin2_source == 1) { // Direct Drive via PCA9685
                pcaManager.getOrCreateDriver(ch.pin2_addr);
                pcaManager.setFrequency(ch.pin2_addr, ch.mc_freq ? ch.mc_freq : 1000);
                for (uint8_t s = 0; s < numSeg; s++) {
                    pcaManager.write(ch.pin2_addr, ch.pin2_channel + s, 0);
                }
                // Also if Common Dim and COM pin is GPIO, allocate LEDC for COM pin
                if (isCommonDim && ch.source == 0) {
                    uint8_t pwmChan = allocateLedc();
                    if (pwmChan != 255) {
                        ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                        ledcAttachPin(ch.pin, pwmChan);
                        ledcWrite(pwmChan, (ch.mc_mode == 6 || ch.mc_mode == 8) ? 0 : 255); // CA=0, CC=255
                        ch.dmxPort = pwmChan;
                    }
                }
                ch.prev_7seg_val = 0;
                ch.prev_7seg_valid = false;
                Serial.printf("7-Segment DD PCA9685: mc_mode=%d pin2_source=1 baseCh=%d\n",
                              ch.mc_mode, ch.pin2_channel);
            } else if (ch.mc_mode >= 2 && ch.pin2_source >= 2 && ch.pin2_source <= 4) { // Direct Drive via Expander
                for (uint8_t s = 0; s < numSeg; s++) {
                    digitalExpanderManager.write(ch.pin2_source, ch.pin2_addr, ch.pin2_channel + s, false, true);
                }
                // Also if Common Dim and COM pin is GPIO, allocate LEDC for COM pin
                if (isCommonDim && ch.source == 0) {
                    uint8_t pwmChan = allocateLedc();
                    if (pwmChan != 255) {
                        ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                        ledcAttachPin(ch.pin, pwmChan);
                        ledcWrite(pwmChan, (ch.mc_mode == 6 || ch.mc_mode == 8) ? 0 : 255); // CA=0, CC=255
                        ch.dmxPort = pwmChan;
                    }
                }
                ch.prev_7seg_val = 0;
                ch.prev_7seg_valid = false;
                Serial.printf("7-Segment DD Expander: mc_mode=%d pin2_source=%d baseCh=%d\n",
                              ch.mc_mode, ch.pin2_source, ch.pin2_channel);
            } else if (ch.mc_mode >= 2 && ch.pin2_source == 0) { // Direct Drive via GPIO
                if (isCommonDim) {
                    // COM pin is PWM
                    if (ch.source == 0) {
                        uint8_t pwmChan = allocateLedc();
                        if (pwmChan != 255) {
                            ledcSetup(pwmChan, ch.mc_freq ? ch.mc_freq : 1000, 8);
                            ledcAttachPin(ch.pin, pwmChan);
                            ledcWrite(pwmChan, (ch.mc_mode == 6 || ch.mc_mode == 8) ? 0 : 255);
                            ch.dmxPort = pwmChan;
                        }
                    }
                    // Segments are digital GPIO
                    for (uint8_t s = 0; s < numSeg; s++) {
                        setupSegmentOutput(ch, s, (ch.mc_mode == 6 || ch.mc_mode == 8));
                    }
                    Serial.printf("7-Segment DD Common Dim: COM_GPIO=%d segs=%d dmxPort=%d\n",
                                  ch.pin, numSeg, ch.dmxPort);
                } else {
                    // Modes 2-5: individual segment PWM (or digital if type 11)
                    if (ch.type == 12 || ch.type == 13) {
                        uint8_t baseChan = allocateLedc();
                        if (baseChan != 255) {
                            for (uint8_t s = 0; s < numSeg; s++) {
                                uint8_t segPin = segmentGpio(ch, s);
                                if (ch.seg_sources[s] == 0 && segPin != 255 && baseChan + s <= 15) {
                                    ledcSetup(baseChan + s, ch.mc_freq ? ch.mc_freq : 1000, 8);
                                    ledcAttachPin(segPin, baseChan + s);
                                    ledcWrite(baseChan + s, 0);
                                } else if (ch.seg_sources[s] >= 1 && ch.seg_sources[s] <= 4) {
                                    setupSegmentOutput(ch, s, false);
                                }
                            }
                            ch.dmxPort = baseChan;
                            Serial.printf("7-Segment DD PWM: GPIO base=%d pins=%d baseLEDC=%d freq=%dHz\n",
                                          ch.pin, numSeg, baseChan, ch.mc_freq ? ch.mc_freq : 1000);
                        }
                    } else {
                        // Type 11
                        for (uint8_t s = 0; s < numSeg; s++) {
                            setupSegmentOutput(ch, s, false);
                        }
                    }
                }
                ch.prev_7seg_val = 0;
                ch.prev_7seg_valid = false;
            } else { // TM1637
                TM1637Driver tm(ch.pin, ch.pin2);
                tm.begin();
                ch.prev_7seg_val = 0;
                ch.prev_7seg_valid = false;
                Serial.printf("7-Segment Display initialized: CLK %d, DIO %d\n", ch.pin, ch.pin2);
            }
        }
            else if (ch.type == 16) { // Function Generator
                uint8_t pwmChan = allocateLedc();
                if (pwmChan != 255) {
                    ch.funcGen = new FuncGenController();
                    ch.funcGen->begin(ch.pin, pwmChan, ch.mc_freq ? ch.mc_freq : 50000);
                    ch.dmxPort = pwmChan;
                    Serial.printf("Function Generator initialized: GPIO %d, LEDC %d, PWM %dHz\n", ch.pin, pwmChan, ch.mc_freq ? ch.mc_freq : 50000);
                } else {
                    Serial.println("Error: No LEDC channel available for Function Generator!");
                }
            }
        }
    }

    void update() {
        for (auto& ch : outputCtrl.getChannels()) {
            if (ch.dmxBuffer == nullptr) continue;

            uint32_t val = getDmxValue(ch);
            uint32_t max_val = getMaxValue(ch.mc_resolution);

            if (ch.type == CHAN_TYPE_ANALOG_RGB) {
                uint16_t r = ch.dmxBuffer[0];
                uint16_t g = ch.dmxBuffer[1];
                uint16_t b = ch.dmxBuffer[2];
                uint16_t w = (ch.color_order >= 4) ? ch.dmxBuffer[3] : 0;

                // Red
                if (ch.source == 0) {
                    if (ch.dmxPort != 255) ledcWrite(ch.dmxPort, r);
                } else if (ch.source == 1) {
                    pcaManager.write(ch.pca_addr, ch.pca_channel, (r * 4095) / 255);
                }

                // Green
                if (ch.pin2_source == 0) {
                    if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, g);
                } else if (ch.pin2_source == 1) {
                    pcaManager.write(ch.pin2_addr, ch.pin2_channel, (g * 4095) / 255);
                }

                // Blue
                if (ch.pin3_source == 0) {
                    if (ch.ledc_chan3 != 255) ledcWrite(ch.ledc_chan3, b);
                } else if (ch.pin3_source == 1) {
                    pcaManager.write(ch.pin3_addr, ch.pin3_channel, (b * 4095) / 255);
                }

                // White
                if (ch.color_order >= 4) {
                    if (ch.pin4_source == 0) {
                        if (ch.ledc_chan4 != 255) ledcWrite(ch.ledc_chan4, w);
                    } else if (ch.pin4_source == 1) {
                        pcaManager.write(ch.pin4_addr, ch.pin4_channel, (w * 4095) / 255);
                    }
                }
                continue;
            }

            if (ch.source == 1) { // PCA9685 Expander
                if (ch.type == 4 || ch.type == 15) { // PWM Dimmer / PWM DAC
                    uint16_t duty = ch.type == 15 ? calibratedPwmDacDuty(ch, val, max_val, 4095) : (uint32_t)((uint64_t)val * 4095) / max_val;
                    pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
                }
                else if (ch.type == 8) { // RC Servo (50 Hz) (v3)
                    uint32_t pulse_us = ch.mc_min_us + (val * (ch.mc_max_us - ch.mc_min_us)) / max_val;
                    uint32_t ticks = (pulse_us * 4096) / 20000;
                    pcaManager.write(ch.pca_addr, ch.pca_channel, ticks > 4095 ? 4095 : ticks);
                }
                else if (ch.type == 6) { // DC Motor (v3)
                    int32_t center = max_val / 2;
                    int32_t offset = (int32_t)val - center;
                    bool is_forward = ch.mc_invert ? (offset < 0) : (offset > 0);
                    uint32_t abs_offset = abs(offset);
                    
                    if (abs_offset < ch.mc_deadband) {
                        abs_offset = 0;
                    }

                    uint32_t duty = (abs_offset * 4095) / center;
                    if (duty > 4095) duty = 4095;

                    if (abs_offset == 0) {
                        // STOP
                        if (ch.mc_mode == 0) { // PWM+PWM
                            pcaManager.write(ch.pca_addr, ch.pca_channel, ch.mc_brake ? 4095 : 0);
                            if (ch.pca_channel2 != 255) {
                                pcaManager.write(ch.pca_addr, ch.pca_channel2, ch.mc_brake ? 4095 : 0);
                            }
                        } else if (ch.mc_mode == 1) { // PWM+DIR
                            pcaManager.write(ch.pca_addr, ch.pca_channel, 0);
                        } else if (ch.mc_mode == 2) { // IN1+IN2+EN
                            pcaManager.write(ch.pca_addr, ch.pca_channel, ch.mc_brake ? 4095 : 0);
                            if (ch.pca_channel2 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel2, ch.mc_brake ? 4095 : 0);
                            if (ch.pca_channel3 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel3, ch.mc_brake ? 4095 : 0);
                        }
                    } else {
                        // MOVE
                        if (ch.mc_mode == 0) { // PWM+PWM
                            if (is_forward) {
                                pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
                                if (ch.pca_channel2 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel2, 0);
                            } else {
                                pcaManager.write(ch.pca_addr, ch.pca_channel, 0);
                                if (ch.pca_channel2 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel2, duty);
                            }
                        } else if (ch.mc_mode == 1) { // PWM+DIR
                            if (ch.pca_channel2 != 255) {
                                pcaManager.write(ch.pca_addr, ch.pca_channel2, is_forward ? 4095 : 0);
                            }
                            pcaManager.write(ch.pca_addr, ch.pca_channel, duty);
                        } else if (ch.mc_mode == 2) { // IN1+IN2+EN
                            pcaManager.write(ch.pca_addr, ch.pca_channel, is_forward ? 4095 : 0);
                            if (ch.pca_channel2 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel2, is_forward ? 0 : 4095);
                            if (ch.pca_channel3 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel3, duty);
                        }
                    }
                }

                continue; // Skip ESP32 direct hardware update
            } else if (ch.source == 5) { // MCP4725 I2C DAC
                if (ch.type == 14) writeMcp4725(ch.pca_addr, ch.dmxBuffer[0]);
                continue;
            } else if (ch.source == 6) { // DAC7571 I2C DAC (single-channel)
                if (ch.type == 14) writeDac7571(ch.pca_addr, ch.dmxBuffer[0]);
                continue;
            } else if (ch.source == 7) { // DAC7573 I2C DAC (quad-channel)
                if (ch.type == 14) writeDac7573(ch.pca_addr, ch.pca_channel, ch.dmxBuffer[0]);
                continue;
            } else if (ch.source != 0) {
                continue; // Digital expanders are handled by OutputControl for on/off modes.
            }

            if (ch.type == 4) { // PWM
                ledcWrite(ch.dmxPort, val);
            }
            else if (ch.type == 15) { // PWM DAC
                ledcWrite(ch.dmxPort, calibratedPwmDacDuty(ch, val, max_val, max_val));
            }
            else if (ch.type == 8) { // SERVO (v3)
                // map DMX value to min_us -> max_us
                uint32_t pulse_us = ch.mc_min_us + (val * (ch.mc_max_us - ch.mc_min_us)) / max_val;
                // Duty cycle for 16-bit at 50Hz (20,000 us period)
                uint32_t duty = (pulse_us * 65535) / 20000;
                ledcWrite(ch.dmxPort, duty);
            }
            else if (ch.type == 6) { // DC MOTOR (v3)
                int32_t center = max_val / 2;
                int32_t offset = (int32_t)val - center;
                bool is_forward = ch.mc_invert ? (offset < 0) : (offset > 0);
                uint32_t abs_offset = abs(offset);
                
                // Deadband check
                if (abs_offset < ch.mc_deadband) {
                    abs_offset = 0;
                }

                // Scale to full PWM range (0 to max_val)
                uint32_t duty = (abs_offset * max_val) / center;
                if (duty > max_val) duty = max_val;

                if (abs_offset == 0) {
                    // STOP
                    if (ch.mc_mode == 0) { // PWM+PWM
                        ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
                        ledcWrite(ch.ledc_chan2, ch.mc_brake ? max_val : 0);
                    } else if (ch.mc_mode == 1) { // PWM+DIR
                        ledcWrite(ch.dmxPort, 0);
                    } else if (ch.mc_mode == 2) { // IN1+IN2+EN
                        writeOutputPin(ch, 1, ch.mc_brake);
                        writeOutputPin(ch, 2, ch.mc_brake);
                        if (ch.pin3_source == 1) {
                            if (ch.pin3_channel != 255) pcaManager.write(ch.pin3_addr, ch.pin3_channel, ch.mc_brake ? 4095 : 0);
                        } else {
                            ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
                        }
                    }
                } else {
                    // MOVE
                    if (ch.mc_mode == 0) { // PWM+PWM
                        if (is_forward) {
                            ledcWrite(ch.dmxPort, duty);
                            ledcWrite(ch.ledc_chan2, 0);
                        } else {
                            ledcWrite(ch.dmxPort, 0);
                            ledcWrite(ch.ledc_chan2, duty);
                        }
                    } else if (ch.mc_mode == 1) { // PWM+DIR
                        writeOutputPin(ch, 2, is_forward);
                        ledcWrite(ch.dmxPort, duty);
                    } else if (ch.mc_mode == 2) { // IN1+IN2+EN
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

            else if (ch.type == 7) { // STEPPER (v3)
                if (ch.dmxPort < stepperCount) {
                    FastAccelStepper* stepper = steppers[ch.dmxPort];
                    if (stepper) {
                        uint8_t pos_bytes = getValueByteCount(ch.mc_resolution);
                        uint32_t speed_val = ch.dmxBuffer[pos_bytes];
                        uint8_t cmd_val = ch.dmxBuffer[pos_bytes + 1];
                        
                        // Dynamic Speed Update
                        // Map DMX speed (0-255) to speed (0 to ch.mc_freq)
                        uint32_t target_speed = (speed_val * ch.mc_freq) / 255;
                        if (target_speed < 50) target_speed = 50; // Minimum speed to avoid stall
                        
                        unsigned long now = millis();
                        
                        // Handle Command DMX channel
                        if (cmd_val >= 100 && cmd_val <= 199) {
                            // Stop Command
                            if (ch.stepper_cmd_state != 2) { // not already stopped
                                if (ch.stepper_cmd_state != 1) { // not pending stop
                                    ch.stepper_cmd_state = 1;
                                    ch.stepper_cmd_start_time = now;
                                } else if (now - ch.stepper_cmd_start_time >= 1000) {
                                    // Debounce 1s passed, execute Stop
                                    stepper->stopMove();
                                    ch.stepper_cmd_state = 2; // stop active
                                    Serial.println("Stepper Stop command executed");
                                }
                            }
                        } else if (cmd_val >= 200 && cmd_val <= 255) {
                            // Homing Command
                            if (ch.stepper_cmd_state != 4 && ch.stepper_cmd_state != 5) { // not already homing/done
                                if (ch.stepper_cmd_state != 3) { // not pending homing
                                    ch.stepper_cmd_state = 3;
                                    ch.stepper_cmd_start_time = now;
                                } else if (now - ch.stepper_cmd_start_time >= 1000) {
                                    // Debounce 1s passed, start Homing
                                    ch.stepper_cmd_state = 4; // homing active
                                    ch.stepper_homing_start_time = now;
                                    
                                    // Set speed to homing speed
                                    uint32_t h_speed = ch.mc_homing_speed;
                                    if (h_speed == 0) h_speed = ch.mc_freq / 2; // fallback
                                    stepper->setSpeedInHz(h_speed);
                                    
                                    // Setup sensor pin if Mode A (Sensor)
            if (ch.mc_homing_mode == 0 && ch.pin4 != 255) {
                if (ch.pin4_source == 0) {
                    pinMode(ch.pin4, INPUT_PULLUP);
                } else {
                    digitalExpanderManager.configureInput(ch.pin4_source, ch.pin4_addr, ch.pin4_channel, true);
                }
            }
                                    
                                    // Start running in homing direction
                                    if (ch.mc_homing_dir == 0) {
                                        setStepperDirection(ch, true);
                                        stepper->runForward();
                                    } else {
                                        setStepperDirection(ch, false);
                                        stepper->runBackward();
                                    }
                                    Serial.printf("Stepper Homing started: speed=%d, mode=%d, dir=%d\n", h_speed, ch.mc_homing_mode, ch.mc_homing_dir);
                                }
                            }
                        } else {
                            // Normal Command (0-99)
                            if (ch.stepper_cmd_state != 0) {
                                ch.stepper_cmd_state = 0;
                                // Restore speed to dynamic target speed
                                stepper->setSpeedInHz(target_speed);
                                Serial.println("Stepper returned to Normal mode");
                            }
                        }
                        
                        // If homing is active, run sensor / timeout checks
                        if (ch.stepper_cmd_state == 4) {
                            bool zero_reached = false;
                            if (ch.mc_homing_mode == 0) { // Sensor Mode
                                if (ch.pin4 != 255 && readOutputPin(ch, 4) == false) {
                                    zero_reached = true;
                                    Serial.println("Stepper homing sensor triggered");
                                }
                            } else { // Time/Stall Mode
                                if (now - ch.stepper_homing_start_time >= (unsigned long)ch.mc_homing_timeout * 1000) {
                                    zero_reached = true;
                                    Serial.println("Stepper homing timeout reached");
                                }
                            }
                            
                            if (zero_reached) {
                                stepper->forceStopAndNewPosition(0);
                                ch.stepper_cmd_state = 5; // homing done
                                // Restore speed
                                stepper->setSpeedInHz(target_speed);
                            }
                        }
                        
                        // Normal Position Move (Only in Normal state)
                        if (ch.stepper_cmd_state == 0) {
                            stepper->setSpeedInHz(target_speed);
                            int32_t targetPos;
                            if (ch.mc_scale_factor > 0.0f) {
                                targetPos = (int32_t)round(val * ch.mc_scale_factor);
                            } else {
                                targetPos = (int32_t)(((uint64_t)val * ch.mc_steps_per_rev) / max_val);
                            }
                            if (ch.mc_invert) targetPos = -targetPos;
                            if (ch.pin2_source != 0) {
                                setStepperDirection(ch, targetPos >= stepper->getCurrentPosition());
                            }
                            setStepperEnable(ch, true);
                            stepper->moveTo(targetPos);
                        }
                    }
                }
            }
            else if (ch.type == 9) { // Passive Buzzer (v3)
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
            } else if (ch.type == 11 || ch.type == 12 || ch.type == 13) { // 7-Segment Display (v3)
                uint8_t bytes = 0;
                if (ch.type >= 12) {
                    if (ch.mc_mode == 4 || ch.mc_mode == 5 || ch.mc_mode >= 6) {
                        bytes = 2;
                    } else {
                        bytes = 1;
                    }
                } else {
                    bytes = (ch.mc_mode == 1) ? 4 : 2;
                }
                uint32_t val = 0;
                for (uint8_t i = 0; i < bytes; i++) {
                    val = (val << 8) | ch.dmxBuffer[i];
                }
                if (!ch.prev_7seg_valid || val != ch.prev_7seg_val) {
                    ch.prev_7seg_val = val;
                    ch.prev_7seg_valid = true;

                    bool isCommonDim = (ch.mc_mode >= 6 && ch.mc_mode <= 9);
                    bool isCA = (ch.mc_mode == 6 || ch.mc_mode == 8);
                    uint8_t numSeg = (ch.mc_mode == 3 || ch.mc_mode == 5 || ch.mc_mode == 8 || ch.mc_mode == 9) ? 8 : 7;
                    uint8_t segByte = 0;
                    if (ch.mc_resolution == 10) {
                        uint8_t raw = ch.dmxBuffer[0];
                        if (raw <= 9) {
                            const uint8_t segDigits[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
                            segByte = segDigits[raw];
                        } else if (raw >= 10 && raw <= 19) {
                            const uint8_t segDigits[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
                            segByte = segDigits[raw - 10] | 0x80;
                        }
                    } else {
                        segByte = asciiToSegment(ch.dmxBuffer[0]);
                    }

                    if (ch.mc_mode >= 2 && ch.pin2_source == 1) {
                        // Direct Drive via PCA9685
                        for (uint8_t b = 0; b < numSeg; b++) {
                            bool bitVal = (segByte >> b) & 1;
                            if (isCommonDim) {
                                bitVal = isCA ? !bitVal : bitVal;
                            }
                            bool inv = (ch.seg_inverts >> b) & 1;
                            pcaManager.write(ch.pin2_addr, ch.pin2_channel + b, (bitVal ^ inv) ? 4095 : 0);
                        }
                        if (isCommonDim) {
                            uint8_t brightness = ch.dmxBuffer[1];
                            uint8_t duty = isCA ? brightness : (255 - brightness);
                            if (ch.source == 1) { // PCA9685 COM Pin
                                pcaManager.write(ch.pca_addr, ch.pca_channel, (duty * 4095) / 255);
                            } else if (ch.source == 0 && ch.dmxPort != 255) { // GPIO LEDC COM Pin
                                ledcWrite(ch.dmxPort, duty);
                            }
                        }
                    } else if (ch.mc_mode >= 2 && ch.pin2_source >= 2 && ch.pin2_source <= 4) {
                        // Direct Drive via Expander (1 digit, 7 or 8 pins)
                        for (uint8_t b = 0; b < numSeg; b++) {
                            bool bitVal = (segByte >> b) & 1;
                            if (isCommonDim) {
                                bitVal = isCA ? !bitVal : bitVal;
                            }
                            bool inv = (ch.seg_inverts >> b) & 1;
                            digitalExpanderManager.write(ch.pin2_source, ch.pin2_addr, ch.pin2_channel + b, bitVal ^ inv);
                        }
                        if (isCommonDim) {
                            uint8_t brightness = ch.dmxBuffer[1];
                            uint8_t duty = isCA ? brightness : (255 - brightness);
                            if (ch.source == 1) { // PCA9685 COM Pin
                                pcaManager.write(ch.pca_addr, ch.pca_channel, (duty * 4095) / 255);
                            } else if (ch.source == 0 && ch.dmxPort != 255) { // GPIO LEDC COM Pin
                                ledcWrite(ch.dmxPort, duty);
                            }
                        }
                    } else if (ch.mc_mode >= 2 && ch.pin2_source == 0) {
                        // Direct Drive via GPIO
                        if (isCommonDim) {
                            // COM pin dimming
                            uint8_t brightness = ch.dmxBuffer[1];
                            uint8_t duty = isCA ? brightness : (255 - brightness);
                            if (ch.source == 1) { // PCA9685 COM Pin
                                pcaManager.write(ch.pca_addr, ch.pca_channel, (duty * 4095) / 255);
                            } else if (ch.source == 0 && ch.dmxPort != 255) { // GPIO LEDC COM Pin
                                ledcWrite(ch.dmxPort, duty);
                            }
                            // Segments are digital GPIO
                            for (uint8_t b = 0; b < numSeg; b++) {
                                bool bitVal = (segByte >> b) & 1;
                                writeSegmentOutput(ch, b, isCA ? !bitVal : bitVal);
                            }
                        } else {
                            if (ch.type == 12 || ch.type == 13) {
                                // PWM Direct Drive via LEDC
                                uint8_t brightness = 255;
                                if (ch.mc_mode == 4 || ch.mc_mode == 5) {
                                    brightness = ch.dmxBuffer[1]; // DMX Channel 2
                                }
                                for (uint8_t b = 0; b < numSeg; b++) {
                                    uint32_t duty = ((segByte >> b) & 1) ? brightness : 0;
                                    if (ch.seg_sources[b] >= 1 && ch.seg_sources[b] <= 4) {
                                        writeSegmentOutput(ch, b, duty > 0);
                                    } else if (ch.dmxPort + b <= 15) {
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
                        if (ch.mc_mode == 1) {
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
            }
            else if (ch.type == 10) { // DFPlayer MP3 (v3)
                if (ch.dfPlayer != nullptr && ch.dmxBuffer != nullptr) {
                    ch.dfPlayer->update(ch.dmxBuffer[0], ch.dmxBuffer[1], ch.dmxBuffer[2]);
                }
            }
            else if (ch.type == 14) { // DAC
                if (ch.source == 0) continue; // GPIO DAC blocked on WT32-ETH01 (ADR008)
                dacWrite(ch.pin, ch.dmxBuffer[0]);
            }
            else if (ch.type == 16 && ch.funcGen != nullptr && ch.dmxBuffer != nullptr) { // Function Generator
                uint16_t freq = ch.dmxBuffer[0] | (ch.dmxBuffer[1] << 8);
                uint8_t wfType = ch.dmxBuffer[2];
                uint8_t amp = ch.dmxBuffer[3];
                uint8_t offset = ch.dmxBuffer[4];
                ch.funcGen->update(wfType, freq, amp, offset);
            }
        }
    }
};

// NOTE: SACNControl sacnCtrl is defined in main.cpp
extern MotionControl motionCtrl;
// NOTE: MotionControl motionCtrl is defined in main.cpp

#endif // MOTION_CONTROL_H
