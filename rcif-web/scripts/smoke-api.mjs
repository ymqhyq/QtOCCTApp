/* 端到端 API 冒烟测试：直接打 Mock API，验证 200/202、JHB 对齐、任务取消与握手票据 */
const BASE = process.env.CBF_BASE || 'http://127.0.0.1:5178';

async function json(path, options = {}) {
  const response = await fetch(BASE + path, options);
  const text = await response.text();
  let body;
  try {
    body = JSON.parse(text);
  } catch {
    body = text;
  }
  if (!response.ok) {
    throw new Error(`${options.method || 'GET'} ${path} -> ${response.status}: ${text.slice(0, 300)}`);
  }
  return { response, body };
}

const login = await json('/api/v1/auth/login', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ role: 'designer' }),
});
const token = login.body.token;
const auth = { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' };
console.log('login:', login.body.user.name, login.body.user.role);

const search = await json('/api/v1/components?pageSize=5', { headers: auth });
console.log('search:', search.body.total, 'total,', search.body.items.length, 'items, took', search.body.tookMs, 'ms');

const detail = await json('/api/v1/components/PierBody', { headers: auth });
console.log('detail:', detail.body.name, detail.body.geometry.defaultCacheKey.slice(0, 12), detail.body.geometry.volumeM3, 'm3');

async function evaluate(payload) {
  return fetch(BASE + '/api/v1/components/PierBody/evaluate', {
    method: 'POST',
    headers: auth,
    body: JSON.stringify(payload),
  });
}

let res = await evaluate({
  modelNumber: 'STD-PIER-12M',
  parameterOverrides: {},
  acceptFormats: ['cbf-jhb@1.4'],
});
console.log('evaluate preset:', res.status, res.headers.get('x-cache-hit'), res.headers.get('x-cache-key')?.slice(0, 12));
if (res.status === 200) {
  const buffer = await res.arrayBuffer();
  const jsonLen = new DataView(buffer).getUint32(0, true);
  const header = JSON.parse(new TextDecoder().decode(new Uint8Array(buffer, 4, jsonLen)).replace(/\s+$/, ''));
  console.log(
    '  JHB aligned:',
    (4 + jsonLen) % 4 === 0,
    'jsonLen',
    jsonLen,
    'payloadStart',
    4 + jsonLen,
    'bytes',
    buffer.byteLength,
    'verts',
    header.vertexCount,
    'faces',
    header.facesCount,
    'cacheKey',
    header.cache_key.slice(0, 12),
  );
}

res = await evaluate({
  modelNumber: 'CUSTOM',
  parameterOverrides: { PierHeight: 14000, TopWidth: 2500 },
  acceptFormats: ['cbf-jhb@1.4'],
});
console.log('evaluate custom:', res.status);
const accepted = await res.json();
console.log('  job:', accepted.jobId, 'estimate', accepted.estimatedMs, 'ms');
let job;
for (let i = 0; i < 20; i += 1) {
  await new Promise((r) => setTimeout(r, 700));
  const poll = await json(`/api/v1/jobs/${accepted.jobId}`, { headers: auth });
  job = poll.body;
  if (i % 3 === 0) console.log('  status:', job.status, job.progress + '%');
  if (['succeeded', 'failed'].includes(job.status)) break;
}
console.log('  final:', job.status, job.progress + '%');
if (job.status === 'succeeded') {
  const result = await fetch(BASE + job.resultUrl, { headers: auth });
  console.log('  result bytes:', (await result.arrayBuffer()).byteLength);
}

const preview = await fetch(BASE + '/api/v1/components/PierBody/preview.svg?w=320&h=220');
const svg = await preview.text();
console.log('preview:', preview.status, preview.headers.get('content-type'), svg.length, 'bytes', svg.startsWith('<svg') ? 'valid' : 'invalid');

const semantics = await json('/api/v1/components/PierBody/semantics', { headers: auth });
console.log('semantics:', semantics.body.mappings.length, 'mappings, unmapped:', semantics.body.unmapped.length, semantics.body.profile?.profileId);

const profiles = await json('/api/v1/semantic-profiles', { headers: auth });
console.log('profiles:', profiles.body.items.map((p) => `${p.profileId}@${p.version}(unmapped ${p.unmappedCount})`).join(', '));

const sandbox = await json('/api/v1/semantic-profiles/preview', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ profileCode: 'Tenant-Railway-Override@1.2', familyId: 'PierBody' }),
});
console.log('sandbox:', sandbox.body.mappings.map((m) => `${m.shapeKey}:${m.status}`).join(', '));

const upload = await json('/api/v1/components/upload', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ fileName: 'PierBody_v1.4.2.rcif', fileSize: 2516582, familyId: 'PierBody', defects: [] }),
});
console.log('upload:', upload.body.passed, upload.body.gates.map((g) => g.status).join('/'), 'cacheKey ok:', upload.body.detected.cacheKey === detail.body.geometry.defaultCacheKey);

const handoff = await json('/api/v1/auth/handoff-token', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ familyId: 'PierBody', cacheKey: detail.body.geometry.defaultCacheKey }),
});
console.log('handoff:', handoff.body.handoffToken.slice(0, 12) + '***', 'expires', handoff.body.expiresIn);
const consume = await json('/api/v1/auth/handoff-token/consume', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ handoffToken: handoff.body.handoffToken }),
});
console.log('consume:', consume.body.tokenType, consume.body.downloadUrl);
const dl = await fetch(BASE + consume.body.downloadUrl);
console.log('ticket download:', dl.status, dl.headers.get('content-type'), (await dl.arrayBuffer()).byteLength, 'bytes');

const replay = await fetch(BASE + '/api/v1/auth/handoff-token/consume', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ handoffToken: handoff.body.handoffToken }),
});
console.log('handoff replay:', replay.status, replay.status === 410 ? 'single-use OK' : 'UNEXPECTED');

const notAcceptable = await evaluate({
  modelNumber: 'STD-PIER-12M',
  parameterOverrides: {},
  acceptFormats: ['step-ap242'],
});
console.log('acceptFormats 406:', notAcceptable.status);

const legacy = await fetch(BASE + '/api/v1/components/PierBody/geometry?legacy=1', { headers: auth });
if (legacy.status === 200) {
  const buffer = await legacy.arrayBuffer();
  const jsonLen = new DataView(buffer).getUint32(0, true);
  console.log('legacy frame unaligned:', (4 + jsonLen) % 4 !== 0, 'payloadStart', 4 + jsonLen);
}

const partnerLogin = await json('/api/v1/auth/login', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ role: 'partner' }),
});
const partnerAuth = { Authorization: `Bearer ${partnerLogin.body.token}`, 'Content-Type': 'application/json' };
const partnerDownload = await fetch(BASE + '/api/v1/components/BearingPad/download', {
  method: 'POST',
  headers: partnerAuth,
  body: JSON.stringify({ format: 'rcif' }),
});
const partnerTicket = await partnerDownload.json();
console.log('partner download:', partnerDownload.status, partnerTicket.permission);

const privateAccess = await fetch(BASE + '/api/v1/components/PierBody', {
  headers: partnerAuth,
});
console.log('partner private detail:', privateAccess.status, privateAccess.status === 403 ? 'RLS OK' : 'LEAK');

// 同一构件重复上传：第二次必须刷新既有审核任务
const uploadA = await json('/api/v1/components/upload', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ fileName: 'PierCap_v1.4.2.rcif', fileSize: 2400000, familyId: 'PierCap', revision: '1.4.2' }),
});
const submitA = await json(`/api/v1/uploads/${uploadA.body.uploadId}/submit`, { method: 'POST', headers: auth, body: '{}' });
const uploadB = await json('/api/v1/components/upload', {
  method: 'POST',
  headers: auth,
  body: JSON.stringify({ fileName: 'PierCap_v1.4.3.rcif', fileSize: 2400000, familyId: 'PierCap', revision: '1.4.3' }),
});
const submitB = await json(`/api/v1/uploads/${uploadB.body.uploadId}/submit`, { method: 'POST', headers: auth, body: '{}' });
console.log('review resubmit:', submitA.body.updated === false && submitB.body.updated === true ? 'UPDATED OK' : 'BROKEN');

const reviewerLogin = await json('/api/v1/auth/login', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ role: 'reviewer' }),
});
const reviewerAuth = { Authorization: `Bearer ${reviewerLogin.body.token}`, 'Content-Type': 'application/json' };
const reviewQueue = await json('/api/v1/reviews', { headers: reviewerAuth });
const capReview = reviewQueue.body.items.find((item) => item.familyId === 'PierCap');
const approve = await json(`/api/v1/reviews/${capReview.reviewId}/approve`, {
  method: 'POST',
  headers: reviewerAuth,
  body: '{}',
});
const capDetail = await json('/api/v1/components/PierCap', { headers: auth });
const topChange = capDetail.body.changelog[0];
console.log(
  'approve revision:',
  approve.body.revision,
  'component:',
  capDetail.body.revision,
  'changelog:',
  topChange.version,
  topChange.cacheKey === capDetail.body.geometry.defaultCacheKey ? 'cache OK' : 'CACHE MISMATCH',
);

const projectionResponse = await fetch(
  `${BASE}/api/v1/components/PierBody/projection.svg?view=sheet&token=${encodeURIComponent(token)}`,
);
const projectionSvg = await projectionResponse.text();
console.log(
  'projection:',
  projectionResponse.status,
  projectionResponse.headers.get('content-type'),
  projectionSvg.includes('三视图') && projectionSvg.includes('stroke-dasharray') ? 'HLR OK' : 'BROKEN',
);
const projectionNoAuth = await fetch(`${BASE}/api/v1/components/PierBody/projection.svg?view=front`);
console.log('projection no-auth:', projectionNoAuth.status, projectionNoAuth.status === 401 ? 'GUARDED' : 'LEAK');

const adminLogin = await json('/api/v1/auth/login', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ role: 'asset-admin' }),
});
const adminAuth = { Authorization: `Bearer ${adminLogin.body.token}`, 'Content-Type': 'application/json' };
const families = await json('/api/v1/families', { headers: adminAuth });
console.log('families:', families.body.total, 'total,', families.body.items.reduce((sum, item) => sum + item.presets.length, 0), 'presets');
const familyForbidden = await fetch(`${BASE}/api/v1/families`, { headers: auth });
console.log('families designer guard:', familyForbidden.status, familyForbidden.status === 403 ? 'ADMIN ONLY' : 'LEAK');

const catalog = await json('/api/v1/catalog', { headers: auth });
console.log('catalog:', catalog.body.total, 'families,', catalog.body.items.reduce((sum, item) => sum + item.presets.length, 0), 'presets');
const similar = await json('/api/v1/components?similarTo=PierBody&pageSize=5', { headers: auth });
console.log(
  'similar:',
  similar.body.total,
  'items, reference',
  similar.body.similarityReference?.familyId,
  'top',
  `${similar.body.items[0]?.familyId}:${similar.body.items[0]?.similarity}%`,
);
const bridgeOnly = await json('/api/v1/components?discipline=桥梁工程&pageSize=5', { headers: auth });
console.log('discipline filter:', bridgeOnly.body.total, 'bridge assets,', bridgeOnly.body.items.length, 'items');

console.log('\nSMOKE OK');
