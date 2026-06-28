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

  // ── Field limits & defaults ──
  ETH_DHCP_DEFAULT:          true,
  ETH_IP_DEFAULT:            '192.168.1.200',
  ETH_NETMASK_DEFAULT:       '255.255.255.0',
  ETH_GATEWAY_DEFAULT:       '192.168.1.1',
  ETH_DNS_DEFAULT:           '8.8.8.8',

  WIFI_DHCP_DEFAULT:         true,
  WIFI_IP_DEFAULT:           '192.168.1.201',
  WIFI_NETMASK_DEFAULT:      '255.255.255.0',
  WIFI_GATEWAY_DEFAULT:      '192.168.1.1',
  WIFI_DNS_DEFAULT:          '8.8.8.8',

  AP_SSID_DEFAULT:           'ESP32-ArtNet-Setup',
  MDNS_NAME_DEFAULT:         'artnet',
  MDNS_NAME_MAX_LEN:         31,

  ARTNET_ENABLED_DEFAULT:    true,
  ARTNET_PORT_DEFAULT:       6454,
  ARTNET_PORT_MIN:           1024,
  ARTNET_PORT_MAX:           65535,

  SACN_ENABLED_DEFAULT:      false,
  SACN_MULTICAST_DEFAULT:    true,
  SACN_PORT_DEFAULT:         5568,
  SACN_PORT_MIN:             1024,
  SACN_PORT_MAX:             65535,

  ESPNOW_CHANNEL_DEFAULT:    0,
  ESPNOW_CHUNK_SIZE_DEFAULT: 200,
  ESPNOW_CHUNK_SIZE_MIN:     16,
  ESPNOW_CHUNK_SIZE_MAX:     230,

  OUTPUT_FPS_DEFAULT:        40,
  OUTPUT_FPS_MIN:            1,
  OUTPUT_FPS_MAX:            44,

  I2C_SDA_DEFAULT:           14,
  I2C_SCL_DEFAULT:           15,
  I2C_SPEED_DEFAULT:         400000,
  I2C_SPEED_MIN:             10000,
  I2C_SPEED_MAX:             1000000,

  STATUS_LED_PIN_DEFAULT:    5,
  ZC_PIN_DISABLED:           255,

  DISPLAY_ENABLED_DEFAULT:     0,
  DISPLAY_I2C_ADDR_DEFAULT:    0x3C,
  DISPLAY_BRIGHTNESS_DEFAULT:  128,
  DISPLAY_BRIGHTNESS_MIN:      0,
  DISPLAY_BRIGHTNESS_MAX:      255,
  DISPLAY_REFRESH_MS_DEFAULT:  500,
  DISPLAY_REFRESH_MS_MIN:      100,
  DISPLAY_REFRESH_MS_MAX:      10000,
  DISPLAY_RECOVER_MS_DEFAULT:  5000,
  DISPLAY_RECOVER_MS_MIN:      1000,
  DISPLAY_RECOVER_MS_MAX:      60000,
  DISPLAY_COLS_DEFAULT:        20,
  DISPLAY_COLS_MIN:            16,
  DISPLAY_COLS_MAX:            40,
  DISPLAY_ROWS_DEFAULT:        4,
  DISPLAY_ROWS_MIN:            2,
  DISPLAY_ROWS_MAX:            4,

  WEB_PORT_DEFAULT:            80,
  WEB_PORT_MIN:                1,
  WEB_PORT_MAX:                65535,

  ESPNOW_QUEUE_DEPTH_DEFAULT:  16,
  ESPNOW_QUEUE_DEPTH_MIN:      4,
  ESPNOW_QUEUE_DEPTH_MAX:      64,

  WIFI_RECONNECT_MS_DEFAULT:   10000,
  WIFI_RECONNECT_MS_MIN:       1000,
  WIFI_RECONNECT_MS_MAX:       60000,

  DEFAULT_OUT_TYPE_DEFAULT:    3,
  DEFAULT_OUT_PIN_DEFAULT:     4,
  DEFAULT_LED_COUNT_DEFAULT:   170,
  DEFAULT_LED_COUNT_MIN:       1,
  DEFAULT_LED_COUNT_MAX:       1000,

  // ── Validation helpers ──
  artnetPortValid(port) {
    return port >= this.ARTNET_PORT_MIN && port <= this.ARTNET_PORT_MAX;
  },
  sacnPortValid(port) {
    return port >= this.SACN_PORT_MIN && port <= this.SACN_PORT_MAX;
  },
  espnowChunkSizeValid(size) {
    return size >= this.ESPNOW_CHUNK_SIZE_MIN && size <= this.ESPNOW_CHUNK_SIZE_MAX;
  },
  outputFpsValid(fps) {
    return fps >= this.OUTPUT_FPS_MIN && fps <= this.OUTPUT_FPS_MAX;
  },
  i2cSpeedValid(speed) {
    return speed >= this.I2C_SPEED_MIN && speed <= this.I2C_SPEED_MAX;
  },
  deviceModeValid(mode) {
    return mode === this.MODE_ARTNET_ETHERNET ||
           mode === this.MODE_ESPNOW_MASTER ||
           mode === this.MODE_ESPNOW_SLAVE;
  },
  deviceModeName(mode) {
    return this.MODE_NAMES[mode] || 'Unknown';
  },
  atLeastOneProtocol(artnetEnabled, sacnEnabled) {
    return artnetEnabled || sacnEnabled;
  }
});
