// ==========================================
// === RESOURCE SCORING & BUDGET CHECKS ===
// Calculated entirely on C++ side (Thin Client)
// ==========================================

async function updateScoreBar() {
  const cpuBar=document.getElementById('cpu-score-fill');
  const cpuTxt=document.getElementById('cpu-score-text');
  const ramBar=document.getElementById('ram-score-fill');
  const ramTxt=document.getElementById('ram-score-text');
  if(!cpuBar||!cpuTxt||!ramBar||!ramTxt) return;

  const fps=parseInt(document.getElementById('output_fps')?.value)||40;
  const modeVal=parseInt(document.getElementById('device_mode')?.value||0);
  const chunkSize=parseInt(document.getElementById('espnow_chunk_size')?.value)||512;

  try {
    const res = await fetch('/api/outputs/score', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        outputs: outputs,
        fps: fps,
        device_mode: modeVal,
        espnow_chunk_size: chunkSize
      })
    });
    if (!res.ok) return;
    const d = await res.json();

    // Update CPU Bar
    const cpuPct = Math.min(d.cpu_pct, 100);
    cpuBar.style.width = cpuPct + '%';
    cpuBar.style.background = d.cpu_bad ? '#ef4444' : cpuPct > 80 ? '#eab308' : '#22c55e';
    cpuTxt.textContent = cpuPct.toFixed(1) + '% (' + d.cpu_us + ' / ' + d.cpu_limit + ' \u00B5s)';

    // Update RAM Bar
    const ramPct = Math.min(d.ram_pct, 100);
    ramBar.style.width = ramPct + '%';
    ramBar.style.background = d.ram_bad ? '#ef4444' : ramPct > 80 ? '#eab308' : '#22c55e';
    ramTxt.textContent = ramPct.toFixed(1) + '% (' + (d.ram_bytes/1024).toFixed(3) + ' KB / ' + (d.ram_limit/1024).toFixed(3) + ' KB)';

    // Helper to update peripheral resource bars
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

    updateHwCard('hw-ledc-val','hw-ledc-bar', d.hw.ledc, d.hw.ledc_limit);
    updateHwCard('hw-rmt-val','hw-rmt-bar', d.hw.rmt, d.hw.rmt_limit);
    updateHwCard('hw-uart-val','hw-uart-bar', d.hw.uart, d.hw.uart_limit);
    updateHwCard('hw-dac-val','hw-dac-bar', d.hw.dac, d.hw.dac_limit);
    updateHwCard('hw-timer-val','hw-timer-bar', d.hw.timer, d.hw.timer_limit);

  } catch(e) {}

  setSaveState(outputsDirty ? 'Unsaved changes' : 'Saved', outputsDirty ? 'warn' : 'ok');
}
