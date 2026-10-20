/* 资产大厅：分类标准主目录 / 族下型号列表 / 型号定位 / 过滤与检索 */
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
const evaluate = async (expr) => (await send('Runtime.evaluate', { expression: expr, returnByValue: true, userGesture: true })).result.value;
const text = () => evaluate('document.body.innerText');
const clickText = (t, sel = 'button') => evaluate(`(() => { const el=[...document.querySelectorAll(${JSON.stringify(sel)})].find(e=>e.textContent && e.textContent.trim().includes(${JSON.stringify(t)})); if(!el) return false; el.click(); return true; })()`);
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
await send('Page.navigate', { url: `${BASE}/library?role=designer&_=${Date.now()}` });
await sleep(1200);
await send('Page.reload', { ignoreCache: true });
await sleep(7500);

let body = await text();
check('资产主目录存在', body.includes('资产主目录') && body.includes('分类标准'));
check('CQ 参数化构件已入库', body.includes('桥墩组合体') && body.includes('支承垫石'));
check('目录包含分类层级', Boolean(await evaluate(`document.querySelector('[data-taxonomy-code]')`)));
check('目录包含族与型号节点', Boolean(await evaluate(`document.querySelector('[data-family-id="PierBody"]') && document.querySelector('[data-tree-preset="STD-PIER-8M"]')`)));
await shot('library_tree');

await evaluate(`document.querySelector('[data-family-id="PierBody"]')?.click()`);
await sleep(1800);
body = await text();
check('点击族节点不跳转详情', (await evaluate('location.pathname')) === '/library');
check('族视图显示全部预定义型号', body.includes('STD-PIER-8M') && body.includes('STD-PIER-12M') && body.includes('STD-PIER-16M'));
check('族视图显示族名与型号数量', body.includes('圆端型实体桥墩墩身') && body.includes('个预定义型号'));
await shot('library_family');

await evaluate(`document.querySelector('[data-tree-preset="STD-PIER-12M"]')?.click()`);
await sleep(1200);
body = await text();
check('点击型号不进入详情', (await evaluate('location.pathname')) === '/library');
check('型号定位高亮', Boolean(await evaluate(`(() => { const el=document.querySelector('[data-preset-model="STD-PIER-12M"]'); return el && el.className.includes('ring-2'); })()`)));
check('型号列表仍包含同级型号', body.includes('STD-PIER-8M') && body.includes('STD-PIER-16M'));

await clickText('返回资产目录');
await sleep(1200);
await clickText('参数化构件');
await sleep(900);
const parametric = await evaluate(`(() => { const el=[...document.querySelectorAll('button')].find(b=>b.textContent.includes('参数化构件')); return el?.textContent?.trim() ?? ''; })()`);
check('参数化能力过滤', /参数化构件\s*17/.test(parametric.replace(/\s+/g, ' ')), parametric);

const input = await evaluate(`(() => { const el=document.querySelector('input[placeholder*="构件名称"]'); if(!el) return false; const setter=Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set; setter.call(el,'PierBody'); el.dispatchEvent(new Event('input',{bubbles:true})); el.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true})); return true; })()`);
await sleep(1600);
check('名称/编码检索', input && (await text()).includes('名称/编码 “PierBody”'));

await clickText('清空全部');
await sleep(1000);
await clickText('几何相似');
await sleep(400);
const selected = await evaluate(`(() => { const sel=[...document.querySelectorAll('select')].find(s=>s.textContent.includes('选择参考构件族')); if(!sel) return false; const opt=[...sel.options].find(o=>o.textContent.includes('PierBody')); if(!opt) return false; const setter=Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value').set; setter.call(sel,opt.value); sel.dispatchEvent(new Event('change',{bubbles:true})); return true; })()`);
await sleep(200);
await clickText('查找相似');
await sleep(2000);
body = await text();
check('几何相似检索生效', selected && body.includes('几何相似：') && body.includes('参考'));
check('相似度徽标显示', body.includes('相似 '));
await shot('library_similar');
ws.close();
