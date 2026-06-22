// ==========================================
// === SHARED GPIO HELPERS & OUTPUT DEFS ===
// ==========================================
const ORDERS=['GRB','RGB','BRG','RBG','RGBW','GRBW','BRGW','WRGB'];
const TYPES={0:'AC Dimmer', 1:'DMX Output', 2:'Relay', 3:'RGB LED', 4:'Single Color LED', 5:'Analog RGB / RGBW', 6:'DC Motor', 7:'Stepper', 8:'RC Servo', 9:'Passive Buzzer', 10:'DFPlayer MP3', 11:'7-Segment 2-Pin', 12:'7-Segment DD 7-Pin PWM', 13:'7-Segment DD 8-Pin PWM', 14:'DAC', 15:'PWM DAC', 16:'Function Gen', 17:'Solenoid Trigger', 18:'Smoke Shooter'};
const SOURCES={0:'ESP32',1:'PCA9685',2:'MCP23017',3:'TCA9555',4:'PCF857x',5:'MCP4725 (I2C DAC)',6:'DAC7571 (I2C DAC)',7:'DAC7573 (I2C DAC)'};
const OUTPUT_GPIOS=[4,12,14,15,2,17,32,33];
const INPUT_GPIOS=[36,39,34,35,32,33];
const INPUT_ONLY_GPIOS=[36,39,34,35];
const FORBIDDEN_OUTPUT_GPIOS={0:'Ethernet RMII clock / BOOT',16:'Ethernet PHY power',18:'Ethernet RMII MDIO',19:'Ethernet RMII TXD0',21:'Ethernet RMII TX_EN',22:'Ethernet RMII TXD1',23:'Ethernet RMII MDC',25:'Ethernet RMII RXD0',26:'Ethernet RMII RXD1',27:'Ethernet RMII CRS_DV'};
const SOURCE_ADDRESS_RULES={
  1:{label:'PCA9685 address must be 0x40-0x47',ranges:[[0x40,0x47]]},
  2:{label:'MCP23017 address must be 0x20-0x27',ranges:[[0x20,0x27]]},
  3:{label:'TCA9555 address must be 0x20-0x27',ranges:[[0x20,0x27]]},
  4:{label:'PCF857x address must be 0x20-0x27 or 0x38-0x3F',ranges:[[0x20,0x27],[0x38,0x3F]]},
  5:{label:'MCP4725 address must be 0x60 or 0x61',ranges:[[0x60,0x61]]},
  6:{label:'DAC7571 address must be 0x4C or 0x4D',ranges:[[0x4C,0x4D]]},
  7:{label:'DAC7573 address must be 0x4C-0x5B',ranges:[[0x4C,0x5B]]}
};
const SRC_GPIO=1, SRC_PCA=2, SRC_DIG=4, SRC_DAC=8;

function outputModeKey(type,mode){
  type=parseInt(type); mode=parseInt(mode||0);
  if((type===12||type===13)&&(mode===4||mode===5)) return 'directDim';
  if((type===12||type===13)&&(mode>=6&&mode<=9)) return 'commonDim';
  return String(mode);
}
function outputDef(type){ return OUTPUT_DEFS[parseInt(type)]; }
function outputModeDef(type,mode){
  const def=outputDef(type);
  if(!def) return null;
  return def.modes[outputModeKey(type,mode)]||def.modes.default||null;
}
function outputPinRule(type,mode,slot){ return outputModeDef(type,mode)?.pins?.[slot]||null; }
function outputSegmentPinRule(type,mode,segmentIndex){
  const slot='pin'+(parseInt(segmentIndex)+(((parseInt(type)===12||parseInt(type)===13)&&parseInt(mode||0)>=6&&parseInt(mode||0)<=9)?2:1));
  return outputPinRule(type,mode,slot);
}
function ruleAllows(rule,sourceFlag,fallback=false){ return rule?!!(rule.sources&sourceFlag):fallback; }
function outputModeCpuUs(type,mode){ return outputModeDef(type,mode)?.cost?.cpuUs||0; }

function typeId(o){
  const t=parseInt(o.type);
  return isNaN(t)?-1:t;
}

function deviceLabel(o){
  const t=typeId(o);
  return TYPES[t]||'Unknown';
}

function ledUniverseCount(o){
  const bytesPerPixel=(o.color_order||0)>=4?4:3;
  const pixelsPerUniverse=Math.floor(512/bytesPerPixel);
  return Math.ceil((o.led_count||170)/pixelsPerUniverse);
}

function outputChannelCount(o){
  const t=typeId(o);
  if(t===3) return `${o.led_count||170} LEDs / ${ledUniverseCount(o)}U`;
  if(t===1) return '512 Ch';
  if(t===7){
    const posBytes=valueByteCount(parseInt(o.mc_resolution||8));
    return `${posBytes+2} Ch (${posBytes} Pos + Speed + Cmd)`;
  }
  if(t===5) return (o.color_order||0)>=4 ? '4 Ch (RGBW)' : '3 Ch (RGB)';
  if(t>=4&&t<=8||t===15) return `${valueByteCount(parseInt(o.mc_resolution||8))} Ch`;
  if(t===9) return '2 Ch';
  if(t===18) return '1 Ch';
  if(t>=11&&t<=13){
    const m=parseInt(o.mc_mode||0);
    if(m===1) return '4 Ch (ASCII)';
    if(t>=12) {
      if(m===4||m===5||m===6||m===7||m===8||m===9) return '2 Ch (Char+Dim)';
      return '1 Ch (Direct)';
    }
    return '2 Ch (Numeric)';
  }
  if(t===10) return '3 Ch';
  if(t===14) return '1 Ch';
  if(t===16) return '5 Ch (Freq+Type+Amp+Off)';
  return '1 Ch';
}

function outputByteCount(o){
  const t=typeId(o);
  if(t===6) return valueByteCount(parseInt(o.mc_resolution||8))+2;
  if(t===5) return (o.color_order||0)>=4 ? 4 : 3;
  if(t>=4&&t<=8||t===15) return valueByteCount(parseInt(o.mc_resolution||8));
  if(t===9) return 2;
  if(t===18) return 1;
  if(t>=11&&t<=13){
    const m=parseInt(o.mc_mode||0);
    if(m===1) return 4;
    if(t>=12) {
      if(m===4||m===5||m===6||m===7||m===8||m===9) return 2;
      return 1;
    }
    return 2;
  }
  if(t===10) return 3;
  if(t===16) return 5;
  return 1;
}

function outputGpios(o){
  const t=typeId(o);
  const pins=[];
  const add=p=>{p=parseInt(p); if(!isNaN(p)&&p!==255&&!pins.includes(p)) pins.push(p);};
  if(parseInt(o.source||0)===0) add(o.pin);
  const is7SegDD = (t===12||t===13) && (parseInt(o.mc_mode||0)>=2 && parseInt(o.mc_mode||0)<=9);
  if(is7SegDD && parseInt(o.pin2_source||0)===0){
    const numSeg = (t===13) ? 8 : 7;
    const isCommonDim = (parseInt(o.mc_mode||0)>=6 && parseInt(o.mc_mode||0)<=9);
    const startIdx = isCommonDim ? 0 : 1;
    const sps = o.seg_pins || [];
    const ssrc = o.seg_sources || [];
    for(let s=startIdx; s<numSeg; s++){
      if(parseInt(ssrc[s]||0)!==0) continue;
      const sp = (sps[s] !== undefined && sps[s] !== 255) ? sps[s] : (parseInt(o.pin) + s);
      add(sp);
    }
  }
  if(t===10||(t===11&&!is7SegDD)){
    if(parseInt(o.source||0)===0) add(o.pin2);
  } else if((t===6||t===7||t===5||t===18) && parseInt(o.pin2_source||0)===0){
    add(o.pin2);
  }
  if((t===6 && parseInt(o.mc_mode||0)===2) || t===7 || t===5){
    if(parseInt(o.pin3_source||0)===0) add(o.pin3);
  }
  if((t===7 && parseInt(o.mc_homing_mode||0)===0) || (t===5 && parseInt(o.color_order||0)>=4)){
    if(parseInt(o.pin4_source||0)===0) add(o.pin4);
  }
  return pins;
}

function parseAddrInput(id, fallback){
  const raw=(document.getElementById(id)?.value||'').trim();
  const val=parseInt(raw || fallback);
  return isNaN(val)?fallback:val;
}

function dutyPctToInt(id, fallback){
  const el=document.getElementById(id);
  const val=parseFloat(el?.value ?? fallback);
  if(isNaN(val)) return fallback;
  return Math.max(0, Math.min(10000, Math.round(val*100)));
}

function intToDutyPct(v, fallback){
  v=parseInt(v);
  if(isNaN(v)) v=fallback;
  return (Math.max(0, Math.min(10000, v))/100).toFixed(2);
}

const PIN_GPIOS=['4','12','14','15','2','17','32','33'];
const PIN_CHANS=['0','1','2','3','4','5','6','7','8','9','10','11','12','13','14','15'];

function renderPinRows(){
  const container=document.getElementById('pin-mapping-container');
  if(!container) return;
  const saved={};
  ['no_source','no_pin','no_pca_addr','no_pca_channel','no_pin2','no_pin2_source','no_pin2_addr','no_pin2_channel','no_pin3','no_pin3_source','no_pin3_addr','no_pin3_channel','no_pin4','no_pin4_source','no_pin4_addr','no_pin4_channel'].forEach(id=>{const el=document.getElementById(id);if(el)saved[id]=el.value;});
  ['no_pin_invert','no_pin2_invert','no_pin3_invert','no_pin4_invert'].forEach(id=>{const el=document.getElementById(id);if(el)saved[id]=el.checked;});
  for(let s=0; s<=7; s++){const el=document.getElementById('no_seg_pin_'+s);if(el)saved['no_seg_pin_'+s]=el.value;}
  for(let s=0; s<=7; s++){const el=document.getElementById('no_seg_source_'+s);if(el)saved['no_seg_source_'+s]=el.value;}
  for(let s=0; s<=7; s++){const el=document.getElementById('no_seg_addr_'+s);if(el)saved['no_seg_addr_'+s]=el.value;}
  for(let s=0; s<=7; s++){const el=document.getElementById('no_seg_channel_'+s);if(el)saved['no_seg_channel_'+s]=el.value;}
  for(let s=0; s<=7; s++){const el=document.getElementById('no_seg_pin_invert_'+s);if(el)saved['no_seg_pin_invert_'+s]=el.checked;}

  const t=parseInt(document.getElementById('no_type').value);
  const mcMode=parseInt(document.getElementById('mc_mode')?.value||0);
  const hMode=parseInt(document.getElementById('mc_homing_mode')?.value||0);
  const colorOrder=parseInt(document.getElementById('no_ord')?.value||0);
  const mode=outputModeDef(t,mcMode);
  if(!mode){ container.innerHTML=''; return; }

  const srcOpts=(mask)=>{
    let h='';
    if(mask&SRC_GPIO) h+=`<option value="0">ESP32 GPIO</option>`;
    if(mask&SRC_PCA) h+=`<option value="1">PCA9685</option>`;
    if(mask&SRC_DIG) h+=`<option value="2">MCP23017</option><option value="3">TCA9555</option><option value="4">PCF857x</option>`;
    if(mask&SRC_DAC) h+=`<option value="5">MCP4725</option><option value="6">DAC7571</option><option value="7">DAC7573</option>`;
    return h;
  };
  const mkSel=(id,opts)=>`<select id="${id}" onchange="toggleOutFields()" style="width:100%">${opts}</select>`;
  const addrOpts=(f)=>{
    const gs=[
      {l:'MCP23017 / TCA9555 0x20-0x27', s:[2,3], r:[0x20,0x27]},
      {l:'PCF857x 0x20-0x27', s:[4], r:[0x20,0x27]},
      {l:'PCF8574A 0x38-0x3F', s:[4], r:[0x38,0x3F]},
      {l:'PCA9685 PWM 0x40-0x47', s:[1], r:[0x40,0x47]},
      {l:'MCP4725 DAC 0x60-0x61', s:[5], r:[0x60,0x61]},
      {l:'DAC7571 0x4C-0x4D', s:[6], r:[0x4C,0x4D]},
      {l:'DAC7573 0x4C-0x5B', s:[7], r:[0x4C,0x5B]}
    ];
    return gs.filter(g=>f===undefined||g.s.includes(f)).map(g=>`<optgroup label="${g.l}">${Array.from({length:g.r[1]-g.r[0]+1},(_,i)=>`<option value="${g.r[0]+i}">0x${(g.r[0]+i).toString(16).toUpperCase()}</option>`).join('')}</optgroup>`).join('');
  };
  const localBadge=(label)=>`<span style="display:inline-block;width:100%;background:#f1f5f9;color:#64748b;font-size:0.72rem;padding:5px 8px;border-radius:4px;text-align:center;border:1px solid #e2e8f0">${label}</span>`;
  const gpioOpts=(allowNone,dir)=>`${allowNone?'<option value="255">None</option>':''}${(dir==='in'?INPUT_GPIOS:PIN_GPIOS).map(v=>`<option value="${v}">GPIO ${v}</option>`).join('')}`;
  const chOpts=(allowNone)=>`${allowNone?'<option value="255">None</option>':''}${PIN_CHANS.map(v=>`<option value="${v}">CH ${v}</option>`).join('')}`;
  const dacChOpts=[['0','A'],['1','B'],['2','C'],['3','D']].map(([v,l])=>`<option value="${v}">CH ${l}</option>`).join('');
  const slotNumber=(slot)=>parseInt(slot.replace('pin',''));
  const isSevenSegDD=(t===12||t===13)&&(mcMode>=2&&mcMode<=9);
  const isCommonDim=(t===12||t===13)&&(mcMode>=6&&mcMode<=9);
  const fieldFor=(slot)=>{
    const n=slotNumber(slot);
    if(isSevenSegDD&&n>1){
      const seg=isCommonDim?n-2:n-1;
      return {source:`no_seg_source_${seg}`,pin:`no_seg_pin_${seg}`,addr:`no_seg_addr_${seg}`,channel:`no_seg_channel_${seg}`,invert:`no_seg_pin_invert_${seg}`,seg};
    }
    if(n===1) return {source:'no_source',pin:'no_pin',addr:'no_pca_addr',channel:'no_pca_channel',invert:'no_pin_invert'};
    return {source:`no_pin${n}_source`,pin:`no_pin${n}`,addr:`no_pin${n}_addr`,channel:`no_pin${n}_channel`,invert:`no_pin${n}_invert`};
  };
  const slotActive=(slot)=>{
    if(t===5&&slot==='pin4'&&colorOrder<4) return false;
    if(t===7&&slot==='pin4'&&hMode!==0) return false;
    return true;
  };
  const sourceFor=(field,rule)=>{
    const current=parseInt(saved[field.source]??0);
    if(rule.sources&SRC_DAC) return (current>=5&&current<=7)?current:5;
    if(rule.sources&SRC_GPIO) return current;
    if(rule.sources&SRC_PCA) return current===1?current:1;
    if(rule.sources&SRC_DIG) return (current>=2&&current<=4)?current:2;
    return current;
  };
  const pinDefs=Object.keys(mode.pins).sort((a,b)=>slotNumber(a)-slotNumber(b)).filter(slotActive).map((slot)=>{
    const rule=mode.pins[slot];
    const field=fieldFor(slot);
    const src=sourceFor(field,rule);
    const allowNone=slot!=='pin1'||field.seg!==undefined;
    let addrHtml=localBadge(src===0?'ESP32 GPIO':'Select Source');
    let pinHtml='';
    if(rule.sources&SRC_DAC){
      addrHtml=mkSel(field.addr,addrOpts(src));
      pinHtml=src===7?mkSel(field.channel,dacChOpts):localBadge('CH A');
    } else if(src===0){
      pinHtml=mkSel(field.pin,gpioOpts(allowNone,rule.dir));
    } else {
      addrHtml=mkSel(field.addr,addrOpts(src));
      pinHtml=mkSel(field.channel,chOpts(allowNone));
    }
    return {label:rule.label||slot,desc:rule.dir==='in'?'Input':'Output',srcHtml:mkSel(field.source,srcOpts(rule.sources)),addrHtml,pinHtml,invId:rule.invert?field.invert:''};
  });

  container.innerHTML=pinDefs.map((p)=>`
    <div class="pin-row" style="display:flex;gap:6px;align-items:end;margin-bottom:6px;flex-wrap:wrap">
      <div style="min-width:44px;padding-bottom:4px"><div style="font-weight:600;font-size:0.82rem;color:#475569">${p.label}</div><div style="font-size:0.62rem;color:#94a3b8">${p.desc}</div></div>
      <div class="f" style="flex:1;min-width:120px;margin:0">${p.srcHtml}</div>
      <div class="f" style="flex:1;min-width:100px;margin:0">${p.addrHtml}</div>
      <div class="f" style="flex:1;min-width:100px;margin:0">${p.pinHtml}</div>
      <div class="f" style="flex:0 0 auto;margin:0;padding-bottom:4px;display:flex;align-items:center;gap:4px;${p.invId?'':'visibility:hidden'}">
        <input type="checkbox" id="${p.invId||''}">
        <label for="${p.invId||''}" style="font-size:0.75rem;margin:0">Invert</label>
      </div>
    </div>`).join('');
  Object.keys(saved).forEach(id=>{
    const el=document.getElementById(id);
    if(!el) return;
    if(el.type==='checkbox'){
      el.checked=!!saved[id];
    } else if(el.tagName==='SELECT'){
      el.value=saved[id];
      if(el.value!==saved[id]&&el.options.length) el.value=el.options[0].value;
    }
  });
}

function populateExpanderAddresses(el, filterSource){
  const groups=[
    {label:'MCP23017 / TCA9555 0x20-0x27', sources:[2,3], range:[0x20,0x27]},
    {label:'PCF857x 0x20-0x27', sources:[4], range:[0x20,0x27]},
    {label:'PCF8574A 0x38-0x3F', sources:[4], range:[0x38,0x3F]},
    {label:'PCA9685 PWM 0x40-0x47', sources:[1], range:[0x40,0x47]},
    {label:'MCP4725 DAC 0x60-0x61', sources:[5], range:[0x60,0x61]},
    {label:'DAC7571 0x4C-0x4D', sources:[6], range:[0x4C,0x4D]},
    {label:'DAC7573 0x4C-0x5B', sources:[7], range:[0x4C,0x5B]}
  ];
  el.innerHTML='';
  groups.forEach(g=>{
    if(filterSource!==undefined && !g.sources.includes(filterSource)) return;
    const og=document.createElement('optgroup');
    og.label=g.label;
    for(let a=g.range[0]; a<=g.range[1]; a++){
      const opt=document.createElement('option');
      opt.value=a;
      opt.textContent='0x'+a.toString(16).toUpperCase();
      og.appendChild(opt);
    }
    el.appendChild(og);
  });
}

function sourceAddressValid(src, addr){
  src=parseInt(src||0); addr=parseInt(addr);
  if(src===0) return true;
  const rule=SOURCE_ADDRESS_RULES[src];
  return !!rule&&rule.ranges.some(([min,max])=>addr>=min&&addr<=max);
}

function sourceAddressRangeLabel(src){
  src=parseInt(src||0);
  return SOURCE_ADDRESS_RULES[src]?.label||'Unsupported I2C source';
}

function validateSourceAddress(src, addr, label){
  if(sourceAddressValid(src, addr)) return true;
  alert(label+' has invalid I2C address 0x'+parseInt(addr).toString(16).toUpperCase()+'. '+sourceAddressRangeLabel(src)+'.');
  return false;
}

function displayAddressValid(type, addr){
  type=parseInt(type||0); addr=parseInt(addr);
  if(type===0) return true;
  if(type===1 || type===2) return addr===0x3C || addr===0x3D;
  if(type===3) return addr===0x27 || addr===0x3F;
  return false;
}

function setHybridDefaultAddr(sourceId, addrId){
  const src=parseInt(document.getElementById(sourceId)?.value||0);
  const addrEl=document.getElementById(addrId);
  if(!addrEl) return;
  const addr=parseInt(addrEl.value||'0');
  if(src===1 && addr<64) addrEl.value='64';
  if(src>=2 && src<=4 && addr>=64) addrEl.value='32';
}

function expanderPinLabel(src, addr, channel){
  src=parseInt(src||0);
  channel=parseInt(channel);
  if(src===0||isNaN(channel)||channel===255) return '';
  const srcName=SOURCES[src]||'Expander';
  return `${srcName} 0x${parseInt(addr||32).toString(16).toUpperCase()}:CH${channel}`;
}

function outputReservedPinConflict(list,pin){
  pin=parseInt(pin);
  if(isNaN(pin)||pin===255) return -1;
  return list.findIndex(o=>outputGpios(o).includes(pin));
}

function reservedPins(extra=[]){
  const pins=new Set();
  outputs.forEach((o,i)=>{
    if(i===editOutIdx) return;
    outputGpios(o).forEach(p=>pins.add(p));
  });
  ['status_led_pin','zc_pin','i2c_sda','i2c_scl'].forEach(id=>{
    const e=document.getElementById(id);
    if(!e) return;
    const p=parseInt(e.value);
    if(!isNaN(p)&&p!==255) pins.add(p);
  });
  extra.forEach(p=>{p=parseInt(p); if(!isNaN(p)&&p!==255) pins.add(p);});
  return pins;
}

function firstFreeGpio(pool, used){
  for(const p of pool){ if(!used.has(p)) return p; }
  return 255;
}

function setSelectIfOption(id,value){
  const e=document.getElementById(id);
  if(!e) return;
  if([...e.options].some(o=>parseInt(o.value)===value)) e.value=value;
}

function autoAssignOutputPins(){
  if(editOutIdx!==-1) return;
  const t=parseInt(document.getElementById('no_type').value);
  const mcMode=parseInt(document.getElementById('mc_mode').value);
  const hMode=parseInt(document.getElementById('mc_homing_mode').value);
  const colorOrder = parseInt(document.getElementById('no_ord')?.value||0);
  ['no_pin','no_pin2','no_pin3','no_pin4'].forEach(id=>setSelectIfOption(id,255));
  for(let s=0; s<=7; s++){setSelectIfOption('no_seg_pin_'+s,255);}
  const picked=[];
  const takeOutput=()=>{
    const used=reservedPins(picked);
    const p=firstFreeGpio(OUTPUT_GPIOS,used);
    if(p!==255) picked.push(p);
    return p;
  };
  const takeInput=()=>{
    const used=reservedPins(picked);
    let p=firstFreeGpio(INPUT_ONLY_GPIOS,used);
    if(p===255) p=firstFreeGpio(INPUT_GPIOS,used);
    if(p!==255) picked.push(p);
    return p;
  };

  const pin2Source=parseInt(document.getElementById('no_pin2_source')?.value||0);
  const pin3Source=parseInt(document.getElementById('no_pin3_source')?.value||0);
  
  if((t===12||t===13) && pin2Source===0){
    const numSeg = t === 13 ? 8 : 7;
    const startSeg = (mcMode>=6&&mcMode<=9) ? 0 : 1;
    const basePin = takeOutput();
    setSelectIfOption('no_pin', basePin);
    if(basePin !== 255){
      for(let s = startSeg; s < numSeg; s++){
        setSelectIfOption('no_seg_pin_' + s, takeOutput());
      }
    }
    return;
  }

  if (t !== 14) setSelectIfOption('no_pin',takeOutput());
  if(t===6||t===5||t===18||(t>=11&&t<=13)||t===10||(t===7&&pin2Source===0)) setSelectIfOption('no_pin2',takeOutput());
  if(t===5||(t===6&&mcMode===2)||(t===7&&pin3Source===0)) setSelectIfOption('no_pin3',takeOutput());
  if((t===7&&hMode===0)||(t===5&&colorOrder>=4)) setSelectIfOption('no_pin4',takeOutput());
}

function outputAddressLabel(o){
  const t=typeId(o);
  if(t===3||t===1) return `U${o.start_universe}`;
  return `U${o.start_universe}:CH${o.start_address||1}`;
}

function gpioLabel(o){
  const t=typeId(o);
  const formatAddr=addr=>'0x'+parseInt(addr).toString(16).toUpperCase();
  const getLabel=(src, addr, pin, ch) => {
    if(src === 0) return pin !== 255 ? 'GPIO ' + pin : '';
    const srcName = SOURCES[src] || 'Exp';
    return `${srcName} ${formatAddr(addr)}:CH${ch}`;
  };

  if(t===7) {
    const parts = [];
    parts.push('STEP: ' + getLabel(o.source, o.pca_addr, o.pin, o.pca_channel));
    if(o.pin2 !== 255 || o.pin2_channel !== 255) {
      parts.push('DIR: ' + getLabel(o.pin2_source, o.pin2_addr, o.pin2, o.pin2_channel));
    }
    if(o.pin3 !== 255 || o.pin3_channel !== 255) {
      parts.push('EN: ' + getLabel(o.pin3_source, o.pin3_addr, o.pin3, o.pin3_channel));
    }
    if(o.pin4 !== 255 || o.pin4_channel !== 255) {
      parts.push('LIMIT: ' + getLabel(o.pin4_source, o.pin4_addr, o.pin4, o.pin4_channel));
    }
    return parts.filter(p => !p.endsWith(': ')).join(' / ');
  }

  if(t===6) {
    const parts = [];
    parts.push('PWM/IN1: ' + getLabel(o.source, o.pca_addr, o.pin, o.pca_channel));
    if(o.pin2 !== 255 || o.pin2_channel !== 255) {
      parts.push('IN2/DIR: ' + getLabel(o.pin2_source, o.pin2_addr, o.pin2, o.pin2_channel));
    }
    if(o.pin3 !== 255 || o.pin3_channel !== 255) {
      parts.push('EN: ' + getLabel(o.pin3_source, o.pin3_addr, o.pin3, o.pin3_channel));
    }
    return parts.filter(p => !p.endsWith(': ')).join(' / ');
  }

  if(t===5) {
    const parts = [];
    parts.push('R: ' + getLabel(o.source, o.pca_addr, o.pin, o.pca_channel));
    parts.push('G: ' + getLabel(o.pin2_source, o.pin2_addr, o.pin2, o.pin2_channel));
    parts.push('B: ' + getLabel(o.pin3_source, o.pin3_addr, o.pin3, o.pin3_channel));
    if((o.color_order||0) >= 4) {
      parts.push('W: ' + getLabel(o.pin4_source, o.pin4_addr, o.pin4, o.pin4_channel));
    }
    return parts.filter(p => !p.endsWith(': ')).join(' / ');
  }

  if(t===18) {
    const parts = [];
    parts.push('VALVE: ' + getLabel(o.source, o.pca_addr, o.pin, o.pca_channel));
    parts.push('SHOOT: ' + getLabel(o.pin2_source, o.pin2_addr, o.pin2, o.pin2_channel));
    return parts.filter(p => !p.endsWith(': ')).join(' / ');
  }

  if(parseInt(o.source||0)!==0){
    const srcName=SOURCES[parseInt(o.source||0)]||'Expander';
    return `${srcName} ${formatAddr(o.pca_addr || 0x40)}:CH${o.pca_channel}`;
  }
  const pins=outputGpios(o);
  return pins.length?pins.map(p=>'GPIO '+p).join(' / '):'--';
}

function outputConfigLabel(o){
  const t=typeId(o);
  if(t===3) return `${ORDERS[o.color_order]||'GRB'} order`;
  if(t===1) return 'DMX512 serial';
  if(t===2) return 'Threshold 128';
  if(t===0) return 'ZC dimmer';
  if(t===4) return `${o.mc_resolution||8}-bit PWM @ ${o.mc_freq||1000}Hz`;
  if(t===6){
    const modes=['PWM/PWM','PWM/DIR','IN1/IN2/EN'];
    return `${modes[parseInt(o.mc_mode||0)]||'Motor'} ${o.mc_resolution||8}-bit`;
  }
  if(t===7) return `${o.mc_resolution||8}-bit position`;
  if(t===8) return `${o.mc_min_us||1000}-${o.mc_max_us||2000}us`;
  if(t===17) return `${o.solenoid_pulse_ms||50}ms pulse`;
  if(t===5) return `${(o.color_order||0)>=4?'RGBW':'RGB'} Analog @ ${o.mc_freq||1000}Hz`;
  if(t===9) return 'Passive Buzzer';
  if(t===18) return `Smoke Seq (${o.smoke_duration_ms||1000}/${o.settle_delay_ms||500}/${o.shoot_duration_ms||1000} ms)`;
  if(t>=11&&t<=13){if(t===11){const m=parseInt(o.mc_mode||0);return m===1?'7-Seg 2-Pin ASCII':'7-Seg 2-Pin Numeric';}else{return t===12?'7-Seg DD 7-Pin PWM':'7-Seg DD 8-Pin PWM';}}
  if(t===10) return 'DFPlayer MP3 Module';
  if(t===14) return 'DAC';
  if(t===15) {
    const modes=['Custom','0-10V','4-20mA'];
    return `PWM DAC ${modes[o.pwm_dac_mode||0]||'Custom'} @ ${o.mc_freq||50000}Hz ${o.mc_resolution||8}-bit`;
  }
  if(t===16) return `Function Generator @ ${o.mc_freq||50000}Hz PWM`;
  return `Raw type: ${o.type}`;
}

function migrateOutput(o){
  const t = parseInt(o.type);
  if (t === 4) o.type = 4;
  else if (t === 5) o.type = 6;
  else if (t === 6) o.type = 7;
  else if (t === 7) o.type = 8;
  else if (t === 8) o.type = 17;
  else if (t === 9) o.type = 5;
  else if (t === 10) o.type = 9;
  else if (t === 11) o.type = 18;
  else if (t === 12) o.type = 11;
  else if (t === 13) o.type = 10;
  else if (t === 15) o.type = 15;
  return o;
}

function valueByteCount(res){
  res=parseInt(res||8);
  if(res<=8) return 1;
  if(res<=16) return 2;
  if(res<=24) return 3;
  return 4;
}
function maxDmxFor(o){
  const res=parseInt(o.mc_resolution||8);
  return Math.pow(2,Math.min(res,32))-1;
}
function valueBytes(v,res){
  const bytes=valueByteCount(res);
  const max=Math.pow(2,Math.min(parseInt(res||8),32))-1;
  v=Math.max(0,Math.min(Math.round(Number(v)||0),max));
  const out=[];
  for(let i=bytes-1;i>=0;i--) out.push(Math.floor(v/Math.pow(256,i))&255);
  return out;
}

function setResolutionOptions(isStepper, is7Seg){
  const sel=document.getElementById('mc_resolution');
  const current=parseInt(sel.value||8);
  const opts=is7Seg
    ? [[8,'ASCII / Character'],[10,'Numeric (0-9, 10-19 for DP)']]
    : isStepper
      ? [[8,'8-bit (1 Pos Ch)'],[10,'10-bit (2 Pos Ch)'],[12,'12-bit (2 Pos Ch)'],[16,'16-bit (2 Pos Ch)'],[24,'24-bit (3 Pos Ch)'],[32,'32-bit (4 Pos Ch)']]
      : [[8,'8-bit (1 DMX Ch)'],[10,'10-bit (2 DMX Ch)'],[12,'12-bit (2 DMX Ch)'],[16,'16-bit (2 DMX Ch)']];
  sel.innerHTML=opts.map(([v,t])=>`<option value="${v}">${t}</option>`).join('');
  const allowed=opts.some(([v])=>v===current);
  sel.value=allowed?current:8;
}

function setModeOptions(kind, type){
  const sel=document.getElementById('mc_mode');
  const lbl=document.getElementById('mc_mode_lbl');
  const current=parseInt(sel.value||0);
  const opts=kind==='7seg'
    ? type===11?[[0,'TM1637 Numeric (2 Ch)'],[1,'TM1637 ASCII Text (4 Ch)']]
      :type===12?[[2,'Direct 7-pin (1 Ch - No Dim)'],[4,'Direct 7-pin Dim (2 Ch - Char+Dim)'],[6,'CA 7-pin Dim (2 Ch - COM Anode)'],[7,'CC 7-pin Dim (2 Ch - COM Cathode)']]
      :type===13?[[3,'Direct 8-pin (1 Ch - No Dim)'],[5,'Direct 8-pin Dim (2 Ch - Char+Dim)'],[8,'CA 8-pin Dim (2 Ch - COM Anode)'],[9,'CC 8-pin Dim (2 Ch - COM Cathode)']]
      :[[0,'TM1637 Numeric (2 Ch)'],[1,'TM1637 ASCII Text (4 Ch)'],
       [2,'Direct 7-pin (1 Ch - No Dim)'],[3,'Direct 8-pin (1 Ch - No Dim)'],
       [4,'Direct 7-pin Dim (2 Ch - Char+Dim)'],[5,'Direct 8-pin Dim (2 Ch - Char+Dim)'],
       [6,'CA 7-pin Dim (2 Ch - COM Anode)'],[7,'CC 7-pin Dim (2 Ch - COM Cathode)'],
       [8,'CA 8-pin Dim (2 Ch - COM Anode)'],[9,'CC 8-pin Dim (2 Ch - COM Cathode)']]
    : [[0,'PWM + PWM (2 pins)'],[1,'PWM + DIR (2 pins)'],[2,'IN1 + IN2 + EN (3 pins)']];
  if(lbl) lbl.textContent=kind==='7seg'?'Display Mode':'Motor Sub-Mode';
  sel.innerHTML=opts.map(([v,t])=>`<option value="${v}">${t}</option>`).join('');
  sel.value=opts.some(([v])=>v===current)?current:opts[0][0];
}

function checkStrappingPin() {
  const strapWarn = document.getElementById('strapping-warning');
  if (!strapWarn) return;
  const hasStrapPin = outputs.some(o => outputGpios(o).includes(12)) ||
                      parseInt(document.getElementById('status_led_pin')?.value||'255') === 12 ||
                      parseInt(document.getElementById('zc_pin')?.value||'255') === 12 ||
                      parseInt(document.getElementById('i2c_sda')?.value||'255') === 12 ||
                      parseInt(document.getElementById('i2c_scl')?.value||'255') === 12;
  strapWarn.style.display = hasStrapPin ? '' : 'none';
}
