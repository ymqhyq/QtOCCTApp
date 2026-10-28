import { describe, expect, it } from 'vitest';
import { cacheKeyFor, cacheKeyInputs, SPEC_PIER_PARAMS } from './cache-key';
import type { InternalComponent } from './seed';

function component(overrides: Partial<InternalComponent> = {}): InternalComponent {
  return {
    familyId: 'PierBody',
    recipeInputs: ['CrossSlope', 'PierHeight', 'Slope', 'TopLength', 'TopWidth'],
    recipe: {
      engine: 'build123d',
      source: 'generators/generator.py',
      entry: 'build',
      version: '1.4.2',
      digest: 'e2047ce0ca67f84dc340cbd67376b6b52728fd287edfa2985b2998daae8d2cd9',
      inputs: ['CrossSlope', 'PierHeight', 'Slope', 'TopLength', 'TopWidth'],
    },
    ...overrides,
  } as unknown as InternalComponent;
}

describe('RCIF cacheKey 单一实现', () => {
  it('规范向量 4 的默认参数命中权威 cacheKey', async () => {
    const key = await cacheKeyFor(component(), { ...SPEC_PIER_PARAMS });
    expect(key).toBe('8361c4b4b7916b48c16a2687f3d73f1fb3ccf1f850f12e201333f77257697405');
  });

  it('同一输入稳定，修改任一参数即变化', async () => {
    const params = { CrossSlope: 2, PierHeight: 12000, Slope: 35, TopLength: 6400, TopWidth: 2400 };
    const first = await cacheKeyFor(component(), params);
    const second = await cacheKeyFor(component(), { ...params });
    const changed = await cacheKeyFor(component(), { ...params, PierHeight: 12001 });
    expect(first).toBe(second);
    expect(changed).not.toBe(first);
  });

  it('字符串/枚举输入不会被降维成 0', () => {
    const inputs = cacheKeyInputs(
      component({ recipeInputs: ['StrengthGrade', 'Count'] }),
      { StrengthGrade: 'C50', Count: 0 },
    );
    expect(inputs).toEqual({ StrengthGrade: 'C50', Count: 0 });
    expect(inputs.StrengthGrade).not.toBe(0);
  });

  it('非参数化构件也使用 DeterministicJSON（键序无关）', async () => {
    const base = component({ familyId: 'Native', recipe: undefined, recipeInputs: ['B', 'A'] });
    const first = await cacheKeyFor(base, { A: 1, B: 2 });
    const second = await cacheKeyFor(base, { B: 2, A: 1 });
    expect(first).toBe(second);
  });
});
