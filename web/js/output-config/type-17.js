const CONFIG_TYPE_17 = {
  toggleFields: function() {},
  loadFields: function(o) {
    document.getElementById('sol_threshold').value = o.solenoid_threshold || 127;
    document.getElementById('sol_pulse_ms').value = o.solenoid_pulse_ms || 50;
    document.getElementById('sol_pre_delay').value = o.solenoid_pre_delay || 0;
    document.getElementById('sol_post_delay').value = o.solenoid_post_delay || 100;
  },
  saveFields: function(ch) {
    ch.solenoid_threshold = parseInt(document.getElementById('sol_threshold')?.value || 127);
    ch.solenoid_pulse_ms = parseInt(document.getElementById('sol_pulse_ms')?.value || 50);
    ch.solenoid_pre_delay = parseInt(document.getElementById('sol_pre_delay')?.value || 0);
    ch.solenoid_post_delay = parseInt(document.getElementById('sol_post_delay')?.value || 100);
    return ch;
  },
  configLabel: function(o) { return `${o.solenoid_pulse_ms||50}ms pulse`; }
};
