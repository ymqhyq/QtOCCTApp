/**
 * 服务端二维投影图：将程序化三角网格投影为带消隐线的 SVG。
 * - 显式构造视平面基向量，图像上方恒为模型 +Z
 * - 可见线实线、隐藏线虚线、自动居中、XYZ 轴指示器
 * - 附加三视图排版与标题栏
 *
 * 本项目没有 OCCT，使用三角网格深度缓冲实现等价的消隐判断（mesh HLR），
 * 因而可在纯 Node/Vite 环境内运行。
 */
import type { MeshData } from './geometry';

export type ProjectionView = 'sheet' | 'front' | 'top' | 'side' | 'iso';

export interface ProjectionMeta {
  familyId: string;
  name: string;
  revision: string;
  semanticCode: string;
  cacheKey: string;
}

export interface ProjectionOptions {
  view?: ProjectionView;
  hidden?: boolean;
  dimensions?: boolean;
  generatedAt?: string;
}

type Vec3 = [number, number, number];

interface Basis {
  normal: Vec3;
  right: Vec3;
  up: Vec3;
}

interface Panel {
  x: number;
  y: number;
  w: number;
  h: number;
  label: string;
}

interface ProjectedVertex {
  x: number;
  y: number;
  d: number;
}

interface ViewGeometry {
  view: Exclude<ProjectionView, 'sheet'>;
  basis: Basis;
  projected: ProjectedVertex[];
  minU: number;
  maxU: number;
  minV: number;
  maxV: number;
}

const VIEW_LABELS: Record<Exclude<ProjectionView, 'sheet'>, string> = {
  front: '主视图（正视）',
  top: '俯视图',
  side: '左视图（侧视）',
  iso: '轴测图',
};

const HTML_ESCAPE: Record<string, string> = {
  '&': '&amp;',
  '<': '&lt;',
  '>': '&gt;',
  '"': '&quot;',
  "'": '&#39;',
};

function escapeXml(value: string): string {
  return value.replace(/[&<>"']/g, (char) => HTML_ESCAPE[char] ?? char);
}

function norm(v: Vec3): Vec3 {
  const length = Math.hypot(v[0], v[1], v[2]);
  return length ? [v[0] / length, v[1] / length, v[2] / length] : [0, 0, 1];
}

function dot(a: Vec3, b: Vec3): number {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

function sub(a: Vec3, b: Vec3): Vec3 {
  return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function scale3(v: Vec3, s: number): Vec3 {
  return [v[0] * s, v[1] * s, v[2] * s];
}

function cross(a: Vec3, b: Vec3): Vec3 {
  return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
}

/** 视平面基：图像向右 / 向上 / 朝向相机的法向 */
export function viewBasis(view: Exclude<ProjectionView, 'sheet'>): Basis {
  let normal: Vec3;
  let up: Vec3;
  if (view === 'front') {
    normal = [0, -1, 0];
    up = [0, 0, 1];
  } else if (view === 'top') {
    normal = [0, 0, 1];
    up = [0, 1, 0];
  } else if (view === 'side') {
    normal = [1, 0, 0];
    up = [0, 0, 1];
  } else {
    normal = norm([1, -1, 0.6]);
    // +Z 在视平面上的投影作为图像向上，保持 Z 轴朝上
    const z = [0, 0, 1] as Vec3;
    const projectedZ = sub(z, scale3(normal, dot(z, normal)));
    up = norm(projectedZ);
  }
  const right = norm(cross(up, normal));
  return { normal, right, up };
}

function buildViewGeometry(
  mesh: MeshData,
  view: Exclude<ProjectionView, 'sheet'>,
): ViewGeometry {
  const basis = viewBasis(view);
  const projected: ProjectedVertex[] = [];
  let minU = Infinity;
  let maxU = -Infinity;
  let minV = Infinity;
  let maxV = -Infinity;
  for (let i = 0; i < mesh.positions.length / 3; i += 1) {
    const p: Vec3 = [mesh.positions[i * 3], mesh.positions[i * 3 + 1], mesh.positions[i * 3 + 2]];
    const x = dot(p, basis.right);
    const y = dot(p, basis.up);
    const d = dot(p, basis.normal);
    projected.push({ x, y, d });
    minU = Math.min(minU, x);
    maxU = Math.max(maxU, x);
    minV = Math.min(minV, y);
    maxV = Math.max(maxV, y);
  }
  return { view, basis, projected, minU, maxU, minV, maxV };
}

function fillDepthBuffer(
  mesh: MeshData,
  geometry: ViewGeometry,
  panel: Panel,
  scale: number,
  cx: number,
  cy: number,
): Float32Array {
  const buffer = new Float32Array(panel.w * panel.h).fill(-Infinity);
  const project = (index: number) => {
    const point = geometry.projected[index];
    return {
      x: cx + (point.x - (geometry.minU + geometry.maxU) / 2) * scale,
      y: cy - (point.y - (geometry.minV + geometry.maxV) / 2) * scale,
      d: point.d,
    };
  };
  for (let t = 0; t < mesh.indices.length; t += 3) {
    const a = project(mesh.indices[t]);
    const b = project(mesh.indices[t + 1]);
    const c = project(mesh.indices[t + 2]);
    const minX = Math.max(0, Math.floor(Math.min(a.x, b.x, c.x) - panel.x));
    const maxX = Math.min(panel.w - 1, Math.ceil(Math.max(a.x, b.x, c.x) - panel.x));
    const minY = Math.max(0, Math.floor(Math.min(a.y, b.y, c.y) - panel.y));
    const maxY = Math.min(panel.h - 1, Math.ceil(Math.max(a.y, b.y, c.y) - panel.y));
    if (minX > maxX || minY > maxY) continue;
    const denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (Math.abs(denom) < 1e-9) continue;
    for (let py = minY; py <= maxY; py += 1) {
      const worldY = panel.y + py + 0.5;
      for (let px = minX; px <= maxX; px += 1) {
        const worldX = panel.x + px + 0.5;
        const w0 = ((b.y - c.y) * (worldX - c.x) + (c.x - b.x) * (worldY - c.y)) / denom;
        const w1 = ((c.y - a.y) * (worldX - c.x) + (a.x - c.x) * (worldY - c.y)) / denom;
        const w2 = 1 - w0 - w1;
        if (w0 < -1e-7 || w1 < -1e-7 || w2 < -1e-7) continue;
        const depth = w0 * a.d + w1 * b.d + w2 * c.d;
        const index = py * panel.w + px;
        if (depth > buffer[index]) buffer[index] = depth;
      }
    }
  }
  return buffer;
}

function fallbackEdgePolylines(mesh: MeshData): { points: Vec3[]; closed: boolean }[] {
  const edges = new Map<string, { a: Vec3; b: Vec3; normals: Vec3[] }>();
  const keyOf = (p: Vec3) => p.map((value) => Math.round(value * 100)).join(',');
  const addEdge = (ia: number, ib: number, normal: Vec3) => {
    const a: Vec3 = [mesh.positions[ia * 3], mesh.positions[ia * 3 + 1], mesh.positions[ia * 3 + 2]];
    const b: Vec3 = [mesh.positions[ib * 3], mesh.positions[ib * 3 + 1], mesh.positions[ib * 3 + 2]];
    const ka = keyOf(a);
    const kb = keyOf(b);
    const key = ka < kb ? `${ka}|${kb}` : `${kb}|${ka}`;
    const entry = edges.get(key) ?? { a, b, normals: [] };
    entry.normals.push(normal);
    edges.set(key, entry);
  };
  for (let t = 0; t < mesh.indices.length; t += 3) {
    const ia = mesh.indices[t];
    const ib = mesh.indices[t + 1];
    const ic = mesh.indices[t + 2];
    const a: Vec3 = [mesh.positions[ia * 3], mesh.positions[ia * 3 + 1], mesh.positions[ia * 3 + 2]];
    const b: Vec3 = [mesh.positions[ib * 3], mesh.positions[ib * 3 + 1], mesh.positions[ib * 3 + 2]];
    const c: Vec3 = [mesh.positions[ic * 3], mesh.positions[ic * 3 + 1], mesh.positions[ic * 3 + 2]];
    const normal = norm(cross(sub(b, a), sub(c, a)));
    addEdge(ia, ib, normal);
    addEdge(ib, ic, normal);
    addEdge(ic, ia, normal);
  }
  const result: { points: Vec3[]; closed: boolean }[] = [];
  const threshold = Math.cos((20 * Math.PI) / 180);
  for (const edge of edges.values()) {
    if (edge.normals.length === 1 || edge.normals.some((normal, index) =>
      edge.normals.slice(index + 1).some((other) => dot(normal, other) < threshold),
    )) {
      result.push({ points: [edge.a, edge.b], closed: false });
    }
  }
  return result;
}

function edgePolylines(mesh: MeshData): { points: Vec3[]; closed: boolean }[] {
  if (!mesh.edges.length) return fallbackEdgePolylines(mesh);
  const result: { points: Vec3[]; closed: boolean }[] = [];
  for (let e = 0; e < mesh.edgeOffsets.length - 1; e += 1) {
    const start = mesh.edgeOffsets[e];
    const end = mesh.edgeOffsets[e + 1];
    const points: Vec3[] = [];
    for (let i = start; i < end; i += 1) {
      points.push([mesh.edges[i * 3], mesh.edges[i * 3 + 1], mesh.edges[i * 3 + 2]]);
    }
    if (points.length >= 2) {
      result.push({ points, closed: Boolean(mesh.edgeLines[e]?.closed) });
    }
  }
  return result;
}

interface LineSet {
  visible: string[];
  hidden: string[];
}

function clipLine(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

function buildLineSet(
  mesh: MeshData,
  geometry: ViewGeometry,
  panel: Panel,
  scale: number,
  cx: number,
  cy: number,
  buffer: Float32Array,
  showHidden: boolean,
): LineSet {
  const lines: LineSet = { visible: [], hidden: [] };
  const project = (p: Vec3) => {
    const x = dot(p, geometry.basis.right);
    const y = dot(p, geometry.basis.up);
    const d = dot(p, geometry.basis.normal);
    return {
      x: cx + (x - (geometry.minU + geometry.maxU) / 2) * scale,
      y: cy - (y - (geometry.minV + geometry.maxV) / 2) * scale,
      d,
    };
  };
  const span = Math.max(geometry.maxU - geometry.minU, geometry.maxV - geometry.minV, 1);
  // 边缘采样点落在像素边界或共面接触面时，深度缓冲会略大于线本身的深度。
  // 俯视图的轮廓几乎都是可见外轮廓，圆弧/像素边界误差更大，单独放宽到 8%；
  // 其余视图使用 2.5%，仍能保留真实背线的消隐。
  const bias = span * (geometry.view === 'top' ? 0.08 : 0.025);
  const visibleAt = (point: Vec3) => {
    const p = project(point);
    const px = Math.floor(p.x - panel.x);
    const py = Math.floor(p.y - panel.y);
    if (px < 0 || py < 0 || px >= panel.w || py >= panel.h) return false;
    const depth = buffer[py * panel.w + px];
    return !Number.isFinite(depth) || p.d >= depth - bias;
  };
  const push = (a: Vec3, b: Vec3, visible: boolean) => {
    if (!visible && !showHidden) return;
    const pa = project(a);
    const pb = project(b);
    const x1 = clipLine(pa.x, panel.x, panel.x + panel.w);
    const y1 = clipLine(pa.y, panel.y, panel.y + panel.h);
    const x2 = clipLine(pb.x, panel.x, panel.x + panel.w);
    const y2 = clipLine(pb.y, panel.y, panel.y + panel.h);
    if (Math.hypot(x2 - x1, y2 - y1) < 0.15) return;
    const path = `M${x1.toFixed(2)} ${y1.toFixed(2)}L${x2.toFixed(2)} ${y2.toFixed(2)}`;
    (visible ? lines.visible : lines.hidden).push(path);
  };
  const midpoint = (a: Vec3, b: Vec3): Vec3 => [
    (a[0] + b[0]) / 2,
    (a[1] + b[1]) / 2,
    (a[2] + b[2]) / 2,
  ];

  const classify = (a: Vec3, b: Vec3, depth = 0): boolean => {
    const m = midpoint(a, b);
    const va = visibleAt(a);
    const vm = visibleAt(m);
    const vb = visibleAt(b);
    if (va === vb && va === vm) {
      push(a, b, va);
      return va;
    }
    if (depth >= 4) {
      push(a, b, vm);
      return vm;
    }
    const left = classify(a, m, depth + 1);
    const right = classify(m, b, depth + 1);
    return left && right;
  };

  for (const polyline of edgePolylines(mesh)) {
    const points = polyline.closed ? [...polyline.points, polyline.points[0]] : polyline.points;
    for (let i = 0; i < points.length - 1; i += 1) {
      classify(points[i], points[i + 1]);
    }
  }
  return lines;
}

function pathMarkup(paths: string[], hidden: boolean): string {
  if (!paths.length) return '';
  return `<path d="${paths.join('')}" fill="none" stroke="${hidden ? '#9CA3AF' : '#1F2937'}" stroke-width="${hidden ? 0.9 : 1.35}" stroke-linecap="round" stroke-linejoin="round"${hidden ? ' stroke-dasharray="5 4"' : ''}/>`;
}

function dimensionMarkup(
  panel: Panel,
  scale: number,
  geometry: ViewGeometry,
  cx: number,
  cy: number,
): string {
  const width = (geometry.maxU - geometry.minU) * scale;
  const height = (geometry.maxV - geometry.minV) * scale;
  if (width < 2 || height < 2) return '';
  const left = cx - width / 2;
  const right = cx + width / 2;
  const bottom = cy + height / 2;
  const top = cy - height / 2;
  const dimY = Math.min(panel.y + panel.h - 18, bottom + 26);
  const dimX = Math.max(panel.x + 22, left - 28);
  const widthMm = Math.round(geometry.maxU - geometry.minU);
  const heightMm = Math.round(geometry.maxV - geometry.minV);
  const tick = 4;
  return [
    `<g stroke="#64748B" stroke-width="0.8" fill="none">`,
    `<line x1="${left.toFixed(2)}" y1="${(bottom + 4).toFixed(2)}" x2="${left.toFixed(2)}" y2="${(dimY + 4).toFixed(2)}"/>`,
    `<line x1="${right.toFixed(2)}" y1="${(bottom + 4).toFixed(2)}" x2="${right.toFixed(2)}" y2="${(dimY + 4).toFixed(2)}"/>`,
    `<line x1="${left.toFixed(2)}" y1="${dimY.toFixed(2)}" x2="${right.toFixed(2)}" y2="${dimY.toFixed(2)}"/>`,
    `<line x1="${left.toFixed(2)}" y1="${(dimY - tick).toFixed(2)}" x2="${left.toFixed(2)}" y2="${(dimY + tick).toFixed(2)}"/>`,
    `<line x1="${right.toFixed(2)}" y1="${(dimY - tick).toFixed(2)}" x2="${right.toFixed(2)}" y2="${(dimY + tick).toFixed(2)}"/>`,
    `<line x1="${(left - 4).toFixed(2)}" y1="${top.toFixed(2)}" x2="${(dimX - 4).toFixed(2)}" y2="${top.toFixed(2)}"/>`,
    `<line x1="${(left - 4).toFixed(2)}" y1="${bottom.toFixed(2)}" x2="${(dimX - 4).toFixed(2)}" y2="${bottom.toFixed(2)}"/>`,
    `<line x1="${dimX.toFixed(2)}" y1="${top.toFixed(2)}" x2="${dimX.toFixed(2)}" y2="${bottom.toFixed(2)}"/>`,
    `<line x1="${(dimX - tick).toFixed(2)}" y1="${top.toFixed(2)}" x2="${(dimX + tick).toFixed(2)}" y2="${top.toFixed(2)}"/>`,
    `<line x1="${(dimX - tick).toFixed(2)}" y1="${bottom.toFixed(2)}" x2="${(dimX + tick).toFixed(2)}" y2="${bottom.toFixed(2)}"/>`,
    `</g>`,
    `<text x="${((left + right) / 2).toFixed(2)}" y="${(dimY - 5).toFixed(2)}" text-anchor="middle" font-size="10" fill="#475569">${widthMm} mm</text>`,
    `<text x="${(dimX - 8).toFixed(2)}" y="${((top + bottom) / 2).toFixed(2)}" text-anchor="middle" font-size="10" fill="#475569" transform="rotate(-90 ${(dimX - 8).toFixed(2)} ${((top + bottom) / 2).toFixed(2)})">${heightMm} mm</text>`,
  ].join('');
}

function axisMarkup(panel: Panel, basis: Basis): string {
  const ox = panel.x + 26;
  const oy = panel.y + panel.h - 26;
  const length = 25;
  const axes: { vec: Vec3; color: string; label: string }[] = [
    { vec: [1, 0, 0], color: '#B91C1C', label: 'X' },
    { vec: [0, 1, 0], color: '#15803D', label: 'Y' },
    { vec: [0, 0, 1], color: '#1D4ED8', label: 'Z' },
  ];
  const parts = [`<g stroke-width="1.4">`];
  for (const axis of axes) {
    const x = ox + dot(axis.vec, basis.right) * length;
    const y = oy - dot(axis.vec, basis.up) * length;
    parts.push(`<line x1="${ox}" y1="${oy}" x2="${x.toFixed(2)}" y2="${y.toFixed(2)}" stroke="${axis.color}"/>`);
    parts.push(
      `<text x="${(x + (x >= ox ? 4 : -10)).toFixed(2)}" y="${(y + (y >= oy ? 10 : -4)).toFixed(2)}" font-size="9" fill="${axis.color}">${axis.label}</text>`,
    );
  }
  parts.push(`<circle cx="${ox}" cy="${oy}" r="2" fill="#475569"/></g>`);
  return parts.join('');
}

function renderPanel(
  mesh: MeshData,
  geometry: ViewGeometry,
  panel: Panel,
  scale: number,
  options: ProjectionOptions,
): { body: string; cx: number; cy: number } {
  const cx = panel.x + panel.w / 2;
  const cy = panel.y + panel.h / 2;
  const buffer = fillDepthBuffer(mesh, geometry, panel, scale, cx, cy);
  const lines = buildLineSet(mesh, geometry, panel, scale, cx, cy, buffer, options.hidden !== false);
  const body = [
    `<rect x="${panel.x}" y="${panel.y}" width="${panel.w}" height="${panel.h}" fill="#FFFFFF" stroke="#E2E8F0" stroke-width="0.8"/>`,
    `<text x="${panel.x + 12}" y="${panel.y + 20}" font-size="11" font-weight="600" fill="#334155">${escapeXml(panel.label)}</text>`,
    escapeXml(''),
    pathMarkup(lines.hidden, true),
    pathMarkup(lines.visible, false),
    options.dimensions !== false ? dimensionMarkup(panel, scale, geometry, cx, cy) : '',
    axisMarkup(panel, geometry.basis),
  ].join('');
  return { body, cx, cy };
}

function titleMarkup(meta: ProjectionMeta, width: number, height: number, generatedAt: string, viewLabel: string): string {
  const x = 40;
  const y = height - 74;
  const w = width - 80;
  const h = 54;
  return [
    `<g>`,
    `<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="#F8FAFC" stroke="#CBD5E1" stroke-width="0.8"/>`,
    `<line x1="${x}" y1="${y + 26}" x2="${x + w}" y2="${y + 26}" stroke="#E2E8F0" stroke-width="0.8"/>`,
    `<text x="${x + 10}" y="${y + 17}" font-size="12" font-weight="700" fill="#0F172A">${escapeXml(meta.name)}</text>`,
    `<text x="${x + w - 10}" y="${y + 17}" text-anchor="end" font-size="10" fill="#475569">${escapeXml(viewLabel)} · RCIF v1.4.2 · 单位 mm · 第一角投影</text>`,
    `<text x="${x + 10}" y="${y + 42}" font-size="9.5" fill="#64748B">族 ID：${escapeXml(meta.familyId)} · 版本：${escapeXml(meta.revision)} · 语义码：${escapeXml(meta.semanticCode)}</text>`,
    `<text x="${x + w - 10}" y="${y + 42}" text-anchor="end" font-size="9" font-family="ui-monospace,monospace" fill="#64748B">cacheKey ${escapeXml(meta.cacheKey.slice(0, 16))}… · ${escapeXml(generatedAt)}</text>`,
    `</g>`,
  ].join('');
}

function singleViewSvg(
  mesh: MeshData,
  geometry: ViewGeometry,
  meta: ProjectionMeta,
  options: ProjectionOptions,
): string {
  const width = 900;
  const height = 650;
  const panel: Panel = { x: 40, y: 66, w: width - 80, h: height - 150, label: VIEW_LABELS[geometry.view] };
  const innerW = panel.w - 120;
  const innerH = panel.h - 80;
  const spanU = Math.max(geometry.maxU - geometry.minU, 1);
  const spanV = Math.max(geometry.maxV - geometry.minV, 1);
  const scale = Math.min(innerW / spanU, innerH / spanV);
  const rendered = renderPanel(mesh, geometry, panel, scale, options);
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="${escapeXml(meta.name)} 二维投影图">`,
    `<rect width="${width}" height="${height}" fill="#FFFFFF"/>`,
    `<text x="40" y="38" font-size="18" font-weight="700" fill="#0F172A">${escapeXml(meta.name)} · 二维投影图</text>`,
    `<text x="${width - 40}" y="38" text-anchor="end" font-size="10" fill="#64748B">RCIF COMPONENT FOUNDATION · AUTO GENERATED</text>`,
    rendered.body,
    titleMarkup(meta, width, height, options.generatedAt ?? new Date().toISOString().slice(0, 10), VIEW_LABELS[geometry.view]),
    `</svg>`,
  ].join('');
}

export function renderProjectionSvg(
  mesh: MeshData,
  meta: ProjectionMeta,
  options: ProjectionOptions = {},
): string {
  const view = options.view ?? 'sheet';
  const generatedAt = options.generatedAt ?? new Date().toISOString().slice(0, 10);
  if (view !== 'sheet') {
    return singleViewSvg(mesh, buildViewGeometry(mesh, view), meta, { ...options, generatedAt });
  }

  const width = 1240;
  const height = 900;
  const panels: Record<Exclude<ProjectionView, 'sheet'>, Panel> = {
    front: { x: 40, y: 70, w: 560, h: 340, label: VIEW_LABELS.front },
    top: { x: 40, y: 430, w: 560, h: 320, label: VIEW_LABELS.top },
    side: { x: 630, y: 70, w: 270, h: 340, label: VIEW_LABELS.side },
    iso: { x: 630, y: 430, w: 570, h: 320, label: VIEW_LABELS.iso },
  };
  const geometries: Record<Exclude<ProjectionView, 'sheet'>, ViewGeometry> = {
    front: buildViewGeometry(mesh, 'front'),
    top: buildViewGeometry(mesh, 'top'),
    side: buildViewGeometry(mesh, 'side'),
    iso: buildViewGeometry(mesh, 'iso'),
  };
  // 每个视图独立适配面板，避免 18m 高墩把俯视图压成几像素
  const body = (Object.keys(panels) as Exclude<ProjectionView, 'sheet'>[])
    .map((key) => {
      const panel = panels[key];
      const geometry = geometries[key];
      const panelScale = Math.min(
        (panel.w - 130) / Math.max(geometry.maxU - geometry.minU, 1),
        (panel.h - 78) / Math.max(geometry.maxV - geometry.minV, 1),
      );
      return renderPanel(mesh, geometry, panel, panelScale, options).body;
    })
    .join('');
  return [
    `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="${escapeXml(meta.name)} 三视图投影">`,
    `<rect width="${width}" height="${height}" fill="#FFFFFF"/>`,
    `<text x="40" y="38" font-size="19" font-weight="700" fill="#0F172A">${escapeXml(meta.name)} · 二维投影图（三视图 + 轴测）</text>`,
    `<text x="${width - 40}" y="38" text-anchor="end" font-size="10" fill="#64748B">RCIF COMPONENT FOUNDATION · FIRST-ANGLE PROJECTION · AUTO GENERATED</text>`,
    `<line x1="40" y1="50" x2="${width - 40}" y2="50" stroke="#0F172A" stroke-width="1.2"/>`,
    body,
    titleMarkup(meta, width, height, generatedAt, '三视图 + 轴测'),
    `</svg>`,
  ].join('');
}
