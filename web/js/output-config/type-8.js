const CONFIG_TYPE_8 = {
  toggleFields: function() {
    setResolutionOptions(false, false);
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_min_us_grp').style.display = '';
    document.getElementById('mc_max_us_grp').style.display = '';
  },
  loadFields: function(o) {
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_min_us').value = o.mc_min_us || 1000;
    document.getElementById('mc_max_us').value = o.mc_max_us || 2000;
  },
  saveFields: function(ch) {
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_min_us = parseInt(document.getElementById('mc_min_us').value);
    ch.mc_max_us = parseInt(document.getElementById('mc_max_us').value);
    return ch;
  }
};
