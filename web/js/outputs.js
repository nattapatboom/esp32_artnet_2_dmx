// ==========================================
// === OUTPUT CHANNELS MANAGER ===
// ==========================================
let outputs = [];
let editOutIdx = -1;
let outputsDirty = false;
let savedOutputsJson = '';
let currentTestIdx = -1;
var OUTPUT_IDX = null;

function fieldDefaultValue(field){
  return field&&field.def!==undefined?field.def:0;
}
function renderGeneratedField(field){
  var id=field.key;
  if(field.type==='bool'){
    return `<div class="f" id="${id}_grp" style="flex:0 0 auto"><div class="cb"><input type="checkbox" id="${id}" ${fieldDefaultValue(field)?'checked':''}><label for="${id}" id="${id}_lbl">${field.label}</label></div></div>`;
  }
  if(field.type==='select'){
    var opts=(field.opts||[]).map(function(opt){return `<option value="${opt[0]}">${opt[1]}</option>`;}).join('');
    return `<div class="f" id="${id}_grp"><label for="${id}" id="${id}_lbl">${field.label}</label><select id="${id}">${opts}</select></div>`;
  }
  var inputType=field.type==='color'?'color':'number';
  var step=field.type==='float'?'any':'1';
  return `<div class="f" id="${id}_grp"><label for="${id}" id="${id}_lbl">${field.label}</label><input type="${inputType}" id="${id}" value="${fieldDefaultValue(field)}" min="${field.min}" max="${field.max}" step="${step}"></div>`;
}
function renderTypeConfig(t){
  var el=document.getElementById('type-config-generated');
  if(!el) return;
  var typeKey=String(parseInt(t));
  var preserve=el.dataset.type===typeKey;
  var saved={};
  if(preserve){
    Array.from(el.querySelectorAll('input,select')).forEach(function(input){
      saved[input.id]=input.type==='checkbox'?!!input.checked:input.value;
    });
  }
  var fields=(TYPE_META.fields&&TYPE_META.fields[typeKey])||[];
  el.innerHTML=fields.map(renderGeneratedField).join('');
  el.dataset.type=typeKey;
  Object.keys(saved).forEach(function(id){
    var input=document.getElementById(id);
    if(!input) return;
    if(input.type==='checkbox') input.checked=!!saved[id];
    else input.value=saved[id];
  });
  el.style.display=fields.length?'contents':'none';
}
function showTypeConfig(t) {
  document.querySelectorAll('.type-config').forEach(function(el) { el.style.display = 'none'; });
  renderTypeConfig(t);
}
function writeGeneratedFields(o){
  var fields=(TYPE_META.fields&&TYPE_META.fields[String(parseInt(o.type))])||[];
  fields.forEach(function(field){
    var el=document.getElementById(field.key);
    if(!el) return;
    var val=o[field.key];
    if(val===undefined) val=fieldDefaultValue(field);
    if(field.type==='bool') el.checked=!!val;
    else el.value=String(val);
  });
}
function readGeneratedFields(ch,type){
  var fields=(TYPE_META.fields&&TYPE_META.fields[String(parseInt(type))])||[];
  fields.forEach(function(field){
    var el=document.getElementById(field.key);
    if(!el){ ch[field.key]=fieldDefaultValue(field); return; }
    if(field.type==='bool') ch[field.key]=!!el.checked;
    else if(field.type==='float') ch[field.key]=parseFloat(el.value||fieldDefaultValue(field));
    else ch[field.key]=parseInt(el.value||fieldDefaultValue(field));
  });
}
function routeValue(id, fallback){
  var el=document.getElementById(id);
  return el?parseInt(el.value):fallback;
}
function routeChecked(id, fallback){
  var el=document.getElementById(id);
  return el?!!el.checked:!!fallback;
}
function setRouteValue(id, val){
  var el=document.getElementById(id);
  if(el) el.value=val;
}
function setRouteChecked(id, val){
  var el=document.getElementById(id);
  if(el) el.checked=!!val;
}
function routeKeysForSlot(slot){
  if(slot==='pin1') return {pin:'pin',source:'source',addr:'pca_addr',channel:'pca_channel',invert:'pin_invert'};
  var n=slot.replace('pin','');
  return {pin:'pin'+n,source:'pin'+n+'_source',addr:'pin'+n+'_addr',channel:'pin'+n+'_channel',invert:'pin'+n+'_invert'};
}
function routeDomForKeys(keys){
  if(keys.pin==='pin') return {pin:'no_pin',source:'no_source',addr:'no_pca_addr',channel:'no_pca_channel',invert:'no_pin_invert'};
  var n=keys.pin.replace('pin','');
  return {pin:'no_pin'+n,source:'no_pin'+n+'_source',addr:'no_pin'+n+'_addr',channel:'no_pin'+n+'_channel',invert:'no_pin'+n+'_invert'};
}
function defaultChannelRoutes(ch){
  ['pin','pin2','pin3','pin4'].forEach(function(k){ ch[k]=255; });
  ch.source=0;
  ch.pin2_source=0;
  ch.pin3_source=0;
  ch.pin4_source=0;
  ch.pca_addr=64;
  ch.pca_channel=0;
  ch.pin2_addr=32;
  ch.pin3_addr=32;
  ch.pin4_addr=32;
  ch.pin2_channel=255;
  ch.pin3_channel=255;
  ch.pin4_channel=255;
  ch.pin_invert=false;
  ch.pin2_invert=false;
  ch.pin3_invert=false;
  ch.pin4_invert=false;
  ch.seg_pins=[255,255,255,255,255,255,255,255];
  ch.seg_sources=[0,0,0,0,0,0,0,0];
  ch.seg_addrs=[32,32,32,32,32,32,32,32];
  ch.seg_channels=[255,255,255,255,255,255,255,255];
  ch.seg_inverts=0;
}
function writeRouteSlot(slot,o){
  var keys=routeKeysForSlot(slot);
  var ids=routeDomForKeys(keys);
  setRouteValue(ids.pin,o[keys.pin]??255);
  setRouteValue(ids.source,o[keys.source]??0);
  setRouteValue(ids.addr,parseInt(o[keys.addr]??(keys.addr==='pca_addr'?64:32)));
  setRouteValue(ids.channel,o[keys.channel]??(keys.channel==='pca_channel'?0:255));
  setRouteChecked(ids.invert,o[keys.invert]||false);
}
function writeRouteFields(o){
  var mode=outputModeDef(typeId(o),parseInt(o.mc_mode||0));
  Object.keys(mode?.pins||{}).forEach(function(slot){ writeRouteSlot(slot,o); });
  if(typeId(o)===T.STEPPER){
    setRouteChecked('no_pin_invert',o.mc_step_invert||false);
    setRouteChecked('no_pin2_invert',o.mc_dir_invert||false);
    setRouteChecked('no_pin3_invert',o.mc_enable_active_high||false);
  }
  var ssrc=o.seg_sources||[], sps=o.seg_pins||[], saddrs=o.seg_addrs||[], sch=o.seg_channels||[], sinvs=o.seg_inverts||0;
  for(var s=0;s<8;s++){
    setRouteValue('no_seg_source_'+s,ssrc[s]!==undefined?ssrc[s]:0);
    setRouteValue('no_seg_addr_'+s,saddrs[s]!==undefined?saddrs[s]:32);
    setRouteValue('no_seg_channel_'+s,sch[s]!==undefined?sch[s]:255);
    setRouteValue('no_seg_pin_'+s,sps[s]!==undefined?sps[s]:255);
    setRouteChecked('no_seg_pin_invert_'+s,!!((sinvs>>s)&1));
  }
}
function readRouteSlot(ch,slot){
  var keys=routeKeysForSlot(slot);
  var ids=routeDomForKeys(keys);
  ch[keys.pin]=routeValue(ids.pin,255);
  ch[keys.source]=routeValue(ids.source,0);
  ch[keys.addr]=routeValue(ids.addr,keys.addr==='pca_addr'?64:32);
  ch[keys.channel]=routeValue(ids.channel,keys.channel==='pca_channel'?0:255);
  ch[keys.invert]=routeChecked(ids.invert,false);
}
function readRouteFields(ch,type){
  var mode=outputModeDef(type,parseInt(cfgEl('mc_mode')?.value||0));
  defaultChannelRoutes(ch);
  Object.keys(mode?.pins||{}).forEach(function(slot){ readRouteSlot(ch,slot); });
  if(!mode?.segmentLayout) return;
  var commonDim=outputModeKey(type,parseInt(cfgEl('mc_mode')?.value||0))==='commonDim';
  var segCount=Object.keys(mode.pins||{}).length-(commonDim?1:0);
  for(var s=0;s<segCount;s++){
    if(s===0&&!commonDim){
      ch.seg_pins[s]=ch.pin;
      ch.seg_sources[s]=ch.source;
      ch.seg_addrs[s]=ch.pca_addr;
      ch.seg_channels[s]=ch.source!==0?ch.pca_channel:255;
      if(ch.pin_invert) ch.seg_inverts|=(1<<s);
    } else {
      ch.seg_pins[s]=routeValue('no_seg_pin_'+s,255);
      ch.seg_sources[s]=routeValue('no_seg_source_'+s,0);
      ch.seg_addrs[s]=routeValue('no_seg_addr_'+s,32);
      ch.seg_channels[s]=routeValue('no_seg_channel_'+s,255);
      if(routeChecked('no_seg_pin_invert_'+s,false)) ch.seg_inverts|=(1<<s);
    }
  }
}
function resetRouteFields(){
  var ch={type:parseInt(document.getElementById('no_type')?.value||0),mc_mode:parseInt(cfgEl('mc_mode')?.value||0)};
  defaultChannelRoutes(ch);
  writeRouteFields(ch);
}

function setSaveState(label,kind='ok'){
  const el=document.getElementById('out-save-state');
  if(!el) return;
  el.className='pill '+(kind==='warn'?'pill-warn':kind==='err'?'ib-red':'pill-ok');
  el.textContent=label;
}

function setOutputsDirty(dirty){
  outputsDirty=dirty;
  setSaveState(dirty?'Unsaved changes':'Saved',dirty?'warn':'ok');
}

function markOutputsSaved(){
  savedOutputsJson=JSON.stringify(outputs);
  setOutputsDirty(false);
}

async function loadOutputs(){
  try{
    const d=await(await fetch('/api/outputs')).json();
    if (d.version === 3) {
      outputs = d.outputs || [];
    } else {
      outputs = (d.outputs || []).map(migrateOutput);
    }
    markOutputsSaved();
    renderOutputs();
    autoAssignOutputPins();
  }catch(e){}
}

function renderOutputs(){
  const tb=document.getElementById('out-tbody');
  const cnt=document.getElementById('out-count');
  if(cnt) cnt.textContent=`${outputs.length} channels`;
  updateScoreBar();
  const zcWarn=document.getElementById('zc-warning');
  if(zcWarn){
    const hasDimmer=outputs.some(o=>typeId(o)===0);
    const zcPin=parseInt(document.getElementById('zc_pin')?.value||'255');
    zcWarn.style.display=hasDimmer&&zcPin===255?'':'none';
  }
  checkStrappingPin();
  tb.innerHTML='';
  if(!outputs.length){tb.innerHTML='<tr><td colspan="8" style="text-align:center;color:#94a3b8;padding:12px">No channels configured.</td></tr>';return;}
  outputs.forEach((o,i)=>{
    const tr=document.createElement('tr');
    if(i===currentTestIdx) tr.classList.add('test-row');
    if(outputsDirty) tr.classList.add('unsaved-row');
    const removed=typeId(o)<0||typeId(o)>18;
    const h=channelHardware(o);
    const cc=channelCost(o);
    const hwStr='L'+(h.ledc||'')+' R'+(h.rmt||'')+' U'+(h.uart||'')+(h.dac?' D':'')+(h.timer?' T'+h.timer:'');
    tr.innerHTML=`<td>${i+1}</td><td>${deviceLabel(o)}</td><td>${gpioLabel(o)}</td><td>${outputAddressLabel(o)}</td>
      <td>${outputChannelCount(o)}</td><td><span class="pill pill-ok" style="font-size:0.65rem">${hwStr} ${cc.cpu}\u00B5s</span></td>
      <td>${i===currentTestIdx?'<span class="pill pill-warn" style="margin-right:4px">TEST</span>':''}${removed?'<span class="pill pill-warn" style="margin-right:4px">DELETE</span>':`<button class="btn bg" style="padding:3px 8px;font-size:0.72rem;margin-right:4px" onclick="showOutputTest(${i})">Test</button>
          <button class="btn bs" style="padding:3px 8px;font-size:0.72rem;margin-right:4px" onclick="editOutput(${i})">Edit</button>`}
          <button class="btn bd" style="padding:3px 8px;font-size:0.72rem" onclick="delOutput(${i})">Delete</button></td>`;
    tb.appendChild(tr);
  });
}

function setTestState(active,label=''){
  const el=document.getElementById('out-test-state');
  const pill=document.getElementById('test-mode-pill');
  if(el){
    el.className='ib '+(active?'ib-red':'ib-blue');
    el.textContent=active?`TEST MODE ACTIVE: ${label}. Output is being overridden. Press Stop Test to return to normal mode.`:'Normal output mode. Test tools are idle.';
  }
  if(pill){
    pill.className='pill '+(active?'pill-warn':'pill-ok');
    pill.textContent=active?'TEST ACTIVE':'Idle';
  }
}
async function sendOutputTest(idx,values,durationMs=30000){
  try{
    const res=await fetch('/api/output-test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:idx,values:values,duration_ms:durationMs})});
    if(res.ok){
      currentTestIdx=idx;
      setTestState(true,`#${idx+1} ${deviceLabel(outputs[idx]||{})}`);
      renderOutputs();
      clearTimeout(window.outputTestTimer);
      window.outputTestTimer=setTimeout(()=>{currentTestIdx=-1;setTestState(false);renderOutputs();},durationMs+500);
    }
    showAlert(res.ok);
  }catch(e){showAlert(false);}
}
async function stopOutputTest(){
  try{
    const res=await fetch('/api/output-test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({off:true})});
    if(res.ok){
      currentTestIdx=-1;
      clearTimeout(window.outputTestTimer);
      setTestState(false);
      const p=document.getElementById('out-test-panel');
      if(p) p.style.display='none';
      renderOutputs();
    }
    showAlert(res.ok);
  }catch(e){showAlert(false);}
}
const TEST_UI_FIELDS={
  1:[{key:'test_level_num',type:'number',label:'Value',min:0,max:255,def:128}],
  2:[{key:'test_dmx_ch',type:'number',label:'DMX Channel',min:1,max:512,def:1},{key:'test_level_num',type:'number',label:'Value',min:0,max:255,def:128}],
  3:[{key:'test_color',type:'color',label:'Custom Color',def:'#ff0000'},{key:'test_white',type:'number',label:'White',min:0,max:255,def:0},{key:'test_pixel',type:'number',label:'Pixel Number',min:1,max:1360,def:1}],
  4:[{key:'test_motor_num',type:'number',label:'Speed',min:0,max:255,def:128}],
  5:[{key:'test_step_pos',type:'number',label:'Position',min:0,max:4294967295,def:128},{key:'test_step_speed',type:'number',label:'Speed',min:0,max:255,def:180}],
  6:[{key:'test_mp3_track',type:'number',label:'Track',min:0,max:255,def:1},{key:'test_mp3_vol',type:'number',label:'Volume',min:0,max:255,def:200}],
  7:[{key:'test_7seg_num',type:'number',label:'Number',min:0,max:9999,def:1234},{key:'test_7seg_text',type:'text',label:'ASCII Text',min:0,max:4,def:'ABCD'}]
};
function renderTestField(field){
  var value=field.def!==undefined?field.def:'';
  if(field.type==='color') return `<div class="f"><label for="${field.key}">${field.label}</label><input type="color" id="${field.key}" value="${value}"></div>`;
  if(field.type==='text') return `<div class="f"><label for="${field.key}">${field.label}</label><input type="text" id="${field.key}" value="${value}" maxlength="${field.max||32}"></div>`;
  return `<div class="f"><label for="${field.key}">${field.label}</label><input type="number" id="${field.key}" value="${value}" min="${field.min}" max="${field.max}"></div>`;
}
function renderTestFields(ui){
  var box=document.getElementById('test-field-container');
  if(!box) return;
  box.innerHTML=(TEST_UI_FIELDS[ui]||[]).map(renderTestField).join('');
}
function testValue(id,fallback){
  var el=document.getElementById(id);
  if(!el) return fallback;
  return el.type==='text'||el.type==='color'?el.value:parseInt(el.value||fallback);
}
function encodeTestValues(o,ui,cmd){
  if(ui===1){
    var max=maxDmxFor(o), v=Math.max(0,Math.min(max,parseInt(testValue('test_level_num',128))||0));
    return valueBytes(v,parseInt(o.mc_resolution||8));
  }
  if(ui===2){
    var ch=Math.max(1,Math.min(512,parseInt(testValue('test_dmx_ch',1))||1));
    var dmxVal=Math.max(0,Math.min(255,parseInt(testValue('test_level_num',128))||0));
    var vals=new Array(512).fill(0); vals[ch-1]=dmxVal; return vals;
  }
  if(ui===3){
    var hex=String(testValue('test_color','#ff0000'));
    var r=parseInt(hex.slice(1,3),16), g=parseInt(hex.slice(3,5),16), b=parseInt(hex.slice(5,7),16), w=parseInt(testValue('test_white',0))||0;
    if(o.type===T.ANALOG_RGB) return (parseInt(o.color_order||0)>=4)?[r,g,b,w]:[r,g,b];
    var count=o.led_count||170, out=[], target=Math.max(1,Math.min(count,parseInt(testValue('test_pixel',1))||1))-1;
    for(var i=0;i<count;i++){
      var on=(cmd||'all')!=='pixel'||i===target;
      out.push(on?r:0,on?g:0,on?b:0);
      if((o.color_order||0)>=4) out.push(on?w:0);
    }
    return out;
  }
  if(ui===4){
    var res=parseInt(o.mc_resolution||8), maxMotor=maxDmxFor(o), speed=Math.max(0,Math.min(255,parseInt(testValue('test_motor_num',128))||0));
    var center=Math.floor(maxMotor/2), dir=parseInt(cmd||0);
    var mv=dir===0?center:(dir>0?Math.round(center+(speed/255)*(maxMotor-center)):Math.round(center-(speed/255)*center));
    return valueBytes(mv,res);
  }
  if(ui===5){
    var stepRes=parseInt(o.mc_resolution||8), maxStep=maxDmxFor(o), pos=Math.max(0,Math.min(maxStep,Number(testValue('test_step_pos',128))||0));
    var stepVals=valueBytes(pos,stepRes);
    stepVals.push(Math.max(0,Math.min(255,parseInt(testValue('test_step_speed',180))||0)),cmd);
    return stepVals;
  }
  if(ui===6){
    var track=parseInt(testValue('test_mp3_track',1))||1, vol=parseInt(testValue('test_mp3_vol',200))||200;
    return parseInt(cmd)===0?[track,vol,0]:parseInt(cmd)===1?[0,vol,0]:parseInt(cmd)===2?[track,vol,2]:parseInt(cmd)===3?[track,vol,1]:parseInt(cmd)===4?[track,vol,2]:[cmd||0,vol,0];
  }
  if(ui===7){
    if(parseInt(o.mc_mode||0)===1){
      return String(testValue('test_7seg_text','')).padEnd(4,' ').slice(0,4).split('').map(function(c){return c.charCodeAt(0)&255;});
    }
    var n=Math.max(0,Math.min(9999,parseInt(testValue('test_7seg_num',1234))||0));
    return [(n>>8)&255,n&255];
  }
  return [cmd];
}
function showOutputTest(idx){
  const o=outputs[idx];
  const t=parseInt(o.type);
  const p=document.getElementById('out-test-panel');
  document.getElementById('test-title').textContent='#'+(idx+1)+' '+deviceLabel(o);
  OUTPUT_IDX = idx;
  const ui=TYPE_META.testUi[t];
  renderTestFields(ui);
  const cmds=TYPE_META.testCmds[t];
  const btnContainer=document.getElementById('test-buttons');
  btnContainer.innerHTML='';
  if(cmds&&cmds.length){
    cmds.forEach(cmd=>{
      const btn=document.createElement('button');
      btn.type='button';
      btn.className='btn bt';
      btn.textContent=cmd.label;
      btn.onclick=function(){sendTestCmd(idx,cmd.value);};
      btnContainer.appendChild(btn);
    });
  }
  p.style.display='block';
  p.scrollIntoView({behavior:'smooth',block:'nearest'});
}
function sendTestCmd(idx,val){
  const o=outputs[idx];
  const t=parseInt(o.type);
  const ui=TYPE_META.testUi[t];
  const dur=testDur();
  sendOutputTest(idx,encodeTestValues(o,ui,val),dur);
}
function anyStopTest(idx){
  sendOutputTest(parseInt(idx), [], 1000);
}
function calcRcCutoff(){
  const r=parseFloat(cfgEl('rc_r')?.value);
  const c=parseFloat(cfgEl('rc_c')?.value);
  const fcEl=cfgEl('rc_fc');
  if(!r||!c||r<1||c<1){fcEl&&(fcEl.textContent='\u2014');return;}
  const fc=1/(2*Math.PI*r*c*1e-9);
  fcEl&&(fcEl.textContent=fc>=1000?fc.toFixed(0):fc.toFixed(1));
}
function testDur(){return (parseInt(document.getElementById('test_duration')?.value)||30)*1000;}

function toggleOutFields(){
  const t=parseInt(document.getElementById('no_type').value);
  showTypeConfig(t);
  const mcMode = parseInt(cfgEl('mc_mode')?.value||0);
  // Helper: safe style access; no-op if element missing
  const _st=(id,v)=>{const e=cfgEl(id);if(e)e.style.display=v;};
  if(!document.getElementById('no_source')) renderPinRows();
  const srcSelect=document.getElementById('no_source');
  
  const isDmx = t===T.DMX;
  const isRelay = t===T.RELAY;
  const isDimmer = t===T.DIMMER;
  const isMc = t>=T.SINGLE_LED && t<=T.SERVO;
  const isPwmDimmer = t===T.SINGLE_LED;
  const isMotor = t===T.MOTOR;
  const isStepper = t===T.STEPPER;
  const isServo = t===T.SERVO;
  const isSolenoid = t===T.SOLENOID;
  const isAnalogRgb = t===T.ANALOG_RGB;
  const isBuzzer = t===T.BUZZER;
  const isSmoke = t===T.SMOKE;
  const is7Seg = t>=T.TM1637 && t<=T['7SEG_8PIN'];
  const isDfPlayer = t===T.DFPLAYER;
  const isPwmDac = t===T.PWM_DAC;
  const isFuncGen = t===T.FUNC_GEN;
  const isDac = t===T.DAC;
  const isCommonDim = is7Seg && mcMode >= 6 && mcMode <= 9;
  const is7SegDD = is7Seg && mcMode >= 2 && mcMode <= 9;
  const is7SegDirectDim = is7Seg && (mcMode === 4 || mcMode === 5);

  const canUsePca = modeAllowsSource(t,mcMode,SRC_PCA);
  const canUseDigitalExpander = modeAllowsSource(t,mcMode,SRC_DIG);
  const canUseDac = modeAllowsSource(t,mcMode,SRC_DAC);
  const isGpioOnly = modeIsGpioOnly(t,mcMode);
  
  if (srcSelect && srcSelect.tagName === 'SELECT') {
    [...srcSelect.options].forEach(opt=>{
      const v=parseInt(opt.value);
      opt.disabled=(v===1&&!canUsePca)||(v>=2&&v<=4&&!canUseDigitalExpander)||(v>=5&&v<=7&&!canUseDac);
    });
    if (srcSelect.options[srcSelect.selectedIndex]?.disabled) srcSelect.value = 0;
    if ((!canUsePca && !canUseDigitalExpander && !canUseDac) || isGpioOnly) {
      srcSelect.value = 0;
      srcSelect.disabled = true;
    } else {
      srcSelect.disabled = false;
    }
  }
  
  const src = srcSelect ? parseInt(srcSelect.value||0) : 0;
  const isPca = src===1;
  const isExpander = src!==0;
  const addrSel=document.getElementById('no_pca_addr');
  if(addrSel){
    populateExpanderAddresses(addrSel, isExpander ? src : undefined);
    if(isPca && !addrSel.querySelector('option[value="'+addrSel.value+'"]')) addrSel.value=64;
    if(src===5 && !addrSel.querySelector('option[value="'+addrSel.value+'"]')) addrSel.value=96;
    if(src===6 && !addrSel.querySelector('option[value="'+addrSel.value+'"]')) addrSel.value=76;
    if(src===7 && !addrSel.querySelector('option[value="'+addrSel.value+'"]')) addrSel.value=76;
    if(src>=2 && !addrSel.querySelector('option[value="'+addrSel.value+'"]')) addrSel.value=32;
  }
  
  _st('no_addr_grp',(isDmx||t===T.LED_STRIP)?'none':'');
  _st('no_mc_grp',(isMc||is7Seg||isFuncGen||isPwmDac)?'':'none');
  _st('no_sol_grp',isSolenoid?'':'none');
  _st('no_smoke_grp',isSmoke?'':'none');
  if(isDfPlayer) renderPinRows();
  
  const hMode = parseInt(cfgEl('mc_homing_mode')?.value||0);
  const colorOrder = parseInt(cfgEl('color_order')?.value||0);
  _st('no_pca_channel2_grp','none');
  _st('no_pca_channel3_grp','none');
  _st('no_pca_channel4_grp','none');

  if (isMc || is7Seg || isFuncGen || isPwmDac) {
    _st('mc_mode_grp',(is7Seg || isMotor) ? '' : 'none');
    _st('mc_res_grp',(isMc || isPwmDac || is7Seg) ? '' : 'none');
    const resLbl = cfgEl('mc_resolution_lbl');
    if(resLbl) resLbl.textContent = is7Seg ? 'Decode Mode' : 'Resolution';
    const freqGrp = cfgEl('mc_freq_grp');
    const freqLbl = cfgEl('mc_freq_lbl');
    const pwmDacCalGrp = cfgEl('pwm_dac_cal_grp');
    if (isFuncGen || isPwmDac) {
      if(freqGrp) freqGrp.style.display = '';
      if(freqLbl) freqLbl.textContent = "PWM Carrier Frequency (Hz)";
      _st('rc_filter_grp','');
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = isPwmDac ? '' : 'none';
      calcRcCutoff();
    } else if (is7Seg) {
      if(freqGrp) freqGrp.style.display = (t >= 12) ? '' : 'none';
      if(freqLbl) freqLbl.textContent = "PWM Frequency (Hz)";
      _st('rc_filter_grp','none');
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else if (isPwmDimmer || isMotor) {
      if(freqGrp) freqGrp.style.display = '';
      if(freqLbl) freqLbl.textContent = "Frequency (Hz)";
      _st('rc_filter_grp','none');
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else if (isStepper) {
      if(freqGrp) freqGrp.style.display = '';
      if(freqLbl) freqLbl.textContent = "Speed (Hz)";
      _st('rc_filter_grp','none');
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else if (isAnalogRgb) {
      if(freqGrp) freqGrp.style.display = '';
      if(freqLbl) freqLbl.textContent = "Frequency (Hz)";
      _st('rc_filter_grp','none');
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else {
      if(freqGrp) freqGrp.style.display = 'none';
      _st('rc_filter_grp','none');
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    }
    
    _st('mc_deadband_grp',isMotor ? '' : 'none');
    _st('mc_min_us_grp',isServo ? '' : 'none');
    _st('mc_max_us_grp',isServo ? '' : 'none');
    _st('mc_stepper_scale_grp',isStepper ? '' : 'none');
    _st('mc_steps_grp',isStepper ? '' : 'none');
    _st('mc_unit_type_grp',isStepper ? '' : 'none');
    _st('mc_scale_factor_grp',isStepper ? '' : 'none');
    _st('mc_stepper_homing_grp',isStepper ? '' : 'none');
    _st('mc_homing_mode_grp',isStepper ? '' : 'none');
    _st('mc_homing_dir_grp',isStepper ? '' : 'none');
    _st('mc_homing_speed_grp',isStepper ? '' : 'none');
    _st('mc_homing_timeout_grp',(isStepper && hMode === 1) ? '' : 'none');
    _st('mc_invert_grp',(isMotor || isStepper) ? '' : 'none');
    _st('mc_brake_grp',(isMotor && (mcMode === 0 || mcMode === 2)) ? '' : 'none');
  }
  renderPinRows();
  autoAssignOutputPins();
  const pinMapContainer = document.getElementById('pin-mapping-container');
  if(pinMapContainer) pinMapContainer.style.display = '';
}

document.addEventListener('change', function(e){
  var id=e.target&&e.target.id;
  if(id==='mc_mode'||id==='mc_homing_mode'||id==='color_order') toggleOutFields();
});
document.getElementById('status_led_pin').addEventListener('change', autoAssignOutputPins);
document.getElementById('zc_pin').addEventListener('change', autoAssignOutputPins);

function editOutput(idx){
  editOutIdx=idx;
  const o=outputs[idx];
  if(typeId(o)<0||typeId(o)>18){
    alert('This output type is no longer supported. Please delete and recreate this channel.');
    return;
  }
  document.getElementById('no_type').value=o.type;
  toggleOutFields();
  
  function setVal(id,val){const el=cfgEl(id);if(el)el.value=val;}
  function setChk(id,val){const el=cfgEl(id);if(el)el.checked=val;}
  
  setVal('no_uni',o.start_universe);
  setVal('no_addr',o.start_address||1);
  writeGeneratedFields(o);
  writeRouteFields(o);
  toggleOutFields();
  writeRouteFields(o);
  
  document.getElementById('out-add-btn').textContent='Update Channel';
  document.getElementById('out-cancel-btn').style.display='';
  document.getElementById('edit-idx-label').textContent=idx+1;
  document.getElementById('edit-banner').style.display='';
  document.getElementById('no_type').scrollIntoView({behavior:'smooth',block:'start'});
}

function cancelEditOutput(){
  editOutIdx=-1;
  const setVal=(id,val)=>{const el=document.getElementById(id);if(el)el.value=val;};
  resetRouteFields();
  setVal('pwm_dac_mode',0);
  setVal('pwm_dac_min','0.00');
  setVal('pwm_dac_max','100.00');
  setVal('sol_threshold',127);
  if(document.getElementById('smoke_threshold')) setVal('smoke_threshold',127);
  document.getElementById('out-add-btn').textContent='Add Channel';
  document.getElementById('out-cancel-btn').style.display='none';
  document.getElementById('edit-banner').style.display='none';
  autoAssignOutputPins();
}

function addOrUpdateOutput(){
  const type=parseInt(document.getElementById('no_type').value);
  const ch={
    type:type,
    start_universe:parseInt(document.getElementById('no_uni').value),
    start_address:parseInt(document.getElementById('no_addr').value)||1,
    led_count:parseInt(cfgEl('led_count')?.value||170),
    color_order:parseInt(cfgEl('color_order')?.value||0),
    led_protocol:parseInt(cfgEl('led_protocol')?.value||0),
    mc_mode: parseInt(cfgEl('mc_mode')?.value||0),
    mc_resolution: parseInt(cfgEl('mc_resolution')?.value||8),
    mc_freq: parseInt(cfgEl('mc_freq')?.value||1000),
    pwm_dac_mode: parseInt(document.getElementById('pwm_dac_mode')?.value || 0),
    pwm_dac_min: dutyPctToInt('pwm_dac_min', 0),
    pwm_dac_max: dutyPctToInt('pwm_dac_max', 10000),
    mc_deadband: parseInt(cfgEl('mc_deadband')?.value||0),
    mc_min_us: parseInt(cfgEl('mc_min_us')?.value||1000),
    mc_max_us: parseInt(cfgEl('mc_max_us')?.value||2000),
    mc_steps_per_rev: parseInt(cfgEl('mc_steps_per_rev')?.value||200),
    mc_unit_type: parseInt(cfgEl('mc_unit_type')?.value||0),
    mc_scale_factor: parseFloat(cfgEl('mc_scale_factor')?.value||'0'),
    mc_invert: cfgEl('mc_invert')?.checked||false,
    mc_brake: cfgEl('mc_brake')?.checked||false,
    mc_enable_active_high: false,
    mc_dir_invert: false,
    mc_step_invert: false,
    mc_homing_mode: parseInt(cfgEl('mc_homing_mode')?.value||0),
    mc_homing_dir: parseInt(cfgEl('mc_homing_dir')?.value||0),
    mc_homing_speed: parseInt(cfgEl('mc_homing_speed')?.value||500),
    mc_homing_timeout: parseInt(cfgEl('mc_homing_timeout')?.value||5),
    solenoid_mode: 0,
    solenoid_threshold: (type===18) ? parseInt(document.getElementById('smoke_threshold')?.value || 127) : parseInt(document.getElementById('sol_threshold')?.value || 127),
    solenoid_pulse_ms: parseInt(document.getElementById('sol_pulse_ms')?.value || 50),
    solenoid_pre_delay: parseInt(document.getElementById('sol_pre_delay')?.value || 0),
    solenoid_post_delay: parseInt(document.getElementById('sol_post_delay')?.value || 100),
    
    smoke_duration_ms: parseInt(document.getElementById('smoke_dur')?.value || 1000),
    settle_delay_ms: parseInt(document.getElementById('smoke_settle')?.value || 500),
    shoot_duration_ms: parseInt(document.getElementById('smoke_shoot')?.value || 1000),
    smoke_lockout_ms: parseInt(document.getElementById('smoke_lockout')?.value || 2000)
  };
  readRouteFields(ch,type);
  if(type===T.STEPPER){
    ch.pin_invert=false;
    ch.pin2_invert=false;
    ch.pin3_invert=false;
    ch.mc_enable_active_high=routeChecked('no_pin3_invert',false);
    ch.mc_dir_invert=routeChecked('no_pin2_invert',false);
    ch.mc_step_invert=routeChecked('no_pin_invert',false);
  }
  readGeneratedFields(ch,type);

  if(editOutIdx>=0){outputs[editOutIdx]=ch;cancelEditOutput();}
  else {outputs.push(ch); autoAssignOutputPins();}
  setOutputsDirty(true);
  renderOutputs();
}

function delOutput(idx){
  if(!confirm('Delete channel '+(idx+1)+'?')) return;
  if(editOutIdx===idx) cancelEditOutput();
  outputs.splice(idx,1);
  if(currentTestIdx===idx) currentTestIdx=-1;
  else if(currentTestIdx>idx) currentTestIdx--;
  setOutputsDirty(true);
  renderOutputs();
}

async function saveOutputs(){
  try{
    setSaveState('Saving...','warn');
    const res=await fetch('/api/outputs',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({version:3,outputs})});
    let msg='';
    try{const d=await res.json(); msg=d.message||'';}catch(e){}
    if(!res.ok&&msg) {
      setSaveState('Save failed','err');
      alert(msg);
    }
    if(res.ok){
      markOutputsSaved();
      setSaveState('Saved - rebooting','warn');
      const state=document.getElementById('out-test-state');
      if(state){
        state.className='ib ib-yellow';
        state.textContent='Outputs saved. Device is rebooting. This page will reload automatically.';
      }
      startRebootCountdown('Saving output channels and restarting...', 9);
    }
    showAlert(res.ok);
  }catch(e){
    setSaveState('Reconnecting...','warn');
    startRebootCountdown('Reconnecting and reloading...', 9);
  }
}
