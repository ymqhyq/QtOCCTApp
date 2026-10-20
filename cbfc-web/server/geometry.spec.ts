import { describe, expect, it } from 'vitest';
import { buildMesh } from './geometry';

describe('几何生成器自检', () => {
  it('顶面法线朝上、体积为正且包围盒与参数一致', () => {
    const mesh = buildMesh('pier', { TopWidth: 2400, TopLength: 6400, PierHeight: 12000, Slope: 35 });
    let topNormalZ = 0;
    let topCount = 0;
    for (let t = 0; t < mesh.triangles; t += 1) {
      if (mesh.faceIds[t] !== 4) continue;
      const ia = mesh.indices[t * 3];
      topNormalZ += mesh.normals[ia * 3 + 2];
      topCount += 1;
    }
    expect(topNormalZ / topCount).toBeGreaterThan(0.9);
    expect(mesh.volumeMm3).toBeGreaterThan(1.5e11);
    expect(mesh.volumeMm3).toBeLessThan(2.5e11);
    expect(mesh.bounds.min[2]).toBe(0);
    expect(mesh.bounds.max[2]).toBe(12000);
    expect(mesh.faceIds.length).toBe(mesh.triangles);
    expect(mesh.edgeOffsets.length).toBe(mesh.edgesCount + 1);
  });

  it('工字形梁截面耳切三角化与管状截面均生成封闭网格', () => {
    const ibeam = buildMesh('ibeam', {
      LengthX: 30000,
      Depth: 2000,
      FlangeWidth: 1600,
      WebThickness: 240,
      FlangeThickness: 200,
    });
    const tube = buildMesh('tube', { OuterDiameter: 9600, Thickness: 500, LengthX: 12000 });
    expect(ibeam.volumeMm3).toBeGreaterThan(1e9);
    expect(tube.volumeMm3).toBeGreaterThan(1e9);
    expect(ibeam.facesCount).toBeGreaterThanOrEqual(4);
    expect(tube.facesCount).toBe(3);
  });

  it('接闪杆基础按双层基座、中段与四根立柱重建，不再是外包体', () => {
    const mesh = buildMesh('lightning-foundation', {
      BaseWidth: 2700,
      BaseHeight: 1000,
      MidWidth: 2200,
      MidHeight: 800,
      PostSize: 500,
      PostHeight: 900,
      PostOffset: 647.5,
    });
    expect(mesh.bounds.min[2]).toBe(0);
    expect(mesh.bounds.max[2]).toBe(2700);
    expect(mesh.faceGroups.length).toBeGreaterThanOrEqual(30);
    expect(mesh.volumeMm3).toBeGreaterThan(11e9);
    expect(mesh.volumeMm3).toBeLessThan(13e9);
    const topZ = mesh.bounds.max[2];
    const topVertices = [];
    for (let i = 0; i < mesh.positions.length / 3; i += 1) {
      if (Math.abs(mesh.positions[i * 3 + 2] - topZ) < 1e-6) {
        topVertices.push([mesh.positions[i * 3], mesh.positions[i * 3 + 1]]);
      }
    }
    expect(topVertices.length).toBeGreaterThanOrEqual(16);
  });
});
