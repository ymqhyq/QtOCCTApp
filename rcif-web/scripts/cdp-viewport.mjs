/* 3D 视口交互走查：面拾取、双通道剖切、测距、投影与黑边开关 */
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
async function text() {
  return evaluate('document.body.innerText');
}
async function check(name, condition, detail = '') {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
}
async function clickText(text, selector = 'button') {
  return evaluate(
    `(() => { const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(e => e.textContent && e.textContent.trim() === ${JSON.stringify(text)}); if (!el) return false; el.click(); return true; })()`,
  );
}
async function mouseClick(x, y) {
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x, y, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x, y, button: 'left', clickCount: 1 });
}
async function shot(name) {
  const { data } = await send('Page.captureScreenshot', { format: 'png' });
  await fs.writeFile(`shots/${name}.png`, Buffer.from(data, 'base64'));
  console.log('shot', name);
}

await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: `${BASE}/components/PierBody/configurator?role=designer&_=${Date.now()}` });
await sleep(1500);
await send('Page.reload', { ignoreCache: true });
await sleep(7000);

const rect = await evaluate(`(() => { const c = document.querySelector('canvas'); const r = c.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; })()`);
console.log('canvas rect', rect);
await check('WebGL 画布存在', rect.w > 300 && rect.h > 300, JSON.stringify(rect));

// 面拾取
await mouseClick(rect.x + rect.w * 0.5, rect.y + rect.h * 0.55);
await sleep(600);
let body = await text();
await check('Raycast 面拾取反查 shapeKey', /拓扑面 #\d+/.test(body) && body.includes('Bridge.Pier.'), body.match(/拓扑面 #\d+/)?.[0] ?? 'no pick');
await clickText('清除选择');
await sleep(500);

// 剖切
const clipClicked = await clickText('x');
await sleep(800);
body = await text();
await check('X 轴剖切开启', clipClicked && body.includes('截面加盖') && body.includes('剖切'));
await shot('viewport_clip');

// 测距
await clickText('×'); // 关闭剖切（按钮文字为 ×）
await sleep(300);
const measureClicked = await clickText('测距');
await sleep(300);
await mouseClick(rect.x + rect.w * 0.46, rect.y + rect.h * 0.32);
await sleep(300);
await mouseClick(rect.x + rect.w * 0.54, rect.y + rect.h * 0.68);
await sleep(600);
body = await text();
await check('端点吸附测距', measureClicked && body.includes('端点吸附测距') && body.includes('水平投影'), body.match(/[\d,]+\s*mm/)?.[0] ?? '');
await shot('viewport_measure');

// 投影 / 黑边
await clickText('透视');
await sleep(400);
body = await text();
await check('正交投影切换', body.includes('正交'));
await clickText('正交');
await sleep(300);
await clickText('黑边');
await sleep(300);
body = await text();
await check('矢量黑边开关', body.includes('黑边'));

ws.close();
console.log('VIEWPORT DONE');
