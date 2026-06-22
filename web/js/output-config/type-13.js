const CONFIG_TYPE_13 = {
  toggleFields: function() {
    setModeOptions('7seg', 13);
    setResolutionOptions(false, true);
    const mcMode = parseInt(document.getElementById('mc_mode').value || 3);
    const isDirectDim = (mcMode === 4 || mcMode === 5);
    const isCommonDim = (mcMode >= 6 && mcMode <= 9);
    document.getElementById('mc_mode_grp').style.display = '';
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_resolution_lbl').textContent = 'Decode Mode';
    document.getElementById('mc_freq_grp').style.display = (isDirectDim || isCommonDim) ? '' : 'none';
    document.getElementById('mc_freq_lbl').textContent = 'PWM Frequency (Hz)';
  },
  loadFields: function(o) {
    document.getElementById('mc_mode').value = o.mc_mode || 3;
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
  },
  saveFields: function(ch) {
    ch.mc_mode = parseInt(document.getElementById('mc_mode').value);
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    return ch;
  }
};
