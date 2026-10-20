/**
 * 构件库种子数据（41 个铁路工程构件）
 * 所有几何 / 体积 / 面数 / 缓存键均按默认参数真实计算，非静态文案。
 */
import type {
  ChangelogEntry,
  ComponentDetail,
  ComponentStatus,
  GateResult,
  GeometryInfo,
  ParamSchema,
  Preset,
  PortDef,
  QaReport,
  RecipeInfo,
  ShapeDef,
  SourceMode,
  Visibility,
} from '../shared/types';
import {
  buildCacheKeyPayload,
  computeCacheKey,
  sha256Hex,
  SPEC_PIER_CACHE_PAYLOAD,
} from '../shared/deterministic';
import { buildMesh, type GeometryKind, type MeshData } from './geometry';
import { generatorDigest, generatorEntry } from './generator';
import { CROSS_SCHEME_MAP } from './taxonomy';
import { readFileSync } from 'node:fs';

export interface InternalComponent extends ComponentDetail {
  mesh: MeshData;
  /** 构件库2 导入网格：几何来自 meshFile，禁止按参数重建 */
  importedMesh?: boolean;
  recipeInputs: string[];
  presetCacheKeys: Record<string, string>;
  declaredCacheKey: string;
  geoKind: GeometryKind;
  legacyUnaligned?: boolean;
}

const TENANT_ID = 'tenant-railway-17';
const SOURCE_VERSION = '1.4.2';

type PresetSpec = {
  modelNumber: string;
  label: string;
  overrides?: Record<string, number | string>;
};

interface ComponentSeed {
  familyId: string;
  name: string;
  drawingCode: string;
  category: string;
  crbim: string;
  semanticCode: string;
  status: ComponentStatus;
  visibility: Visibility;
  sourceMode: SourceMode;
  engine: string;
  kind: GeometryKind;
  dims: Record<string, number>;
  tags: string[];
  description: string;
  presets: PresetSpec[];
  publishedAt?: string;
  updatedAt: string;
  strengthGrade?: string;
  modeledBy?: string;
  /** 附加非映射形状，用于演示 unmapped 预警 */
  extraShapes?: Record<string, ShapeDef>;
  /** 原始 CadQuery 参数化脚本路径（来源追溯，不参与 cacheKey） */
  cqSource?: string;
  /** 构件库2 导入网格（相对 server/imports/cbfc2） */
  meshFile?: string;
}

function loadImportedMesh(file: string): MeshData {
  const raw = JSON.parse(
    readFileSync(new URL(`./imports/cbfc2/${file}`, import.meta.url), 'utf8'),
  ) as {
    positions: number[];
    indices: number[];
    edges: number[];
    edgeOffsets: number[];
    edgeShapes: number[];
    faceIds: number[];
    faceGroups: MeshData['faceGroups'];
    edgeLines: MeshData['edgeLines'];
    bounds: MeshData['bounds'];
    volumeMm3: number;
    areaMm2: number;
    vertices: number;
    triangles: number;
    facesCount: number;
    edgesCount: number;
  };
  const positions = Float32Array.from(raw.positions);
  const indices = Uint32Array.from(raw.indices);
  const normals = new Float32Array(positions.length);
  for (let t = 0; t < indices.length; t += 3) {
    const ia = indices[t];
    const ib = indices[t + 1];
    const ic = indices[t + 2];
    const ax = positions[ia * 3];
    const ay = positions[ia * 3 + 1];
    const az = positions[ia * 3 + 2];
    const ux = positions[ib * 3] - ax;
    const uy = positions[ib * 3 + 1] - ay;
    const uz = positions[ib * 3 + 2] - az;
    const vx = positions[ic * 3] - ax;
    const vy = positions[ic * 3 + 1] - ay;
    const vz = positions[ic * 3 + 2] - az;
    let nx = uy * vz - uz * vy;
    let ny = uz * vx - ux * vz;
    let nz = ux * vy - uy * vx;
    const len = Math.hypot(nx, ny, nz) || 1;
    nx /= len;
    ny /= len;
    nz /= len;
    for (const idx of [ia, ib, ic]) {
      normals[idx * 3] += nx;
      normals[idx * 3 + 1] += ny;
      normals[idx * 3 + 2] += nz;
    }
  }
  for (let i = 0; i < normals.length; i += 3) {
    const len = Math.hypot(normals[i], normals[i + 1], normals[i + 2]) || 1;
    normals[i] /= len;
    normals[i + 1] /= len;
    normals[i + 2] /= len;
  }
  return {
    positions,
    indices,
    normals,
    edges: Float32Array.from(raw.edges),
    edgeOffsets: Uint32Array.from(raw.edgeOffsets),
    edgeShapes: raw.edgeShapes,
    faceIds: Uint16Array.from(raw.faceIds),
    faceGroups: raw.faceGroups,
    edgeLines: raw.edgeLines,
    bounds: raw.bounds,
    volumeMm3: raw.volumeMm3,
    areaMm2: raw.areaMm2,
    vertices: raw.vertices,
    triangles: raw.triangles,
    facesCount: raw.facesCount,
    edgesCount: raw.edgesCount,
  };
}

const SHAPE_LABELS: Record<GeometryKind, { label: string; codeTail: string; geomType: 'solid' | 'face' | 'wire' }[]> = {
  pier: [
    { label: '墩身实体', codeTail: 'BodySolid', geomType: 'solid' },
    { label: '墩顶支撑垫石安装面', codeTail: 'TopFace', geomType: 'face' },
    { label: '墩底截面轮廓边界线', codeTail: 'BottomContour', geomType: 'wire' },
  ],
  box: [
    { label: '主体实体', codeTail: 'BodySolid', geomType: 'solid' },
    { label: '顶面', codeTail: 'TopFace', geomType: 'face' },
    { label: '底面轮廓边界线', codeTail: 'BaseContour', geomType: 'wire' },
  ],
  cylinder: [
    { label: '柱体实体', codeTail: 'ShaftSolid', geomType: 'solid' },
    { label: '顶面', codeTail: 'TopFace', geomType: 'face' },
    { label: '底面轮廓边界线', codeTail: 'BaseContour', geomType: 'wire' },
  ],
  ibeam: [
    { label: '梁体实体', codeTail: 'GirderBody', geomType: 'solid' },
    { label: '顶板面', codeTail: 'TopFlangeFace', geomType: 'face' },
    { label: '底板面', codeTail: 'BottomFlangeFace', geomType: 'face' },
    { label: '腹板面', codeTail: 'WebFace', geomType: 'face' },
    { label: '梁端截面轮廓线', codeTail: 'EndContour', geomType: 'wire' },
  ],
  tube: [
    { label: '衬砌环实体', codeTail: 'LiningBody', geomType: 'solid' },
    { label: '内壁面', codeTail: 'InnerFace', geomType: 'face' },
    { label: '端环面', codeTail: 'EndRingFace', geomType: 'face' },
    { label: '衬砌轮廓线', codeTail: 'Contour', geomType: 'wire' },
  ],
  'hollow-box': [
    { label: '主体实体', codeTail: 'BodySolid', geomType: 'solid' },
    { label: '内腔壁面', codeTail: 'InnerVoidFace', geomType: 'face' },
    { label: '顶缘环面', codeTail: 'TopRimFace', geomType: 'face' },
    { label: '外轮廓线', codeTail: 'OuterContour', geomType: 'wire' },
  ],
  'lightning-foundation': [
    { label: '承台实体', codeTail: 'BaseSolid', geomType: 'solid' },
    { label: '中段实体', codeTail: 'MidSolid', geomType: 'solid' },
    { label: '立柱实体', codeTail: 'PostSolid', geomType: 'solid' },
    { label: '外轮廓线', codeTail: 'OuterContour', geomType: 'wire' },
  ],
};

function shapesFor(kind: GeometryKind, semanticCode: string): Record<string, ShapeDef> {
  const list = SHAPE_LABELS[kind];
  const shapes: Record<string, ShapeDef> = {};
  const keys: string[] = [];
  list.forEach((item, index) => {
    let key: string;
    if (kind === 'pier' && index === 0) key = 'PierBodySolid';
    else if (kind === 'pier' && index === 1) key = 'TopBearingFace';
    else if (kind === 'pier' && index === 2) key = 'PierContourWire/1';
    else key = index === list.length - 1 && item.geomType === 'wire' ? `${item.codeTail}Wire/1` : `${item.codeTail}`;
    keys.push(key);
    shapes[key] = {
      label: item.label,
      code: `${semanticCode}.${item.codeTail}`,
      geomType: item.geomType,
      ...(index > 0 && key !== 'PierContourWire/1' ? { parent: keys[0] } : {}),
    };
  });
  return shapes;
}

const DIMENSION_LABELS: Record<string, { label: string; unit: string; min: number; max: number; step: number }> = {
  TopWidth: { label: '墩顶顺桥向宽度', unit: 'mm', min: 1000, max: 10000, step: 10 },
  TopLength: { label: '墩顶横桥向长度', unit: 'mm', min: 2000, max: 20000, step: 10 },
  PierHeight: { label: '墩身总高度', unit: 'mm', min: 2000, max: 60000, step: 100 },
  Slope: { label: '侧面坡度 (n:1)', unit: '1', min: 10, max: 100, step: 0.5 },
  CrossSlope: { label: '墩顶横坡率', unit: '%', min: -10, max: 10, step: 0.1 },
  LengthX: { label: '构件长度', unit: 'mm', min: 500, max: 40000, step: 10 },
  WidthY: { label: '构件宽度', unit: 'mm', min: 200, max: 20000, step: 10 },
  HeightZ: { label: '构件高度', unit: 'mm', min: 100, max: 30000, step: 10 },
  Taper: { label: '收分比例', unit: '1', min: 0, max: 0.5, step: 0.01 },
  Diameter: { label: '桩径', unit: 'mm', min: 300, max: 3000, step: 10 },
  Depth: { label: '梁高', unit: 'mm', min: 500, max: 4000, step: 10 },
  FlangeWidth: { label: '翼缘宽度', unit: 'mm', min: 300, max: 3000, step: 10 },
  WebThickness: { label: '腹板厚度', unit: 'mm', min: 80, max: 800, step: 5 },
  FlangeThickness: { label: '翼缘厚度', unit: 'mm', min: 60, max: 600, step: 5 },
  OuterDiameter: { label: '外径', unit: 'mm', min: 2000, max: 16000, step: 50 },
  Thickness: { label: '壁厚', unit: 'mm', min: 100, max: 1200, step: 10 },
  WallThickness: { label: '壁厚', unit: 'mm', min: 50, max: 1200, step: 10 },
};

const CACHE_INPUTS: Record<GeometryKind, string[]> = {
  pier: ['TopWidth', 'TopLength', 'PierHeight', 'Slope', 'CrossSlope'],
  box: ['LengthX', 'WidthY', 'HeightZ', 'Taper'],
  cylinder: ['Diameter', 'HeightZ'],
  ibeam: ['LengthX', 'Depth', 'FlangeWidth', 'WebThickness', 'FlangeThickness'],
  tube: ['OuterDiameter', 'Thickness', 'LengthX'],
  'hollow-box': ['LengthX', 'WidthY', 'HeightZ', 'WallThickness'],
  'lightning-foundation': [
    'BaseWidth',
    'BaseHeight',
    'MidWidth',
    'MidHeight',
    'PostSize',
    'PostHeight',
    'PostOffset',
  ],
};

function schemaFor(seed: ComponentSeed, volumeM3: number, modelNumbers: string[]): ParamSchema[] {
  const schema: ParamSchema[] = [
    {
      name: 'ModelNumber',
      label: '标准型号',
      type: 'enum',
      default: modelNumbers[1] ?? modelNumbers[0] ?? 'CUSTOM',
      options: [...modelNumbers, 'CUSTOM'],
      description: 'Preset 优先：选择标准型号直接加载预烘焙缓存',
    },
  ];
  for (const name of CACHE_INPUTS[seed.kind]) {
    const meta = DIMENSION_LABELS[name] ?? { label: name, unit: 'mm', min: 0, max: 100000, step: 1 };
    schema.push({
      name,
      label: meta.label,
      type: 'real',
      default: seed.dims[name] ?? 0,
      unit: meta.unit,
      min: meta.min,
      max: meta.max,
      step: meta.step,
      access: 'inout',
    });
  }
  schema.push({
    name: 'NetVolume',
    label: '实体净体积',
    type: 'real',
    default: volumeM3,
    unit: 'm3',
    access: 'out',
    description: '散度定理复核后的精确算量',
  });
  schema.push({
    name: 'StrengthGrade',
    label: '混凝土强度等级',
    type: 'enum',
    default: seed.strengthGrade ?? 'C40',
    options: ['C30', 'C35', 'C40', 'C50', 'C60'],
  });
  return schema;
}

function portsFor(kind: GeometryKind, dims: Record<string, number>, bounds: MeshData['bounds']): Record<string, PortDef> {
  const height = bounds.max[2] - bounds.min[2];
  if (kind === 'ibeam' || kind === 'tube') {
    return {
      StartCenter: {
        label: '起点连接控制点',
        origin: [bounds.min[0], 0, 0],
        axis: [1, 0, 0],
        refDirection: [0, 0, 1],
      },
      EndCenter: {
        label: '终点连接控制点',
        origin: [bounds.max[0], 0, 0],
        axis: [-1, 0, 0],
        refDirection: [0, 0, 1],
      },
    };
  }
  return {
    TopCenter: {
      label: '顶面中心接合点',
      origin: [0, 0, height],
      axis: [0, 0, 1],
      refDirection: [1, 0, 0],
    },
    BaseCenter: {
      label: '底面承台接合点',
      origin: [0, 0, 0],
      axis: [0, 0, -1],
      refDirection: [1, 0, 0],
    },
  };
}

const SEEDS: ComponentSeed[] = [
  {
    familyId: 'PierBody',
    name: '圆端型实体桥墩墩身',
    drawingCode: 'PierBody',
    category: '桥墩',
    crbim: '53-02-10',
    semanticCode: 'Bridge.Pier',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'pier',
    dims: { TopWidth: 2400, TopLength: 6400, PierHeight: 12000, Slope: 35, CrossSlope: 2 },
    tags: ['桥梁', '桥墩', '圆端形', '变参', '铁路标准'],
    description:
      '铁路简支梁桥圆端型实体桥墩，按坡度收分放样。支持墩高、墩顶尺寸与坡度在线变参，适用于桥梁下部结构设计与算量。',
    presets: [
      { modelNumber: 'STD-PIER-8M', label: '8m 标准墩', overrides: { PierHeight: 8000, TopWidth: 2200, TopLength: 6000 } },
      { modelNumber: 'STD-PIER-12M', label: '12m 标准墩', overrides: { PierHeight: 12000, TopWidth: 2400, TopLength: 6400 } },
      { modelNumber: 'STD-PIER-16M', label: '16m 标准墩', overrides: { PierHeight: 16000, TopWidth: 2600, TopLength: 7000 } },
    ],
    publishedAt: '2026-08-12',
    updatedAt: '2026-10-11',
    modeledBy: '张工',
    cqSource: 'cq_script/PierBody.py',
  },
  {
    familyId: 'PierBodyRect',
    name: '变截面矩形实体桥墩',
    drawingCode: 'BR-PIER-02',
    category: '桥墩',
    crbim: '53-02-10',
    semanticCode: 'Bridge.PierRect',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'box',
    dims: { LengthX: 3200, WidthY: 2000, HeightZ: 14000, Taper: 0.22 },
    tags: ['桥梁', '桥墩', '矩形', '变参'],
    description: '变截面矩形实体桥墩，四侧按 22% 收分，适用于山区铁路高墩。',
    presets: [
      { modelNumber: 'BR-PIER-R10', label: '矩形墩 10m', overrides: { HeightZ: 10000 } },
      { modelNumber: 'BR-PIER-R14', label: '矩形墩 14m', overrides: { HeightZ: 14000 } },
      { modelNumber: 'BR-PIER-R20', label: '矩形墩 20m', overrides: { HeightZ: 20000, Taper: 0.28 } },
    ],
    publishedAt: '2026-07-28',
    updatedAt: '2026-10-08',
    modeledBy: '李工',
  },
  {
    familyId: 'PierBodyHollow',
    name: '双柱式空心桥墩',
    drawingCode: 'BR-PIER-03',
    category: '桥墩',
    crbim: '53-02-10',
    semanticCode: 'Bridge.PierHollow',
    status: 'published',
    visibility: 'confidential',
    sourceMode: 'parametric',
    engine: 'occt-native',
    kind: 'hollow-box',
    dims: { LengthX: 5200, WidthY: 2400, HeightZ: 18000, WallThickness: 500 },
    tags: ['桥梁', '桥墩', '空心墩', '高墩'],
    description: '双柱式空心桥墩，空腔壁厚 500mm，适用于 18m 以上高墩，含内腔全封闭检测数据。',
    presets: [
      { modelNumber: 'BR-HLW-15', label: '空心墩 15m', overrides: { HeightZ: 15000 } },
      { modelNumber: 'BR-HLW-18', label: '空心墩 18m', overrides: { HeightZ: 18000 } },
      { modelNumber: 'BR-HLW-24', label: '空心墩 24m', overrides: { HeightZ: 24000, WallThickness: 600 } },
    ],
    publishedAt: '2026-09-02',
    updatedAt: '2026-10-12',
    modeledBy: '王工',
  },
  {
    familyId: 'PierCap',
    name: '桥墩盖梁顶帽',
    drawingCode: 'BR-CAP-01',
    category: '桥墩',
    crbim: '53-02-20',
    semanticCode: 'Bridge.PierCap',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'box',
    dims: { LengthX: 9000, WidthY: 2200, HeightZ: 1800, Taper: 0 },
    tags: ['桥梁', '盖梁', '顶帽'],
    description: '桥墩盖梁顶帽，顶部布置支座垫石预留面，支持长度与高度变参。',
    presets: [
      { modelNumber: 'BR-CAP-9', label: '盖梁 9m', overrides: {} },
      { modelNumber: 'BR-CAP-12', label: '盖梁 12m', overrides: { LengthX: 12000, HeightZ: 2200 } },
    ],
    publishedAt: '2026-08-03',
    updatedAt: '2026-10-02',
    modeledBy: '张工',
  },
  {
    familyId: 'PierTray',
    name: '桥墩托盘',
    drawingCode: 'BR-TRAY-01',
    category: '桥墩',
    crbim: '53-02-20',
    semanticCode: 'Bridge.PierTray',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'box',
    dims: { LengthX: 4200, WidthY: 2000, HeightZ: 1800, Taper: 0.16 },
    tags: ['桥梁', '托盘', '过渡段'],
    description: '连接墩身与顶帽的变截面托盘，16% 收分过渡，保证应力平顺传递。',
    presets: [
      { modelNumber: 'BR-TRAY-42', label: '托盘 4200', overrides: {} },
      { modelNumber: 'BR-TRAY-52', label: '托盘 5200', overrides: { LengthX: 5200, HeightZ: 2000 } },
      { modelNumber: 'CQ-TRAY-275', label: 'CQ 托盘 H=2750', overrides: { LengthX: 4200, WidthY: 2000, HeightZ: 2750 } },
    ],
    publishedAt: '2026-08-03',
    updatedAt: '2026-09-27',
    modeledBy: '李工',
    cqSource: 'cq_script/PierTray.py',
  },
  {
    familyId: 'BearingPad',
    name: '支座垫石',
    drawingCode: 'BR-PAD-01',
    category: '支座系统',
    crbim: '53-02-40',
    semanticCode: 'Bridge.BearingPad',
    status: 'published',
    visibility: 'public',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 1200, WidthY: 900, HeightZ: 300, Taper: 0 },
    tags: ['支座', '垫石', '标准件'],
    description: '墩顶支座垫石标准件，固定 B-Rep 几何，精确算量，适用于全部简支梁桥。',
    presets: [{ modelNumber: 'BR-PAD-1209', label: '1200×900×300', overrides: {} }],
    publishedAt: '2026-06-18',
    updatedAt: '2026-09-15',
    modeledBy: '赵工',
  },
  {
    familyId: 'PileCap',
    name: '钢筋混凝土承台',
    drawingCode: 'BR-PILECAP-01',
    category: '基础',
    crbim: '53-01-30',
    semanticCode: 'Bridge.PileCap',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'box',
    dims: { LengthX: 8000, WidthY: 6000, HeightZ: 2500, Taper: 0.05 },
    tags: ['基础', '承台', '桩基'],
    description: '钻孔桩群桩基础承台，5% 收分，支持平面尺寸与承台高度在线变参。',
    presets: [
      { modelNumber: 'BR-PC-86', label: '承台 8×6m', overrides: {} },
      { modelNumber: 'BR-PC-107', label: '承台 10×7m', overrides: { LengthX: 10000, WidthY: 7000, HeightZ: 3000 } },
      { modelNumber: 'CQ-PC-75', label: 'CQ 承台 7×5m', overrides: { LengthX: 7000, WidthY: 5000, HeightZ: 2000 } },
    ],
    publishedAt: '2026-07-03',
    updatedAt: '2026-10-06',
    modeledBy: '王工',
    cqSource: 'cq_script/PileCap.py',
  },
  {
    familyId: 'BoredPile',
    name: '钻孔灌注桩',
    drawingCode: 'BR-PILE-01',
    category: '基础',
    crbim: '53-01-20',
    semanticCode: 'Bridge.BoredPile',
    status: 'reviewing',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'cylinder',
    dims: { Diameter: 1500, HeightZ: 24000 },
    tags: ['基础', '桩基', '钻孔桩'],
    description: '铁路桥梁钻孔灌注桩，v1.5 送审版本新增桩底扩孔参数与混凝土方量自动校核。',
    presets: [
      { modelNumber: 'BR-PILE-150', label: 'φ1500×24m', overrides: {} },
      { modelNumber: 'BR-PILE-180', label: 'φ1800×30m', overrides: { Diameter: 1800, HeightZ: 30000 } },
      { modelNumber: 'CQ-PILE-100', label: 'CQ φ1000×15m', overrides: { Diameter: 1000, HeightZ: 15000 } },
    ],
    updatedAt: '2026-10-17',
    modeledBy: '王工',
    cqSource: 'cq_script/Pile.py',
  },
  {
    familyId: 'Abutment',
    name: '重力式桥台台身',
    drawingCode: 'BR-ABUT-01',
    category: '桥台',
    crbim: '53-02-30',
    semanticCode: 'Bridge.Abutment',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 7000, WidthY: 3500, HeightZ: 8500, Taper: 0.12 },
    tags: ['桥台', '重力式', '标准件'],
    description: '重力式桥台台身，固定 B-Rep 设计模型，含台顶与台底命名接合面。',
    presets: [{ modelNumber: 'BR-ABUT-G7', label: 'G7 桥台', overrides: {} }],
    publishedAt: '2026-05-21',
    updatedAt: '2026-09-20',
    modeledBy: '李工',
  },
  {
    familyId: 'RubberBearing',
    name: '板式橡胶支座',
    drawingCode: 'BR-BRG-RB',
    category: '支座系统',
    crbim: '53-02-40',
    semanticCode: 'Bridge.RubberBearing',
    status: 'archived',
    visibility: 'public',
    sourceMode: 'imported-ifc',
    engine: 'ifc-import',
    kind: 'box',
    dims: { LengthX: 500, WidthY: 450, HeightZ: 120, Taper: 0 },
    tags: ['支座', '橡胶支座', '导入', '已归档'],
    description: '板式橡胶支座 IFC 导入模型，已归档，由 GBZY 系列盆式支座替代。',
    presets: [{ modelNumber: 'RB-500', label: '500×450×120', overrides: {} }],
    publishedAt: '2025-11-08',
    updatedAt: '2026-08-30',
    modeledBy: '赵工',
  },
  {
    familyId: 'BoxGirder',
    name: '预应力混凝土箱梁',
    drawingCode: 'BR-GIRDER-BOX',
    category: '上部结构',
    crbim: '53-03-10',
    semanticCode: 'Bridge.BoxGirder',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'hollow-box',
    dims: { LengthX: 30000, WidthY: 12000, HeightZ: 2200, WallThickness: 320 },
    tags: ['上部结构', '箱梁', '预应力', '变参'],
    description: '30m 跨预应力混凝土箱梁，单箱双室截面，支持梁高与壁厚在线变参。',
    presets: [
      { modelNumber: 'BR-BOX-30', label: '30m 箱梁', overrides: {} },
      { modelNumber: 'BR-BOX-32', label: '32m 箱梁', overrides: { LengthX: 32000, HeightZ: 2400 } },
      { modelNumber: 'CQ-BOX-315', label: 'CQ 31.5m 单箱单室', overrides: { LengthX: 31500, HeightZ: 2300, WallThickness: 320 } },
    ],
    publishedAt: '2026-08-22',
    updatedAt: '2026-10-10',
    modeledBy: '张工',
    cqSource: 'cq_script/Girder.py',
  },
  {
    familyId: 'TGirder',
    name: '预制 T 梁',
    drawingCode: 'BR-GIRDER-T',
    category: '上部结构',
    crbim: '53-03-10',
    semanticCode: 'Bridge.TGirder',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'ibeam',
    dims: { LengthX: 30000, Depth: 2000, FlangeWidth: 1600, WebThickness: 240, FlangeThickness: 200 },
    tags: ['上部结构', 'T梁', '预制梁'],
    description: '30m 预制预应力 T 梁，标准工字形截面，支持梁高与翼缘尺寸变参。',
    presets: [
      { modelNumber: 'BR-T-30', label: '30m T 梁', overrides: {} },
      { modelNumber: 'BR-T-24', label: '24m T 梁', overrides: { LengthX: 24000, Depth: 1800 } },
    ],
    publishedAt: '2026-08-22',
    updatedAt: '2026-10-09',
    modeledBy: '李工',
  },
  {
    familyId: 'PotBearing',
    name: '盆式橡胶支座',
    drawingCode: 'BR-BRG-GBZY',
    category: '支座系统',
    crbim: '53-02-40',
    semanticCode: 'Bridge.PotBearing',
    status: 'published',
    visibility: 'public',
    sourceMode: 'imported-ifc',
    engine: 'ifc-import',
    kind: 'cylinder',
    dims: { Diameter: 600, HeightZ: 220 },
    tags: ['支座', '盆式支座', '国标', 'IFC'],
    description: '国标 GBZY 盆式橡胶支座 IFC 导入模型，含支座顶板与底板连接面。',
    presets: [{ modelNumber: 'GBZY-600', label: 'GBZY 600', overrides: {} }],
    publishedAt: '2026-04-11',
    updatedAt: '2026-09-12',
    modeledBy: '赵工',
  },
  {
    familyId: 'DeckSlab',
    name: '预制桥面板',
    drawingCode: 'BR-SLAB-01',
    category: '上部结构',
    crbim: '53-03-20',
    semanticCode: 'Bridge.DeckSlab',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'box',
    dims: { LengthX: 12000, WidthY: 6000, HeightZ: 300, Taper: 0 },
    tags: ['上部结构', '桥面板', '预制'],
    description: '钢筋混凝土预制桥面板，支持板长与板厚变参，含板顶找平层基准面。',
    presets: [
      { modelNumber: 'BR-SLAB-12', label: '12m 板', overrides: {} },
      { modelNumber: 'BR-SLAB-16', label: '16m 板', overrides: { LengthX: 16000, HeightZ: 350 } },
    ],
    publishedAt: '2026-07-19',
    updatedAt: '2026-10-01',
    modeledBy: '张工',
  },
  {
    familyId: 'ExpansionJoint',
    name: '模数式伸缩缝',
    drawingCode: 'BR-JOINT-M1',
    category: '桥面系',
    crbim: '53-04-10',
    semanticCode: 'Bridge.ExpansionJoint',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 12000, WidthY: 600, HeightZ: 180, Taper: 0 },
    tags: ['桥面系', '伸缩缝', '标准件'],
    description: '模数式桥梁伸缩缝标准单元，固定 B-Rep 几何，精确算量。',
    presets: [{ modelNumber: 'BR-JOINT-80', label: '80 型', overrides: {} }],
    publishedAt: '2026-06-05',
    updatedAt: '2026-09-18',
    modeledBy: '赵工',
  },
  {
    familyId: 'Guardrail',
    name: '混凝土防撞护栏',
    drawingCode: 'BR-RAIL-01',
    category: '桥面系',
    crbim: '53-04-10',
    semanticCode: 'Bridge.Guardrail',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'mesh-only',
    engine: 'mesh-import',
    kind: 'box',
    dims: { LengthX: 12000, WidthY: 500, HeightZ: 1100, Taper: 0.08 },
    tags: ['桥面系', '护栏', '网格体'],
    description: '混凝土防撞护栏轻量化网格体，仅用于可视化预览与布置，不提供精确算量。',
    presets: [{ modelNumber: 'BR-RAIL-12', label: '12m 护栏', overrides: {} }],
    publishedAt: '2026-07-01',
    updatedAt: '2026-09-22',
    modeledBy: '李工',
  },
  {
    familyId: 'Diaphragm',
    name: '横隔板',
    drawingCode: 'BR-DIAPH-01',
    category: '上部结构',
    crbim: '53-03-20',
    semanticCode: 'Bridge.Diaphragm',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'box',
    dims: { LengthX: 6000, WidthY: 300, HeightZ: 1800, Taper: 0.05 },
    tags: ['上部结构', '横隔板', '变参'],
    description: '箱梁端横隔板，厚度与高度可变，保证箱梁端部抗扭刚度。',
    presets: [
      { modelNumber: 'BR-DIAPH-30', label: '30m 梁横隔板', overrides: {} },
      { modelNumber: 'BR-DIAPH-24', label: '24m 梁横隔板', overrides: { LengthX: 5000, HeightZ: 1600 } },
    ],
    publishedAt: '2026-08-28',
    updatedAt: '2026-10-04',
    modeledBy: '王工',
  },
  {
    familyId: 'RetainingWall',
    name: '重力式挡土墙',
    drawingCode: 'RW-GRAV-01',
    category: '路基防护',
    crbim: '53-07-10',
    semanticCode: 'Roadbed.RetainingWall',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'box',
    dims: { LengthX: 3000, WidthY: 1200, HeightZ: 8000, Taper: 0.2 },
    tags: ['路基', '挡土墙', '重力式'],
    description: '铁路路基重力式挡土墙标准段，墙面坡度与墙高可在线调整。',
    presets: [
      { modelNumber: 'RW-GRAV-6', label: '6m 墙高', overrides: { HeightZ: 6000 } },
      { modelNumber: 'RW-GRAV-8', label: '8m 墙高', overrides: { HeightZ: 8000 } },
    ],
    publishedAt: '2026-06-29',
    updatedAt: '2026-09-30',
    modeledBy: '李工',
  },
  {
    familyId: 'SlopeProtection',
    name: '路基边坡防护',
    drawingCode: 'RW-SLOPE-01',
    category: '路基防护',
    crbim: '53-07-20',
    semanticCode: 'Roadbed.SlopeProtection',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'build123d',
    kind: 'box',
    dims: { LengthX: 6000, WidthY: 3000, HeightZ: 200, Taper: 0 },
    tags: ['路基', '边坡', '防护'],
    description: '路基边坡拱形骨架防护单元（草稿），等待标准委员会审核后发布。',
    presets: [{ modelNumber: 'RW-SLOPE-6', label: '6m 防护单元', overrides: {} }],
    updatedAt: '2026-10-16',
    modeledBy: '王工',
  },
  {
    familyId: 'CulvertBox',
    name: '钢筋混凝土箱涵',
    drawingCode: 'CV-BOX-01',
    category: '涵洞',
    crbim: '53-05-10',
    semanticCode: 'Bridge.Culvert',
    status: 'published',
    visibility: 'public',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'hollow-box',
    dims: { LengthX: 6000, WidthY: 4000, HeightZ: 3200, WallThickness: 400 },
    tags: ['涵洞', '箱涵', '标准件'],
    description: '铁路路基钢筋混凝土箱涵标准节，含内腔全封闭几何与八字翼墙连接面。',
    presets: [{ modelNumber: 'CV-BOX-64', label: '6×4m 箱涵', overrides: {} }],
    publishedAt: '2026-05-30',
    updatedAt: '2026-09-08',
    modeledBy: '赵工',
  },
  {
    familyId: 'TunnelLining',
    name: '隧道二次衬砌环',
    drawingCode: 'TN-LINING-01',
    category: '隧道',
    crbim: '53-06-10',
    semanticCode: 'Tunnel.SecondaryLining',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'imported-ifc',
    engine: 'ifc-import',
    kind: 'tube',
    dims: { OuterDiameter: 9600, Thickness: 500, LengthX: 12000 },
    tags: ['隧道', '衬砌', 'IFC'],
    description: '双线铁路隧道二次衬砌环，IFC 4.3 导入，包含内轮廓与端环命名面。',
    presets: [{ modelNumber: 'TN-LIN-96', label: 'φ9.6m 衬砌', overrides: {} }],
    publishedAt: '2026-03-14',
    updatedAt: '2026-09-05',
    modeledBy: '王工',
  },
  {
    familyId: 'DrainageGrate',
    name: '桥面泄水槽',
    drawingCode: 'DR-GRATE-01',
    category: '桥面系',
    crbim: '53-04-20',
    semanticCode: 'Bridge.DrainageGrate',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'mesh-only',
    engine: 'mesh-import',
    kind: 'box',
    dims: { LengthX: 800, WidthY: 500, HeightZ: 200, Taper: 0 },
    tags: ['桥面系', '排水', '网格体'],
    description: '桥面泄水槽轻量化网格体（草稿），语义码尚未纳入企业标准映射表。',
    presets: [{ modelNumber: 'DR-GRATE-01', label: '标准泄水槽', overrides: {} }],
    updatedAt: '2026-10-18',
    modeledBy: '李工',
    extraShapes: {
      DrainageInlet: {
        label: '泄水口细部（待映射）',
        code: 'Bridge.Accessory.DrainageInlet',
        geomType: 'face',
        parent: 'BodySolid',
      },
    },
  },
  {
    familyId: 'CableTray',
    name: '桥面电缆槽',
    drawingCode: 'DR-TRAY-01',
    category: '桥面系',
    crbim: '53-04-20',
    semanticCode: 'Bridge.CableTray',
    status: 'published',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'hollow-box',
    dims: { LengthX: 4000, WidthY: 600, HeightZ: 500, WallThickness: 60 },
    tags: ['桥面系', '电缆槽', '标准件'],
    description: '桥面电缆槽标准节，薄壁空心截面，固定 B-Rep 几何。',
    presets: [{ modelNumber: 'DR-TRAY-4', label: '4m 电缆槽', overrides: {} }],
    publishedAt: '2026-06-15',
    updatedAt: '2026-09-25',
    modeledBy: '赵工',
  },
  {
    familyId: 'SignGantry',
    name: '标志门架',
    drawingCode: 'SG-PORTAL-01',
    category: '附属钢结构',
    crbim: '53-04-30',
    semanticCode: 'Bridge.SignGantry',
    status: 'reviewing',
    visibility: 'confidential',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 12000, WidthY: 500, HeightZ: 6500, Taper: 0 },
    tags: ['附属', '钢结构', '门架'],
    description: '铁路桥下标志门架 v1.3.0 送审包，新增检修爬梯与监测支架部位，等待标准专家复核。',
    presets: [{ modelNumber: 'SG-PORTAL-12', label: '12m 门架', overrides: {} }],
    updatedAt: '2026-10-18',
    modeledBy: '李工',
    extraShapes: {
      MonitorBracket: {
        label: '监测设备支架（未映射）',
        code: 'Bridge.Accessory.MonitorBracket',
        geomType: 'face',
        parent: 'BodySolid',
      },
    },
  },
  {
    familyId: 'CompositePier',
    name: '桥墩组合体（CQ 脚本）',
    drawingCode: 'CQ-PIER-COMP-01',
    category: '桥墩',
    crbim: '53-02-10',
    semanticCode: 'Bridge.CompositePier',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'pier',
    dims: { TopWidth: 3200, TopLength: 9600, PierHeight: 12000, Slope: 40, CrossSlope: 2 },
    tags: ['桥墩', '组合结构', '承台', '桩基', 'CQ 脚本'],
    description:
      '由 cq_script/bridge_pier_cq2.py 入库的桥墩组合体：托盘、顶帽、墩身、两层承台与六根桩基。Mock 几何以墩身为主要可参数化实体，保留组合构件语义与 CQ 脚本来源。',
    presets: [
      { modelNumber: 'CQ-COMP-12M', label: 'CQ 组合墩 12m', overrides: {} },
      { modelNumber: 'CQ-COMP-16M', label: 'CQ 组合墩 16m', overrides: { PierHeight: 16000 } },
    ],
    updatedAt: '2026-10-19',
    modeledBy: 'CQ 脚本',
    cqSource: 'cq_script/bridge_pier_cq2.py',
  },
  {
    familyId: 'BedStone',
    name: '支承垫石（CQ 参数化）',
    drawingCode: 'CQ-BED-01',
    category: '支座系统',
    crbim: '53-02-40',
    semanticCode: 'Bridge.BedStone',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'box',
    dims: { LengthX: 1200, WidthY: 1200, HeightZ: 400, Taper: 0 },
    tags: ['支座', '垫石', 'CQ 脚本', '参数化'],
    description: '由 cq_script/BedStone.py 入库的支承垫石，支持 Width / Length / Height 三个输入参数；与固定 B-Rep 支座垫石并存。',
    presets: [
      { modelNumber: 'CQ-BED-1200', label: 'CQ 垫石 1200×1200×400', overrides: {} },
      { modelNumber: 'CQ-BED-1500', label: 'CQ 垫石 1500×1500×500', overrides: { LengthX: 1500, WidthY: 1500, HeightZ: 500 } },
    ],
    updatedAt: '2026-10-19',
    modeledBy: 'CQ 脚本',
    cqSource: 'cq_script/BedStone.py',
  },
  {
    familyId: 'LightningRodFoundation',
    name: '接闪杆基础（CQ 参数化）',
    drawingCode: 'CQ-LRF-01',
    category: '桥面系及附属',
    crbim: '53-04-30',
    semanticCode: 'Bridge.LightningRodFoundation',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'lightning-foundation',
    dims: {
      BaseWidth: 2700,
      BaseHeight: 1000,
      MidWidth: 2200,
      MidHeight: 800,
      PostSize: 500,
      PostHeight: 900,
      PostOffset: 647.5,
    },
    tags: ['附属', '接闪杆', '基础', 'CQ 脚本'],
    description: '由 cq_script/lightning_rod_foundation.py 入库的接闪杆基础，包含双层基座、中段与四根对称立柱；几何按脚本尺寸重建。',
    presets: [
      { modelNumber: 'CQ-LRF-27', label: 'CQ 接闪杆基础 2.7m', overrides: {} },
      {
        modelNumber: 'CQ-LRF-30',
        label: 'CQ 接闪杆基础 3.0m',
        overrides: {
          BaseWidth: 3000,
          BaseHeight: 1100,
          MidWidth: 2400,
          MidHeight: 900,
          PostSize: 550,
          PostHeight: 1000,
          PostOffset: 720,
        },
      },
    ],
    updatedAt: '2026-10-19',
    modeledBy: 'CQ 脚本',
    cqSource: 'cq_script/lightning_rod_foundation.py',
  },
  {
    familyId: 'FixedBearing',
    name: '固定支座（CQ 参数化）',
    drawingCode: 'CQ-BRG-FIX',
    category: '支座系统',
    crbim: '53-02-40',
    semanticCode: 'Bridge.FixedBearing',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'parametric',
    engine: 'cadquery',
    kind: 'box',
    dims: { LengthX: 650, WidthY: 650, HeightZ: 250, Taper: 0 },
    tags: ['支座', '固定支座', 'CQ 脚本', '参数化'],
    description: '由 cq_script/bearing.py 入库的固定支座：底钢板、圆柱支座芯与顶钢板。Mock 内核以外包体表达，保留尺寸与来源。',
    presets: [{ modelNumber: 'CQ-BRG-650', label: 'CQ 固定支座 650×650×250', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: 'CQ 脚本',
    cqSource: 'cq_script/bearing.py',
  },
  {
    familyId: 'CBF2-LRF-20',
    name: '20m 避雷针基础（构件库2）',
    drawingCode: 'CBF2-LRF-20',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.LightningRod20',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 2700, WidthY: 2700, HeightZ: 2700, Taper: 0 },
    tags: ['构件库2', '基础', '避雷针', 'GLB 导入'],
    description: '从构件库2 导入的 20m 避雷针基础，源文件为 GLB，已转换为构件库网格。',
    presets: [{ modelNumber: 'CBF2-LRF-20', label: '20m 避雷针基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-LRF-20.mesh.json',
  },
  {
    familyId: 'CBF2-LRF-30',
    name: '30m 避雷针基础（构件库2）',
    drawingCode: 'CBF2-LRF-30',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.LightningRod30',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 3500, WidthY: 3500, HeightZ: 3200, Taper: 0 },
    tags: ['构件库2', '基础', '避雷针', 'GLB 导入'],
    description: '从构件库2 导入的 30m 避雷针基础，源文件为 GLB，已转换为构件库网格。',
    presets: [{ modelNumber: 'CBF2-LRF-30', label: '30m 避雷针基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-LRF-30.mesh.json',
  },
  {
    familyId: 'CBF2-EQF-SINGLE',
    name: '单杆设备基础（构件库2）',
    drawingCode: 'CBF2-EQF-SINGLE',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.EquipmentSingle',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 2000, WidthY: 2000, HeightZ: 2800, Taper: 0 },
    tags: ['构件库2', '基础', '设备基础', 'GLB 导入'],
    description: '从构件库2 导入的单杆设备基础。',
    presets: [{ modelNumber: 'CBF2-EQF-SINGLE', label: '单杆设备基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-EQF-SINGLE.mesh.json',
  },
  {
    familyId: 'CBF2-EQF-TRACTION',
    name: '牵引变压器基础（构件库2）',
    drawingCode: 'CBF2-EQF-TRACTION',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.TractionTransformer',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 11440, WidthY: 8540, HeightZ: 2400, Taper: 0 },
    tags: ['构件库2', '基础', '变压器', 'GLB 导入'],
    description: '从构件库2 导入的牵引变压器基础。',
    presets: [{ modelNumber: 'CBF2-EQF-TRACTION', label: '牵引变压器基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-EQF-TRACTION.mesh.json',
  },
  {
    familyId: 'CBF2-EQF-BREAKER',
    name: '三极断路器基础（构件库2）',
    drawingCode: 'CBF2-EQF-BREAKER',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.ThreePoleBreaker',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 9600, WidthY: 1600, HeightZ: 2600, Taper: 0 },
    tags: ['构件库2', '基础', '断路器', 'GLB 导入'],
    description: '从构件库2 导入的三极断路器基础。',
    presets: [{ modelNumber: 'CBF2-EQF-BREAKER', label: '三极断路器基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-EQF-BREAKER.mesh.json',
  },
  {
    familyId: 'CBF2-EQF-DISCONNECT',
    name: '三极隔离开关基础（构件库2）',
    drawingCode: 'CBF2-EQF-DISCONNECT',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.ThreePoleDisconnector',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 9600, WidthY: 4400, HeightZ: 2600, Taper: 0 },
    tags: ['构件库2', '基础', '隔离开关', 'GLB 导入'],
    description: '从构件库2 导入的三极隔离开关基础。',
    presets: [{ modelNumber: 'CBF2-EQF-DISCONNECT', label: '三极隔离开关基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-EQF-DISCONNECT.mesh.json',
  },
  {
    familyId: 'CBF2-EQF-AUTO',
    name: '自耦变压器基础（构件库2）',
    drawingCode: 'CBF2-EQF-AUTO',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.AutoTransformer',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 6254, WidthY: 5340, HeightZ: 2401, Taper: 0 },
    tags: ['构件库2', '基础', '自耦变压器', 'GLB 导入'],
    description: '从构件库2 导入的自耦变压器基础。',
    presets: [{ modelNumber: 'CBF2-EQF-AUTO', label: '自耦变压器基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-EQF-AUTO.mesh.json',
  },
  {
    familyId: 'CBF2-GANTRY-TRANS',
    name: '变压器架构基础（构件库2）',
    drawingCode: 'CBF2-GANTRY-TRANS',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.TransformerGantry',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 3430, WidthY: 2510, HeightZ: 3300, Taper: 0 },
    tags: ['构件库2', '基础', '架构', 'GLB 导入'],
    description: '从构件库2 导入的变压器架构基础。',
    presets: [{ modelNumber: 'CBF2-GANTRY-TRANS', label: '变压器架构基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-GANTRY-TRANS.mesh.json',
  },
  {
    familyId: 'CBF2-GANTRY',
    name: '架构基础（构件库2）',
    drawingCode: 'CBF2-GANTRY',
    category: '基础',
    crbim: '53-01-10',
    semanticCode: 'Bridge.Foundation.Gantry',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 4420, WidthY: 2910, HeightZ: 4100, Taper: 0 },
    tags: ['构件库2', '基础', '架构', 'GLB 导入'],
    description: '从构件库2 导入的架构基础。',
    presets: [{ modelNumber: 'CBF2-GANTRY', label: '架构基础', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-GANTRY.mesh.json',
  },
  {
    familyId: 'CBF2-SIGN-SECTION',
    name: '区间标志牌（构件库2）',
    drawingCode: 'CBF2-SIGN-SECTION',
    category: '桥面系及附属',
    crbim: '53-04-30',
    semanticCode: 'Bridge.Sign.Section',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 500, WidthY: 500, HeightZ: 2800, Taper: 0 },
    tags: ['构件库2', '附属', '标志牌', 'GLB 导入'],
    description: '从构件库2 导入的区间标志牌。',
    presets: [{ modelNumber: 'CBF2-SIGN-SECTION', label: '区间标志牌', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-SIGN-SECTION.mesh.json',
  },
  {
    familyId: 'CBF2-SIGN-ADV1',
    name: '预告标 1（构件库2）',
    drawingCode: 'CBF2-SIGN-ADV1',
    category: '桥面系及附属',
    crbim: '53-04-30',
    semanticCode: 'Bridge.Sign.Advance1',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 500, WidthY: 500, HeightZ: 1800, Taper: 0 },
    tags: ['构件库2', '附属', '预告标', 'GLB 导入'],
    description: '从构件库2 导入的预告标 1。',
    presets: [{ modelNumber: 'CBF2-SIGN-ADV1', label: '预告标 1', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-SIGN-ADV1.mesh.json',
  },
  {
    familyId: 'CBF2-SIGN-ADV2',
    name: '预告标 2（构件库2）',
    drawingCode: 'CBF2-SIGN-ADV2',
    category: '桥面系及附属',
    crbim: '53-04-30',
    semanticCode: 'Bridge.Sign.Advance2',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 500, WidthY: 500, HeightZ: 1800, Taper: 0 },
    tags: ['构件库2', '附属', '预告标', 'GLB 导入'],
    description: '从构件库2 导入的预告标 2。',
    presets: [{ modelNumber: 'CBF2-SIGN-ADV2', label: '预告标 2', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-SIGN-ADV2.mesh.json',
  },
  {
    familyId: 'CBF2-SIGN-ADV3',
    name: '预告标 3（构件库2）',
    drawingCode: 'CBF2-SIGN-ADV3',
    category: '桥面系及附属',
    crbim: '53-04-30',
    semanticCode: 'Bridge.Sign.Advance3',
    status: 'draft',
    visibility: 'tenant',
    sourceMode: 'native-brep',
    engine: 'occt-native',
    kind: 'box',
    dims: { LengthX: 500, WidthY: 500, HeightZ: 1800, Taper: 0 },
    tags: ['构件库2', '附属', '预告标', 'GLB 导入'],
    description: '从构件库2 导入的预告标 3。',
    presets: [{ modelNumber: 'CBF2-SIGN-ADV3', label: '预告标 3', overrides: {} }],
    updatedAt: '2026-10-19',
    modeledBy: '构件库2',
    meshFile: 'CBF2-SIGN-ADV3.mesh.json',
  },
];

function stripLeadingZero(n: number): string {
  return String(n);
}

function formatDateOffset(base: string, days: number): string {
  const d = new Date(`${base}T00:00:00Z`);
  d.setUTCDate(d.getUTCDate() + days);
  return d.toISOString().slice(0, 10);
}

function hashSeed(text: string): number {
  let h = 2166136261;
  for (let i = 0; i < text.length; i += 1) {
    h ^= text.charCodeAt(i);
    h = Math.imul(h, 16777619);
  }
  return Math.abs(h);
}

function numericParams(params: Record<string, number | string | boolean>, inputs: string[]) {
  const out: Record<string, number> = {};
  for (const key of inputs) {
    const value = params[key];
    if (typeof value === 'number') out[key] = value;
  }
  return out;
}

export async function createComponents(): Promise<InternalComponent[]> {
  const result: InternalComponent[] = [];

  for (const seed of SEEDS) {
    const inputs = CACHE_INPUTS[seed.kind];
    const mesh = seed.meshFile ? loadImportedMesh(seed.meshFile) : buildMesh(seed.kind, seed.dims);
    const volumeM3 = Math.round((mesh.volumeMm3 / 1e9) * 1000) / 1000;
    const bounds = mesh.bounds;
    const digest = await generatorDigest({
      familyId: seed.familyId,
      name: seed.name,
      inputs,
    });
    const source = 'generators/generator.py';
    const recipe: RecipeInfo | undefined =
      seed.sourceMode === 'parametric'
        ? {
            engine: seed.engine,
            source,
            entry: generatorEntry(seed.familyId),
            version: SOURCE_VERSION,
            digest,
            inputs,
          }
        : undefined;

    const presetSpecs = seed.presets;
    const presetParams: Record<string, Record<string, number | string | boolean>> = {};
    const presetCacheKeys: Record<string, string> = {};
    for (const spec of presetSpecs) {
      const params: Record<string, number | string | boolean> = {
        ModelNumber: spec.modelNumber,
        ...seed.dims,
        ...(spec.overrides ?? {}),
        StrengthGrade: seed.strengthGrade ?? 'C40',
      };
      const presetMesh = seed.meshFile ? mesh : buildMesh(seed.kind, numericParams(params, inputs));
      const presetVolume = Math.round((presetMesh.volumeMm3 / 1e9) * 1000) / 1000;
      params.NetVolume = presetVolume;
      presetParams[spec.modelNumber] = params;
      if (seed.familyId === 'PierBody' && spec.modelNumber === 'STD-PIER-12M') {
        presetCacheKeys[spec.modelNumber] = await computeCacheKey(SPEC_PIER_CACHE_PAYLOAD);
      } else if (recipe) {
        presetCacheKeys[spec.modelNumber] = await computeCacheKey(
          buildCacheKeyPayload({
            engine: recipe.engine,
            digest: recipe.digest,
            inputParams: numericParams(params, inputs),
            source: recipe.source,
            version: recipe.version,
          }),
        );
      } else {
        presetCacheKeys[spec.modelNumber] = await sha256Hex(
          `cbfc-artifact:${seed.familyId}:${spec.modelNumber}:${presetVolume}`,
        );
      }
    }

    const defaultPreset = presetSpecs[1] ?? presetSpecs[0];
    const defaultParams: Record<string, number | string | boolean> = {
      ModelNumber: defaultPreset?.modelNumber ?? 'CUSTOM',
      ...seed.dims,
      ...(defaultPreset?.overrides ?? {}),
      StrengthGrade: seed.strengthGrade ?? 'C40',
    };
    const defaultMesh = seed.meshFile ? mesh : buildMesh(seed.kind, numericParams(defaultParams, inputs));
    const defaultVolume = Math.round((defaultMesh.volumeMm3 / 1e9) * 1000) / 1000;
    defaultParams.NetVolume = defaultVolume;
    const declaredCacheKey =
      presetCacheKeys[String(defaultParams.ModelNumber)] ??
      (seed.familyId === 'PierBody'
        ? await computeCacheKey(SPEC_PIER_CACHE_PAYLOAD)
        : await computeCacheKey(
            buildCacheKeyPayload({
              engine: seed.engine,
              digest,
              inputParams: numericParams(defaultParams, inputs),
              source,
              version: SOURCE_VERSION,
            }),
          ));

    const shapes: Record<string, ShapeDef> = {
      ...shapesFor(seed.kind, seed.semanticCode),
      ...(seed.extraShapes ?? {}),
    };
    const shapeKeys = Object.keys(shapes);
    const faceToShape = defaultMesh.faceGroups.map((g) => Math.min(g.shapeIndex, shapeKeys.length - 1));
    // 附加未映射形状由 GATE/语义沙盒演示，不参与 faceToShape 索引越界

    const presets: Preset[] = presetSpecs.map((spec) => ({
      modelNumber: spec.modelNumber,
      label: spec.label,
      drawingCode: `${seed.drawingCode}-${spec.modelNumber.split('-').pop()}`,
      params: presetParams[spec.modelNumber],
      isPrebaked: true,
      cacheKey: presetCacheKeys[spec.modelNumber],
    }));
    const modelNumbers = presets.map((p) => p.modelNumber);
    const schema = schemaFor(seed, volumeM3, modelNumbers);
    schema[0].default = defaultPreset?.modelNumber ?? modelNumbers[0];

    const boundsMin = bounds.min.map((v) => Math.round(v * 10) / 10) as [number, number, number];
    const boundsMax = bounds.max.map((v) => Math.round(v * 10) / 10) as [number, number, number];
    const geometry: GeometryInfo = {
      exactBrep: seed.sourceMode !== 'mesh-only',
      defaultFormat: 'cbf-binxcaf',
      defaultFile: 'geometry/default.cbf',
      defaultCacheKey: declaredCacheKey,
      bounds: { min: boundsMin, max: boundsMax },
      volumeM3: defaultVolume,
      surfaceM2: Math.round((defaultMesh.areaMm2 / 1e6) * 100) / 100,
      triangles: defaultMesh.triangles,
      vertices: defaultMesh.vertices,
      facesCount: defaultMesh.facesCount,
      edgesCount: defaultMesh.edgesCount,
    };

    const seedHash = hashSeed(seed.familyId);
    const changelog: ChangelogEntry[] = [];
    const currentVersion = seed.status === 'reviewing' ? '1.5.0' : '1.4.2';
    changelog.push({
      version: currentVersion,
      date: seed.updatedAt,
      author: seed.modeledBy ?? '张工',
      type: seed.status === 'reviewing' ? 'minor' : 'patch',
      changes:
        seed.status === 'reviewing'
          ? ['新增变参维度与几何验真用例', '重新生成预烘焙 JHB 并刷新权威缓存键', '提交标准化委员会技术审核']
          : ['更新默认型号参数与预烘焙几何', '修正特征边线拓扑映射（edgeToShape）'],
      cacheKey: declaredCacheKey,
    });
    changelog.push({
      version: '1.4.0',
      date: formatDateOffset(seed.publishedAt ?? '2026-06-01', -14),
      author: seed.modeledBy ?? '张工',
      type: 'minor',
      changes: ['适配 CBF-C v1.4.2 规范：新增 facesCount 与 faceToShape 拓扑映射', '补齐 4 字节对齐 JHB 头部'],
      cacheKey: await sha256Hex(`${seed.familyId}:1.4.0`),
    });
    if (seed.status === 'published' || seed.status === 'archived') {
      changelog.push({
        version: '1.0.0',
        date: seed.publishedAt ?? '2026-01-01',
        author: seed.modeledBy ?? '张工',
        type: 'major',
        changes: ['构件首次入库并通过 6 道门禁质检', '建立外部语义映射与 IFC 4.3 交付物'],
        cacheKey: await sha256Hex(`${seed.familyId}:1.0.0`),
      });
    }

    const gates: GateResult[] = [
      {
        code: 'GATE-01',
        name: '解压安全与配额',
        status: 'passed',
        message: '压缩比 3.2:1（限值 100:1），解压 2.4MB / 限值 200MB，文件 86 / 限值 500',
        durationMs: 32,
        details: [
          { label: '膨胀比', value: '3.2 : 1（硬限值 100:1）', status: 'ok' },
          { label: '解压总尺寸', value: '2.4 MB / 200 MB', status: 'ok' },
          { label: '文件数', value: '86 / 500', status: 'ok' },
          { label: '目录穿越检测', value: '未发现 ../ 逃逸路径', status: 'ok' },
        ],
      },
      {
        code: 'GATE-02',
        name: 'checksums.json 完整性签名',
        status: 'passed',
        message: '86/86 文件 SHA-256 全部吻合，无游离文件',
        durationMs: 68,
        details: [
          { label: '清单条目', value: '86 项全部匹配', status: 'ok' },
          { label: '游离文件', value: '0 项', status: 'ok' },
          { label: '清单自身摘要', value: `${digest.slice(0, 16)}…`, status: 'ok' },
        ],
      },
      {
        code: 'GATE-03',
        name: '脚本真实性与代码验真',
        status: 'passed',
        message: '源码 SHA-256 匹配，AST 白名单扫描通过，调试语句位于 __main__ 块内',
        durationMs: 121,
        details: [
          { label: '脚本摘要', value: `${digest.slice(0, 24)}…`, status: 'ok' },
          { label: '禁止模块', value: 'os / sys / subprocess / ctypes 均未引入', status: 'ok' },
          { label: '调试语句 show()', value: '仅存在于 __main__ 块（不触发）', status: 'ok' },
        ],
      },
      {
        code: 'GATE-04',
        name: 'Schema 契约与真值表验证',
        status: 'passed',
        message: `符合 v1.4.2 Schema，(sourceMode=${seed.sourceMode}, exactBrep=${seed.sourceMode !== 'mesh-only'}) 通过能力推导真值表`,
        durationMs: 19,
        details: [
          { label: 'JSON Schema', value: 'component.json 校验通过', status: 'ok' },
          { label: '能力推导', value: `(sourceMode=${seed.sourceMode}, exactBrep=${seed.sourceMode !== 'mesh-only'})`, status: 'ok' },
          { label: 'DAG 环检测', value: `${shapeKeys.length} 个部位无循环依赖`, status: 'ok' },
        ],
      },
      {
        code: 'GATE-05',
        name: '几何封闭流形与散度自检',
        status: 'passed',
        message: `BRepCheck 封闭无缝，散度体积 ${defaultVolume.toFixed(2)} m³，偏差 0.00%`,
        durationMs: 842,
        details: [
          { label: '流形状态', value: '封闭实体 (Solid)', status: 'ok' },
          { label: '顶点 / 三角面', value: `${defaultMesh.vertices.toLocaleString()} / ${defaultMesh.triangles.toLocaleString()}`, status: 'ok' },
          { label: '自交 / 自重叠', value: '0 处', status: 'ok' },
          { label: '散度体积复核', value: `${defaultVolume.toFixed(3)} m³（偏差 < 1.0%）`, status: 'ok' },
        ],
      },
      {
        code: 'GATE-06',
        name: '确定性缓存键同源复核',
        status: 'passed',
        message: `实算 ${declaredCacheKey.slice(0, 12)}… == 声明 defaultCacheKey`,
        durationMs: 26,
        details: [
          { label: '实算 cacheKey', value: `${declaredCacheKey.slice(0, 24)}…`, status: 'ok' },
          { label: '声明 cacheKey', value: `${declaredCacheKey.slice(0, 24)}…`, status: 'ok' },
          { label: 'DeterministicJSON', value: '键序 / %.12g / UTF-8 原生直出', status: 'ok' },
        ],
      },
    ];

    const qaReport: QaReport = {
      passed: true,
      executedAt: seed.updatedAt,
      gates,
      volumeDivergence: 0,
      bRepCheck: 'BRepCheck_Analyzer: Valid Solid, Closed Shell',
      scriptDigest: digest,
      extremeTests: [
        { label: '最小尺寸边界压测', ok: true, note: '几何生成成功，体积 41.20 m³' },
        { label: '最大尺寸边界压测', ok: true, note: '几何生成成功，体积 486.35 m³' },
      ],
      mesh: {
        vertices: defaultMesh.vertices,
        triangles: defaultMesh.triangles,
        selfIntersections: 0,
      },
    };

    const downloads = 320 + (seedHash % 2400);
    const references = 12 + (seedHash % 96);
    const component: InternalComponent = {
      componentId: crypto.randomUUID(),
      familyId: seed.familyId,
      revision: currentVersion,
      name: seed.name,
      drawingCode: seed.drawingCode,
      category: seed.category,
      taxonomy: {
        crbim: seed.crbim,
        gb51269: CROSS_SCHEME_MAP[seed.crbim]?.gb51269 ?? '04.02.10',
        jtg: CROSS_SCHEME_MAP[seed.crbim]?.jtg ?? 'JTG-3.2.1',
        omniclass: '21-02 10 10',
      },
      semanticCode: seed.semanticCode,
      semanticProfileHint: 'Railway-CRBIM-IFC4X3@1.4',
      sourceMode: seed.sourceMode,
      status: seed.status,
      visibility: seed.visibility,
      engine: seed.engine,
      tags: seed.tags,
      description: seed.description,
      tenantId: TENANT_ID,
      updatedAt: seed.updatedAt,
      publishedAt: seed.publishedAt,
      geometry,
      presetCount: presets.length,
      keyParams: {
        ...Object.fromEntries(inputs.slice(0, 3).map((key) => [key, defaultParams[key]])),
        StrengthGrade: String(defaultParams.StrengthGrade),
      },
      dims: {
        height: Math.round(boundsMax[2] - boundsMin[2]),
        length: Math.round(boundsMax[0] - boundsMin[0]),
        width: Math.round(boundsMax[1] - boundsMin[1]),
      },
      qa: {
        passed: true,
        passedAt: seed.updatedAt,
        score: seed.familyId === 'PierBody' ? 100 : 96 + (seedHash % 5),
      },
      stats: {
        downloads,
        references,
        rating: 4.2 + ((seedHash % 8) / 10),
      },
      parameters: {
        schema,
        presets,
      },
      shapes,
      ports: portsFor(seed.kind, seed.dims, defaultMesh.bounds),
      recipe,
      recipeSource: seed.cqSource,
      changelog,
      qaReport,
      geometryKind: seed.kind,
      mesh: defaultMesh,
      importedMesh: Boolean(seed.meshFile),
      recipeInputs: inputs,
      presetCacheKeys,
      declaredCacheKey,
      geoKind: seed.kind,
      legacyUnaligned: false,
    };
    result.push(component);
  }

  // 稳定的展示顺序：已发布优先，按更新时间倒序
  return result.sort((a, b) => {
    const order: Record<ComponentStatus, number> = { published: 0, reviewing: 1, draft: 2, archived: 3 };
    if (order[a.status] !== order[b.status]) return order[a.status] - order[b.status];
    return b.updatedAt.localeCompare(a.updatedAt);
  });
}

export { CACHE_INPUTS, DIMENSION_LABELS, TENANT_ID, stripLeadingZero };
