/* 安全边界快速验证：签名 token、角色与对象级授权 */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';
const results = [];
const check = (name, ok, detail = '') => {
  results.push({ name, ok, detail });
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? ' — ' + detail : ''}`);
};
async function login(role) {
  const res = await fetch(`${BASE}/api/v1/auth/login`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ role }),
  });
  return res.json();
}
async function status(path, token, method = 'GET') {
  const res = await fetch(`${BASE}${path}`, {
    method,
    headers: token ? { Authorization: `Bearer ${token}` } : {},
  });
  await res.text();
  return res.status;
}
const designer = await login('designer');
const partner = await login('partner');
check('签名 token 登录可用', /^demo\.u-designer\.\d+\.[A-Za-z0-9_-]+$/.test(designer.token), designer.token.slice(0, 28));
check('伪造 token 被拒绝', (await status('/api/v1/families', 'demo.u-asset.forged')) === 401);
check('匿名读取私有预览被拒绝', (await status('/api/v1/components/PierBody/preview.svg')) === 401);
check('匿名读取公开预览允许', (await status('/api/v1/components/BearingPad/preview.svg')) === 200);
check(
  '带 token 读取私有预览允许',
  (await status(`/api/v1/components/PierBody/preview.svg?token=${encodeURIComponent(designer.token)}`)) === 200,
);
check('合作方读取私有预览被拒绝', (await status('/api/v1/components/PierBody/preview.svg', partner.token)) === 403);
check('设计工程师读取审核队列被拒绝', (await status('/api/v1/reviews', designer.token)) === 403);
check('合作方读取审核队列被拒绝', (await status('/api/v1/reviews', partner.token)) === 403);
check('合作方读取运营看板被拒绝', (await status('/api/v1/analytics/overview', partner.token)) === 403);
check('合作方上传被拒绝', (await status('/api/v1/components/upload', partner.token, 'POST')) === 403);
check('合作方读取语义模板被拒绝', (await status('/api/v1/semantic-profiles', partner.token)) === 403);
check(
  'handoff 拒绝不属于构件的 cacheKey',
  (await fetch(`${BASE}/api/v1/auth/handoff-token`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${designer.token}` },
    body: JSON.stringify({ familyId: 'Abutment', cacheKey: 'a'.repeat(64) }),
  })).status === 422,
);

async function evaluate(familyId, token) {
  const detail = await (await fetch(`${BASE}/api/v1/components/${familyId}`, {
    headers: { Authorization: `Bearer ${token}` },
  })).json();
  const modelNumber = String(detail.parameters.schema[0]?.default ?? '');
  const response = await fetch(`${BASE}/api/v1/components/${familyId}/evaluate`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
    body: JSON.stringify({ modelNumber }),
  });
  const buffer = Buffer.from(await response.arrayBuffer());
  const jsonLen = buffer.readUInt32LE(0);
  return JSON.parse(buffer.subarray(4, 4 + jsonLen).toString('utf8'));
}
for (const familyId of ['PierBodyRect', 'PierTray', 'BoredPile']) {
  const header = await evaluate(familyId, designer.token);
  const bounds = header.bounds;
  const isAbutmentShape = bounds.min[0] === -6300 && bounds.max[2] > 5000;
  check(`${familyId} 不返回桥台几何`, !isAbutmentShape, `${header.stats?.volumeM3} m³`);
}
const failed = results.filter((r) => !r.ok);
console.log(`SECURITY DONE: ${results.length - failed.length}/${results.length}`);
if (failed.length) process.exitCode = 2;
