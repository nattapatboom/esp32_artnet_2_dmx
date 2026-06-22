const CONFIG_TYPE_11 = {
  toggleFields: function() {
    setModeOptions('7seg', 11);
    document.getElementById('mc_mode_grp').style.display = '';
  },
  loadFields: function(o) {
    document.getElementById('mc_mode').value = o.mc_mode || 0;
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
  },
  saveFields: function(ch) {
    ch.mc_mode = parseInt(document.getElementById('mc_mode').value);
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    return ch;
  }
};
