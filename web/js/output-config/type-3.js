const CONFIG_TYPE_3 = {
  toggleFields: function() {},
  loadFields: function(o) {},
  saveFields: function(ch) { return ch; },
  channelCount: function(o) { return `${o.led_count||170} LEDs / ${ledUniverseCount(o)}U`; },
  configLabel: function(o) { return `${ORDERS[o.color_order]||'GRB'} order`; }
};
