#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <Arduino.h>
#include <stdint.h>

// ─────────────────────────────────────────────
//  Network Protocol
//  System/network config field keys, limits,
//  defaults, and validation helpers.
//  Single source of truth shared between C++
//  and Web UI (mirror of network config in
//  web/js/app.js and pane-network.html).
// ─────────────────────────────────────────────

namespace NetworkProtocol {

// ── Device mode IDs ──
constexpr uint8_t MODE_ARTNET_ETHERNET = 0;
constexpr uint8_t MODE_ESPNOW_MASTER   = 1;
constexpr uint8_t MODE_ESPNOW_SLAVE    = 2;

constexpr const char* MODE_NAMES[] = {
    "Art-Net Ethernet",
    "ESP-NOW Master",
    "ESP-NOW Slave"
};

// ── JSON field keys ( /api/settings POST/GET ) ──
// Device mode
constexpr const char* KEY_DEVICE_MODE        = "device_mode";

// Ethernet
constexpr const char* KEY_ETH_DHCP           = "eth_dhcp";
constexpr const char* KEY_ETH_IP             = "eth_ip";
constexpr const char* KEY_ETH_NETMASK        = "eth_netmask";
constexpr const char* KEY_ETH_GATEWAY        = "eth_gateway";
constexpr const char* KEY_ETH_DNS            = "eth_dns";

// Wi-Fi STA
constexpr const char* KEY_WIFI_SSID          = "wifi_ssid";
constexpr const char* KEY_WIFI_PASS          = "wifi_pass";
constexpr const char* KEY_WIFI_DHCP          = "wifi_dhcp";
constexpr const char* KEY_WIFI_IP            = "wifi_ip";
constexpr const char* KEY_WIFI_NETMASK       = "wifi_netmask";
constexpr const char* KEY_WIFI_GATEWAY       = "wifi_gateway";
constexpr const char* KEY_WIFI_DNS           = "wifi_dns";
constexpr const char* KEY_WIFI_IN_ETH_MODE   = "wifi_enable_in_eth_mode";

// Wi-Fi AP
constexpr const char* KEY_AP_SSID            = "ap_ssid";
constexpr const char* KEY_AP_PASS            = "ap_pass";
constexpr const char* KEY_AP_IN_ETH_MODE     = "ap_enable_in_eth_mode";

// mDNS
constexpr const char* KEY_MDNS_NAME          = "mdns_name";

// Art-Net
constexpr const char* KEY_ARTNET_ENABLED     = "artnet_enabled";
constexpr const char* KEY_ARTNET_PORT        = "artnet_port";

// sACN
constexpr const char* KEY_SACN_ENABLED       = "sacn_enabled";
constexpr const char* KEY_SACN_MULTICAST     = "sacn_multicast";
constexpr const char* KEY_SACN_PORT          = "sacn_port";

// ESP-NOW
constexpr const char* KEY_ESPNOW_CHANNEL     = "espnow_channel";
constexpr const char* KEY_ESPNOW_CHUNK_SIZE  = "espnow_chunk_size";

// Output
constexpr const char* KEY_OUTPUT_FPS         = "output_fps";

// I2C
constexpr const char* KEY_I2C_SDA            = "i2c_sda";
constexpr const char* KEY_I2C_SCL            = "i2c_scl";
constexpr const char* KEY_I2C_SPEED          = "i2c_speed";

// Pins
constexpr const char* KEY_STATUS_LED_PIN     = "status_led_pin";
constexpr const char* KEY_ZC_PIN             = "zc_pin";

// Display
constexpr const char* KEY_DISPLAY_ENABLED    = "display_enabled";
constexpr const char* KEY_DISPLAY_I2C_ADDR   = "display_i2c_addr";
constexpr const char* KEY_DISPLAY_BRIGHTNESS = "display_brightness";
constexpr const char* KEY_DISPLAY_REFRESH_MS = "display_refresh_ms";
constexpr const char* KEY_DISPLAY_RECOVER_MS = "display_recover_ms";
constexpr const char* KEY_DISPLAY_COLS       = "display_cols";
constexpr const char* KEY_DISPLAY_ROWS       = "display_rows";

// Web Server
constexpr const char* KEY_WEB_PORT           = "web_port";

// ESP-NOW
constexpr const char* KEY_ESPNOW_QUEUE_DEPTH = "espnow_queue_depth";

// Wi-Fi
constexpr const char* KEY_WIFI_RECONNECT_MS  = "wifi_reconnect_interval";

// Default output channel
constexpr const char* KEY_DEFAULT_OUT_TYPE   = "default_output_type";
constexpr const char* KEY_DEFAULT_OUT_PIN    = "default_output_pin";
constexpr const char* KEY_DEFAULT_LED_COUNT  = "default_led_count";

// Art-Net identity
constexpr const char* KEY_ARTNET_SHORT_NAME  = "artnet_short_name";
constexpr const char* KEY_ARTNET_LONG_NAME   = "artnet_long_name";

// ── Field limits & defaults ──
// Ethernet
constexpr bool     ETH_DHCP_DEFAULT          = true;
constexpr const char* ETH_IP_DEFAULT         = "192.168.1.200";
constexpr const char* ETH_NETMASK_DEFAULT    = "255.255.255.0";
constexpr const char* ETH_GATEWAY_DEFAULT    = "192.168.1.1";
constexpr const char* ETH_DNS_DEFAULT        = "8.8.8.8";

// Wi-Fi STA
constexpr bool     WIFI_DHCP_DEFAULT         = true;
constexpr const char* WIFI_IP_DEFAULT        = "192.168.1.201";
constexpr const char* WIFI_NETMASK_DEFAULT   = "255.255.255.0";
constexpr const char* WIFI_GATEWAY_DEFAULT   = "192.168.1.1";
constexpr const char* WIFI_DNS_DEFAULT       = "8.8.8.8";

// Wi-Fi AP
constexpr const char* AP_SSID_DEFAULT        = "ESP32-ArtNet-Setup";

// mDNS
constexpr const char* MDNS_NAME_DEFAULT       = "artnet";
constexpr uint8_t     MDNS_NAME_MAX_LEN       = 31;

// Art-Net
constexpr bool     ARTNET_ENABLED_DEFAULT    = true;
constexpr uint16_t ARTNET_PORT_DEFAULT       = 6454;
constexpr uint16_t ARTNET_PORT_MIN           = 1024;
constexpr uint16_t ARTNET_PORT_MAX           = 65535;

// sACN
constexpr bool     SACN_ENABLED_DEFAULT      = false;
constexpr bool     SACN_MULTICAST_DEFAULT     = true;
constexpr uint16_t SACN_PORT_DEFAULT         = 5568;
constexpr uint16_t SACN_PORT_MIN             = 1024;
constexpr uint16_t SACN_PORT_MAX             = 65535;

// ESP-NOW
constexpr uint8_t  ESPNOW_CHANNEL_DEFAULT    = 0;    // 0 = auto (Slave)
constexpr uint16_t ESPNOW_CHUNK_SIZE_DEFAULT = 200;
constexpr uint16_t ESPNOW_CHUNK_SIZE_MIN     = 16;
constexpr uint16_t ESPNOW_CHUNK_SIZE_MAX     = 230;

// Output
constexpr uint8_t  OUTPUT_FPS_DEFAULT        = 40;
constexpr uint8_t  OUTPUT_FPS_MIN            = 1;
constexpr uint8_t  OUTPUT_FPS_MAX            = 44;

// I2C
constexpr uint8_t  I2C_SDA_DEFAULT           = 14;
constexpr uint8_t  I2C_SCL_DEFAULT           = 15;
constexpr uint32_t I2C_SPEED_DEFAULT         = 400000;  // 400 kHz
constexpr uint32_t I2C_SPEED_MIN             = 10000;   // 10 kHz
constexpr uint32_t I2C_SPEED_MAX             = 1000000; // 1 MHz

// Status LED
constexpr uint8_t  STATUS_LED_PIN_DEFAULT    = 5;   // UART2 RX, LED3 on WT32-ETH01
constexpr uint8_t  ZC_PIN_DISABLED           = 255;

// Display
constexpr uint8_t  DISPLAY_ENABLED_DEFAULT   = 0;   // 0 = off
constexpr uint8_t  DISPLAY_I2C_ADDR_DEFAULT  = 0x3C;
constexpr uint8_t  DISPLAY_BRIGHTNESS_DEFAULT = 128;
constexpr uint8_t  DISPLAY_BRIGHTNESS_MIN    = 0;
constexpr uint8_t  DISPLAY_BRIGHTNESS_MAX    = 255;
constexpr uint16_t DISPLAY_REFRESH_MS_DEFAULT = 500;
constexpr uint16_t DISPLAY_REFRESH_MS_MIN     = 100;
constexpr uint16_t DISPLAY_REFRESH_MS_MAX     = 10000;
constexpr uint16_t DISPLAY_RECOVER_MS_DEFAULT = 5000;
constexpr uint16_t DISPLAY_RECOVER_MS_MIN     = 1000;
constexpr uint16_t DISPLAY_RECOVER_MS_MAX     = 60000;
constexpr uint8_t  DISPLAY_COLS_DEFAULT       = 20;
constexpr uint8_t  DISPLAY_COLS_MIN           = 16;
constexpr uint8_t  DISPLAY_COLS_MAX           = 40;
constexpr uint8_t  DISPLAY_ROWS_DEFAULT       = 4;
constexpr uint8_t  DISPLAY_ROWS_MIN           = 2;
constexpr uint8_t  DISPLAY_ROWS_MAX           = 4;

// Web Server
constexpr uint16_t WEB_PORT_DEFAULT           = 80;
constexpr uint16_t WEB_PORT_MIN               = 1;
constexpr uint16_t WEB_PORT_MAX               = 65535;

// ESP-NOW queue depth
constexpr uint8_t  ESPNOW_QUEUE_DEPTH_DEFAULT = 16;
constexpr uint8_t  ESPNOW_QUEUE_DEPTH_MIN     = 4;
constexpr uint8_t  ESPNOW_QUEUE_DEPTH_MAX     = 64;

// Wi-Fi reconnect interval
constexpr uint16_t WIFI_RECONNECT_MS_DEFAULT  = 10000;
constexpr uint16_t WIFI_RECONNECT_MS_MIN      = 1000;
constexpr uint16_t WIFI_RECONNECT_MS_MAX      = 60000;

// Default output channel
constexpr uint8_t  DEFAULT_OUT_TYPE_DEFAULT   = 3;    // TYPE_LED_STRIP
constexpr uint8_t  DEFAULT_OUT_PIN_DEFAULT    = 4;    // DEFAULT_LED_DATA_PIN
constexpr uint16_t DEFAULT_LED_COUNT_DEFAULT  = 170;
constexpr uint16_t DEFAULT_LED_COUNT_MIN      = 1;
constexpr uint16_t DEFAULT_LED_COUNT_MAX      = 1000;

// ── Validation helpers ──

inline bool artnetPortValid(uint16_t port) {
    return port >= ARTNET_PORT_MIN && port <= ARTNET_PORT_MAX;
}

inline bool sacnPortValid(uint16_t port) {
    return port >= SACN_PORT_MIN && port <= SACN_PORT_MAX;
}

inline bool espnowChunkSizeValid(uint16_t size) {
    return size >= ESPNOW_CHUNK_SIZE_MIN && size <= ESPNOW_CHUNK_SIZE_MAX;
}

inline bool outputFpsValid(uint8_t fps) {
    return fps >= OUTPUT_FPS_MIN && fps <= OUTPUT_FPS_MAX;
}

inline bool i2cSpeedValid(uint32_t speed) {
    return speed >= I2C_SPEED_MIN && speed <= I2C_SPEED_MAX;
}

inline bool deviceModeValid(uint8_t mode) {
    return mode == MODE_ARTNET_ETHERNET || mode == MODE_ESPNOW_MASTER || mode == MODE_ESPNOW_SLAVE;
}

inline bool ip4Valid(const char* s) {
    if (s == nullptr || s[0] == '\0') return false;
    uint8_t part = 0;
    uint16_t value = 0;
    bool hasDigit = false;
    for (const char* p = s; ; ++p) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            hasDigit = true;
            value = value * 10 + (uint16_t)(c - '0');
            if (value > 255) return false;
        } else if (c == '.' || c == '\0') {
            if (!hasDigit) return false;
            part++;
            if (c == '\0') return part == 4;
            if (part >= 4) return false;
            value = 0;
            hasDigit = false;
        } else {
            return false;
        }
    }
}

inline const char* deviceModeName(uint8_t mode) {
    if (mode <= MODE_ESPNOW_SLAVE) return MODE_NAMES[mode];
    return "Unknown";
}

// Returns true if at least one lighting protocol (Art-Net or sACN) is enabled
inline bool atLeastOneProtocol(bool artnetEnabled, bool sacnEnabled) {
    return artnetEnabled || sacnEnabled;
}

}  // namespace NetworkProtocol

#endif
