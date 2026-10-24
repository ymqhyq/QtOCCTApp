/* 三项新增功能视觉/交互回归：投影图 Tab、族库管理、线框模式 */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';
const fs = await import('node:fs/promises');
const targets = await (await fetch(`${DEBUG}/json/list`)).json();
const page = targets.find((t) => t.type === 'page');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
let id = 1;
const pending = new Map();
const consoleErrors = [];
ws.onmessage = (event) => {
  const m = JSON.parse(event.data);
  if (m.method === 'Runtime.exceptionThrown') consoleErrors.push(m.params.exceptionDetails?.text ?? 'exception');
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
const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true, userGesture: true })).result.value;
const text = () => evaluate('document.body.innerText');
const clickText = (t, selector = 'button, a') => evaluate(
  `(() => { const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(e => e.textContent && e.textContent.trim().includes(${JSON.stringify(t)})); if (!el) return false; el.click(); return true; })()`,
);
const shot = async (name) => {
  const { data } = await send('Page.captureScreenshot', { format: 'png' });
  await fs.writeFile(`shots/${name}.png`, Buffer.from(data, 'base64'));
};
const check = (name, condition, detail = '') => {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
};
await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });

async function navigate(path, wait = 6500) {
  await send('Page.navigate', { url: `${BASE}${path}${path.includes('?') ? '&' : '?'}_=${Date.now()}` });
  await sleep(1200);
  await send('Page.reload', { ignoreCache: true });
  await sleep(wait);
}

// 1) 投影图 Tab
await navigate('/components/PierBody?role=designer');
await clickText('二维投影图');
await sleep(2800);
const projection = await evaluate(`(() => {
  const img = [...document.querySelectorAll('img')].find(i => (i.alt || '').includes('二维投影图'));
  return img ? { w: img.naturalWidth, h: img.naturalHeight, src: img.getAttribute('src') } : null;
})()`);
check('详情页投影图 Tab 加载 SVG', Boolean(projection?.w > 0), projection ? `${projection.w}×${projection.h}` : 'no img');
check('投影图 URL 使用 token 加载', Boolean(projection?.src?.includes('projection.svg') && projection.src.includes('token=')));
await evaluate(`document.querySelector('img[alt*="二维投影图"]')?.scrollIntoView({ block: 'center' })`);
await sleep(400);
await shot('projection_tab');
await evaluate(`document.querySelector('[data-projection-view="front"]')?.click()`);
await sleep(1600);
const front = await evaluate(`(() => { const img = [...document.querySelectorAll('img')].find(i => (i.alt || '').includes('二维投影图')); return img ? img.getAttribute('src') : ''; })()`);
check('切换到正视投影', front.includes('view=front'));

// 2) 族库管理
await navigate('/families?role=asset-admin');
let body = await text();
check('族库管理页加载', body.includes('族库管理') && body.includes('族定义'), body.split('\n').slice(0, 4).join(' | '));
const familyCards = await evaluate(`document.querySelectorAll('article').length`);
check('族卡片渲染', familyCards >= 20, `cards=${familyCards}`);
await shot('families');
await clickText('基于模板新建族');
await sleep(700);
body = await text();
check('新建族弹窗打开', body.includes('基于模板新建族') && body.includes('新族 ID'));
await shot('family_create_modal');
await send('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27 });
await send('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Escape', code: 'Escape', windowsVirtualKeyCode: 27 });
await sleep(400);

async function mouseClick(x, y) {
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x, y, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x, y, button: 'left', clickCount: 1 });
}

// 3) 线框模式
await navigate('/components/PierBody/configurator?role=designer');
const before = await clickText('着色');
await sleep(1000);
await shot('wireframe_mode');
body = await text();
check('渲染模式切换按钮存在', before === true);
check('线框模式可切换', body.includes('线框') && body.includes('黑边'));
check('坐标轴罗盘已渲染', Boolean(await evaluate(`document.querySelector('[aria-label="三维坐标轴"]')`)));
const canvasRect = await evaluate(`(() => { const c=document.querySelector('canvas'); const r=c.getBoundingClientRect(); return {x:r.x,y:r.y,w:r.width,h:r.height}; })()`);
await mouseClick(canvasRect.x + canvasRect.w * 0.5, canvasRect.y + canvasRect.h * 0.5);
await sleep(700);
check('线框模式下仍可拾取拓扑面', (await text()).includes('拓扑面'), 'face pick');
await clickText('线框');
await sleep(800);
await shot('xray_mode');
check('X-Ray 模式可切换', (await text()).includes('X-Ray'));

console.log('console errors:', consoleErrors.length ? consoleErrors.join(' | ') : 'none');
ws.close();
