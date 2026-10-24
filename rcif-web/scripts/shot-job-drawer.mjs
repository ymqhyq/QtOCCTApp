import cp from 'node:child_process';
import fs from 'node:fs/promises';

const BASE = 'http://127.0.0.1:5178';
const DEBUG = 'http://127.0.0.1:9222';
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const chromeProc = cp.spawn('C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe', [
  '--remote-debugging-port=9222',
  '--headless=new',
  '--window-size=1600,1000',
  '--user-data-dir=d:\\QtOCCTApp\\scratch\\chrome-cdp-profile',
  '--disable-gpu',
  'about:blank'
]);

for (let i = 0; i < 30; i++) {
  await sleep(200);
  try {
    const res = await fetch(`${DEBUG}/json/version`);
    if (res.ok) break;
  } catch {}
}

const targets = await (await fetch(`${DEBUG}/json/list`)).json();
const page = targets.find((t) => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((r) => { ws.onopen = r; });

let id = 1;
const pending = new Map();
ws.onmessage = (e) => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) {
    pending.get(m.id)(m.result);
    pending.delete(m.id);
  }
};

const send = (method, params = {}) =>
  new Promise((resolve) => {
    const i = id++;
    pending.set(i, resolve);
    ws.send(JSON.stringify({ id: i, method, params }));
  });

await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });

await send('Page.navigate', { url: `${BASE}/components/PierBody/configurator?role=designer&_=${Date.now()}` });
await sleep(6500);

await send('Runtime.evaluate', {
  expression: `(() => {
    const inputs = [...document.querySelectorAll('input[type=number]')];
    const target = inputs.find(i => i.value === '12000');
    if (target) {
      target.value = '22000';
      target.dispatchEvent(new Event('change', { bubbles: true }));
    }
  })()`,
});

await sleep(1200);

await send('Runtime.evaluate', {
  expression: `(() => {
    const btn = [...document.querySelectorAll('button')].find(b => b.textContent.includes('计算任务'));
    if (btn) btn.click();
  })()`,
});

await sleep(1500);

const { data } = await send('Page.captureScreenshot', { format: 'png' });
await fs.writeFile('shots/job_center.png', Buffer.from(data, 'base64'));
console.log('SUCCESS: shots/job_center.png updated');

ws.close();
chromeProc.kill();
