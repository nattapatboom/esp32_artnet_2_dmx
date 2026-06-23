const CONFIG_TYPE_16 = {
  toggleFields: function() {
    document.getElementById('mc_freq_grp').style.display = '';
    document.getElementById('mc_freq_lbl').textContent = 'PWM Carrier Frequency (Hz)';
    document.getElementById('rc_filter_grp').style.display = '';
    calcRcCutoff();
  },
  loadFields: function(o) {
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
    calcRcCutoff();
  },
  saveFields: function(ch) {
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    return ch;
  },
  channelCount: function(o) { return '5 Ch (Freq+Type+Amp+Off)'; },
  byteCount: function(o) { return 5; },
  configLabel: function(o) { return `Function Generator @ ${o.mc_freq||50000}Hz PWM`; }
};
