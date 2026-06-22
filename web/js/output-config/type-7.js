const CONFIG_TYPE_7 = {
  toggleFields: function() {
    setResolutionOptions(true, false);
    const hMode = parseInt(document.getElementById('mc_homing_mode').value || 0);
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_freq_grp').style.display = '';
    document.getElementById('mc_freq_lbl').textContent = 'Speed (Hz)';
    document.getElementById('mc_stepper_scale_grp').style.display = '';
    document.getElementById('mc_steps_grp').style.display = '';
    document.getElementById('mc_unit_type_grp').style.display = '';
    document.getElementById('mc_scale_factor_grp').style.display = '';
    document.getElementById('mc_stepper_homing_grp').style.display = '';
    document.getElementById('mc_homing_mode_grp').style.display = '';
    document.getElementById('mc_homing_dir_grp').style.display = '';
    document.getElementById('mc_homing_speed_grp').style.display = '';
    document.getElementById('mc_homing_timeout_grp').style.display = (hMode === 1) ? '' : 'none';
    document.getElementById('mc_invert_grp').style.display = '';
  },
  loadFields: function(o) {
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
    document.getElementById('mc_steps_per_rev').value = o.mc_steps_per_rev || 200;
    document.getElementById('mc_unit_type').value = o.mc_unit_type || 0;
    document.getElementById('mc_scale_factor').value = o.mc_scale_factor || 0.0;
    document.getElementById('mc_homing_mode').value = o.mc_homing_mode || 0;
    document.getElementById('mc_homing_dir').value = o.mc_homing_dir || 0;
    document.getElementById('mc_homing_speed').value = o.mc_homing_speed || 500;
    document.getElementById('mc_homing_timeout').value = o.mc_homing_timeout || 5;
    document.getElementById('mc_invert').checked = o.mc_invert || false;
  },
  saveFields: function(ch) {
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    ch.mc_steps_per_rev = parseInt(document.getElementById('mc_steps_per_rev').value);
    ch.mc_unit_type = parseInt(document.getElementById('mc_unit_type').value);
    ch.mc_scale_factor = parseFloat(document.getElementById('mc_scale_factor').value);
    ch.mc_homing_mode = parseInt(document.getElementById('mc_homing_mode').value);
    ch.mc_homing_dir = parseInt(document.getElementById('mc_homing_dir').value);
    ch.mc_homing_speed = parseInt(document.getElementById('mc_homing_speed').value);
    ch.mc_homing_timeout = parseInt(document.getElementById('mc_homing_timeout').value);
    ch.mc_invert = document.getElementById('mc_invert').checked;
    return ch;
  }
};
