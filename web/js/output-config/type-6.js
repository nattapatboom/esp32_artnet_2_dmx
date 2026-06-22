const CONFIG_TYPE_6 = {
  toggleFields: function() {
    setModeOptions('motor', 6);
    const mcMode = parseInt(document.getElementById('mc_mode').value || 0);
    document.getElementById('mc_mode_grp').style.display = '';
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_freq_grp').style.display = '';
    document.getElementById('mc_freq_lbl').textContent = 'Frequency (Hz)';
    document.getElementById('mc_deadband_grp').style.display = '';
    document.getElementById('mc_invert_grp').style.display = '';
    document.getElementById('mc_brake_grp').style.display = (mcMode === 0 || mcMode === 2) ? '' : 'none';
  },
  loadFields: function(o) {
    document.getElementById('mc_mode').value = o.mc_mode || 0;
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
    document.getElementById('mc_deadband').value = o.mc_deadband || 10;
    document.getElementById('mc_invert').checked = o.mc_invert || false;
    document.getElementById('mc_brake').checked = o.mc_brake !== false;
  },
  saveFields: function(ch) {
    ch.mc_mode = parseInt(document.getElementById('mc_mode').value);
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    ch.mc_deadband = parseInt(document.getElementById('mc_deadband').value);
    ch.mc_invert = document.getElementById('mc_invert').checked;
    ch.mc_brake = document.getElementById('mc_brake').checked;
    return ch;
  }
};
