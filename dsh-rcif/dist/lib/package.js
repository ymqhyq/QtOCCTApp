/**
 * RCIF `.rcif` package inspection (metadata, checksums, preview, source).
 *
 * The workbench calls this on every package change; results are cached by
 * (path, mtime, size) so polling `/state` + `/package` stays cheap.
 */
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { readZip, extractEntry, ZipError } = require('./zip');

const CACHE_TTL_MS = 60 * 1000;
const MAX_SOURCE_BYTES = 2 * 1024 * 1024;
const cache = new Map();

function sha256(data) {
  return crypto.createHash('sha256').update(data).digest('hex');
}

function normalizeEntryName(name) {
  return String(name || '').replace(/\\/g, '/').replace(/^\.\//, '');
}

function findEntry(zip, candidates) {
  for (const candidate of candidates) {
    const name = normalizeEntryName(candidate);
    if (name && zip.entries.has(name)) return name;
  }
  return null;
}

function verifyChecksums(zip, checksumsEntry) {
  if (!checksumsEntry) return { present: false, verified: false, fileCount: 0, checked: 0, mismatches: [] };
  let doc;
  try {
    doc = JSON.parse(extractEntry(zip, checksumsEntry).toString('utf8'));
  } catch (error) {
    return { present: true, verified: false, fileCount: 0, checked: 0, mismatches: [`checksums.json 解析失败: ${error.message}`] };
  }
  const files = doc && typeof doc === 'object' && doc.files && typeof doc.files === 'object' ? doc.files : null;
  if (!files) {
    return { present: true, verified: false, fileCount: 0, checked: 0, mismatches: ['checksums.json 缺少 files 映射'] };
  }
  const mismatches = [];
  let checked = 0;
  for (const [name, expected] of Object.entries(files)) {
    if (!zip.entries.has(name)) {
      mismatches.push(`清单文件缺失: ${name}`);
      continue;
    }
    try {
      const actual = sha256(extractEntry(zip, name));
      checked += 1;
      if (actual !== expected) mismatches.push(`SHA-256 失配: ${name}`);
    } catch (error) {
      mismatches.push(`读取失败: ${name}: ${error.message}`);
    }
  }
  return {
    present: true,
    algorithm: doc.algorithm || 'sha256',
    verified: mismatches.length === 0,
    fileCount: Object.keys(files).length,
    checked,
    mismatches: mismatches.slice(0, 20),
  };
}

function buildMetadata(component, zip, checksums, preview, source, packageSha256, sizeBytes) {
  const identity = component.identity || {};
  const geometry = component.geometry || {};
  const parameters = component.parameters || {};
  const shapes = component.shapes || {};
  return {
    format: component.format || null,
    schemaVersion: component.schemaVersion || null,
    familyId: identity.familyId || null,
    componentId: identity.componentId || null,
    revision: identity.revision || null,
    name: identity.name || identity.familyId || null,
    category: identity.category || null,
    semanticCode: identity.semanticCode || null,
    drawingCode: identity.drawingCode || null,
    sourceMode: identity.sourceMode || null,
    engine: component.recipe?.engine || null,
    recipeSource: component.recipe?.source || null,
    exactBrep: geometry.exactBrep === true,
    defaultFormat: geometry.defaultFormat || null,
    bounds: geometry.bounds || null,
    volumeM3: typeof geometry.volumeM3 === 'number' ? geometry.volumeM3 : null,
    surfaceM2: typeof geometry.surfaceM2 === 'number' ? geometry.surfaceM2 : null,
    parameterCount: Array.isArray(parameters.schema) ? parameters.schema.length : 0,
    presetCount: Array.isArray(parameters.presets) ? parameters.presets.length : 0,
    shapeCount: Object.keys(shapes).length,
    shapes: Object.entries(shapes).map(([key, value]) => ({
      key,
      label: value?.label || key,
      code: value?.code || null,
      geomType: value?.geomType || null,
      material: value?.material || null,
      node: value?.node || key,
      parent: value?.parent || null,
      selector: value?.selector || null,
      instances: Array.isArray(value?.instances) ? value.instances : [],
      volumeM3: typeof value?.volumeM3 === 'number' ? value.volumeM3 : null,
    })),
    fileCount: zip.entries.size,
    packageSha256,
    sizeBytes,
    checksums,
    preview,
    source,
    qa: component.qa || null,
    changelog: Array.isArray(component.changelog) ? component.changelog.slice(-5) : [],
    extractionMode: component.qa?.extractionMode || null,
  };
}

function readPackageMeta(packagePath) {
  const resolved = path.resolve(packagePath);
  const stat = fs.statSync(resolved);
  const key = `${resolved}|${stat.mtimeMs}|${stat.size}`;
  const cached = cache.get(key);
  if (cached && Date.now() - cached.at < CACHE_TTL_MS) return cached.value;

  const zip = readZip(resolved);
  const componentEntry = findEntry(zip, ['component.json']);
  if (!componentEntry) throw new ZipError('构件包缺少 component.json', 'MISSING_COMPONENT_JSON');
  let component;
  try {
    component = JSON.parse(extractEntry(zip, componentEntry).toString('utf8'));
  } catch (error) {
    throw new ZipError(`component.json 解析失败: ${error.message}`, 'BAD_COMPONENT_JSON');
  }

  const checksumsEntry = findEntry(zip, ['checksums.json']);
  const checksums = verifyChecksums(zip, checksumsEntry);

  const sourceCandidates = [
    component.recipe?.source,
    'generators/generator.py',
    'generator.py',
  ].filter(Boolean);
  const sourceEntry = findEntry(zip, sourceCandidates);
  const source = sourceEntry
    ? { available: true, path: sourceEntry, size: zip.entries.get(sourceEntry).uncompressedSize }
    : { available: false, path: null, size: 0 };

  const previewCandidates = [
    'display/preview.glb',
    'rcif/display/preview.glb',
    component.geometry?.defaultFile,
  ].filter((name) => name && /\.glb$/i.test(String(name)));
  const previewEntry = findEntry(zip, previewCandidates);
  const preview = previewEntry
    ? { available: true, path: previewEntry, size: zip.entries.get(previewEntry).uncompressedSize }
    : { available: false, path: null, size: 0 };

  const packageSha256 = sha256(zip.buffer);
  const metadata = buildMetadata(component, zip, checksums, preview, source, packageSha256, stat.size);

  let provenance = null;
  try {
    provenance = JSON.parse(fs.readFileSync(resolved.replace(/\.rcif$/i, '.provenance.json'), 'utf8'));
  } catch {
    provenance = null;
  }

  const value = {
    ok: true,
    packagePath: resolved,
    packageSha256,
    sizeBytes: stat.size,
    updatedAt: new Date(stat.mtimeMs).toISOString(),
    component,
    metadata,
    files: [...zip.entries.values()].map((entry) => ({
      path: entry.name,
      size: entry.uncompressedSize,
      compressedSize: entry.compressedSize,
    })),
    preview,
    source,
    checksums,
    provenance,
  };
  cache.set(key, { at: Date.now(), value });
  // Keep the cache bounded for long-running hosts.
  if (cache.size > 32) {
    const oldest = [...cache.entries()].sort((a, b) => a[1].at - b[1].at)[0];
    if (oldest) cache.delete(oldest[0]);
  }
  return value;
}

function readPackageFile(packagePath, entryName) {
  const zip = readZip(path.resolve(packagePath));
  const name = normalizeEntryName(entryName);
  return { name, data: extractEntry(zip, name), entry: zip.entries.get(name) };
}

function readPackageSource(packagePath) {
  const meta = readPackageMeta(packagePath);
  if (!meta.source.available) return { ok: true, available: false, path: null, text: '' };
  const { data } = readPackageFile(packagePath, meta.source.path);
  const text = data.subarray(0, MAX_SOURCE_BYTES).toString('utf8');
  return {
    ok: true,
    available: true,
    path: meta.source.path,
    text,
    truncated: data.length > MAX_SOURCE_BYTES,
  };
}

module.exports = {
  readPackageMeta,
  readPackageFile,
  readPackageSource,
  normalizeEntryName,
  ZipError,
};
