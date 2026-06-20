#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <Arduino.h>
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

    void setStepperDirection(OutputChannel& ch, bool forward) {
        if (ch.pin2_source == 0) return;
        writeOutputPin(ch, 2, forward);
    }

    void setStepperEnable(OutputChannel& ch, bool enabled) {
        if (ch.pin3_source == 0) return;
        writeOutputPin(ch, 3, !enabled);
    }

public:
    void begin() {
        ledcChannelIndex = 0;
        stepperCount = 0;
        engine.init();

        for (auto& ch : outputCtrl.getChannels()) {
            if (ch.source != 0 && ch.type != 7) continue; // Stepper STEP must remain ESP32 GPIO; DIR/EN may be hybrid.
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
                    uint8_t pwmChan = allocateLedc();
                    if (pwmChan != 255) {
                        ledcSetup(pwmChan, ch.mc_freq, ledcResolution(ch));
                        ledcAttachPin(ch.pin3, pwmChan); // EN pin
                        if (ch.pin2_source == 0) {
                            pinMode(ch.pin, OUTPUT);  // IN1
                            digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
                        } else {
                            writeOutputPin(ch, 2, false);
                        }
                        if (ch.pin3_source == 0) {
                            pinMode(ch.pin2, OUTPUT); // IN2
                            digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
                        } else {
                            writeOutputPin(ch, 3, false);
                        }
                        ledcWrite(pwmChan, 0);
                        ch.dmxPort = pwmChan;
                        Serial.printf("Motor (IN1+IN2+EN) src=%d EN=GPIO%d IN1 src=%d IN2 src=%d\n", ch.source, ch.pin3, ch.pin2_source, ch.pin3_source);
                    }
                }
            }
            else if (ch.type == CHAN_TYPE_ANALOG_RGB) {
                uint8_t rChan = allocateLedc();
                uint8_t gChan = allocateLedc();
                uint8_t bChan = allocateLedc();
                uint8_t wChan = (ch.color_order >= 4) ? allocateLedc() : 255;
                if (rChan != 255 && gChan != 255 && bChan != 255) {
                    ledcSetup(rChan, ch.mc_freq, 8);
                    ledcAttachPin(ch.pin, rChan);
                    ledcWrite(rChan, 0);
                    ch.dmxPort = rChan;

                    ledcSetup(gChan, ch.mc_freq, 8);
                    ledcAttachPin(ch.pin2, gChan);
                    ledcWrite(gChan, 0);
                    ch.ledc_chan2 = gChan;

                    ledcSetup(bChan, ch.mc_freq, 8);
                    ledcAttachPin(ch.pin3, bChan);
                    ledcWrite(bChan, 0);
                    ch.ledc_chan3 = bChan;

                    if (wChan != 255) {
                        ledcSetup(wChan, ch.mc_freq, 8);
                        ledcAttachPin(ch.pin4, wChan);
                        ledcWrite(wChan, 0);
                        ch.ledc_chan4 = wChan;
                    }
                    Serial.printf("Analog %s initialized: R:%d (LEDC %d), G:%d (LEDC %d), B:%d (LEDC %d)%s\n",
                                  (ch.color_order >= 4) ? "RGBW" : "RGB",
                                  ch.pin, rChan, ch.pin2, gChan, ch.pin3, bChan,
                                  (wChan != 255) ? " (W attached)" : "");
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
            if (ch.mc_mode >= 2 && ch.pin2_source >= 2 && ch.pin2_source <= 4) { // Direct Drive via Expander
                for (uint8_t s = 0; s < (ch.mc_mode == 3 ? 8 : 7); s++) {
                    digitalExpanderManager.write(ch.pin2_source, ch.pin2_addr, ch.pin2_channel + s, false, true);
                }
                ch.prev_7seg_val = 0;
                ch.prev_7seg_valid = false;
                Serial.printf("7-Segment Direct Drive: expander src=%d addr=0x%02X baseCh=%d pins=%d\n",
                              ch.pin2_source, ch.pin2_addr, ch.pin2_channel, ch.mc_mode == 3 ? 8 : 7);
            } else if (ch.mc_mode >= 2 && ch.pin2_source == 0) { // Direct Drive via GPIO
                if (ch.type == 12 || ch.type == 13) {
                    uint8_t numSeg = (ch.mc_mode == 3) ? 8 : 7;
                    uint8_t baseChan = allocateLedc();
                    if (baseChan != 255) {
                        for (uint8_t s = 0; s < numSeg; s++) {
                            uint8_t segPin = (ch.seg_pins[s] != 255) ? ch.seg_pins[s] : (ch.pin + s);
                            if (segPin != 255 && baseChan + s <= 15) {
                                ledcSetup(baseChan + s, ch.mc_freq ? ch.mc_freq : 1000, 8);
                                ledcAttachPin(segPin, baseChan + s);
                                ledcWrite(baseChan + s, 0);
                            }
                        }
                        ch.dmxPort = baseChan;
                        Serial.printf("7-Segment DD PWM: GPIO base=%d pins=%d baseLEDC=%d freq=%dHz\n",
                                      ch.pin, numSeg, baseChan, ch.mc_freq ? ch.mc_freq : 1000);
                    }
                } else {
                    for (uint8_t s = 0; s < (ch.mc_mode == 3 ? 8 : 7); s++) {
                        uint8_t segPin = (ch.seg_pins[s] != 255) ? ch.seg_pins[s] : (ch.pin + s);
                        if (segPin != 255) {
                            pinMode(segPin, OUTPUT);
                            digitalWrite(segPin, LOW);
                        }
                    }
                    Serial.printf("7-Segment Direct Drive: GPIO base=%d pins=%d\n",
                                  ch.pin, ch.mc_mode == 3 ? 8 : 7);
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

            if (ch.source == 1) { // PCA9685 Expander
                if (ch.type == 4 || ch.type == 15) { // PWM Dimmer / PWM DAC
                    uint16_t duty = (uint32_t)((uint64_t)val * 4095) / max_val;
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
                else if (ch.type == CHAN_TYPE_ANALOG_RGB) {
                    uint16_t r = ch.dmxBuffer[0];
                    uint16_t g = ch.dmxBuffer[1];
                    uint16_t b = ch.dmxBuffer[2];
                    uint16_t w = (ch.color_order >= 4) ? ch.dmxBuffer[3] : 0;

                    pcaManager.write(ch.pca_addr, ch.pca_channel, (r * 4095) / 255);
                    if (ch.pca_channel2 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel2, (g * 4095) / 255);
                    if (ch.pca_channel3 != 255) pcaManager.write(ch.pca_addr, ch.pca_channel3, (b * 4095) / 255);
                    if (ch.color_order >= 4 && ch.pca_channel4 != 255) {
                        pcaManager.write(ch.pca_addr, ch.pca_channel4, (w * 4095) / 255);
                    }
                }
                continue; // Skip ESP32 direct hardware update
            } else if (ch.source != 0) {
                continue; // Digital expanders are handled by OutputControl for on/off modes.
            }

            if (ch.type == 4) { // PWM
                ledcWrite(ch.dmxPort, val);
            }
            else if (ch.type == 15) { // PWM DAC
                ledcWrite(ch.dmxPort, val);
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
                        writeOutputPin(ch, 2, ch.mc_brake);
                        writeOutputPin(ch, 3, ch.mc_brake);
                        ledcWrite(ch.dmxPort, ch.mc_brake ? max_val : 0);
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
                        writeOutputPin(ch, 2, is_forward);
                        writeOutputPin(ch, 3, !is_forward);
                        ledcWrite(ch.dmxPort, duty);
                    }
                }
            }
            else if (ch.type == CHAN_TYPE_ANALOG_RGB) {
                ledcWrite(ch.dmxPort, ch.dmxBuffer[0]);
                if (ch.ledc_chan2 != 255) ledcWrite(ch.ledc_chan2, ch.dmxBuffer[1]);
                if (ch.ledc_chan3 != 255) ledcWrite(ch.ledc_chan3, ch.dmxBuffer[2]);
                if (ch.color_order >= 4 && ch.ledc_chan4 != 255) {
                    ledcWrite(ch.ledc_chan4, ch.dmxBuffer[3]);
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
                    bytes = 1;
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
                    if (ch.mc_mode >= 2 && ch.pin2_source >= 2 && ch.pin2_source <= 4) {
                        // Direct Drive via Expander (1 digit, 7 or 8 pins)
                        uint8_t numSeg = (ch.mc_mode == 3 || ch.mc_mode == 5) ? 8 : 7;
                        uint8_t segByte = asciiToSegment(ch.dmxBuffer[0]);
                        for (uint8_t b = 0; b < numSeg; b++) {
                            digitalExpanderManager.write(ch.pin2_source, ch.pin2_addr, ch.pin2_channel + b, (segByte >> b) & 1);
                        }
                    } else if (ch.mc_mode >= 2 && ch.pin2_source == 0) {
                        // Direct Drive via GPIO
                        uint8_t numSeg = (ch.mc_mode == 3 || ch.mc_mode == 5) ? 8 : 7;
                        uint8_t segByte = asciiToSegment(ch.dmxBuffer[0]);
                        if (ch.type == 12 || ch.type == 13) {
                            // PWM Direct Drive via LEDC
                            uint8_t brightness = 255;
                            if (ch.mc_mode == 4 || ch.mc_mode == 5) {
                                brightness = ch.dmxBuffer[1]; // DMX Channel 2
                            }
                            for (uint8_t b = 0; b < numSeg; b++) {
                                uint32_t duty = ((segByte >> b) & 1) ? brightness : 0;
                                if (ch.dmxPort + b <= 15) {
                                    ledcWrite(ch.dmxPort + b, duty);
                                }
                            }
                        } else {
                            for (uint8_t b = 0; b < numSeg; b++) {
                                uint8_t segPin = (ch.seg_pins[b] != 255) ? ch.seg_pins[b] : (ch.pin + b);
                                if (segPin != 255) {
                                    digitalWrite(segPin, (segByte >> b) & 1);
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
