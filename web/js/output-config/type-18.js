const CONFIG_TYPE_18 = {
  toggleFields: function() {},
  loadFields: function(o) {
    document.getElementById('smoke_threshold').value = o.solenoid_threshold || 127;
    document.getElementById('smoke_dur').value = o.smoke_duration_ms || 1000;
    document.getElementById('smoke_settle').value = o.settle_delay_ms || 500;
    document.getElementById('smoke_shoot').value = o.shoot_duration_ms || 1000;
    document.getElementById('smoke_lockout').value = o.smoke_lockout_ms || 2000;
  },
  saveFields: function(ch) {
    ch.solenoid_threshold = parseInt(document.getElementById('smoke_threshold')?.value || 127);
    ch.smoke_duration_ms = parseInt(document.getElementById('smoke_dur')?.value || 1000);
    ch.settle_delay_ms = parseInt(document.getElementById('smoke_settle')?.value || 500);
    ch.shoot_duration_ms = parseInt(document.getElementById('smoke_shoot')?.value || 1000);
    ch.smoke_lockout_ms = parseInt(document.getElementById('smoke_lockout')?.value || 2000);
    return ch;
  },
  channelCount: function(o) { return '1 Ch'; },
  configLabel: function(o) { return `Smoke Seq (${o.smoke_duration_ms||1000}/${o.settle_delay_ms||500}/${o.shoot_duration_ms||1000} ms)`; },
  gpioExtract: function(o) {
    const pins=[];
    const add=p=>{p=parseInt(p); if(!isNaN(p)&&p!==255&&!pins.includes(p)) pins.push(p);};
    if(parseInt(o.source||0)===0) add(o.pin);
    if(parseInt(o.pin2_source||0)===0) add(o.pin2);
    return pins;
  },
  gpioLabel: function(o) {
    const fmt=(src,addr,pin,ch)=>{if(src===0) return pin!==255?'GPIO '+pin:''; return (SOURCES[src]||'Exp')+' 0x'+parseInt(addr).toString(16).toUpperCase()+':CH'+ch;};
    const parts=['VALVE: '+fmt(o.source,o.pca_addr,o.pin,o.pca_channel),'SHOOT: '+fmt(o.pin2_source,o.pin2_addr,o.pin2,o.pin2_channel)];
    return parts.filter(p=>!p.endsWith(': ')).join(' / ');
  },
  autoAssignPins: function({takeOutput,takeInput}) {
    setSelectIfOption('no_pin',takeOutput());
    setSelectIfOption('no_pin2',takeOutput());
  }
};
