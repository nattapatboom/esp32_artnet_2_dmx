import { spawn } from 'node:child_process';
import { mkdtemp, rm } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const ROOT = path.resolve(new URL('..', import.meta.url).pathname.replace(/^\/(.:)/, '$1'));
const MOCK_URL = process.env.WEB_UI_URL || 'http://127.0.0.1:8000';
const PYTHON = process.env.PYTHON || 'C:\\Users\\natta\\.platformio\\penv\\Scripts\\python.exe';
const BROWSER_PATHS = [
  process.env.BROWSER_PATH,
  'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
  'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe',
  'C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe'
].filter(Boolean);

function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function fetchOk(url, timeoutMs = 1000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const res = await fetch(url, { signal: controller.signal, cache: 'no-store' });
    return res.ok;
  } catch {
    return false;
  } finally {
    clearTimeout(timer);
  }
}

async function waitForHttp(url, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (await fetchOk(url)) return true;
    await delay(250);
  }
  return false;
}

function startMockServer() {
  if (!existsSync(PYTHON)) throw new Error(`Python not found: ${PYTHON}`);
  return spawn(PYTHON, ['tools/web_mock_server.py'], {
    cwd: ROOT,
    stdio: ['ignore', 'pipe', 'pipe'],
    windowsHide: true
  });
}

function findBrowser() {
  const browser = BROWSER_PATHS.find(p => existsSync(p));
  if (!browser) throw new Error('Chrome/Edge not found. Set BROWSER_PATH to a Chromium executable.');
  return browser;
}

class CdpClient {
  constructor(wsUrl) {
    this.wsUrl = wsUrl;
    this.id = 0;
    this.pending = new Map();
    this.events = new Map();
  }

  async connect() {
    if (typeof WebSocket === 'undefined') throw new Error('Node WebSocket is not available. Use Node 22+ or newer.');
    this.ws = new WebSocket(this.wsUrl);
    this.ws.onmessage = event => {
      const msg = JSON.parse(event.data);
      if (msg.id && this.pending.has(msg.id)) {
        const { resolve, reject } = this.pending.get(msg.id);
        this.pending.delete(msg.id);
        msg.error ? reject(new Error(msg.error.message)) : resolve(msg.result || {});
        return;
      }
      if (msg.method && this.events.has(msg.method)) {
        for (const handler of this.events.get(msg.method)) handler(msg.params || {});
      }
    };
    await new Promise((resolve, reject) => {
      this.ws.onopen = resolve;
      this.ws.onerror = () => reject(new Error(`Failed to connect CDP: ${this.wsUrl}`));
    });
  }

  on(method, handler) {
    if (!this.events.has(method)) this.events.set(method, []);
    this.events.get(method).push(handler);
  }

  send(method, params = {}) {
    const id = ++this.id;
    this.ws.send(JSON.stringify({ id, method, params }));
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      setTimeout(() => {
        if (!this.pending.has(id)) return;
        this.pending.delete(id);
        reject(new Error(`CDP timeout: ${method}`));
      }, 10000);
    });
  }

  close() {
    if (this.ws) this.ws.close();
  }
}

async function waitForCdp(port, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      const res = await fetch(`http://127.0.0.1:${port}/json/list`, { cache: 'no-store' });
      if (res.ok) {
        const targets = await res.json();
        const page = targets.find(t => t.type === 'page' && t.webSocketDebuggerUrl);
        if (page) return page.webSocketDebuggerUrl;
      }
    } catch {}
    await delay(250);
  }
  throw new Error('Timed out waiting for browser CDP endpoint.');
}

async function main() {
  let server = null;
  let browser = null;
  let userDataDir = null;
  let cdp = null;

  try {
    if (!(await waitForHttp(`${MOCK_URL}/api/status`, 800))) {
      server = startMockServer();
      server.stderr.on('data', d => process.stderr.write(d));
      if (!(await waitForHttp(`${MOCK_URL}/api/status`, 8000))) throw new Error('Mock server did not start.');
    }

    const browserPath = findBrowser();
    const debugPort = 9333 + Math.floor(Math.random() * 1000);
    userDataDir = await mkdtemp(path.join(os.tmpdir(), 'web-ui-smoke-'));
    browser = spawn(browserPath, [
      '--headless=new',
      '--disable-gpu',
      '--no-first-run',
      '--no-default-browser-check',
      `--remote-debugging-port=${debugPort}`,
      `--user-data-dir=${userDataDir}`,
      'about:blank'
    ], { stdio: ['ignore', 'ignore', 'pipe'], windowsHide: true });
    browser.stderr.on('data', () => {});

    const wsUrl = await waitForCdp(debugPort, 10000);
    cdp = new CdpClient(wsUrl);
    await cdp.connect();

    const runtimeErrors = [];
    cdp.on('Runtime.exceptionThrown', params => runtimeErrors.push(params.exceptionDetails?.text || 'Runtime exception'));
    await cdp.send('Page.enable');
    await cdp.send('Runtime.enable');
    await cdp.send('Page.navigate', { url: MOCK_URL });
    await delay(2500);

    const expression = `
      (async function(){
        const result = { renderErrors: [], alerts: [], hasFuncGen: false, outputCount: 0, typeOptions: 0, typeGroups: false, sevenSegDirect: false, sevenSegMixed: false, gpioInvert: false, analogPcaChannels: false, solenoidPca: false, smokeExpander: false, motorEnPca: false, mcp4725: false };
        const byId = id => document.getElementById(id);
        if(!byId('no_type')) throw new Error('missing #no_type');
        if(typeof toggleOutFields !== 'function') throw new Error('missing toggleOutFields()');
        if(typeof renderPinRows !== 'function') throw new Error('missing renderPinRows()');

        const typeSel = byId('no_type');
        result.typeOptions = typeSel.options.length;
        result.typeGroups = typeSel.querySelectorAll('optgroup').length >= 6;
        const setValue = (id, value) => {
          const el = byId(id);
          if(!el) return;
          el.value = String(value);
          el.dispatchEvent(new Event('change', { bubbles: true }));
        };

        for(const opt of Array.from(typeSel.options)){
          typeSel.value = opt.value;
          try {
            toggleOutFields();
            renderPinRows();
          } catch(e) {
            result.renderErrors.push({ type: opt.value, label: opt.textContent, error: e && e.message ? e.message : String(e) });
          }
        }

        setValue('no_type', 12);
        setValue('mc_mode', 2);
        setValue('no_pin2_source', 0);
        toggleOutFields();
        renderPinRows();
        const pinText = document.getElementById('pin-mapping-container')?.textContent || '';
        result.sevenSegDirect = !pinText.includes('Segs Src') && !!document.getElementById('no_seg_pin_1');
        setValue('no_seg_source_1', 2);
        toggleOutFields();
        renderPinRows();
        result.sevenSegMixed = document.getElementById('no_seg_source_1')?.value === '2'
          && !!document.getElementById('no_seg_addr_1')
          && !!document.getElementById('no_seg_channel_1')
          && !document.getElementById('no_seg_pin_1');

        setValue('no_type', 2);
        setValue('no_source', 0);
        toggleOutFields();
        renderPinRows();
        const invert = document.getElementById('no_pin_invert');
        const invertWrap = invert?.closest('.f');
        const invertStyle = invertWrap ? getComputedStyle(invertWrap) : null;
        result.gpioInvert = !!invert && !!invertWrap && invertStyle.display !== 'none' && invertStyle.visibility !== 'hidden';

        setValue('no_type', 5);
        setValue('no_source', 1);
        setValue('no_ord', 4);
        toggleOutFields();
        renderPinRows();
        setValue('no_pin2_source', 1);
        setValue('no_pin3_source', 1);
        setValue('no_pin4_source', 1);
        toggleOutFields();
        renderPinRows();
        result.analogPcaChannels = !!document.getElementById('no_pca_channel')
          && !!document.getElementById('no_pin2_channel')
          && !!document.getElementById('no_pin3_channel')
          && !!document.getElementById('no_pin4_channel')
          && document.getElementById('no_pca_channel2_grp')?.style.display === 'none';

        setValue('no_type', 17);
        setValue('no_source', 1);
        toggleOutFields();
        renderPinRows();
        result.solenoidPca = document.getElementById('no_source')?.value === '1' && !!document.getElementById('no_pca_channel');

        setValue('no_type', 18);
        setValue('no_source', 2);
        toggleOutFields();
        renderPinRows();
        setValue('no_pin2_source', 2);
        toggleOutFields();
        renderPinRows();
        result.smokeExpander = document.getElementById('no_source')?.value === '2'
          && !!document.getElementById('no_pca_channel')
          && !!document.getElementById('no_pin2_channel')
          && document.getElementById('no_pca_channel2_grp')?.style.display === 'none';

        setValue('no_type', 6);
        setValue('mc_mode', 2);
        setValue('no_source', 0);
        toggleOutFields();
        renderPinRows();
        setValue('no_pin3_source', 1);
        toggleOutFields();
        renderPinRows();
        result.motorEnPca = document.getElementById('no_pin3_source')?.value === '1'
          && !!document.getElementById('no_pin3_addr')
          && !!document.getElementById('no_pin3_channel')
          && !document.getElementById('no_pin3');

        setValue('no_type', 14);
        toggleOutFields();
        renderPinRows();
        setValue('no_source', 5);
        toggleOutFields();
        renderPinRows();
        result.mcp4725 = document.getElementById('no_source')?.value === '5'
          && !!document.getElementById('no_pca_addr')
          && (document.getElementById('pin-mapping-container')?.textContent || '').includes('12-bit I2C');

        window.alert = msg => result.alerts.push(String(msg));
        window.confirm = () => true;
        try { outputs.length = 0; } catch(e) { window.outputs = []; }

        setValue('no_type', 16);
        toggleOutFields();
        renderPinRows();
        setValue('no_uni', 0);
        setValue('no_addr', 1);
        setValue('no_pin', 4);
        setValue('mc_mode', 0);
        setValue('mc_resolution', 8);
        setValue('mc_freq', 1000);
        if(typeof addOrUpdateOutput !== 'function') throw new Error('missing addOrUpdateOutput()');
        addOrUpdateOutput();

        const list = typeof outputs !== 'undefined' ? outputs : window.outputs;
        result.outputCount = Array.isArray(list) ? list.length : -1;
        result.hasFuncGen = Array.isArray(list) && list.some(o => Number(o.type) === 16);
        return result;
      })()
    `;

    const evalResult = await cdp.send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
    if (evalResult.exceptionDetails) throw new Error(evalResult.exceptionDetails.text || 'Runtime.evaluate exception');
    const value = evalResult.result?.value;
    if (!value) throw new Error('No smoke test result returned.');
    if (runtimeErrors.length) throw new Error(`Runtime exceptions: ${runtimeErrors.join('; ')}`);
    if (value.renderErrors.length) throw new Error(`Render errors: ${JSON.stringify(value.renderErrors)}`);
    if (value.alerts.length) throw new Error(`Unexpected alerts: ${value.alerts.join('; ')}`);
    if (!value.typeGroups) throw new Error(`Type dropdown is not grouped: ${JSON.stringify(value)}`);
    if (!value.sevenSegDirect) throw new Error(`7-Seg DD direct GPIO UI still requires segment source selector: ${JSON.stringify(value)}`);
    if (!value.sevenSegMixed) throw new Error(`7-Seg DD mixed segment source UI is incomplete: ${JSON.stringify(value)}`);
    if (!value.gpioInvert) throw new Error(`GPIO invert checkbox is not visible: ${JSON.stringify(value)}`);
    if (!value.analogPcaChannels) throw new Error(`Analog RGB/RGBW PCA channel UI is incomplete: ${JSON.stringify(value)}`);
    if (!value.solenoidPca) throw new Error(`Solenoid PCA source UI is incomplete: ${JSON.stringify(value)}`);
    if (!value.smokeExpander) throw new Error(`Smoke Shooter expander UI is incomplete: ${JSON.stringify(value)}`);
    if (!value.motorEnPca) throw new Error(`Motor EN PCA UI is incomplete: ${JSON.stringify(value)}`);
    if (!value.mcp4725) throw new Error(`MCP4725 DAC UI is incomplete: ${JSON.stringify(value)}`);
    if (!value.hasFuncGen) throw new Error(`Function Generator add failed: ${JSON.stringify(value)}`);

    console.log(`PASS web UI smoke test: ${value.typeOptions} types rendered, type groups OK, 7-Seg direct/mixed OK, GPIO invert visible, Analog PCA channels OK, Solenoid/Smoke expander OK, Motor EN PCA OK, MCP4725 OK, Function Generator add OK.`);
  } finally {
    if (cdp) cdp.close();
    if (browser) browser.kill();
    if (server) server.kill();
    if (userDataDir) await rm(userDataDir, { recursive: true, force: true }).catch(() => {});
  }
}

main().catch(err => {
  console.error(`FAIL web UI smoke test: ${err.message}`);
  process.exit(1);
});
