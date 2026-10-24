/* 关键交互态截图（CDP） */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';
const fs = await import('node:fs/promises');

const targets = await (await fetch(`${DEBUG}/json/list`)).json();
const page = targets.find((t) => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => {
  ws.onopen = resolve;
  ws.onerror = reject;
});
let nextId = 1;
const pending = new Map();
ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  if (!message.id) return;
  const waiter = pending.get(message.id);
  if (!waiter) return;
  pending.delete(message.id);
  if (message.error) waiter.reject(new Error(JSON.stringify(message.error)));
  else waiter.resolve(message.result);
};
function send(method, params = {}) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params }));
  });
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function evaluate(expression) {
  const result = await send('Runtime.evaluate', { expression, returnByValue: true, userGesture: true });
  return result.result.value;
}
async function navigate(path, wait = 5000) {
  const sep = path.includes('?') ? '&' : '?';
  await send('Page.navigate', { url: BASE + path + sep + '_=' + Date.now() });
  await sleep(wait);
}
async function shot(name) {
  const { data } = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
  await fs.writeFile(`shots/${name}.png`, Buffer.from(data, 'base64'));
  console.log('shot', name);
}
async function clickText(text, selector = 'button, a, [role="button"]') {
  return evaluate(
    `(() => { const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(e => e.textContent && e.textContent.includes(${JSON.stringify(text)})); if (!el) return false; el.click(); return true; })()`,
  );
}
await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', {
  width: 1600,
  height: 1000,
  deviceScaleFactor: 1,
  mobile: false,
});

// 1. 对比抽屉
await navigate('/library?role=designer', 5000);
await evaluate(`(() => {
  const cards = [...document.querySelectorAll('article')];
  cards.slice(0, 3).forEach(card => {
    const btn = [...card.querySelectorAll('button')].find(b => b.textContent.includes('对比'));
    btn?.click();
  });
  return true;
})()`);
await sleep(500);
await clickText('启动深度对比');
await sleep(600);
await shot('compare_drawer');

// 2. 导出弹窗
await navigate('/components/PierBody', 6000);
await clickText('下载构件包');
await sleep(1200);
await shot('export_modal');

// 3. 入库通过
await navigate('/ingestion?role=asset-admin', 5000);
await evaluate(`(() => {
  const zone = [...document.querySelectorAll('div')].find(el => el.className && el.className.includes('border-dashed') && el.className.includes('rounded-md'));
  const dt = new DataTransfer();
  dt.items.add(new File(['x'.repeat(4096)], 'PierBody_v1.4.2.rcif', { type: 'application/zip' }));
  zone.dispatchEvent(new DragEvent('drop', { bubbles: true, cancelable: true, dataTransfer: dt }));
  return true;
})()`);
await sleep(6000);
await shot('ingestion_passed');

// 4. 入库故障
await navigate('/ingestion?role=asset-admin&debug=1', 5000);
await clickText('演示故障注入');
await sleep(300);
await evaluate(`(() => {
  const label = [...document.querySelectorAll('label')].find(l => l.textContent.includes('几何非流形开边'));
  label?.querySelector('input[type=checkbox]')?.click();
  const zone = [...document.querySelectorAll('div')].find(el => el.className && el.className.includes('border-dashed') && el.className.includes('rounded-md'));
  const dt = new DataTransfer();
  dt.items.add(new File(['y'.repeat(4096)], 'PierBody_broken.rcif', { type: 'application/zip' }));
  zone.dispatchEvent(new DragEvent('drop', { bubbles: true, cancelable: true, dataTransfer: dt }));
  return true;
})()`);
await sleep(6000);
await shot('ingestion_blocked');

// 5. 语义中枢修复后的规则表
await navigate('/semantics?role=standard-admin', 5000);
await shot('semantics2');

// 6. 配置器异步任务态（滑块改动后立即截图）
await navigate('/components/PierBody/configurator', 7000);
await evaluate(`(() => {
  const input = [...document.querySelectorAll('input[type=number]')].find(i => i.value === '12000');
  if (!input) return false;
  input.value = '18500';
  input.dispatchEvent(new Event('change', { bubbles: true }));
  return true;
})()`);
await sleep(1100);
await shot('configurator_job');
await sleep(7000);
await shot('configurator_done');

ws.close();
console.log('SHOTS DONE');
