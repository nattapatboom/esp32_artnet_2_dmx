const CONFIG_TYPE_15 = {
  toggleFields: function() {
    setResolutionOptions(false, false);
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
  }
};
