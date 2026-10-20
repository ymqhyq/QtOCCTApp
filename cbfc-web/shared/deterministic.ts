/**
 * DeterministicJSON / 权威 cacheKey 实现
 * 对齐 CBF-C v1.4.2 §5.1 五条序列化规则：
 *  1. 键名按 UTF-8 字节序递归升序，冒号与逗号后不加空格
 *  2. 浮点 %.12g 规范化，|val| < 1e-12 置 0，禁止 NaN/Inf
 *  3. 可选外部几何参数缺省为 null
 *  4. UTF-8 原生直出（不转义 \\uXXXX）
 *  5. 四条标准测试向量可验真
 */

const encoder = new TextEncoder();

/** %.12g 规范化：12 位有效数字、无多余尾零、指数按需 */
export function formatNumber(value: number): string {
  if (!Number.isFinite(value)) throw new Error('DeterministicJSON: NaN/Inf 不允许');
  if (Object.is(value, -0) || Math.abs(value) < 1e-12) return '0';
  if (Number.isInteger(value) && Math.abs(value) < 1e15) return String(value);
  let s = value.toPrecision(12);
  if (s.includes('e') || s.includes('E')) {
    const [mantissa, exponent] = s.toLowerCase().split('e');
    const m = mantissa.includes('.') ? mantissa.replace(/0+$/, '').replace(/\.$/, '') : mantissa;
    return `${m}e${Number(exponent) >= 0 ? '+' : '-'}${String(Math.abs(Number(exponent))).padStart(2, '0')}`;
  }
  if (s.includes('.')) s = s.replace(/0+$/, '').replace(/\.$/, '');
  return s;
}

function isAscii(text: string): boolean {
  for (let i = 0; i < text.length; i += 1) {
    if (text.charCodeAt(i) > 0x7f) return false;
  }
  return true;
}

function compareAscii(a: string, b: string): number {
  const len = Math.min(a.length, b.length);
  for (let i = 0; i < len; i += 1) {
    const diff = a.charCodeAt(i) - b.charCodeAt(i);
    if (diff !== 0) return diff;
  }
  return a.length - b.length;
}

function compareBytes(ba: Uint8Array, bb: Uint8Array): number {
  const len = Math.min(ba.length, bb.length);
  for (let i = 0; i < len; i += 1) {
    if (ba[i] !== bb[i]) return ba[i] - bb[i];
  }
  return ba.length - bb.length;
}

export function deterministicJson(value: unknown): string {
  if (value === null) return 'null';
  if (typeof value === 'number') return formatNumber(value);
  if (typeof value === 'boolean') return value ? 'true' : 'false';
  if (typeof value === 'string') return JSON.stringify(value);
  if (Array.isArray(value)) return `[${value.map((item) => deterministicJson(item)).join(',')}]`;
  if (typeof value === 'object') {
    const entries = Object.entries(value as Record<string, unknown>)
      .filter(([, v]) => v !== undefined)
      .map(([key, child]) => ({
        key,
        child,
        ascii: isAscii(key),
        // 仅非 ASCII 键预编码一次，避免排序比较时反复 encode
        bytes: isAscii(key) ? null : encoder.encode(key),
      }));
    entries.sort((a, b) => {
      if (a.ascii && b.ascii) return compareAscii(a.key, b.key);
      return compareBytes(
        a.bytes ?? encoder.encode(a.key),
        b.bytes ?? encoder.encode(b.key),
      );
    });
    return `{${entries
      .map(({ key, child }) => `${JSON.stringify(key)}:${deterministicJson(child)}`)
      .join(',')}}`;
  }
  throw new Error(`DeterministicJSON: 不支持的类型 ${typeof value}`);
}

export async function sha256Hex(input: string | Uint8Array): Promise<string> {
  const bytes = typeof input === 'string' ? encoder.encode(input) : input;
  const digest = await globalThis.crypto.subtle.digest('SHA-256', bytes as unknown as BufferSource);
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

export interface CacheKeyPayload {
  cacheKeyVersion: number;
  digest: string;
  engine: string;
  inputParams: Record<string, CacheKeyValue>;
  kernelId: string;
  kernelVersion: string;
  source: string;
  tolerance: { angular: number; linear: number };
  units: { angle: string; length: string };
  version: string;
}

export type CacheKeyValue =
  | number
  | string
  | boolean
  | null
  | CacheKeyValue[]
  | { [key: string]: CacheKeyValue };

export interface CacheKeyInput {
  engine: string;
  digest: string;
  inputParams: Record<string, CacheKeyValue>;
  source: string;
  version: string;
  kernelId?: string;
  kernelVersion?: string;
  tolerance?: { angular: number; linear: number };
  units?: { angle: string; length: string };
  cacheKeyVersion?: number;
}

export function buildCacheKeyPayload(input: CacheKeyInput): CacheKeyPayload {
  return {
    cacheKeyVersion: input.cacheKeyVersion ?? 1,
    digest: input.digest,
    engine: input.engine,
    inputParams: input.inputParams,
    kernelId: input.kernelId ?? 'occt',
    kernelVersion: input.kernelVersion ?? '8.0.0',
    source: input.source,
    tolerance: input.tolerance ?? { angular: 0.001, linear: 0.01 },
    units: input.units ?? { angle: 'rad', length: 'mm' },
    version: input.version,
  };
}

export async function computeCacheKey(payload: CacheKeyPayload): Promise<string> {
  return sha256Hex(deterministicJson(payload));
}

/** 规范 §5.1 向量 4 权威 Payload（PierBody 默认预设 STDPIER12M） */
export const SPEC_PIER_CACHE_PAYLOAD: CacheKeyPayload = buildCacheKeyPayload({
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
});

export const SPEC_PIER_CACHE_KEY = '8361c4b4b7916b48c16a2687f3d73f1fb3ccf1f850f12e201333f77257697405';

export async function verifySpecVectors(): Promise<
  { name: string; ok: boolean; actual: string; expected: string }[]
> {
  const vectors = [
    {
      name: '向量 1 基础数据类型归一化',
      text: deterministicJson({ a: 1, b: 1.5, c: null, d: [10, 20] }),
      expected: '51a01f79a9ca9470a9d4373b8f06ab39e92a849bed2035ee58d92f7bc640cb0c',
    },
    {
      name: '向量 2 中文字符串 UTF-8 直出',
      text: deterministicJson({ Layout: '梅花形' }),
      expected: '1135ce65b3a41eb7ab8e979d812eb41d730b8e0227d76c9dac782cb32497d647',
    },
    {
      name: '向量 3 路基边坡默认 Payload',
      text: deterministicJson(
        buildCacheKeyPayload({
          engine: 'cadquery',
          digest: 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
          inputParams: {
            Alignment: null,
            GroundSurface: null,
            Height: 8000,
            Length: 20000,
            SlopeRatio: 1.5,
            Tiers: [],
          },
          source: 'service://subgrade_slope',
          version: '1.0.0',
        }),
      ),
      expected: '0fee26374b7a518d8098da4ad6c1ad52716d9f33f5bf183fe0f75f90f4bafac0',
    },
    {
      name: '向量 4 PierBody 权威 cacheKey',
      text: deterministicJson(SPEC_PIER_CACHE_PAYLOAD),
      expected: SPEC_PIER_CACHE_KEY,
    },
  ];
  return Promise.all(
    vectors.map(async (v) => {
      const actual = await sha256Hex(v.text);
      return { name: v.name, ok: actual === v.expected, actual, expected: v.expected };
    }),
  );
}
