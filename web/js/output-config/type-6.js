const RES_OPTS_COMMON=[[8,'8-bit (1 DMX Ch)'],[10,'10-bit (2 DMX Ch)'],[12,'12-bit (2 DMX Ch)'],[16,'16-bit (2 DMX Ch)']];
const MODE_OPTS_MOTOR=[[0,'PWM + PWM (2 pins)'],[1,'PWM + DIR (2 pins)'],[2,'IN1 + IN2 + EN (3 pins)']];
const CONFIG_TYPE_6 = {
  toggleFields: function() {
    setModeOptions(MODE_OPTS_MOTOR, 'Motor Sub-Mode');
    const mcMode = parseInt(document.getElementById('mc_mode').value || 0);
    document.getElementById('mc_mode_grp').style.display = '';
    setResolutionOptions(RES_OPTS_COMMON);
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_freq_grp').style.display = '';
    document.getElementById('mc_freq_lbl').textContent = 'Frequency (Hz)';
    document.getElementById('mc_deadband_grp').style.display = '';
    document.getElementById('mc_invert_grp').style.display = '';
    document.getElementById('mc_brake_grp').style.display = (mcMode === 0 || mcMode === 2) ? '' : 'none';
  },
  loadFields: function(o) {
    document.getElementById('mc_mode').value = o.mc_mode || 0;
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
    document.getElementById('mc_deadband').value = o.mc_deadband || 10;
    document.getElementById('mc_invert').checked = o.mc_invert || false;
    document.getElementById('mc_brake').checked = o.mc_brake !== false;
  },
  saveFields: function(ch) {
    ch.mc_mode = parseInt(document.getElementById('mc_mode').value);
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    ch.mc_deadband = parseInt(document.getElementById('mc_deadband').value);
    ch.mc_invert = document.getElementById('mc_invert').checked;
    ch.mc_brake = document.getElementById('mc_brake').checked;
    return ch;
  },
  channelCount: function(o) { return `${valueByteCount(parseInt(o.mc_resolution||8))} Ch`; },
  byteCount: function(o) { return valueByteCount(parseInt(o.mc_resolution||8))+2; },
  configLabel: function(o) {
    const modes=['PWM/PWM','PWM/DIR','IN1/IN2/EN'];
    return `${modes[parseInt(o.mc_mode||0)]||'Motor'} ${o.mc_resolution||8}-bit`;
  },
  gpioExtract: function(o) {
    const pins=[];
    const add=p=>{p=parseInt(p); if(!isNaN(p)&&p!==255&&!pins.includes(p)) pins.push(p);};
    if(parseInt(o.source||0)===0) add(o.pin);
    if(parseInt(o.pin2_source||0)===0) add(o.pin2);
    if(parseInt(o.mc_mode||0)===2 && parseInt(o.pin3_source||0)===0) add(o.pin3);
    return pins;
  },
  gpioLabel: function(o) {
    const fmt=(src,addr,pin,ch)=>{if(src===0) return pin!==255?'GPIO '+pin:''; return (SOURCES[src]||'Exp')+' 0x'+parseInt(addr).toString(16).toUpperCase()+':CH'+ch;};
    const parts=['PWM/IN1: '+fmt(o.source,o.pca_addr,o.pin,o.pca_channel)];
    if(o.pin2!==255||o.pin2_channel!==255) parts.push('IN2/DIR: '+fmt(o.pin2_source,o.pin2_addr,o.pin2,o.pin2_channel));
    if(o.pin3!==255||o.pin3_channel!==255) parts.push('EN: '+fmt(o.pin3_source,o.pin3_addr,o.pin3,o.pin3_channel));
    return parts.filter(p=>!p.endsWith(': ')).join(' / ');
  },
  autoAssignPins: function({takeOutput,takeInput}) {
    setSelectIfOption('no_pin',takeOutput());
    setSelectIfOption('no_pin2',takeOutput());
    if(parseInt(document.getElementById('mc_mode').value)===2) setSelectIfOption('no_pin3',takeOutput());
  }
};
