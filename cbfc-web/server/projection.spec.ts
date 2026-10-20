import { describe, expect, it } from 'vitest';
import { createState } from './state';
import { renderProjectionSvg } from './projection';

function metaFor(component: Awaited<ReturnType<typeof createState>>['components'][number]) {
  return {
    familyId: component.familyId,
    name: component.name,
    revision: component.revision,
    semanticCode: component.semanticCode,
    cacheKey: component.geometry.defaultCacheKey,
  };
}

describe('二维投影图（mesh HLR → SVG）', () => {
  it('生成三视图 + 轴测图，包含消隐虚线、尺寸与标题栏', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'PierBody')!;
    const svg = renderProjectionSvg(component.mesh, metaFor(component), {
      view: 'sheet',
      hidden: true,
      dimensions: true,
      generatedAt: '2026-10-19',
    });
    expect(svg.startsWith('<svg')).toBe(true);
    expect(svg).toContain('viewBox="0 0 1240 900"');
    expect(svg).toContain('主视图（正视）');
    expect(svg).toContain('俯视图');
    expect(svg).toContain('左视图（侧视）');
    expect(svg).toContain('轴测图');
    expect(svg).toContain('stroke-dasharray="5 4"');
    expect(svg).toContain('mm</text>');
    expect(svg).toContain('cacheKey');
    expect(svg).not.toContain('<script');
  });

  it('单视图支持前/顶/侧/轴测，且可关闭隐藏线', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'PierBody')!;
    for (const view of ['front', 'top', 'side', 'iso'] as const) {
      const svg = renderProjectionSvg(component.mesh, metaFor(component), {
        view,
        hidden: false,
        dimensions: true,
        generatedAt: '2026-10-19',
      });
      expect(svg).toContain('viewBox="0 0 900 650"');
      expect(svg).toContain('mm</text>');
      expect(svg).not.toContain('stroke-dasharray="5 4"');
    }
  });

  it('桥墩组合体俯视图的可见外轮廓不被误判为消隐线', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'CompositePier')!;
    const top = renderProjectionSvg(component.mesh, metaFor(component), {
      view: 'top',
      hidden: true,
      dimensions: true,
      generatedAt: '2026-10-19',
    });
    expect(top).toContain('俯视图');
    expect(top).not.toContain('stroke-dasharray="5 4"');
  });

  it('非参数化构件（native-brep / mesh-only）同样可生成投影图', async () => {    const state = await createState();
    for (const familyId of ['SignGantry', 'DrainageGrate']) {
      const component = state.components.find((item) => item.familyId === familyId)!;
      const svg = renderProjectionSvg(component.mesh, metaFor(component), {
        view: 'sheet',
        hidden: true,
        dimensions: true,
        generatedAt: '2026-10-19',
      });
      expect(svg).toContain('<svg');
      expect(svg).toContain('主视图');
      expect(svg).toContain('轴测图');
    }
  });

  it('构件名称中的 XML 特殊字符被转义', async () => {
    const state = await createState();
    const component = state.components.find((item) => item.familyId === 'PierBody')!;
    const svg = renderProjectionSvg(component.mesh, {
      ...metaFor(component),
      name: '<script>alert("x")</script>',
    }, { view: 'front' });
    expect(svg).not.toContain('<script>');
    expect(svg).toContain('&lt;script&gt;');
  });
});
