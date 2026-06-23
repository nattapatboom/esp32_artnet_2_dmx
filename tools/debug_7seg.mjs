import { spawn } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname.replace(/^\/(.:)/, '$1'));
const PYTHON = 'C:\\Users\\natta\\.platformio\\penv\\Scripts\\python.exe';

async function main() {
  // Start mock server
  const server = spawn(PYTHON, ['tools/web_mock_server.py'], { cwd: ROOT, stdio: ['ignore', 'pipe', 'pipe'], windowsHide: true });
  await new Promise(r => setTimeout(r, 3000));

  // Start Chrome
  const chromePath = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
  const debugPort = 9333 + Math.floor(Math.random() * 1000);
  const userDataDir = await mkdtemp(path.join(os.tmpdir(), 'cdp-test-'));
  const browser = spawn(chromePath, [
    '--headless=new', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
    `--remote-debugging-port=${debugPort}`, `--user-data-dir=${userDataDir}`, 'about:blank'
  ], { stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });

  await new Promise(r => setTimeout(r, 3000));

  // Get CDP websocket URL
  const resp = await fetch(`http://127.0.0.1:${debugPort}/json/list`);
  const targets = await resp.json();
  const page = targets.find(t => t.type === 'page');
  if (!page) throw new Error('No page target');

  const ws = new WebSocket(page.webSocketDebuggerUrl);
  const pending = {};
  let id = 0;

  ws.on('message', (data) => {
    const msg = JSON.parse(data.toString());
    if (msg.id && pending[msg.id]) {
      pending[msg.id](msg);
      delete pending[msg.id];
    }
  });

  await new Promise(r => ws.on('open', r));

  const send = (method, params) => new Promise(r => {
    pending[++id] = r;
    ws.send(JSON.stringify({ id, method, params }));
  });

  await send('Page.enable');
  await send('Page.navigate', { url: 'http://127.0.0.1:8000/' });
  await new Promise(r => setTimeout(r, 2000));

  const evalCode = `(function() {
    const byId = id => document.getElementById(id);
    const setVal = (id, v) => {
      const el = byId(id);
      if (!el) return;
      el.value = String(v);
      el.dispatchEvent(new Event('change', {bubbles: true}));
    };

    // Test 1: 7-segment, type 12, mode 2
    setVal('no_type', 12);
    setVal('mc_mode', 2);
    setVal('no_pin2_source', 0);

    const result1 = {
      t: parseInt(byId('no_type')?.value),
      mcMode: parseInt(byId('mc_mode')?.value || 0),
      hasSegPin1: !!byId('no_seg_pin_1'),
      hasSegSource1: !!byId('no_seg_source_1'),
      containerEmpty: (byId('pin-mapping-container') || {}).innerHTML === '',
    };

    // Test 2: Motor, type 6, mode 2
    setVal('no_type', 6);
    setVal('mc_mode', 2);
    setVal('no_source', 0);
    setVal('no_pin3_source', 1);

    const result2 = {
      hasPin3Source: !!byId('no_pin3_source'),
      hasPin3Addr: !!byId('no_pin3_addr'),
      hasPin3Ch: !!byId('no_pin3_channel'),
      hasNoPin3: !!byId('no_pin3'),
      pin3SourceVal: byId('no_pin3_source') ? byId('no_pin3_source').value : null,
    };

    return JSON.stringify({ result1, result2 });
  })()`;

  const evalResult = await send('Runtime.evaluate', { expression: evalCode, returnByValue: true });
  if (evalResult.exceptionDetails) {
    console.log('EXCEPTION:', evalResult.exceptionDetails.text);
  } else {
    console.log('Result:', evalResult.result?.value);
  }

  ws.close();
  browser.kill();
  server.kill();
  await rm(userDataDir, { recursive: true, force: true }).catch(() => {});
}

main().catch(e => console.error('Error:', e.message));
