#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H

#include <Arduino.h>
#include <atomic>
#include <new>
#include <esp_dmx.h>
#include <NeoPixelBus.h>
#include <vector>
#include <algorithm>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "output_common.h"
#include "config.h"
#include "output_devices/dfplayer_control.h"
#include "i2c_devices/pwm_expander.h"
#include "i2c_devices/i2c_gpio_expander.h"
#include "output_devices/funcgen_control.h"
#include "output_defs.h"
#include "output_devices/rmt_dmx.h"

extern PwmExpanderManager pwmExpanderManager;
extern DigitalExpanderManager digitalExpanderManager;

class OutputControl;
extern OutputControl outputCtrl;

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

struct PinRoute {
    uint8_t pin = 255;
    uint8_t source = 0;
    uint8_t addr = 32;
    uint8_t channel = 255;
    bool invert = false;
};

struct OutputChannel {
    OutputChannel();

    uint8_t type = 0;
    PinRoute routes[9];

    uint16_t start_universe = 0;
    uint16_t start_address = 1;
    uint16_t led_count = 0;
    uint8_t color_order = 0;
    uint8_t led_protocol = 0;
    uint8_t pwm_dac_mode = 0;
    uint16_t pwm_dac_min = 0;
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

    uint8_t mc_resolution = 8;
    uint16_t mc_freq = 1000;
    uint8_t mc_mode = 0;
    uint8_t mc_deadband = 10;
    bool mc_invert = false;
    bool mc_brake = true;
    uint16_t mc_min_us = 1000;
    uint16_t mc_max_us = 2000;
    uint16_t mc_steps_per_rev = 200;
    uint8_t mc_homing_dir = 0;
    uint8_t mc_homing_mode = 0;
    uint16_t mc_homing_speed = 500;
    uint16_t mc_homing_timeout = 5;
    float mc_scale_factor = 0.0f;
    uint8_t mc_unit_type = 0;

    uint8_t stepper_cmd_state = 0;
    unsigned long stepper_cmd_start_time = 0;
    unsigned long stepper_homing_start_time = 0;

    uint8_t ledc_chan2 = 255;
    uint8_t ledc_chan3 = 255;
    uint8_t ledc_chan4 = 255;

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

void loadChannelPins(OutputChannel& ch, JsonArrayConst pinsArr);
void saveChannelPins(const OutputChannel& ch, JsonArray pinsArr);
uint16_t outputDmxByteCount(const OutputChannel& ch);

class OutputControl {
private:
    std::vector<OutputChannel> channels;

    uint16_t getPcaSharedFrequency(uint8_t address) const;

public:
    uint16_t sharedPcaFrequency(uint8_t address) const;
    uint16_t getUniverseCount(const OutputChannel& ch) const;
    bool mapDmxDataToChannels(uint16_t universe, const uint8_t* data, uint16_t length, bool updateActiveBuffer = true, uint16_t dataOffset = 0);
    void swapBuffers();
    void setupChannels();
    void begin();
    void loadChannels();
    bool saveChannels();
    void clearChannels();
    void loop();
    void updateLeds();

    std::vector<OutputChannel>& getChannels();
};

void writeOutputPin(OutputChannel& ch, uint8_t pinNum, bool state);
bool readOutputPin(OutputChannel& ch, uint8_t pinNum);

#endif // OUTPUT_CONTROL_H
