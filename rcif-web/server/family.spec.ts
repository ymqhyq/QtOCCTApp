import { describe, expect, it } from 'vitest';
import { createState } from './state';
import { addPreset, createFamilyFromTemplate, removePreset, updatePreset } from './family';

describe('族库管理领域逻辑', () => {
  it('基于模板新建族：新 familyId、独立脚本摘要与逐型号缓存键', async () => {
    const state = await createState();
    const template = state.components.find((component) => component.familyId === 'PierBody')!;
    const created = await createFamilyFromTemplate(template, {
      templateFamilyId: 'PierBody',
      familyId: 'PierBodyV2',
      name: '圆端形桥墩墩身 V2',
      author: '测试员',
    });
    expect(created.familyId).toBe('PierBodyV2');
    expect(created.componentId).not.toBe(template.componentId);
    expect(created.status).toBe('draft');
    expect(created.recipe!.digest).not.toBe(template.recipe!.digest);
    expect(created.parameters.presets.length).toBe(template.parameters.presets.length);
    for (const preset of created.parameters.presets) {
      expect(preset.cacheKey).toMatch(/^[0-9a-f]{64}$/);
      expect(created.presetCacheKeys[preset.modelNumber]).toBe(preset.cacheKey);
    }
    expect(created.geometry.defaultCacheKey).toMatch(/^[0-9a-f]{64}$/);
  });

  it('新增/编辑/删除型号会重算缓存键；编辑默认型号会重建几何', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'PierBody')!;
    const beforeVolume = component.geometry.volumeM3;
    const preset = await addPreset(component, {
      modelNumber: 'STD-PIER-20M',
      label: '20m 标准墩',
      overrides: { PierHeight: 20000, TopWidth: 2600, TopLength: 7200 },
    });
    expect(preset.cacheKey).toMatch(/^[0-9a-f]{64}$/);
    expect(component.parameters.presets.some((item) => item.modelNumber === 'STD-PIER-20M')).toBe(true);

    const defaultModel = String(component.parameters.schema[0].default);
    const updated = await updatePreset(component, defaultModel, {
      label: '12m 标准墩（修订）',
      overrides: { PierHeight: 15000, TopWidth: 2500, TopLength: 6600 },
    });
    expect(updated.cacheKey).toMatch(/^[0-9a-f]{64}$/);
    expect(component.geometry.defaultCacheKey).toBe(updated.cacheKey);
    expect(component.geometry.volumeM3).not.toBe(beforeVolume);
    expect(component.mesh.volumeMm3).toBeGreaterThan(0);

    expect(() => removePreset(component, defaultModel)).toThrow(/默认型号/);
    removePreset(component, 'STD-PIER-20M');
    expect(component.parameters.presets.some((item) => item.modelNumber === 'STD-PIER-20M')).toBe(false);
  });
});
