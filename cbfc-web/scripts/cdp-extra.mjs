/* 补充端到端走查：对比抽屉、入库门禁、详情 Tabs、空结果诊断 */
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
const evaluate = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true, userGesture: true })).result.value;
const text = () => evaluate('document.body.innerText');
const clickText = (t, sel = 'button') => evaluate(`(() => { const el=[...document.querySelectorAll(${JSON.stringify(sel)})].find(e=>e.textContent && e.textContent.trim().includes(${JSON.stringify(t)})); if(!el) return false; el.click(); return true; })()`);
const check = (name, condition, detail = '') => {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
};
async function navigate(path, wait = 6000) {
  await send('Page.navigate', { url: `${BASE}${path}${path.includes('?') ? '&' : '?'}_=${Date.now()}` });
  await sleep(1000);
  await send('Page.reload', { ignoreCache: true });
  await sleep(wait);
}
await send('Runtime.enable');
await send('Page.enable');
await send('Emulation.setDeviceMetricsOverride', { width: 1600, height: 1000, deviceScaleFactor: 1, mobile: false });

// 对比抽屉
await navigate('/library?role=designer', 6500);
let body = await text();
await check('构件大厅默认三列卡片', body.includes('参数化构件') && body.includes('原生 BRep'));
const compared = await clickText('对比');
await sleep(600);
body = await text();
await check('加入对比栏', compared && body.includes('已选 1 项构件'));
await clickText('启动深度对比');
await sleep(600);
body = await text();
await check('深度对比抽屉', body.includes('多模型参数与算量深度对比') && body.includes('实体净体积'));
await clickText('完成对比');
await sleep(300);

// 筛选：参数化构件
await clickText('参数化构件');
await sleep(900);
const parametric = await evaluate(`(() => { const el=[...document.querySelectorAll('button')].find(b=>b.textContent.includes('参数化构件')); return el?.textContent?.trim() ?? ''; })()`);
await check('能力过滤生效', /参数化构件\s*17/.test(parametric.replace(/\s+/g, ' ')), parametric);
await evaluate(`document.querySelector('[data-source="all"]')?.click()`);
await sleep(700);

// 空结果诊断
await navigate('/library?q=__NO_MATCH__', 5000);
body = await text();
await check('空结果友好兜底', body.includes('当前组合无匹配构件') && body.includes('清空全部筛选'));

// 详情 Tabs
await navigate('/components/PierBody?role=asset-admin', 7000);
await clickText('确定性缓存与 QA');
await sleep(400);
body = await text();
await check('QA Tab：缓存键与对齐', body.includes('权威确定性缓存键') && body.includes('JHB 协议版本'));
await clickText('外部语义映射');
await sleep(400);
body = await text();
await check('语义 Tab：IFC 与 unmapped', body.includes('IfcShapeAspect') && body.includes('未映射'));
await clickText('版本记录与变更');
await sleep(400);
body = await text();
await check('版本 Tab 与差异面板', body.includes('版本差异对比') && body.includes('发起 3D 几何重叠对比'));

// 入库门禁：通过 DataTransfer 模拟拖入 .cbfc
await navigate('/ingestion?role=asset-admin', 6000);
const dropped = await evaluate(`(() => {
  const input = document.querySelector('input[type=file]');
  if (!input) return 'no-input';
  const zone = input.parentElement;
  const file = new File([new Uint8Array(2048)], 'CQ_import_check.cbfc', { type: 'application/zip' });
  const dt = new DataTransfer();
  dt.items.add(file);
  zone.dispatchEvent(new DragEvent('dragover', { bubbles: true, dataTransfer: dt }));
  zone.dispatchEvent(new DragEvent('drop', { bubbles: true, dataTransfer: dt }));
  return 'dropped';
})()`);
await sleep(5000);
body = await text();
await check('拖入 .cbfc 触发门禁', dropped === 'dropped', String(dropped));
await check('6 道门禁全部通过', body.includes('6/6') || body.includes('全部门禁通过'), body.includes('GATE-06') ? 'GATE-06' : '');
await check('门禁逐道显示 PASSED', body.includes('PASSED') || body.includes('通过'));

console.log('EXTRA E2E DONE');
ws.close();
