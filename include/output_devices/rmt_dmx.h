#ifndef OUTPUT_DEVICES_RMT_DMX_H
#define OUTPUT_DEVICES_RMT_DMX_H

#include <Arduino.h>
#include <driver/rmt.h>
#include <stdlib.h>

#define DMX_BREAK_US 100
#define DMX_MAB_US 12
#define DMX_BIT_US 4

class RmtDmxDriver {
private:
    rmt_channel_t channel;
    uint8_t pin;
    rmt_item32_t* rmt_buffer;
    bool started;

public:
    RmtDmxDriver(uint8_t rmt_channel, uint8_t gpio_pin) {
        channel = (rmt_channel_t)rmt_channel;
        pin = gpio_pin;
        rmt_buffer = (rmt_item32_t*)malloc(sizeof(rmt_item32_t) * 5150);
        started = false;
    }

    ~RmtDmxDriver() {
        if (rmt_buffer) free(rmt_buffer);
        if (started) rmt_driver_uninstall(channel);
    }

    void begin() {
        if (rmt_buffer == nullptr) {
            Serial.printf("RmtDmx: OOM allocating RMT buffer ch=%d\n", channel);
            return;
        }
        rmt_config_t config;
        config.rmt_mode = RMT_MODE_TX;
        config.channel = channel;
        config.gpio_num = (gpio_num_t)pin;
        config.clk_div = 80;
        config.tx_config.loop_en = false;
        config.tx_config.carrier_en = false;
        config.tx_config.idle_output_en = true;
        config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;

        esp_err_t err;
        err = rmt_config(&config);
        if (err != ESP_OK) {
            Serial.printf("RmtDmx: rmt_config failed ch=%d err=%d\n", channel, err);
            return;
        }
        err = rmt_driver_install(channel, 0, 0);
        if (err != ESP_OK) {
            Serial.printf("RmtDmx: rmt_driver_install failed ch=%d err=%d\n", channel, err);
            return;
        }
        started = true;
    }

    bool ready() const { return rmt_buffer != nullptr && started; }

    void send(uint8_t* dmx_data, uint16_t data_len = 512) {
        if (!ready()) return;
        if (rmt_wait_tx_done(channel, 0) != ESP_OK) return;
        if (data_len > 512) data_len = 512;
        size_t item_count = 0;

        // 1. Break and MAB
        rmt_buffer[item_count++] = {{{DMX_BREAK_US, 0, DMX_MAB_US, 1}}};

        // 2. Data slots: start code + channel bytes.
        uint8_t byte = 0x00; // Start code
        encodeByte(byte, item_count);

        for (uint16_t i = 0; i < data_len; i++) {
            encodeByte(dmx_data[i], item_count);
        }
        
        // End marker
        rmt_buffer[item_count++] = {{{0, 0, 0, 0}}};
        
        // Write to RMT
        rmt_write_items(channel, rmt_buffer, item_count, false);
    }

private:
    void encodeByte(uint8_t byte, size_t &item_count) {
        uint32_t level0_duration = DMX_BIT_US; // Start bit
        uint32_t level1_duration = 0;
        bool current_level = 0;
        uint32_t current_duration = DMX_BIT_US;
        
        for (int b = 0; b < 8; b++) {
            bool bit = (byte >> b) & 0x01;
            if (bit == current_level) {
                current_duration += DMX_BIT_US;
            } else {
                if (current_level == 0) {
                    level0_duration = current_duration;
                    current_level = 1;
                    current_duration = DMX_BIT_US;
                } else {
                    level1_duration = current_duration;
                    rmt_buffer[item_count++] = {{{level0_duration, 0, level1_duration, 1}}};
                    current_level = 0;
                    current_duration = DMX_BIT_US;
                    level0_duration = 0;
                    level1_duration = 0;
                }
            }
        }
        
        // Stop bits (HIGH)
        if (current_level == 1) {
            current_duration += (DMX_BIT_US * 2);
            level1_duration = current_duration;
            rmt_buffer[item_count++] = {{{level0_duration, 0, level1_duration, 1}}};
        } else {
            level0_duration = current_duration;
            level1_duration = DMX_BIT_US * 2;
            rmt_buffer[item_count++] = {{{level0_duration, 0, level1_duration, 1}}};
        }
    }
};

#endif // OUTPUT_DEVICES_RMT_DMX_H
