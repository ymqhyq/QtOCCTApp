/* 全量页面与交互态截图脚本（CDP 1600x1000） */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';
const cp = await import('node:child_process');
const fs = await import('node:fs/promises');

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let chromeProc = null;
async function ensureChrome() {
  try {
    const res = await fetch(`${DEBUG}/json/version`);
    if (res.ok) return;
  } catch {}
  console.log('[CDP] 正在拉起 Headless Chrome 实例 (1600x1000)...');
  const chromePath = 'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe';
  chromeProc = cp.spawn(chromePath, [
    '--remote-debugging-port=9222',
    '--headless=new',
    '--window-size=1600,1000',
    '--user-data-dir=d:\\QtOCCTApp\\scratch\\chrome-cdp-profile',
    '--no-first-run',
    '--no-default-browser-check',
    '--disable-gpu',
    'about:blank'
  ]);
  for (let i = 0; i < 30; i++) {
    await sleep(200);
    try {
      const res = await fetch(`${DEBUG}/json/version`);
      if (res.ok) {
        console.log('[CDP] Headless Chrome 就绪！');
        return;
      }
    } catch {}
  }
  throw new Error('启动 Chrome 失败或无法连接 9222 端口');
}

await ensureChrome();

let targets = await (await fetch(`${DEBUG}/json/list`)).json();
let page = targets.find((t) => t.type === 'page');
if (!page) {
  // 如果没有可用页面，新建一个
  await fetch(`${DEBUG}/json/new?about:blank`, { method: 'PUT' });
  targets = await (await fetch(`${DEBUG}/json/list`)).json();
  page = targets.find((t) => t.type === 'page');
}
if (!page) throw new Error('未找到可用页面 target');

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

async function evaluate(expression) {
  const result = await send('Runtime.evaluate', { expression, returnByValue: true, userGesture: true });
  return result?.result?.value;
}

async function navigate(path, wait = 5000) {
  const sep = path.includes('?') ? '&' : '?';
  await send('Page.navigate', { url: BASE + path + sep + '_=' + Date.now() });
  await sleep(wait);
}

async function shot(name) {
  const { data } = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: false });
  await fs.writeFile(`shots/${name}.png`, Buffer.from(data, 'base64'));
  console.log(`[SHOT] shots/${name}.png 完成 (${Math.round(data.length * 0.75 / 1024)} KB)`);
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

console.log('=== 开始执行全量截图 ===');

// 1. 登录页
try {
  await navigate('/login', 3000);
  await shot('login');
} catch (e) { console.error('login shot failed:', e.message); }

// 2. 工作台 (Designer 角色)
try {
  await navigate('/?role=designer', 5000);
  await shot('home');
} catch (e) { console.error('home shot failed:', e.message); }

// 3. 资产大厅网格试图
try {
  await navigate('/library?role=designer', 5000);
  await shot('library');
} catch (e) { console.error('library shot failed:', e.message); }

// 4. 资产大厅表格试图
try {
  await evaluate(`(() => {
    const btn = document.querySelector('button[title*="列表"], button[title*="表格"], [aria-label*="表格"], [aria-label*="列表"]') ||
                [...document.querySelectorAll('button')].find(b => b.innerHTML.includes('M4 6h16') || b.textContent.includes('表格'));
    if (btn) btn.click();
  })()`);
  await sleep(1500);
  await shot('library_table');
} catch (e) { console.error('library_table shot failed:', e.message); }

// 5. 资产大厅目录树展开
try {
  await navigate('/library?role=designer', 5000);
  await shot('library_tree');
} catch (e) { console.error('library_tree shot failed:', e.message); }

// 6. 资产大厅族视图
try {
  await evaluate(`document.querySelector('[data-family-id="PierBody"]')?.click()`);
  await sleep(2000);
  await shot('library_family');
} catch (e) { console.error('library_family shot failed:', e.message); }

// 7. 资产大厅几何相似检索
try {
  await clickText('返回资产目录');
  await sleep(1000);
  await clickText('几何相似');
  await sleep(500);
  await evaluate(`(() => {
    const sel = [...document.querySelectorAll('select')].find(s => s.textContent.includes('选择参考构件族'));
    if (!sel) return false;
    const opt = [...sel.options].find(o => o.textContent.includes('PierBody'));
    if (!opt) return false;
    const setter = Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype, 'value').set;
    setter.call(sel, opt.value);
    sel.dispatchEvent(new Event('change', { bubbles: true }));
    return true;
  })()`);
  await sleep(300);
  await clickText('查找相似');
  await sleep(2000);
  await shot('library_similar');
} catch (e) { console.error('library_similar shot failed:', e.message); }

// 8. 对比抽屉
try {
  await navigate('/library?role=designer', 5000);
  await evaluate(`(() => {
    const cards = [...document.querySelectorAll('article')];
    cards.slice(0, 3).forEach(card => {
      const btn = [...card.querySelectorAll('button')].find(b => b.textContent.includes('对比'));
      btn?.click();
    });
    return true;
  })()`);
  await sleep(800);
  await clickText('启动深度对比');
  await sleep(1000);
  await shot('compare_drawer');
} catch (e) { console.error('compare_drawer shot failed:', e.message); }

// 9. 构件详情页 (PierBody)
try {
  await navigate('/components/PierBody?role=designer', 7000);
  await shot('detail_full');
} catch (e) { console.error('detail_full shot failed:', e.message); }

// 10. 详情页投影图 Tab
try {
  await clickText('二维投影图');
  await sleep(2500);
  await evaluate(`document.querySelector('img[alt*="二维投影图"]')?.scrollIntoView({ block: 'center' })`);
  await sleep(500);
  await shot('projection_tab');
} catch (e) { console.error('projection_tab shot failed:', e.message); }

// 11. 导出弹窗
try {
  await navigate('/components/PierBody?role=designer', 6000);
  await clickText('下载构件包');
  await sleep(1500);
  await shot('export_modal');
} catch (e) { console.error('export_modal shot failed:', e.message); }

// 12. 导出中心
try {
  await navigate('/exports?role=designer', 5000);
  await shot('exports');
} catch (e) { console.error('exports shot failed:', e.message); }

// 13. 配置器完成态
try {
  await navigate('/components/PierBody/configurator?role=designer', 7000);
  await shot('configurator_done');
} catch (e) { console.error('configurator_done shot failed:', e.message); }

// 14. 配置器线框模式与 X-Ray 模式
try {
  await clickText('着色');
  await sleep(1000);
  await shot('wireframe_mode');
  await clickText('线框');
  await sleep(1000);
  await shot('xray_mode');
} catch (e) { console.error('wireframe/xray shot failed:', e.message); }

// 15. 配置器求解/排队态
try {
  await navigate('/components/PierBody/configurator?role=designer', 6000);
  await evaluate(`(() => {
    const input = [...document.querySelectorAll('input[type=number]')].find(i => i.value === '12000');
    if (!input) return false;
    input.value = '18500';
    input.dispatchEvent(new Event('change', { bubbles: true }));
    return true;
  })()`);
  await sleep(1200);
  await shot('configurator_job');
} catch (e) { console.error('configurator_job shot failed:', e.message); }

// 16. 族库管理页面与新建弹窗
try {
  await navigate('/families?role=asset-admin', 5000);
  await shot('families');
  await clickText('基于模板新建族');
  await sleep(800);
  await shot('family_create_modal');
} catch (e) { console.error('families shot failed:', e.message); }

// 17. 审核工作台
try {
  await navigate('/reviews?role=reviewer', 8000);
  await shot('review');
} catch (e) { console.error('review shot failed:', e.message); }

// 18. 语义中枢规则表与沙盒
try {
  await navigate('/semantics?role=standard-admin', 5000);
  await shot('semantics2');
  await clickText('在线推演沙盒');
  await sleep(600);
  await clickText('全量语义码扫描');
  await sleep(2500);
  await shot('semantics_unmapped');
} catch (e) { console.error('semantics shot failed:', e.message); }

// 19. 入库通过
try {
  await navigate('/ingestion?role=asset-admin', 5000);
  await evaluate(`(() => {
    const zone = [...document.querySelectorAll('div')].find(el => el.className && el.className.includes('border-dashed') && el.className.includes('rounded-md'));
    const dt = new DataTransfer();
    dt.items.add(new File(['x'.repeat(4096)], 'PierBody_v1.4.2.cbfc', { type: 'application/zip' }));
    zone.dispatchEvent(new DragEvent('drop', { bubbles: true, cancelable: true, dataTransfer: dt }));
    return true;
  })()`);
  await sleep(6000);
  await shot('ingestion_passed');
  await shot('ingestion');
} catch (e) { console.error('ingestion_passed shot failed:', e.message); }

// 20. 入库阻断
try {
  await navigate('/ingestion?role=asset-admin&debug=1', 5000);
  await clickText('演示故障注入');
  await sleep(400);
  await evaluate(`(() => {
    const label = [...document.querySelectorAll('label')].find(l => l.textContent.includes('几何非流形开边'));
    label?.querySelector('input[type=checkbox]')?.click();
    const zone = [...document.querySelectorAll('div')].find(el => el.className && el.className.includes('border-dashed') && el.className.includes('rounded-md'));
    const dt = new DataTransfer();
    dt.items.add(new File(['y'.repeat(4096)], 'PierBody_broken.cbfc', { type: 'application/zip' }));
    zone.dispatchEvent(new DragEvent('drop', { bubbles: true, cancelable: true, dataTransfer: dt }));
    return true;
  })()`);
  await sleep(6000);
  await shot('ingestion_blocked');
} catch (e) { console.error('ingestion_blocked shot failed:', e.message); }

// 21. 监控看板
try {
  await navigate('/analytics?role=asset-admin', 5000);
  await shot('analytics');
} catch (e) { console.error('analytics shot failed:', e.message); }

// 22. 计算任务抽屉
try {
  await navigate('/components/PierBody/configurator?role=designer', 5000);
  await evaluate(`(() => {
    const inputs = [...document.querySelectorAll('input[type=number]')];
    const target = inputs.find(i => i.value === '12000');
    if (target) {
      target.value = '22000';
      target.dispatchEvent(new Event('change', { bubbles: true }));
    }
  })()`);
  await sleep(1200);
  await clickText('计算任务');
  await sleep(1000);
  await shot('job_center');
} catch (e) { console.error('job_center shot failed:', e.message); }

ws.close();
if (chromeProc) {
  chromeProc.kill();
  console.log('[CDP] Headless Chrome 实例已关闭');
}
console.log('=== 全量截图任务已全部完成！ ===');
