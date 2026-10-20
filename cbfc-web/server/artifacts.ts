/**
 * 制品导出：JHB / GLB / .cbfc 制品包（真实 ZIP） / STEP 与 IFC 交付清单
 */
import type { InternalComponent } from './seed';
import { encodeGlb, renderPreviewSvg } from './geometry';
import { buildGeneratorSource } from './generator';
import { encodeJhb } from './jhb';
import { deterministicJson, sha256Hex } from '../shared/deterministic';

export const FORMATS = [
  {
    id: 'cbfc',
    ext: 'cbfc',
    title: 'CBF-C 构件制品包',
    mime: 'application/zip',
    sizeMB: 2.4,
    description: '元数据 + 建模源码 + 默认预烘焙几何 + 校验清单',
    scenario: '归档 / 全量复用 / 跨项目移交',
    scriptIncluded: true,
    priority: 'P0' as const,
  },
  {
    id: 'cbf',
    ext: 'cbf',
    title: 'CBF 精确几何',
    mime: 'application/zip',
    sizeMB: 1.8,
    description: 'OCCT 原生彩色拓扑流，供桌面 CAD 极速无损加载',
    scenario: 'QtOCCTApp / OCCT 生态',
    scriptIncluded: false,
    priority: 'P0' as const,
  },
  {
    id: 'step',
    ext: 'step',
    title: '工业标准交换几何',
    mime: 'model/step',
    sizeMB: 3.1,
    description: 'STEP AP242 国际标准模型（含交付元数据头）',
    scenario: '通用工业 CAD 交换',
    scriptIncluded: false,
    priority: 'P1' as const,
  },
  {
    id: 'jhb',
    ext: 'jhb',
    title: '拓扑感知二进制流',
    mime: 'application/x-cbf-jhb',
    sizeMB: 0.7,
    description: '适用于网页端轻量化秒级三维浏览的拓扑感知二进制流',
    scenario: 'Web 端极速漫游 / 视口直读',
    scriptIncluded: false,
    priority: 'P0' as const,
  },
  {
    id: 'glb',
    ext: 'glb',
    title: '轻量化可视化网格',
    mime: 'model/gltf-binary',
    sizeMB: 0.9,
    description: 'glTF 2.0 二进制，供通用三维、AR/VR 与大屏渲染',
    scenario: '可视化 / AR/VR / 大屏',
    scriptIncluded: false,
    priority: 'P1' as const,
  },
  {
    id: 'ifc',
    ext: 'ifc',
    title: 'IFC 4.3 数字工程交付物',
    mime: 'application/x-step',
    sizeMB: 3.6,
    description: 'buildingSMART IFC 4.3 桥梁数字模型交付清单（含 IfcShapeAspect 命名面）',
    scenario: 'BIM 交付 / 算量 / 协同',
    scriptIncluded: false,
    priority: 'P0' as const,
  },
];

export function makeJhb(component: InternalComponent, params: Record<string, number | string | boolean>, cacheKey: string, mesh = component.mesh, legacy = false) {
  const shapeKeys = Object.keys(component.shapes);
  const faceToShape = mesh.faceGroups.map((g) => Math.min(g.shapeIndex, shapeKeys.length - 1));
  const edgeToShape = mesh.edgeLines.map((e) => Math.min(e.shapeIndex, shapeKeys.length - 1));
  const outputs: Record<string, Record<string, number | string | number[]>> = {};
  const inputs = component.recipeInputs;

  // 每个 shape 独立计算体积、面积、长度和包围盒，避免所有 solid 共享整体统计。
  const shapeStats = shapeKeys.map((key) => {
    const shape = component.shapes[key];
    return {
      key,
      geomType: shape.geomType,
      volume: 0,
      area: 0,
      length: 0,
      min: [Infinity, Infinity, Infinity] as [number, number, number],
      max: [-Infinity, -Infinity, -Infinity] as [number, number, number],
    };
  });
  const faceShape = new Map<number, number>();
  for (const group of mesh.faceGroups) {
    faceShape.set(group.faceId, Math.min(group.shapeIndex, shapeKeys.length - 1));
  }
  for (let t = 0; t < mesh.indices.length; t += 3) {
    const faceId = mesh.faceIds[Math.floor(t / 3)] ?? 0;
    const shapeIndex = faceShape.get(faceId) ?? 0;
    const stat = shapeStats[shapeIndex];
    if (!stat) continue;
    const ia = mesh.indices[t] * 3;
    const ib = mesh.indices[t + 1] * 3;
    const ic = mesh.indices[t + 2] * 3;
    const ax = mesh.positions[ia], ay = mesh.positions[ia + 1], az = mesh.positions[ia + 2];
    const bx = mesh.positions[ib], by = mesh.positions[ib + 1], bz = mesh.positions[ib + 2];
    const cx = mesh.positions[ic], cy = mesh.positions[ic + 1], cz = mesh.positions[ic + 2];
    const ux = bx - ax, uy = by - ay, uz = bz - az;
    const vx = cx - ax, vy = cy - ay, vz = cz - az;
    stat.area += 0.5 * Math.hypot(uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx);
    stat.volume += (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6;
    for (const [x, y, z] of [[ax, ay, az], [bx, by, bz], [cx, cy, cz]]) {
      stat.min[0] = Math.min(stat.min[0], x);
      stat.min[1] = Math.min(stat.min[1], y);
      stat.min[2] = Math.min(stat.min[2], z);
      stat.max[0] = Math.max(stat.max[0], x);
      stat.max[1] = Math.max(stat.max[1], y);
      stat.max[2] = Math.max(stat.max[2], z);
    }
  }
  for (let e = 0; e < mesh.edgeLines.length; e += 1) {
    const line = mesh.edgeLines[e];
    const shapeIndex = Math.min(line.shapeIndex, shapeKeys.length - 1);
    const stat = shapeStats[shapeIndex];
    if (!stat) continue;
    for (let i = line.vertexStart; i < line.vertexStart + line.vertexCount - 1; i += 1) {
      const a = i * 3;
      const b = (i + 1) * 3;
      stat.length += Math.hypot(
        mesh.edges[b] - mesh.edges[a],
        mesh.edges[b + 1] - mesh.edges[a + 1],
        mesh.edges[b + 2] - mesh.edges[a + 2],
      );
    }
  }

  for (let index = 0; index < shapeKeys.length; index += 1) {
    const shape = component.shapes[shapeKeys[index]];
    const stat = shapeStats[index];
    const finiteBounds = stat && Number.isFinite(stat.min[0]);
    outputs[shapeKeys[index]] = {
      geomType: shape.geomType,
      ...(shape.geomType === 'solid'
        ? { volume: Math.round(Math.abs(stat?.volume ?? 0)) }
        : shape.geomType === 'face'
          ? { area: Math.round(stat?.area ?? 0) }
          : { length: Math.round(stat?.length ?? 0) }),
      bbox: finiteBounds
        ? [...stat.min, ...stat.max]
        : [...mesh.bounds.min, ...mesh.bounds.max],
    };
  }
  const volumeM3 = Math.round((mesh.volumeMm3 / 1e9) * 1000) / 1000;
  const args: Record<string, number | string> = { NetVolume: volumeM3 };
  for (const key of inputs) {
    const value = params[key];
    if (typeof value === 'number') args[key] = value;
  }
  const anchors = Object.fromEntries(
    Object.entries(component.ports).map(([key, port]) => {
      const height = mesh.bounds.max[2] - mesh.bounds.min[2];
      const origin = [...port.origin] as number[];
      if (key === 'TopCenter') origin[2] = height;
      if (key === 'EndCenter') origin[0] = mesh.bounds.max[0];
      if (key === 'StartCenter') origin[0] = mesh.bounds.min[0];
      return [key, { origin, axis: port.axis, refDirection: port.refDirection }];
    }),
  );
  return encodeJhb({
    mesh,
    cacheKey,
    shapeKeys,
    faceToShape,
    edgeToShape,
    outputs,
    args,
    anchors,
    legacyUnaligned: legacy,
    cacheHit: false,
  });
}

function crc32(data: Uint8Array): number {
  let crc = 0xffffffff;
  for (let i = 0; i < data.length; i += 1) {
    crc ^= data[i];
    for (let k = 0; k < 8; k += 1) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}

/** 无压缩 ZIP（store），足以生成可在资源管理器中解压的 .cbfc 包 */
export function zipStore(files: { name: string; data: Uint8Array }[]): Uint8Array {
  const encoder = new TextEncoder();
  const chunks: Uint8Array[] = [];
  const central: Uint8Array[] = [];
  let offset = 0;
  // 固定为 1980-01-01 00:00:00，保证相同输入生成字节级可复现的 ZIP。
  const now = { time: 0, date: 0x21 };
  for (const file of files) {
    const nameBytes = encoder.encode(file.name);
    const crc = crc32(file.data);
    const local = new Uint8Array(30 + nameBytes.length);
    const lv = new DataView(local.buffer);
    lv.setUint32(0, 0x04034b50, true);
    lv.setUint16(4, 20, true);
    lv.setUint16(6, 0x0800, true);
    lv.setUint16(8, 0, true);
    lv.setUint16(10, now.time, true);
    lv.setUint16(12, now.date, true);
    lv.setUint32(14, crc, true);
    lv.setUint32(18, file.data.length, true);
    lv.setUint32(22, file.data.length, true);
    lv.setUint16(26, nameBytes.length, true);
    lv.setUint16(28, 0, true);
    local.set(nameBytes, 30);
    chunks.push(local, file.data);

    const cd = new Uint8Array(46 + nameBytes.length);
    const cv = new DataView(cd.buffer);
    cv.setUint32(0, 0x02014b50, true);
    cv.setUint16(4, 20, true);
    cv.setUint16(6, 20, true);
    cv.setUint16(8, 0x0800, true);
    cv.setUint16(10, 0, true);
    cv.setUint16(12, now.time, true);
    cv.setUint16(14, now.date, true);
    cv.setUint32(16, crc, true);
    cv.setUint32(20, file.data.length, true);
    cv.setUint32(24, file.data.length, true);
    cv.setUint16(28, nameBytes.length, true);
    cv.setUint16(30, 0, true);
    cv.setUint16(32, 0, true);
    cv.setUint16(34, 0, true);
    cv.setUint16(36, 0, true);
    cv.setUint32(38, 0, true);
    cv.setUint32(42, offset, true);
    cd.set(nameBytes, 46);
    central.push(cd);

    offset += local.length + file.data.length;
  }
  const centralSize = central.reduce((sum, c) => sum + c.length, 0);
  const end = new Uint8Array(22);
  const ev = new DataView(end.buffer);
  ev.setUint32(0, 0x06054b50, true);
  ev.setUint16(8, files.length, true);
  ev.setUint16(10, files.length, true);
  ev.setUint32(12, centralSize, true);
  ev.setUint32(16, offset, true);
  const total = offset + centralSize + 22;
  const out = new Uint8Array(total);
  let cursor = 0;
  for (const chunk of chunks) {
    out.set(chunk, cursor);
    cursor += chunk.length;
  }
  for (const chunk of central) {
    out.set(chunk, cursor);
    cursor += chunk.length;
  }
  out.set(end, cursor);
  return out;
}

function pySource(component: InternalComponent): string {
  return buildGeneratorSource({
    familyId: component.familyId,
    name: component.name,
    inputs: component.recipeInputs,
  });
}

export async function buildCbfcFiles(
  component: InternalComponent,
  selectedParams: Record<string, number | string | boolean> = {},
  selectedCacheKey = component.geometry.defaultCacheKey,
): Promise<{ name: string; data: Uint8Array }[]> {
  const encoder = new TextEncoder();
  const defaultModel = String(component.parameters.schema[0]?.default ?? '');
  const defaultPreset = component.parameters.presets.find((p) => p.modelNumber === defaultModel)
    ?? component.parameters.presets[0];
  const defaultParams = Object.keys(selectedParams).length ? selectedParams : defaultPreset?.params ?? {};
  const jhb = makeJhb(component, defaultParams, selectedCacheKey);
  const preview = renderPreviewSvg(component.mesh, Object.values(component.shapes).map((s) => s.code), 640, 460);
  const scriptBytes = encoder.encode(pySource(component));
  const scriptDigest = await sha256Hex(scriptBytes);
  const packageRecipe = component.recipe
    ? { ...component.recipe, source: 'generators/generator.py', digest: scriptDigest }
    : undefined;
  const componentJson = {
    format: 'cbf-component',
    schemaVersion: '1.4.2',
    identity: {
      componentId: component.componentId,
      familyId: component.familyId,
      revision: component.revision,
      name: component.name,
      category: component.category,
      semanticCode: component.semanticCode,
      semanticProfileHint: component.semanticProfileHint,
      sourceMode: component.sourceMode,
      placement: { modes: ['point'], orientation: 'azimuth' },
    },
    environment: {
      units: { length: 'mm', angle: 'rad' },
      tolerance: { linear: 0.01, angular: 0.001 },
    },
    parameters: component.parameters,
    geometry: {
      exactBrep: component.geometry.exactBrep,
      defaultFormat: component.geometry.defaultFormat,
      defaultFile: 'geometry/default.jhb',
      defaultCacheKey: selectedCacheKey,
    },
    recipe: packageRecipe,
    shapes: component.shapes,
    ports: component.ports,
  };
  const componentBytes = encoder.encode(deterministicJson(componentJson));
  const jhbBytes = new Uint8Array(jhb.buffer);
  const previewBytes = encoder.encode(preview);
  const included: { name: string; data: Uint8Array }[] = [
    { name: 'component.json', data: componentBytes },
    { name: 'geometry/default.jhb', data: jhbBytes },
    { name: 'preview.svg', data: previewBytes },
    { name: 'generators/generator.py', data: scriptBytes },
  ];
  const checksums: Record<string, string> = {};
  for (const file of included) {
    checksums[file.name] = `sha256:${await sha256Hex(file.data)}`;
  }
  included.push({
    name: 'checksums.json',
    data: encoder.encode(`${JSON.stringify({ algorithm: 'sha256', files: checksums }, null, 2)}\n`),
  });
  return included;
}

export async function buildCbfcZip(
  component: InternalComponent,
  selectedParams: Record<string, number | string | boolean> = {},
  selectedCacheKey = component.geometry.defaultCacheKey,
): Promise<Uint8Array> {
  return zipStore(await buildCbfcFiles(component, selectedParams, selectedCacheKey));
}

export function makeCbfZip(
  component: InternalComponent,
  selectedParams: Record<string, number | string | boolean> = {},
  selectedCacheKey = component.geometry.defaultCacheKey,
): Uint8Array {
  const encoder = new TextEncoder();
  const defaultModel = String(component.parameters.schema[0]?.default ?? '');
  const defaultPreset = component.parameters.presets.find((p) => p.modelNumber === defaultModel)
    ?? component.parameters.presets[0];
  const jhb = makeJhb(
    component,
    Object.keys(selectedParams).length ? selectedParams : defaultPreset?.params ?? {},
    selectedCacheKey,
  );
  return zipStore([
    {
      name: 'geometry/default.jhb',
      data: new Uint8Array(jhb.buffer),
    },
    {
      name: 'manifest.json',
      data: encoder.encode(
        JSON.stringify(
          {
            format: 'cbf-geometry',
            familyId: component.familyId,
            cacheKey: selectedCacheKey,
            exactBrep: component.geometry.exactBrep,
            note: 'CBF 精确几何容器（演示快照：含拓扑感知 JHB 与权威缓存键）',
          },
          null,
          2,
        ),
      ),
    },
  ]);
}

export function makeStepFile(component: InternalComponent): Uint8Array {
  const b = component.geometry.bounds;
  const text = `ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('CBF-C Advanced BRep Delivery','STEP AP242'),'2;1');
FILE_NAME('${component.familyId}_${component.revision}.step','2026-10-19T03:00:00',('CBF-C Web Library'),('China Railway 17th Bureau'),'CBF-C Web 2.5','CBF-C Web 2.5','');
FILE_SCHEMA(('AP242_MANAGED_MODEL_BASED_3D_ENGINEERING_MIM_LF'));
ENDSEC;
DATA;
/* CBF-C 构件交付元数据
   familyId      : ${component.familyId}
   revision      : ${component.revision}
   cacheKey      : ${component.geometry.defaultCacheKey}
   volume        : ${component.geometry.volumeM3} m3
   bbox          : [${b.min.join(', ')}] .. [${b.max.join(', ')}]
   几何实体由 CBF-C 几何 Worker 生成，本文件为演示交付占位（无 B-Rep 面包）。
*/
ENDSEC;
END-ISO-10303-21;
`;
  return new TextEncoder().encode(text);
}

export function makeIfcFile(component: InternalComponent): Uint8Array {
  const step = (value: string) => String(value).replace(/'/g, "''").replace(/[\r\n]/g, ' ');
  const safeName = step(component.name);
  const safeCategory = step(component.category);
  const safeFamilyId = step(component.familyId);
  const text = `ISO-10303-21;
HEADER;
FILE_DESCRIPTION(('ViewDefinition [ReferenceView_V1.2]'),'2;1');
FILE_NAME('${safeFamilyId}_${step(component.revision)}.ifc','2026-10-19T03:00:00',('CBF-C'),('CR17J'),'CBF-C Web 2.5','CBF-C Web 2.5','');
FILE_SCHEMA(('IFC4X3_ADD2'));
ENDSEC;
DATA;
/* CBF-C IFC 4.3 数字工程交付物
   familyId=${safeFamilyId} revision=${step(component.revision)}
   semanticProfile=${step(component.semanticProfileHint)}
   cacheKey=${component.geometry.defaultCacheKey}
   shapes=${Object.keys(component.shapes).map(step).join(', ')}
   unmapped=${Object.values(component.shapes).filter((s) => s.code.includes('Monitor') || s.code.includes('Drainage')).length}
   本文件为演示导出：保留 IFC 4.3 HEADER 与 IfcShapeAspect 命名面清单，几何由服务端导出管线生成。
*/
#1=IFCPROJECT('2f7c9e12-0000-4000-8000-000000000001',$,'${safeName}',$,$,$,$,$,$);
#2=IFCSITE('2f7c9e12-0000-4000-8000-000000000002',$,'CBF-C Site',$,$,$,$,$,$,$,$,$,$,$);
#3=IFCBUILDING('2f7c9e12-0000-4000-8000-000000000003',$,'${safeCategory}',$,$,$,$,$,$,$,$,$);
#4=IFCBRIDGE('2f7c9e12-0000-4000-8000-000000000004',$,'${safeName}',$,$,$,$,$,$,$,$,$);
#5=IFCCOLUMN('2f7c9e12-0000-4000-8000-000000000005',$,'${safeFamilyId}',$,$,$,$,$,$,$);
ENDSEC;
END-ISO-10303-21;
`;
  return new TextEncoder().encode(text);
}

export async function buildArtifact(
  component: InternalComponent,
  format: string,
  cacheKey: string,
  params: Record<string, number | string | boolean> = {},
): Promise<{ data: Uint8Array; fileName: string; mime: string }> {
  const f = FORMATS.find((x) => x.id === format) ?? FORMATS[0];
  const base = `${component.familyId}_${component.revision}`;
  switch (format) {
    case 'jhb':
      return { data: new Uint8Array(makeJhb(component, params, cacheKey).buffer), fileName: `${base}.jhb`, mime: f.mime };
    case 'glb':
      return { data: encodeGlb(component.mesh, component.name), fileName: `${base}.glb`, mime: f.mime };
    case 'cbfc':
      return { data: await buildCbfcZip(component, params, cacheKey), fileName: `${base}.cbfc`, mime: f.mime };
    case 'cbf':
      return { data: makeCbfZip(component, params, cacheKey), fileName: `${base}.cbf`, mime: f.mime };
    case 'step':
      return { data: makeStepFile(component), fileName: `${base}.step`, mime: f.mime };
    case 'ifc':
      return { data: makeIfcFile(component), fileName: `${base}.ifc`, mime: f.mime };
    default:
      return { data: await buildCbfcZip(component), fileName: `${base}.cbfc`, mime: 'application/zip' };
  }
}
