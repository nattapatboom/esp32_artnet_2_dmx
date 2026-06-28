#include "config.h"

void loadConfig(SystemConfig& cfg) {
    Preferences prefs;
    prefs.begin("system", true);

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
    if (cfg.espnow_channel > 13) cfg.espnow_channel = 0;
    cfg.espnow_chunk_size = prefs.getUShort("now_chunk", 200);
    if (cfg.espnow_chunk_size < 16 || cfg.espnow_chunk_size > 230) cfg.espnow_chunk_size = 200;
    cfg.artnet_enabled = prefs.getBool("art_on", true);
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
    cfg.display_refresh_ms = prefs.getUShort("disp_ref", 500);
    cfg.display_recover_ms = prefs.getUShort("disp_rcv", 5000);
    cfg.display_cols = prefs.getUChar("disp_col", 20);
    cfg.display_rows = prefs.getUChar("disp_row", 4);

    cfg.web_port = prefs.getUShort("web_port", 80);
    if (cfg.web_port == 0) cfg.web_port = 80;

    cfg.espnow_queue_depth = prefs.getUChar("now_qdep", 16);
    if (cfg.espnow_queue_depth == 0) cfg.espnow_queue_depth = 16;

    cfg.wifi_reconnect_interval = prefs.getUShort("wf_recnn", 10000);
    if (cfg.wifi_reconnect_interval < 1000) cfg.wifi_reconnect_interval = 1000;

    cfg.default_output_type = prefs.getUChar("def_type", 3);
    cfg.default_output_pin = prefs.getUChar("def_pin", DEFAULT_LED_DATA_PIN);
    cfg.default_led_count = prefs.getUShort("def_led", 170);
    if (cfg.default_led_count == 0) cfg.default_led_count = 170;

    String shortName = prefs.getString("art_sname", "CHAL Node-");
    strncpy(cfg.artnet_short_name, shortName.c_str(), sizeof(cfg.artnet_short_name) - 1);
    cfg.artnet_short_name[sizeof(cfg.artnet_short_name) - 1] = '\0';

    String longName = prefs.getString("art_lname", "CHAL WT32-ETH01 Converter - ");
    strncpy(cfg.artnet_long_name, longName.c_str(), sizeof(cfg.artnet_long_name) - 1);
    cfg.artnet_long_name[sizeof(cfg.artnet_long_name) - 1] = '\0';

    prefs.end();
}

bool saveConfig(const SystemConfig& cfg) {
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
    prefs.putUShort("disp_ref", cfg.display_refresh_ms);
    prefs.putUShort("disp_rcv", cfg.display_recover_ms);
    prefs.putUChar("disp_col", cfg.display_cols);
    prefs.putUChar("disp_row", cfg.display_rows);
    prefs.putUShort("web_port", cfg.web_port);
    prefs.putUChar("now_qdep", cfg.espnow_queue_depth);
    prefs.putUShort("wf_recnn", cfg.wifi_reconnect_interval);
    prefs.putUChar("def_type", cfg.default_output_type);
    prefs.putUChar("def_pin", cfg.default_output_pin);
    prefs.putUShort("def_led", cfg.default_led_count);
    prefs.putString("art_sname", String(cfg.artnet_short_name));
    prefs.putString("art_lname", String(cfg.artnet_long_name));

    prefs.end();
    return true;
}
