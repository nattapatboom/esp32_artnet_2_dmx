#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H

#include <Arduino.h>
#include <atomic>
#include <esp_dmx.h>
#include <NeoPixelBus.h>
#include "rmt_dmx.h"
#include <vector>
#include <algorithm>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"
#include "dfplayer_control.h"
#include "pca9685_control.h"
#include "i2c_gpio_expander.h"
#include "funcgen_control.h"

extern PCA9685Manager pcaManager;
extern DigitalExpanderManager digitalExpanderManager;

// Max universes supported per channel
#define DMX_BUFFER_SIZE 512
#define CHAN_TYPE_ANALOG_RGB 5

// Global active DMX channel buffer for current universe-0 frame state
extern uint8_t activeDmxBuffer[DMX_BUFFER_SIZE];
extern unsigned long lastDmxUpdateTime;
extern bool systemActive;
extern std::atomic<bool> networkFramePending;

// Interface for virtualized NeoPixelBus
class PixelStripWrapper {
public:
    virtual ~PixelStripWrapper() {}
    virtual void Begin() = 0;
    virtual void Show() = 0;
    virtual void SetPixelColor(uint16_t index, RgbColor color) = 0;
    virtual void SetPixelColorRgbw(uint16_t index, RgbwColor color) {
        // Default: RGB wrappers ignore this
    }
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
    void Show() override {
        if (strip) strip->Show();
    }
    void SetPixelColor(uint16_t index, RgbColor color) override {
        if (strip) strip->SetPixelColor(index, color);
    }
    bool IsRgbw() const override { return false; }
};

// RGBW (4-channel) LED Strip Wrapper
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
    uint8_t led_protocol = 0; // 0 = WS2812x (800kHz), 1 = WS2811 (400kHz)
    
    // Unicast feedback & Smoke parameters
    char dest_ip[16] = "192.168.1.100";
    uint16_t dest_port = 9000;
    uint16_t smoke_duration_ms = 1000;
    uint16_t settle_delay_ms = 500;
    uint16_t shoot_duration_ms = 1000;
    uint16_t smoke_lockout_ms = 2000;

    // Runtime state
    uint8_t smoke_state = 0; // 0=IDLE, 1=SMOKE, 2=SETTLE, 3=SHOOT, 4=COOLDOWN
    unsigned long smoke_timer = 0;
    bool smoke_prev_trigger = false;
    uint32_t prev_7seg_val = 0;
    bool prev_7seg_valid = false;
    
    // Motion Control (Phase 5.5) fields
    uint8_t pin2;
    uint8_t pin3;
    uint8_t pin4 = 255; // Limit Switch / Homing Sensor pin (255 = unused)
    uint8_t pin4_source = 0; // Homing sensor source: 0=ESP32 GPIO, 2=MCP23017, 3=TCA9555, 4=PCF857x
    uint8_t pin4_addr = 0x20; // Homing sensor expander I2C address
    uint8_t pin4_channel = 255; // Homing sensor expander channel (255 = unused)
    uint8_t pin2_source = 0; // Stepper DIR source: 0=ESP32, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x
    uint8_t pin2_addr = 0x20;
    uint8_t pin2_channel = 255;
    uint8_t pin3_source = 0; // Stepper ENABLE source
    uint8_t pin3_addr = 0x20;
    uint8_t pin3_channel = 255;
    uint8_t mc_resolution; // 8, 10, 12, 16 bit
    uint16_t mc_freq;      // PWM Frequency / Speed
    uint8_t mc_mode;       // Motor Sub-mode
    uint8_t mc_deadband;
    bool mc_invert;
    bool mc_brake;
    uint16_t mc_min_us;
    uint16_t mc_max_us;
    uint16_t mc_steps_per_rev;
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
    
    // Runtime Homing/Command state trackers
    uint8_t stepper_cmd_state = 0; // 0=normal, 1=stop_pending, 2=stop_active, 3=homing_pending, 4=homing_active, 5=homing_done
    unsigned long stepper_cmd_start_time = 0;
    unsigned long stepper_homing_start_time = 0;
    
    uint8_t ledc_chan2 = 255; // 2nd LEDC channel for DC Motor PWM+PWM mode (255 = unused)
    uint8_t ledc_chan3 = 255; // 3rd LEDC channel for Analog RGB Blue (255 = unused)
    uint8_t ledc_chan4 = 255; // 4th LEDC channel for Analog RGBW White (255 = unused)
    
    uint8_t* dmxBuffer = nullptr;
    uint16_t bufferSize = 0;
    
    PixelStripWrapper* pixelStrip = nullptr;
    uint8_t dmxPort = 255; // 255 = uninitialized, otherwise DMX_NUM_2 or DMX_NUM_1
    RmtDmxDriver* rmtDmx = nullptr;
    
    // Solenoid state
    uint16_t solenoid_pulse_ms = 50;    // Pulse width in milliseconds
    uint8_t solenoid_mode = 0;          // 0=threshold, 1=frame-sync
    uint8_t solenoid_threshold = 127;   // DMX value threshold
    uint16_t solenoid_pre_delay = 0;    // Delay before pulse in milliseconds
    uint16_t solenoid_post_delay = 100; // Delay after pulse before next allowed trigger
    unsigned long solenoid_pulse_start = 0;  // Timestamp when pulse started
    bool solenoid_pulse_active = false;      // True when pulse is currently driving GPIO
    unsigned long solenoid_last_trigger = 0; // Timestamp of last trigger

    // DFPlayer MP3 state
    DFPlayerController* dfPlayer = nullptr;

    // Function Generator state
    FuncGenController* funcGen = nullptr;
};

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
    // source == 1 (PCA9685) — cannot read back, return false
    return val ^ inv;
}

inline uint8_t asciiToSegment(uint8_t c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= '0' && c <= '9') {
        const uint8_t segDigits[] = {
            0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
        };
        return segDigits[c - '0'];
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
    uint8_t dmxTxBuffer[513]; // 1 start code + 512 channels
    
    std::vector<OutputChannel> channels;

    PixelStripWrapper* createPixelStrip(uint8_t rmtChannel, uint16_t count, uint8_t pin, uint8_t protocol) {
        if (protocol == 1) {
            switch (rmtChannel) {
                case 0: return new PixelStripRmt<NeoEsp32Rmt0Ws2811Method>(count, pin);
                case 1: return new PixelStripRmt<NeoEsp32Rmt1Ws2811Method>(count, pin);
                case 2: return new PixelStripRmt<NeoEsp32Rmt2Ws2811Method>(count, pin);
                case 3: return new PixelStripRmt<NeoEsp32Rmt3Ws2811Method>(count, pin);
                case 4: return new PixelStripRmt<NeoEsp32Rmt4Ws2811Method>(count, pin);
                case 5: return new PixelStripRmt<NeoEsp32Rmt5Ws2811Method>(count, pin);
                case 6: return new PixelStripRmt<NeoEsp32Rmt6Ws2811Method>(count, pin);
                case 7: return new PixelStripRmt<NeoEsp32Rmt7Ws2811Method>(count, pin);
                default: return nullptr;
            }
        }
        switch (rmtChannel) {
            case 0: return new PixelStripRmt<NeoEsp32Rmt0Ws2812xMethod>(count, pin);
            case 1: return new PixelStripRmt<NeoEsp32Rmt1Ws2812xMethod>(count, pin);
            case 2: return new PixelStripRmt<NeoEsp32Rmt2Ws2812xMethod>(count, pin);
            case 3: return new PixelStripRmt<NeoEsp32Rmt3Ws2812xMethod>(count, pin);
            case 4: return new PixelStripRmt<NeoEsp32Rmt4Ws2812xMethod>(count, pin);
            case 5: return new PixelStripRmt<NeoEsp32Rmt5Ws2812xMethod>(count, pin);
            case 6: return new PixelStripRmt<NeoEsp32Rmt6Ws2812xMethod>(count, pin);
            case 7: return new PixelStripRmt<NeoEsp32Rmt7Ws2812xMethod>(count, pin);
            default: return nullptr;
        }
    }

    PixelStripWrapper* createPixelStripRgbw(uint8_t rmtChannel, uint16_t count, uint8_t pin, uint8_t protocol) {
        if (protocol == 1) {
            switch (rmtChannel) {
                case 0: return new PixelStripRmtRgbw<NeoEsp32Rmt0Ws2811Method>(count, pin);
                case 1: return new PixelStripRmtRgbw<NeoEsp32Rmt1Ws2811Method>(count, pin);
                case 2: return new PixelStripRmtRgbw<NeoEsp32Rmt2Ws2811Method>(count, pin);
                case 3: return new PixelStripRmtRgbw<NeoEsp32Rmt3Ws2811Method>(count, pin);
                case 4: return new PixelStripRmtRgbw<NeoEsp32Rmt4Ws2811Method>(count, pin);
                case 5: return new PixelStripRmtRgbw<NeoEsp32Rmt5Ws2811Method>(count, pin);
                case 6: return new PixelStripRmtRgbw<NeoEsp32Rmt6Ws2811Method>(count, pin);
                case 7: return new PixelStripRmtRgbw<NeoEsp32Rmt7Ws2811Method>(count, pin);
                default: return nullptr;
            }
        }
        switch (rmtChannel) {
            case 0: return new PixelStripRmtRgbw<NeoEsp32Rmt0Ws2812xMethod>(count, pin);
            case 1: return new PixelStripRmtRgbw<NeoEsp32Rmt1Ws2812xMethod>(count, pin);
            case 2: return new PixelStripRmtRgbw<NeoEsp32Rmt2Ws2812xMethod>(count, pin);
            case 3: return new PixelStripRmtRgbw<NeoEsp32Rmt3Ws2812xMethod>(count, pin);
            case 4: return new PixelStripRmtRgbw<NeoEsp32Rmt4Ws2812xMethod>(count, pin);
            case 5: return new PixelStripRmtRgbw<NeoEsp32Rmt5Ws2812xMethod>(count, pin);
            case 6: return new PixelStripRmtRgbw<NeoEsp32Rmt6Ws2812xMethod>(count, pin);
            case 7: return new PixelStripRmtRgbw<NeoEsp32Rmt7Ws2812xMethod>(count, pin);
            default: return nullptr;
        }
    }

    uint16_t getPcaSharedFrequency(uint8_t address) const {
        uint16_t firstConfiguredFreq = 0;
        for (const auto& candidate : channels) {
            if (candidate.source != 1 || candidate.pca_addr != address) continue;
            if (candidate.type == 8) { // v3 8 = Servo
                return 50; // PCA9685 frequency is shared per chip; RC servo requires 50 Hz.
            }
            if ((candidate.type == 4 || candidate.type == 6 || // v3 6 = Motor
                 candidate.type == CHAN_TYPE_ANALOG_RGB || candidate.type == 18 || candidate.type == 15) && // v3 18 = Smoke Shooter, 15 = PWM DAC
                firstConfiguredFreq == 0) {
                firstConfiguredFreq = candidate.mc_freq;
            }
        }
        return firstConfiguredFreq > 0 ? firstConfiguredFreq : 1000;
    }

public:
    uint16_t getUniverseCount(const OutputChannel& ch) const {
        if (ch.type == 3) { // RGB LED strip (v3)
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
            if (ch.dmxBuffer == nullptr) continue;

            if (ch.type == 3) { // RGB LED strip (v3)
                uint16_t numUniverses = getUniverseCount(ch);
                if (universe >= ch.start_universe && universe < ch.start_universe + numUniverses) {
                    uint16_t universeOffset = universe - ch.start_universe;
                    uint32_t bufferOffset = ((uint32_t)universeOffset * DMX_BUFFER_SIZE) + dataOffset;
                    if (bufferOffset < ch.bufferSize) {
                        uint32_t available = (uint32_t)ch.bufferSize - bufferOffset;
                        uint32_t lenToCopy = min((uint32_t)copyLen, available);
                        memcpy(ch.dmxBuffer + bufferOffset, data, lenToCopy);
                        matched = true;
                    }
                }
            } else if (ch.type == 1 && ch.start_universe == universe) {
                if (dataOffset < ch.bufferSize) {
                    uint32_t available = (uint32_t)ch.bufferSize - dataOffset;
                    uint32_t lenToCopy = min((uint32_t)copyLen, available);
                    memcpy(ch.dmxBuffer + dataOffset, data, lenToCopy);
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
                    memcpy(ch.dmxBuffer + dstOffset, data + srcOffset, lenToCopy);
                    matched = true;
                }
            }
        }

        if (updateActiveBuffer && universe == 0) {
            memcpy(activeDmxBuffer + dataOffset, data, copyLen);
            matched = true;
        }

        return matched;
    }

    void begin() {
        memset(dmxTxBuffer, 0, sizeof(dmxTxBuffer));
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
            uint16_t numUniverses = getUniverseCount(defCh);
            defCh.bufferSize = numUniverses * 512;
            defCh.dmxBuffer = (uint8_t*)calloc(defCh.bufferSize, 1);
            if (defCh.dmxBuffer == nullptr) {
                Serial.println("OOM error allocating default channel DMX buffer!");
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
            uint16_t numUniverses = getUniverseCount(defCh);
            defCh.bufferSize = numUniverses * 512;
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
            ch.pin = item["pin"] | 4;
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
            if (ch.type != 7 && ch.mc_resolution > 16) ch.mc_resolution = 16; // Stepper v3 = 7
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
            
            // New network/smoke properties
            String destIp = item["dest_ip"] | "192.168.1.100";
            strncpy(ch.dest_ip, destIp.c_str(), sizeof(ch.dest_ip) - 1);
            ch.dest_ip[sizeof(ch.dest_ip) - 1] = '\0';
            ch.dest_port = item["dest_port"] | 9000;
            ch.smoke_duration_ms = item["smoke_duration_ms"] | 1000;
            ch.settle_delay_ms = item["settle_delay_ms"] | 500;
            ch.shoot_duration_ms = item["shoot_duration_ms"] | 1000;
            ch.smoke_lockout_ms = item["smoke_lockout_ms"] | 2000;
            
            if (ch.type == 3) { // RGB LED strip (v3)
                uint16_t numUniverses = getUniverseCount(ch);
                ch.bufferSize = numUniverses * 512;
            } else { // DMX and all other small types
                ch.bufferSize = 512;
            }
            ch.dmxBuffer = (uint8_t*)calloc(ch.bufferSize, 1);
            if (ch.dmxBuffer == nullptr) {
                Serial.printf("OOM error allocating DMX buffer for channel type %d!\n", ch.type);
                continue;
            }
            
            channels.push_back(ch);
        }
        if (layoutVersion < 3) {
            saveChannels();
            Serial.println("Saved migrated output channels to /outputs.json with version 3");
        }
        Serial.printf("Loaded %d output channels from /outputs.json\n", channels.size());
    }
 
     void saveChannels() {
         File file = LittleFS.open("/outputs.json", "w");
         if (!file) {
             Serial.println("Failed to open outputs.json for writing");
             return;
         }
 
         JsonDocument doc;
         doc["version"] = 3;
         JsonArray arr = doc["outputs"].to<JsonArray>();
         for (const auto& ch : channels) {
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
             item["pin_invert"] = ch.pin_invert;
             item["pin2_invert"] = ch.pin2_invert;
             item["pin3_invert"] = ch.pin3_invert;
             item["pin4_invert"] = ch.pin4_invert;
             
             // Serialize new v1.11.0 fields
             item["dest_ip"] = ch.dest_ip;
             item["dest_port"] = ch.dest_port;
             item["smoke_duration_ms"] = ch.smoke_duration_ms;
             item["settle_delay_ms"] = ch.settle_delay_ms;
             item["shoot_duration_ms"] = ch.shoot_duration_ms;
             item["smoke_lockout_ms"] = ch.smoke_lockout_ms;

            if (ch.type >= 4 && ch.type <= 8) { // v3 motion range: Single Color(4), Analog RGB(5), Motor(6), Stepper(7), Servo(8)
                item["pin2"] = ch.pin2;
                item["pin3"] = ch.pin3;
                item["pin4"] = ch.pin4;
                if (ch.type == 7) {  // Stepper (v3)
                    item["pin4_source"] = ch.pin4_source;
                    item["pin4_addr"] = ch.pin4_addr;
                    item["pin4_channel"] = ch.pin4_channel;
                    item["pin2_source"] = ch.pin2_source;
                    item["pin2_addr"] = ch.pin2_addr;
                    item["pin2_channel"] = ch.pin2_channel;
                    item["pin3_source"] = ch.pin3_source;
                    item["pin3_addr"] = ch.pin3_addr;
                    item["pin3_channel"] = ch.pin3_channel;
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
            } else if (ch.type == 17) {  // Solenoid (v3)
                item["solenoid_mode"] = ch.solenoid_mode;
                item["solenoid_threshold"] = ch.solenoid_threshold;
                item["solenoid_pulse_ms"] = ch.solenoid_pulse_ms;
                item["solenoid_pre_delay"] = ch.solenoid_pre_delay;
                item["solenoid_post_delay"] = ch.solenoid_post_delay;
            } else if (ch.type == 9) {  // Buzzer (v3) — save mc_freq for tone restore
                item["mc_freq"] = ch.mc_freq;
            } else if (ch.type == 11 || ch.type == 12 || ch.type == 13) {  // 7-Segment (all variants)
                item["pin2"] = ch.pin2;
                item["pin2_source"] = ch.pin2_source;
                item["pin2_addr"] = ch.pin2_addr;
                item["pin2_channel"] = ch.pin2_channel;
                item["mc_mode"] = ch.mc_mode;
            } else if (ch.type == 10) {  // DFPlayer (v3)
                item["pin2"] = ch.pin2;
            } else if (ch.type == 15 || ch.type == 16) {  // PWM DAC / FuncGen
                item["mc_freq"] = ch.mc_freq;
                item["mc_resolution"] = ch.mc_resolution;
            }
        }

        serializeJson(doc, file);
        file.close();
    }
    void clearChannels() {
        for (auto& ch : channels) {
            if (ch.pixelStrip != nullptr) {
                delete ch.pixelStrip;
                ch.pixelStrip = nullptr;
            }
            if (ch.type == 1 && ch.dmxPort != 255) {
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
        }
        channels.clear();
    }

    void setupChannels() {
        uint8_t rmtIdx = 0;
        uint8_t dmxIdx = 0;
        pcaManager.clear();
        digitalExpanderManager.clear();

        // Track UART allocations
        bool uart2Used = false;
        bool uart1Used = false;

        // DFPlayer gets priority because DMX can fall back to RMT
        for (const auto& ch : channels) {
            if (ch.type == 10) { // DFPlayer (v3)
                if (!uart2Used) {
                    uart2Used = true;
                } else if (!uart1Used) {
                    uart1Used = true;
                }
            }
        }

        uint8_t dfPlayerCount = 0;

        for (auto& ch : channels) {
            if (ch.source == 1) { // PCA9685 Expander
                uint16_t freq = getPcaSharedFrequency(ch.pca_addr);
                pcaManager.getOrCreateDriver(ch.pca_addr);
                pcaManager.setFrequency(ch.pca_addr, freq);
                Serial.printf("PCA9685 initialized at 0x%02X: shared freq = %d Hz, type = %d\n",
                              ch.pca_addr, freq, ch.type);
                continue;
            } else if (ch.source >= 2 && ch.source <= 4) {
                writeOutputPin(ch, 1, false);
                if ((ch.type == 18 || ch.type == 6 || ch.type == 7) && ch.pca_channel2 != 255) { // v3: Smoke(18), Motor(6), Stepper(7)
                    writeOutputPin(ch, 2, false);
                }
                if ((ch.type == 6 || ch.type == 7) && ch.pca_channel3 != 255) { // Motor(6), Stepper(7)
                    writeOutputPin(ch, 3, false);
                }
                Serial.printf("Digital expander initialized: source %d, addr 0x%02X, type %d\n",
                              ch.source, ch.pca_addr, ch.type);
                continue;
            }
            if (ch.type == 3) { // RGB LED strip (v3)
                if (ch.pixelStrip != nullptr) {
                    delete ch.pixelStrip;
                    ch.pixelStrip = nullptr;
                }
                if (rmtIdx < 8) {
                    // Detect RGBW vs RGB based on color_order (0-3=RGB, 4-7=RGBW)
                    if (ch.color_order >= 4) {
                        ch.pixelStrip = createPixelStripRgbw(rmtIdx, ch.led_count, ch.pin, ch.led_protocol);
                    } else {
                        ch.pixelStrip = createPixelStrip(rmtIdx, ch.led_count, ch.pin, ch.led_protocol);
                    }
                    if (ch.pixelStrip != nullptr) {
                        ch.pixelStrip->Begin();
                        ch.pixelStrip->Show();
                        const char* stripType = (ch.color_order >= 4) ? "RGBW" : "RGB";
                        Serial.printf("LED Channel initialized: GPIO %d, Count %d, Start Universe %d, RMT %d, Type %s\n", 
                                      ch.pin, ch.led_count, ch.start_universe, rmtIdx, stripType);
                    }
                    rmtIdx++;
                } else {
                    Serial.println("Max 8 LED channels reached. Cannot initialize more.");
                }
            } else if (ch.type == 2) { // CHAN_TYPE_RELAY
                pinMode(ch.pin, OUTPUT);
                digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
                Serial.printf("Relay Channel initialized: GPIO %d, Start Universe %d\n", ch.pin, ch.start_universe);
            } else if (ch.type == 17) { // CHAN_TYPE_SOLENOID (v3)
                pinMode(ch.pin, OUTPUT);
                digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
                // Set defaults if not configured
                if (ch.solenoid_pulse_ms == 0) ch.solenoid_pulse_ms = 50;  // Default 50ms pulse
                if (ch.solenoid_threshold == 0) ch.solenoid_threshold = 127; // Default 50% threshold
                Serial.printf("Solenoid Channel initialized: GPIO %d, Start Universe %d, Pulse %dms, Mode %s\n", 
                              ch.pin, ch.start_universe, ch.solenoid_pulse_ms, 
                              (ch.solenoid_mode == 0) ? "Threshold" : "FrameSync");
            } else if (ch.type == 1) { // CHAN_TYPE_DMX
                if (ch.dmxPort != 255) {
                    dmx_driver_delete((dmx_port_t)ch.dmxPort);
                    ch.dmxPort = 255;
                }
                if (ch.rmtDmx != nullptr) {
                    delete ch.rmtDmx;
                    ch.rmtDmx = nullptr;
                }
                
                if (!uart2Used) {
                    ch.dmxPort = (uint8_t)DMX_NUM_2;
                    uart2Used = true;
                    dmx_config_t dmxConfig = DMX_CONFIG_DEFAULT;
                    dmx_driver_install(DMX_NUM_2, &dmxConfig, DMX_INTR_FLAGS_DEFAULT);
                    dmx_set_pin(DMX_NUM_2, ch.pin, DMX_PIN_NO_CHANGE, DMX_PIN_NO_CHANGE);
                    Serial.printf("DMX Channel initialized: GPIO %d, Start Universe %d on UART2\n", 
                                  ch.pin, ch.start_universe);
                } else if (!uart1Used) {
                    ch.dmxPort = (uint8_t)DMX_NUM_1;
                    uart1Used = true;
                    dmx_config_t dmxConfig = DMX_CONFIG_DEFAULT;
                    dmx_driver_install(DMX_NUM_1, &dmxConfig, DMX_INTR_FLAGS_DEFAULT);
                    dmx_set_pin(DMX_NUM_1, ch.pin, DMX_PIN_NO_CHANGE, DMX_PIN_NO_CHANGE);
                    Serial.printf("DMX Channel initialized: GPIO %d, Start Universe %d on UART1\n", 
                                  ch.pin, ch.start_universe);
                } else if (rmtIdx < 8) {
                    // Fallback to RMT for extra DMX channels
                    ch.rmtDmx = new RmtDmxDriver(rmtIdx, ch.pin);
                    ch.rmtDmx->begin();
                    Serial.printf("DMX Channel (Extra) initialized: GPIO %d, Start Universe %d on RMT %d\n", 
                                  ch.pin, ch.start_universe, rmtIdx);
                    rmtIdx++;
                } else {
                    Serial.println("Max 8 RMTs reached. Cannot initialize more DMX channels.");
                }
            } else if (ch.type == 18) { // Sequential Smoke Shooter (v3)
                if (ch.source == 0) { // ESP32 GPIO
                    pinMode(ch.pin, OUTPUT);
                    pinMode(ch.pin2, OUTPUT);
                    digitalWrite(ch.pin, ch.pin_invert ? HIGH : LOW);
                    digitalWrite(ch.pin2, ch.pin2_invert ? HIGH : LOW);
                }
                ch.smoke_state = 0;
                ch.smoke_prev_trigger = false;
                Serial.printf("Smoke Shooter initialized: Smoke Pin %d, Shooter Pin %d\n", ch.pin, ch.pin2);
            } else if (ch.type == 10) { // DFPlayer MP3 (v3)
                ch.dfPlayer = new DFPlayerController();
                if (dfPlayerCount == 0) {
                    ch.dfPlayer->begin(Serial2, ch.pin, ch.pin2);
                    Serial.printf("DFPlayer MP3 initialized on Serial2: TX Pin %d, RX Pin %d\n", ch.pin, ch.pin2);
                    dfPlayerCount++;
                } else if (dfPlayerCount == 1) {
                    ch.dfPlayer->begin(Serial1, ch.pin, ch.pin2);
                    Serial.printf("DFPlayer MP3 initialized on Serial1: TX Pin %d, RX Pin %d\n", ch.pin, ch.pin2);
                    dfPlayerCount++;
                } else {
                    Serial.println("Error: No available UART port for DFPlayer MP3!");
                }
            }
        }
    }

    void loop() {
        // Trigger DMX Serial Frame send periodically based on target FPS
        static unsigned long lastDmxSend = 0;
        unsigned long now = millis();
        uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
        unsigned long interval = 1000 / fps;
        if (now - lastDmxSend >= interval) {
            lastDmxSend = now; // Update before send to avoid drift accumulation
            sendDmxFrames();
            updateRelays();
            updateSolenoids(); // Frame-rate update for solenoid pulse control
            updateSmokeShooters(); // Frame-rate update for smoke shooter sequence
        }
    }

    void updateSmokeShooters() {
        unsigned long now = millis();
        for (auto& ch : channels) {
            if (ch.type != 18) continue; // Sequential Smoke Shooter only (v3)
            if (ch.dmxBuffer == nullptr) continue;
            
            uint8_t dmx_val = ch.dmxBuffer[0];
            bool trigger_active = (dmx_val > 127);
            
            if (ch.smoke_state == 0) { // IDLE
                writeOutputPin(ch, 1, false);
                writeOutputPin(ch, 2, false);
                
                if (trigger_active && !ch.smoke_prev_trigger) {
                    ch.smoke_state = 1; // SMOKE_ON
                    ch.smoke_timer = now;
                    writeOutputPin(ch, 1, true);
                    Serial.println("Smoke Shooter: SMOKE state active");
                }
            }
            else if (ch.smoke_state == 1) { // SMOKE_ON
                writeOutputPin(ch, 1, true);
                writeOutputPin(ch, 2, false);
                if (now - ch.smoke_timer >= ch.smoke_duration_ms) {
                    ch.smoke_state = 2; // SETTLE
                    ch.smoke_timer = now;
                    writeOutputPin(ch, 1, false);
                    Serial.println("Smoke Shooter: SETTLE state active");
                }
            }
            else if (ch.smoke_state == 2) { // SETTLE
                writeOutputPin(ch, 1, false);
                writeOutputPin(ch, 2, false);
                if (now - ch.smoke_timer >= ch.settle_delay_ms) {
                    ch.smoke_state = 3; // SHOOT_ON
                    ch.smoke_timer = now;
                    writeOutputPin(ch, 2, true);
                    Serial.println("Smoke Shooter: SHOOT state active");
                }
            }
            else if (ch.smoke_state == 3) { // SHOOT_ON
                writeOutputPin(ch, 1, false);
                writeOutputPin(ch, 2, true);
                if (now - ch.smoke_timer >= ch.shoot_duration_ms) {
                    ch.smoke_state = 4; // COOLDOWN
                    ch.smoke_timer = now;
                    writeOutputPin(ch, 2, false);
                    Serial.println("Smoke Shooter: COOLDOWN state active");
                }
            }
            else if (ch.smoke_state == 4) { // COOLDOWN
                writeOutputPin(ch, 1, false);
                writeOutputPin(ch, 2, false);
                if (now - ch.smoke_timer >= ch.smoke_lockout_ms) {
                    ch.smoke_state = 0; // Back to IDLE
                    Serial.println("Smoke Shooter: returned to IDLE");
                }
            }
            ch.smoke_prev_trigger = trigger_active;
        }
    }

    void updateRelays() {
        for (auto& ch : channels) {
            if (ch.type == 2 && ch.dmxBuffer != nullptr) {
                // Relay threshold at 50% (127)
                bool state = ch.dmxBuffer[0] > 127;
                writeOutputPin(ch, 1, state);
            }
        }
    }

    void updateSolenoids() {
        unsigned long now = millis();
        for (auto& ch : channels) {
            if (ch.type != 17) continue; // Solenoid only (v3)
            if (ch.dmxBuffer == nullptr) continue;
            
            uint8_t dmx_value = ch.dmxBuffer[0];
            
            // Check if trigger condition is met
            bool should_trigger = false;
            if (ch.solenoid_mode == 0) {
                // Threshold mode: trigger when DMX > threshold
                should_trigger = dmx_value > ch.solenoid_threshold;
            } else if (ch.solenoid_mode == 1) {
                // Frame-sync mode: trigger every frame if DMX > 127
                should_trigger = dmx_value > 127;
            }
            
            // Check cooldown period (post_delay)
            if (now - ch.solenoid_last_trigger < ch.solenoid_post_delay) {
                should_trigger = false;
            }
            
            // Trigger new pulse if condition met and not already pulsing
            if (should_trigger && !ch.solenoid_pulse_active) {
                // Wait pre_delay before starting pulse
                if (now >= ch.solenoid_last_trigger + ch.solenoid_post_delay + ch.solenoid_pre_delay) {
                    ch.solenoid_pulse_active = true;
                    ch.solenoid_pulse_start = now;
                    writeOutputPin(ch, 1, true);
                    ch.solenoid_last_trigger = now;
                }
            }
            
            // End pulse if duration elapsed
            if (ch.solenoid_pulse_active && (now - ch.solenoid_pulse_start >= ch.solenoid_pulse_ms)) {
                writeOutputPin(ch, 1, false);
                ch.solenoid_pulse_active = false;
            }
        }
    }

    // Refresh LED strip output from the local DMX buffers
    void updateLeds() {
        static unsigned long lastUpdate = 0;
        unsigned long now = millis();
        uint8_t fps = sysCfg.output_fps > 0 ? sysCfg.output_fps : 40;
        if (now - lastUpdate < (1000 / fps)) return;
        lastUpdate = now;

        for (auto& ch : channels) {
            if (ch.type != 3) continue; // LED only (v3)
            if (ch.pixelStrip == nullptr || ch.dmxBuffer == nullptr) continue;

            const uint8_t colorOrder = ch.color_order;
            const uint16_t ledCount = ch.led_count;
            const uint16_t bufSize = ch.bufferSize;
            const uint8_t* buf = ch.dmxBuffer;
            const bool isRgbw = ch.pixelStrip->IsRgbw();
            const uint8_t bytesPerPixel = isRgbw ? 4 : 3;
            const uint16_t pixelsPerUniverse = 512 / bytesPerPixel; // 128 for RGBW, 170 for RGB

            // Precompute universe boundary to avoid per-pixel division
            uint16_t universe = 0;
            uint16_t posInUniverse = 0; // pixel index within current universe
            uint16_t bufOffset = 0;     // byte index into dmxBuffer

            for (uint16_t i = 0; i < ledCount; i++) {
                // Advance universe boundary when pixel block is full
                if (posInUniverse == pixelsPerUniverse) {
                    universe++;
                    posInUniverse = 0;
                    bufOffset = universe * 512;
                }

                uint16_t idx = bufOffset + posInUniverse * bytesPerPixel;
                posInUniverse++;

                if (idx + bytesPerPixel > bufSize) continue;

                uint8_t r = buf[idx];
                uint8_t g = buf[idx + 1];
                uint8_t b = buf[idx + 2];
                uint8_t w = isRgbw ? buf[idx + 3] : 0;

                // Apply software color reordering and set pixel
                if (isRgbw) {
                    RgbwColor color;
                    switch (colorOrder) {
                        case COLOR_RGBW: color = RgbwColor(r, g, b, w); break;
                        case COLOR_GRBW: color = RgbwColor(g, r, b, w); break;
                        case COLOR_BRGW: color = RgbwColor(b, r, g, w); break;
                        case COLOR_WRGB: color = RgbwColor(w, r, g, b); break;
                        default:         color = RgbwColor(r, g, b, w); break;
                    }
                    ch.pixelStrip->SetPixelColorRgbw(i, color);
                } else {
                    RgbColor color;
                    switch (colorOrder) {
                        case COLOR_GRB: color = RgbColor(g, r, b); break;
                        case COLOR_BRG: color = RgbColor(b, r, g); break;
                        case COLOR_RBG: color = RgbColor(r, b, g); break;
                        case COLOR_RGB:
                        default:        color = RgbColor(r, g, b); break;
                    }
                    ch.pixelStrip->SetPixelColor(i, color);
                }
            }
            ch.pixelStrip->Show();
        }

        // Let MotionControl read the same buffers
        extern void updateMotionControl();
        updateMotionControl();
    }

    std::vector<OutputChannel>& getChannels() {
        return channels;
    }

private:
    void sendDmxFrames() {
        for (auto& ch : channels) {
            if (ch.type == 1 && ch.dmxBuffer != nullptr) {
                if (ch.dmxPort != 255) {
                    dmx_port_t port = (dmx_port_t)ch.dmxPort;
                    dmxTxBuffer[0] = 0x00;
                    memcpy(dmxTxBuffer + 1, ch.dmxBuffer, 512);

                    // Write and send via hardware serial
                    dmx_write(port, dmxTxBuffer, DMX_PACKET_SIZE);
                    dmx_send(port, DMX_PACKET_SIZE);
                    
                    // Wait for send to complete
                    dmx_wait_sent(port, DMX_TIMEOUT_TICK);
                } else if (ch.rmtDmx != nullptr) {
                    ch.rmtDmx->send(ch.dmxBuffer);
                }
            }
        }
    }
};

extern OutputControl outputCtrl;

#endif // OUTPUT_CONTROL_H
