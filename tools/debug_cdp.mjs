import { spawn } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname.replace(/^\/(.:)/, '$1'));
const PYTHON = 'C:\\Users\\natta\\.platformio\\penv\\Scripts\\python.exe';

function delay(ms) { return new Promise(r => setTimeout(r, ms)); }

class CdpClient {
  constructor(wsUrl) {
    this.wsUrl = wsUrl;
    this.id = 0;
    this.pending = new Map();
  }
  async connect() {
    this.ws = new WebSocket(this.wsUrl);
    this.ws.onmessage = event => {
      const msg = JSON.parse(event.data);
      if (msg.id && this.pending.has(msg.id)) {
        const { resolve } = this.pending.get(msg.id);
        this.pending.delete(msg.id);
        resolve(msg.result || {});
      }
    };
    await new Promise((resolve, reject) => {
      this.ws.onopen = resolve;
      this.ws.onerror = () => reject(new Error('ws error'));
    });
  }
  send(method, params = {}) {
    const id = ++this.id;
    this.ws.send(JSON.stringify({ id, method, params }));
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      setTimeout(() => reject(new Error('CDP timeout')), 10000);
    });
  }
  close() { if (this.ws) this.ws.close(); }
}

async function main() {
  const server = spawn(PYTHON, ['tools/web_mock_server.py'], { cwd: ROOT, stdio: ['ignore', 'pipe', 'pipe'], windowsHide: true });
  await delay(3000);

  const chromePath = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
  const debugPort = 9333 + Math.floor(Math.random() * 1000);
  const userDataDir = await mkdtemp(path.join(os.tmpdir(), 'cdp-'));
  const browser = spawn(chromePath, [
    '--headless=new', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
    `--remote-debugging-port=${debugPort}`, `--user-data-dir=${userDataDir}`, 'about:blank'
  ], { stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });
  await delay(3000);

  const resp = await fetch(`http://127.0.0.1:${debugPort}/json/list`);
  const targets = await resp.json();
  const page = targets.find(t => t.type === 'page');
  if (!page) throw new Error('No page');
  
  const cdp = new CdpClient(page.webSocketDebuggerUrl);
  await cdp.connect();
  await cdp.send('Page.enable');
  await cdp.send('Page.navigate', { url: 'http://127.0.0.1:8000/' });
  await delay(2000);

  const evalCode = `(function() {
    var r = {};
    var typeSel = document.getElementById('no_type');
    typeSel.value = '12';
    typeSel.dispatchEvent(new Event('change', {bubbles: true}));

    // Dump type 12 mode "2" definition
    var d12 = outputDef(12);
    r.modeKeyMap12 = JSON.stringify(d12?.modeKeyMap || TYPE_META.modeKeyMap['12']);
    r.modes2 = JSON.stringify(d12?.modes?.['2']);
    r.modesDefault = JSON.stringify(d12?.modes?.default);
    r.allModes = JSON.stringify(Object.keys(d12?.modes||{}));
    
    // Dump modeKey for (12,2) 
    r.modeKey2 = outputModeKey(12,2);
    var def = outputModeDef(12,2);
    r.def = def ? JSON.stringify(def) : 'NULL';
    
    return JSON.stringify(r);
  })()`;

  const evalResult = await cdp.send('Runtime.evaluate', { expression: evalCode, returnByValue: true });
  if (evalResult.exceptionDetails) {
    console.log('EXCEPTION:', evalResult.exceptionDetails.text);
  } else {
    console.log('Result:', evalResult.result?.value);
  }

  cdp.close();
  browser.kill();
  server.kill();
  await rm(userDataDir, { recursive: true, force: true }).catch(() => {});
}

main().catch(e => console.error('Error:', e));
