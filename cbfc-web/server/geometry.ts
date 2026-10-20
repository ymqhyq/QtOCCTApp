/**
 * 服务端权威几何生成器（Mock Worker 内核）
 * 以纯 JS 程序化生成 CBF-C 拓扑感知网格：顶点 / 法线 / 索引 / 特征边线 /
 * faceIds（面 -> 拓扑面）与 edgeShapes（特征边 -> shapeKey），
 * 供 JHB 编码器打包为规范二进制帧。
 */

export type GeometryKind = 'pier' | 'box' | 'cylinder' | 'ibeam' | 'tube' | 'hollow-box' | 'lightning-foundation';

export interface FaceGroup {
  faceId: number;
  shapeIndex: number;
  label: string;
  triangles: number;
  areaMm2: number;
  centroid: [number, number, number];
}

export interface EdgeLine {
  shapeIndex: number;
  closed: boolean;
  vertexStart: number;
  vertexCount: number;
  label: string;
}

export interface MeshData {
  positions: Float32Array;
  indices: Uint32Array;
  normals: Float32Array;
  edges: Float32Array;
  edgeOffsets: Uint32Array;
  edgeShapes: number[];
  faceIds: Uint16Array;
  faceGroups: FaceGroup[];
  edgeLines: EdgeLine[];
  bounds: { min: [number, number, number]; max: [number, number, number] };
  volumeMm3: number;
  areaMm2: number;
  vertices: number;
  triangles: number;
  facesCount: number;
  edgesCount: number;
}

type Vec3 = [number, number, number];

const FACE_LABELS = [
  '墩身侧面',
  '墩顶支撑垫石安装面',
  '墩底轮廓',
  '主体顶面',
  '主体底面',
  '内侧壁面',
  '顶缘环面',
  '顶板面',
  '底板面',
  '腹板面',
  '内壁面',
  '外壁面',
];

class MeshBuilder {
  positions: number[] = [];
  normals: number[] = [];
  indices: number[] = [];
  faceIds: number[] = [];
  edgePoints: number[] = [];
  edgeOffsets: number[] = [0];
  edgeShapes: number[] = [];
  edgeLines: EdgeLine[] = [];
  private areas = new Map<number, number>();
  private centroids = new Map<number, Vec3>();
  private triCounts = new Map<number, number>();

  private pushVertex(p: Vec3, n: Vec3): number {
    const index = this.positions.length / 3;
    this.positions.push(p[0], p[1], p[2]);
    this.normals.push(n[0], n[1], n[2]);
    return index;
  }

  addTriangle(a: Vec3, b: Vec3, c: Vec3, faceId: number, normal?: Vec3): void {
    const n = normal ?? triangleNormal(a, b, c);
    const ia = this.pushVertex(a, n);
    const ib = this.pushVertex(b, n);
    const ic = this.pushVertex(c, n);
    this.indices.push(ia, ib, ic);
    this.faceIds.push(faceId);

    const area = triangleArea(a, b, c);
    this.areas.set(faceId, (this.areas.get(faceId) ?? 0) + area);
    this.triCounts.set(faceId, (this.triCounts.get(faceId) ?? 0) + 1);
    const prev = this.centroids.get(faceId) ?? [0, 0, 0];
    const w = area / Math.max(this.areas.get(faceId) ?? area, 1e-9);
    this.centroids.set(faceId, [
      prev[0] * (1 - w) + ((a[0] + b[0] + c[0]) / 3) * w,
      prev[1] * (1 - w) + ((a[1] + b[1] + c[1]) / 3) * w,
      prev[2] * (1 - w) + ((a[2] + b[2] + c[2]) / 3) * w,
    ]);
  }

  /** 逆时针四边形（a-b-c-d）拆两个三角形 */
  addQuad(a: Vec3, b: Vec3, c: Vec3, d: Vec3, faceId: number): void {
    const n = quadNormal(a, b, c);
    const ia = this.pushVertex(a, n);
    const ib = this.pushVertex(b, n);
    const ic = this.pushVertex(c, n);
    const id = this.pushVertex(d, n);
    this.indices.push(ia, ib, ic, ia, ic, id);
    const area = quadArea(a, b, c, d);
    this.faceIds.push(faceId, faceId);
    this.areas.set(faceId, (this.areas.get(faceId) ?? 0) + area);
    this.triCounts.set(faceId, (this.triCounts.get(faceId) ?? 0) + 2);
    const prev = this.centroids.get(faceId) ?? [0, 0, 0];
    const w = area / Math.max(this.areas.get(faceId) ?? area, 1e-9);
    this.centroids.set(faceId, [
      prev[0] * (1 - w) + ((a[0] + b[0] + c[0] + d[0]) / 4) * w,
      prev[1] * (1 - w) + ((a[1] + b[1] + c[1] + d[1]) / 4) * w,
      prev[2] * (1 - w) + ((a[2] + b[2] + c[2] + d[2]) / 4) * w,
    ]);
  }

  addTriFan(points: Vec3[], faceId: number, flip = false): void {
    if (points.length < 3) return;
    const center: Vec3 = [0, 0, 0];
    for (const p of points) {
      center[0] += p[0] / points.length;
      center[1] += p[1] / points.length;
      center[2] += p[2] / points.length;
    }
    for (let i = 0; i < points.length; i += 1) {
      const a = points[i];
      const b = points[(i + 1) % points.length];
      if (flip) this.addTriangle(center, b, a, faceId);
      else this.addTriangle(center, a, b, faceId);
    }
  }

  addPolyline(points: Vec3[], shapeIndex: number, label: string, closed = true): void {
    if (points.length < 2) return;
    const start = this.edgePoints.length / 3;
    for (const p of points) this.edgePoints.push(p[0], p[1], p[2]);
    this.edgeLines.push({
      shapeIndex,
      closed,
      vertexStart: start,
      vertexCount: points.length,
      label,
    });
    this.edgeShapes.push(shapeIndex);
    this.edgeOffsets.push(this.edgePoints.length / 3);
  }

  finalize(): MeshData {
    const positions = new Float32Array(this.positions);
    const indices = new Uint32Array(this.indices);
    const normals = new Float32Array(this.normals);
    const edges = new Float32Array(this.edgePoints);
    const edgeOffsets = new Uint32Array(this.edgeOffsets);
    const faceIds = new Uint16Array(this.faceIds);
    const min: Vec3 = [Infinity, Infinity, Infinity];
    const max: Vec3 = [-Infinity, -Infinity, -Infinity];
    for (let i = 0; i < positions.length; i += 3) {
      for (let k = 0; k < 3; k += 1) {
        min[k] = Math.min(min[k], positions[i + k]);
        max[k] = Math.max(max[k], positions[i + k]);
      }
    }
    let volumeMm3 = 0;
    for (let i = 0; i < indices.length; i += 3) {
      const a = [positions[indices[i] * 3], positions[indices[i] * 3 + 1], positions[indices[i] * 3 + 2]];
      const b = [
        positions[indices[i + 1] * 3],
        positions[indices[i + 1] * 3 + 1],
        positions[indices[i + 1] * 3 + 2],
      ];
      const c = [
        positions[indices[i + 2] * 3],
        positions[indices[i + 2] * 3 + 1],
        positions[indices[i + 2] * 3 + 2],
      ];
      volumeMm3 += signedTetraVolume(a, b, c);
    }
    const faceGroups: FaceGroup[] = [];
    for (const faceId of [...this.areas.keys()].sort((a, b) => a - b)) {
      faceGroups.push({
        faceId,
        shapeIndex: 0,
        label: FACE_LABELS[faceId] ?? `拓扑面 ${faceId}`,
        triangles: this.triCounts.get(faceId) ?? 0,
        areaMm2: this.areas.get(faceId) ?? 0,
        centroid: this.centroids.get(faceId) ?? [0, 0, 0],
      });
    }
    const areaMm2 = [...this.areas.values()].reduce((sum, v) => sum + v, 0);
    return {
      positions,
      indices,
      normals,
      edges,
      edgeOffsets,
      edgeShapes: this.edgeShapes,
      faceIds,
      faceGroups,
      edgeLines: this.edgeLines,
      bounds: { min, max },
      volumeMm3: Math.abs(volumeMm3),
      areaMm2,
      vertices: positions.length / 3,
      triangles: indices.length / 3,
      facesCount: faceGroups.length,
      edgesCount: this.edgeLines.length,
    };
  }
}

function sub(a: Vec3, b: Vec3): Vec3 {
  return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}
function cross(a: Vec3, b: Vec3): Vec3 {
  return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
}
function dot(a: Vec3, b: Vec3): number {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
function length(a: Vec3): number {
  return Math.hypot(a[0], a[1], a[2]);
}
function normalize(a: Vec3): Vec3 {
  const l = length(a) || 1;
  return [a[0] / l, a[1] / l, a[2] / l];
}
function triangleNormal(a: Vec3, b: Vec3, c: Vec3): Vec3 {
  return normalize(cross(sub(b, a), sub(c, a)));
}
function quadNormal(a: Vec3, b: Vec3, c: Vec3): Vec3 {
  return triangleNormal(a, b, c);
}
function triangleArea(a: Vec3, b: Vec3, c: Vec3): number {
  return length(cross(sub(b, a), sub(c, a))) / 2;
}
function quadArea(a: Vec3, b: Vec3, c: Vec3, d: Vec3): number {
  return triangleArea(a, b, c) + triangleArea(a, c, d);
}
function signedTetraVolume(a: number[], b: number[], c: number[]): number {
  return (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) + a[2] * (b[0] * c[1] - b[1] * c[0])) / 6;
}

/** 圆端形（Slot）截面离散点：CCW，起点 (straight, -halfW) */
function slotProfile(lengthX: number, widthY: number, segments: number): Vec3[] {
  const halfL = lengthX / 2;
  const halfW = widthY / 2;
  const r = Math.min(halfW, halfL);
  const straight = halfL - r;
  const perQuarter = Math.max(2, Math.round(segments / 4));
  const pts: Vec3[] = [];
  // 右半圆：-90° -> 90°
  for (let i = 0; i <= perQuarter; i += 1) {
    const t = -Math.PI / 2 + (Math.PI * i) / perQuarter;
    pts.push([straight + r * Math.cos(t), r * Math.sin(t), 0]);
  }
  // 上直线：+X 侧 -> -X 侧
  for (let i = 1; i < perQuarter; i += 1) {
    const x = straight - (2 * straight * i) / perQuarter;
    pts.push([x, halfW, 0]);
  }
  // 左半圆：90° -> 270°
  for (let i = 0; i <= perQuarter; i += 1) {
    const t = Math.PI / 2 + (Math.PI * i) / perQuarter;
    pts.push([-straight + r * Math.cos(t), r * Math.sin(t), 0]);
  }
  // 下直线：-X 侧 -> +X 侧
  for (let i = 1; i < perQuarter; i += 1) {
    const x = -straight + (2 * straight * i) / perQuarter;
    pts.push([x, -halfW, 0]);
  }
  // 去掉闭合重复点
  const unique: Vec3[] = [];
  for (const p of pts) {
    const last = unique[unique.length - 1];
    if (!last || Math.hypot(p[0] - last[0], p[1] - last[1]) > 1e-6) unique.push(p);
  }
  return unique;
}

function boxCorners(lengthX: number, widthY: number, z0: number, z1: number): Vec3[] {
  const hx = lengthX / 2;
  const hy = widthY / 2;
  return [
    [-hx, -hy, z0],
    [hx, -hy, z0],
    [hx, hy, z0],
    [-hx, hy, z0],
    [-hx, -hy, z1],
    [hx, -hy, z1],
    [hx, hy, z1],
    [-hx, hy, z1],
  ];
}

function addBox(b: MeshBuilder, lengthX: number, widthY: number, z0: number, z1: number, faceIds: number[]): void {
  const [p0, p1, p2, p3, p4, p5, p6, p7] = boxCorners(lengthX, widthY, z0, z1);
  b.addQuad(p0, p3, p2, p1, faceIds[0]); // bottom (down)
  b.addQuad(p4, p5, p6, p7, faceIds[1]); // top
  b.addQuad(p0, p1, p5, p4, faceIds[2]); // -Y
  b.addQuad(p1, p2, p6, p5, faceIds[3]); // +X
  b.addQuad(p2, p3, p7, p6, faceIds[4]); // +Y
  b.addQuad(p3, p0, p4, p7, faceIds[5]); // -X
}

function boxCornersAt(cx: number, cy: number, lengthX: number, widthY: number, z0: number, z1: number): Vec3[] {
  const x0 = cx - lengthX / 2;
  const x1 = cx + lengthX / 2;
  const y0 = cy - widthY / 2;
  const y1 = cy + widthY / 2;
  return [
    [x0, y0, z0],
    [x1, y0, z0],
    [x1, y1, z0],
    [x0, y1, z0],
    [x0, y0, z1],
    [x1, y0, z1],
    [x1, y1, z1],
    [x0, y1, z1],
  ];
}

function addBoxAt(
  b: MeshBuilder,
  cx: number,
  cy: number,
  lengthX: number,
  widthY: number,
  z0: number,
  z1: number,
  faceIds: number[],
): void {
  const [p0, p1, p2, p3, p4, p5, p6, p7] = boxCornersAt(cx, cy, lengthX, widthY, z0, z1);
  b.addQuad(p0, p3, p2, p1, faceIds[0]);
  b.addQuad(p4, p5, p6, p7, faceIds[1]);
  b.addQuad(p0, p1, p5, p4, faceIds[2]);
  b.addQuad(p1, p2, p6, p5, faceIds[3]);
  b.addQuad(p2, p3, p7, p6, faceIds[4]);
  b.addQuad(p3, p0, p4, p7, faceIds[5]);
}

function addBoxEdges(
  b: MeshBuilder,
  cx: number,
  cy: number,
  lengthX: number,
  widthY: number,
  z0: number,
  z1: number,
  shapeIndex: number,
  label: string,
): void {
  const c = boxCornersAt(cx, cy, lengthX, widthY, z0, z1);
  polyline(b, [c[0], c[1], c[2], c[3]], shapeIndex, `${label}底面轮廓`, true);
  polyline(b, [c[4], c[5], c[6], c[7]], shapeIndex, `${label}顶面轮廓`, true);
  for (let i = 0; i < 4; i += 1) {
    polyline(b, [c[i], c[i + 4]], shapeIndex, `${label}竖向棱线`, false);
  }
}

function polyline(b: MeshBuilder, points: Vec3[], shapeIndex: number, label: string, closed = true): void {
  b.addPolyline(points, shapeIndex, label, closed);
}

/** 圆端型桥墩墩身：底部截面按坡度放大，顶部略收 */
function buildPier(params: Record<string, number>): MeshData {
  const topWidth = params.TopWidth ?? 2400;
  const topLength = params.TopLength ?? 6400;
  const height = params.PierHeight ?? 12000;
  const slope = Math.max(params.Slope ?? 35, 1);
  const segments = 64;
  const levels = 14;
  const b = new MeshBuilder();
  const rings: Vec3[][] = [];
  const bottomWidth = topWidth + (height / slope) * 2;
  const bottomLength = topLength + (height / slope) * 2;

  for (let k = 0; k <= levels; k += 1) {
    const u = k / levels;
    const z = height * u;
    const width = bottomWidth + (topWidth - bottomWidth) * u;
    const lengthX = bottomLength + (topLength - bottomLength) * u;
    const profile = slotProfile(lengthX, width, segments);
    rings.push(profile.map((p) => [p[0], p[1], z] as Vec3));
  }

  for (let k = 0; k < levels; k += 1) {
    const lower = rings[k];
    const upper = rings[k + 1];
    const n = lower.length;
    for (let i = 0; i < n; i += 1) {
      const j = (i + 1) % n;
      // 拓扑面分组：0..3 侧面（两端半圆 + 两侧平面），4 顶面，5 底面
      const seg = i / n;
      let face = 0;
      if (seg >= 0.25 && seg < 0.5) face = 1;
      else if (seg >= 0.5 && seg < 0.75) face = 2;
      else if (seg >= 0.75) face = 3;
      b.addQuad(lower[i], lower[j], upper[j], upper[i], face);
    }
  }
  b.addTriFan(rings[levels], 4, false);
  b.addTriFan(rings[0], 5, true);

  polyline(b, rings[0], 2, '墩底截面轮廓边界线');
  polyline(b, rings[levels], 1, '墩顶支撑垫石安装面轮廓');
  for (const idx of [0, Math.round(rings[0].length * 0.25), Math.round(rings[0].length * 0.5), Math.round(rings[0].length * 0.75)]) {
    polyline(b, [rings[0][idx], rings[levels][idx]], 0, '墩身侧棱', false);
  }

  const mesh = b.finalize();
  mesh.faceGroups.forEach((g) => {
    if (g.faceId <= 3) g.shapeIndex = 0;
    else if (g.faceId === 4) g.shapeIndex = 1;
    else g.shapeIndex = 0;
  });
  return mesh;
}

/** 矩形（可收分）棱柱：承台 / 顶帽 / 托盘 / 台身 / 板 */
function buildBox(params: Record<string, number>): MeshData {
  const lengthX = params.LengthX ?? params.TopLength ?? 6400;
  const widthY = params.WidthY ?? params.TopWidth ?? 2400;
  const height = params.HeightZ ?? params.Height ?? 2000;
  const taper = params.Taper ?? 0; // 顶面相对底面的收分比例
  const b = new MeshBuilder();
  const z0 = 0;
  const z1 = height;
  const topL = lengthX * (1 - taper);
  const topW = widthY * (1 - taper);
  const bottom: Vec3[] = [
    [-lengthX / 2, -widthY / 2, z0],
    [lengthX / 2, -widthY / 2, z0],
    [lengthX / 2, widthY / 2, z0],
    [-lengthX / 2, widthY / 2, z0],
  ];
  const top: Vec3[] = [
    [-topL / 2, -topW / 2, z1],
    [topL / 2, -topW / 2, z1],
    [topL / 2, topW / 2, z1],
    [-topL / 2, topW / 2, z1],
  ];
  b.addQuad(bottom[0], bottom[3], bottom[2], bottom[1], 1);
  b.addQuad(top[0], top[1], top[2], top[3], 0);
  for (let i = 0; i < 4; i += 1) {
    const j = (i + 1) % 4;
    b.addQuad(bottom[i], bottom[j], top[j], top[i], 2 + i);
  }
  polyline(b, bottom, 2, '底面轮廓线');
  polyline(b, top, 1, '顶面轮廓线');
  for (let i = 0; i < 4; i += 1) polyline(b, [bottom[i], top[i]], 0, '竖向棱线', false);

  const mesh = b.finalize();
  mesh.faceGroups.forEach((g) => {
    g.shapeIndex = g.faceId === 0 ? 1 : 0;
  });
  return mesh;
}

/** 接闪杆基础：双层基座 + 中段 + 四根对称立柱（cq_script/lightning_rod_foundation.py） */
function buildLightningFoundation(params: Record<string, number>): MeshData {
  const baseWidth = params.BaseWidth ?? 2700;
  const baseHeight = params.BaseHeight ?? 1000;
  const midWidth = params.MidWidth ?? 2200;
  const midHeight = params.MidHeight ?? 800;
  const postSize = params.PostSize ?? 500;
  const postHeight = params.PostHeight ?? 900;
  const postOffset = params.PostOffset ?? 647.5;
  const b = new MeshBuilder();

  const baseTop = baseHeight;
  const midTop = baseTop + midHeight;
  addBoxAt(b, 0, 0, baseWidth, baseWidth, 0, baseTop, [0, 1, 2, 3, 4, 5]);
  addBoxAt(b, 0, 0, midWidth, midWidth, baseTop, midTop, [6, 7, 8, 9, 10, 11]);

  let face = 12;
  for (const cx of [-postOffset, postOffset]) {
    for (const cy of [-postOffset, postOffset]) {
      addBoxAt(b, cx, cy, postSize, postSize, midTop, midTop + postHeight, [
        face,
        face + 1,
        face + 2,
        face + 3,
        face + 4,
        face + 5,
      ]);
      face += 6;
    }
  }

  addBoxEdges(b, 0, 0, baseWidth, baseWidth, 0, baseTop, 0, '承台');
  addBoxEdges(b, 0, 0, midWidth, midWidth, baseTop, midTop, 1, '中段');
  for (const cx of [-postOffset, postOffset]) {
    for (const cy of [-postOffset, postOffset]) {
      addBoxEdges(b, cx, cy, postSize, postSize, midTop, midTop + postHeight, 2, '立柱');
    }
  }

  const mesh = b.finalize();
  mesh.faceGroups.forEach((group) => {
    group.shapeIndex = group.faceId <= 5 ? 0 : group.faceId <= 11 ? 1 : 2;
  });
  return mesh;
}

/** 圆柱：钻孔灌注桩 / 墩柱 / 盆式支座 */
function buildCylinder(params: Record<string, number>): MeshData {
  const diameter = params.Diameter ?? 1500;
  const height = params.HeightZ ?? params.Height ?? 20000;
  const segments = 48;
  const b = new MeshBuilder();
  const r = diameter / 2;
  const bottom: Vec3[] = [];
  const top: Vec3[] = [];
  for (let i = 0; i < segments; i += 1) {
    const t = (i / segments) * Math.PI * 2;
    bottom.push([r * Math.cos(t), r * Math.sin(t), 0]);
    top.push([r * Math.cos(t), r * Math.sin(t), height]);
  }
  for (let i = 0; i < segments; i += 1) {
    const j = (i + 1) % segments;
    b.addQuad(bottom[i], bottom[j], top[j], top[i], 0);
  }
  b.addTriFan(top, 1, false);
  b.addTriFan(bottom, 2, true);
  polyline(b, bottom, 2, '桩底轮廓线');
  polyline(b, top, 1, '桩顶面轮廓线');
  return b.finalize();
}

/** 工字形梁：沿 X 轴挤出 I 型截面 */
function buildIBeam(params: Record<string, number>): MeshData {
  const lengthX = params.LengthX ?? 30000;
  const depth = params.Depth ?? 1800;
  const flangeWidth = params.FlangeWidth ?? 1200;
  const webThickness = params.WebThickness ?? 240;
  const flangeThickness = params.FlangeThickness ?? 200;
  const b = new MeshBuilder();
  const hx = lengthX / 2;
  const hd = depth / 2;
  const hf = flangeWidth / 2;
  const hw = webThickness / 2;
  const ft = flangeThickness;
  // 截面多边形（Y, Z）逆时针
  const profile: [number, number][] = [
    [-hf, -hd],
    [hf, -hd],
    [hf, -hd + ft],
    [hw, -hd + ft],
    [hw, hd - ft],
    [hf, hd - ft],
    [hf, hd],
    [-hf, hd],
    [-hf, hd - ft],
    [-hw, hd - ft],
    [-hw, -hd + ft],
    [-hf, -hd + ft],
  ];
  const n = profile.length;
  const quadFace = (i: number): number => {
    // 0 下翼缘, 1 腹板, 2 上翼缘
    if (i === 0 || i === 1 || i === 10 || i === 11) return 2;
    if (i === 3 || i === 4 || i === 7 || i === 8) return 4;
    return 3;
  };
  for (let i = 0; i < n; i += 1) {
    const j = (i + 1) % n;
    const a: Vec3 = [-hx, profile[i][0], profile[i][1]];
    const bb: Vec3 = [hx, profile[i][0], profile[i][1]];
    const c: Vec3 = [hx, profile[j][0], profile[j][1]];
    const d: Vec3 = [-hx, profile[j][0], profile[j][1]];
    // 截面多边形为 CCW（Y-Z 平面），挤出 +X，侧壁需按 -X 法向顺序
    b.addQuad(a, d, c, bb, quadFace(i));
  }
  // 端面（耳切三角化）
  const startPts = profile.map(([y, z]) => [-hx, y, z] as Vec3);
  const endPts = profile.map(([y, z]) => [hx, y, z] as Vec3);
  for (const tri of earClip(profile)) {
    b.addTriangle(startPts[tri[0]], startPts[tri[2]], startPts[tri[1]], 0);
    b.addTriangle(endPts[tri[0]], endPts[tri[1]], endPts[tri[2]], 1);
  }
  const startLoop = startPts.map((p) => p);
  const endLoop = endPts.map((p) => p);
  polyline(b, startLoop, 4, '梁端截面轮廓线');
  polyline(b, endLoop, 4, '梁端截面轮廓线');
  for (const i of [0, 2, 3, 5, 6, 8, 9, 11]) {
    polyline(b, [startPts[i], endPts[i]], 0, '翼缘纵向棱线', false);
  }
  const mesh = b.finalize();
  mesh.faceGroups.forEach((g) => {
    if (g.faceId === 0 || g.faceId === 1) g.shapeIndex = 0;
    else if (g.faceId === 2) g.shapeIndex = 1;
    else if (g.faceId === 4) g.shapeIndex = 2;
    else g.shapeIndex = 3;
  });
  return mesh;
}

/** 圆管：隧道二次衬砌 */
function buildTube(params: Record<string, number>): MeshData {
  const outer = params.OuterDiameter ?? 9600;
  const thickness = params.Thickness ?? 500;
  const lengthX = params.LengthX ?? 12000;
  const segments = 48;
  const b = new MeshBuilder();
  const ro = outer / 2;
  const ri = ro - thickness;
  const x0 = -lengthX / 2;
  const x1 = lengthX / 2;
  const ang = (i: number) => (i / segments) * Math.PI * 2;
  for (let i = 0; i < segments; i += 1) {
    const t0 = ang(i);
    const t1 = ang(i + 1);
    const oa: Vec3 = [x0, ro * Math.cos(t0), ro * Math.sin(t0)];
    const ob: Vec3 = [x0, ro * Math.cos(t1), ro * Math.sin(t1)];
    const oc: Vec3 = [x1, ro * Math.cos(t1), ro * Math.sin(t1)];
    const od: Vec3 = [x1, ro * Math.cos(t0), ro * Math.sin(t0)];
    b.addQuad(oa, od, oc, ob, 0); // 外壁
    const ia: Vec3 = [x0, ri * Math.cos(t0), ri * Math.sin(t0)];
    const ib: Vec3 = [x0, ri * Math.cos(t1), ri * Math.sin(t1)];
    const ic: Vec3 = [x1, ri * Math.cos(t1), ri * Math.sin(t1)];
    const id: Vec3 = [x1, ri * Math.cos(t0), ri * Math.sin(t0)];
    b.addQuad(ia, ib, ic, id, 1); // 内壁
    b.addQuad(oa, ob, ib, ia, 2); // 端环
    b.addQuad(od, id, ic, oc, 2);
  }
  const outerLoop = Array.from({ length: segments }, (_, i) => [x0, ro * Math.cos(ang(i)), ro * Math.sin(ang(i))] as Vec3);
  const innerLoop = Array.from({ length: segments }, (_, i) => [x0, ri * Math.cos(ang(i)), ri * Math.sin(ang(i))] as Vec3);
  polyline(b, outerLoop, 3, '衬砌外轮廓线');
  polyline(b, innerLoop, 3, '衬砌内轮廓线');
  const mesh = b.finalize();
  mesh.faceGroups.forEach((g) => {
    if (g.faceId === 0) g.shapeIndex = 0;
    else if (g.faceId === 1) g.shapeIndex = 1;
    else g.shapeIndex = 2;
  });
  return mesh;
}

/** 空心矩形：箱涵 / 电缆槽 / 空心墩 */
function buildHollowBox(params: Record<string, number>): MeshData {
  const lengthX = params.LengthX ?? 4000;
  const widthY = params.WidthY ?? 3000;
  const height = params.HeightZ ?? 3000;
  const wall = params.WallThickness ?? 400;
  const b = new MeshBuilder();
  const hx = lengthX / 2;
  const hy = widthY / 2;
  const ix = hx - wall;
  const iy = hy - wall;
  const innerZ = wall;
  const outer: Vec3[][] = [
    [[-hx, -hy, 0], [hx, -hy, 0], [hx, hy, 0], [-hx, hy, 0]],
    [[-hx, -hy, height], [hx, -hy, height], [hx, hy, height], [-hx, hy, height]],
  ];
  const inner: Vec3[][] = [
    [[-ix, -iy, innerZ], [ix, -iy, innerZ], [ix, iy, innerZ], [-ix, iy, innerZ]],
    [[-ix, -iy, height], [ix, -iy, height], [ix, iy, height], [-ix, iy, height]],
  ];
  const ob = outer[0];
  const ot = outer[1];
  const ib = inner[0];
  const it = inner[1];
  b.addQuad(ob[0], ob[3], ob[2], ob[1], 0); // 外底
  for (let i = 0; i < 4; i += 1) {
    const j = (i + 1) % 4;
    b.addQuad(ob[i], ob[j], ot[j], ot[i], 0); // 外壁
    b.addQuad(ib[i], it[i], it[j], ib[j], 1); // 内壁（法向朝内）
    b.addQuad(ot[i], it[i], it[j], ot[j], 2); // 顶缘环面
    b.addQuad(ob[i], ib[i], ib[j], ob[j], 3); // 底缘环面
  }
  polyline(b, ob, 3, '外轮廓底边');
  polyline(b, ot, 0, '外轮廓顶边');
  polyline(b, it, 1, '内腔顶边');
  for (const i of [0, 1, 2, 3]) {
    polyline(b, [ob[i], ot[i]], 0, '外壁竖棱', false);
    polyline(b, [ib[i], it[i]], 1, '内壁竖棱', false);
  }
  const mesh = b.finalize();
  mesh.faceGroups.forEach((g) => {
    if (g.faceId === 0) g.shapeIndex = 0;
    else if (g.faceId === 1) g.shapeIndex = 1;
    else if (g.faceId === 2) g.shapeIndex = 2;
    else g.shapeIndex = 0;
  });
  return mesh;
}

/** 二维耳切三角化（凸/凹简单多边形，CCW 输入） */
function earClip(points: [number, number][]): [number, number, number][] {
  const n = points.length;
  const idx = Array.from({ length: n }, (_, i) => i);
  const triangles: [number, number, number][] = [];
  const area2 = (a: [number, number], b: [number, number], c: [number, number]) =>
    (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  let guard = 0;
  while (idx.length > 3 && guard < 200) {
    guard += 1;
    let clipped = false;
    for (let i = 0; i < idx.length; i += 1) {
      const i0 = idx[(i - 1 + idx.length) % idx.length];
      const i1 = idx[i];
      const i2 = idx[(i + 1) % idx.length];
      const a = points[i0];
      const b = points[i1];
      const c = points[i2];
      if (area2(a, b, c) <= 1e-9) continue;
      let contains = false;
      for (const k of idx) {
        if (k === i0 || k === i1 || k === i2) continue;
        const p = points[k];
        if (area2(a, b, p) >= 0 && area2(b, c, p) >= 0 && area2(c, a, p) >= 0) contains = true;
      }
      if (contains) continue;
      triangles.push([i0, i1, i2]);
      idx.splice(i, 1);
      clipped = true;
      break;
    }
    if (!clipped) break;
  }
  if (idx.length === 3) triangles.push([idx[0], idx[1], idx[2]]);
  return triangles;
}

export function buildMesh(kind: GeometryKind, params: Record<string, number>): MeshData {
  switch (kind) {
    case 'pier':
      return buildPier(params);
    case 'box':
      return buildBox(params);
    case 'cylinder':
      return buildCylinder(params);
    case 'ibeam':
      return buildIBeam(params);
    case 'tube':
      return buildTube(params);
    case 'hollow-box':
      return buildHollowBox(params);
    case 'lightning-foundation':
      return buildLightningFoundation(params);
    default:
      throw new Error(`未知几何类型: ${kind satisfies never}`);
  }
}

/** 等轴测 SVG 预览（供 <img> 加载，天然隔离 SVG 脚本注入） */
export function renderPreviewSvg(
  mesh: MeshData,
  shapeCodes: string[],
  width = 360,
  height = 260,
): string {
  const { min, max } = mesh.bounds;
  const cx = (min[0] + max[0]) / 2;
  const cy = (min[1] + max[1]) / 2;
  const cz = (min[2] + max[2]) / 2;
  const cos = Math.cos(Math.PI / 6);
  const sin = Math.sin(Math.PI / 6);
  // 轴测投影：x' = (x-y)cos30, y' = (x+y)sin30 - z
  const project = (p: number[]): [number, number] => {
    const x = p[0] - cx;
    const y = p[1] - cy;
    const z = (p[2] - cz) * 1.0;
    return [(x - y) * cos, (x + y) * sin - z];
  };
  const projected = new Float32Array((mesh.positions.length / 3) * 2);
  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  for (let i = 0; i < mesh.positions.length / 3; i += 1) {
    const [px, py] = project([
      mesh.positions[i * 3],
      mesh.positions[i * 3 + 1],
      mesh.positions[i * 3 + 2],
    ]);
    projected[i * 2] = px;
    projected[i * 2 + 1] = py;
    minX = Math.min(minX, px);
    maxX = Math.max(maxX, px);
    minY = Math.min(minY, py);
    maxY = Math.max(maxY, py);
  }
  const pad = 18;
  const scale = Math.min((width - pad * 2) / (maxX - minX || 1), (height - pad * 2) / (maxY - minY || 1));
  const ox = width / 2 - ((minX + maxX) / 2) * scale;
  const oy = height / 2 - ((minY + maxY) / 2) * scale;
  const tx = (i: number) => (projected[i * 2] * scale + ox).toFixed(2);
  const ty = (i: number) => (projected[i * 2 + 1] * scale + oy).toFixed(2);

  // 画家算法：按平均深度排序三角形；先做背面剔除，控制 SVG 体积
  const tris: { idx: [number, number, number]; depth: number; light: number }[] = [];
  const inv = 1 / Math.sqrt(3);
  for (let t = 0; t < mesh.triangles; t += 1) {
    const ia = mesh.indices[t * 3];
    const ib = mesh.indices[t * 3 + 1];
    const ic = mesh.indices[t * 3 + 2];
    const nx = mesh.normals[ia * 3];
    const ny = mesh.normals[ia * 3 + 1];
    const nz = mesh.normals[ia * 3 + 2];
    if (nx * inv + ny * inv + nz * inv <= 0.001) continue;
    const dz =
      mesh.positions[ia * 3 + 2] + mesh.positions[ib * 3 + 2] + mesh.positions[ic * 3 + 2];
    const dx = mesh.positions[ia * 3] + mesh.positions[ib * 3] + mesh.positions[ic * 3];
    const dy = mesh.positions[ia * 3 + 1] + mesh.positions[ib * 3 + 1] + mesh.positions[ic * 3 + 1];
    const light = Math.max(0.25, Math.min(1, nx * 0.3 + ny * 0.42 + nz * 0.86));
    tris.push({ idx: [ia, ib, ic], depth: dx + dy + dz, light });
  }
  tris.sort((a, b) => a.depth - b.depth);
  const palette = ['#c7d0da', '#b9c4d1', '#aeb9c7', '#d3dae2'];
  const body = tris
    .map((tri, i) => {
      const shade = palette[Math.min(palette.length - 1, Math.floor((1 - tri.light) * palette.length))];
      return `<polygon points="${tx(tri.idx[0])},${ty(tri.idx[0])} ${tx(tri.idx[1])},${ty(tri.idx[1])} ${tx(tri.idx[2])},${ty(tri.idx[2])}" fill="${shade}" stroke="${shade}" stroke-width="0.4"/>`;
    })
    .join('');

  // 特征边线：edges 是独立顶点流，不能直接复用 mesh.positions 的投影索引
  let edgesSvg = '';
  for (let e = 0; e < mesh.edgeLines.length; e += 1) {
    const line = mesh.edgeLines[e];
    const pts: string[] = [];
    for (let v = line.vertexStart; v < line.vertexStart + line.vertexCount; v += 1) {
      const [px, py] = project([
        mesh.edges[v * 3],
        mesh.edges[v * 3 + 1],
        mesh.edges[v * 3 + 2],
      ]);
      pts.push(`${(px * scale + ox).toFixed(2)},${(py * scale + oy).toFixed(2)}`);
    }
    if (line.closed && pts.length) pts.push(pts[0]);
    edgesSvg += `<polyline points="${pts.join(' ')}" fill="none" stroke="#0f172a" stroke-width="0.9" stroke-linejoin="round" stroke-linecap="round" opacity="0.85"/>`;
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="构件等轴测预览">
<defs><radialGradient id="bg" cx="50%" cy="38%" r="72%"><stop offset="0%" stop-color="#ffffff"/><stop offset="100%" stop-color="#eef2f6"/></radialGradient></defs>
<rect width="${width}" height="${height}" fill="url(#bg)"/>
<g>${body}</g>
<g>${edgesSvg}</g>
</svg>`;
}

/** 真实 GLB（glTF 2.0 二进制）导出，供通用三维引擎消费 */
export function encodeGlb(mesh: MeshData, name: string): Uint8Array {
  const posBytes = mesh.positions.byteLength;
  const nrmBytes = mesh.normals.byteLength;
  const idxBytes = mesh.indices.byteLength;
  const idxPad = (4 - (idxBytes % 4)) % 4;
  const binLength = posBytes + nrmBytes + idxBytes + idxPad;
  const encoder = new TextEncoder();
  const json = {
    asset: { version: '2.0', generator: 'CBF-C Web Library mock exporter v2.5' },
    scene: 0,
    scenes: [{ nodes: [0], name }],
    nodes: [{ mesh: 0, name }],
    meshes: [
      {
        name,
        primitives: [
          {
            attributes: { POSITION: 0, NORMAL: 1 },
            indices: 2,
            material: 0,
            mode: 4,
          },
        ],
      },
    ],
    materials: [
      {
        pbrMetallicRoughness: {
          baseColorFactor: [0.72, 0.76, 0.8, 1],
          metallicFactor: 0.1,
          roughnessFactor: 0.86,
        },
      },
    ],
    buffers: [{ byteLength: binLength }],
    bufferViews: [
      { buffer: 0, byteOffset: 0, byteLength: posBytes, target: 34962 },
      { buffer: 0, byteOffset: posBytes, byteLength: nrmBytes, target: 34962 },
      { buffer: 0, byteOffset: posBytes + nrmBytes, byteLength: idxBytes, target: 34963 },
    ],
    accessors: [
      {
        bufferView: 0,
        componentType: 5126,
        count: mesh.vertices,
        type: 'VEC3',
        min: mesh.bounds.min,
        max: mesh.bounds.max,
      },
      { bufferView: 1, componentType: 5126, count: mesh.vertices, type: 'VEC3' },
      { bufferView: 2, componentType: 5125, count: mesh.indices.length, type: 'SCALAR' },
    ],
  };
  let jsonText = JSON.stringify(json);
  while (jsonText.length % 4 !== 0) jsonText += ' ';
  const jsonBytes = encoder.encode(jsonText);
  const total = 12 + 8 + jsonBytes.length + 8 + binLength;
  const buffer = new ArrayBuffer(total);
  const view = new DataView(buffer);
  const bytes = new Uint8Array(buffer);
  let o = 0;
  const u32 = (v: number) => {
    view.setUint32(o, v, true);
    o += 4;
  };
  u32(0x46546c67); // glTF
  u32(2);
  u32(total);
  u32(jsonBytes.length);
  u32(0x4e4f534a); // JSON
  bytes.set(jsonBytes, o);
  o += jsonBytes.length;
  u32(binLength);
  u32(0x004e4942); // BIN
  bytes.set(new Uint8Array(mesh.positions.buffer), o);
  o += posBytes;
  bytes.set(new Uint8Array(mesh.normals.buffer), o);
  o += nrmBytes;
  bytes.set(new Uint8Array(mesh.indices.buffer), o);
  o += idxBytes;
  for (let i = 0; i < idxPad; i += 1) bytes[o + i] = 0;
  return bytes;
}
