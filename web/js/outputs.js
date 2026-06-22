// ==========================================
// === OUTPUT CHANNELS MANAGER ===
// ==========================================
let outputs = [];
let editOutIdx = -1;
let outputsDirty = false;
let savedOutputsJson = '';
let currentTestIdx = -1;

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
function showOutputTest(idx){
  const o=outputs[idx];
  const p=document.getElementById('out-test-panel');
  const c=document.getElementById('test-controls');
  document.getElementById('test-title').textContent=`#${idx+1} ${deviceLabel(o)}`;
  p.style.display='block';
  const dur=`<div class="f"><label for="test_duration">Duration (seconds)</label><input type="number" id="test_duration" value="30" min="1" max="300"></div>`;
  const runBtn=(label,js,cls='bp')=>`<button type="button" class="btn ${cls}" onclick="${js}">${label}</button>`;
  let html='';
  if(o.type===3||o.type===5){
    const rgbw=(o.color_order||0)>=4;
    const isLed = o.type===3;
    html=`<div class="fg">${dur}<div class="f"><label for="test_color">Custom Color</label><input type="color" id="test_color" value="#ff0000"></div>
      <div class="f" style="display:${rgbw?'':'none'}"><label for="test_white">White</label><input type="number" id="test_white" value="0" min="0" max="255"></div>
      ${isLed ? `<div class="f"><label for="test_pixel">Single Pixel Number</label><input type="number" id="test_pixel" value="1" min="1" max="${o.led_count||170}"><div class="hint">Use one lit pixel to count LEDs. Pixel numbering starts at 1.</div></div>` : ''}</div>
      <div class="btns">${runBtn(isLed ? 'All Custom' : 'Apply Color',isLed ? `testLed(${idx},'all')` : `testLed(${idx})`)}
      ${runBtn(isLed ? 'All Red' : 'Red',isLed ? `testLedPreset(${idx},255,0,0,'all')` : `testLedPreset(${idx},255,0,0)`,'bd')}
      ${runBtn(isLed ? 'All Green' : 'Green',isLed ? `testLedPreset(${idx},0,255,0,'all')` : `testLedPreset(${idx},0,255,0)`,'bg')}
      ${runBtn(isLed ? 'All Blue' : 'Blue',isLed ? `testLedPreset(${idx},0,0,255,'all')` : `testLedPreset(${idx},0,0,255)`,'bp')}
      ${isLed ? `${runBtn('Pixel Custom',`testLed(${idx},'pixel')`)}${runBtn('Pixel Red',`testLedPreset(${idx},255,0,0,'pixel')`,'bd')}${runBtn('Pixel Green',`testLedPreset(${idx},0,255,0,'pixel')`,'bg')}${runBtn('Pixel Blue',`testLedPreset(${idx},0,0,255,'pixel')`,'bp')}` : ''}
      ${runBtn(isLed ? 'Blackout' : 'Off',`sendOutputTest(${idx},[],1000)`,'bd')}</div>`;
  } else if(o.type===1){
    html=`<div class="fg">${dur}<div class="f"><label for="test_dmx_ch">DMX Channel</label><input type="number" id="test_dmx_ch" value="1" min="1" max="512"></div><div class="f"><label for="test_level">Value</label><input type="range" id="test_level" min="0" max="255" value="255" oninput="test_level_num.value=this.value"></div><div class="f"><label for="test_level_num">Value Number</label><input type="number" id="test_level_num" value="255" min="0" max="255" oninput="test_level.value=this.value"></div></div>
      <div class="btns">${runBtn('Send Channel',`testDmx(${idx})`)}${runBtn('Blackout DMX',`sendOutputTest(${idx},[],1000)`,'bd')}</div>`;
  } else if(o.type===2){
    html=`<div class="fg">${dur}</div><div class="btns">${runBtn('Relay ON',`sendOutputTest(${idx},[255],testDur())`)}${runBtn('Relay OFF',`sendOutputTest(${idx},[0],1000)`,'bd')}</div>`;
  } else if(o.type===0||o.type===4||o.type===8){
    const label=o.type===8?'Servo Position':'Level';
    const res=parseInt(o.mc_resolution||8);
    const max=maxDmxFor(o);
    if(res>8){
      html=`<div class="fg">${dur}<div class="f"><label for="test_level_num">${label} (0-${max})</label><input type="number" id="test_level_num" value="${Math.floor(max/2)}" min="0" max="${max}"><div class="hint">${valueByteCount(res)} bytes resolution.</div></div></div>
        <div class="btns">${runBtn('Apply',`testSingleValue(${idx})`)}${runBtn('Min/Off',`testSinglePreset(${idx},0)`,'bs')}${runBtn('Max',`testSinglePreset(${idx},${max})`,'bg')}</div>`;
    }else{
      html=`<div class="fg">${dur}<div class="f"><label for="test_level">${label}</label><input type="range" id="test_level" min="0" max="255" value="128" oninput="test_level_num.value=this.value"></div><div class="f"><label for="test_level_num">Value Number</label><input type="number" id="test_level_num" value="128" min="0" max="255" oninput="test_level.value=this.value"></div></div>
        <div class="btns">${runBtn('Apply',`testSingleValue(${idx})`)}${runBtn('Min/Off',`testSinglePreset(${idx},0)`,'bs')}${runBtn('Max',`testSinglePreset(${idx},255)`,'bg')}</div>`;
    }
  } else if(o.type===6){
    html=`<div class="fg">${dur}<div class="f"><label for="test_level_num">Speed (0-255)</label><input type="number" id="test_level_num" value="200" min="0" max="255"></div></div>
      <div class="btns">${runBtn('Forward',`testMotor(${idx},1)`,'bg')}${runBtn('Stop',`testMotor(${idx},0)`,'bs')}${runBtn('Reverse',`testMotor(${idx},-1)`,'bd')}</div>`;
  } else if(o.type===7){
    const max=maxDmxFor(o);
    const unitLabel=o.mc_unit_type===1?'\u00B0':(o.mc_unit_type===2?' mm':' Steps');
    html=`<div class="fg">${dur}<div class="f"><label for="test_step_pos">Position (0-${max}${unitLabel})</label><input type="number" id="test_step_pos" value="${Math.floor(max/2)}" min="0" max="${max}"><div class="hint">${valueByteCount(o.mc_resolution)} position byte(s), then Speed, then Cmd.</div></div><div class="f"><label for="test_step_speed">Speed</label><input type="number" id="test_step_speed" value="180" min="0" max="255"></div></div>
      <div class="btns">${runBtn('Move',`testStepper(${idx},0)`)}${runBtn('Stop Cmd',`testStepper(${idx},120)`,'bs')}${runBtn('Home Cmd',`testStepper(${idx},220)`,'bd')}</div>`;
  } else if(o.type===17){
    html=`<div class="fg"><div class="f"><label for="test_duration">Duration (seconds)</label><input type="number" id="test_duration" value="1" min="1" max="10"></div></div>
      <div class="btns">${runBtn('Trigger Pulse',`sendOutputTest(${idx},[255],testDur())`,'bp')}${runBtn('Stop',`sendOutputTest(${idx},[0],1000)`,'bd')}</div>`;
  } else if(o.type>=11&&o.type<=13){
    const m=parseInt(o.mc_mode||0);
    if(o.type>=12){
      html=`<div class="fg">${dur}<div class="f"><label for="test_7seg_char">Character</label><input type="text" id="test_7seg_char" value="8" maxlength="1"><div class="hint">Sends 1 byte to direct-drive 7-segment.</div></div></div>
        <div class="btns">${runBtn('Show',`sendOutputTest(${idx},[document.getElementById('test_7seg_char').value.charCodeAt(0)&255],testDur())`)}${runBtn('Blank',`sendOutputTest(${idx},[32],1000)`,'bd')}</div>`;
    } else if(m===1){
      html=`<div class="fg">${dur}<div class="f"><label for="test_7seg_text">Text (4 chars)</label><input type="text" id="test_7seg_text" value="TEST" maxlength="4"><div class="hint">Sends ASCII bytes to the TM1637 display.</div></div></div>
        <div class="btns">${runBtn('Show Text',`test7Seg(${idx})`)}${runBtn('Blank',`sendOutputTest(${idx},[32,32,32,32],1000)`,'bd')}</div>`;
    } else {
      html=`<div class="fg">${dur}<div class="f"><label for="test_7seg_num">Number (0-9999)</label><input type="number" id="test_7seg_num" value="1234" min="0" max="9999"></div></div>
        <div class="btns">${runBtn('Show Number',`test7Seg(${idx})`)}${runBtn('Zero',`sendOutputTest(${idx},[0,0],1000)`,'bd')}</div>`;
    }
  } else if(o.type===10){
    html=`<div class="fg">${dur}<div class="f"><label for="test_mp3_track">Track (1-255)</label><input type="number" id="test_mp3_track" value="1" min="0" max="255"></div>
      <div class="f"><label for="test_mp3_vol">Volume (0-255)</label><input type="range" id="test_mp3_vol" min="0" max="255" value="200" oninput="test_mp3_vol_num.value=this.value"></div>
      <div class="f"><label for="test_mp3_vol_num">Volume Number</label><input type="number" id="test_mp3_vol_num" value="200" min="0" max="255" oninput="test_mp3_vol.value=this.value"></div></div>
      <div class="btns">${runBtn('Play Track',`testMp3(${idx},'play')`,'bg')}${runBtn('Stop',`testMp3(${idx},'stop')`,'bd')}${runBtn('Pause',`testMp3(${idx},'pause')`,'bs')}${runBtn('Next',`testMp3(${idx},'next')`)}${runBtn('Prev',`testMp3(${idx},'prev')`)}</div>`;
  } else if(o.type===14){
    html=`<div class="fg">${dur}<div class="f"><label for="test_dac_val">Value (0-255)</label><input type="range" id="test_dac_val" min="0" max="255" value="128" oninput="test_dac_val_num.value=this.value"></div>
      <div class="f"><label for="test_dac_val_num">Value Number</label><input type="number" id="test_dac_val_num" value="128" min="0" max="255" oninput="test_dac_val.value=this.value"></div></div>
      <div class="btns">${runBtn('Set',`sendOutputTest(${idx},[parseInt(document.getElementById('test_dac_val').value)],testDur())`,'bg')}${runBtn('Zero',`sendOutputTest(${idx},[0],1000)`,'bd')}${runBtn('Full',`sendOutputTest(${idx},[255],1000)`,'bs')}</div>`;
  } else if(o.type===15){
    html=`<div class="fg">${dur}<div class="f"><label for="test_level">PWM Level (0-255)</label><input type="range" id="test_level" min="0" max="255" value="128" oninput="test_level_num.value=this.value"></div>
      <div class="f"><label for="test_level_num">Value Number</label><input type="number" id="test_level_num" value="128" min="0" max="255" oninput="test_level.value=this.value"></div></div>
      <div class="btns">${runBtn('Set',`testSingleValue(${idx})`,'bg')}${runBtn('Min',`testSinglePreset(${idx},0)`,'bs')}${runBtn('Max',`testSinglePreset(${idx},255)`,'bd')}</div>`;
  } else if(o.type===9){
    html=`<div class="fg">${dur}<div class="f"><label for="test_buz_freq">Frequency</label><input type="range" id="test_buz_freq" min="0" max="255" value="128" oninput="test_buz_freq_num.value=this.value"></div>
      <div class="f"><label for="test_buz_freq_num">Frequency Value (1-255 \u2192 100-5000Hz)</label><input type="number" id="test_buz_freq_num" value="128" min="0" max="255" oninput="test_buz_freq.value=this.value"></div>
      <div class="f"><label for="test_buz_vol">Volume</label><input type="range" id="test_buz_vol" min="0" max="255" value="200" oninput="test_buz_vol_num.value=this.value"></div>
      <div class="f"><label for="test_buz_vol_num">Volume Number</label><input type="number" id="test_buz_vol_num" value="200" min="0" max="255" oninput="test_buz_vol.value=this.value"></div></div>
      <div class="btns">${runBtn('Play',`sendOutputTest(${idx},[parseInt(document.getElementById('test_buz_freq').value),parseInt(document.getElementById('test_buz_vol').value)],testDur())`,'bg')}${runBtn('Stop',`sendOutputTest(${idx},[0,0],1000)`,'bd')}</div>`;
  } else if(o.type===18){
    html=`<div class="fg">${dur}</div><div class="btns">${runBtn('Trigger Smoke+Shoot',`sendOutputTest(${idx},[255],testDur())`,'bp')}${runBtn('Off',`sendOutputTest(${idx},[0],1000)`,'bd')}</div>`;
  } else if(o.type===16){
    html=`<div class="fg">${dur}<div class="f"><label for="test_fg_freq">Frequency (1-10000 Hz)</label><input type="number" id="test_fg_freq" value="100" min="1" max="10000"></div>
      <div class="f"><label for="test_fg_type">Waveform</label><select id="test_fg_type"><option value="0">Sine</option><option value="1">Triangle</option><option value="2">Sawtooth</option><option value="3">Square</option><option value="4">PWM</option></select></div>
      <div class="f"><label for="test_fg_amp">Amplitude (0-255)</label><input type="range" id="test_fg_amp" min="0" max="255" value="128" oninput="test_fg_amp_num.value=this.value"></div>
      <div class="f"><label for="test_fg_amp_num">Amplitude Number</label><input type="number" id="test_fg_amp_num" value="128" min="0" max="255" oninput="test_fg_amp.value=this.value"></div>
      <div class="f"><label for="test_fg_off">Offset (0-255)</label><input type="range" id="test_fg_off" min="0" max="255" value="128" oninput="test_fg_off_num.value=this.value"></div>
      <div class="f"><label for="test_fg_off_num">Offset Number</label><input type="number" id="test_fg_off_num" value="128" min="0" max="255" oninput="test_fg_off.value=this.value"></div></div>
      <div class="btns">${runBtn('Apply',`sendOutputTest(${idx},[parseInt(document.getElementById('test_fg_freq').value)&255,(parseInt(document.getElementById('test_fg_freq').value)>>8)&255,parseInt(document.getElementById('test_fg_type').value),parseInt(document.getElementById('test_fg_amp').value),parseInt(document.getElementById('test_fg_off').value)],testDur())`,'bg')}${runBtn('Stop',`sendOutputTest(${idx},[0,0,0,0,0],1000)`,'bd')}</div>`;
  }
  c.innerHTML=html;
  p.scrollIntoView({behavior:'smooth',block:'nearest'});
}
function calcRcCutoff(){
  const r=parseFloat(document.getElementById('rc_r')?.value);
  const c=parseFloat(document.getElementById('rc_c')?.value);
  const fcEl=document.getElementById('rc_fc');
  if(!r||!c||r<1||c<1){fcEl&&(fcEl.textContent='\u2014');return;}
  const fc=1/(2*Math.PI*r*c*1e-9);
  fcEl&&(fcEl.textContent=fc>=1000?fc.toFixed(0):fc.toFixed(1));
}
function testDur(){return (parseInt(document.getElementById('test_duration')?.value)||30)*1000;}
function buildLedValues(o,r,g,b,w,mode){
  const rgbw=(o.color_order||0)>=4;
  if(o.type===5){
    return rgbw?[r,g,b,w]:[r,g,b];
  }
  const count=o.led_count||170, vals=[];
  const target=Math.max(1,Math.min(count,parseInt(document.getElementById('test_pixel')?.value)||1))-1;
  for(let i=0;i<count;i++){
    const on=mode!=='pixel'||i===target;
    vals.push(on?r:0,on?g:0,on?b:0);
    if(rgbw) vals.push(on?w:0);
  }
  return vals;
}
function testLed(idx,mode='all'){
  const o=outputs[idx], hex=document.getElementById('test_color').value;
  const r=parseInt(hex.slice(1,3),16), g=parseInt(hex.slice(3,5),16), b=parseInt(hex.slice(5,7),16), w=parseInt(document.getElementById('test_white').value)||0;
  sendOutputTest(idx,buildLedValues(o,r,g,b,w,mode),testDur());
}
function testLedPreset(idx,r,g,b,mode='all'){
  const o=outputs[idx];
  sendOutputTest(idx,buildLedValues(o,r,g,b,0,mode),testDur());
}
function testDmx(idx){
  const ch=Math.max(1,Math.min(512,parseInt(document.getElementById('test_dmx_ch').value)||1));
  const val=Math.max(0,Math.min(255,parseInt(document.getElementById('test_level_num').value)||0));
  const vals=new Array(512).fill(0); vals[ch-1]=val; sendOutputTest(idx,vals,testDur());
}
function testSingleValue(idx){
  const o=outputs[idx];
  const res=parseInt(o.mc_resolution||8);
  const max=maxDmxFor(o);
  const val=Math.max(0,Math.min(max,parseInt(document.getElementById('test_level_num').value)||0));
  testSinglePreset(idx,val);
}
function testSinglePreset(idx,val){
  const o=outputs[idx];
  const res=parseInt(o.mc_resolution||8);
  const max=maxDmxFor(o);
  val=Math.max(0,Math.min(max,parseInt(val)||0));
  if(o.type===0||o.type===4||o.type===8){
    sendOutputTest(idx,valueBytes(val,res),testDur());
  } else {
    sendOutputTest(idx,[val],testDur());
  }
}
function testMotor(idx,dir){
  const o=outputs[idx], res=parseInt(o.mc_resolution||8), max=maxDmxFor(o), speed=Math.max(0,Math.min(255,parseInt(document.getElementById('test_level_num').value)||0));
  const center=Math.floor(max/2), v=dir===0?center:(dir>0?Math.round(center+(speed/255)*(max-center)):Math.round(center-(speed/255)*center));
  sendOutputTest(idx,valueBytes(v,res),testDur());
}
function testStepper(idx,cmd){
  const o=outputs[idx], res=parseInt(o.mc_resolution||8), max=maxDmxFor(o);
  const pos=Math.max(0,Math.min(max,Number(document.getElementById('test_step_pos').value)||0)), vals=valueBytes(pos,res);
  vals.push(Math.max(0,Math.min(255,parseInt(document.getElementById('test_step_speed').value)||0)),cmd);
  sendOutputTest(idx,vals,testDur());
}
function test7Seg(idx){
  const o=outputs[idx];
  if(parseInt(o.mc_mode||0)===1){
    const text=(document.getElementById('test_7seg_text').value||'').padEnd(4,' ').slice(0,4);
    sendOutputTest(idx,[...text].map(c=>c.charCodeAt(0)&255),testDur());
  } else {
    const n=Math.max(0,Math.min(9999,parseInt(document.getElementById('test_7seg_num').value)||0));
    sendOutputTest(idx,[(n>>8)&255,n&255],testDur());
  }
}
function testMp3(idx,action){
  const track=parseInt(document.getElementById('test_mp3_track').value)||1;
  const vol=parseInt(document.getElementById('test_mp3_vol').value)||200;
  if(action==='play'){
    sendOutputTest(idx,[track,vol,0],testDur());
  } else if(action==='stop'){
    sendOutputTest(idx,[0,vol,0],1000);
  } else if(action==='pause'){
    sendOutputTest(idx,[track,vol,3],testDur());
  } else if(action==='next'){
    sendOutputTest(idx,[track,vol,1],testDur());
  } else if(action==='prev'){
    sendOutputTest(idx,[track,vol,2],testDur());
  }
}

function toggleOutFields(){
  const t=parseInt(document.getElementById('no_type').value);
  const mcMode = parseInt(document.getElementById('mc_mode')?.value||0);
  if(!document.getElementById('no_source')) renderPinRows();
  const srcSelect=document.getElementById('no_source');
  
  const isDmx = t===1;
  const isRelay = t===2;
  const isDimmer = t===0;
  const isMc = (t>=4 && t<=8);
  const isPwmDimmer = t===4;
  const isMotor = t===6;
  const isStepper = t===7;
  const isServo = t===8;
  const isSolenoid = t===17;
  const isAnalogRgb = t===5;
  const isBuzzer = t===9;
  const isSmoke = t===18;
  const is7Seg = t>=11 && t<=13;
  const isDfPlayer = t===10;
  const isPwmDac = t===15;
  const isFuncGen = t===16;
  const isDac = t===14;
  const isCommonDim = is7Seg && mcMode >= 6 && mcMode <= 9;
  const is7SegDD = is7Seg && mcMode >= 2 && mcMode <= 9;
  const is7SegDirectDim = is7Seg && (mcMode === 4 || mcMode === 5);

  const canUsePca = (t===2 || t===4 || t===6 || t===8 || t===5 || t===15 || t===17 || t===18 || is7SegDD);
  const canUseDigitalExpander = (t===2 || t===17 || t===18 || (is7SegDD && !isCommonDim && !is7SegDirectDim));
  const isGpioOnly = t===16;
  
  if (srcSelect && srcSelect.tagName === 'SELECT') {
    [...srcSelect.options].forEach(opt=>{
      const v=parseInt(opt.value);
      opt.disabled=(v===1&&!canUsePca)||(v>=2&&v<=4&&!canUseDigitalExpander)||(v>=5&&v<=7&&t!==14);
    });
    if (srcSelect.options[srcSelect.selectedIndex]?.disabled) srcSelect.value = 0;
    if (((!canUsePca && !canUseDigitalExpander) && t!==14) || isGpioOnly) {
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
  
  document.getElementById('no_addr_grp').style.display=(isDmx||t===3)?'none':'';
  document.getElementById('no_cnt_grp').style.display=(isDmx||isRelay||isDimmer||isMc||isSolenoid||isAnalogRgb||isBuzzer||isSmoke||is7Seg||isDfPlayer||isPwmDac||isDac||isFuncGen)?'none':'';
  document.getElementById('no_ord_grp').style.display=(t===3||t===5)?'':'none';
  document.getElementById('no_led_proto_grp').style.display=(t===3)?'':'none';
  
  setModeOptions(is7Seg?'7seg':'motor', t);
  document.getElementById('no_mc_grp').style.display=(isMc||is7Seg||isFuncGen||isPwmDac)?'':'none';
  document.getElementById('no_sol_grp').style.display=isSolenoid?'':'none';
  document.getElementById('no_smoke_grp').style.display=isSmoke?'':'none';
  if(isDfPlayer) renderPinRows();
  if(isMc || is7Seg) setResolutionOptions(isStepper, is7Seg);
  
  const hMode = parseInt(document.getElementById('mc_homing_mode').value);
  const colorOrder = parseInt(document.getElementById('no_ord').value||0);
  document.getElementById('no_pca_channel2_grp').style.display='none';
  document.getElementById('no_pca_channel3_grp').style.display='none';
  document.getElementById('no_pca_channel4_grp').style.display='none';

  if (isMc || is7Seg || isFuncGen || isPwmDac) {
    document.getElementById('mc_mode_grp').style.display = (is7Seg || isMotor) ? '' : 'none';
    document.getElementById('mc_res_grp').style.display = (isMc || isPwmDac || is7Seg) ? '' : 'none';
    const resLbl = document.getElementById('mc_resolution_lbl');
    if(resLbl) resLbl.textContent = is7Seg ? 'Decode Mode' : 'Resolution';
    const freqGrp = document.getElementById('mc_freq_grp');
    const freqLbl = document.getElementById('mc_freq_lbl');
    const pwmDacCalGrp = document.getElementById('pwm_dac_cal_grp');
    if (isFuncGen || isPwmDac) {
      freqGrp.style.display = '';
      freqLbl.textContent = "PWM Carrier Frequency (Hz)";
      document.getElementById('rc_filter_grp').style.display = '';
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = isPwmDac ? '' : 'none';
      calcRcCutoff();
    } else if (is7Seg) {
      freqGrp.style.display = (t >= 12) ? '' : 'none';
      freqLbl.textContent = "PWM Frequency (Hz)";
      document.getElementById('rc_filter_grp').style.display = 'none';
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else if (isPwmDimmer || isMotor) {
      freqGrp.style.display = '';
      freqLbl.textContent = "Frequency (Hz)";
      document.getElementById('rc_filter_grp').style.display = 'none';
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else if (isStepper) {
      freqGrp.style.display = '';
      freqLbl.textContent = "Speed (Hz)";
      document.getElementById('rc_filter_grp').style.display = 'none';
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else if (isAnalogRgb) {
      freqGrp.style.display = '';
      freqLbl.textContent = "Frequency (Hz)";
      document.getElementById('rc_filter_grp').style.display = 'none';
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    } else {
      freqGrp.style.display = 'none';
      document.getElementById('rc_filter_grp').style.display = 'none';
      if(pwmDacCalGrp) pwmDacCalGrp.style.display = 'none';
    }
    
    document.getElementById('mc_deadband_grp').style.display = isMotor ? '' : 'none';
    document.getElementById('mc_min_us_grp').style.display = isServo ? '' : 'none';
    document.getElementById('mc_max_us_grp').style.display = isServo ? '' : 'none';
    document.getElementById('mc_stepper_scale_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_steps_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_unit_type_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_scale_factor_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_stepper_homing_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_homing_mode_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_homing_dir_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_homing_speed_grp').style.display = isStepper ? '' : 'none';
    document.getElementById('mc_homing_timeout_grp').style.display = (isStepper && hMode === 1) ? '' : 'none';
    document.getElementById('mc_invert_grp').style.display = (isMotor || isStepper) ? '' : 'none';
    document.getElementById('mc_brake_grp').style.display = (isMotor && (mcMode === 0 || mcMode === 2)) ? '' : 'none';
  }
  renderPinRows();
  autoAssignOutputPins();
  const pinMapContainer = document.getElementById('pin-mapping-container');
  if(pinMapContainer) pinMapContainer.style.display = '';
}

document.getElementById('mc_mode').addEventListener('change', toggleOutFields);
document.getElementById('mc_homing_mode').addEventListener('change', toggleOutFields);
document.getElementById('no_ord').addEventListener('change', toggleOutFields);
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
  
  function setVal(id,val){const el=document.getElementById(id);if(el)el.value=val;}
  function setChk(id,val){const el=document.getElementById(id);if(el)el.checked=val;}
  
  setVal('no_source',o.source??0);
  setVal('no_pin',o.pin);
  setVal('no_pca_addr',o.pca_addr??64);
  setVal('no_pca_channel',o.pca_channel??0);
  setVal('no_pca_channel2',o.pca_channel2??255);
  setVal('no_pca_channel3',o.pca_channel3??255);
  setVal('no_pca_channel4',o.pca_channel4??255);
  setVal('no_uni',o.start_universe);
  setVal('no_addr',o.start_address||1);
  setVal('no_cnt',o.led_count||170);
  setVal('no_ord',o.color_order||0);
  setVal('no_led_proto',o.led_protocol??0);
  if(o.type===4||o.type===5||o.type===6||o.type===7||o.type===8||o.type===15) setResolutionOptions(o.type===7);
  
  setVal('no_pin2',o.pin2??255);
  setVal('no_pin3',o.pin3??255);
  setVal('no_pin4',o.pin4??255);
  setVal('no_pin4_source',o.pin4_source??0);
  setVal('no_pin4_addr',o.pin4_addr??32);
  setVal('no_pin4_channel',o.pin4_channel??255);
  setVal('no_pin2_source',o.pin2_source??0);
  setVal('no_pin2_addr',parseInt(o.pin2_addr??32));
  setVal('no_pin2_channel',o.pin2_channel??255);
  setVal('no_pin3_source',o.pin3_source??0);
  setVal('no_pin3_addr',parseInt(o.pin3_addr??32));
  setVal('no_pin3_channel',o.pin3_channel??255);
  setVal('mc_mode',o.mc_mode||0);
  setVal('mc_resolution',o.mc_resolution||8);
  setVal('mc_freq',o.mc_freq||1000);
  setVal('pwm_dac_mode',o.pwm_dac_mode??0);
  setVal('pwm_dac_min',intToDutyPct(o.pwm_dac_min,0));
  setVal('pwm_dac_max',intToDutyPct(o.pwm_dac_max,10000));
  setVal('mc_deadband',o.mc_deadband||10);
  setVal('mc_min_us',o.mc_min_us||1000);
  setVal('mc_max_us',o.mc_max_us||2000);
  setVal('mc_steps_per_rev',o.mc_steps_per_rev||200);
  setVal('mc_unit_type',o.mc_unit_type??0);
  setVal('mc_scale_factor',o.mc_scale_factor??0.0);
  setChk('mc_invert',o.mc_invert||false);
  setChk('mc_brake',o.mc_brake||false);
  const isStepperType=o.type===7;
  setChk('no_pin_invert',isStepperType?(o.mc_step_invert||false):(o.pin_invert||false));
  setChk('no_pin2_invert',isStepperType?(o.mc_dir_invert||false):(o.pin2_invert||false));
  setChk('no_pin3_invert',isStepperType?(o.mc_enable_active_high||false):(o.pin3_invert||false));
  setChk('no_pin4_invert',o.pin4_invert||false);
  setVal('mc_homing_mode',o.mc_homing_mode??0);
  setVal('mc_homing_dir',o.mc_homing_dir??0);
  setVal('mc_homing_speed',o.mc_homing_speed??500);
  setVal('mc_homing_timeout',o.mc_homing_timeout??5);
  setVal('sol_threshold',o.solenoid_threshold??127);
  setVal('sol_pulse_ms',o.solenoid_pulse_ms??50);
  setVal('sol_pre_delay',o.solenoid_pre_delay??0);
  setVal('sol_post_delay',o.solenoid_post_delay??100);
  
  if(document.getElementById('smoke_threshold')) setVal('smoke_threshold',o.solenoid_threshold??127);
  if(document.getElementById('smoke_dur')) setVal('smoke_dur',o.smoke_duration_ms??1000);
  if(document.getElementById('smoke_settle')) setVal('smoke_settle',o.settle_delay_ms??500);
  if(document.getElementById('smoke_shoot')) setVal('smoke_shoot',o.shoot_duration_ms??1000);
  if(document.getElementById('smoke_lockout')) setVal('smoke_lockout',o.smoke_lockout_ms??2000);

  const ssrc = o.seg_sources || [];
  for (let s = 0; s <= 7; s++) {
    setVal('no_seg_source_' + s, ssrc[s] !== undefined ? ssrc[s] : 0);
  }

  toggleOutFields();
  if(o.type===14){
    toggleOutFields();
  }
  if (o.source !== 0) {
    setVal('no_pca_addr', o.pca_addr??(o.source===5?96:o.source===6?76:o.source===7?76:64));
    setVal('no_pca_channel', o.pca_channel??0);
  }
  
  const sps = o.seg_pins || [];
  const saddrs = o.seg_addrs || [];
  const sch = o.seg_channels || [];
  const sinvs = o.seg_inverts || 0;
  for (let s = 0; s <= 7; s++) {
    setVal('no_seg_addr_' + s, saddrs[s] !== undefined ? saddrs[s] : 32);
    setVal('no_seg_channel_' + s, sch[s] !== undefined ? sch[s] : 255);
    setVal('no_seg_pin_' + s, sps[s] !== undefined ? sps[s] : 255);
    const invEl = document.getElementById('no_seg_pin_invert_' + s);
    if(invEl) invEl.checked = !!((sinvs >> s) & 1);
  }
  
  document.getElementById('out-add-btn').textContent='Update Channel';
  document.getElementById('out-cancel-btn').style.display='';
  document.getElementById('edit-idx-label').textContent=idx+1;
  document.getElementById('edit-banner').style.display='';
  document.getElementById('no_type').scrollIntoView({behavior:'smooth',block:'start'});
}

function cancelEditOutput(){
  editOutIdx=-1;
  const setVal=(id,val)=>{const el=document.getElementById(id);if(el)el.value=val;};
  document.getElementById('no_pca_channel2').value=255;
  document.getElementById('no_pca_channel3').value=255;
  document.getElementById('no_pca_channel4').value=255;
  document.getElementById('no_led_proto').value=0;
  document.getElementById('no_pin4_source').value=0;
  document.getElementById('no_pin4_addr').value=32;
  document.getElementById('no_pin4_channel').value=255;
  document.getElementById('no_pin2_source').value=0;
  document.getElementById('no_pin2_channel').value=255;
  document.getElementById('no_pin3_source').value=0;
  document.getElementById('no_pin3_channel').value=255;
  setVal('pwm_dac_mode',0);
  setVal('pwm_dac_min','0.00');
  setVal('pwm_dac_max','100.00');
  for(let s=0; s<=7; s++){
    const pinEl=document.getElementById('no_seg_pin_'+s);
    const srcEl=document.getElementById('no_seg_source_'+s);
    const addrEl=document.getElementById('no_seg_addr_'+s);
    const chEl=document.getElementById('no_seg_channel_'+s);
    const invEl=document.getElementById('no_seg_pin_invert_'+s);
    if(pinEl) pinEl.value=255;
    if(srcEl) srcEl.value=0;
    if(addrEl) addrEl.value=32;
    if(chEl) chEl.value=255;
    if(invEl) invEl.checked=false;
  }
  setVal('sol_threshold',127);
  if(document.getElementById('smoke_threshold')) setVal('smoke_threshold',127);
  ['no_pin_invert','no_pin2_invert','no_pin3_invert','no_pin4_invert'].forEach(id=>{const el=document.getElementById(id);if(el)el.checked=false;});
  document.getElementById('out-add-btn').textContent='Add Channel';
  document.getElementById('out-cancel-btn').style.display='none';
  document.getElementById('edit-banner').style.display='none';
  autoAssignOutputPins();
}

function addOrUpdateOutput(){
  const type=parseInt(document.getElementById('no_type').value);
  function gv(id,d){const el=document.getElementById(id);return el?parseInt(el.value):d;}
  const source=parseInt(document.getElementById('no_source').value||0);
  const ch={
    type:type,
    source:source,
    pin: gv('no_pin',255),
    pca_addr: gv('no_pca_addr',64),
    pca_channel: gv('no_pca_channel',0),
    pca_channel2: gv('no_pca_channel2',255),
    pca_channel3: gv('no_pca_channel3',255),
    pca_channel4: gv('no_pca_channel4',255),
    start_universe:parseInt(document.getElementById('no_uni').value),
    start_address:parseInt(document.getElementById('no_addr').value)||1,
    led_count:parseInt(document.getElementById('no_cnt').value)||170,
    color_order:parseInt(document.getElementById('no_ord').value),
    led_protocol:parseInt(document.getElementById('no_led_proto').value||0),
    pin_invert: type===7?false:(document.getElementById('no_pin_invert')?.checked || false),
    pin2_invert: type===7?false:(document.getElementById('no_pin2_invert')?.checked || false),
    pin3_invert: type===7?false:(document.getElementById('no_pin3_invert')?.checked || false),
    pin4_invert: document.getElementById('no_pin4_invert')?.checked || false,
    pin2: gv('no_pin2',255),
    pin3: gv('no_pin3',255),
    pin4: gv('no_pin4',255),
    pin4_source: gv('no_pin4_source',0),
    pin4_addr: gv('no_pin4_addr',32),
    pin4_channel: gv('no_pin4_channel',255),
    pin2_source: gv('no_pin2_source',0),
    pin2_addr: gv('no_pin2_addr',32),
    pin2_channel: gv('no_pin2_channel',255),
    pin3_source: gv('no_pin3_source',0),
    pin3_addr: gv('no_pin3_addr',32),
    pin3_channel: gv('no_pin3_channel',255),
    mc_mode: parseInt(document.getElementById('mc_mode').value),
    mc_resolution: parseInt(document.getElementById('mc_resolution').value),
    mc_freq: parseInt(document.getElementById('mc_freq').value),
    pwm_dac_mode: parseInt(document.getElementById('pwm_dac_mode')?.value || 0),
    pwm_dac_min: dutyPctToInt('pwm_dac_min', 0),
    pwm_dac_max: dutyPctToInt('pwm_dac_max', 10000),
    mc_deadband: parseInt(document.getElementById('mc_deadband').value),
    mc_min_us: parseInt(document.getElementById('mc_min_us').value),
    mc_max_us: parseInt(document.getElementById('mc_max_us').value),
    mc_steps_per_rev: parseInt(document.getElementById('mc_steps_per_rev').value),
    mc_unit_type: parseInt(document.getElementById('mc_unit_type').value),
    mc_scale_factor: parseFloat(document.getElementById('mc_scale_factor').value),
    mc_invert: document.getElementById('mc_invert').checked,
    mc_brake: document.getElementById('mc_brake').checked,
    mc_enable_active_high: type===7?(document.getElementById('no_pin3_invert')?.checked || false):false,
    mc_dir_invert: type===7?(document.getElementById('no_pin2_invert')?.checked || false):false,
    mc_step_invert: type===7?(document.getElementById('no_pin_invert')?.checked || false):false,
    mc_homing_mode: parseInt(document.getElementById('mc_homing_mode').value),
    mc_homing_dir: parseInt(document.getElementById('mc_homing_dir').value),
    mc_homing_speed: parseInt(document.getElementById('mc_homing_speed').value),
    mc_homing_timeout: parseInt(document.getElementById('mc_homing_timeout').value),
    solenoid_mode: 0,
    solenoid_threshold: (type===18) ? parseInt(document.getElementById('smoke_threshold')?.value || 127) : parseInt(document.getElementById('sol_threshold')?.value || 127),
    solenoid_pulse_ms: parseInt(document.getElementById('sol_pulse_ms')?.value || 50),
    solenoid_pre_delay: parseInt(document.getElementById('sol_pre_delay')?.value || 0),
    solenoid_post_delay: parseInt(document.getElementById('sol_post_delay')?.value || 100),
    
    smoke_duration_ms: parseInt(document.getElementById('smoke_dur')?.value || 1000),
    settle_delay_ms: parseInt(document.getElementById('smoke_settle')?.value || 500),
    shoot_duration_ms: parseInt(document.getElementById('smoke_shoot')?.value || 1000),
    smoke_lockout_ms: parseInt(document.getElementById('smoke_lockout')?.value || 2000),
    seg_pins: (type===12||type===13)? [
      ((type===12||type===13) && (parseInt(document.getElementById('mc_mode').value)>=6 && parseInt(document.getElementById('mc_mode').value)<=9)) ? gv('no_seg_pin_0', 255) : gv('no_pin', 255),
      gv('no_seg_pin_1', 255),
      gv('no_seg_pin_2', 255),
      gv('no_seg_pin_3', 255),
      gv('no_seg_pin_4', 255),
      gv('no_seg_pin_5', 255),
      gv('no_seg_pin_6', 255),
      gv('no_seg_pin_7', 255)
    ] : [255, 255, 255, 255, 255, 255, 255, 255],
    seg_sources: (type===12||type===13)? [0,1,2,3,4,5,6,7].map(s=>{
      if (s === 0 && !(parseInt(document.getElementById('mc_mode').value)>=6 && parseInt(document.getElementById('mc_mode').value)<=9)) {
        return parseInt(document.getElementById('no_source').value);
      }
      return gv('no_seg_source_'+s,0);
    }) : [0,0,0,0,0,0,0,0],
    seg_addrs: (type===12||type===13)? [0,1,2,3,4,5,6,7].map(s=>{
      if (s === 0 && !(parseInt(document.getElementById('mc_mode').value)>=6 && parseInt(document.getElementById('mc_mode').value)<=9)) {
        return parseInt(document.getElementById('no_pca_addr')?.value||32);
      }
      return gv('no_seg_addr_'+s,32);
    }) : [32,32,32,32,32,32,32,32],
    seg_channels: (type===12||type===13)? [0,1,2,3,4,5,6,7].map(s=>{
      if (s === 0 && !(parseInt(document.getElementById('mc_mode').value)>=6 && parseInt(document.getElementById('mc_mode').value)<=9)) {
        const mainSrc = parseInt(document.getElementById('no_source').value);
        return mainSrc !== 0 ? parseInt(document.getElementById('no_pca_channel')?.value||255) : 255;
      }
      return gv('no_seg_channel_'+s,255);
    }) : [255,255,255,255,255,255,255,255],
    seg_inverts: (type===12||type===13) ? [0,1,2,3,4,5,6,7].reduce((acc, s)=>{
      if (s === 0 && !(parseInt(document.getElementById('mc_mode').value)>=6 && parseInt(document.getElementById('mc_mode').value)<=9)) {
        const isInv = document.getElementById('no_pin_invert')?.checked || false;
        return acc | (isInv ? (1 << s) : 0);
      }
      const isInv = document.getElementById('no_seg_pin_invert_'+s)?.checked || false;
      return acc | (isInv ? (1 << s) : 0);
    }, 0) : 0
  };

  const statusPin=parseInt(document.getElementById('status_led_pin').value);
  const zcPin=parseInt(document.getElementById('zc_pin').value);
  const sdaPin=parseInt(document.getElementById('i2c_sda')?.value||255);
  const sclPin=parseInt(document.getElementById('i2c_scl')?.value||255);
  
  if(statusPin!==255&&zcPin!==255&&statusPin===zcPin){
    alert('Status LED GPIO and Zero-Crossing GPIO cannot use the same pin.');
    return;
  }
  const gpios = outputGpios(ch);
  for(const gp of gpios){
    if(FORBIDDEN_OUTPUT_GPIOS[gp]){
      alert('GPIO '+gp+' is not allowed on WT32-ETH01 ('+FORBIDDEN_OUTPUT_GPIOS[gp]+'). Select another pin.');
      return;
    }
    if(INPUT_ONLY_GPIOS.includes(gp)){
      alert('GPIO '+gp+' is input-only on ESP32 and cannot be used as an output pin.');
      return;
    }
  }
  if(gpios.includes(statusPin) && statusPin!==255){
    alert('GPIO '+statusPin+' is reserved for Status LED. Disable Status LED or select another output GPIO.');
    return;
  }
  if(gpios.includes(zcPin) && zcPin!==255){
    alert('GPIO '+zcPin+' is reserved for Zero-Crossing input. Disable Zero-Crossing or select another output GPIO.');
    return;
  }
  if(gpios.includes(sdaPin) && sdaPin!==255){
    alert('GPIO '+sdaPin+' is reserved for I2C SDA. Select another output GPIO.');
    return;
  }
  if(gpios.includes(sclPin) && sclPin!==255){
    alert('GPIO '+sclPin+' is reserved for I2C SCL. Select another output GPIO.');
    return;
  }
  if(type===7){
    ch.source=0;
    if(ch.pin2_source!==0 && ch.pin2_channel===255){
      alert('Stepper DIR expander channel is required.');
      return;
    }
    if(ch.pin3_source!==0 && ch.pin3_channel===255){
      alert('Stepper ENABLE expander channel is required.');
      return;
    }
    if(ch.mc_homing_mode===0 && ch.pin4_source!==0 && ch.pin4_channel===255){
      alert('Stepper HOME expander channel is required.');
      return;
    }
  }
  if(type===18){
    if(ch.pin2_source>4){
      alert('Unsupported Smoke Shooter Shoot pin source.');
      return;
    }
    if(ch.pin2_source!==0 && ch.pin2_channel===255){
      alert('Smoke Shooter Shoot expander channel is required.');
      return;
    }
  }
  if(type===5){
    if((ch.source===1 && ch.pca_channel===255) ||
       (ch.pin2_source===1 && ch.pin2_channel===255) ||
       (ch.pin3_source===1 && ch.pin3_channel===255) ||
       ((ch.color_order||0)>=4 && ch.pin4_source===1 && ch.pin4_channel===255)){
      alert('Analog RGB/RGBW PCA9685 channel is required for each PCA-routed color.');
      return;
    }
  }
  if((type===12||type===13) && ch.seg_sources){
    const numSeg = type===13 ? 8 : 7;
    const isDirectDim = ch.mc_mode===4 || ch.mc_mode===5;
    if(isDirectDim && ch.pin2_source>=2 && ch.pin2_source<=4){
      alert('7-Segment Direct Dim needs PWM per segment. Use ESP32 GPIO or PCA9685, not a digital GPIO expander.');
      return;
    }
    for(let s=0; s<numSeg; s++){
      if(isDirectDim && (ch.seg_sources[s]||0)>=2 && (ch.seg_sources[s]||0)<=4){
        alert('7-Segment Direct Dim segment '+(s+1)+' needs ESP32 GPIO or PCA9685, not a digital GPIO expander.');
        return;
      }
      if((ch.seg_sources[s]||0)!==0 && (ch.seg_channels[s]===undefined || ch.seg_channels[s]===255)){
        alert('7-Segment segment '+(s+1)+' expander channel is required.');
        return;
      }
      if((ch.seg_sources[s]||0)>=1 && (ch.seg_sources[s]||0)<=4 && ch.seg_channels[s]!==255 && ch.seg_channels[s]>15){
        alert('7-Segment segment '+(s+1)+' expander channel must be 0-15.');
        return;
      }
    }
  }
  if((type===12||type===13) && ch.pin2_source>=1 && ch.pin2_source<=4){
    const numSeg = type===13 ? 8 : 7;
    const baseCh = parseInt(ch.pin2_channel);
    if(!isNaN(baseCh) && baseCh!==255 && baseCh+numSeg-1>15){
      alert('7-Segment base channel '+baseCh+' with '+numSeg+' segments exceeds channel 15.');
      return;
    }
  }
  const validateOutputI2cAddresses=()=>{
    const check=(src,addr,label)=>validateSourceAddress(parseInt(src||0), parseInt(addr||0), label);
    if(type===14 && ch.source>=5 && ch.source<=7){
      if(!check(ch.source, ch.pca_addr, 'Primary source')) return false;
      if(ch.source!==7 && (ch.pca_channel||0)!==0){
        alert('Only DAC7573 supports DAC channels B-D.');
        return false;
      }
      if(ch.source===7 && (ch.pca_channel<0 || ch.pca_channel>3)){
        alert('DAC7573 channel must be 0-3 (A-D).');
        return false;
      }
    } else if(ch.source!==0 && !check(ch.source, ch.pca_addr, 'Primary source')) return false;
    if(type===12||type===13){
      if(ch.pin2_source!==0 && !check(ch.pin2_source, ch.pin2_addr, '7-Segment segment base source')) return false;
      const numSeg = type===13 ? 8 : 7;
      for(let s=0; s<numSeg; s++){
        const src=parseInt(ch.seg_sources?.[s]||0);
        if(src!==0 && !check(src, ch.seg_addrs?.[s]||32, '7-Segment segment '+(s+1)+' source')) return false;
      }
    }
    if(type===5){
      if(ch.pin2_source!==0 && !check(ch.pin2_source, ch.pin2_addr, 'Analog RGB/RGBW Green source')) return false;
      if(ch.pin3_source!==0 && !check(ch.pin3_source, ch.pin3_addr, 'Analog RGB/RGBW Blue source')) return false;
      if((ch.color_order||0)>=4 && ch.pin4_source!==0 && !check(ch.pin4_source, ch.pin4_addr, 'Analog RGB/RGBW White source')) return false;
    }
    if(type===6){
      if(ch.pin2_source!==0 && !check(ch.pin2_source, ch.pin2_addr, 'Motor IN2/DIR source')) return false;
      if(ch.pin3_source!==0 && !check(ch.pin3_source, ch.pin3_addr, 'Motor EN source')) return false;
    }
    if(type===7){
      if(ch.pin2_source!==0 && !check(ch.pin2_source, ch.pin2_addr, 'Stepper DIR source')) return false;
      if(ch.pin3_source!==0 && !check(ch.pin3_source, ch.pin3_addr, 'Stepper ENABLE source')) return false;
      if(ch.mc_homing_mode===0 && ch.pin4_source!==0 && !check(ch.pin4_source, ch.pin4_addr, 'Stepper HOME source')) return false;
    }
    if(type===18 && ch.pin2_source!==0 && !check(ch.pin2_source, ch.pin2_addr, 'Smoke Shooter shoot source')) return false;
    return true;
  };
  if(!validateOutputI2cAddresses()) return;
  const checkChan15=(src,ch,label)=>{src=parseInt(src||0);ch=parseInt(ch);if(src>=1&&src<=4&&ch!==255&&(ch<0||ch>15)){alert(label+' channel must be 0-15.');return false;}return true;};
  if(!checkChan15(ch.source,ch.pca_channel,'Primary source')) return;
  if(!checkChan15(ch.pin2_source,ch.pin2_channel,'Pin 2')) return;
  if(!checkChan15(ch.pin3_source,ch.pin3_channel,'Pin 3')) return;
  if(!checkChan15(ch.pin4_source,ch.pin4_channel,'Pin 4')) return;
  if(type===6 && ch.mc_mode===2){
    if(ch.pin3_source>=2){
      alert('Motor EN pin needs PWM. Use ESP32 GPIO or PCA9685 for EN, not a digital expander.');
      return;
    }
    if(ch.pin3_source===1 && ch.pin3_channel===255){
      alert('Motor EN PCA9685 channel is required.');
      return;
    }
  }
  if(type===15){
    if(ch.pwm_dac_mode<0 || ch.pwm_dac_mode>2){
      alert('Unsupported PWM DAC calibration mode.');
      return;
    }
    if(ch.pwm_dac_min<0 || ch.pwm_dac_min>10000 || ch.pwm_dac_max<0 || ch.pwm_dac_max>10000){
      alert('PWM DAC calibration duty must be between 0.00% and 100.00%.');
      return;
    }
  }

  if(ch.start_address<1) ch.start_address=1;
  if(ch.start_address>512) ch.start_address=512;
  if(type!==3&&type!==1){
    const needed=outputByteCount(ch);
    if(ch.start_address+needed-1>512){
      alert('Start Address is too high for this output. It needs '+needed+' DMX channel(s), so the last usable start address is '+(513-needed)+'.');
      return;
    }
  }

  const newOutputs = [...outputs];
  if(editOutIdx>=0) newOutputs[editOutIdx]=ch; else newOutputs.push(ch);

  const usedPins={};
  for(let i=0;i<newOutputs.length;i++){
    for(const gp of outputGpios(newOutputs[i])){
      if(usedPins[gp]!==undefined){
        alert('GPIO '+gp+' is already used by channel '+(usedPins[gp]+1)+' and channel '+(i+1)+'. Please select another pin.');
        return;
      }
      usedPins[gp]=i;
    }
  }

  {
    const formatAddr=addr=>'0x'+parseInt(addr).toString(16).toUpperCase();
    const usedExpander = {};
    const addUsedExpander=(src,addr,chn,i)=>{
      src=parseInt(src||0); chn=parseInt(chn);
      if(src===0||isNaN(chn)||chn===255) return false;
      if(chn>15){alert('Expander channel '+chn+' must be 0-15.');return true;}
      const key = `${src}_${addr}_${chn}`;
      if (usedExpander[key] !== undefined) {
        const srcName=SOURCES[src]||'Expander';
        alert(`${srcName} Channel ${chn} on Address ${formatAddr(addr)} is already used by channel ${usedExpander[key]+1} and channel ${i+1}. Select another channel.`);
        return true;
      }
      usedExpander[key] = i;
      return false;
    };
    for (let i = 0; i < newOutputs.length; i++) {
      const o = newOutputs[i];
      const src = parseInt(o.source||0);
      const type = o.type;
      const mcMode = parseInt(o.mc_mode||0);
      const colorOrder = parseInt(o.color_order||0);
      const is7SegDD = (type===12||type===13) && (mcMode>=2 && mcMode<=9);
      const primaryIsSevenSegA = (type===12||type===13) && (mcMode>=2 && mcMode<=5);

      if (src !== 0 && !primaryIsSevenSegA) {
        if(addUsedExpander(src, o.pca_addr, o.pca_channel, i)) return;
        if ((type===6||type===7||type===5||type===18) && o.pca_channel2!==255)
          if(addUsedExpander(src, o.pca_addr, o.pca_channel2, i)) return;
        if ((type===7||type===5||(type===6&&mcMode===2)) && o.pca_channel3!==255)
          if(addUsedExpander(src, o.pca_addr, o.pca_channel3, i)) return;
        if (type===5 && colorOrder>=4 && o.pca_channel4!==255)
          if(addUsedExpander(src, o.pca_addr, o.pca_channel4, i)) return;
      }

      const p2s = parseInt(o.pin2_source||0);
      const p3s = parseInt(o.pin3_source||0);
      const p4s = parseInt(o.pin4_source||0);
      if (p2s !== 0 && !is7SegDD)
        if(addUsedExpander(p2s, o.pin2_addr, o.pin2_channel, i)) return;
      if (p3s !== 0)
        if(addUsedExpander(p3s, o.pin3_addr, o.pin3_channel, i)) return;
      if (p4s !== 0)
        if(addUsedExpander(p4s, o.pin4_addr, o.pin4_channel, i)) return;

      if (is7SegDD && p2s !== 0) {
        const numSeg = (type === 13) ? 8 : 7;
        const baseCh = parseInt(o.pin2_channel);
        for (let s = 0; s < numSeg; s++) {
          if(addUsedExpander(p2s, o.pin2_addr, isNaN(baseCh)||baseCh===255 ? 255 : baseCh + s, i)) return;
        }
      } else if (is7SegDD) {
        const numSeg = (type === 13) ? 8 : 7;
        const segSources = o.seg_sources || [];
        const segAddrs = o.seg_addrs || [];
        const segChannels = o.seg_channels || [];
        for (let s = 0; s < numSeg; s++) {
          if(addUsedExpander(segSources[s] || 0, segAddrs[s] || 32, segChannels[s], i)) return;
        }
      }
    }

    const pcaInfo = {};
    const registerPca = (addr, freq, isServo, label) => {
      addr = parseInt(addr);
      if (!pcaInfo[addr]) pcaInfo[addr] = {hasServo: false, firstFreq: null, mixed: false, affected: []};
      if (isServo) { pcaInfo[addr].hasServo = true; return; }
      if (pcaInfo[addr].firstFreq === null) pcaInfo[addr].firstFreq = freq;
      else if (pcaInfo[addr].firstFreq !== freq) pcaInfo[addr].mixed = true;
      if (freq !== 50) pcaInfo[addr].affected.push(label);
    };
    for (let i = 0; i < newOutputs.length; i++) {
      const o = newOutputs[i];
      const t = o.type;
      const freq = (t === 8) ? 50 : parseInt(o.mc_freq || 1000);
      const isServo = (t === 8);
      const lbl = `${deviceLabel(o)} ch ${i+1} (${freq} Hz)`;
      const src = parseInt(o.source||0);
      if (src === 1) registerPca(o.pca_addr, freq, isServo, lbl);
      if ((t===6||t===7||t===5||t===18) && parseInt(o.pin2_source||0)===1)
        registerPca(o.pin2_addr, freq, isServo, lbl);
      if ((t===7||t===5||(t===6&&parseInt(o.mc_mode||0)===2)) && parseInt(o.pin3_source||0)===1)
        registerPca(o.pin3_addr, freq, isServo, lbl);
      if (t===5 && parseInt(o.color_order||0)>=4 && parseInt(o.pin4_source||0)===1)
        registerPca(o.pin4_addr, freq, isServo, lbl);
    }
    for(const addr in pcaInfo){
      const info=pcaInfo[addr];
      if(info.hasServo && info.affected.length){
        if(!confirm(`PCA9685 address ${formatAddr(addr)} has an RC Servo, so the whole chip will run at 50 Hz. This is OK for relays, but may affect PWM dimmers/DC motors/analog LEDs:\n\n${info.affected.join('\n')}\n\nContinue?`)) return;
      } else if(!info.hasServo && info.mixed){
        if(!confirm(`PCA9685 address ${formatAddr(addr)} has mixed PWM frequencies. One PCA9685 chip can use only one shared frequency; firmware will use the first configured frequency on that address. Continue?`)) return;
      }
    }
  }
  
  let ledcUsed = 0, rmtUsed = 0, timerUsed = 0, stepperCount = 0, totalLedPixels = 0;
  newOutputs.forEach(o => {
    const h = channelHardware(o);
    ledcUsed += h.ledc;
    rmtUsed += h.rmt;
    timerUsed += h.timer;
    if(o.type===3) totalLedPixels += o.led_count||170;
    if(o.type===7) stepperCount++;
  });
  
  let warnings=[];
  if(rmtUsed>8) warnings.push('Total RMT usage exceeds hardware limit (max 8). Some outputs may fail.');
  if(ledcUsed>16) warnings.push('LEDC PWM channels exceed hardware limit (max 16).');
  if(totalHardware(newOutputs).timer>HW_MAX.timer) warnings.push('Timer usage exceeds hardware/runtime limit (max 4).');
  if(stepperCount>2) warnings.push('Multiple stepper motors may cause timing jitter at high speed.');
  if(totalLedPixels>4000) warnings.push('Total LED pixels ('+totalLedPixels+') may cause RAM exhaustion.');
  newOutputs.forEach(o => { if(o.type===3&&o.led_count>1111) warnings.push('Single GPIO LED count >1111 drops refresh rate below 30 FPS.'); });
  
  const fps=parseInt(document.getElementById('output_fps')?.value)||40;
  const hwTot=totalHardware(newOutputs);
  const cpuTot=totalCpu(newOutputs,fps);
  const ramTot=totalRam(newOutputs);
  const cLim=cpuLimit(fps);
  if(hwBlocked(hwTot)){
    let what='', altMsg='';
    if(hwTot.ledc>HW_MAX.ledc){
      what='LEDC';
      if([4,5,6,8,15].includes(typeId(ch))) altMsg=' Use PCA9685 (source=1) to bypass LEDC limit.';
      else if(typeId(ch)===12||typeId(ch)===13) altMsg=' Route segments to PCA9685 instead of GPIO.';
      else altMsg=' This output type has no expander alternative.';
    } else if(hwTot.rmt>HW_MAX.rmt){
      what='RMT';
      altMsg=' This output type has no expander alternative.';
    } else if(hwTot.uart>HW_MAX.uart){
      what='UART';
      if(typeId(ch)===1) altMsg=' Use RMT-based DMX (only if at least 2 DFPlayers already use UART).';
      else altMsg=' This output type has no expander alternative.';
    } else if(hwTot.dac>HW_MAX.dac){
      what='DAC';
      altMsg=' Use I2C DAC (source=5,6,7) instead.';
    } else if(hwTot.timer>HW_MAX.timer){
      what='TIMER';
      altMsg=' Reduce AC Dimmer/Function Generator channels.';
    }
    alert(what+' limit exceeded ('+hwTot[what.toLowerCase()]+'/'+HW_MAX[what.toLowerCase()]+').'+altMsg);
    return;
  }
  if(cpuBlocked(cpuTot,fps)){
    alert('CPU budget '+cpuTot+'\u00B5s exceeds limit of '+cLim+'\u00B5s at '+fps+' FPS. Reduce channels or FPS.');
    return;
  }
  if(ramBlocked(ramTot)){
    alert('RAM budget '+(ramTot/1024).toFixed(0)+'KB exceeds limit of '+(RAM_LIMIT/1024)+'KB. Reduce pixel count or channels.');
    return;
  }

  if(warnings.length>0){
    const msg='Resource Warnings:\n\n'+warnings.join('\n');
    if(!confirm(msg)) return;
  }

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
  const removedIdx=outputs.findIndex(o=>typeId(o)<0||typeId(o)>18);
  if(removedIdx>=0){
    alert('Channel '+(removedIdx+1)+' uses an unsupported output type. Delete it or add a supported replacement before saving.');
    setSaveState('Remove old channel','err');
    return;
  }
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
