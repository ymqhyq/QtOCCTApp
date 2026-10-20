const DEBUG = 'http://127.0.0.1:9222';
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
const result = await send('Runtime.evaluate', {
  expression: `(() => {
    const rows=[...document.querySelectorAll('[data-family-id="PierBody"]')];
    return rows.map(r=>({text:r.innerText,children:r.children.length,rect:r.getBoundingClientRect().toJSON()}));
  })()`,
  returnByValue: true,
});
console.log(JSON.stringify(result.result.value, null, 2));
ws.close();
