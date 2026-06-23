const CONFIG_TYPE_5 = {
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
  channelCount: function(o) { return (o.color_order||0)>=4 ? '4 Ch (RGBW)' : '3 Ch (RGB)'; },
  byteCount: function(o) { return (o.color_order||0)>=4 ? 4 : 3; },
  configLabel: function(o) { return `${(o.color_order||0)>=4?'RGBW':'RGB'} Analog @ ${o.mc_freq||1000}Hz`; },
  gpioExtract: function(o) {
    const pins=[];
    const add=p=>{p=parseInt(p); if(!isNaN(p)&&p!==255&&!pins.includes(p)) pins.push(p);};
    if(parseInt(o.source||0)===0) add(o.pin);
    if(parseInt(o.pin2_source||0)===0) add(o.pin2);
    if(parseInt(o.pin3_source||0)===0) add(o.pin3);
    if((o.color_order||0)>=4 && parseInt(o.pin4_source||0)===0) add(o.pin4);
    return pins;
  },
  gpioLabel: function(o) {
    const fmt=(src,addr,pin,ch)=>{if(src===0) return pin!==255?'GPIO '+pin:''; return (SOURCES[src]||'Exp')+' 0x'+parseInt(addr).toString(16).toUpperCase()+':CH'+ch;};
    const parts=['R: '+fmt(o.source,o.pca_addr,o.pin,o.pca_channel),'G: '+fmt(o.pin2_source,o.pin2_addr,o.pin2,o.pin2_channel),'B: '+fmt(o.pin3_source,o.pin3_addr,o.pin3,o.pin3_channel)];
    if((o.color_order||0)>=4) parts.push('W: '+fmt(o.pin4_source,o.pin4_addr,o.pin4,o.pin4_channel));
    return parts.filter(p=>!p.endsWith(': ')).join(' / ');
  },
  slotActive: function(slot,t,mcMode,colorOrder,hMode) {
    if(slot==='pin4'&&colorOrder<4) return false;
    return true;
  },
  autoAssignPins: function({takeOutput,takeInput}) {
    setSelectIfOption('no_pin',takeOutput());
    setSelectIfOption('no_pin2',takeOutput());
    setSelectIfOption('no_pin3',takeOutput());
    if(parseInt(document.getElementById('no_ord')?.value||0)>=4) setSelectIfOption('no_pin4',takeOutput());
  }
};
