#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t i2cMutex;
extern SemaphoreHandle_t swapMutex;

// Device Mode Enum
enum DeviceMode {
    MODE_ARTNET_ETHERNET = 0,
    MODE_ESPNOW_MASTER   = 1,
    MODE_ESPNOW_SLAVE    = 2
};

// LED Color Order Map (RGB strips: 0-3, RGBW strips: 4-7)
enum LedColorOrder {
    // RGB Strips (3 channels per pixel)
    COLOR_GRB = 0,
    COLOR_RGB = 1,
    COLOR_BRG = 2,
    COLOR_RBG = 3,
    
    // RGBW Strips (4 channels per pixel - W is separate white LED)
    COLOR_RGBW = 4,    // Red, Green, Blue, White
    COLOR_GRBW = 5,    // Green, Red, Blue, White
    COLOR_BRGW = 6,    // Blue, Red, Green, White
    COLOR_WRGB = 7     // White, Red, Green, Blue (uncommon but supported)
};

// Main Configuration Struct
struct SystemConfig {
    // Mode
    uint8_t device_mode;       // DeviceMode

    // Ethernet Settings
    bool eth_dhcp;
    char eth_ip[16];
    char eth_netmask[16];
    char eth_gateway[16];
    char eth_dns[16];

    // Wi-Fi SoftAP Settings
    char ap_ssid[32];
    char ap_pass[64];

    // ESP-NOW Settings
    uint8_t espnow_channel;    // 0 = Auto (Slave only), 1-13 = Fixed channel
    uint16_t espnow_chunk_size; // 16 to 230 bytes payload per packet

    // Art-Net Settings
    bool artnet_enabled;       // Enable Art-Net listener
    uint16_t artnet_port;

    // sACN/E1.31 Protocol Settings
    bool sacn_enabled;         // Enable sACN listener (can run alongside Art-Net)
    bool sacn_multicast;       // true: multicast 239.69.X.Y, false: unicast
    uint16_t sacn_port;

    // Pin mappings
    uint8_t status_led_pin;
    uint8_t zc_pin;
    uint8_t i2c_sda;
    uint8_t i2c_scl;
    uint32_t i2c_speed;

    // Outputs Settings
    uint8_t output_fps;

    // Wi-Fi Client Connection Settings
    char wifi_ssid[32];
    char wifi_pass[64];
    bool wifi_dhcp;
    char wifi_ip[16];
    char wifi_netmask[16];
    char wifi_gateway[16];
    char wifi_dns[16];
    bool wifi_enable_in_eth_mode;
    bool ap_enable_in_eth_mode;

    // mDNS hostname (e.g. "artnet" -> http://artnet.local)
    char mdns_name[32];

    // I2C Display Settings
    uint8_t display_enabled;    // 0=off, 1=SSD1306 OLED, 2=SH1106 OLED, 3=PCF8574 LCD
    uint8_t display_i2c_addr;   // I2C address (default 0x3C for SSD1306)
    uint8_t display_brightness; // 0-255
    uint16_t display_refresh_ms; // display update interval (default 500ms)
    uint16_t display_recover_ms; // I2C display recovery probe interval (default 5000ms)
    uint8_t display_cols;        // LCD character columns (default 20)
    uint8_t display_rows;        // LCD character rows (default 4)

    // Web Server
    uint16_t web_port;           // HTTP server port (default 80)

    // ESP-NOW slave queue depth
    uint8_t espnow_queue_depth;  // FreeRTOS queue depth (default 16)

    // Wi-Fi reconnect interval
    uint16_t wifi_reconnect_interval; // ms between reconnect attempts (default 10000)

    // Default output channel (used when /outputs.json does not exist)
    uint8_t default_output_type;
    uint8_t default_output_pin;
    uint16_t default_led_count;

    // Art-Net identity
    char artnet_short_name[32];
    char artnet_long_name[64];
};

// Global config instance
extern SystemConfig sysCfg;

// Setup pins default definitions
#define DEFAULT_LED_DATA_PIN     4  // Default pixel data pin (User pins: 4, 12, 14, 15)
#define DEFAULT_STATUS_LED_PIN   5  // UART2 RX (LED3 on WT32-ETH01 used as status indicator hack)

// LAN8720 Ethernet Pin Settings for WT32-ETH01
#define ETH_PHY_ADDR             1
#define ETH_PHY_POWER            16
#define ETH_PHY_MDC              23
#define ETH_PHY_MDIO             18
#define ETH_PHY_TYPE             ETH_PHY_LAN8720
#define ETH_CLK_MODE             ETH_CLOCK_GPIO0_IN

// Configuration Helper Functions
void loadConfig(SystemConfig& cfg);
bool saveConfig(const SystemConfig& cfg);

#endif // CONFIG_H
