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
};

// Global config instance
extern SystemConfig sysCfg;

// Setup pins default definitions
#define DEFAULT_DMX_TX_PIN       17 // UART2 TX (LED4 on WT32-ETH01 blinks on DMX traffic)
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
inline void loadConfig(SystemConfig& cfg) {
    Preferences prefs;
    prefs.begin("system", true); // Read-only

    cfg.device_mode = prefs.getUChar("mode", MODE_ARTNET_ETHERNET);
    cfg.eth_dhcp = prefs.getBool("dhcp", true);
    
    String ip = prefs.getString("ip", "192.168.1.200");
    strncpy(cfg.eth_ip, ip.c_str(), sizeof(cfg.eth_ip) - 1);
    cfg.eth_ip[sizeof(cfg.eth_ip) - 1] = '\0';

    String mask = prefs.getString("mask", "255.255.255.0");
    strncpy(cfg.eth_netmask, mask.c_str(), sizeof(cfg.eth_netmask) - 1);
    cfg.eth_netmask[sizeof(cfg.eth_netmask) - 1] = '\0';

    String gw = prefs.getString("gw", "192.168.1.1");
    strncpy(cfg.eth_gateway, gw.c_str(), sizeof(cfg.eth_gateway) - 1);
    cfg.eth_gateway[sizeof(cfg.eth_gateway) - 1] = '\0';

    String dns = prefs.getString("dns", "8.8.8.8");
    strncpy(cfg.eth_dns, dns.c_str(), sizeof(cfg.eth_dns) - 1);
    cfg.eth_dns[sizeof(cfg.eth_dns) - 1] = '\0';

    String ssid = prefs.getString("ap_ssid", "ESP32-ArtNet-Setup");
    strncpy(cfg.ap_ssid, ssid.c_str(), sizeof(cfg.ap_ssid) - 1);
    cfg.ap_ssid[sizeof(cfg.ap_ssid) - 1] = '\0';

    String pass = prefs.getString("ap_pass", "");
    strncpy(cfg.ap_pass, pass.c_str(), sizeof(cfg.ap_pass) - 1);
    cfg.ap_pass[sizeof(cfg.ap_pass) - 1] = '\0';

    cfg.espnow_channel = prefs.getUChar("now_chan", 0);
    cfg.espnow_chunk_size = prefs.getUShort("now_chunk", 200);
    if (cfg.espnow_chunk_size < 16 || cfg.espnow_chunk_size > 230) cfg.espnow_chunk_size = 200;
    cfg.artnet_enabled = prefs.getBool("art_on", true); // Default to true
    cfg.artnet_port = prefs.getUShort("art_port", 6454);
    if (cfg.artnet_port == 0) cfg.artnet_port = 6454;
    cfg.sacn_enabled = prefs.getBool("sacn_on", false);
    cfg.sacn_multicast = prefs.getBool("sacn_mcast", true);
    cfg.sacn_port = prefs.getUShort("sacn_port", 5568);
    if (cfg.sacn_port == 0) cfg.sacn_port = 5568;
    cfg.status_led_pin = prefs.getUChar("status_pin", DEFAULT_STATUS_LED_PIN);
    cfg.zc_pin = prefs.getUChar("zc_pin", 255);
    cfg.i2c_sda = prefs.getUChar("i2c_sda", 14);
    cfg.i2c_scl = prefs.getUChar("i2c_scl", 15);
    cfg.i2c_speed = prefs.getUInt("i2c_speed", 400000);
    cfg.output_fps = prefs.getUChar("fps", 40);
    if (cfg.output_fps == 0 || cfg.output_fps > 44) cfg.output_fps = 40;

    String w_ssid = prefs.getString("wf_ssid", "");
    strncpy(cfg.wifi_ssid, w_ssid.c_str(), sizeof(cfg.wifi_ssid) - 1);
    cfg.wifi_ssid[sizeof(cfg.wifi_ssid) - 1] = '\0';

    String w_pass = prefs.getString("wf_pass", "");
    strncpy(cfg.wifi_pass, w_pass.c_str(), sizeof(cfg.wifi_pass) - 1);
    cfg.wifi_pass[sizeof(cfg.wifi_pass) - 1] = '\0';

    cfg.wifi_dhcp = prefs.getBool("wf_dhcp", true);

    String w_ip = prefs.getString("wf_ip", "192.168.1.201");
    strncpy(cfg.wifi_ip, w_ip.c_str(), sizeof(cfg.wifi_ip) - 1);
    cfg.wifi_ip[sizeof(cfg.wifi_ip) - 1] = '\0';

    String w_mask = prefs.getString("wf_mask", "255.255.255.0");
    strncpy(cfg.wifi_netmask, w_mask.c_str(), sizeof(cfg.wifi_netmask) - 1);
    cfg.wifi_netmask[sizeof(cfg.wifi_netmask) - 1] = '\0';

    String w_gw = prefs.getString("wf_gw", "192.168.1.1");
    strncpy(cfg.wifi_gateway, w_gw.c_str(), sizeof(cfg.wifi_gateway) - 1);
    cfg.wifi_gateway[sizeof(cfg.wifi_gateway) - 1] = '\0';

    String w_dns = prefs.getString("wf_dns", "8.8.8.8");
    strncpy(cfg.wifi_dns, w_dns.c_str(), sizeof(cfg.wifi_dns) - 1);
    cfg.wifi_dns[sizeof(cfg.wifi_dns) - 1] = '\0';
    cfg.wifi_enable_in_eth_mode = prefs.getBool("wf_eth_en", true);
    cfg.ap_enable_in_eth_mode = prefs.getBool("ap_eth_en", true);

    String mdns = prefs.getString("mdns", "artnet");
    strncpy(cfg.mdns_name, mdns.c_str(), sizeof(cfg.mdns_name) - 1);
    cfg.mdns_name[sizeof(cfg.mdns_name) - 1] = '\0';

    cfg.display_enabled = prefs.getUChar("disp_en", 0);
    cfg.display_i2c_addr = prefs.getUChar("disp_addr", 0x3C);
    cfg.display_brightness = prefs.getUChar("disp_brt", 128);

    prefs.end();
}

inline bool saveConfig(const SystemConfig& cfg) {
    Preferences prefs;
    if (!prefs.begin("system", false)) {
        Serial.println("[config] ERROR: Failed to open NVS 'system' namespace for writing");
        return false;
    }

    prefs.putUChar("mode", cfg.device_mode);
    prefs.putBool("dhcp", cfg.eth_dhcp);
    prefs.putString("ip", String(cfg.eth_ip));
    prefs.putString("mask", String(cfg.eth_netmask));
    prefs.putString("gw", String(cfg.eth_gateway));
    prefs.putString("dns", String(cfg.eth_dns));
    prefs.putString("ap_ssid", String(cfg.ap_ssid));
    prefs.putString("ap_pass", String(cfg.ap_pass));
    prefs.putUChar("now_chan", cfg.espnow_channel);
    prefs.putUShort("now_chunk", cfg.espnow_chunk_size);
    prefs.putBool("art_on", cfg.artnet_enabled);
    prefs.putUShort("art_port", cfg.artnet_port);
    prefs.putBool("sacn_on", cfg.sacn_enabled);
    prefs.putBool("sacn_mcast", cfg.sacn_multicast);
    prefs.putUShort("sacn_port", cfg.sacn_port);
    prefs.putUChar("status_pin", cfg.status_led_pin);
    prefs.putUChar("zc_pin", cfg.zc_pin);
    prefs.putUChar("i2c_sda", cfg.i2c_sda);
    prefs.putUChar("i2c_scl", cfg.i2c_scl);
    prefs.putUInt("i2c_speed", cfg.i2c_speed);
    prefs.putUChar("fps", cfg.output_fps);
    prefs.putString("wf_ssid", String(cfg.wifi_ssid));
    prefs.putString("wf_pass", String(cfg.wifi_pass));
    prefs.putBool("wf_dhcp", cfg.wifi_dhcp);
    prefs.putString("wf_ip", String(cfg.wifi_ip));
    prefs.putString("wf_mask", String(cfg.wifi_netmask));
    prefs.putString("wf_gw", String(cfg.wifi_gateway));
    prefs.putString("wf_dns", String(cfg.wifi_dns));
    prefs.putBool("wf_eth_en", cfg.wifi_enable_in_eth_mode);
    prefs.putBool("ap_eth_en", cfg.ap_enable_in_eth_mode);
    prefs.putString("mdns", String(cfg.mdns_name));
    prefs.putUChar("disp_en", cfg.display_enabled);
    prefs.putUChar("disp_addr", cfg.display_i2c_addr);
    prefs.putUChar("disp_brt", cfg.display_brightness);

    prefs.end();
    return true;
}

#endif // CONFIG_H
