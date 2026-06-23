#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H

#include <Arduino.h>
#include <atomic>
#include <esp_dmx.h>
#include <NeoPixelBus.h>
#include "output_devices/rmt_dmx.h"
#include <vector>
#include <algorithm>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "output_devices/dfplayer_control.h"
#include "i2c_devices/pca9685.h"

#define SENTINEL_NONE 255
#include "i2c_devices/i2c_gpio_expander.h"
#include "output_devices/funcgen_control.h"
#include "output_defs.h"
#include "output_devices/seven_seg_digits.h"

extern PCA9685Manager pcaManager;
extern DigitalExpanderManager digitalExpanderManager;

#define DMX_BUFFER_SIZE 512

inline uint8_t dmxValueByteCount(uint8_t resolution) {
    if (resolution <= 8) return 1;
    if (resolution <= 16) return 2;
    if (resolution <= 24) return 3;
    return 4;
}

extern std::atomic<unsigned long> lastDmxUpdateTime;
extern std::atomic<bool> systemActive;
extern std::atomic<bool> networkFramePending;

class PixelStripWrapper {
public:
    virtual ~PixelStripWrapper() {}
    virtual void Begin() = 0;
    virtual bool CanShow() const = 0;
    virtual void Show() = 0;
    virtual void SetPixelColor(uint16_t index, RgbColor color) = 0;
    virtual void SetPixelColorRgbw(uint16_t index, RgbwColor color) {}
    virtual bool IsRgbw() const { return false; }
};

template <typename T_Method>
class PixelStripRmt : public PixelStripWrapper {
private:
    NeoPixelBus<NeoRgbFeature, T_Method>* strip = nullptr;
public:
    PixelStripRmt(uint16_t count, uint8_t pin) {
        strip = new NeoPixelBus<NeoRgbFeature, T_Method>(count, pin);
    }
    ~PixelStripRmt() override {
        if (strip) {
            delete strip;
        }
    }
    void Begin() override {
        if (strip) strip->Begin();
    }
    bool CanShow() const override {
        return strip && strip->CanShow();
    }
    void Show() override {
        if (strip) strip->Show();
    }
    void SetPixelColor(uint16_t index, RgbColor color) override {
        if (strip) strip->SetPixelColor(index, color);
    }
    bool IsRgbw() const override { return false; }
};


template <typename T_Method>
class PixelStripRmtRgbw : public PixelStripWrapper {
private:
    NeoPixelBus<NeoRgbwFeature, T_Method>* strip = nullptr;
public:
    PixelStripRmtRgbw(uint16_t count, uint8_t pin) {
        strip = new NeoPixelBus<NeoRgbwFeature, T_Method>(count, pin);
    }
    ~PixelStripRmtRgbw() override {
        if (strip) {
            delete strip;
        }
    }
    void Begin() override {
        if (strip) strip->Begin();
    }
    bool CanShow() const override {
        return strip && strip->CanShow();
    }
    void Show() override {
        if (strip) strip->Show();
    }
    void SetPixelColor(uint16_t index, RgbColor color) override {
        // For RGB data on RGBW strip: map R,G,B to R,G,B,W(0)
        if (strip) strip->SetPixelColor(index, RgbwColor(color.R, color.G, color.B, 0));
    }
    void SetPixelColorRgbw(uint16_t index, RgbwColor color) override {
        if (strip) strip->SetPixelColor(index, color);
    }
    bool IsRgbw() const override { return true; }
};

struct OutputChannel {
    uint8_t type = 0;
    uint8_t source = 0;
    uint8_t pin = 255;
    uint8_t pca_addr = 0x40;
    uint8_t pca_channel = 0;
    uint8_t pca_channel2 = 255;
    uint8_t pca_channel3 = 255;
    uint8_t pca_channel4 = 255;
    uint16_t start_universe = 0;
    uint16_t start_address = 1;
    uint16_t led_count = 0;
    uint8_t color_order = 0;
    uint8_t led_protocol = 0;
    uint8_t pwm_dac_mode = 0; // 0=custom, 1=0-10V, 2=4-20mA
    uint16_t pwm_dac_min = 0; // duty percent * 100
    uint16_t pwm_dac_max = 10000;
    

    uint16_t smoke_duration_ms = 1000;
    uint16_t settle_delay_ms = 500;
    uint16_t shoot_duration_ms = 1000;
    uint16_t smoke_lockout_ms = 2000;

    uint8_t smoke_state = 0;
    unsigned long smoke_timer = 0;
    bool smoke_prev_trigger = false;
    uint32_t prev_7seg_val = 0;
    bool prev_7seg_valid = false;
    
    uint8_t pin2 = 255;
    uint8_t pin3 = 255;
    uint8_t pin4 = 255;
    uint8_t pin4_source = 0;
    uint8_t pin4_addr = 0x20;
    uint8_t pin4_channel = 255;
    uint8_t pin2_source = 0;
    uint8_t pin2_addr = 0x20;
    uint8_t pin2_channel = 255;
    uint8_t pin3_source = 0;
    uint8_t pin3_addr = 0x20;
    uint8_t pin3_channel = 255;
    uint8_t mc_resolution = 8; // 8, 10, 12, 16 bit
    uint16_t mc_freq = 1000;      // PWM Frequency / Speed
    uint8_t mc_mode = 0;       // Motor Sub-mode
    uint8_t mc_deadband = 10;
    bool mc_invert = false;
    bool mc_brake = true;
    uint16_t mc_min_us = 1000;
    uint16_t mc_max_us = 2000;
    uint16_t mc_steps_per_rev = 200;
    uint8_t mc_homing_dir = 0; // 0 = Forward, 1 = Reverse
    uint8_t mc_homing_mode = 0; // 0 = Sensor, 1 = Time/Stall
    uint16_t mc_homing_speed = 500; // speed in Hz during homing
    uint16_t mc_homing_timeout = 5; // homing timeout in seconds
    float mc_scale_factor = 0.0f;
    uint8_t mc_unit_type = 0; // 0 = Steps, 1 = Degrees, 2 = mm
    bool mc_enable_active_high = false;
    bool mc_dir_invert = false;
    bool mc_step_invert = false;
    bool pin_invert = false;
    bool pin2_invert = false;
    bool pin3_invert = false;
    bool pin4_invert = false;
    
    uint8_t stepper_cmd_state = 0;
    unsigned long stepper_cmd_start_time = 0;
    unsigned long stepper_homing_start_time = 0;
    
    uint8_t ledc_chan2 = 255;
    uint8_t ledc_chan3 = 255;
    uint8_t ledc_chan4 = 255;
    uint8_t seg_pins[8] = {255,255,255,255,255,255,255,255};
    uint8_t seg_sources[8] = {0,0,0,0,0,0,0,0};
    uint8_t seg_addrs[8] = {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
    uint8_t seg_channels[8] = {255,255,255,255,255,255,255,255};
    uint8_t seg_inverts = 0;
    
    uint8_t* dmxBuffer = nullptr;
    uint8_t* shadowBuffer = nullptr;
    uint16_t bufferSize = 0;
    PixelStripWrapper* pixelStrip = nullptr;
    uint8_t dmxPort = 255;
    RmtDmxDriver* rmtDmx = nullptr;
    uint16_t solenoid_pulse_ms = 50;
    uint8_t solenoid_mode = 0;
    uint8_t solenoid_threshold = 127;
    uint16_t solenoid_pre_delay = 0;
    uint16_t solenoid_post_delay = 100;
    unsigned long solenoid_pulse_start = 0;
    bool solenoid_pulse_active = false;
    unsigned long solenoid_last_trigger = 0;
    DFPlayerController* dfPlayer = nullptr;
    FuncGenController* funcGen = nullptr;
};

inline uint16_t outputDmxByteCount(const OutputChannel& ch) {
    int8_t mode = (int8_t)ch.mc_mode;
    switch (ch.type) {
        case 1:
            return DMX_BUFFER_SIZE;
        case 3: {
            uint8_t bytesPerPixel = ch.color_order >= 4 ? 4 : 3;
            uint16_t pixelsPerUniverse = DMX_BUFFER_SIZE / bytesPerPixel;
            uint16_t universes = (ch.led_count + pixelsPerUniverse - 1) / pixelsPerUniverse;
            if (universes < 1) universes = 1;
            return universes * DMX_BUFFER_SIZE;
        }
        case 5:
            return ch.color_order >= 4 ? 4 : 3;
        case 7:
            return dmxValueByteCount(ch.mc_resolution) + 2;
        case 9:
        case 11:
            return mode == 1 ? 4 : 2;
        case 12:
        case 13:
            return (mode == 4 || mode == 5 || mode >= 6) ? 2 : 1;
        case 10:
            return 3;
        case 16:
            return 5;
        case 4:
        case 6:
        case 8:
        case 15:
            return dmxValueByteCount(ch.mc_resolution);
        default:
            return 1;
    }
}

inline void writeOutputPin(OutputChannel& ch, uint8_t pinNum, bool state) {
    uint8_t source = 0;
    uint8_t address = 0x20;
    uint8_t channel = 255;
    uint8_t gpio = 255;
    bool inv = false;
    
    if (pinNum == 1) {
        source = ch.source;
        address = ch.pca_addr;
        channel = ch.pca_channel;
        gpio = ch.pin;
        inv = ch.pin_invert;
    } else if (pinNum == 2) {
        source = ch.pin2_source;
        address = ch.pin2_addr;
        channel = ch.pin2_channel;
        gpio = ch.pin2;
        inv = ch.pin2_invert;
        if (source == 0 && ch.source != 0 && ch.pca_channel2 != 255) {
            source = ch.source;
            address = ch.pca_addr;
            channel = ch.pca_channel2;
            gpio = 255;
        }
    } else if (pinNum == 3) {
        source = ch.pin3_source;
        address = ch.pin3_addr;
        channel = ch.pin3_channel;
        gpio = ch.pin3;
        inv = ch.pin3_invert;
        if (source == 0 && ch.source != 0 && ch.pca_channel3 != 255) {
            source = ch.source;
            address = ch.pca_addr;
            channel = ch.pca_channel3;
            gpio = 255;
        }
    } else if (pinNum == 4) {
        source = ch.pin4_source;
        address = ch.pin4_addr;
        channel = ch.pin4_channel;
        gpio = ch.pin4;
        inv = ch.pin4_invert;
        if (source == 0 && ch.source != 0 && ch.pca_channel4 != 255) {
            source = ch.source;
            address = ch.pca_addr;
            channel = ch.pca_channel4;
            gpio = 255;
        }
    }
    
    if (gpio == 255 && channel == 255) return;
    if (source == 0) { // ESP32 GPIO
        if (gpio != 255) {
            digitalWrite(gpio, (state ^ inv) ? HIGH : LOW);
        }
    } else if (source == 1) { // PCA9685
        if (channel != 255) {
            pcaManager.write(address, channel, (state ^ inv) ? 4095 : 0);
        }
    } else if (source >= 2 && source <= 4) { // Digital expander
        if (channel != 255) {
            digitalExpanderManager.write(source, address, channel, state ^ inv);
        }
    }
}

inline bool readOutputPin(OutputChannel& ch, uint8_t pinNum) {
    uint8_t source = 0;
    uint8_t address = 0x20;
    uint8_t channel = 255;
    uint8_t gpio = 255;
    bool inv = false;
    
    if (pinNum == 1) {
        source = ch.source;
        address = ch.pca_addr;
        channel = ch.pca_channel;
        gpio = ch.pin;
        inv = ch.pin_invert;
    } else if (pinNum == 2) {
        source = ch.pin2_source;
        address = ch.pin2_addr;
        channel = ch.pin2_channel;
        gpio = ch.pin2;
        inv = ch.pin2_invert;
    } else if (pinNum == 3) {
        source = ch.pin3_source;
        address = ch.pin3_addr;
        channel = ch.pin3_channel;
        gpio = ch.pin3;
        inv = ch.pin3_invert;
    } else if (pinNum == 4) {
        source = ch.pin4_source;
        address = ch.pin4_addr;
        channel = ch.pin4_channel;
        gpio = ch.pin4;
        inv = ch.pin4_invert;
    }
    
    bool val = false;
    if (source == 0) {
        if (gpio != 255) {
            val = digitalRead(gpio) == HIGH;
        }
    } else if (source >= 2 && source <= 4) {
        if (channel != 255) {
            val = digitalExpanderManager.digitalRead(source, address, channel);
        }
    }
    return val ^ inv;
}

inline uint8_t asciiToSegment(uint8_t c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= '0' && c <= '9') {
        return SEG_DIGITS[c - '0'];
    }
    switch (c) {
        case 'A': return 0x77;
        case 'B': return 0x7C;
        case 'C': return 0x39;
        case 'D': return 0x5E;
        case 'E': return 0x79;
        case 'F': return 0x71;
        case 'G': return 0x3D;
        case 'H': return 0x76;
        case 'I': return 0x06;
        case 'J': return 0x0E;
        case 'K': return 0x76;
        case 'L': return 0x38;
        case 'M': return 0x37;
        case 'N': return 0x54;
        case 'O': return 0x3F;
        case 'P': return 0x73;
        case 'Q': return 0x67;
        case 'R': return 0x50;
        case 'S': return 0x6D;
        case 'T': return 0x78;
        case 'U': return 0x3E;
        case 'V': return 0x1C;
        case 'W': return 0x3E;
        case 'X': return 0x40;
        case 'Y': return 0x6E;
        case 'Z': return 0x5B;
        case '-': return 0x40;
        case '_': return 0x08;
        case ' ': return 0x00;
        default: return 0x00;
    }
}

class OutputControl {
private:
    std::vector<OutputChannel> channels;

    uint16_t getPcaSharedFrequency(uint8_t address) const {
        uint16_t firstConfiguredFreq = 0;
        for (const auto& candidate : channels) {
            if (candidate.source != 1 || candidate.pca_addr != address) continue;
            if (candidate.type == OutputDefs::TYPE_SERVO) {
                return 50; // PCA9685 frequency is shared per chip; RC servo requires 50 Hz.
            }
            if ((candidate.type == OutputDefs::TYPE_SINGLE_LED || candidate.type == OutputDefs::TYPE_MOTOR ||
                 candidate.type == OutputDefs::TYPE_ANALOG_RGB || candidate.type == OutputDefs::TYPE_SMOKE || candidate.type == OutputDefs::TYPE_PWM_DAC) &&
                firstConfiguredFreq == 0) {
                firstConfiguredFreq = candidate.mc_freq;
            }
        }
        return firstConfiguredFreq > 0 ? firstConfiguredFreq : 1000;
    }

public:
    uint16_t getUniverseCount(const OutputChannel& ch) const {
        if (ch.type == OutputDefs::TYPE_LED_STRIP) { // RGB LED strip (v3)
            uint8_t bytesPerPixel = (ch.color_order >= 4) ? 4 : 3;
            uint16_t pixelsPerUniverse = 512 / bytesPerPixel;
            return (ch.led_count + pixelsPerUniverse - 1) / pixelsPerUniverse;
        }
        return 1;
    }

    bool mapDmxDataToChannels(uint16_t universe, const uint8_t* data, uint16_t length, bool updateActiveBuffer = true, uint16_t dataOffset = 0) {
        if (data == nullptr) return false;
        if (dataOffset >= DMX_BUFFER_SIZE) return false;

        bool matched = false;
        uint16_t maxLen = DMX_BUFFER_SIZE - dataOffset;
        uint16_t copyLen = length > maxLen ? maxLen : length;

        for (auto& ch : channels) {
            if (ch.shadowBuffer == nullptr) continue;

            if (ch.type == OutputDefs::TYPE_LED_STRIP) { // RGB LED strip (v3)
                uint16_t numUniverses = getUniverseCount(ch);
                if (universe >= ch.start_universe && universe < ch.start_universe + numUniverses) {
                    uint16_t universeOffset = universe - ch.start_universe;
                    uint32_t bufferOffset = ((uint32_t)universeOffset * DMX_BUFFER_SIZE) + dataOffset;
                    if (bufferOffset < ch.bufferSize) {
                        uint32_t available = (uint32_t)ch.bufferSize - bufferOffset;
                        uint32_t lenToCopy = min((uint32_t)copyLen, available);
                        memcpy(ch.shadowBuffer + bufferOffset, data, lenToCopy);
                        matched = true;
                    }
                }
            } else if (ch.type == OutputDefs::TYPE_DMX && ch.start_universe == universe) {
                if (dataOffset < ch.bufferSize) {
                    uint32_t available = (uint32_t)ch.bufferSize - dataOffset;
                    uint32_t lenToCopy = min((uint32_t)copyLen, available);
                    memcpy(ch.shadowBuffer + dataOffset, data, lenToCopy);
                    matched = true;
                }
            } else if (ch.start_universe == universe) {
                uint16_t sourceStart = constrain((int)ch.start_address, 1, DMX_BUFFER_SIZE) - 1;
                uint16_t chunkStart = dataOffset;
                uint16_t chunkEnd = dataOffset + copyLen;
                uint16_t sourceEnd = sourceStart + ch.bufferSize;
                if (sourceEnd > DMX_BUFFER_SIZE) sourceEnd = DMX_BUFFER_SIZE;

                uint16_t overlapStart = max(chunkStart, sourceStart);
                uint16_t overlapEnd = min(chunkEnd, sourceEnd);
                if (overlapStart < overlapEnd) {
                    uint16_t srcOffset = overlapStart - chunkStart;
                    uint16_t dstOffset = overlapStart - sourceStart;
                    uint16_t lenToCopy = overlapEnd - overlapStart;
                    memcpy(ch.shadowBuffer + dstOffset, data + srcOffset, lenToCopy);
                    matched = true;
                }
            }
        }

        if (updateActiveBuffer && universe == 0) {
            matched = true;
        }

        return matched;
    }

    void swapBuffers() {
        if (swapMutex && xSemaphoreTake(swapMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
        for (auto& ch : channels) {
            if (ch.shadowBuffer != nullptr && ch.dmxBuffer != nullptr) {
                uint8_t* tmp = ch.dmxBuffer;
                ch.dmxBuffer = ch.shadowBuffer;
                ch.shadowBuffer = tmp;
            }
        }
        if (swapMutex) xSemaphoreGive(swapMutex);
    }

    void setupChannels();

    void begin() {
        loadChannels();
        setupChannels();
        Serial.println("Output Control initialized");
    }

    void loadChannels() {
        clearChannels();

        File file = LittleFS.open("/outputs.json", "r");
        if (!file) {
            Serial.println("/outputs.json not found. Creating default output channel...");
            OutputChannel defCh;
            defCh.type = 3; // RGB LED strip (v3)
            defCh.pin = DEFAULT_LED_DATA_PIN;
            defCh.start_universe = 0;
            defCh.start_address = 1;
            defCh.led_count = 170;
            defCh.color_order = COLOR_GRB;
            defCh.bufferSize = outputDmxByteCount(defCh);
            defCh.dmxBuffer = (uint8_t*)calloc(defCh.bufferSize, 1);
            if (defCh.dmxBuffer == nullptr) {
                Serial.println("OOM error allocating default channel DMX buffer!");
                return;
            }
            defCh.shadowBuffer = (uint8_t*)calloc(defCh.bufferSize, 1);
            if (defCh.shadowBuffer == nullptr) {
                Serial.println("OOM error allocating default channel shadow buffer!");
                return;
            }
            defCh.pixelStrip = nullptr;
            defCh.dmxPort = 255;
            
            channels.push_back(defCh);
            saveChannels();
            return;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.println("Failed to read outputs.json. Using fallback default.");
            OutputChannel defCh;
            defCh.type = 3; // RGB LED strip (v3)
            defCh.pin = DEFAULT_LED_DATA_PIN;
            defCh.start_universe = 0;
            defCh.start_address = 1;
            defCh.led_count = 170;
            defCh.color_order = COLOR_GRB;
            defCh.bufferSize = outputDmxByteCount(defCh);
            defCh.dmxBuffer = (uint8_t*)calloc(defCh.bufferSize, 1);
            if (defCh.dmxBuffer == nullptr) {
                Serial.println("OOM error allocating default fallback DMX buffer!");
                return;
            }
            defCh.pixelStrip = nullptr;
            defCh.dmxPort = 255;
            
            channels.push_back(defCh);
            return;
        }
        JsonArray arr = doc["outputs"].as<JsonArray>();
        int layoutVersion = doc["version"] | 1;
        for (JsonObject item : arr) {
            // No hard limit — enforced by scoring + hardware resource validation
            OutputChannel ch;
            uint8_t rawType = item["type"] | 0;
            if (layoutVersion < 2) {
                // v1→v2→v3 migration: skip deprecated WiZ (v1 type 4), map everything else to v3
                if (rawType == 4) {
                    Serial.println("Skipping deprecated WiZ output channel from /outputs.json during migration");
                    continue;
                } else if (rawType == 5) ch.type = 6; // Motor → v3 6
                else if (rawType == 6) ch.type = 7; // Stepper → v3 7
                else if (rawType == 7) ch.type = 8; // Servo → v3 8
                else if (rawType == 8) ch.type = 17; // Solenoid → v3 17
                else if (rawType == 9) ch.type = 5; // Analog RGB → v3 5
                else if (rawType == 10) ch.type = 9; // Buzzer → v3 9
                else if (rawType == 11) ch.type = 18; // Smoke → v3 18
                else if (rawType == 12) ch.type = 11; // 7-Seg → v3 11 (2-pin)
                else if (rawType == 13) ch.type = 10; // DFPlayer → v3 10
                else if (rawType == 15) ch.type = 15; // PWM DAC (stays 15)
                else if (rawType == 14) ch.type = 14; // DAC
                else if (rawType == 16) ch.type = 16; // Func Gen
                else ch.type = rawType;
            } else if (layoutVersion == 2) {
                // v2→v3 migration
                if (rawType == 4) ch.type = 4; // PWM Dimmer → Single Color LED (stays)
                else if (rawType == 5) ch.type = 6; // Motor → v3
                else if (rawType == 6) ch.type = 7; // Stepper → v3
                else if (rawType == 7) ch.type = 8; // Servo → v3
                else if (rawType == 8) ch.type = 17; // Solenoid → v3
                else if (rawType == 9) ch.type = 5; // Analog RGB → v3
                else if (rawType == 10) ch.type = 9; // Buzzer → v3
                else if (rawType == 11) ch.type = 18; // Smoke → v3
                else if (rawType == 12) ch.type = 11; // 7-Seg → v3 2-pin
                else if (rawType == 13) ch.type = 10; // DFPlayer → v3
                else if (rawType == 14) ch.type = 14; // DAC (stays)
                else if (rawType == 15) ch.type = 15; // PWM DAC (stays)
                else if (rawType == 16) ch.type = 16; // Func Gen (stays)
                else ch.type = rawType;
            } else {
                ch.type = rawType;
            }
            ch.source = item["source"] | 0;
            ch.pin = item["pin"] | 255;
            ch.pca_addr = item["pca_addr"] | 0x40;
            ch.pca_channel = item["pca_channel"] | 0;
            ch.pca_channel2 = item["pca_channel2"] | 255;
            ch.pca_channel3 = item["pca_channel3"] | 255;
            ch.pca_channel4 = item["pca_channel4"] | 255;
            ch.ledc_chan3 = 255;
            ch.ledc_chan4 = 255;
            ch.start_universe = item["start_universe"] | 0;
            ch.start_address = constrain((int)(item["start_address"] | 1), 1, DMX_BUFFER_SIZE);
            ch.led_count = item["led_count"] | 170;
            ch.color_order = item["color_order"] | 0;
            ch.led_protocol = item["led_protocol"] | 0;
            ch.pwm_dac_mode = item["pwm_dac_mode"] | 0;
            ch.pwm_dac_min = constrain((int)(item["pwm_dac_min"] | 0), 0, 10000);
            ch.pwm_dac_max = constrain((int)(item["pwm_dac_max"] | 10000), 0, 10000);
            ch.pixelStrip = nullptr;
            ch.dmxPort = 255;
            
            ch.pin2 = item["pin2"] | 255;
            ch.pin3 = item["pin3"] | 255;
            ch.pin4 = item["pin4"] | 255;
            ch.pin4_source = item["pin4_source"] | 0;
            ch.pin4_addr = item["pin4_addr"] | 0x20;
            ch.pin4_channel = item["pin4_channel"] | 255;
            ch.pin2_source = item["pin2_source"] | 0;
            ch.pin2_addr = item["pin2_addr"] | 0x20;
            ch.pin2_channel = item["pin2_channel"] | 255;
            ch.pin3_source = item["pin3_source"] | 0;
            ch.pin3_addr = item["pin3_addr"] | 0x20;
            ch.pin3_channel = item["pin3_channel"] | 255;
            ch.mc_resolution = item["mc_resolution"] | 8;
            if (ch.type != 7 && ch.mc_resolution > 16) ch.mc_resolution = 16;
            if (ch.mc_resolution == 0) ch.mc_resolution = 8;
            ch.mc_freq = item["mc_freq"] | 1000;
            ch.mc_mode = item["mc_mode"] | 0;
            ch.mc_deadband = item["mc_deadband"] | 10;
            ch.mc_invert = item["mc_invert"] | false;
            ch.mc_brake = item["mc_brake"] | true;
            ch.mc_min_us = item["mc_min_us"] | 1000;
            ch.mc_max_us = item["mc_max_us"] | 2000;
            ch.mc_steps_per_rev = item["mc_steps_per_rev"] | 200;
            ch.mc_homing_dir = item["mc_homing_dir"] | 0;
            ch.mc_homing_mode = item["mc_homing_mode"] | 0;
            ch.mc_homing_speed = item["mc_homing_speed"] | 500;
            ch.mc_homing_timeout = item["mc_homing_timeout"] | 5;
            ch.mc_scale_factor = item["mc_scale_factor"] | 0.0f;
            ch.mc_unit_type = item["mc_unit_type"] | 0;
            ch.mc_enable_active_high = item["mc_enable_active_high"] | false;
            ch.mc_dir_invert = item["mc_dir_invert"] | false;
            ch.mc_step_invert = item["mc_step_invert"] | false;
            ch.pin_invert = item.containsKey("pin_invert") ? item["pin_invert"].as<bool>() : ch.mc_step_invert;
            ch.pin2_invert = item.containsKey("pin2_invert") ? item["pin2_invert"].as<bool>() : ch.mc_dir_invert;
            ch.pin3_invert = item.containsKey("pin3_invert") ? item["pin3_invert"].as<bool>() : ch.mc_enable_active_high;
            ch.pin4_invert = item["pin4_invert"] | false;
            ch.solenoid_mode = item["solenoid_mode"] | 0;
            ch.solenoid_threshold = item["solenoid_threshold"] | 127;
            ch.solenoid_pulse_ms = item["solenoid_pulse_ms"] | 50;
            ch.solenoid_pre_delay = item["solenoid_pre_delay"] | 0;
            ch.solenoid_post_delay = item["solenoid_post_delay"] | 100;
            ch.smoke_duration_ms = item["smoke_duration_ms"] | 1000;
            ch.settle_delay_ms = item["settle_delay_ms"] | 500;
            ch.shoot_duration_ms = item["shoot_duration_ms"] | 1000;
            ch.smoke_lockout_ms = item["smoke_lockout_ms"] | 2000;
            if (item.containsKey("seg_pins")) {
                JsonArray segArr = item["seg_pins"].as<JsonArray>();
                for (int s = 0; s < 8; s++) {
                    if (s < segArr.size()) {
                        ch.seg_pins[s] = segArr[s] | 255;
                    } else {
                        ch.seg_pins[s] = 255;
                    }
                }
            } else {
                for (int s = 0; s < 8; s++) {
                    ch.seg_pins[s] = 255;
                }
            }
            for (int s = 0; s < 8; s++) {
                ch.seg_sources[s] = 0;
                ch.seg_addrs[s] = 0x20;
                ch.seg_channels[s] = 255;
            }
            if (item.containsKey("seg_sources")) {
                JsonArray arr = item["seg_sources"].as<JsonArray>();
                for (int s = 0; s < 8 && s < arr.size(); s++) ch.seg_sources[s] = arr[s] | 0;
            }
            if (item.containsKey("seg_addrs")) {
                JsonArray arr = item["seg_addrs"].as<JsonArray>();
                for (int s = 0; s < 8 && s < arr.size(); s++) ch.seg_addrs[s] = arr[s] | 0x20;
            }
            if (item.containsKey("seg_channels")) {
                JsonArray arr = item["seg_channels"].as<JsonArray>();
                for (int s = 0; s < 8 && s < arr.size(); s++) ch.seg_channels[s] = arr[s] | 255;
            }
            ch.seg_inverts = item["seg_inverts"] | 0;
            ch.bufferSize = outputDmxByteCount(ch);
            ch.dmxBuffer = (uint8_t*)calloc(ch.bufferSize, 1);
            if (ch.dmxBuffer == nullptr) {
                Serial.printf("OOM error allocating DMX buffer for channel type %d!\n", ch.type);
                continue;
            }
            ch.shadowBuffer = (uint8_t*)calloc(ch.bufferSize, 1);
            if (ch.shadowBuffer == nullptr) {
                Serial.printf("OOM error allocating shadow buffer for channel type %d!\n", ch.type);
                continue;
            }
            channels.push_back(ch);
        }
        if (layoutVersion < 3) {
            saveChannels();
        }
        Serial.printf("Loaded %d output channels from /outputs.json\n", channels.size());
    }
 
     bool saveChannels() {
         File file = LittleFS.open("/outputs.json", "w");
         if (!file) {
             Serial.println("[config] ERROR: Failed to open outputs.json for writing");
             return false;
         }

         JsonDocument doc;
         doc["version"] = 3;
         JsonArray arr = doc["outputs"].to<JsonArray>();
         for (const auto& ch : channels) {
             const OutputDefs::OutputModeDef* modeDef = OutputDefs::modeDef(ch.type, ch.mc_mode);
             JsonObject item = arr.add<JsonObject>();
             item["type"] = ch.type;
             item["source"] = ch.source;
             item["pin"] = ch.pin;
             item["pca_addr"] = ch.pca_addr;
             item["pca_channel"] = ch.pca_channel;
             item["pca_channel2"] = ch.pca_channel2;
             item["pca_channel3"] = ch.pca_channel3;
             item["pca_channel4"] = ch.pca_channel4;
             item["start_universe"] = ch.start_universe;
             item["start_address"] = ch.start_address;
             item["led_count"] = ch.led_count;
             item["color_order"] = ch.color_order;
             item["led_protocol"] = ch.led_protocol;
             item["pwm_dac_mode"] = ch.pwm_dac_mode;
             item["pwm_dac_min"] = ch.pwm_dac_min;
             item["pwm_dac_max"] = ch.pwm_dac_max;
             item["pin_invert"] = ch.pin_invert;
             item["pin2_invert"] = ch.pin2_invert;
             item["pin3_invert"] = ch.pin3_invert;
             item["pin4_invert"] = ch.pin4_invert;
             item["smoke_duration_ms"] = ch.smoke_duration_ms;
             item["settle_delay_ms"] = ch.settle_delay_ms;
             item["shoot_duration_ms"] = ch.shoot_duration_ms;
             item["smoke_lockout_ms"] = ch.smoke_lockout_ms;

                if (ch.type >= 4 && ch.type <= 8) {
                    item["pin2"] = ch.pin2;
                    item["pin3"] = ch.pin3;
                    item["pin4"] = ch.pin4;
                    if (ch.type >= 5 && ch.type <= 7) {
                        item["pin4_source"] = ch.pin4_source;
                        item["pin4_addr"] = ch.pin4_addr;
                        item["pin4_channel"] = ch.pin4_channel;
                        item["pin2_source"] = ch.pin2_source;
                        item["pin2_addr"] = ch.pin2_addr;
                        item["pin2_channel"] = ch.pin2_channel;
                        item["pin3_source"] = ch.pin3_source;
                        item["pin3_addr"] = ch.pin3_addr;
                        item["pin3_channel"] = ch.pin3_channel;
                    }
                    if (ch.type == OutputDefs::TYPE_STEPPER) {
                        item["mc_enable_active_high"] = ch.mc_enable_active_high;
                        item["mc_dir_invert"] = ch.mc_dir_invert;
                        item["mc_step_invert"] = ch.mc_step_invert;
                    }
                item["mc_resolution"] = ch.mc_resolution;
                item["mc_freq"] = ch.mc_freq;
                item["mc_mode"] = ch.mc_mode;
                item["mc_deadband"] = ch.mc_deadband;
                item["mc_invert"] = ch.mc_invert;
                item["mc_brake"] = ch.mc_brake;
                item["mc_min_us"] = ch.mc_min_us;
                item["mc_max_us"] = ch.mc_max_us;
                item["mc_steps_per_rev"] = ch.mc_steps_per_rev;
                item["mc_homing_dir"] = ch.mc_homing_dir;
                item["mc_homing_mode"] = ch.mc_homing_mode;
                item["mc_homing_speed"] = ch.mc_homing_speed;
                item["mc_homing_timeout"] = ch.mc_homing_timeout;
                item["mc_scale_factor"] = ch.mc_scale_factor;
                item["mc_unit_type"] = ch.mc_unit_type;
            } else if (ch.type == OutputDefs::TYPE_SOLENOID) {
                item["solenoid_mode"] = ch.solenoid_mode;
                item["solenoid_threshold"] = ch.solenoid_threshold;
                item["solenoid_pulse_ms"] = ch.solenoid_pulse_ms;
                item["solenoid_pre_delay"] = ch.solenoid_pre_delay;
                item["solenoid_post_delay"] = ch.solenoid_post_delay;
            } else if (ch.type == OutputDefs::TYPE_SMOKE) {
                item["pin2"] = ch.pin2;
                item["pin2_source"] = ch.pin2_source;
                item["pin2_addr"] = ch.pin2_addr;
                item["pin2_channel"] = ch.pin2_channel;
                item["solenoid_threshold"] = ch.solenoid_threshold;
            } else if (ch.type == OutputDefs::TYPE_BUZZER) {
                item["mc_freq"] = ch.mc_freq;
            } else if (modeDef != nullptr && modeDef->testUi == OutputDefs::TEST_UI_7SEG) {
                item["pin2"] = ch.pin2;
                item["pin2_source"] = ch.pin2_source;
                item["pin2_addr"] = ch.pin2_addr;
                item["pin2_channel"] = ch.pin2_channel;
                item["mc_mode"] = ch.mc_mode;
                if (modeDef->segmentCount > 0) {
                    JsonArray segArr = item["seg_pins"].to<JsonArray>();
                    for (int s = 0; s < 8; s++) {
                        segArr.add(ch.seg_pins[s]);
                    }
                    JsonArray segSources = item["seg_sources"].to<JsonArray>();
                    JsonArray segAddrs = item["seg_addrs"].to<JsonArray>();
                    JsonArray segChannels = item["seg_channels"].to<JsonArray>();
                    for (int s = 0; s < 8; s++) {
                        segSources.add(ch.seg_sources[s]);
                        segAddrs.add(ch.seg_addrs[s]);
                        segChannels.add(ch.seg_channels[s]);
                    }
                    item["seg_inverts"] = ch.seg_inverts;
                }
            } else if (ch.type == OutputDefs::TYPE_DFPLAYER) {
                item["pin2"] = ch.pin2;
            } else if (ch.type == OutputDefs::TYPE_PWM_DAC || ch.type == OutputDefs::TYPE_FUNC_GEN) {
                item["mc_freq"] = ch.mc_freq;
                item["mc_resolution"] = ch.mc_resolution;
            }
        }

        serializeJson(doc, file);
        file.close();
        return true;
    }
    void clearChannels() {
        for (auto& ch : channels) {
            if (ch.pixelStrip != nullptr) {
                delete ch.pixelStrip;
                ch.pixelStrip = nullptr;
            }
            if (ch.type == OutputDefs::TYPE_DMX && ch.dmxPort != 255) {
                dmx_driver_delete((dmx_port_t)ch.dmxPort);
                ch.dmxPort = 255;
            }
            if (ch.rmtDmx != nullptr) {
                delete ch.rmtDmx;
                ch.rmtDmx = nullptr;
            }
            if (ch.dmxBuffer != nullptr) {
                free(ch.dmxBuffer);
                ch.dmxBuffer = nullptr;
            }
            if (ch.shadowBuffer != nullptr) {
                free(ch.shadowBuffer);
                ch.shadowBuffer = nullptr;
            }
            if (ch.dfPlayer != nullptr) {
                ch.dfPlayer->stop();
                delete ch.dfPlayer;
                ch.dfPlayer = nullptr;
            }
            if (ch.funcGen != nullptr) {
                ch.funcGen->stop();
                delete ch.funcGen;
                ch.funcGen = nullptr;
            }
            ch.dmxPort = 255;
            ch.ledc_chan2 = 255;
            ch.ledc_chan3 = 255;
            ch.ledc_chan4 = 255;
        }
        channels.clear();
    }



    void loop();



    void updateLeds();

    std::vector<OutputChannel>& getChannels() {
        return channels;
    }

};

extern OutputControl outputCtrl;

// Per-type output device includes (after class definition to break circular deps)
#include "output_devices/relay.h"
#include "output_devices/solenoid.h"
#include "output_devices/smoke_shooter.h"
#include "output_devices/dmx.h"
#include "output_devices/led_strip.h"

inline void OutputControl::loop() {
    static unsigned long lastDmxSend = 0;
    unsigned long now = millis();
    uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
    unsigned long interval = 1000 / fps;
    if (now - lastDmxSend >= interval) {
        lastDmxSend = now;
        dmxUpdate();
        relayUpdate();
        solenoidUpdate();
        smokeShooterUpdate();
    }
}

inline void OutputControl::updateLeds() {
    ledStripUpdate();
    extern void updateMotionControl();
    updateMotionControl();
}

inline void OutputControl::setupChannels() {
    uint8_t rmtIdx = 0;
    uint8_t dmxIdx = 0;
    pcaManager.clear();
    digitalExpanderManager.clear();
    bool uart2Used = false;
    bool uart1Used = false;
    for (const auto& ch : channels) {
        if (ch.type == OutputDefs::TYPE_DFPLAYER) {
            if (!uart2Used) {
                uart2Used = true;
            } else if (!uart1Used) {
                uart1Used = true;
            }
        }
    }

    uint8_t dfPlayerCount = 0;
    uint8_t pcaAddrs[8];
    uint8_t pcaAddrCount = 0;

    for (auto& ch : channels) {
        if (ch.source == 1) {
            bool alreadyInitialized = false;
            for (uint8_t i = 0; i < pcaAddrCount; i++) {
                if (pcaAddrs[i] == ch.pca_addr) {
                    alreadyInitialized = true;
                    break;
                }
            }
            if (!alreadyInitialized) {
                uint16_t freq = getPcaSharedFrequency(ch.pca_addr);
                pcaManager.getOrCreateDriver(ch.pca_addr);
                pcaManager.setFrequency(ch.pca_addr, freq);
                if (pcaAddrCount < 8) pcaAddrs[pcaAddrCount++] = ch.pca_addr;
                Serial.printf("PCA9685 initialized at 0x%02X: freq=%dHz type=%d\n", ch.pca_addr, freq, ch.type);
            } else {
                pcaManager.getOrCreateDriver(ch.pca_addr);
                uint16_t freq = getPcaSharedFrequency(ch.pca_addr);
                Serial.printf("PCA9685 at 0x%02X already initialized (shared), type=%d uses freq=%dHz\n", ch.pca_addr, ch.type, freq);
            }
            continue;
        } else if (ch.source >= 2 && ch.source <= 4) {
            writeOutputPin(ch, 1, false);
            if ((ch.type == OutputDefs::TYPE_SMOKE || ch.type == OutputDefs::TYPE_MOTOR || ch.type == OutputDefs::TYPE_STEPPER) && ch.pca_channel2 != 255) {
                writeOutputPin(ch, 2, false);
            }
            if ((ch.type == OutputDefs::TYPE_MOTOR || ch.type == OutputDefs::TYPE_STEPPER) && ch.pca_channel3 != 255) {
                writeOutputPin(ch, 3, false);
            }
            Serial.printf("Expander initialized: src=%d addr=0x%02X type=%d\n", ch.source, ch.pca_addr, ch.type);
            continue;
        }
        if (ch.type == OutputDefs::TYPE_LED_STRIP) {
            ledStripSetup(ch, rmtIdx);
        } else if (ch.type == OutputDefs::TYPE_RELAY) {
            relaySetup(ch);
            Serial.printf("Relay init: GPIO%d U%d\n", ch.pin, ch.start_universe);
        } else if (ch.type == OutputDefs::TYPE_SOLENOID) {
            solenoidSetup(ch);
            Serial.printf("Solenoid init: GPIO%d U%d pulse=%dms thresh=%d\n",
                ch.pin, ch.start_universe, ch.solenoid_pulse_ms, ch.solenoid_threshold);
        } else if (ch.type == OutputDefs::TYPE_DMX) {
            dmxSetup(ch, uart2Used, uart1Used, rmtIdx);
        } else if (ch.type == OutputDefs::TYPE_SMOKE) {
            smokeShooterSetup(ch);
            Serial.printf("Smoke Shooter init: pin=%d pin2=%d thresh=%d\n", ch.pin, ch.pin2, ch.solenoid_threshold);
        } else if (ch.type == OutputDefs::TYPE_DFPLAYER) {
            ch.dfPlayer = new DFPlayerController();
            if (!ch.dfPlayer) {
                Serial.println("Error: DFPlayer malloc failed!");
                continue;
            }
            if (dfPlayerCount == 0) {
                ch.dfPlayer->begin(Serial2, ch.pin, ch.pin2);
                Serial.printf("DFPlayer (UART2): TX=%d RX=%d\n", ch.pin, ch.pin2);
                dfPlayerCount++;
            } else if (dfPlayerCount == 1) {
                ch.dfPlayer->begin(Serial1, ch.pin, ch.pin2);
                Serial.printf("DFPlayer (UART1): TX=%d RX=%d\n", ch.pin, ch.pin2);
                dfPlayerCount++;
            } else {
                Serial.println("Error: No UART for DFPlayer!");
            }
        }
    }
}

#endif // OUTPUT_CONTROL_H
