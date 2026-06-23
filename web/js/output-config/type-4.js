const CONFIG_TYPE_4 = {
  toggleFields: function() {
    setResolutionOptions([[8,'8-bit (1 DMX Ch)'],[10,'10-bit (2 DMX Ch)'],[12,'12-bit (2 DMX Ch)'],[16,'16-bit (2 DMX Ch)']]);
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_freq_grp').style.display = '';
    document.getElementById('mc_freq_lbl').textContent = 'Frequency (Hz)';
  },
  loadFields: function(o) {
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
  },
  saveFields: function(ch) {
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    return ch;
  },
  channelCount: function(o) { return `${valueByteCount(parseInt(o.mc_resolution||8))} Ch`; },
  byteCount: function(o) { return valueByteCount(parseInt(o.mc_resolution||8)); },
  configLabel: function(o) { return `${o.mc_resolution||8}-bit PWM @ ${o.mc_freq||1000}Hz`; }
};
