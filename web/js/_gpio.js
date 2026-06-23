// ==========================================
// === SHARED GPIO HELPERS & OUTPUT DEFS ===
// ==========================================

const SENTINEL_NONE = 255;

// Routing slot field keys: "pinN" → JSON object property for pin value and source value
var SLOT_PIN_KEY={pin1:'pin',pin2:'pin2',pin3:'pin3',pin4:'pin4'};
var SLOT_SRC_KEY={pin1:'source',pin2:'pin2_source',pin3:'pin3_source',pin4:'pin4_source'};

function outputModeKeyForObj(o){
  var t=parseInt(o.type||0);
  var mcMode=parseInt(o.mc_mode||0);
  return outputModeKey(t,mcMode);
}
function eachSlot(o,fn){
  var t=parseInt(o.type||0);
  var mode=outputModeDef(t,parseInt(o.mc_mode||0));
  if(!mode) return;
  var is7seg=!!mode.segmentLayout;
  var isCommonDim=outputModeKeyForObj(o)==='commonDim';
  var pinDefs=mode.pins||{};
  Object.keys(pinDefs).sort().forEach(function(slot){
    var rule=pinDefs[slot];
    var n=parseInt(slot.replace('pin',''));
    var pin=255,src=0;
    if(is7seg&&n>1){
      var seg=isCommonDim?n-2:n-1;
      pin=parseInt(o.seg_pins?.[seg]??255);
      src=parseInt(o.seg_sources?.[seg]??0);
    } else if(is7seg&&n===1){
      if(isCommonDim){
        pin=parseInt(o.pin??255);
        src=parseInt(o.source??0);
      } else {
        pin=parseInt(o.seg_pins?.[0]??255);
        src=parseInt(o.seg_sources?.[0]??0);
      }
    } else {
      pin=parseInt(o[SLOT_PIN_KEY[slot]]??255);
      src=parseInt(o[SLOT_SRC_KEY[slot]]??0);
    }
    fn(slot,pin,src,rule);
  });
}
// Generated constants (from C++ headers via build_web.py):
//   GPIO_PINS  — GPIO_PINS.{output,input,inputOnly,reserved}
//   SOURCE_DATA — SOURCE_DATA.{names,addressRules,masks}
//   OUTPUT_DEFS — per-type mode/pin definitions
//   TYPES       — flat type display name map
// ==========================================
const ORDERS=['GRB','RGB','BRG','RBG','RGBW','GRBW','BRGW','WRGB'];

// Derive lookup maps from generated data
const SOURCES=Object.fromEntries(SOURCE_DATA.names.map((n,i)=>[i,n]));
const OUTPUT_GPIOS=GPIO_PINS.output;
const INPUT_GPIOS=GPIO_PINS.input;
const INPUT_ONLY_GPIOS=GPIO_PINS.inputOnly;
const FORBIDDEN_OUTPUT_GPIOS=Object.fromEntries(GPIO_PINS.reserved.map(r=>[r.pin,r.reason]));
const SOURCE_ADDRESS_RULES=Object.fromEntries(SOURCE_DATA.addressRules.map(r=>[r.source,{label:r.label,ranges:r.ranges}]));
const SRC_GPIO=SOURCE_DATA.masks.GPIO, SRC_PCA=SOURCE_DATA.masks.PCA, SRC_DIG=SOURCE_DATA.masks.DIGITAL_EXPANDER, SRC_DAC=SOURCE_DATA.masks.I2C_DAC;
var T=TYPE_META.typeIds;

function outputModeKey(type,mode){
  return TYPE_META.modeKeyMap[parseInt(type)]?.[parseInt(mode||0)]||String(mode);
}
function outputDef(type){ return OUTPUT_DEFS[parseInt(type)]; }
function outputModeDef(type,mode){
  const def=outputDef(type);
  if(!def) return null;
  return def.modes[outputModeKey(type,mode)]||def.modes.default||null;
}
function outputPinRule(type,mode,slot){ return outputModeDef(type,mode)?.pins?.[slot]||null; }
function outputSegmentPinRule(type,mode,segmentIndex){
  const mk=outputModeKey(type,mode);
  const offset=(mk==='commonDim')?2:1;
  const slot='pin'+(parseInt(segmentIndex)+offset);
  return outputPinRule(type,mode,slot);
}
function ruleAllows(rule,sourceFlag,fallback=false){ return rule?!!(rule.sources&sourceFlag):fallback; }
function outputModeCpuUs(type,mode){ return outputModeDef(type,mode)?.cost?.cpuUs||0; }
function modeAllowsSource(type,mode,sourceFlag){
  var def=outputModeDef(type,mode);
  if(!def) return false;
  return Object.values(def.pins||{}).some(function(rule){return !!(rule.sources&sourceFlag);});
}
function modeIsGpioOnly(type,mode){
  var def=outputModeDef(type,mode);
  if(!def) return false;
  var rules=Object.values(def.pins||{});
  return rules.length>0&&rules.every(function(rule){return (rule.sources&~SRC_GPIO)===0;});
}
function modePrimaryAllowsSource(type,mode,sourceFlag){
  var primary=outputPinRule(type,mode,'pin1');
  return !!primary&&!!(primary.sources&sourceFlag);
}

function cfgEl(id){
  var t=document.getElementById('no_type');
  t=t&&parseInt(t.value);
  if(!isNaN(t)){
    var cfg=document.getElementById('type-config-generated');
    if(cfg){
      var el=cfg.querySelector('#'+id);
      if(el) return el;
    }
  }
  return document.getElementById(id);
}

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
  const t=parseInt(o.type);
  if(t===T.DMX) return '512 Ch';
  if(t===T.LED_STRIP) return `${o.led_count||170} LEDs / ${ledUniverseCount(o)}U`;
  if(t===T.STEPPER){
    const posBytes=valueByteCount(parseInt(o.mc_resolution||8));
    return `${posBytes+2} Ch (${posBytes} Pos + Speed + Cmd)`;
  }
  return TYPE_META.channelCounts[t]||'1 Ch';
}

function outputByteCount(o){
  const t=parseInt(o.type);
  if(t===T.STEPPER) return valueByteCount(parseInt(o.mc_resolution||8));
  return TYPE_META.byteCounts[t]??1;
}

function gpioPinCount(o){
  return outputGpios(o).length;
}

function outputGpios(o){
  var pins=[];
  eachSlot(o,function(slot,pin,src,rule){
    if(src===0&&pin!==255&&pins.indexOf(pin)===-1) pins.push(pin);
  });
  var t=parseInt(o.type||0);
  var isStepper=t===T.STEPPER;
  if(isStepper&&parseInt(o.mc_homing_mode||0)>0&&parseInt(o.pin4_source||0)!==0){
    var idx=pins.indexOf(parseInt(o.pin4));
    if(idx!==-1) pins.splice(idx,1);
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

const PIN_GPIOS=GPIO_PINS.output.map(String);
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
  const mcMode=parseInt(cfgEl('mc_mode')?.value||0);
  const hMode=parseInt(cfgEl('mc_homing_mode')?.value||0);
  const colorOrder=parseInt(cfgEl('color_order')?.value||0);
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
  const addrOpts=(srcFilter)=>{
    const rules=SOURCE_DATA.addressRules;
    let groups=rules;
    if(srcFilter!==undefined){
      groups=rules.filter(r=>r.source===srcFilter);
    }
    const deviceLabel = (src) => SOURCES[src] || ('Source '+src);
    const groupKey = (r) => r.ranges.map(ra=>ra[0].toString(16)+'-'+ra[1].toString(16)).join('_');
    const grouped = {};
    for(const r of groups){
      const key = groupKey(r);
      if(!grouped[key]) grouped[key]={label:deviceLabel(r.source)+' 0x'+r.ranges[0][0].toString(16).toUpperCase()+'-0x'+r.ranges[0][1].toString(16).toUpperCase()+(r.ranges[1]?' / 0x'+r.ranges[1][0].toString(16).toUpperCase()+'-0x'+r.ranges[1][1].toString(16).toUpperCase():''),ranges:[]};
      for(const ra of r.ranges) grouped[key].ranges.push(ra);
    }
    return Object.values(grouped).map(g=>`<optgroup label="${g.label}">${g.ranges.map(ra=>Array.from({length:ra[1]-ra[0]+1},(_,i)=>`<option value="${ra[0]+i}">0x${(ra[0]+i).toString(16).toUpperCase()}</option>`).join('')).join('')}</optgroup>`).join('');
  };
  const localBadge=(label)=>`<span style="display:inline-block;width:100%;background:#f1f5f9;color:#64748b;font-size:0.72rem;padding:5px 8px;border-radius:4px;text-align:center;border:1px solid #e2e8f0">${label}</span>`;
  const gpioOpts=(allowNone,dir)=>`${allowNone?'<option value="255">None</option>':''}${(dir==='in'?INPUT_GPIOS:PIN_GPIOS).map(v=>`<option value="${v}">GPIO ${v}</option>`).join('')}`;
  const chOpts=(allowNone)=>`${allowNone?'<option value="255">None</option>':''}${PIN_CHANS.map(v=>`<option value="${v}">CH ${v}</option>`).join('')}`;
  const dacChOpts=[['0','A'],['1','B'],['2','C'],['3','D']].map(([v,l])=>`<option value="${v}">CH ${l}</option>`).join('');
  const slotNumber=(slot)=>parseInt(slot.replace('pin',''));
  const isSevenSegDD=!!(mode?.segmentLayout);
  const isCommonDim=outputModeKey(t,mcMode)==='commonDim';
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
    const n=slotNumber(slot);
    const mask=mode?.slotActiveMask??0;
    if(!(mask&(1<<(n-1)))) return false;
    return true;
  };
  const sourceFor=(field,rule)=>{
    const current=parseInt(saved[field.source]??0);
    if(rule.sources&(1<<current)) return current;
    const allowed=rule.sources;
    if(allowed&SRC_GPIO) return 0;
    if(allowed&SRC_PCA) return 1;
    if(allowed&SRC_DIG) return 2;
    if(allowed&SRC_DAC) return 5;
    return 0;
  };
  const pinDefs=Object.keys(mode.pins).sort((a,b)=>slotNumber(a)-slotNumber(b)).filter(slot=>!isSevenSegDD||slot==='pin1').filter(slotActive).map((slot)=>{
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

  let rows=pinDefs.map((p)=>`
    <div class="pin-row" style="display:flex;gap:6px;align-items:end;margin-bottom:6px;flex-wrap:wrap">
      <div style="min-width:44px;padding-bottom:4px"><div style="font-weight:600;font-size:0.82rem;color:#475569">${p.label}</div><div style="font-size:0.62rem;color:#94a3b8">${p.desc}</div></div>
      <div class="f" style="flex:1;min-width:120px;margin:0">${p.srcHtml}</div>
      <div class="f" style="flex:1;min-width:100px;margin:0">${p.addrHtml}</div>
      <div class="f" style="flex:1;min-width:100px;margin:0">${p.pinHtml}</div>
      <div class="f" style="flex:0 0 auto;margin:0;padding-bottom:4px;display:flex;align-items:center;gap:4px;${p.invId?'':'visibility:hidden'}">
        <input type="checkbox" id="${p.invId||''}">
        <label for="${p.invId||''}" style="font-size:0.75rem;margin:0">Invert</label>
      </div>
    </div>`);

  // 7-segment DD segment pin rows
  if(isSevenSegDD){
    const numSeg=Object.keys(mode.pins||{}).length-(isCommonDim?1:0);
    const firstSegOff=isCommonDim?0:1;
    const segSrc=(seg)=>{
      if(seg===0&&!isCommonDim) return parseInt(saved['no_source']??0);
      return parseInt(saved['no_seg_source_'+seg]??0);
    };
    for(let s=firstSegOff;s<numSeg;s++){
      const segSrcVal=segSrc(s);
      const segLabel='Seg '+(s+1);
      const segDesc=s===0&&!isCommonDim?'Seg A (main pin)':'';
      const segField={source:'no_seg_source_'+s,pin:'no_seg_pin_'+s,addr:'no_seg_addr_'+s,channel:'no_seg_channel_'+s,invert:'no_seg_pin_invert_'+s};
      const segPinSlot=isCommonDim?'pin'+(s+2):'pin'+(s+1);
      const segPinRule=mode.pins[segPinSlot];
      const segRule={sources:segPinRule?.sources??(SRC_GPIO|SRC_PCA),dir:'out',invert:segPinRule?.invert??true};
      const src=sourceFor(segField,segRule);
      let addrHtml=localBadge(src===0?'ESP32 GPIO':'Select Source');
      let pinHtml='';
      if(src===0){
        pinHtml=mkSel(segField.pin,gpioOpts(true));
      } else {
        addrHtml=mkSel(segField.addr,addrOpts(src));
        pinHtml=mkSel(segField.channel,chOpts(true));
      }
      rows.push(`<div class="pin-row" style="display:flex;gap:6px;align-items:end;margin-bottom:6px;flex-wrap:wrap">
        <div style="min-width:44px;padding-bottom:4px"><div style="font-weight:600;font-size:0.82rem;color:#475569">${segLabel}</div><div style="font-size:0.62rem;color:#94a3b8">${segDesc}</div></div>
        <div class="f" style="flex:1;min-width:120px;margin:0">${mkSel(segField.source,srcOpts(segRule.sources))}</div>
        <div class="f" style="flex:1;min-width:100px;margin:0">${addrHtml}</div>
        <div class="f" style="flex:1;min-width:100px;margin:0">${pinHtml}</div>
        <div class="f" style="flex:0 0 auto;margin:0;padding-bottom:4px;display:flex;align-items:center;gap:4px">
          <input type="checkbox" id="${segField.invert}">
          <label for="${segField.invert}" style="font-size:0.75rem;margin:0">Invert</label>
        </div>
      </div>`);
    }
  }

  container.innerHTML=rows.join('');
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
  container.querySelectorAll('select').forEach(function(el){
    if(el.selectedIndex===-1&&el.options.length) el.selectedIndex=0;
  });
}

function populateExpanderAddresses(el, filterSource){
  const rules=SOURCE_DATA.addressRules;
  el.innerHTML='';
  rules.forEach(r=>{
    if(filterSource!==undefined && r.source!==filterSource) return;
    const deviceName=SOURCES[r.source]||('Source '+r.source);
    r.ranges.forEach(ra=>{
      if(ra[0]===0&&ra[1]===0) return;
      const og=document.createElement('optgroup');
      og.label=deviceName+' 0x'+ra[0].toString(16).toUpperCase()+'-0x'+ra[1].toString(16).toUpperCase();
      for(let a=ra[0]; a<=ra[1]; a++){
        const opt=document.createElement('option');
        opt.value=a;
        opt.textContent='0x'+a.toString(16).toUpperCase();
        og.appendChild(opt);
      }
      el.appendChild(og);
    });
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

  if(modePrimaryAllowsSource(t,parseInt(cfgEl('mc_mode')?.value||0),SRC_GPIO)) setSelectIfOption('no_pin',takeOutput());
}

function outputAddressLabel(o){
  const t=typeId(o);
  if(t===T.LED_STRIP||t===T.DMX) return `U${o.start_universe}`;
  return `U${o.start_universe}:CH${o.start_address||1}`;
}

function gpioLabel(o){
  if(parseInt(o.source||0)!==0){
    const srcName=SOURCES[parseInt(o.source||0)]||'Expander';
    return `${srcName} 0x${parseInt(o.pca_addr||0x40).toString(16).toUpperCase()}:CH${o.pca_channel}`;
  }
  const pins=outputGpios(o);
  return pins.length?pins.map(p=>'GPIO '+p).join(' / '):'--';
}

function outputConfigLabel(o){
  return TYPE_META.configLabels[parseInt(o.type)]||`Raw type: ${o.type}`;
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

function setResolutionOptions(opts){
  const sel=cfgEl('mc_resolution');
  if(!sel) return;
  if(!opts||!opts.length){ sel.innerHTML=''; return; }
  const current=parseInt(sel.value||8);
  sel.innerHTML=opts.map(([v,t])=>`<option value="${v}">${t}</option>`).join('');
  sel.value=opts.some(([v])=>v===current)?current:opts[0][0];
}

function setModeOptions(opts, labelText){
  const sel=cfgEl('mc_mode');
  const lbl=cfgEl('mc_mode_lbl');
  if(!sel) return;
  const current=parseInt(sel.value||0);
  if(!opts||!opts.length){ sel.innerHTML=''; return; }
  if(lbl) lbl.textContent=labelText||'Mode';
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
