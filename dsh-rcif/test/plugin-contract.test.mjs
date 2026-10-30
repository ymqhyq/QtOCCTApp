/**
 * RCIF host-plugin contract test.
 *
 * Runs the plugin against a fake Cordis context shaped exactly like the DSH
 * services it injects (webServer.register, connection fence, tools.register).
 * No DSH process and no real CadQuery/Python run are required: the injected
 * process runner stands in for the toolchain.
 *
 * Run: node --test dsh-rcif/test/plugin-contract.test.mjs
 */

import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const pluginModule = require('../dist/index.js');
const HERE = path.dirname(fileURLToPath(import.meta.url));

const WORKSPACE = fs.mkdtempSync(path.join(HERE, '.contract-'));
const TOOLS = path.join(WORKSPACE, 'rcif-tools');
const FAMILY = path.join(WORKSPACE, 'abutment_family');

function write(file, content) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, content);
}

let server;
let baseUrl;
const routes = [];
let rejection;
let processCalls = [];
let enableSession = false;
const sessionPrompts = [];
let fakeWorkspaces = [];
let workspaceSeq = 0;
let sessionSeq = 0;

function dispatch(req, res) {
  let pathname;
  try {
    pathname = new URL(req.url, 'http://x').pathname;
  } catch {
    res.statusCode = 400;
    res.end();
    return;
  }
  const exact = routes.find((route) => route.kind === 'exact' && route.path === pathname);
  const prefixes = routes
    .filter((route) => route.kind === 'prefix' && (pathname === route.path || pathname.startsWith(`${route.path}/`)))
    .sort((a, b) => b.path.length - a.path.length);
  const route = exact || prefixes[0];
  if (!route) {
    res.statusCode = 404;
    res.end();
    return;
  }
  Promise.resolve(route.handler(req, res)).catch((error) => {
    if (!res.headersSent) {
      res.statusCode = 500;
      res.setHeader('content-type', 'application/json');
      res.end(JSON.stringify({ error: String(error) }));
    } else {
      res.destroy();
    }
  });
}

function fakeCtx() {
  const browserPeer = { principal: { kind: 'owner' } };
  return {
    effect(fn) {
      return fn();
    },
    webServer: {
      getOrigin: () => baseUrl,
      register(route) {
        routes.push(route);
        return () => {};
      },
    },
    connection: {
      browserPeer,
      requestRejection: () => rejection,
      admit: () => (rejection ? { rejection } : { peer: browserPeer }),
    },
    tools: {
      registered: [],
      register(definition) {
        this.registered.push(definition);
        return () => {};
      },
    },
    logger: { info() {}, warn() {}, error() {} },
    get(name) {
      if (name === 'workspaceRegistry') {
        return {
          list: () => fakeWorkspaces,
          get: (workspaceId) => fakeWorkspaces.find((workspace) => workspace.id === workspaceId),
        };
      }
      if (name === 'workspaceController') {
        return {
          async create({ path: workspacePath }) {
            const workspace = {
              id: `ws-${++workspaceSeq}`,
              path: workspacePath,
              title: path.basename(workspacePath),
              sessionIds: [],
              createdAt: new Date().toISOString(),
              updatedAt: new Date().toISOString(),
            };
            fakeWorkspaces.push(workspace);
            return { workspace: { ...workspace, workspaceId: workspace.id }, created: true };
          },
        };
      }
      if (name === 'sessionController' && enableSession) {
        return {
          async prompt(request, signal) {
            sessionPrompts.push({ request, signal });
          },
          async create({ workspaceId }) {
            const workspace = fakeWorkspaces.find((item) => item.id === workspaceId);
            const sessionId = `session-created-${++sessionSeq}`;
            if (workspace) workspace.sessionIds.push(sessionId);
            return { sessionId };
          },
        };
      }
      return undefined;
    },
  };
}

const fakeRunner = async (_python, args, options) => {
  const script = path.basename(args[0]);
  processCalls.push({ script, args, cwd: options?.cwd });
  if (script === 'verify_sandbox.py') {
    return {
      code: 0,
      stdout: JSON.stringify({
        passed: true,
        blocker_count: 0,
        major_count: 0,
        minor_count: 0,
        duration_sec: 0.01,
        table_truth_sha256: 'stub-truth',
        params_sha256: 'stub-params',
        checks: [{ gate_id: 'GATE-01', passed: true }],
      }),
      stderr: '',
      timedOut: false,
    };
  }
  if (script === 'pack_rcif_cli.py') {
    const out = args[2];
    const approvalIndex = args.indexOf('--approval');
    const hasApproval = approvalIndex !== -1;
    write(
      out.replace(/\.rcif$/i, '.provenance.json'),
      JSON.stringify({
        schema: 'rcif-provenance/1',
        signatureStatus: hasApproval ? 'verified' : 'unsigned-dev-build',
        packageDigest: 'stub-package-digest',
      }),
    );
    return { code: 0, stdout: `[PACKAGED] ${out}\nSignature Status: verified\n`, stderr: '', timedOut: false };
  }
  if (['step2rcif.py', 'ifc2rcif.py', 'pdf2rcif.py'].includes(script)) {
    const out = args[2];
    fs.mkdirSync(path.dirname(out), { recursive: true });
    fs.copyFileSync(path.join(WORKSPACE, 'sample.rcif'), out);
    return { code: 0, stdout: `[OK] 生成成功: ${out}\n`, stderr: '', timedOut: false };
  }
  return { code: 0, stdout: '{}', stderr: '', timedOut: false };
};

let ctx;
let dispose;

test.before(async () => {
  write(path.join(TOOLS, 'verify_sandbox.py'), '# stub\n');
  write(path.join(TOOLS, 'pack_rcif_cli.py'), '# stub\n');
  write(path.join(TOOLS, 'step2rcif.py'), '# stub\n');
  write(path.join(TOOLS, 'ifc2rcif.py'), '# stub\n');
  write(path.join(TOOLS, 'pdf2rcif.py'), '# stub\n');
  write(path.join(FAMILY, 'table_truth.json'), JSON.stringify({ rows: [] }));
  write(path.join(FAMILY, 'params.csv'), 'ModelNumber\nA\n');
  write(path.join(FAMILY, 'rcif', 'display', 'preview.glb'), Buffer.from('glTF-stub'));
  fs.copyFileSync(
    path.resolve(HERE, '../../abutment_family/rcif/StraightAbutment_1.0.0.rcif'),
    path.join(WORKSPACE, 'sample.rcif'),
  );

  server = http.createServer(dispatch);
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  baseUrl = `http://127.0.0.1:${server.address().port}`;

  ctx = fakeCtx();
  dispose = pluginModule.apply(ctx, {
    workspaceRoot: WORKSPACE,
    toolsDir: TOOLS,
    pythonPath: 'python',
    defaultFamilyDir: 'abutment_family',
    runProcess: fakeRunner,
    libraryUrl: 'http://127.0.0.1:1',
  });
});

test.after(async () => {
  if (typeof dispose === 'function') dispose();
  await new Promise((resolve) => server.close(resolve));
  fs.rmSync(WORKSPACE, { recursive: true, force: true });
});

test.beforeEach(() => {
  rejection = undefined;
  processCalls = [];
  enableSession = false;
  sessionPrompts.length = 0;
  fakeWorkspaces = [];
  workspaceSeq = 0;
  sessionSeq = 0;
});

async function initToken(sessionId = 'session-test') {
  const response = await fetch(`${baseUrl}/rcif-studio/api/init-token?sessionId=${sessionId}`);
  assert.equal(response.status, 200);
  return response.json();
}

function jsonPost(url, body, token) {
  return fetch(`${baseUrl}${url}`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      ...(token ? { 'x-rcif-cap-token': token } : {}),
    },
    body: JSON.stringify(body),
  });
}

test('registers the real DSH route kinds and all browser endpoints', () => {
  const exact = routes.filter((route) => route.kind === 'exact').map((route) => route.path).sort();
  assert.deepEqual(exact, [
    '/rcif-studio/api/annotation-history',
    '/rcif-studio/api/approve-control-grid',
    '/rcif-studio/api/assistant',
    '/rcif-studio/api/command',
    '/rcif-studio/api/compile-package',
    '/rcif-studio/api/feedback',
    '/rcif-studio/api/generic/rebuild',
    '/rcif-studio/api/health',
    '/rcif-studio/api/ingest',
    '/rcif-studio/api/init-token',
    '/rcif-studio/api/library/ingest',
    '/rcif-studio/api/package',
    '/rcif-studio/api/package/edit',
    '/rcif-studio/api/package/patch',
    '/rcif-studio/api/package/preview.glb',
    '/rcif-studio/api/package/source',
    '/rcif-studio/api/preview.glb',
    '/rcif-studio/api/run-sandbox',
    '/rcif-studio/api/session-files',
    '/rcif-studio/api/session-inject',
    '/rcif-studio/api/session-meta',
    '/rcif-studio/api/sessions',
    '/rcif-studio/api/sessions/delete',
    '/rcif-studio/api/state',
    '/rcif-studio/api/tree',
    '/rcif-studio/api/upload',
    '/rcif-studio/api/workspaces',
    '/rcif-studio/api/workspaces/delete',
  ]);
  assert.ok(routes.some((route) => route.kind === 'prefix' && route.path === '/rcif-studio/workbench'));
  assert.ok(!routes.some((route) => route.path === '/rcif-studio/api/compile-package' && route.kind !== 'exact'));
});

test('registers seven tools with the DSH output contract', () => {
  assert.deepEqual(
    ctx.tools.registered.map((tool) => tool.name).sort(),
    [
      'rcif_compile_package',
      'rcif_ingest_ifc',
      'rcif_ingest_pdf',
      'rcif_ingest_rfa',
      'rcif_ingest_step',
      'rcif_open_package',
      'rcif_patch_package',
      'rcif_rebuild_generic',
      'rcif_verify_sandbox',
    ],
  );
  for (const tool of ctx.tools.registered) {
    assert.equal(typeof tool.execute, 'function');
    assert.equal(typeof tool.output.render, 'function');
    assert.equal(tool.output.schema.type, 'object');
    assert.ok(tool.parameters.properties);
  }
});

test('health is gated by the DSH connection fence', async () => {
  let response = await fetch(`${baseUrl}/rcif-studio/api/health`);
  assert.equal(response.status, 200);
  rejection = 403;
  response = await fetch(`${baseUrl}/rcif-studio/api/health`);
  assert.equal(response.status, 403);
});

test('init-token issues a session-bound capability token', async () => {
  const data = await initToken('session-a');
  assert.equal(data.sessionId, 'session-a');
  assert.equal(data.expiresIn, 900);
  assert.match(data.capToken, /^owner\|session-a\|/);
  assert.ok(data.pubkey.length > 20);
});

test('run-sandbox rejects missing token and path traversal, then returns the report', async () => {
  const { capToken } = await initToken('session-a');

  let response = await jsonPost('/rcif-studio/api/run-sandbox', { sessionId: 'session-a', targetDir: 'abutment_family' });
  assert.equal(response.status, 403);

  response = await jsonPost(
    '/rcif-studio/api/run-sandbox',
    { sessionId: 'session-a', targetDir: '../outside' },
    capToken,
  );
  assert.equal(response.status, 403);
  assert.equal((await response.json()).code, 'PATH_OUTSIDE_WORKSPACE');

  response = await jsonPost(
    '/rcif-studio/api/run-sandbox',
    { sessionId: 'session-a', targetDir: 'abutment_family' },
    capToken,
  );
  assert.equal(response.status, 200);
  const data = await response.json();
  assert.equal(data.ok, true);
  assert.equal(data.passed, true);
  assert.equal(data.report.checks[0].gate_id, 'GATE-01');
  assert.equal(processCalls[0].script, 'verify_sandbox.py');
});

test('approval is Ed25519-signed and verify() rejects tampering', async () => {
  const { capToken } = await initToken('session-a');
  const response = await jsonPost('/rcif-studio/api/approve-control-grid', { sessionId: 'session-a' }, capToken);
  assert.equal(response.status, 200);
  const { approval } = await response.json();
  assert.equal(approval.approvalAlgorithm, 'Ed25519');
  assert.equal(approval.approvalVerified, true);
  assert.ok(approval.approvalSignature);

  const payload = {
    approvalId: approval.approvalId,
    approvedBy: approval.approvedBy,
    approvedExactBrep: approval.approvedExactBrep,
    approvedParamsSha256: approval.approvedParamsSha256,
    approvedSourceMode: approval.approvedSourceMode,
    approvedTableTruthSha256: approval.approvedTableTruthSha256,
    sessionId: approval.sessionId,
    signedAt: approval.signedAt,
  };
  const canonical = JSON.stringify(payload, [
    'approvalId',
    'approvedBy',
    'approvedExactBrep',
    'approvedParamsSha256',
    'approvedSourceMode',
    'approvedTableTruthSha256',
    'sessionId',
    'signedAt',
  ]);
  const spkiPrefix = Buffer.from('302a300506032b6570032100', 'hex');
  const tokenData = await initToken('session-a');
  const raw = Buffer.from(tokenData.pubkey, 'base64url');
  const key = crypto.createPublicKey({
    key: Buffer.concat([spkiPrefix, raw]),
    format: 'der',
    type: 'spki',
  });
  assert.equal(
    crypto.verify(null, Buffer.from(canonical, 'utf8'), key, Buffer.from(approval.approvalSignature, 'base64url')),
    true,
    'approval signature must verify against the handshake public key',
  );
});

test('compile requires a fresh approval and passes it to the packer', async () => {
  const { capToken } = await initToken('session-a');

  let response = await jsonPost('/rcif-studio/api/compile-package', { sessionId: 'session-a' }, capToken);
  assert.equal(response.status, 403);
  assert.equal((await response.json()).code, 'APPROVAL_REQUIRED');

  const approved = await (
    await jsonPost('/rcif-studio/api/approve-control-grid', { sessionId: 'session-a' }, capToken)
  ).json();
  const tampered = { ...approved.approval, approvedBy: 'mallory' };
  response = await jsonPost(
    '/rcif-studio/api/compile-package',
    { sessionId: 'session-a', approval: tampered },
    capToken,
  );
  assert.equal(response.status, 403);
  assert.equal((await response.json()).code, 'APPROVAL_INVALID');

  response = await jsonPost(
    '/rcif-studio/api/compile-package',
    { sessionId: 'session-a', approval: approved.approval },
    capToken,
  );
  assert.equal(response.status, 200);
  const data = await response.json();
  assert.equal(data.ok, true);
  assert.equal(data.provenance.signatureStatus, 'verified');
  const packCall = processCalls.find((call) => call.script === 'pack_rcif_cli.py');
  assert.ok(packCall.args.includes('--approval'));
  assert.ok(packCall.args.includes('--pubkey'));
});

test('session-inject sanitises input, rejects replay and expired requests', async () => {
  const { capToken } = await initToken('session-a');
  const nonce = 'nonce-contract-1';
  const body = {
    sessionId: 'session-a',
    nonce,
    expiresAt: Date.now() + 30000,
    annotations: [{ file: 'generators/generator.py', lines: [40, 43], comment: '<script>alert(1)</script> 加宽垫石' }],
  };
  let response = await jsonPost('/rcif-studio/api/session-inject', body, capToken);
  assert.equal(response.status, 200);
  const data = await response.json();
  assert.equal(data.annotationCount, 1);
  assert.ok(!data.injectedPrompt.includes('<script>'));
  assert.equal(data.delivered, false);
  assert.ok(data.deliveryReason.length > 0);

  response = await jsonPost('/rcif-studio/api/session-inject', body, capToken);
  assert.equal(response.status, 409);
  assert.equal((await response.json()).code, 'REPLAY_DETECTED');

  response = await jsonPost(
    '/rcif-studio/api/session-inject',
    { ...body, nonce: 'nonce-contract-2', expiresAt: Date.now() - 1000 },
    capToken,
  );
  assert.equal(response.status, 409);
});

test('approval canonical payload matches the Python packer byte-for-byte', () => {
  const fixture = JSON.parse(
    fs.readFileSync(path.resolve(HERE, '../../rcif-web/shared/fixtures/approval_canonical.json'), 'utf8'),
  );
  const fields = [
    'approvalId',
    'approvedBy',
    'approvedExactBrep',
    'approvedParamsSha256',
    'approvedSourceMode',
    'approvedTableTruthSha256',
    'sessionId',
    'signedAt',
  ];
  assert.deepEqual(fields, fixture.fields);
  assert.equal(JSON.stringify(fixture.payload, fields), fixture.canonical);
  assert.equal(
    crypto.createHash('sha256').update(fixture.canonical, 'utf8').digest('hex'),
    fixture.sha256,
  );
});

test('workbench serves files, blocks traversal and serves the real GLB', async () => {
  let response = await fetch(`${baseUrl}/rcif-studio/workbench/index.html`);
  assert.equal(response.status, 200);
  assert.match(response.headers.get('content-type'), /text\/html/);
  assert.match(await response.text(), /RCIF/);

  response = await fetch(`${baseUrl}/rcif-studio/workbench/%2e%2e%2fdist%2findex.js`);
  assert.equal(response.status, 403);

  response = await fetch(`${baseUrl}/rcif-studio/api/preview.glb`);
  assert.equal(response.status, 200);
  assert.equal(response.headers.get('content-type'), 'model/gltf-binary');
  assert.equal(await response.text(), 'glTF-stub');
});

test('tools stay inside the workspace', async () => {
  const verify = ctx.tools.registered.find((tool) => tool.name === 'rcif_verify_sandbox');
  const escape = await verify.execute({ familyDir: 'C:\\Windows' }, {});
  assert.equal(escape.ok, false);
  assert.match(escape.summary, /越界|路径/);

  const ok = await verify.execute({ familyDir: 'abutment_family' }, {});
  assert.equal(ok.ok, true);
  assert.match(ok.summary, /门禁通过/);

  const step = ctx.tools.registered.find((tool) => tool.name === 'rcif_ingest_step');
  const missing = await step.execute({ stepPath: 'missing.step' }, {});
  assert.equal(missing.ok, false);
});

test('package inspection exposes RCIF metadata, preview and source', async () => {
  let response = await fetch(`${baseUrl}/rcif-studio/api/package?path=sample.rcif`);
  assert.equal(response.status, 200);
  const meta = await response.json();
  assert.equal(meta.metadata.familyId, 'StraightAbutment');
  assert.equal(meta.metadata.revision, '1.0.0');
  assert.equal(meta.metadata.sourceMode, 'parametric');
  assert.equal(meta.metadata.preview.available, true);
  assert.equal(meta.metadata.shapeCount > 0, true);
  assert.equal(meta.packageSha256.length, 64);
  assert.ok(meta.files.some((file) => file.path === 'component.json'));

  response = await fetch(`${baseUrl}/rcif-studio/api/package/preview.glb?path=sample.rcif`);
  assert.equal(response.status, 200);
  assert.equal(response.headers.get('content-type'), 'model/gltf-binary');
  const glb = Buffer.from(await response.arrayBuffer());
  assert.equal(glb.subarray(0, 4).toString('ascii'), 'glTF');

  response = await fetch(`${baseUrl}/rcif-studio/api/package/source?path=sample.rcif`);
  assert.equal(response.status, 200);
  const source = await response.json();
  assert.equal(source.available, false);

  response = await fetch(`${baseUrl}/rcif-studio/api/package?path=%2e%2e%2foutside.rcif`);
  assert.equal(response.status, 403);
});

test('PDF direct conversion is rejected; assistant tool remains the only PDF path', async () => {
  const { capToken } = await initToken('session-a');
  const response = await jsonPost(
    '/rcif-studio/api/ingest',
    {
      sessionId: 'session-a',
      kind: 'pdf',
      sourcePath: 'drawing.pdf',
      familyId: 'PdfDirect',
      revision: '0.1.0',
    },
    capToken,
  );
  assert.equal(response.status, 403);
  const data = await response.json();
  assert.equal(data.code, 'PDF_DIRECT_CONVERSION_DISABLED');
  assert.match(data.error, /让智能助手转换/);
});

test('ingest converts a source file and pushes the package to the workbench', async () => {
  const { capToken } = await initToken('session-a');
  write(path.join(WORKSPACE, 'model.step'), 'ISO-10303-21;');
  const response = await jsonPost(
    '/rcif-studio/api/ingest',
    {
      sessionId: 'session-a',
      kind: 'step',
      sourcePath: 'model.step',
      familyId: 'ContractStep',
      revision: '1.0.0',
      outputPath: 'out/ContractStep_1.0.0.rcif',
    },
    capToken,
  );
  assert.equal(response.status, 200);
  const data = await response.json();
  assert.equal(data.ok, true);
  assert.equal(data.packagePath, path.join(WORKSPACE, 'out', 'ContractStep_1.0.0.rcif'));
  assert.ok(data.metadata, JSON.stringify(data));
  assert.equal(data.metadata.familyId, 'StraightAbutment');
  assert.equal(processCalls.at(-1).script, 'step2rcif.py');

  const state = await (await fetch(`${baseUrl}/rcif-studio/api/state?sessionId=session-a`)).json();
  assert.equal(state.packagePath, data.packagePath);
  assert.equal(state.sessionId, 'session-a');
  assert.equal(state.capabilities.agent, false);
  assert.deepEqual(state.capabilities.ingest, ['pdf', 'step', 'ifc', 'rfa']);

  const other = await (await fetch(`${baseUrl}/rcif-studio/api/state?sessionId=session-b`)).json();
  assert.equal(other.packagePath, null);
});

test('commands and feedback are reported honestly when no session controller exists', async () => {
  const { capToken } = await initToken('session-a');
  let response = await jsonPost('/rcif-studio/api/command', { sessionId: 'session-a', text: '把模型加宽 200mm' }, capToken);
  assert.equal(response.status, 200);
  let data = await response.json();
  assert.equal(data.delivered, false);
  assert.match(data.deliveryReason, /sessionController/);
  assert.match(data.prompt, /把模型加宽/);

  const png =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
  response = await jsonPost(
    '/rcif-studio/api/feedback',
    { sessionId: 'session-a', kind: 'screenshot', text: '垫石悬空，请下移 50mm', screenshotDataUrls: [png, png] },
    capToken,
  );
  assert.equal(response.status, 200);
  data = await response.json();
  assert.equal(data.delivered, false);
  assert.equal(data.screenshotPaths.length, 2);
  assert.ok(data.screenshotPath.endsWith('.png'));
  assert.equal(fs.existsSync(data.screenshotPath), true);
});

test('library ingest always writes a submission inbox', async () => {
  const { capToken } = await initToken('session-a');
  const response = await jsonPost(
    '/rcif-studio/api/library/ingest',
    { sessionId: 'session-a', packagePath: 'sample.rcif', note: '合同测试入库' },
    capToken,
  );
  assert.equal(response.status, 200);
  const data = await response.json();
  assert.equal(data.ok, true);
  assert.equal(data.mode, 'inbox');
  assert.equal(fs.existsSync(path.join(data.inboxPath, 'submission.json')), true);
  const submission = JSON.parse(fs.readFileSync(path.join(data.inboxPath, 'submission.json'), 'utf8'));
  assert.equal(submission.metadata.familyId, 'StraightAbutment');
  assert.equal(submission.note, '合同测试入库');
});

test('feedback and commands are delivered through sessionController when present', async () => {
  enableSession = true;
  const { capToken } = await initToken('session-a');

  let response = await jsonPost('/rcif-studio/api/command', { sessionId: 'session-a', text: '台身加宽 200mm' }, capToken);
  assert.equal(response.status, 200);
  let data = await response.json();
  assert.equal(data.delivered, true);
  assert.equal(sessionPrompts.length, 1);
  assert.equal(sessionPrompts[0].request.mode, 'followup');
  assert.equal(sessionPrompts[0].request.sessionId, 'session-a');
  assert.match(sessionPrompts[0].request.content[0].text, /台身加宽 200mm/);
  assert.equal(typeof sessionPrompts[0].signal?.throwIfAborted, 'function');

  const png =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==';
  response = await jsonPost(
    '/rcif-studio/api/feedback',
    {
      sessionId: 'session-a',
      kind: 'annotation',
      text: '垫石下移',
      annotations: [{ file: 'generators/generator.py', lines: [40, 43], comment: '垫石下移 50mm' }],
      screenshotDataUrl: png,
    },
    capToken,
  );
  assert.equal(response.status, 200);
  data = await response.json();
  assert.equal(data.delivered, true);
  assert.equal(data.annotationCount, 1);
  assert.equal(sessionPrompts.length, 2);
  assert.match(sessionPrompts[1].request.content[0].text, /垫石下移 50mm/);
  assert.ok(sessionPrompts[1].request.content[0].text.includes(data.screenshotPath));
});

test('workspace/session tree, creation and session metadata round-trip', async () => {
  enableSession = true;
  const assistantRoot = path.join(WORKSPACE, '铁路构件智能助手');

  let response = await fetch(`${baseUrl}/rcif-studio/api/workspaces`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ name: '合同工作区' }),
  });
  assert.equal(response.status, 200);
  const createdWorkspace = await response.json();
  assert.equal(createdWorkspace.ok, true);
  assert.equal(createdWorkspace.workspace.path, path.join(assistantRoot, '合同工作区'));
  assert.equal(fs.existsSync(path.join(assistantRoot, '合同工作区', '.rcif', 'sessions')), true);

  response = await fetch(`${baseUrl}/rcif-studio/api/sessions`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ workspaceId: createdWorkspace.workspace.workspaceId }),
  });
  assert.equal(response.status, 200);
  const createdSession = await response.json();
  assert.equal(createdSession.ok, true);
  assert.match(createdSession.sessionId, /^session-created-/);

  let tree = await (await fetch(`${baseUrl}/rcif-studio/api/tree`)).json();
  assert.equal(tree.workspaces.length, 1);
  assert.equal(tree.workspaces[0].title, '合同工作区');
  assert.equal(tree.workspaces[0].sessions[0].sessionId, createdSession.sessionId);

  response = await fetch(`${baseUrl}/rcif-studio/api/session-meta`, {
    method: 'PUT',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({
      sessionId: createdSession.sessionId,
      inputFile: 'inputs/model.step',
      componentName: 'ContractGirder',
      outputPath: 'out/ContractGirder_1.0.0.rcif',
    }),
  });
  assert.equal(response.status, 200);

  const meta = await (await fetch(
    `${baseUrl}/rcif-studio/api/session-meta?sessionId=${createdSession.sessionId}`,
  )).json();
  assert.equal(meta.meta.inputFile, 'inputs/model.step');
  assert.equal(meta.meta.componentName, 'ContractGirder');
  assert.equal(meta.meta.outputPath, 'out/ContractGirder_1.0.0.rcif');

  const state = await (await fetch(
    `${baseUrl}/rcif-studio/api/state?sessionId=${createdSession.sessionId}`,
  )).json();
  assert.equal(state.workspace.title, '合同工作区');
  assert.equal(state.sessionInfo.componentName, 'ContractGirder');

  const { capToken } = await initToken(createdSession.sessionId);
  response = await fetch(
    `${baseUrl}/rcif-studio/api/upload?sessionId=${createdSession.sessionId}&name=model.ifc`,
    {
      method: 'POST',
      headers: { 'content-type': 'application/octet-stream', 'X-RCIF-Cap-Token': capToken },
      body: Buffer.from('ISO-10303-21;'),
    },
  );
  assert.equal(response.status, 200);
  const upload = await response.json();
  assert.equal(upload.ok, true);
  assert.equal(upload.sessionInfo.inputFile, path.join('inputs', 'model.ifc'));
  assert.equal(upload.sessionInfo.sourceKind, 'ifc');

  const files = await (await fetch(
    `${baseUrl}/rcif-studio/api/session-files?sessionId=${createdSession.sessionId}`,
  )).json();
  assert.equal(files.ok, true);
  assert.equal(files.inputsDir, 'inputs');
  assert.equal(files.files[0].path, 'inputs/model.ifc');

  const openTool = ctx.tools.registered.find((tool) => tool.name === 'rcif_open_package');
  assert.ok(openTool);
  const openResult = await openTool.execute(
    { packagePath: 'sample.rcif' },
    { agent: { session: { id: createdSession.sessionId } } },
  );
  assert.equal(openResult.ok, true);
  const persisted = await (await fetch(
    `${baseUrl}/rcif-studio/api/session-meta?sessionId=${createdSession.sessionId}`,
  )).json();
  assert.match(persisted.meta.packagePath, /sample\.rcif$/);

  const siblingResponse = await jsonPost(
    '/rcif-studio/api/sessions',
    { workspaceId: createdWorkspace.workspace.workspaceId },
  );
  const siblingText = await siblingResponse.text();
  assert.equal(siblingResponse.status, 200, siblingText);
  const sibling = JSON.parse(siblingText);
  const putMeta = (body) => fetch(`${baseUrl}/rcif-studio/api/session-meta`, {
    method: 'PUT',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(body),
  });
  const firstTitleResponse = await putMeta({
    sessionId: createdSession.sessionId,
    componentName: '同名构件',
  });
  const firstTitleText = await firstTitleResponse.text();
  assert.equal(firstTitleResponse.status, 200, firstTitleText);
  const firstTitle = JSON.parse(firstTitleText).meta.title;
  const secondTitleResponse = await putMeta({
    sessionId: sibling.sessionId,
    componentName: '同名构件',
  });
  const secondTitleText = await secondTitleResponse.text();
  assert.equal(secondTitleResponse.status, 200, secondTitleText);
  const secondTitle = JSON.parse(secondTitleText).meta.title;
  assert.equal(firstTitle, '同名构件');
  assert.equal(secondTitle, '同名构件 2');

  response = await jsonPost(
    '/rcif-studio/api/feedback',
    {
      sessionId: createdSession.sessionId,
      kind: 'annotation',
      annotations: [
        { file: 'generators/generator.py', lines: [10, 14], comment: '第一段批注' },
        { file: 'generators/generator.py', lines: [40, 42], comment: '第二段批注' },
      ],
    },
    capToken,
  );
  assert.equal(response.status, 200);
  const feedback = await response.json();
  assert.equal(feedback.annotationCount, 2);
  assert.ok(feedback.historyId);

  let history = await (await fetch(
    `${baseUrl}/rcif-studio/api/annotation-history?sessionId=${createdSession.sessionId}`,
  )).json();
  assert.equal(history.history.length, 1);
  assert.equal(history.history[0].annotations.length, 2);
  assert.equal(history.history[0].annotations[1].lines[0], 40);

  response = await fetch(
    `${baseUrl}/rcif-studio/api/annotation-history?sessionId=${createdSession.sessionId}&id=${feedback.historyId}`,
    { method: 'DELETE', headers: { 'X-RCIF-Cap-Token': capToken } },
  );
  assert.equal(response.status, 200);
  history = await (await fetch(
    `${baseUrl}/rcif-studio/api/annotation-history?sessionId=${createdSession.sessionId}`,
  )).json();
  assert.equal(history.history.length, 0);
});

test('packages are bound per session; sessions and assistant endpoints expose the binding', async () => {
  const { capToken } = await initToken('session-a');
  write(path.join(WORKSPACE, 'model.step'), 'ISO-10303-21;');
  await jsonPost(
    '/rcif-studio/api/ingest',
    {
      sessionId: 'session-a',
      kind: 'step',
      sourcePath: 'model.step',
      familyId: 'SessionBound',
      revision: '1.0.0',
      outputPath: 'out/SessionBound_1.0.0.rcif',
    },
    capToken,
  );

  const sessions = await (await fetch(`${baseUrl}/rcif-studio/api/sessions`)).json();
  const bound = sessions.sessions.find((item) => item.sessionId === 'session-a');
  assert.equal(bound.hasPackage, true);
  assert.match(bound.packagePath, /SessionBound_1\.0\.0\.rcif$/);

  const assistant = await (await fetch(`${baseUrl}/rcif-studio/api/assistant`)).json();
  assert.equal(assistant.running, true);
  assert.equal(assistant.deepLinkSupported, false);
  assert.match(assistant.url, /^http:\/\/127\.0\.0\.1:/);

  // 工具调用应绑定到它自己的 Agent 会话，而不是全局默认会话
  const open = ctx.tools.registered.find((tool) => tool.name === 'rcif_open_package');
  const result = await open.execute(
    { packagePath: 'sample.rcif' },
    { agent: { session: { id: 'session-tool' } } },
  );
  assert.equal(result.ok, true);

  const toolState = await (await fetch(`${baseUrl}/rcif-studio/api/state?sessionId=session-tool`)).json();
  assert.match(toolState.packagePath, /sample\.rcif$/);
  const defaultState = await (await fetch(`${baseUrl}/rcif-studio/api/state?sessionId=session-default`)).json();
  assert.equal(defaultState.packagePath, null);
});

