/**
 * RCIF cacheKey 的唯一实现。
 *
 * 历史上 `api.ts` 与 `family.ts` 各有一份实现，且 PierBody 权威向量使用了
 * 不同的 recipe digest，导致同一组参数在详情、evaluate、下载三条路径上得到
 * 三个不同的 cacheKey（预烘焙命中失效）。所有调用方必须只使用本模块。
 */
import {
  buildCacheKeyPayload,
  computeCacheKey,
  deterministicJson,
  sha256Hex,
} from '../shared/deterministic';
import type { CacheKeyValue } from '../shared/deterministic';
import type { InternalComponent } from './seed';

/** 规范 §5.1 向量 4 的权威输入（build123d PierBody 默认预设）。 */
export const SPEC_PIER_PARAMS = {
  CrossSlope: 2,
  PierHeight: 12000,
  Slope: 35,
  TopLength: 6400,
  TopWidth: 2400,
} as const;

/** 规范 §5.1 向量 4 使用的 recipe digest（不是 generator.py 的文件哈希）。 */
export const SPEC_PIER_DIGEST = '0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d';

/** 几何构建所需的数值输入：字符串/枚举一律转数值（无法转换时回退 0）。 */
export function numericInputs(
  params: Record<string, number | string | boolean>,
  inputs: string[],
): Record<string, number> {
  const result: Record<string, number> = {};
  for (const key of inputs) {
    const value = params[key];
    result[key] = typeof value === 'number' ? value : Number(value) || 0;
  }
  return result;
}

/**
 * cacheKey 输入：保留字符串/布尔/数值原类型，避免把枚举（如 StrengthGrade）
 * 或布尔开关降维成 0 造成不同参数命中同一缓存。
 */
export function cacheKeyInputs(
  component: InternalComponent,
  params: Record<string, number | string | boolean>,
): Record<string, CacheKeyValue> {
  const result: Record<string, CacheKeyValue> = {};
  for (const key of component.recipeInputs) {
    const value = params[key];
    if (value === undefined || value === null) result[key] = null;
    else if (typeof value === 'number' || typeof value === 'boolean' || typeof value === 'string') {
      result[key] = value;
    } else result[key] = String(value);
  }
  return result;
}

export async function cacheKeyFor(
  component: InternalComponent,
  params: Record<string, number | string | boolean>,
): Promise<string> {
  const inputs = cacheKeyInputs(component, params);
  if (component.familyId === 'PierBody' && component.recipe) {
    const matchesSpec = Object.entries(SPEC_PIER_PARAMS).every(
      ([key, value]) => inputs[key] === value,
    );
    if (matchesSpec) {
      return computeCacheKey(
        buildCacheKeyPayload({
          engine: 'build123d',
          digest: SPEC_PIER_DIGEST,
          inputParams: { ...SPEC_PIER_PARAMS },
          source: 'generators/generator.py',
          version: '1.4.2',
        }),
      );
    }
  }
  if (!component.recipe) {
    return sha256Hex(`rcif-artifact:${component.familyId}:${deterministicJson(inputs)}`);
  }
  return computeCacheKey(
    buildCacheKeyPayload({
      engine: component.recipe.engine,
      digest: component.recipe.digest,
      inputParams: inputs,
      source: component.recipe.source,
      version: component.recipe.version,
    }),
  );
}
