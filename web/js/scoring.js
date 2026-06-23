// ==========================================
// === RESOURCE SCORING & BUDGET CHECKS ===
// ==========================================
const SCORE_DEFS={
  hwMax:{ledc:16,rmt:8,uart:2,dac:2,timer:4},
  cpu:{baseOverheadUs:500,safetyReserveUs:1500,i2cWriteUs:180,dmxOutputServiceUs:250},
  ram:{limit:48000,baseChannel:224,dmxBuffer:512,pixelStripObject:256,dfPlayerObject:160,stepperRuntime:512,funcGenObject:1120,rmtDmxDriver:5150*4+32,i2cRoute:32}
};
const HW_MAX=SCORE_DEFS.hwMax;
const RAM_LIMIT=SCORE_DEFS.ram.limit;
const BASE_CHANNEL_RAM=SCORE_DEFS.ram.baseChannel;
const BASE_OVERHEAD_US=SCORE_DEFS.cpu.baseOverheadUs;
const SAFETY_RESERVE_US=SCORE_DEFS.cpu.safetyReserveUs;
const I2C_WRITE_US=SCORE_DEFS.cpu.i2cWriteUs;
const DMX_BUFFER_RAM=SCORE_DEFS.ram.dmxBuffer;
const PIXEL_STRIP_OBJECT_RAM=SCORE_DEFS.ram.pixelStripObject;
const DFPLAYER_OBJECT_RAM=SCORE_DEFS.ram.dfPlayerObject;
const STEPPER_RUNTIME_RAM=SCORE_DEFS.ram.stepperRuntime;
const FUNCGEN_OBJECT_RAM=SCORE_DEFS.ram.funcGenObject;
const RMT_DMX_DRIVER_RAM=SCORE_DEFS.ram.rmtDmxDriver;
const I2C_ROUTE_RAM=SCORE_DEFS.ram.i2cRoute;
const DMX_OUTPUT_SERVICE_US=SCORE_DEFS.cpu.dmxOutputServiceUs;

function channelHardware(o){
  const t=typeId(o);
  let ledc=0,rmt=0,uart=0,dac=0,timer=0;
  switch(t){
    case 0: break;
    case 1: uart=1; break;
    case 2: break;
    case 3: rmt=1; break;
    case 4: eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;}); break;
    case 5: eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;}); break;
    case 6: eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;}); break;
    case 7: rmt=2; break;
    case 8: eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;}); break;
    case 9: eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;}); break;
    case 10: uart=1; break;
    case 11: break;
    case 12:
    case 13:
      if((o.mc_mode||0)>=6&&(o.mc_mode||0)<=9){
        eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;});
        break;
      }
      eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;});
      break;
    case 14: if(!(o.source>=5&&o.source<=7)) dac=1; break;
    case 15: eachSlot(o,function(s,pin,src,rule){if(src===0&&rule.hwIfGpio) ledc+=rule.hwIfGpio;}); break;
    case 16: ledc=1; timer=1; break;
    case 17: break;
    case 18: break;
  }
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
  const t=typeId(o);
  const ledCount=parseInt(o.led_count||0);
  let cpu=outputModeCpuUs(t,parseInt(o.mc_mode||0)), ram=BASE_CHANNEL_RAM;
  const i2cWrites=i2cWriteCount(o);
  ram+=dmxBufferRam(t,ledCount,parseInt(o.color_order||0),parseInt(o.mc_resolution||8),parseInt(o.mc_mode||0));
  if(t===3) cpu=ledStripServiceUs(ledCount, parseInt(o.color_order||0));
  if(t===3) ram+=pixelBufferRam(ledCount,parseInt(o.color_order||0))+PIXEL_STRIP_OBJECT_RAM;
  if(t===7) ram+=STEPPER_RUNTIME_RAM;
  if(t===10) ram+=DFPLAYER_OBJECT_RAM+100;
  if(t===16) ram+=FUNCGEN_OBJECT_RAM;
  cpu+=i2cWrites*I2C_WRITE_US;
  ram+=i2cWrites*I2C_ROUTE_RAM;
  return {cpu,ram};
}

function dmxBufferRam(type,ledCount,colorOrder,resolution=8,mode=0){
  type=parseInt(type); mode=parseInt(mode||0);
  if(type===1) return DMX_BUFFER_RAM;
  if(type===3){
    const bytesPerPixel=parseInt(colorOrder||0)>=4?4:3;
    const pixelsPerUniverse=Math.floor(512/bytesPerPixel);
    return Math.max(1,Math.ceil((parseInt(ledCount)||0)/pixelsPerUniverse))*512;
  }
  if(type===5) return parseInt(colorOrder||0)>=4?4:3;
  if(type===7) return valueByteCount(resolution)+2;
  if(type===9||type===11) return mode===1?4:2;
  if(type===12||type===13) return (mode===4||mode===5||mode>=6)?2:1;
  if(type===10) return 3;
  if(type===16) return 5;
  if([4,6,8,15].includes(type)) return valueByteCount(resolution);
  return 1;
}

function pixelBufferRam(ledCount,colorOrder){ return (parseInt(ledCount)||0)*(parseInt(colorOrder||0)>=4?4:3); }

function ledStripServiceUs(ledCount,colorOrder){
  const bytesPerPixel=parseInt(colorOrder||0)>=4?4:3;
  return 80+ledCount*(bytesPerPixel===4?4:3);
}

function sevenSegCount(o){ return typeId(o)===13?8:(typeId(o)===12?7:0); }

function i2cWriteCount(o){
  var t=typeId(o), mode=parseInt(o.mc_mode||0);
  var si=function(s){return srcIsI2C(parseInt(s)||0);};
  if([2,17,4,8,15,14].includes(t)){
    var writes=0;
    eachSlot(o,function(slot,pin,src,rule){if(si(src)) writes++;});
    return writes;
  }
  if(t===5){
    var writes=0;
    eachSlot(o,function(slot,pin,src,rule){if(si(src)) writes++;});
    return writes;
  }
  if(t===6){
    var writes=0;
    if(si(o.source||0)){writes+=mode===2?3:2;}
    else{eachSlot(o,function(slot,pin,src,rule){
      if(slot!=='pin1'&&si(src)) writes++;
    });}
    return writes;
  }
  if(t===7){
    var writes=0;
    eachSlot(o,function(slot,pin,src,rule){if(slot!=='pin1'&&si(src)) writes++;});
    return writes;
  }
  if(t===12||t===13){
    var writes=0;
    var n=sevenSegCount(o);
    if(mode>=2&&si(o.pin2_source)){writes+=n;}
    else{eachSlot(o,function(slot,pin,src,rule){
      if(slot!=='pin1'&&si(src)) writes++;
    });}
    if(mode>=6&&mode<=9&&si(o.source||0)) writes++;
    return writes;
  }
  if(t===18){
    var writes=0;
    eachSlot(o,function(slot,pin,src,rule){if(si(src)) writes++;});
    return writes;
  }
  return 0;
}

function cpuLimit(fps){
  const frameUs=Math.floor(1000000/(parseInt(fps)||40));
  return frameUs>SAFETY_RESERVE_US?frameUs-SAFETY_RESERVE_US:Math.floor(frameUs/2);
}
function totalHardware(arr){
  const t={ledc:0,rmt:0,uart:0,dac:0,timer:0};
  let hasDimmer=false, dmxCount=0, dfPlayerCount=0;
  arr.forEach(o=>{
    const h=channelHardware(o);
    t.ledc+=h.ledc;t.rmt+=h.rmt;t.uart+=h.uart;t.dac+=h.dac;t.timer+=h.timer;
    const typ=typeId(o);
    if(typ===0) hasDimmer=true;
    if(typ===1&&parseInt(o.source||0)===0) dmxCount++;
    else if(typ===10) dfPlayerCount++;
  });
  const freeUarts=Math.max(0,2-dfPlayerCount);
  const dmxUartUse=Math.min(dmxCount,freeUarts);
  const dmxRmtUse=Math.max(0,dmxCount-freeUarts);
  t.uart=dfPlayerCount+dmxUartUse;
  t.rmt+=dmxRmtUse;
  if(hasDimmer) t.timer+=1;
  return t;
}
function frameTimeUs(fps){ return Math.floor(1000000/(parseInt(fps)||40)); }
function acDimmerBackgroundUs(count,fps){ return count>0?Math.floor(frameTimeUs(fps)/39)*(1+count):0; }
function funcGenBackgroundUs(count,fps){ return count>0?Math.floor(frameTimeUs(fps)/50)*4*count:0; }
function totalCpu(arr,fps){
  let us = BASE_OVERHEAD_US;
  let dimmerCount=0, funcGenCount=0;
  arr.forEach(o=>{
    us+=channelCost(o).cpu;
    if(typeId(o)===0) dimmerCount++;
    else if(typeId(o)===16) funcGenCount++;
  });
  us+=acDimmerBackgroundUs(dimmerCount,fps)+funcGenBackgroundUs(funcGenCount,fps);

  const modeVal = parseInt(document.getElementById('device_mode')?.value || 0);
  if (modeVal === 1) {
    const unis = new Set(arr.map(o => parseInt(o.start_universe || 0)));
    const peerCount = Math.max(1, espPeers.length);
    const chunkSize = parseInt(document.getElementById('espnow_chunk_size')?.value || 200);
    us += espnowMasterCost(peerCount, unis.size, chunkSize).cpu;
  }
  return us;
}
function totalRam(arr){
  let b=0, dmxCount=0, dfPlayerCount=0;
  arr.forEach(o=>{
    b+=channelCost(o).ram;
    if(typeId(o)===1&&parseInt(o.source||0)===0) dmxCount++;
    else if(typeId(o)===10) dfPlayerCount++;
  });
  const freeUarts=Math.max(0,2-dfPlayerCount);
  const dmxRmtUse=Math.max(0,dmxCount-freeUarts);
  let ramVal = b+dmxRmtUse*RMT_DMX_DRIVER_RAM;

  const modeVal = parseInt(document.getElementById('device_mode')?.value || 0);
  if (modeVal === 1) {
    const peerCount = Math.max(1, espPeers.length);
    const chunkSize = parseInt(document.getElementById('espnow_chunk_size')?.value || 200);
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
function espnowMasterCost(peerCount, universeCount, chunkSize=200){
  const cpusPerUni = Math.ceil(512/chunkSize);
  return {cpu: 500 + peerCount*cpusPerUni*170 + universeCount*100, ram: 512 + peerCount*(chunkSize+44)};
}
function updateScoreBar(){
  const cpuBar=document.getElementById('cpu-score-fill');
  const cpuTxt=document.getElementById('cpu-score-text');
  const ramBar=document.getElementById('ram-score-fill');
  const ramTxt=document.getElementById('ram-score-text');
  if(!cpuBar||!cpuTxt||!ramBar||!ramTxt) return;
  const fps=parseInt(document.getElementById('output_fps')?.value)||40;
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
