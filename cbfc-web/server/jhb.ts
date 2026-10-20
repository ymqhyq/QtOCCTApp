/**
 * JHB (JSON-Header-Binary) v1.4.3 编码器
 * - JSON Header 尾部以 ASCII 空格填充至 4 字节边界
 * - json_len 记录含填充的完整头部长度（恒为 4 的倍数）
 * - payloadStart = 4 + jsonLen 天然满足 4 字节对齐，Web 端可零拷贝构造视图
 */
import type { MeshData } from './geometry';

export interface JhbEncodeInput {
  mesh: MeshData;
  cacheKey: string;
  shapeKeys: string[];
  faceToShape: number[];
  edgeToShape: number[];
  outputs: Record<string, Record<string, number | string | number[]>>;
  args: Record<string, number | string>;
  anchors?: Record<string, { origin: number[]; axis: number[]; refDirection?: number[] }>;
  /** 演示遗留包：不填充头部，强制走前端通道二对齐拷贝降级 */
  legacyUnaligned?: boolean;
  cacheHit?: boolean;
  elapsedMs?: number;
  protocolVersion?: string;
}

const enc = new TextEncoder();

export interface JhbEncodeResult {
  buffer: ArrayBuffer;
  header: Record<string, unknown>;
  jsonLen: number;
  payloadStart: number;
  aligned: boolean;
}

function padTo4(text: string): string {
  return text + ' '.repeat((4 - (enc.encode(text).length % 4)) % 4);
}

export function encodeJhb(input: JhbEncodeInput): JhbEncodeResult {
  const { mesh } = input;
  const faceIdBytes = mesh.facesCount > 65535 ? 4 : 2;
  const faceIdType = mesh.facesCount > 65535 ? 'uint32' : 'uint16';

  const buffers = {
    vertices: { offset: 0, length: mesh.positions.byteLength, type: 'float32', stride: 3 },
    indices: { offset: 0, length: mesh.indices.byteLength, type: 'uint32', stride: 1 },
    normals: { offset: 0, length: mesh.normals.byteLength, type: 'float32', stride: 3 },
    edges: { offset: 0, length: mesh.edges.byteLength, type: 'float32', stride: 3 },
    edgeOffsets: { offset: 0, length: mesh.edgeOffsets.byteLength, type: 'uint32', stride: 1 },
    faceIds: { offset: 0, length: mesh.faceIds.byteLength, type: faceIdType, stride: 1 },
  };

  let offset = 0;
  for (const key of ['vertices', 'indices', 'normals', 'edges', 'edgeOffsets', 'faceIds'] as const) {
    buffers[key].offset = offset;
    offset += buffers[key].length;
  }

  const header: Record<string, unknown> = {
    protocolVersion: input.protocolVersion ?? '1.4.3',
    status: 'ok',
    cache_key: input.cacheKey,
    format: 'cbf-jhb',
    payloadKind: 'TopologicalMesh',
    shapeKeys: input.shapeKeys,
    vertexCount: mesh.vertices,
    triangleCount: mesh.triangles,
    edgeLineVertexCount: mesh.edges.length / 3,
    edgesCount: mesh.edgesCount,
    facesCount: mesh.facesCount,
    faceToShape: input.faceToShape,
    edgeToShape: input.edgeToShape,
    buffers,
    bounds: { min: mesh.bounds.min, max: mesh.bounds.max },
    stats: {
      volumeM3: round(mesh.volumeMm3 / 1e9, 3),
      surfaceM2: round(mesh.areaMm2 / 1e6, 3),
      triangles: mesh.triangles,
      edgesCount: mesh.edgesCount,
      facesCount: mesh.facesCount,
      elapsedMs: input.elapsedMs ?? 42,
    },
    outputs: input.outputs,
    args: input.args,
    anchors: input.anchors ?? {},
    edges: mesh.edgeLines.map((e) => ({
      shape: e.shapeIndex,
      closed: e.closed,
      label: e.label,
      start: e.vertexStart,
      count: e.vertexCount,
    })),
    cacheHit: Boolean(input.cacheHit),
  };

  const rawText = JSON.stringify(header);
  const text = input.legacyUnaligned ? rawText : padTo4(rawText);
  const headerBytes = enc.encode(text);
  const payloadStart = 4 + headerBytes.length;

  const total = payloadStart + offset;
  const out = new ArrayBuffer(total);
  const view = new DataView(out);
  const bytes = new Uint8Array(out);
  view.setUint32(0, headerBytes.length, true);
  bytes.set(headerBytes, 4);

  const payloads = [
    new Uint8Array(mesh.positions.buffer, mesh.positions.byteOffset, mesh.positions.byteLength),
    new Uint8Array(mesh.indices.buffer, mesh.indices.byteOffset, mesh.indices.byteLength),
    new Uint8Array(mesh.normals.buffer, mesh.normals.byteOffset, mesh.normals.byteLength),
    new Uint8Array(mesh.edges.buffer, mesh.edges.byteOffset, mesh.edges.byteLength),
    new Uint8Array(mesh.edgeOffsets.buffer, mesh.edgeOffsets.byteOffset, mesh.edgeOffsets.byteLength),
    new Uint8Array(mesh.faceIds.buffer, mesh.faceIds.byteOffset, mesh.faceIds.byteLength),
  ];
  let cursor = payloadStart;
  for (const payload of payloads) {
    bytes.set(payload, cursor);
    cursor += payload.byteLength;
  }

  return {
    buffer: out,
    header,
    jsonLen: headerBytes.length,
    payloadStart,
    aligned: payloadStart % 4 === 0,
  };
}

function round(value: number, digits: number): number {
  const f = 10 ** digits;
  return Math.round(value * f) / f;
}
