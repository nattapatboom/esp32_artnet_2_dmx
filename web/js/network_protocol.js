// ─────────────────────────────────────────────
//  Network Protocol (JS)
//  Mirror of C++ include/network_protocol.h.
//  Single source of truth for network/system
//  config field keys, limits, defaults.
// ─────────────────────────────────────────────

const NET_PROTO = Object.freeze({

  // ── Device modes ──
  MODE_ARTNET_ETHERNET: 0,
  MODE_ESPNOW_MASTER:   1,
  MODE_ESPNOW_SLAVE:    2,

  MODE_NAMES: [
    'Art-Net Ethernet',
    'ESP-NOW Master',
    'ESP-NOW Slave'
  ],

  // ── JSON field keys ──
  // Matches NetworkProtocol::KEY_* in network_protocol.h
  KEY_DEVICE_MODE:        'device_mode',
  KEY_ETH_DHCP:           'eth_dhcp',
  KEY_ETH_IP:             'eth_ip',
  KEY_ETH_NETMASK:        'eth_netmask',
  KEY_ETH_GATEWAY:        'eth_gateway',
  KEY_ETH_DNS:            'eth_dns',
  KEY_WIFI_SSID:          'wifi_ssid',
  KEY_WIFI_PASS:          'wifi_pass',
  KEY_WIFI_DHCP:          'wifi_dhcp',
  KEY_WIFI_IP:            'wifi_ip',
  KEY_WIFI_NETMASK:       'wifi_netmask',
  KEY_WIFI_GATEWAY:       'wifi_gateway',
  KEY_WIFI_DNS:           'wifi_dns',
  KEY_WIFI_IN_ETH_MODE:   'wifi_enable_in_eth_mode',
  KEY_AP_SSID:            'ap_ssid',
  KEY_AP_PASS:            'ap_pass',
  KEY_AP_IN_ETH_MODE:     'ap_enable_in_eth_mode',
  KEY_MDNS_NAME:          'mdns_name',
  KEY_ARTNET_ENABLED:     'artnet_enabled',
  KEY_ARTNET_PORT:        'artnet_port',
  KEY_SACN_ENABLED:       'sacn_enabled',
  KEY_SACN_MULTICAST:     'sacn_multicast',
  KEY_SACN_PORT:          'sacn_port',
  KEY_ESPNOW_CHANNEL:     'espnow_channel',
  KEY_ESPNOW_CHUNK_SIZE:  'espnow_chunk_size',
  KEY_OUTPUT_FPS:         'output_fps',
  KEY_I2C_SDA:            'i2c_sda',
  KEY_I2C_SCL:            'i2c_scl',
  KEY_I2C_SPEED:          'i2c_speed',
  KEY_STATUS_LED_PIN:     'status_led_pin',
  KEY_ZC_PIN:             'zc_pin',
  KEY_DISPLAY_ENABLED:    'display_enabled',
  KEY_DISPLAY_I2C_ADDR:   'display_i2c_addr',
  KEY_DISPLAY_BRIGHTNESS: 'display_brightness',
  KEY_DISPLAY_REFRESH_MS: 'display_refresh_ms',
  KEY_DISPLAY_RECOVER_MS: 'display_recover_ms',
  KEY_DISPLAY_COLS:       'display_cols',
  KEY_DISPLAY_ROWS:       'display_rows',

  KEY_WEB_PORT:           'web_port',

  KEY_ESPNOW_QUEUE_DEPTH: 'espnow_queue_depth',

  KEY_WIFI_RECONNECT_MS:  'wifi_reconnect_interval',

  KEY_DEFAULT_OUT_TYPE:   'default_output_type',
  KEY_DEFAULT_OUT_PIN:    'default_output_pin',
  KEY_DEFAULT_LED_COUNT:  'default_led_count',

  KEY_ARTNET_SHORT_NAME:  'artnet_short_name',
  KEY_ARTNET_LONG_NAME:   'artnet_long_name',

  ZC_PIN_DISABLED:           255,
});
