/* P0 回归：隐藏部位后，局部 faceIndex 必须映射回全局三角形，拾取不得错位 */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';
const fs = await import('node:fs/promises');
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
const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true, userGesture: true })).result.value;
const text = () => evaluate('document.body.innerText');
async function mouseClick(x, y) {
  await send('Input.dispatchMouseEvent', { type: 'mousePressed', x, y, button: 'left', clickCount: 1 });
  await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x, y, button: 'left', clickCount: 1 });
}
const check = (name, condition, detail = '') => {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
};

await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });
await send('Page.navigate', { url: `${BASE}/components/PierBody/configurator?role=designer&_=${Date.now()}` });
await sleep(1500);
await send('Page.reload', { ignoreCache: true });
await sleep(7000);

const rect = await evaluate(`(() => { const c = document.querySelector('canvas'); const r = c.getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; })()`);
const hidden = await evaluate(`(() => {
  const rows = [...document.querySelectorAll('div')].filter(d => d.className && String(d.className).includes('group') && d.textContent.includes('PierBodySolid'));
  const row = rows.find(d => d.querySelector('button'));
  if (!row) return false;
  row.querySelector('button').click();
  return true;
})()`);
await sleep(900);
check('隐藏墩身实体', hidden === true);
await mouseClick(rect.x + rect.w * 0.52, rect.y + rect.h * 0.22);
await sleep(800);
const bodyText = await text();
const faceMatch = bodyText.match(/拓扑面 #(\d+)/);
const shapeOk = bodyText.includes('TopBearingFace');
check('局部 faceIndex 映射后 faceId 正确', faceMatch?.[1] === '4', faceMatch?.[0] ?? 'no face');
check('拾取命中 TopBearingFace（未错位到隐藏墩身）', shapeOk && !bodyText.includes('拓扑面 #1'));
const { data } = await send('Page.captureScreenshot', { format: 'png' });
await fs.writeFile('shots/hidden_body_pick.png', Buffer.from(data, 'base64'));
ws.close();
