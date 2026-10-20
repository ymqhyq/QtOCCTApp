const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';
const fs = await import('node:fs/promises');
const targets = await (await fetch(`${DEBUG}/json/list`)).json();
const page = targets.find((t) => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
let id = 1;
const pending = new Map();
const errors = [];
ws.onmessage = (e) => {
  const m = JSON.parse(e.data);
  if (m.method === 'Runtime.exceptionThrown') errors.push(m.params.exceptionDetails?.text ?? 'exception');
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
const evaluate = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true, userGesture: true })).result.value;
const clickText = (t) => evaluate(`(() => { const el = [...document.querySelectorAll('button')].find(e => e.textContent && e.textContent.trim() === ${JSON.stringify(t)}); if (!el) return false; el.click(); return true; })()`);
await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });
for (const family of ['PierBodyHollow', 'CulvertBox']) {
  await send('Page.navigate', { url: `${BASE}/components/${family}/configurator?role=designer&_=${Date.now()}` });
  await sleep(1500);
  await send('Page.reload', { ignoreCache: true });
  await sleep(7000);
  const clicked = await clickText('x');
  await sleep(1200);
  const { data } = await send('Page.captureScreenshot', { format: 'png' });
  await fs.writeFile(`shots/clip_${family}.png`, Buffer.from(data, 'base64'));
  const text = await evaluate('document.body.innerText');
  console.log(family, 'clip:', clicked, 'cap label:', text.includes('鎴潰鍔犵洊'), 'errors:', errors.length);
}
ws.close();
