/**
 * Conversion dispatcher: PDF / STEP / IFC -> .rcif via the Python toolchain.
 *
 * The caller supplies the process runner so contract tests can inject a fake
 * and the host can use the real spawn watchdog.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const KINDS = ['pdf', 'step', 'ifc'];

function tail(text, limit = 4000) {
  const value = String(text || '');
  return value.length > limit ? value.slice(value.length - limit) : value;
}

function defaultFamilyId(kind, sourcePath) {
  const base = path.basename(sourcePath, path.extname(sourcePath));
  const cleaned = base.replace(/[^A-Za-z0-9_]/g, '_').replace(/^_+/, '');
  const prefixed = /^[A-Za-z]/.test(cleaned) ? cleaned : `Imported_${cleaned}`;
  const fallback = { pdf: 'PdfDraft', step: 'ImportedStep', ifc: 'ImportedIFC' }[kind] || 'Imported';
  return (prefixed || fallback).slice(0, 32);
}

function scriptFor(kind) {
  return { pdf: 'pdf2rcif.py', step: 'step2rcif.py', ifc: 'ifc2rcif.py' }[kind];
}

async function ingestSource(options) {
  const {
    kind,
    sourcePath,
    outputPath,
    familyId,
    componentName,
    revision = '1.0.0',
    nodeName,
    material,
    runner,
    pythonPath,
    toolsDir,
    cwd,
    signal,
    onSpawn,
    sessionId,
  } = options;

  if (!KINDS.includes(kind)) throw new Error(`不支持的转换类型: ${kind}`);
  const script = scriptFor(kind);
  if (!fs.existsSync(path.join(toolsDir, script))) {
    throw new Error(`工具链缺少 ${script}: ${toolsDir}`);
  }
  if (!fs.existsSync(sourcePath)) throw new Error(`源文件不存在: ${sourcePath}`);

  const family = familyId || defaultFamilyId(kind, sourcePath);
  const sessionSegment = String(sessionId || '').replace(/[^A-Za-z0-9._-]/g, '_').slice(0, 80);
  const out = outputPath
    || path.join(
      cwd,
      'rcif-tools',
      'out',
      sessionSegment ? path.join('sessions', sessionSegment) : 'ingest',
      `${family}_${revision}.rcif`,
    );
  fs.mkdirSync(path.dirname(out), { recursive: true });

  const args = [path.join(toolsDir, script), sourcePath, out, '--family-id', family, '--revision', revision];
  if (kind === 'step') {
    args.push('--node-name', nodeName || 'ImportedPart', '--material', material || 'C50');
  }
  if (componentName) args.push('--name', String(componentName).slice(0, 120));

  const result = await runner(pythonPath, args, {
    cwd,
    timeoutMs: 300000,
    signal,
    onSpawn,
  });
  const log = tail(`${result.stdout || ''}${result.stderr || ''}`);
  if (result.timedOut) {
    return { ok: false, error: '转换超时（>300s）已被硬熔断', log, exitCode: result.code };
  }
  if (result.code !== 0 || !fs.existsSync(out)) {
    const match = /\[ERROR\][^\r\n]*/.exec(log);
    return {
      ok: false,
      error: match ? match[0] : `转换失败 (exit=${result.code})`,
      log,
      exitCode: result.code,
    };
  }
  return {
    ok: true,
    packagePath: out,
    familyId: family,
    revision,
    kind,
    log,
    exitCode: result.code,
  };
}

module.exports = {
  KINDS,
  ingestSource,
  defaultFamilyId,
};
