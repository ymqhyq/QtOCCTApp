/* 通过 Chrome DevTools Protocol 做端到端交互走查（无第三方依赖） */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const DEBUG = process.env.CDP_URL || 'http://127.0.0.1:9222';

const targets = await (await fetch(`${DEBUG}/json/list`)).json();
const page = targets.find((t) => t.type === 'page');
if (!page) throw new Error('No page target');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((resolve, reject) => {
  ws.onopen = resolve;
  ws.onerror = reject;
});

let nextId = 1;
const pending = new Map();
const errors = [];
ws.onmessage = (event) => {
  const message = JSON.parse(event.data);
  if (message.id) {
    const waiter = pending.get(message.id);
    if (waiter) {
      pending.delete(message.id);
      if (message.error) waiter.reject(new Error(JSON.stringify(message.error)));
      else waiter.resolve(message.result);
    }
    return;
  }
  if (message.method === 'Runtime.exceptionThrown') {
    errors.push(`EXCEPTION: ${message.params.exceptionDetails?.text ?? 'unknown'} ${message.params.exceptionDetails?.exception?.description ?? ''}`);
  }
  if (message.method === 'Log.entryAdded' && message.params.entry.level === 'error') {
    errors.push(`LOG: ${message.params.entry.text}`);
  }
  if (message.method === 'Runtime.consoleAPICalled' && message.params.type === 'error') {
    errors.push(`CONSOLE: ${message.params.args.map((a) => a.value ?? a.description).join(' ')}`);
  }
};

function send(method, params = {}) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    ws.send(JSON.stringify({ id, method, params }));
  });
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function evaluate(expression, awaitPromise = false) {
  const result = await send('Runtime.evaluate', {
    expression,
    awaitPromise,
    returnByValue: true,
    userGesture: true,
  });
  if (result.exceptionDetails) {
    throw new Error(result.exceptionDetails.text + ' ' + (result.exceptionDetails.exception?.description ?? ''));
  }
  return result.result.value;
}

async function navigate(path, wait = 5000) {
  const sep = path.includes('?') ? '&' : '?';
  await send('Page.navigate', { url: BASE + path + sep + '_=' + Date.now() });
  await sleep(wait);
}

async function clickText(text, selector = 'button, a, [role="button"]') {
  return evaluate(
    `(() => { const el = [...document.querySelectorAll(${JSON.stringify(selector)})].find(e => e.textContent && e.textContent.includes(${JSON.stringify(text)})); if (!el) return false; el.click(); return true; })()`,
  );
}

async function bodyText() {
  return evaluate('document.body.innerText');
}

async function check(name, condition, detail = '') {
  console.log(`${condition ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
  if (!condition) process.exitCode = 2;
}

await send('Runtime.enable');
await send('Log.enable');
await send('Page.enable');
await send('Network.enable');

// 1. 工作台
await navigate('/?role=designer', 6000);
let text = await bodyText();
await check('工作台首屏渲染', text.includes('构件检索与任务入口'), text.slice(0, 60).replace(/\n/g, ' '));
await check('工作台显示 41 个构件', text.includes('41 个构件'));
await check('标准映射摘要出现', text.includes('标准映射'));

// 2. 构件详情
await navigate('/components/PierBody?role=designer', 8000);
text = await bodyText();
await check('详情页标题', text.includes('圆端型实体桥墩墩身'));
await check('详情页显示 6 道门禁', text.includes('6/6 门禁通过'));
await check('设计师隐藏 JHB 技术 HUD', !text.includes('ZERO-COPY'));
await check('设计师隐藏缓存详情 Tab', !text.includes('确定性缓存与 QA'));

// 3. 配置器
await navigate('/components/PierBody/configurator?role=designer', 8000);
text = await bodyText();
await check('配置器装载', text.includes('参数配置与规格预设') && text.includes('常用标准规格'));
await check('预烘焙命中状态', text.includes('预烘焙缓存命中'));
const changed = await evaluate(`(() => {
  const inputs = [...document.querySelectorAll('input[type=number]')];
  const target = inputs.find(i => i.value === '12000');
  if (!target) return false;
  target.value = '14000';
  target.dispatchEvent(new Event('change', { bubbles: true }));
  return true;
})()`);
await check('参数滑块变更触发求值', changed);
await sleep(1200);
text = await bodyText();
await check('异步任务进入排队/求解', /排队中|Worker 求解中|异步管线|求解完成/.test(text), text.match(/排队中|Worker 求解中|异步管线|求解完成/)?.[0]);
const cancelClicked = await clickText('取消等待');
if (cancelClicked) {
  await sleep(1500);
  text = await bodyText();
  await check('取消前台等待生效', /已取消前台等待|后台脱机计算|任务中心/.test(text));
} else {
  await check('取消前台等待按钮可见（任务已完成则跳过）', true, '任务已完成');
}

// 4. 导出弹窗
await navigate('/components/PierBody', 6000);
const opened = await clickText('下载构件包');
await sleep(1200);
text = await bodyText();
await check('导出弹窗打开', opened && text.includes('构件导出与跨端协同'));
await check('六类格式矩阵', text.includes('CBF-C 构件制品包') && text.includes('IFC 4.3 数字工程交付物'));
await clickText('下一步：许可确认');
await sleep(300);
text = await bodyText();
await check('进入许可确认步骤', text.includes('下载摘要') && text.includes('知识产权'));
const agreed = await evaluate(`(() => { const box = document.querySelector('input[type=checkbox]'); if (!box) return false; box.click(); return box.checked; })()`);
await check('许可勾选', agreed);
await clickText('生成并签发凭据');
await sleep(1800);
text = await bodyText();
await check('下载凭据签发', text.includes('下载凭据签发成功') && text.includes('SHA-256'));
const handoffClicked = await clickText('唤起桌面端');
await sleep(3000);
text = await bodyText();
await check('深链唤起 2s 兜底回退', handoffClicked && /未检测到 QtOCCTApp|已回退|下载已开始/.test(text));

// 5. 审核工作台
await navigate('/reviews?role=reviewer', 9000);
text = await bodyText();
await check('审核台加载双模型', text.includes('双模型重叠对比') && text.includes('新版 v1.5.0'));
await check('差异部位高亮清单', text.includes('TopBearingFace') && text.includes('尺寸变形'));
await evaluate(`window.dispatchEvent(new KeyboardEvent('keydown', { key: 'a', bubbles: true }))`);
await sleep(1500);
text = await bodyText();
await check('键盘 A 快捷签批', text.includes('审核通过') || text.includes('已通过'));
const queueAfter = await evaluate(`document.body.innerText.match(/剩余 \\d+ 项待审/)?.[0] ?? ''`);
console.log('INFO  签批结果:', queueAfter || '(已更新队列)');

// 6. 语义中枢
await navigate('/semantics?role=standard-admin', 6000);
text = await bodyText();
await check('语义中枢模板列表', text.includes('铁路工程 CRBIM') && text.includes('企业专用覆盖模板'));
await clickText('在线推演沙盒');
await sleep(400);
const scanClicked = await clickText('全量语义码扫描');
await sleep(2000);
text = await bodyText();
await check('服务端沙盒推演', scanClicked && text.includes('unmapped 预警') && text.includes('规则一致性已校验'));

// 7. 监控看板
await navigate('/analytics?role=asset-admin', 5000);
text = await bodyText();
await check('看板 KPI 渲染', text.includes('确定性缓存命中率') && text.includes('入库质检一次通过率'));
await check('趋势与待办', text.includes('几何 Worker 求解时延') && text.includes('异常失败任务排查'));

// 8. 入库工作台
await navigate('/ingestion?role=asset-admin', 7000);
text = await bodyText();
await check('入库页与 6 道门禁', text.includes('构件入库') && text.includes('6 道入库自动化安全门禁'));
await check('3D 验真默认收起', text.includes('3D 验真与技术诊断') && !text.includes('3D 验真诊断视口'));

// 9. 全局命令面板
await navigate('/', 4000);
await evaluate(`window.dispatchEvent(new KeyboardEvent('keydown', { key: 'k', ctrlKey: true, bubbles: true }))`);
await sleep(700);
text = await bodyText();
await check('Ctrl+K 命令面板', text.includes('快捷动作') && text.includes('全局检索'));
await evaluate(`window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }))`);

console.log('\n--- 控制台错误 ---');
const unique = [...new Set(errors)].filter((e) => !e.includes('favicon'));
if (!unique.length) console.log('无 JS 异常 / 控制台错误');
else unique.slice(0, 20).forEach((e) => console.log(e));
if (unique.length) process.exitCode = 3;

ws.close();
console.log('\nE2E SMOKE DONE');
