import { describe, expect, it } from 'vitest';
import { createState } from './state';
import { encodeJhb } from './jhb';
import { buildMesh } from './geometry';
import { makeJhb, zipStore } from './artifacts';
import { SPEC_PIER_CACHE_KEY } from '../shared/deterministic';

describe('构件库种子与 JHB 编码', () => {
  it('41 个构件完成初始化，几何与缓存键均为实算值', async () => {
    const state = await createState();
    expect(state.components.length).toBe(41);
    expect(state.profiles.length).toBe(3);
    const pier = state.components.find((c) => c.familyId === 'PierBody')!;
    expect(pier.geometry.defaultCacheKey).toBe(SPEC_PIER_CACHE_KEY);
    expect(pier.geometry.volumeM3).toBeGreaterThan(150);
    expect(pier.geometry.volumeM3).toBeLessThan(260);
    expect(pier.mesh.triangles).toBeGreaterThan(500);
    expect(pier.mesh.faceIds.length).toBe(pier.mesh.triangles);
    expect(pier.shapes.PierBodySolid).toBeTruthy();
    expect(state.reviews.length).toBe(5);

    expect(state.components.find((c) => c.familyId === 'CompositePier')?.recipeSource).toBe(
      'cq_script/bridge_pier_cq2.py',
    );
    expect(state.components.find((c) => c.familyId === 'BedStone')?.recipeSource).toBe(
      'cq_script/BedStone.py',
    );
    expect(
      state.components.filter((c) => c.recipeSource?.startsWith('cq_script/')).length,
    ).toBeGreaterThanOrEqual(9);
    expect(state.components.reduce((sum, c) => sum + c.parameters.presets.length, 0)).toBe(111);

    for (const component of state.components) {
      const defaultModel = String(component.parameters.schema[0].default);
      expect(component.presetCacheKeys[defaultModel], `${component.familyId} default preset`).toBe(
        component.geometry.defaultCacheKey,
      );
    }
  });

  it('标准 JHB v1.4.3 帧头 4 字节对齐，payloadStart === 4 + jsonLen', async () => {
    const state = await createState();
    const component = state.components.find((c) => c.familyId === 'PierBody')!;
    const jhb = makeJhb(component, {}, SPEC_PIER_CACHE_KEY);
    expect(jhb.jsonLen % 4).toBe(0);
    expect(jhb.payloadStart).toBe(4 + jhb.jsonLen);
    expect(jhb.payloadStart % 4).toBe(0);
    expect(jhb.aligned).toBe(true);
    const header = jhb.header as any;
    const last = header.buffers.faceIds;
    expect(jhb.payloadStart + last.offset + last.length).toBe(jhb.buffer.byteLength);
    expect(header.vertexCount * 3 * 4).toBe(header.buffers.vertices.length);
    expect(header.triangleCount * 3 * 4).toBe(header.buffers.indices.length);
  });

  it('遗留未对齐 JHB 帧可被识别（通道二降级）', () => {
    const mesh = buildMesh('box', { LengthX: 1000, WidthY: 800, HeightZ: 500, Taper: 0 });
    const jhb = encodeJhb({
      mesh,
      cacheKey: 'f'.repeat(64),
      shapeKeys: ['A', 'B', 'C'],
      faceToShape: mesh.faceGroups.map(() => 0),
      edgeToShape: mesh.edgeLines.map(() => 0),
      outputs: {},
      args: {},
      legacyUnaligned: true,
    });
    expect(jhb.payloadStart).toBe(4 + jhb.jsonLen);
    expect(jhb.payloadStart % 4).not.toBe(0);
  });

  it('ZIP 制品包含 component.json / checksums.json 且可解压', async () => {
    const state = await createState();
    const component = state.components.find((c) => c.familyId === 'PierBody')!;
    const zip = zipStore([
      { name: 'component.json', data: new TextEncoder().encode('{"a":1}') },
      { name: 'checksums.json', data: new TextEncoder().encode('{"files":{}}') },
    ]);
    expect(zip[0]).toBe(0x50);
    expect(zip[1]).toBe(0x4b);
    expect(zip.length).toBeGreaterThan(100);
    expect(component.shapes.PierBodySolid.code).toBe('Bridge.Pier.BodySolid');
  });
});
