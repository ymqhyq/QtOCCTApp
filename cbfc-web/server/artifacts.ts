/**
 * 制品导出：JHB / GLB / .cbfc 制品包（真实 ZIP） / STEP 与 IFC 交付清单
 */
import type { InternalComponent } from './seed';
import { buildMesh, encodeGlb, renderPreviewSvg } from './geometry';
import { buildGeneratorSource } from './generator';
import { encodeJhb } from './jhb';
import { deterministicJson, sha256Hex } from '../shared/deterministic';
import { generateAbutmentMesh, generateAbutmentStep } from './cq';

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
    scenario: 'OCCT 生态',
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
  const stepString = (value: string) => `'${String(value).replace(/'/g, "''").replace(/[\r\n]/g, ' ')}'`;
  const number = (value: number) => (Number.isFinite(value) ? Number(value).toFixed(6) : '0.000000');
  const mesh = component.mesh;
  const timestamp = `${(component.updatedAt || '2026-01-01').slice(0, 10)}T00:00:00`;
  const lines: string[] = [
    'ISO-10303-21;',
    'HEADER;',
    "FILE_DESCRIPTION(('CBF-C component delivery'),'2;1');",
    `FILE_NAME(${stepString(`${component.familyId}_${component.revision}.step`)},'${timestamp}',(${stepString('CBF-C')}),(${stepString('CR17J')}),'CBF-C Web 2.5','CBF-C Web 2.5','');`,
    "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));",
    'ENDSEC;',
    'DATA;',
    "#1=APPLICATION_CONTEXT('configuration controlled 3d designs of mechanical parts and assemblies');",
    "#2=APPLICATION_PROTOCOL_DEFINITION('international standard','configuration controlled 3d designs of mechanical parts and assemblies',1994,#1);",
    `#3=PRODUCT(${stepString(component.familyId)},${stepString(component.familyId)},'',(#4));`,
    "#4=PRODUCT_CONTEXT('',#1,'mechanical');",
    "#5=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE('','',#3,.NOT_KNOWN.);",
    "#6=PRODUCT_DEFINITION_CONTEXT('part definition',#1,'design');",
    "#7=PRODUCT_DEFINITION('design','',#5,#6);",
    "#8=PRODUCT_DEFINITION_SHAPE('','',#7);",
    '#9=(LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.));',
    '#10=(NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.));',
    '#11=(NAMED_UNIT(*) SOLID_ANGLE_UNIT() SI_UNIT($,.STERADIAN.));',
    "#12=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-05),#9,'distance_accuracy_value','');",
    "#13=(GEOMETRIC_REPRESENTATION_CONTEXT(3) GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#12)) GLOBAL_UNIT_ASSIGNED_CONTEXT((#9,#10,#11)) REPRESENTATION_CONTEXT('',''));",
    "#14=CARTESIAN_POINT('',(0.,0.,0.));",
    "#15=DIRECTION('',(0.,0.,1.));",
    "#16=DIRECTION('',(1.,0.,0.));",
    "#17=AXIS2_PLACEMENT_3D('',#14,#15,#16);",
  ];

  let nextId = 100;
  const pointIds: number[] = [];
  for (let i = 0; i < mesh.positions.length / 3; i += 1) {
    const id = nextId++;
    pointIds.push(id);
    lines.push(
      `#${id}=CARTESIAN_POINT('',(${number(mesh.positions[i * 3])},${number(mesh.positions[i * 3 + 1])},${number(mesh.positions[i * 3 + 2])}));`,
    );
  }
  const faces: number[] = [];
  for (let t = 0; t < mesh.indices.length; t += 3) {
    const loopId = nextId++;
    const boundId = nextId++;
    const faceId = nextId++;
    lines.push(
      `#${loopId}=POLY_LOOP('',(#${pointIds[mesh.indices[t]]},#${pointIds[mesh.indices[t + 1]]},#${pointIds[mesh.indices[t + 2]]}));`,
      `#${boundId}=FACE_OUTER_BOUND('',#${loopId},.T.);`,
      `#${faceId}=FACE('',(#${boundId}));`,
    );
    faces.push(faceId);
  }
  const shellId = nextId++;
  const brepId = nextId++;
  const shapeRepresentationId = nextId++;
  lines.push(
    `#${shellId}=CLOSED_SHELL('',(${faces.map((id) => `#${id}`).join(',')}));`,
    `#${brepId}=FACETED_BREP('',#${shellId});`,
    `#${shapeRepresentationId}=SHAPE_REPRESENTATION('',(#${brepId}),#13);`,
    `#${nextId++}=SHAPE_DEFINITION_REPRESENTATION(#8,#${shapeRepresentationId});`,
    'ENDSEC;',
    'END-ISO-10303-21;',
  );
  return new TextEncoder().encode(lines.join('\n'));
}

export function makeIfcFile(component: InternalComponent): Uint8Array {
  const ifcString = (value: string) => {
    let out = '';
    for (let i = 0; i < value.length; i += 1) {
      const code = value.charCodeAt(i);
      if (code === 39) out += "''";
      else if (code === 92) out += '\\\\';
      else if (code >= 32 && code <= 126) out += value[i];
      else out += `\\X2\\${code.toString(16).toUpperCase().padStart(4, '0')}\\X0\\`;
    }
    return `'${out}'`;
  };
  const ifcNumber = (value: number) => (Number.isFinite(value) ? Number(value).toFixed(3) : '0.000');
  const mesh = component.mesh;
  const date = `${(component.updatedAt || '2026-01-01').slice(0, 10)}T00:00:00`;
  const lines: string[] = [
    'ISO-10303-21;',
    'HEADER;',
    "FILE_DESCRIPTION(('ViewDefinition [CoordinationView]'),'2;1');",
    `FILE_NAME(${ifcString(`${component.familyId}_${component.revision}.ifc`)},'${date}',(${ifcString('CBF-C')}),(${ifcString('CR17J')}),'CBF-C Web 2.5','CBF-C Web 2.5','');`,
    "FILE_SCHEMA(('IFC4'));",
    'ENDSEC;',
    'DATA;',
    "#1=IFCPERSON($,$,'CBF-C',$,$,$,$,$);",
    "#2=IFCORGANIZATION($,'CR17J',$,$,$);",
    '#3=IFCPERSONANDORGANIZATION(#1,#2,$);',
    "#4=IFCAPPLICATION(#2,'2.5','CBF-C Web','CBF-C Web');",
    '#5=IFCOWNERHISTORY(#3,#4,$,.ADDED.,$,$,$,0);',
    '#6=IFCCARTESIANPOINT((0.,0.,0.));',
    '#7=IFCDIRECTION((0.,0.,1.));',
    '#8=IFCDIRECTION((1.,0.,0.));',
    '#9=IFCAXIS2PLACEMENT3D(#6,#7,#8);',
    "#10=IFCGEOMETRICREPRESENTATIONCONTEXT('Model','Model',3,1.E-05,#9,$);",
    '#11=IFCSIUNIT(*,.LENGTHUNIT.,.MILLI.,.METRE.);',
    '#12=IFCUNITASSIGNMENT((#11));',
    `#13=IFCPROJECT('LyVdQwF4$gR$YaTmP$IJLd',#5,${ifcString(component.name)},$,$,$,$,(#10),#12);`,
    '#14=IFCLOCALPLACEMENT($,#9);',
    `#15=IFCSITE('XsTzLiv6KBzYCBbaW0YfOu',#5,'CBF-C Site',$,$,#14,$,$,.ELEMENT.,$,$,$,$,$);`,
    "#16=IFCRELAGGREGATES('acjn_u4VxmeP9K_Nw00xfz',#5,$,$,#13,(#15));",
    '#17=IFCLOCALPLACEMENT(#14,#9);',
    "#18=IFCBUILDING('QMF0U4519HMeRnwUsHpMtJ',#5,'CBF-C Building',$,$,#17,$,$,.ELEMENT.,$,$,$);",
    "#19=IFCRELAGGREGATES('PKeG5XLNljhqz4zI7HzIe4',#5,$,$,#15,(#18));",
    '#20=IFCCARTESIANPOINT((0.,0.,0.));',
    '#21=IFCDIRECTION((0.,0.,1.));',
    '#22=IFCDIRECTION((1.,0.,0.));',
    '#23=IFCAXIS2PLACEMENT3D(#20,#21,#22);',
    '#24=IFCLOCALPLACEMENT(#17,#23);',
  ];

  let nextId = 1000;
  const pointIds: number[] = [];
  for (let i = 0; i < mesh.positions.length / 3; i += 1) {
    const id = nextId++;
    pointIds.push(id);
    lines.push(
      `#${id}=IFCCARTESIANPOINT((${ifcNumber(mesh.positions[i * 3])},${ifcNumber(mesh.positions[i * 3 + 1])},${ifcNumber(mesh.positions[i * 3 + 2])}));`,
    );
  }

  const faceShape = new Map<number, number>();
  for (const group of mesh.faceGroups) {
    faceShape.set(group.faceId, group.shapeIndex);
  }
  const shapeTriangles = new Map<number, number[]>();
  for (let t = 0; t < mesh.indices.length; t += 3) {
    const faceId = mesh.faceIds[Math.floor(t / 3)] ?? Math.floor(t / 3);
    const shapeIndex = faceShape.get(faceId) ?? 0;
    const list = shapeTriangles.get(shapeIndex) ?? [];
    list.push(t);
    shapeTriangles.set(shapeIndex, list);
  }

  const shapeKeys = Object.keys(component.shapes);
  const productIds: number[] = [];
  const productGuids = [
    'Edul4Zpfg0AXn0t4N7XtCR',
    'MKp7LWrFukOreaDP2UBE37',
    'g395Fx07u2VQJdRMr3mmDr',
    'bkxwvkwbhf99fbRZsb7Up5',
    '2pcbg2OrO58H7Xn0TWffaf',
    'JZbmdg4E5FrC4jTGe3$dhY',
    'shXIBsQisb53snCJfJ$5gn',
    'QAuZEuRfaTLePDNsrwcKsm',
  ];
  for (let shapeIndex = 0; shapeIndex < shapeKeys.length; shapeIndex += 1) {
    const key = shapeKeys[shapeIndex];
    const shape = component.shapes[key];
    const triangles = shapeTriangles.get(shapeIndex) ?? [];
    let representationRef = '$';
    if (triangles.length) {
      const faces: number[] = [];
      for (const t of triangles) {
        const loopId = nextId++;
        const boundId = nextId++;
        const faceId = nextId++;
        lines.push(
          `#${loopId}=IFCPOLYLOOP((#${pointIds[mesh.indices[t]]},#${pointIds[mesh.indices[t + 1]]},#${pointIds[mesh.indices[t + 2]]}));`,
          `#${boundId}=IFCFACEOUTERBOUND(#${loopId},.T.);`,
          `#${faceId}=IFCFACE((#${boundId}));`,
        );
        faces.push(faceId);
      }
      const shellId = nextId++;
      const brepId = nextId++;
      const shapeRepresentationId = nextId++;
      const productShapeId = nextId++;
      lines.push(
        `#${shellId}=IFCCLOSEDSHELL((${faces.map((id) => `#${id}`).join(',')}));`,
        `#${brepId}=IFCFACETEDBREP(#${shellId});`,
        `#${shapeRepresentationId}=IFCSHAPEREPRESENTATION(#10,'Body','Brep',(#${brepId}));`,
        `#${productShapeId}=IFCPRODUCTDEFINITIONSHAPE($,$,(#${shapeRepresentationId}));`,
      );
      representationRef = `#${productShapeId}`;
    }
    const productId = nextId++;
    productIds.push(productId);
    lines.push(
      `#${productId}=IFCBUILDINGELEMENTPROXY('${productGuids[shapeIndex % productGuids.length]}',#5,${ifcString(shape.label)},$,'${component.familyId}/${key}',#24,${representationRef},$,$);`,
    );
  }
  lines.push(
    `#26=IFCRELCONTAINEDINSPATIALSTRUCTURE('oEU57lEksPm6nj9MJU8lr5',#5,$,$,(${productIds.map((id) => `#${id}`).join(',')}),#18);`,
    'ENDSEC;',
    'END-ISO-10303-21;',
  );
  return new TextEncoder().encode(lines.join('\n'));
}

export async function buildArtifact(
  component: InternalComponent,
  format: string,
  cacheKey: string,
  params: Record<string, number | string | boolean> = {},
): Promise<{ data: Uint8Array; fileName: string; mime: string }> {
  const f = FORMATS.find((x) => x.id === format) ?? FORMATS[0];
  const base = `${component.familyId}_${component.revision}`;
  let effectiveComponent = component;
  const usesCq = component.familyId === 'Abutment' || Boolean(component.cqGenerator);
  if (
    usesCq &&
    component.sourceMode === 'parametric' &&
    Object.keys(params).length &&
    format !== 'step'
  ) {
    const cqMesh = await generateAbutmentMesh(params, {
      generatorPath: component.cqGenerator,
      shapeKeys: Object.keys(component.shapes),
    });
    if (!cqMesh) throw new Error('GEOMETRY_BACKEND_UNAVAILABLE');
    if (cqMesh) {
      effectiveComponent = {
        ...component,
        mesh: cqMesh,
        geometry: {
          ...component.geometry,
          volumeM3: Math.round((cqMesh.volumeMm3 / 1e9) * 1000) / 1000,
          triangles: cqMesh.triangles,
          vertices: cqMesh.vertices,
        },
      };
    }
  } else if (
    component.sourceMode === 'parametric' &&
    !component.importedMesh &&
    Object.keys(params).length &&
    format !== 'step'
  ) {
    const mesh = buildMesh(
      component.geoKind,
      Object.fromEntries(component.recipeInputs.map((key) => [key, Number(params[key])])) as Record<string, number>,
    );
    effectiveComponent = {
      ...component,
      mesh,
      geometry: {
        ...component.geometry,
        volumeM3: Math.round((mesh.volumeMm3 / 1e9) * 1000) / 1000,
        triangles: mesh.triangles,
        vertices: mesh.vertices,
      },
    };
  }
  switch (format) {
    case 'jhb':
      return { data: new Uint8Array(makeJhb(effectiveComponent, params, cacheKey).buffer), fileName: `${base}.jhb`, mime: f.mime };
    case 'glb':
      return { data: encodeGlb(effectiveComponent.mesh, effectiveComponent.name), fileName: `${base}.glb`, mime: f.mime };
    case 'cbfc':
      return { data: await buildCbfcZip(effectiveComponent, params, cacheKey), fileName: `${base}.cbfc`, mime: f.mime };
    case 'cbf':
      return { data: makeCbfZip(effectiveComponent, params, cacheKey), fileName: `${base}.cbf`, mime: f.mime };
    case 'step': {
      if (usesCq && component.sourceMode === 'parametric') {
        const cqStep = await generateAbutmentStep(params, { generatorPath: component.cqGenerator });
        if (cqStep) return { data: cqStep, fileName: `${base}.step`, mime: f.mime };
      }
      return { data: makeStepFile(component), fileName: `${base}.step`, mime: f.mime };
    }
    case 'ifc':
      return { data: makeIfcFile(effectiveComponent), fileName: `${base}.ifc`, mime: f.mime };
    default:
      return { data: await buildCbfcZip(effectiveComponent), fileName: `${base}.cbfc`, mime: 'application/zip' };
  }
}
