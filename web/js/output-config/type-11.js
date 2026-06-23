const MODE_OPTS_7SEG_11=[[0,'TM1637 Numeric (2 Ch)'],[1,'TM1637 ASCII Text (4 Ch)']];
const CONFIG_TYPE_11 = {
  toggleFields: function() {
    setModeOptions(MODE_OPTS_7SEG_11, 'Display Mode');
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
  },
  channelCount: function(o) { return parseInt(o.mc_mode||0)===1 ? '4 Ch (ASCII)' : '2 Ch (Numeric)'; },
  byteCount: function(o) { return parseInt(o.mc_mode||0)===1 ? 4 : 2; },
  configLabel: function(o) { return parseInt(o.mc_mode||0)===1 ? '7-Seg 2-Pin ASCII' : '7-Seg 2-Pin Numeric'; },
  gpioExtract: function(o) {
    const pins=[];
    if(parseInt(o.source||0)===0){const p=parseInt(o.pin);if(!isNaN(p)&&p!==255)pins.push(p);}
    if(parseInt(o.source||0)===0){const p=parseInt(o.pin2);if(!isNaN(p)&&p!==255&&!pins.includes(p))pins.push(p);}
    return pins;
  },
  autoAssignPins: function({takeOutput,takeInput}) {
    setSelectIfOption('no_pin',takeOutput());
    setSelectIfOption('no_pin2',takeOutput());
  }
};
