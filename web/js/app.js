// ==========================================
// === GLOBAL STATE, TABS, SETTINGS, OTA ===
// ==========================================
let diagTimer = null;
function showTab(e,id){
  document.querySelectorAll('.pane').forEach(p=>p.classList.remove('on'));
  document.querySelectorAll('.tb').forEach(b=>b.classList.remove('on'));
  document.getElementById('pane-'+id).classList.add('on');
  e.currentTarget.classList.add('on');
  if(diagTimer) { clearInterval(diagTimer); diagTimer = null; }
  if(id==='diag') {
    diagRefreshSysInfo();
    diagTimer = setInterval(diagRefreshSysInfo, 5000);
  }
}

function startRebootCountdown(msg, seconds){
  const overlay = document.getElementById('reboot-overlay');
  const textVal = document.getElementById('reboot-msg');
  const countVal = document.getElementById('reboot-countdown');
  if(!overlay || !textVal || !countVal) return;
  textVal.textContent = msg;
  countVal.textContent = seconds;
  overlay.style.display = 'flex';
  const interval = setInterval(()=>{
    seconds--;
    countVal.textContent = seconds;
    if(seconds <= 0){
      clearInterval(interval);
      location.reload();
    }
  }, 1000);
}

// Diagnostics
async function diagRefreshSysInfo(){
  try{
    const res=await fetch('/api/status');
    const d=await res.json();
    document.getElementById('diag-heap').textContent=(d.heap_free>>10)+' KB';
    document.getElementById('diag-minheap').textContent=(d.min_heap>>10)+' KB';
    document.getElementById('diag-cpu').textContent=d.cpu_freq+' MHz';
    document.getElementById('diag-mac').textContent=d.board_mac;
    document.getElementById('diag-eth-status').innerHTML=d.eth_up?'<span style="color:#16a34a">Connected</span>':'<span style="color:#94a3b8">Down</span>';
    document.getElementById('diag-eth-ip').textContent=d.eth_ip||(d.eth_up?d.ip:'-');
    document.getElementById('diag-wifi-status').innerHTML=d.wifi_up?'<span style="color:#16a34a">Connected</span>':'<span style="color:#94a3b8">Down</span>';
    document.getElementById('diag-wifi-ip').textContent=d.wifi_ip||(d.wifi_up?d.ip:'-');
    document.getElementById('diag-ap-status').innerHTML=d.ap_up?'<span style="color:#16a34a">Active</span>':'<span style="color:#94a3b8">Off</span>';
    document.getElementById('diag-ap-ip').textContent=d.ap_ip||(d.ap_up?d.ip:'-');
  }catch(e){
    document.getElementById('diag-heap').textContent='Error';
  }
}

const I2C_DEVICES={
  '0x20':'MCP23017 / TCA9555 / PCF8574',
  '0x21':'MCP23017 / TCA9555 / PCF8574',
  '0x22':'MCP23017 / TCA9555 / PCF8574',
  '0x23':'MCP23017 / TCA9555 / PCF8574',
  '0x24':'MCP23017 / TCA9555 / PCF8574',
  '0x25':'MCP23017 / TCA9555 / PCF8574',
  '0x26':'MCP23017 / TCA9555 / PCF8574',
  '0x27':'MCP23017 / TCA9555 / PCF8574',
  '0x3C':'SSD1306 OLED',
  '0x3D':'SSD1306 OLED',
  '0x3F':'PCF8574 LCD Backpack',
  '0x40':'PCA9685 PWM Driver',
  '0x41':'PCA9685 PWM Driver',
  '0x42':'PCA9685 PWM Driver',
  '0x43':'PCA9685 PWM Driver',
  '0x44':'PCA9685 PWM Driver',
  '0x45':'PCA9685 PWM Driver',
  '0x46':'PCA9685 PWM Driver',
  '0x47':'PCA9685 PWM Driver',
  '0x48':'ADS1115 / PCF8591',
  '0x49':'ADS1115 / PCF8591',
  '0x4A':'ADS1115 / PCF8591',
  '0x4B':'ADS1115 / PCF8591',
  '0x50':'AT24C EEPROM',
  '0x51':'AT24C EEPROM',
  '0x57':'AT24C EEPROM',
  '0x68':'DS3231 RTC / MPU6050',
  '0x76':'BME280 / BMP280',
  '0x77':'BME280 / BMP280',
};
async function scanI2c(){
  const resultDiv=document.getElementById('i2c-scan-result');
  resultDiv.innerHTML='<span style="color:#475569">Scanning...</span>';
  try{
    const res=await fetch('/api/i2c-scan?refresh=1');
    const data=await res.json();
    if(data.status==='scanning'){
      setTimeout(scanI2c,500);
      return;
    }
    if(!Array.isArray(data)||data.length===0){
      resultDiv.innerHTML='<span style="color:#94a3b8">No I2C devices found. Check wiring and pull-up resistors.</span>';
      return;
    }
    let html='<table><tr><th>Address</th><th>Hex</th><th>Guessed Device</th><th>Used By Output</th></tr>';
    for(const d of data){
      const guess=I2C_DEVICES[d.hex]||'Unknown device';
      const used=d.used_by||'-';
      html+=`<tr><td>${d.address}</td><td><code>${d.hex}</code></td><td>${guess}</td><td>${used}</td></tr>`;
    }
    html+=`</table><div class="hint">Found ${data.length} device(s)</div>`;
    resultDiv.innerHTML=html;
  }catch(e){
    resultDiv.innerHTML='<span style="color:#dc2626">Scan failed: '+e.message+'</span>';
  }
}

function showAlert(ok){
  const el=document.getElementById(ok?'alert-ok':'alert-err');
  el.style.display='block';
  setTimeout(()=>el.style.display='none',3500);
}

function validateIp4(s){
  if(!s||s==='') return true;
  const parts=s.split('.');
  if(parts.length!==4) return false;
  for(const p of parts){
    const n=parseInt(p,10);
    if(isNaN(n)||n<0||n>255||String(n)!==p) return false;
  }
  return true;
}

// Telemetry
async function updateTelemetry(){
  try{
    const d=await(await fetch('/api/status')).json();
    const g=id=>document.getElementById(id);
    if(g('tel-ip'))       g('tel-ip').textContent=d.ip||'--';
    if(g('tel-packets'))  g('tel-packets').textContent=d.packets_received??'--';
    if(g('tel-heap'))     g('tel-heap').textContent=d.heap_free?Math.round(d.heap_free/1024)+' KB':'--';
    if(g('tel-cpu'))      g('tel-cpu').textContent=d.cpu_freq?d.cpu_freq+' MHz':'--';
    if(g('tel-time'))     g('tel-time').textContent=d.time||'--';
    const eth=g('net-eth'),wifi=g('net-wifi'),ap=g('net-ap');
    if(eth){eth.textContent=d.eth_up?'ETH ON':'ETH OFF';eth.style.background=d.eth_up?'#166534':'#991b1b';}
    if(wifi){wifi.textContent=d.wifi_up?'WiFi ON':'WiFi OFF';wifi.style.background=d.wifi_up?'#166534':'#991b1b';}
    if(ap){ap.textContent=d.ap_up?'AP ON':'AP OFF';ap.style.background=d.ap_up?'#854d0e':'rgba(255,255,255,.15)';}
    if(g('board_mac_display')&&d.board_mac) g('board_mac_display').textContent=d.board_mac;
  }catch(e){}
}

// Network form toggles
function toggleEth(){
  const dhcp=document.getElementById('eth_dhcp').checked;
  document.querySelectorAll('#eth-static input').forEach(i=>i.disabled=dhcp);
}
function toggleWifiIp(){
  const dhcp=document.getElementById('wifi_dhcp').checked;
  document.querySelectorAll('#wifi-static input').forEach(i=>i.disabled=dhcp);
}
function updateMdnsPreview(){
  const v=document.getElementById('mdns_name').value||'artnet';
  document.getElementById('mdns_preview').textContent=v;
}
function toggleI2c(){
  const en=document.getElementById('i2c_enabled').checked;
  document.getElementById('i2c-config').style.display=en?'':'none';
}
function toggleDisplayCfg(){
  const en=document.getElementById('disp_enable_ck').checked;
  document.getElementById('display-config').style.display=en?'':'none';
  if(!en) document.getElementById('display_enabled').value=0;
  updateDisplayAddressOptions();
}

function updateDisplayAddressOptions(){
  const type=parseInt(document.getElementById('display_enabled')?.value||0);
  const sel=document.getElementById('display_i2c_addr');
  if(!sel) return;
  const current=parseInt(sel.value||60);
  const opts=(type===3)
    ? [[0x27,'0x27 (LCD default)'],[0x3F,'0x3F (LCD alt)']]
    : [[0x3C,'0x3C (OLED default)'],[0x3D,'0x3D']];
  sel.innerHTML=opts.map(([v,l])=>`<option value="${v}">${l}</option>`).join('');
  sel.value=opts.some(([v])=>v===current)?current:opts[0][0];
}

// Settings load / save
async function loadSettings(){
  try{
    const d=await(await fetch('/api/settings')).json();
    const s=(id,val)=>{const e=document.getElementById(id);if(e&&val!==undefined)e.value=val};
    const c=(id,val)=>{const e=document.getElementById(id);if(e)e.checked=!!val};
    s('device_mode',d.device_mode);
    s('espnow_channel',d.espnow_channel);
    s('espnow_chunk_size',d.espnow_chunk_size);
    s('mdns_name',d.mdns_name);
    const fw=document.getElementById('firmware_version');
    if(fw) fw.textContent=d.firmware_version||'Unknown';
    s('artnet_port',d.artnet_port);
    s('sacn_port',d.sacn_port);
    c('artnet_enabled',d.artnet_enabled);
    c('sacn_enabled',d.sacn_enabled); c('sacn_multicast',d.sacn_multicast);
    c('eth_dhcp',d.eth_dhcp);
    s('eth_ip',d.eth_ip); s('eth_netmask',d.eth_netmask); s('eth_gateway',d.eth_gateway); s('eth_dns',d.eth_dns);
    s('wifi_ssid',d.wifi_ssid); s('wifi_pass',d.wifi_pass);
    c('wifi_dhcp',d.wifi_dhcp);
    s('wifi_ip',d.wifi_ip); s('wifi_netmask',d.wifi_netmask); s('wifi_gateway',d.wifi_gateway); s('wifi_dns',d.wifi_dns);
    c('wifi_enable_in_eth_mode',d.wifi_enable_in_eth_mode);
    c('ap_enable_in_eth_mode',d.ap_enable_in_eth_mode);
    s('ap_ssid',d.ap_ssid); s('ap_pass',d.ap_pass);
    s('output_fps',d.output_fps);
    s('status_led_pin',d.status_led_pin);
    s('zc_pin',d.zc_pin);
    s('i2c_sda',d.i2c_sda);
    s('i2c_scl',d.i2c_scl);
    s('i2c_speed',d.i2c_speed);
    const i2cEn=(d.i2c_sda!==255&&d.i2c_scl!==255);
    c('i2c_enabled',i2cEn);
    toggleI2c();
    s('display_enabled',d.display_enabled);
    const dispEn=(d.display_enabled!==0);
    c('disp_enable_ck',dispEn);
    toggleDisplayCfg();
    s('display_i2c_addr',d.display_i2c_addr);
    updateDisplayAddressOptions();
    s('display_brightness',d.display_brightness);
    toggleEth(); toggleWifiIp(); updateMdnsPreview();
    autoAssignOutputPins();
    document.getElementById('mdns_name').addEventListener('input',updateMdnsPreview);
    ['status_led_pin', 'zc_pin', 'i2c_sda', 'i2c_scl'].forEach(id => {
      document.getElementById(id)?.addEventListener('change', checkStrappingPin);
    });
    checkStrappingPin();
  }catch(e){console.error('loadSettings',e);}
}

async function saveSettings(e){
  e.preventDefault();
  const fd=new FormData(e.target||document.getElementById('cfgForm'));
  const obj={};
  fd.forEach((v,k)=>{
    const boolKeys=[NET_PROTO.KEY_ETH_DHCP,NET_PROTO.KEY_WIFI_DHCP,NET_PROTO.KEY_ARTNET_ENABLED,NET_PROTO.KEY_SACN_ENABLED,NET_PROTO.KEY_SACN_MULTICAST,NET_PROTO.KEY_WIFI_IN_ETH_MODE,NET_PROTO.KEY_AP_IN_ETH_MODE];
    const intKeys=[NET_PROTO.KEY_DEVICE_MODE,NET_PROTO.KEY_ESPNOW_CHANNEL,NET_PROTO.KEY_ESPNOW_CHUNK_SIZE,NET_PROTO.KEY_STATUS_LED_PIN,NET_PROTO.KEY_ZC_PIN,NET_PROTO.KEY_ETH_DHCP,NET_PROTO.KEY_WIFI_DHCP,
                   NET_PROTO.KEY_ARTNET_PORT,NET_PROTO.KEY_SACN_PORT,
                   NET_PROTO.KEY_OUTPUT_FPS,NET_PROTO.KEY_I2C_SDA,NET_PROTO.KEY_I2C_SCL,NET_PROTO.KEY_I2C_SPEED,
                   NET_PROTO.KEY_DISPLAY_ENABLED,NET_PROTO.KEY_DISPLAY_I2C_ADDR,NET_PROTO.KEY_DISPLAY_BRIGHTNESS];
    if(boolKeys.includes(k)) obj[k]=true;
    else if(intKeys.includes(k))
      obj[k]=parseInt(v,10);
    else obj[k]=v;
  });
  if(!document.getElementById('i2c_enabled').checked){ obj[NET_PROTO.KEY_I2C_SDA]=NET_PROTO.ZC_PIN_DISABLED; obj[NET_PROTO.KEY_I2C_SCL]=NET_PROTO.ZC_PIN_DISABLED; }
  if(!document.getElementById('disp_enable_ck').checked) obj[NET_PROTO.KEY_DISPLAY_ENABLED]=0;
  [NET_PROTO.KEY_ETH_DHCP,NET_PROTO.KEY_WIFI_DHCP,NET_PROTO.KEY_ARTNET_ENABLED,NET_PROTO.KEY_SACN_ENABLED,NET_PROTO.KEY_SACN_MULTICAST,NET_PROTO.KEY_WIFI_IN_ETH_MODE,NET_PROTO.KEY_AP_IN_ETH_MODE].forEach(k=>{
    if (e.target.querySelector('#' + k)) {
      if (!obj.hasOwnProperty(k)) obj[k] = false;
    }
  });
  if(!obj[NET_PROTO.KEY_ARTNET_ENABLED] && !obj[NET_PROTO.KEY_SACN_ENABLED]){
    alert('Cannot disable both Art-Net and sACN protocols.');
    return;
  }
  if(obj[NET_PROTO.KEY_DEVICE_MODE] === NET_PROTO.MODE_ESPNOW_MASTER && obj[NET_PROTO.KEY_ESPNOW_CHANNEL] === 0){
    alert('ESP-NOW Master cannot use Auto-Scan channel mode. Please select a fixed channel (1-13).');
    return;
  }
  if(!NET_PROTO.espnowChunkSizeValid(obj[NET_PROTO.KEY_ESPNOW_CHUNK_SIZE])){
    alert('ESP-NOW Chunk Size must be between ' + NET_PROTO.ESPNOW_CHUNK_SIZE_MIN + ' and ' + NET_PROTO.ESPNOW_CHUNK_SIZE_MAX + ' bytes.');
    return;
  }
  if(obj[NET_PROTO.KEY_STATUS_LED_PIN]!==NET_PROTO.ZC_PIN_DISABLED&&obj[NET_PROTO.KEY_ZC_PIN]!==NET_PROTO.ZC_PIN_DISABLED&&obj[NET_PROTO.KEY_STATUS_LED_PIN]===obj[NET_PROTO.KEY_ZC_PIN]){
    alert('Status LED GPIO and Zero-Crossing GPIO cannot use the same pin.');
    return;
  }
  const forbiddenSettingsPins=[['Status LED GPIO',obj[NET_PROTO.KEY_STATUS_LED_PIN]],['Zero-Crossing GPIO',obj[NET_PROTO.KEY_ZC_PIN]],['I2C SDA GPIO',obj[NET_PROTO.KEY_I2C_SDA]],['I2C SCL GPIO',obj[NET_PROTO.KEY_I2C_SCL]]];
  for(const [label,pin] of forbiddenSettingsPins){
    if(pin!==NET_PROTO.ZC_PIN_DISABLED&&FORBIDDEN_OUTPUT_GPIOS[pin]){
      alert(label+' '+pin+' is not allowed on WT32-ETH01 ('+FORBIDDEN_OUTPUT_GPIOS[pin]+'). Select another pin.');
      return;
    }
  }
  if(obj[NET_PROTO.KEY_I2C_SDA]!==NET_PROTO.ZC_PIN_DISABLED&&obj[NET_PROTO.KEY_I2C_SCL]!==NET_PROTO.ZC_PIN_DISABLED&&obj[NET_PROTO.KEY_I2C_SDA]===obj[NET_PROTO.KEY_I2C_SCL]){
    alert('I2C SDA and SCL pins cannot be the same.');
    return;
  }
  if(obj[NET_PROTO.KEY_STATUS_LED_PIN]!==NET_PROTO.ZC_PIN_DISABLED&&(obj[NET_PROTO.KEY_STATUS_LED_PIN]===obj[NET_PROTO.KEY_I2C_SDA]||obj[NET_PROTO.KEY_STATUS_LED_PIN]===obj[NET_PROTO.KEY_I2C_SCL])){
    alert('Status LED GPIO cannot overlap with I2C SDA or SCL.');
    return;
  }
  if(obj[NET_PROTO.KEY_ZC_PIN]!==NET_PROTO.ZC_PIN_DISABLED&&(obj[NET_PROTO.KEY_ZC_PIN]===obj[NET_PROTO.KEY_I2C_SDA]||obj[NET_PROTO.KEY_ZC_PIN]===obj[NET_PROTO.KEY_I2C_SCL])){
    alert('Zero-Crossing GPIO cannot overlap with I2C SDA or SCL.');
    return;
  }
  if(!displayAddressValid(obj[NET_PROTO.KEY_DISPLAY_ENABLED], obj[NET_PROTO.KEY_DISPLAY_I2C_ADDR])){
    alert('I2C display address is invalid for the selected display type. OLED uses 0x3C/0x3D; PCF8574 LCD uses 0x27/0x3F.');
    return;
  }
  const conflict=outputReservedPinConflict(outputs,obj[NET_PROTO.KEY_STATUS_LED_PIN]);
  if(conflict>=0){
    alert('Status LED GPIO '+obj[NET_PROTO.KEY_STATUS_LED_PIN]+' is already used by output channel '+(conflict+1)+'. Disable Status LED or select another GPIO.');
    return;
  }
  const zcConflict=outputReservedPinConflict(outputs,obj[NET_PROTO.KEY_ZC_PIN]);
  if(zcConflict>=0){
    alert('Zero-Crossing GPIO '+obj[NET_PROTO.KEY_ZC_PIN]+' is already used by output channel '+(zcConflict+1)+'. Disable Zero-Crossing or select another GPIO.');
    return;
  }
  const sdaConflict=outputReservedPinConflict(outputs,obj[NET_PROTO.KEY_I2C_SDA]);
  if(sdaConflict>=0){
    alert('I2C SDA GPIO '+obj[NET_PROTO.KEY_I2C_SDA]+' is already used by output channel '+(sdaConflict+1)+'. Select another GPIO.');
    return;
  }
  const sclConflict=outputReservedPinConflict(outputs,obj[NET_PROTO.KEY_I2C_SCL]);
  if(sclConflict>=0){
    alert('I2C SCL GPIO '+obj[NET_PROTO.KEY_I2C_SCL]+' is already used by output channel '+(sclConflict+1)+'. Select another GPIO.');
    return;
  }
  const ipFields=[NET_PROTO.KEY_ETH_IP,NET_PROTO.KEY_ETH_NETMASK,NET_PROTO.KEY_ETH_GATEWAY,NET_PROTO.KEY_ETH_DNS,NET_PROTO.KEY_WIFI_IP,NET_PROTO.KEY_WIFI_NETMASK,NET_PROTO.KEY_WIFI_GATEWAY,NET_PROTO.KEY_WIFI_DNS];
  for(const field of ipFields){
    if(!validateIp4(obj[field])){
      alert('Invalid IP address in '+field+'. Each octet must be 0-255.');
      return;
    }
  }
  const settingsBtn=e.target.querySelector('button[type="submit"]');
  if(settingsBtn){settingsBtn.disabled=true;settingsBtn.textContent='Saving...';}
  try{
    const res=await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(obj)});
    showAlert(res.ok);
    if(res.ok) startRebootCountdown('Saving settings and restarting...', 4);
  }catch(err){showAlert(false);}
  if(settingsBtn){settingsBtn.disabled=false;settingsBtn.textContent='Save & Restart';}
}

function setBackupStatus(text,kind='blue'){
  const el=document.getElementById('backup_status');
  if(!el) return;
  el.className='ib ib-'+kind;
  el.textContent=text;
}
function exportConfig(){
  setBackupStatus('Preparing configuration download...','blue');
  window.location.href='/api/config/backup';
}
async function resetOutputs(){
  if(!confirm('Reset ALL output channels to default (single LED strip)? This cannot be undone.')) return;
  try{
    const res=await fetch('/api/outputs/clear',{method:'POST'});
    const ok=res.ok;
    showAlert(ok);
    if(ok) startRebootCountdown('Resetting outputs and restarting...', 4);
  }catch(e){showAlert(false);}
}
async function factoryReset(){
  if(!confirm('FACTORY RESET: This will clear ALL settings and output channels and reboot. Are you sure?')) return;
  if(!confirm('Really? All WiFi, Ethernet, Art-Net, Dimmer, and output settings will be lost.')) return;
  try{
    const res=await fetch('/api/config/factory-reset',{method:'POST'});
    const ok=res.ok;
    showAlert(ok);
    if(ok) startRebootCountdown('Performing factory reset and restarting...', 6);
  }catch(e){showAlert(false);}
}
async function importConfigFile(input){
  const file=input.files&&input.files[0];
  input.value='';
  if(!file) return;
  let text='';
  try{
    text=await file.text();
    const parsed=JSON.parse(text);
    if(!parsed.settings||!Array.isArray(parsed.outputs)){
      throw new Error('Backup must contain settings and outputs.');
    }
    if(!confirm('Import this backup and replace current settings/output channels? The device will reboot.')) return;
    setBackupStatus('Importing configuration...','yellow');
    const res=await fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(parsed)});
    let msg='';
    try{const d=await res.json(); msg=d.message||'';}catch(e){}
    if(!res.ok){
      setBackupStatus('Import failed','red');
      alert(msg||'Import failed.');
      showAlert(false);
      return;
    }
    setBackupStatus('Import complete. Device is rebooting. This page will reload automatically.','green');
    showAlert(true);
    startRebootCountdown('Importing config and restarting...', 9);
  }catch(err){
    setBackupStatus('Import failed: invalid JSON file','red');
    alert(err.message||'Invalid JSON file.');
    showAlert(false);
  }
}

// WiFi Scan
let scanTimer=null;
async function startScan(){
  document.getElementById('scan_results').style.display='block';
  document.getElementById('scan_results').innerHTML='<option>Scanning...</option>';
  if(scanTimer) clearInterval(scanTimer);
  await fetch('/api/wifi-scan');
  scanTimer=setInterval(async()=>{
    try{
      const d=await(await fetch('/api/wifi-scan')).json();
      if(d.status==='scanning') return;
      clearInterval(scanTimer);
      const sel=document.getElementById('scan_results');
      sel.innerHTML='<option value="">-- Select network --</option>';
      (d.networks||[]).sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
        const opt=document.createElement('option');
        opt.value=n.ssid;
        opt.textContent=n.ssid+' ('+n.rssi+' dBm'+(n.secure?' secure':' open')+')';
        sel.appendChild(opt);
      });
    }catch(e){}
  },2000);
}
function pickSsid(){
  const v=document.getElementById('scan_results').value;
  if(v) document.getElementById('wifi_ssid').value=v;
}

// OTA
function otaFileSelected(){
  const input=document.getElementById('ota_file');
  const f=input.files[0];
  const btn=document.getElementById('ota-btn');
  const info=document.getElementById('ota-file-info');
  const status=document.getElementById('ota-status');
  if(f){
    const kb=Math.round(f.size/1024);
    info.textContent=f.name+' ('+kb+' KB)';
    btn.disabled=false;
    status.textContent='';
    status.style.color='#475569';
  } else {
    info.textContent='';
    btn.disabled=true;
  }
}

function flashOta(){
  const f=document.getElementById('ota_file').files[0];
  if(!f){alert('Select a .bin file first');return;}
  if(!confirm('Flash '+f.name+' now? Board will restart automatically after success.')) return;

  const btn=document.getElementById('ota-btn');
  const prog=document.getElementById('ota-prog');
  const status=document.getElementById('ota-status');
  btn.disabled=true;
  prog.style.display='block';
  prog.value=0;
  status.style.color='#475569';
  status.textContent='Uploading...';

  const xhr=new XMLHttpRequest();
  xhr.upload.onprogress=e=>{
    if(e.lengthComputable){
      const pct=Math.round(e.loaded/e.total*100);
      prog.value=pct;
      status.textContent='Uploading... '+pct+'%';
    }
  };
  xhr.onload=()=>{
    if(xhr.status===200){
      prog.value=100;
      status.textContent='Flash successful. Board is restarting... (reconnect in ~10s)';
      status.style.color='#166534';
      startRebootCountdown('OTA firmware flash complete. Restarting...', 12);
    } else {
      status.textContent='Flash failed: '+(xhr.responseText||'Try again.');
      status.style.color='#dc2626';
      btn.disabled=false;
    }
  };
  xhr.onerror=()=>{
    status.textContent='Upload error. Check network connection and try again.';
    status.style.color='#dc2626';
    btn.disabled=false;
  };
  xhr.open('POST','/update');
  const fd=new FormData();
  fd.append('update',f,f.name);
  xhr.send(fd);
}

window.addEventListener('DOMContentLoaded',()=>{
  loadSettings();
  loadOutputs();
  loadPeers();
  toggleOutFields();
  updateTelemetry();
  setInterval(updateTelemetry,10000);
  window.addEventListener('beforeunload',(e)=>{
    if(outputsDirty){e.preventDefault();e.returnValue='';}
  });
});
