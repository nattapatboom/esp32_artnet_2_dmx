// ==========================================
// === SHARED GPIO HELPERS & OUTPUT DEFS ===
// ==========================================

const SENTINEL_NONE = 255;

function outputModeKeyForObj(o){
  var t=parseInt(o.type||0);
  var mcMode=parseInt(o.mc_mode||0);
  return outputModeKey(t,mcMode);
}
function getSlotValue(o, slot) {
  const n = parseInt(slot.replace('pin',''));
  if (!o.pins || !o.pins[n-1]) {
    return { pin: 255, source: 0, addr: (n === 1 ? 64 : 32), channel: (n === 1 ? 0 : 255), invert: false };
  }
  const p = o.pins[n-1];
  return {
    pin: parseInt(p.pin ?? 255),
    source: parseInt(p.source ?? 0),
    addr: parseInt(p.addr ?? (n === 1 ? 64 : 32)),
    channel: parseInt(p.channel ?? (n === 1 ? 0 : 255)),
    invert: !!p.invert
  };
}

function defaultChannelRoutes(ch){
  const t = parseInt(ch.type || 0);
  const modeVal = parseInt(ch.mc_mode || 0);
  const mode = outputModeDef(t, modeVal);
  const pinSlots = mode ? Object.keys(mode.pins || {}) : [];
  
  ch.pins = pinSlots.map(function(slot) {
    const n = parseInt(slot.replace('pin',''));
    const isFirst = (n === 1);
    return {
      pin: 255,
      source: 0,
      addr: isFirst ? 64 : 32,
      channel: isFirst ? 0 : 255,
      invert: false
    };
  });
}

function setSlotValue(ch, slot, val) {
  const n = parseInt(slot.replace('pin',''));
  if (!ch.pins) {
    defaultChannelRoutes(ch);
  }
  ch.pins[n-1] = val;
}

function eachSlot(o,fn){
  var t=parseInt(o.type||0);
  var modeVal=parseInt(o.mc_mode||0);
  var mode=outputModeDef(t,modeVal);
  if(!mode) return;
  var pinDefs=mode.pins||{};
  Object.keys(pinDefs).sort((a,b)=>parseInt(a.replace('pin',''))-parseInt(b.replace('pin',''))).forEach(function(slot){
    var rule=pinDefs[slot];
    var val=getSlotValue(o,slot,t,modeVal);
    fn(slot,val.pin,val.source,rule);
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
const SOURCES=SOURCE_DATA?Object.fromEntries(SOURCE_DATA.names.map((n,i)=>[i,n])):{};
const GPIO_PINS_SAFE=GPIO_PINS||{output:[],input:[],inputOnly:[],reserved:[]};
const OUTPUT_GPIOS=GPIO_PINS_SAFE.output;
const INPUT_GPIOS=GPIO_PINS_SAFE.input;
const INPUT_ONLY_GPIOS=GPIO_PINS_SAFE.inputOnly;
const FORBIDDEN_OUTPUT_GPIOS=Object.fromEntries(GPIO_PINS_SAFE.reserved.map(r=>[r.pin,r.reason]));
const SOURCE_ADDRESS_RULES=SOURCE_DATA?Object.fromEntries(SOURCE_DATA.addressRules.map(r=>[r.source,{label:r.label,ranges:r.ranges}])):{};
const SRC_GPIO=SOURCE_DATA&&SOURCE_DATA.masks?SOURCE_DATA.masks.GPIO:1;
const SRC_PCA=SOURCE_DATA&&SOURCE_DATA.masks?SOURCE_DATA.masks.PCA:2;
const SRC_DIG=SOURCE_DATA&&SOURCE_DATA.masks?SOURCE_DATA.masks.DIGITAL_EXPANDER:4;
const SRC_DAC=SOURCE_DATA&&SOURCE_DATA.masks?SOURCE_DATA.masks.I2C_DAC:8;
var T=TYPE_META?.typeIds||{};

function sourceMaskForId(source){
  source=parseInt(source||0);
  if(source===0) return SRC_GPIO;
  if(source===1) return SRC_PCA;
  if(source>=2&&source<=4) return SRC_DIG;
  if(source>=5&&source<=7) return SRC_DAC;
  return 0;
}

function outputModeKey(type,mode){
  return TYPE_META.modeKeyMap[parseInt(type)]?.[parseInt(mode||0)]||String(mode);
}
function outputDef(type){ return (OUTPUT_DEFS||{})[parseInt(type)]; }
function outputModeDef(type,mode){
  const def=outputDef(type);
  if(!def) return null;
  return def.modes[outputModeKey(type,mode)]||def.modes.default||null;
}
function outputModeForObj(o){ return outputModeDef(typeId(o),parseInt(o.mc_mode||0)); }
function typeFields(type){ return (TYPE_META.fields&&TYPE_META.fields[String(parseInt(type))])||[]; }
function typeHasField(type,key){ return typeFields(type).some(function(field){return field.key===key;}); }
function outputTestUi(o){ return TYPE_META.testUi[typeId(o)]||0; }
function outputColorByteCount(o){ return parseInt(o.color_order||0)>=4?4:3; }
function costFlag(name){ return TYPE_META.costFlags?.[name]||0; }
function costHas(cost,name){ return !!((cost?.flags||0)&costFlag(name)); }
function outputHasCostFlag(o,name){ return costHas(outputModeForObj(o)?.cost,name); }
function outputPinRule(type,mode,slot){ return outputModeDef(type,mode)?.pins?.[slot]||null; }
function outputSegmentPinRule(type,mode,segmentIndex){
  const mk=outputModeKey(type,mode);
  const offset=(mk==='commonDim')?2:1;
  const slot='pin'+(parseInt(segmentIndex)+offset);
  return outputPinRule(type,mode,slot);
}
function outputUsesSegmentRoutes(type,mode){
  const def=outputModeDef(type,mode);
  return !!(def&&(def.segmentLayout||def.primaryRouteIsSegment));
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
  const bytesPerPixel=outputColorByteCount(o);
  const pixelsPerUniverse=Math.floor(512/bytesPerPixel);
  return Math.ceil((o.led_count!==undefined?o.led_count:170)/pixelsPerUniverse);
}

function outputChannelCount(o){
  const def=outputModeForObj(o);
  if(def?.cost?.dmxSlots===512) return '512 Ch';
  if(typeHasField(o.type,'led_count')) return `${o.led_count||170} LEDs / ${ledUniverseCount(o)}U`;
  if(outputTestUi(o)===5){
    const posBytes=valueByteCount(parseInt(o.mc_resolution||8));
    return `${posBytes+2} Ch (${posBytes} Pos + Speed + Cmd)`;
  }
  return TYPE_META.channelCounts[typeId(o)]||'1 Ch';
}

function outputByteCount(o){
  if(outputTestUi(o)===5) return valueByteCount(parseInt(o.mc_resolution||8));
  return TYPE_META.byteCounts[typeId(o)]??1;
}

function gpioPinCount(o){
  return outputGpios(o).length;
}

function outputGpios(o){
  var pins=[];
  eachSlot(o,function(slot,pin,src,rule){
    if(src===0&&pin!==255&&pins.indexOf(pin)===-1) pins.push(pin);
  });
  if(outputTestUi(o)===5&&parseInt(o.mc_homing_mode||0)>0&&parseInt(o.pin4_source||0)!==0){
    var p4=parseInt(o.pin4);
    if(!isNaN(p4)){
      var idx=pins.indexOf(p4);
      if(idx!==-1) pins.splice(idx,1);
    }
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

  const slotNumber=(slot)=>parseInt(slot.replace('pin',''));
  const fieldFor=(slot)=>{
    const n=slotNumber(slot);
    if(n===1) return {source:'no_source',pin:'no_pin',addr:'no_pca_addr',channel:'no_pca_channel',invert:'no_pin_invert'};
    return {source:`no_pin${n}_source`,pin:`no_pin${n}`,addr:`no_pin${n}_addr`,channel:`no_pin${n}_channel`,invert:`no_pin${n}_invert`};
  };

  const saved={};
  for (let n = 1; n <= 9; n++) {
    const field = fieldFor('pin' + n);
    [field.source, field.pin, field.addr, field.channel].forEach(id => {
      const el = document.getElementById(id);
      if (el) saved[id] = el.value;
    });
    const invEl = document.getElementById(field.invert);
    if (invEl) saved[field.invert] = invEl.checked;
  }

  const t=parseInt(document.getElementById('no_type')?.value||0);
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

  const slotActive=(slot)=>{
    const n=slotNumber(slot);
    const mask=mode?.slotActiveMask??0;
    if(!(mask&(1<<(n-1)))) return false;
    return true;
  };
  const sourceFor=(field,rule)=>{
    const current=parseInt(saved[field.source]??0);
    if(rule.sources&sourceMaskForId(current)) return current;
    const allowed=rule.sources;
    if(allowed&SRC_GPIO) return 0;
    if(allowed&SRC_PCA) return 1;
    if(allowed&SRC_DIG) return 2;
    if(allowed&SRC_DAC) return 5;
    return 0;
  };
  const pinDefs=Object.keys(mode.pins).sort((a,b)=>slotNumber(a)-slotNumber(b)).filter(slotActive).map((slot)=>{
    const rule=mode.pins[slot];
    const field=fieldFor(slot);
    const src=sourceFor(field,rule);
    const allowNone=slot!=='pin1';
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
        <input type="checkbox"${p.invId?' id="'+p.invId+'"':''}>
        <label${p.invId?' for="'+p.invId+'"':''} style="font-size:0.75rem;margin:0">Invert</label>
      </div>
    </div>`);

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
  const t=parseInt(document.getElementById('no_type')?.value||0);
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
  if(outputModeForObj(o)?.startAtFirstUniverse) return `U${o.start_universe}`;
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

function populateSystemPins() {
  var specialLabels = {};
  specialLabels[5] = 'GPIO 5 (LED3 on-board)';
  specialLabels[17] = 'GPIO 17 (LED4 on-board)';
  var outGpios = OUTPUT_GPIOS;
  var inGpios = INPUT_GPIOS;

  function fillSelect(id, list) {
    var sel = document.getElementById(id);
    if (!sel) return;
    var html = '<option value="255">Disabled</option>';
    list.forEach(function(p) {
      var label = 'GPIO ' + p;
      if (specialLabels[p]) label = specialLabels[p];
      html += '<option value="' + p + '">' + label + '</option>';
    });
    sel.innerHTML = html;
  }

  fillSelect('status_led_pin', outGpios);
  fillSelect('i2c_sda', outGpios);
  fillSelect('i2c_scl', outGpios);
  fillSelect('zc_pin', inGpios);
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
