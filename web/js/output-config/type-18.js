const CONFIG_TYPE_18 = {
  toggleFields: function() {},
  loadFields: function(o) {
    document.getElementById('smoke_threshold').value = o.solenoid_threshold || 127;
    document.getElementById('smoke_dur').value = o.smoke_duration_ms || 1000;
    document.getElementById('smoke_settle').value = o.settle_delay_ms || 500;
    document.getElementById('smoke_shoot').value = o.shoot_duration_ms || 1000;
    document.getElementById('smoke_lockout').value = o.smoke_lockout_ms || 2000;
  },
  saveFields: function(ch) {
    ch.solenoid_threshold = parseInt(document.getElementById('smoke_threshold')?.value || 127);
    ch.smoke_duration_ms = parseInt(document.getElementById('smoke_dur')?.value || 1000);
    ch.settle_delay_ms = parseInt(document.getElementById('smoke_settle')?.value || 500);
    ch.shoot_duration_ms = parseInt(document.getElementById('smoke_shoot')?.value || 1000);
    ch.smoke_lockout_ms = parseInt(document.getElementById('smoke_lockout')?.value || 2000);
    return ch;
  }
};
