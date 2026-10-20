/* 角色导航与页面边界 */
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
  const waiter = pending.get(m.id);
  if (!waiter) return;
  pending.delete(m.id);
  m.error ? waiter.reject(new Error(JSON.stringify(m.error))) : waiter.resolve(m.result);
};
const send = (method, params = {}) => new Promise((resolve, reject) => {
  const i = id++;
  pending.set(i, { resolve, reject });
  ws.send(JSON.stringify({ id: i, method, params }));
});
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true })).result.value;
const text = () => evaluate('document.body.innerText');
const check = (name, condition, detail = '') => {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
};
async function go(path, wait = 4500) {
  await send('Page.navigate', { url: `${BASE}${path}${path.includes('?') ? '&' : '?'}_=${Date.now()}` });
  await sleep(1000);
  await send('Page.reload', { ignoreCache: true });
  await sleep(wait);
}
await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });

await go('/?role=designer');
let body = await text();
check('设计师导航：构件入库可见', body.includes('构件入库'));
check('设计师导航：无审核质检', !body.includes('审核质检'));
check('设计师导航：无自审核路由', !body.includes('自审核'));
await go('/library?role=designer', 5500);
body = await text();
check('资产大厅无尺寸过滤', !body.includes('构件尺寸') && !body.includes('构件高度'));

await go('/semantics?panel=sandbox&filter=unmapped&role=designer', 6000);
body = await text();
check('标准映射查看定位到未映射', body.includes('仅未映射') && body.includes('部位映射结果'));

await go('/?role=reviewer');
body = await text();
check('审查专家导航：审核质检可见', body.includes('审核质检'));
check('审查专家导航：无构件入库', !body.includes('构件入库'));
await go('/reviews?role=reviewer', 7000);
body = await text();
check('专家审核页标题', body.includes('审核质检') && !body.includes('当值'));
check('专家审核页仍保留签批', body.includes('通过审核') && body.includes('驳回修改'));

await go('/?role=asset-admin');
body = await text();
check('资产管理员导航：族库/监控可见', body.includes('族库管理') && body.includes('监控看板'));

await go('/?role=standard-admin');
body = await text();
check('标准管理员导航：语义中枢可见', body.includes('语义标准中枢'));
check('标准管理员导航：无族库管理', !body.includes('族库管理'));

await go('/reviews?role=designer', 5000);
check('设计师访问审核页回工作台', (await evaluate('location.pathname')) === '/');
ws.close();
console.log('ROLE E2E DONE');
