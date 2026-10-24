const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';
const targets = await (await fetch(`${DEBUG}/json/list`)).json();
const page = targets.find((t) => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
let id = 1;
const pending = new Map();
ws.onmessage = (event) => {
  const m = JSON.parse(event.data);
  if (!m.id) return;
  const w = pending.get(m.id);
  if (!w) return;
  pending.delete(m.id);
  m.error ? w.reject(new Error(JSON.stringify(m.error))) : w.resolve(m.result);
};
const send = (method, params = {}) => new Promise((resolve, reject) => {
  const i = id++;
  pending.set(i, { resolve, reject });
  ws.send(JSON.stringify({ id: i, method, params }));
});
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const evaluate = async (expression) => {
  const r = await send('Runtime.evaluate', { expression, returnByValue: true });
  return r.result.value;
};
await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 430, height: 900, deviceScaleFactor: 1, mobile: true });
await send('Page.navigate', { url: `${BASE}/?role=designer&_=${Date.now()}` });
await sleep(6000);
const info = await evaluate(`(() => {
  const vw = document.documentElement.clientWidth;
  const list = [...document.querySelectorAll('*')]
    .map(el => ({ el, r: el.getBoundingClientRect() }))
    .filter(({ el, r }) => r.width > 0 && r.right > vw + 1 && getComputedStyle(el).position !== 'fixed')
    .slice(0, 25)
    .map(({ el, r }) => ({
      tag: el.tagName.toLowerCase(),
      cls: String(el.className).slice(0, 90),
      text: (el.textContent || '').trim().slice(0, 40),
      width: Math.round(r.width),
      right: Math.round(r.right),
    }));
  return { vw, scrollWidth: document.documentElement.scrollWidth, bodyScroll: document.body.scrollWidth, list };
})()`);
console.log(JSON.stringify(info, null, 2));
ws.close();
