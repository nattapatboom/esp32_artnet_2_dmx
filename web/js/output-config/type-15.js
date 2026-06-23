const RES_OPTS_PWM_DAC=[[8,'8-bit (1 DMX Ch)'],[10,'10-bit (2 DMX Ch)'],[12,'12-bit (2 DMX Ch)'],[16,'16-bit (2 DMX Ch)']];
const CONFIG_TYPE_15 = {
  toggleFields: function() {
    setResolutionOptions(RES_OPTS_PWM_DAC);
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_freq_grp').style.display = '';
    document.getElementById('mc_freq_lbl').textContent = 'PWM Carrier Frequency (Hz)';
    document.getElementById('rc_filter_grp').style.display = '';
    document.getElementById('pwm_dac_cal_grp').style.display = '';
    calcRcCutoff();
  },
  loadFields: function(o) {
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
    document.getElementById('pwm_dac_mode').value = o.pwm_dac_mode || 0;
    document.getElementById('pwm_dac_min').value = intToDutyPct(o.pwm_dac_min, 0);
    document.getElementById('pwm_dac_max').value = intToDutyPct(o.pwm_dac_max, 10000);
    calcRcCutoff();
  },
  saveFields: function(ch) {
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    ch.pwm_dac_mode = parseInt(document.getElementById('pwm_dac_mode').value);
    ch.pwm_dac_min = dutyPctToInt('pwm_dac_min', 0);
    ch.pwm_dac_max = dutyPctToInt('pwm_dac_max', 10000);
    return ch;
  },
  channelCount: function(o) { return `${valueByteCount(parseInt(o.mc_resolution||8))} Ch`; },
  byteCount: function(o) { return valueByteCount(parseInt(o.mc_resolution||8)); },
  configLabel: function(o) {
    const modes=['Custom','0-10V','4-20mA'];
    return `PWM DAC ${modes[o.pwm_dac_mode||0]||'Custom'} @ ${o.mc_freq||50000}Hz ${o.mc_resolution||8}-bit`;
  }
};
