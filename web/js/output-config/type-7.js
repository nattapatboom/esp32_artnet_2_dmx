const RES_OPTS_STEPPER=[[8,'8-bit (1 Pos Ch)'],[10,'10-bit (2 Pos Ch)'],[12,'12-bit (2 Pos Ch)'],[16,'16-bit (2 Pos Ch)'],[24,'24-bit (3 Pos Ch)'],[32,'32-bit (4 Pos Ch)']];
const CONFIG_TYPE_7 = {
  toggleFields: function() {
    setResolutionOptions(RES_OPTS_STEPPER);
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
  },
  channelCount: function(o) {
    const posBytes=valueByteCount(parseInt(o.mc_resolution||8));
    return `${posBytes+2} Ch (${posBytes} Pos + Speed + Cmd)`;
  },
  byteCount: function(o) { return valueByteCount(parseInt(o.mc_resolution||8)); },
  configLabel: function(o) { return `${o.mc_resolution||8}-bit position`; },
  gpioExtract: function(o) {
    const pins=[];
    const add=p=>{p=parseInt(p); if(!isNaN(p)&&p!==255&&!pins.includes(p)) pins.push(p);};
    if(parseInt(o.source||0)===0) add(o.pin);
    if(parseInt(o.pin2_source||0)===0) add(o.pin2);
    if(parseInt(o.pin3_source||0)===0) add(o.pin3);
    if(parseInt(o.mc_homing_mode||0)===0 && parseInt(o.pin4_source||0)===0) add(o.pin4);
    return pins;
  },
  gpioLabel: function(o) {
    const fmt=(src,addr,pin,ch)=>{if(src===0) return pin!==255?'GPIO '+pin:''; return (SOURCES[src]||'Exp')+' 0x'+parseInt(addr).toString(16).toUpperCase()+':CH'+ch;};
    const parts=['STEP: '+fmt(o.source,o.pca_addr,o.pin,o.pca_channel)];
    if(o.pin2!==255||o.pin2_channel!==255) parts.push('DIR: '+fmt(o.pin2_source,o.pin2_addr,o.pin2,o.pin2_channel));
    if(o.pin3!==255||o.pin3_channel!==255) parts.push('EN: '+fmt(o.pin3_source,o.pin3_addr,o.pin3,o.pin3_channel));
    if(o.pin4!==255||o.pin4_channel!==255) parts.push('LIMIT: '+fmt(o.pin4_source,o.pin4_addr,o.pin4,o.pin4_channel));
    return parts.filter(p=>!p.endsWith(': ')).join(' / ');
  },
  slotActive: function(slot,t,mcMode,colorOrder,hMode) {
    if(slot==='pin4'&&hMode!==0) return false;
    return true;
  },
  autoAssignPins: function({takeOutput,takeInput}) {
    const pin2Source=parseInt(document.getElementById('no_pin2_source')?.value||0);
    const pin3Source=parseInt(document.getElementById('no_pin3_source')?.value||0);
    const hMode=parseInt(document.getElementById('mc_homing_mode').value);
    setSelectIfOption('no_pin',takeOutput());
    if(pin2Source===0) setSelectIfOption('no_pin2',takeOutput());
    if(pin3Source===0) setSelectIfOption('no_pin3',takeOutput());
    if(hMode===0) setSelectIfOption('no_pin4',takeOutput());
  }
};
