const CONFIG_TYPE_10 = {
  toggleFields: function() {},
  loadFields: function(o) {},
  saveFields: function(ch) { return ch; },
  channelCount: function(o) { return '3 Ch'; },
  byteCount: function(o) { return 3; },
  configLabel: function(o) { return 'DFPlayer MP3 Module'; },
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
