import { describe, expect, it } from 'vitest';
import {
  buildCacheKeyPayload,
  deterministicJson,
  formatNumber,
  verifySpecVectors,
} from './deterministic';

describe('DeterministicJSON 规范自检', () => {
  it('浮点 %.12g 规范化', () => {
    expect(formatNumber(1.5)).toBe('1.5');
    expect(formatNumber(12000)).toBe('12000');
    expect(formatNumber(0.001)).toBe('0.001');
    expect(formatNumber(1e-13)).toBe('0');
    expect(formatNumber(-0)).toBe('0');
    expect(formatNumber(2.0000000000001)).toBe('2');
    expect(formatNumber(1 / 3)).toBe('0.333333333333');
  });

  it('键名 UTF-8 字节序递归升序且无空白', () => {
    expect(deterministicJson({ b: 1, a: { d: 2, c: 3 } })).toBe('{"a":{"c":3,"d":2},"b":1}');
    expect(deterministicJson({ Layout: '梅花形' })).toBe('{"Layout":"梅花形"}');
  });

  it('规范四条标准测试向量全部命中', async () => {
    const results = await verifySpecVectors();
    for (const r of results) {
      expect(r.actual, r.name).toBe(r.expected);
    }
  });

  it('向量 4 Payload 序列化字节长度与规范一致（389 字节）', () => {
    const text = deterministicJson(
      buildCacheKeyPayload({
        engine: 'build123d',
        digest: '0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d',
        inputParams: {
          CrossSlope: 2,
          PierHeight: 12000,
          Slope: 35,
          TopLength: 6400,
          TopWidth: 2400,
        },
        source: 'generators/generator.py',
        version: '1.4.2',
      }),
    );
    expect(new TextEncoder().encode(text).length).toBe(389);
  });
});
