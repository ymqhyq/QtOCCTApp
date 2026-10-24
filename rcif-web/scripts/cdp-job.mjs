/* 计算任务中心验证：异步任务进入、取消、断点恢复 */
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
async function clickText(text, selector = 'button, a') {
  return evaluate(
    `(() => { const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(e => e.textContent && e.textContent.trim().includes(${JSON.stringify(text)})); if (!el) return false; el.click(); return true; })()`,
  );
}
async function check(name, condition, detail = '') {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
}

await send('Runtime.enable');
await send('Page.enable');
await send('Page.navigate', { url: `${BASE}/components/PierBody/configurator?role=designer&_=${Date.now()}` });
await sleep(7000);
await evaluate(`(() => {
  const inputs = [...document.querySelectorAll('input[type=number]')];
  const target = inputs.find(i => i.value === '12000');
  if (!target) return false;
  target.value = '20000';
  target.dispatchEvent(new Event('change', { bubbles: true }));
  return true;
})()`);
await sleep(1500);
await clickText('计算任务');
await sleep(900);
let body = await text();
await check('任务中心打开', body.includes('计算任务中心'));
await check('进行中任务可见', body.includes('进行中') && /排队中|求解中|后台脱机/.test(body), body.match(/排队中|求解中|后台脱机/)?.[0] ?? '');
const jobText = await evaluate(`(() => { const el = [...document.querySelectorAll('article')].find(a => a.textContent.includes('圆端型')); return el ? el.textContent.replace(/\\s+/g, ' ').slice(0, 120) : ''; })()`);
console.log('JOB:', jobText);
const { data } = await send('Page.captureScreenshot', { format: 'png' });
await fs.writeFile('shots/job_center.png', Buffer.from(data, 'base64'));
console.log('shot job_center');
await clickText('取消等待');
await sleep(1500);
body = await text();
await check('任务取消与后台脱机', body.includes('已取消前台等待') || body.includes('后台脱机'));
ws.close();
console.log('JOB DONE');
