// ==========================================
// === ESP-NOW PEERS CONFIGURATION ===
// ==========================================
let espPeers = [];
function normPeer(p){
  return {
    mac:(p.mac||'').toUpperCase(),
    start_universe:parseInt(p.start_universe!==undefined?p.start_universe:0),
    start_address:parseInt(p.start_address!==undefined?p.start_address:1),
    end_universe:parseInt(p.end_universe!==undefined?p.end_universe:(p.start_universe!==undefined?p.start_universe:32767)),
    end_address:parseInt(p.end_address!==undefined?p.end_address:512)
  };
}
function peerRangeLabel(p){
  p=normPeer(p);
  return `U${p.start_universe}:CH${p.start_address} - U${p.end_universe}:CH${p.end_address}`;
}
async function loadPeers(){
  try{
    const d=await(await fetch('/api/espnow-peers')).json();
    espPeers=(d.peers||[]).map(normPeer);
    renderPeers();
  }catch(e){}
}
function renderPeers(){
  const tb=document.getElementById('peer-tbody');
  tb.innerHTML='';
  updateScoreBar();
  if(!espPeers.length){tb.innerHTML='<tr><td colspan="4" style="text-align:center;color:#94a3b8;padding:12px">No peers added (Fallback to Broadcast).</td></tr>';return;}
  espPeers.forEach((p,i)=>{
    p=normPeer(p);
    const tr=document.createElement('tr');
    tr.innerHTML=`<td>${i+1}</td><td><code style="font-size:0.9rem">${p.mac}</code></td><td>${peerRangeLabel(p)}</td>
      <td><button class="btn bd" style="padding:3px 8px;font-size:0.72rem" onclick="delPeer(${i})">Delete</button></td>`;
    tb.appendChild(tr);
  });
}
function addPeer(){
  const mac=document.getElementById('np_mac').value.trim().toUpperCase();
  if(!/^([0-9A-F]{2}[:-]){5}([0-9A-F]{2})$/.test(mac)){alert('Invalid MAC Address format');return;}
  const peer={
    mac:mac,
    start_universe:parseInt(document.getElementById('np_start_uni').value)||0,
    start_address:parseInt(document.getElementById('np_start_addr').value)||1,
    end_universe:parseInt(document.getElementById('np_end_uni').value)||0,
    end_address:parseInt(document.getElementById('np_end_addr').value)||512
  };
  if(peer.start_address<1) peer.start_address=1;
  if(peer.start_address>512) peer.start_address=512;
  if(peer.end_address<1) peer.end_address=1;
  if(peer.end_address>512) peer.end_address=512;
  if(peer.end_universe<peer.start_universe || (peer.end_universe===peer.start_universe && peer.end_address<peer.start_address)){
    alert('End range must be after Start range.');
    return;
  }
  espPeers.push(peer);
  document.getElementById('np_mac').value='';
  document.getElementById('np_start_uni').value=peer.end_universe;
  document.getElementById('np_start_addr').value=peer.end_address<512?peer.end_address+1:1;
  document.getElementById('np_end_uni').value=peer.end_address<512?peer.end_universe:peer.end_universe+1;
  document.getElementById('np_end_addr').value=512;
  renderPeers();
}
function delPeer(idx){espPeers.splice(idx,1);renderPeers();}
async function savePeers(){
  try{
    const res=await fetch('/api/espnow-peers',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({peers:espPeers.map(normPeer)})});
    showAlert(res.ok);
  }catch(e){showAlert(false);}
}

function copyMac(){
  const mac=document.getElementById('board_mac_display').textContent;
  if(mac&&mac!=='--:--:--:--:--:--'){
    navigator.clipboard.writeText(mac).then(()=>{
      const btn=event.target; btn.textContent='Copied!';
      setTimeout(()=>btn.textContent='Copy',1500);
    });
  }
}
