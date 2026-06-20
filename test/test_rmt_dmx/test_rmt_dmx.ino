#include <Arduino.h>
#include <driver/rmt.h>

#define DMX_TX_PIN 18
#define RMT_TX_CHANNEL RMT_CHANNEL_3
#define DMX_UNIVERSE_SIZE 512

// RMT clock divider to get 1 tick = 1 microsecond (80MHz / 80)
#define RMT_CLK_DIV 80

// DMX timings in microseconds
#define DMX_BREAK_US 100
#define DMX_MAB_US 12
#define DMX_BIT_US 4

// RMT item buffer (max size without merging: 1 start + 8 data + 1 stop item per byte)
// Each byte takes max 10 items. Start code (1) + 512 bytes = 513 * 10 = 5130 items
static rmt_item32_t rmt_buffer[5150];
static uint8_t dmx_data[513];

void rmt_dmx_init() {
    rmt_config_t config;
    config.rmt_mode = RMT_MODE_TX;
    config.channel = RMT_TX_CHANNEL;
    config.gpio_num = (gpio_num_t)DMX_TX_PIN;
    config.clk_div = RMT_CLK_DIV;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;

    rmt_config(&config);
    rmt_driver_install(config.channel, 0, 0);
    
    // Clear initial data
    memset(dmx_data, 0, sizeof(dmx_data));
}

void rmt_dmx_send() {
    size_t item_count = 0;

    // 1. Break (LOW) and MAB (HIGH)
    rmt_buffer[item_count++] = {{{DMX_BREAK_US, 0, DMX_MAB_US, 1}}};

    // 2. Data slots (Start code + 512 channels)
    for (int i = 0; i < 513; i++) {
        uint8_t byte = dmx_data[i];
        
        // Start bit (LOW)
        uint32_t level0_duration = DMX_BIT_US;
        uint32_t level0_val = 0;
        
        uint32_t level1_duration = 0;
        uint32_t level1_val = 1;
        
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
        
        // Stop bits (HIGH for 8us)
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
    
    // End marker (empty item)
    rmt_buffer[item_count++] = {{{0, 0, 0, 0}}};
    
    // Send via RMT
    rmt_write_items(RMT_TX_CHANNEL, rmt_buffer, item_count, true);
}

void setup() {
    Serial.begin(115200);
    rmt_dmx_init();
    Serial.println("RMT DMX initialized");
}

void loop() {
    // Chaser effect
    static int ch = 1;
    memset(dmx_data + 1, 0, 512);
    dmx_data[ch] = 255;
    
    rmt_dmx_send();
    
    ch++;
    if (ch > 512) ch = 1;
    
    delay(25); // ~40 FPS
}
