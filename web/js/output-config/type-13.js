const MODE_OPTS_7SEG_13=[[3,'Direct 8-pin (1 Ch - No Dim)'],[5,'Direct 8-pin Dim (2 Ch - Char+Dim)'],[8,'CA 8-pin Dim (2 Ch - COM Anode)'],[9,'CC 8-pin Dim (2 Ch - COM Cathode)']];
const CONFIG_TYPE_13 = {
  modeKey: function(mode) {
    if(mode===4||mode===5) return 'directDim';
    if(mode>=6&&mode<=9) return 'commonDim';
    return String(mode);
  },
  toggleFields: function() {
    setModeOptions(MODE_OPTS_7SEG_13, 'Display Mode');
    setResolutionOptions(RES_OPTS_7SEG);
    const mcMode = parseInt(document.getElementById('mc_mode').value || 3);
    const isDirectDim = (mcMode === 4 || mcMode === 5);
    const isCommonDim = (mcMode >= 6 && mcMode <= 9);
    document.getElementById('mc_mode_grp').style.display = '';
    document.getElementById('mc_res_grp').style.display = '';
    document.getElementById('mc_resolution_lbl').textContent = 'Decode Mode';
    document.getElementById('mc_freq_grp').style.display = (isDirectDim || isCommonDim) ? '' : 'none';
    document.getElementById('mc_freq_lbl').textContent = 'PWM Frequency (Hz)';
  },
  loadFields: function(o) {
    document.getElementById('mc_mode').value = o.mc_mode || 3;
    document.getElementById('mc_resolution').value = o.mc_resolution || 8;
    document.getElementById('mc_freq').value = o.mc_freq || 1000;
  },
  saveFields: function(ch) {
    ch.mc_mode = parseInt(document.getElementById('mc_mode').value);
    ch.mc_resolution = parseInt(document.getElementById('mc_resolution').value);
    ch.mc_freq = parseInt(document.getElementById('mc_freq').value);
    return ch;
  },
  channelCount: function(o) {
    const m=parseInt(o.mc_mode||0);
    if(m===4||m===5||m===6||m===7||m===8||m===9) return '2 Ch (Char+Dim)';
    return '1 Ch (Direct)';
  },
  byteCount: function(o) {
    const m=parseInt(o.mc_mode||0);
    if(m===4||m===5||m===6||m===7||m===8||m===9) return 2;
    return 1;
  },
  segmentPinLayout: function(mcMode) { return mcMode>=2&&mcMode<=9; },
  configLabel: function(o) { return '7-Seg DD 8-Pin PWM'; },
  gpioExtract: function(o) {
    const t=13; const mcMode=parseInt(o.mc_mode||0);
    if(!(mcMode>=2&&mcMode<=9)) return [];
    const pins=[];
    const add=p=>{p=parseInt(p); if(!isNaN(p)&&p!==255&&!pins.includes(p)) pins.push(p);};
    if(parseInt(o.source||0)===0) add(o.pin);
    if(parseInt(o.pin2_source||0)===0){
      const numSeg=8; const isCommonDim=(mcMode>=6&&mcMode<=9);
      const startIdx=isCommonDim?0:1;
      const sps=o.seg_pins||[]; const ssrc=o.seg_sources||[];
      for(let s=startIdx;s<numSeg;s++){
        if(parseInt(ssrc[s]||0)!==0) continue;
        const sp=(sps[s]!==undefined&&sps[s]!==255)?sps[s]:(parseInt(o.pin)+s);
        add(sp);
      }
    }
    return pins;
  },
  autoAssignPins: function({takeOutput,takeInput}) {
    const mcMode=parseInt(document.getElementById('mc_mode').value);
    const pin2Source=parseInt(document.getElementById('no_pin2_source')?.value||0);
    if(pin2Source===0){
      const numSeg=8; const startSeg=(mcMode>=6&&mcMode<=9)?0:1;
      const basePin=takeOutput();
      setSelectIfOption('no_pin',basePin);
      if(basePin!==255){for(let s=startSeg;s<numSeg;s++) setSelectIfOption('no_seg_pin_'+s,takeOutput());}
    }
  }
};
