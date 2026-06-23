// ==========================================
// === RESOURCE SCORING & BUDGET CHECKS ===
// Single source of truth: SCORE_LIMITS from scoring_limits.h
// ==========================================
const S=SCORE_LIMITS;
const HW_MAX={ledc:S.MAX_LEDC,rmt:S.MAX_RMT,uart:S.MAX_UART,dac:S.MAX_DAC,timer:S.MAX_TIMER};
const RAM_LIMIT=S.LIMIT_CAP_BYTES;
const BASE_CHANNEL_RAM=S.BASE_CHANNEL_BYTES;
const BASE_OVERHEAD_US=S.BASE_OVERHEAD_US;
const SAFETY_RESERVE_US=S.SAFETY_RESERVE_US;
const I2C_WRITE_US=S.I2C_WRITE_US;
const DMX_BUFFER_RAM=S.DMX_BUFFER_BYTES;
const I2C_ROUTE_RAM=S.I2C_ROUTE_BYTES;

function channelHardware(o){
  var mode=outputModeDef(typeId(o),parseInt(o.mc_mode||0));
  if(!mode) return {ledc:0,rmt:0,uart:0,dac:0,timer:0};
  var hw=(mode.cost&&mode.cost.hardware)||{};
  var ledc=0,rmt=hw.rmt||0,uart=hw.uart||0,dac=hw.dac||0,timer=hw.timer||0,countedW=false;

  eachSlot(o,function(slot,pin,src,rule){
    if(src===0){
      if(rule.hwIfGpio===1){ledc++;if(slot==='pin4') countedW=true;}
      if(rule.hwIfGpio===2&&rmt<(hw.rmt||0)) rmt++;
    }
  });

  if(typeId(o)===T.ANALOG_RGB&&(parseInt(o.color_order||0))<4&&countedW) ledc--;
  if(typeId(o)===T.DAC&&(parseInt(o.source||0))===0) dac=1;

  return {ledc,rmt,uart,dac,timer};
}
function srcIsI2C(src){ return src>=1&&src<=7; }
function channelUsesI2C(o){
  var found=false;
  eachSlot(o,function(slot,pin,src,rule){
    if(srcIsI2C(src)) found=true;
  });
  return found;
}

function channelCost(o){
  var t=typeId(o);
  var mode=outputModeDef(t,parseInt(o.mc_mode||0));
  var ledCount=parseInt(o.led_count||0);
  var cpu=(mode&&mode.cost&&mode.cost.cpuUs)||0, ram=BASE_CHANNEL_RAM;
  ram+=dmxBufferRam(t,ledCount,parseInt(o.color_order||0),parseInt(o.mc_resolution||8),parseInt(o.mc_mode||0));
  if(mode&&mode.cost&&mode.cost.extraRam) ram+=mode.cost.extraRam;
  if(t===T.LED_STRIP) cpu=ledStripServiceUs(ledCount, parseInt(o.color_order||0));
  if(t===T.LED_STRIP) ram+=pixelBufferRam(ledCount,parseInt(o.color_order||0));
  var i2cWrites=i2cWriteCount(o);
  cpu+=i2cWrites*I2C_WRITE_US;
  ram+=i2cWrites*I2C_ROUTE_RAM;
  return {cpu,ram};
}

function dmxBufferRam(type,ledCount,colorOrder,resolution=8,mode=0){
  type=parseInt(type); mode=parseInt(mode||0);
  if(type===T.DMX) return DMX_BUFFER_RAM;
  if(type===T.LED_STRIP){
    var bpp=parseInt(colorOrder||0)>=4?4:3;
    var pixPerUni=Math.floor(S.DMX_BUFFER_BYTES/bpp);
    return Math.max(1,Math.ceil((parseInt(ledCount)||0)/pixPerUni))*S.DMX_BUFFER_BYTES;
  }
  if(type===T.ANALOG_RGB) return parseInt(colorOrder||0)>=4?4:3;
  if(type===T.STEPPER) return valueByteCount(resolution)+S.STEPPER_POSITION_BYTES;
  if(type===T.BUZZER||type===T.TM1637) return mode===1?4:2;
  if(type===T['7SEG_7PIN']||type===T['7SEG_8PIN']) return (mode===4||mode===5||mode>=6)?2:1;
  var def=outputModeDef(type,mode);
  if(def&&def.cost&&def.cost.dmxSlots>0) return def.cost.dmxSlots;
  return valueByteCount(resolution);
}

function pixelBufferRam(ledCount,colorOrder){
  var cost=outputModeDef(T.LED_STRIP,0)?.cost||{};
  var bpp=parseInt(colorOrder||0)>=4?4:(cost.ramPerUnit||3);
  return (parseInt(ledCount)||0)*bpp;
}

function ledStripServiceUs(ledCount,colorOrder){
  var cost=outputModeDef(T.LED_STRIP,0)?.cost||{};
  var bpp=parseInt(colorOrder||0)>=4?4:(cost.cpuPerUnit||3);
  return (cost.cpuUs||80)+ledCount*bpp;
}


function i2cWriteCount(o){
  var writes=0;
  eachSlot(o,function(slot,pin,src,rule){if(srcIsI2C(src)) writes++;});
  return writes;
}

function cpuLimit(fps){
  const frameUs=Math.floor(1000000/(parseInt(fps)||S.DEFAULT_OUTPUT_FPS));
  return frameUs>SAFETY_RESERVE_US?frameUs-SAFETY_RESERVE_US:Math.floor(frameUs/2);
}
function totalHardware(arr){
  const t={ledc:0,rmt:0,uart:0,dac:0,timer:0};
  let hasDimmer=false, dmxCount=0, dfPlayerCount=0;
  arr.forEach(o=>{
    const h=channelHardware(o);
    t.ledc+=h.ledc;t.rmt+=h.rmt;t.uart+=h.uart;t.dac+=h.dac;t.timer+=h.timer;
    const typ=typeId(o);
    if(typ===T.DIMMER) hasDimmer=true;
    if(typ===T.DMX&&parseInt(o.source||0)===0) dmxCount++;
    else if(typ===T.DFPLAYER) dfPlayerCount++;
  });
  const freeUarts=Math.max(0,S.MAX_UART-dfPlayerCount);
  const dmxUartUse=Math.min(dmxCount,freeUarts);
  const dmxRmtUse=Math.max(0,dmxCount-freeUarts);
  t.uart=dfPlayerCount+dmxUartUse;
  t.rmt+=dmxRmtUse;
  if(hasDimmer) t.timer+=S.AC_DIMMER_TIMER_COUNT;
  return t;
}
function frameTimeUs(fps){ return Math.floor(1000000/(parseInt(fps)||S.DEFAULT_OUTPUT_FPS)); }
function acDimmerBackgroundUs(count,fps){ return count>0?Math.floor(frameTimeUs(fps)/S.DIMMER_TICK_US)*(1+count):0; }
function funcGenBackgroundUs(count,fps){ return count>0?Math.floor(frameTimeUs(fps)/S.FUNCGEN_MIN_PERIOD_US)*S.FUNCGEN_ISR_US*count:0; }
function totalCpu(arr,fps){
  let us = BASE_OVERHEAD_US;
  let dimmerCount=0, funcGenCount=0;
  arr.forEach(o=>{
    us+=channelCost(o).cpu;
    if(typeId(o)===T.DIMMER) dimmerCount++;
    else if(typeId(o)===T.FUNC_GEN) funcGenCount++;
  });
  us+=acDimmerBackgroundUs(dimmerCount,fps)+funcGenBackgroundUs(funcGenCount,fps);

  const modeVal = parseInt(document.getElementById('device_mode')?.value || 0);
  if (modeVal === 1) {
    const unis = new Set(arr.map(o => parseInt(o.start_universe || 0)));
    const peerCount = Math.max(1, espPeers.length);
    const chunkSize = parseInt(document.getElementById('espnow_chunk_size')?.value) || S.DMX_BUFFER_BYTES;
    us += espnowMasterCost(peerCount, unis.size, chunkSize).cpu;
  }
  return us;
}
function totalRam(arr){
  let b=0, dmxCount=0, dfPlayerCount=0;
  arr.forEach(o=>{
    b+=channelCost(o).ram;
    if(typeId(o)===T.DMX&&parseInt(o.source||0)===0) dmxCount++;
    else if(typeId(o)===T.DFPLAYER) dfPlayerCount++;
  });
  const freeUarts=Math.max(0,S.MAX_UART-dfPlayerCount);
  const dmxRmtUse=Math.max(0,dmxCount-freeUarts);
  let ramVal = b+dmxRmtUse*S.RMT_DMX_DRIVER_RAM;

  const modeVal = parseInt(document.getElementById('device_mode')?.value || 0);
  if (modeVal === 1) {
    const peerCount = Math.max(1, espPeers.length);
    const chunkSize = parseInt(document.getElementById('espnow_chunk_size')?.value) || S.DMX_BUFFER_BYTES;
    ramVal += espnowMasterCost(peerCount, 0, chunkSize).ram;
  }
  return ramVal;
}
function hwBlocked(hw){
  return hw.ledc>HW_MAX.ledc||hw.rmt>HW_MAX.rmt||hw.uart>HW_MAX.uart||hw.dac>HW_MAX.dac||hw.timer>HW_MAX.timer;
}
function cpuBlocked(cpu,fps){
  return cpu>cpuLimit(fps);
}
function ramBlocked(ram){
  return ram>RAM_LIMIT;
}
function espnowMasterCost(peerCount, universeCount, chunkSize){
  if(!chunkSize) chunkSize=S.DMX_BUFFER_BYTES;
  const cpusPerUni=Math.ceil(S.DMX_BUFFER_BYTES/chunkSize);
  return {cpu:S.ESPNOW_BASE_US+peerCount*cpusPerUni*S.ESPNOW_PER_CHUNK_US+universeCount*S.ESPNOW_PER_UNIVERSE_US, ram:S.DMX_BUFFER_BYTES+peerCount*(chunkSize+S.ESPNOW_OVERHEAD_BYTES)};
}
function updateScoreBar(){
  const cpuBar=document.getElementById('cpu-score-fill');
  const cpuTxt=document.getElementById('cpu-score-text');
  const ramBar=document.getElementById('ram-score-fill');
  const ramTxt=document.getElementById('ram-score-text');
  if(!cpuBar||!cpuTxt||!ramBar||!ramTxt) return;
  const fps=parseInt(document.getElementById('output_fps')?.value)||S.DEFAULT_OUTPUT_FPS;
  const hw=totalHardware(outputs);
  const cpu=totalCpu(outputs,fps);
  const ram=totalRam(outputs);
  const cLim=cpuLimit(fps);

  const hwBad=hwBlocked(hw);
  const cpuBad=cpuBlocked(cpu,fps);
  const ramBad=ramBlocked(ram);
  const budgetBad=hwBad||cpuBad||ramBad;

  const cpuPct=Math.min(cpu/cLim*100,100);
  cpuBar.style.width=cpuPct+'%';
  cpuBar.style.background=cpuBad?'#ef4444':cpuPct>80?'#eab308':'#22c55e';
  cpuTxt.textContent=cpuPct.toFixed(1)+'% ('+cpu+' / '+cLim+' \u00B5s)';

  const ramPct=Math.min(ram/RAM_LIMIT*100,100);
  ramBar.style.width=ramPct+'%';
  ramBar.style.background=ramBad?'#ef4444':ramPct>80?'#eab308':'#22c55e';
  ramTxt.textContent=ramPct.toFixed(1)+'% ('+(ram/1024).toFixed(3)+' KB / '+(RAM_LIMIT/1024).toFixed(3)+' KB)';

  const updateHwCard = (valId, barId, used, maxLimit) => {
    const valEl = document.getElementById(valId);
    const barEl = document.getElementById(barId);
    if (!valEl || !barEl) return;
    valEl.textContent = `${used} / ${maxLimit}`;
    const pct = Math.min(used / maxLimit * 100, 100);
    barEl.style.width = pct + '%';
    if (used > maxLimit) {
      valEl.style.color = '#ef4444';
      barEl.style.background = '#ef4444';
    } else if (used === maxLimit) {
      valEl.style.color = '#eab308';
      barEl.style.background = '#eab308';
    } else {
      valEl.style.color = '#1e3a8a';
      barEl.style.background = '#22c55e';
    }
  };
  updateHwCard('hw-ledc-val','hw-ledc-bar',hw.ledc,HW_MAX.ledc);
  updateHwCard('hw-rmt-val','hw-rmt-bar',hw.rmt,HW_MAX.rmt);
  updateHwCard('hw-uart-val','hw-uart-bar',hw.uart,HW_MAX.uart);
  updateHwCard('hw-dac-val','hw-dac-bar',hw.dac,HW_MAX.dac);
  updateHwCard('hw-timer-val','hw-timer-bar',hw.timer,HW_MAX.timer);

  const saveBtn = document.getElementById('out-save-btn');
  if (saveBtn) saveBtn.disabled = budgetBad;

  if (budgetBad) {
    let errMsg = hwBad ? 'Hardware Resource Exceeded' : (cpuBad ? 'CPU Budget Exceeded' : 'RAM Budget Exceeded');
    setSaveState(errMsg, 'err');
  } else {
    setSaveState(outputsDirty ? 'Unsaved changes' : 'Saved', outputsDirty ? 'warn' : 'ok');
  }
}
