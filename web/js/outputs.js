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
  if(!el) return fallback;
  var v=parseInt(el.value);
  return isNaN(v)?fallback:v;
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
function getDomIdsForSlot(slot) {
  const n = parseInt(slot.replace('pin',''));
  if (n === 1) {
    return { pin: 'no_pin', source: 'no_source', addr: 'no_pca_addr', channel: 'no_pca_channel', invert: 'no_pin_invert' };
  } else {
    return {
      pin: `no_pin${n}`,
      source: `no_pin${n}_source`,
      addr: `no_pin${n}_addr`,
      channel: `no_pin${n}_channel`,
      invert: `no_pin${n}_invert`
    };
  }
}

function writeRouteFields(o) {
  const t = typeId(o);
  const modeVal = parseInt(o.mc_mode || 0);
  const mode = outputModeDef(t, modeVal);
  if (!mode) return;

  Object.keys(mode.pins || {}).forEach(function(slot) {
    const ids = getDomIdsForSlot(slot);
    const val = getSlotValue(o, slot);
    setRouteValue(ids.pin, val.pin);
    setRouteValue(ids.source, val.source);
    setRouteValue(ids.addr, val.addr);
    setRouteValue(ids.channel, val.channel);
    setRouteChecked(ids.invert, val.invert);
  });
}

function readRouteFields(ch, type) {
  const modeVal = parseInt(cfgEl('mc_mode')?.value || 0);
  const mode = outputModeDef(type, modeVal);
  defaultChannelRoutes(ch);
  if (!mode) return;

  Object.keys(mode.pins || {}).forEach(function(slot) {
    const ids = getDomIdsForSlot(slot);
    const val = {
      pin: routeValue(ids.pin, 255),
      source: routeValue(ids.source, 0),
      addr: routeValue(ids.addr, 32),
      channel: routeValue(ids.channel, 255),
      invert: routeChecked(ids.invert, false)
    };
    setSlotValue(ch, slot, val);
  });
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
    outputs = d.outputs || [];
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
async function sendOutputTest(idx,cmd,params,durationMs=30000){
  try{
    const res=await fetch('/api/output-test',{
      method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({
        index: idx,
        cmd: cmd,
        params: params,
        duration_ms: durationMs
      })
    });
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
function renderTestField(field){
  var value=field.def!==undefined?field.def:'';
  if(field.type==='color') return `<div class="f"><label for="${field.key}">${field.label}</label><input type="color" id="${field.key}" value="${value}"></div>`;
  if(field.type==='text') return `<div class="f"><label for="${field.key}">${field.label}</label><input type="text" id="${field.key}" value="${value}" maxlength="${field.max||32}"></div>`;
  return `<div class="f"><label for="${field.key}">${field.label}</label><input type="number" id="${field.key}" value="${value}" min="${field.min}" max="${field.max}"></div>`;
}
function testValue(id,fallback){
  var el=document.getElementById(id);
  if(!el) return fallback;
  return el.type==='text'||el.type==='color'?el.value:parseInt(el.value||fallback);
}
function showOutputTest(idx){
  const o=outputs[idx];
  if(!o) return;
  const t=parseInt(o.type);
  const p=document.getElementById('out-test-panel');
  const titleEl=document.getElementById('test-title');
  if(titleEl) titleEl.textContent='#'+(idx+1)+' '+deviceLabel(o);
  OUTPUT_IDX = idx;

  var box=document.getElementById('test-field-container');
  if(box) {
    box.innerHTML=(TYPE_META.testFields?.[t]||[]).map(renderTestField).join('');
  }

  const cmds=TYPE_META.testCmds[t];
  const btnContainer=document.getElementById('test-buttons');
  if(!btnContainer) return;
  btnContainer.innerHTML='';
  if(cmds&&cmds.length){
    cmds.forEach(cmd=>{
      const btn=document.createElement('button');
      btn.type='button';
      btn.className='btn bt';
      btn.textContent=cmd[0];
      btn.title=cmd[2]||'';
      btn.onclick=function(){sendTestCmd(idx,cmd[1]);};
      btnContainer.appendChild(btn);
    });
  }
  if(p){ p.style.display='block'; p.scrollIntoView({behavior:'smooth',block:'nearest'}); }
}
function sendTestCmd(idx,cmdVal){
  const o=outputs[idx];
  const t=parseInt(o.type);
  const dur=testDur();
  const params={};
  (TYPE_META.testFields?.[t]||[]).forEach(field=>{
    if(field.type==='color'){
      const hex=String(testValue(field.key,'#ff0000'));
      params.r=parseInt(hex.slice(1,3),16);
      params.g=parseInt(hex.slice(3,5),16);
      params.b=parseInt(hex.slice(5,7),16);
    } else {
      params[field.key]=testValue(field.key, field.def);
    }
  });
  sendOutputTest(idx,cmdVal,params,dur);
}
function anyStopTest(idx){
  stopOutputTest();
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
  const t=parseInt(document.getElementById('no_type')?.value||0);
  showTypeConfig(t);
  const mcMode = parseInt(cfgEl('mc_mode')?.value||0);
  const _st=(id,v)=>{const e=cfgEl(id);if(e)e.style.display=v;};
  _st('no_addr_grp',outputModeDef(t,mcMode)?.startAtFirstUniverse?'none':'');
  calcRcCutoff();
  renderPinRows();
  autoAssignOutputPins();
  const pinMapContainer = document.getElementById('pin-mapping-container');
  if(pinMapContainer) pinMapContainer.style.display = '';
}

document.addEventListener('change', function(e){
  var id=e.target&&e.target.id;
  if(id==='mc_mode'||id==='mc_homing_mode'||id==='color_order') toggleOutFields();
});
document.getElementById('status_led_pin')?.addEventListener('change', autoAssignOutputPins);
document.getElementById('zc_pin')?.addEventListener('change', autoAssignOutputPins);

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
  
  document.getElementById('out-add-btn').textContent='Update Channel';
  document.getElementById('out-cancel-btn').style.display='';
  document.getElementById('edit-idx-label').textContent=idx+1;
  document.getElementById('edit-banner').style.display='';
  document.getElementById('no_type').scrollIntoView({behavior:'smooth',block:'start'});
}

function cancelEditOutput(){
  editOutIdx=-1;
  resetRouteFields();
  writeGeneratedFields({type:parseInt(document.getElementById('no_type')?.value||0)});
  document.getElementById('out-add-btn').textContent='Add Channel';
  document.getElementById('out-cancel-btn').style.display='none';
  document.getElementById('edit-banner').style.display='none';
  autoAssignOutputPins();
}

function addOrUpdateOutput(){
  const type=parseInt(document.getElementById('no_type')?.value||0);
  const ch={
    type:type,
    start_universe:parseInt(document.getElementById('no_uni')?.value)||0,
    start_address:parseInt(document.getElementById('no_addr').value)||1
  };
  readGeneratedFields(ch,type);
  readRouteFields(ch,type);

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

function channelHardware(o) {
  const def = outputModeForObj(o);
  const cost = def?.cost || {};
  let ledc = 0;
  if (cost.flags) {
    const isBgDimmer = costHas(cost, 'CF_BG_DIMMER');
    const isAggDmx = costHas(cost, 'CF_AGG_DMX');
    if (!isBgDimmer && !isAggDmx) {
      const pinSlots = def ? Object.keys(def.pins || {}) : ['pin1', 'pin2', 'pin3', 'pin4'];
      pinSlots.forEach((slot, idx) => {
        const routeVal = o.pins?.[idx];
        if (routeVal && routeVal.source === 0 && routeVal.pin !== 255) {
          ledc++;
        }
      });
    }
  }
  return {
    ledc: ledc,
    rmt: cost.rmt || 0,
    uart: cost.uart || 0,
    dac: cost.dac || 0,
    timer: cost.timer || 0
  };
}

function channelCost(o) {
  const def = outputModeForObj(o);
  const cost = def?.cost || {};
  return {
    cpu: cost.cpuUs || 0,
    ram: cost.ramBytes || 0
  };
}

