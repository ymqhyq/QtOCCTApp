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
const evaluate = async (expression) => (await send('Runtime.evaluate', { expression, returnByValue: true })).result.value;
const text = () => evaluate('document.body.innerText');
const check = (name, cond, detail = '') => {
  console.log(`${cond ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!cond) process.exitCode = 2;
};
const navigate = async (path, wait) => {
  await send('Page.navigate', { url: `${BASE}${path}${path.includes('?') ? '&' : '?'}_=${Date.now()}` });
  await sleep(wait);
};
await send('Runtime.enable');
await send('Page.enable');

await navigate('/components/SignGantry?role=designer', 8000);
let body = await text();
check('native-brep 详情 3D 加载', body.includes('标志门架') && body.includes('3D 交互画布'), body.includes('3D 交互画布') ? '3D ok' : 'no 3D');
check('native-brep 禁止在线变参', body.includes('固定几何') || body.includes('只读'), body.match(/只读[^\n]{0,20}/)?.[0] ?? '');

await navigate('/components/DrainageGrate?role=designer', 8000);
body = await text();
check('mesh-only 详情 3D 加载', body.includes('桥面泄水槽') && body.includes('3D 交互画布'));
check('mesh-only 精确格式受限', body.includes('mesh-only') || body.includes('网格体'));

await navigate('/components/SignGantry/configurator?role=designer', 8000);
body = await text();
check('非参构件配置器只读提示', body.includes('不支持在线修改') || body.includes('只读预览'));
check('非参构件仍可预览几何', body.includes('3D 交互画布'));

await navigate('/library?role=partner', 6000);
body = await text();
const cards = await evaluate(`document.querySelectorAll('article').length`);
const privateBadge = await evaluate(`document.body.innerText.includes('受保密') || document.body.innerText.includes('租户内')`);
check('外部合作方仅公开目录', cards > 0 && !privateBadge, `cards=${cards}`);
ws.close();
console.log('NONPARAM DONE');
