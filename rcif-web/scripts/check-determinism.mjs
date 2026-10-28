/**
 * RCIF 确定性契约自检（不是 vitest，可直接 `node scripts/check-determinism.mjs`）。
 *
 * Node 24 原生执行 .ts（type stripping），因此在没有 Vite/esbuild 的环境里
 * 也能验证 TS 与 Python 两份实现是否逐字节一致。
 */

import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import {
  buildCacheKeyPayload,
  deterministicJson,
  computeCacheKey,
  verifySpecVectors,
} from '../shared/deterministic.ts';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const vectors = JSON.parse(
  readFileSync(path.join(HERE, '../shared/fixtures/deterministic_vectors.json'), 'utf8'),
);
const approval = JSON.parse(
  readFileSync(path.join(HERE, '../shared/fixtures/approval_canonical.json'), 'utf8'),
);

let failures = 0;

function check(name, actual, expected) {
  if (actual === expected) {
    console.log(`  ok   ${name}`);
    return;
  }
  failures += 1;
  console.error(`  FAIL ${name}\n       actual:   ${actual}\n       expected: ${expected}`);
}

console.log('DeterministicJSON 跨语言向量');
for (const testCase of vectors.cases) {
  const canonical = deterministicJson(testCase.payload);
  check(JSON.stringify(testCase.payload), canonical, testCase.canonical);
  const digest = createHash('sha256').update(canonical, 'utf8').digest('hex');
  check(`${JSON.stringify(testCase.payload)} sha256`, digest, testCase.sha256);
}

console.log('Ed25519 审批单规范化');
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
check('approval fields order', JSON.stringify(fields), JSON.stringify(approval.fields));
check('approval canonical', JSON.stringify(approval.payload, fields), approval.canonical);

console.log('RCIF §5.1 规范向量');
const spec = await verifySpecVectors();
for (const vector of spec) check(vector.name, vector.actual, vector.expected);

const cacheKey = await computeCacheKey(
  buildCacheKeyPayload({
    engine: 'build123d',
    digest: '0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d',
    inputParams: { CrossSlope: 2, PierHeight: 12000, Slope: 35, TopLength: 6400, TopWidth: 2400 },
    source: 'generators/generator.py',
    version: '1.4.2',
  }),
);
check('PierBody 权威 cacheKey', cacheKey, '8361c4b4b7916b48c16a2687f3d73f1fb3ccf1f850f12e201333f77257697405');

if (failures) {
  console.error(`\n${failures} 项失败`);
  process.exit(1);
}
console.log('\n全部确定性契约检查通过');
