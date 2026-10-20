/**
 * JHB 双通道对齐解码引擎（报告 §5.3 / 规范 v1.4.3）
 *
 * 通道一：标准包 payloadStart % 4 === 0，直接在同一个 jhbBuffer 上构造 TypedArray 视图（零拷贝）
 * 通道二：遗留未 padding 包，完整拷贝真实载荷至新对齐 Buffer（baseOffset = 0），绝不静默丢字节
 *
 * 严禁使用 `(payloadStart + 3) & ~3` 之类的位运算自愈。
 */

export interface JhbBufferMeta {
  offset: number;
  length: number;
  type: 'float32' | 'uint32' | 'uint16' | 'uint8';
  stride: number;
}

export interface JhbEdgeMeta {
  shape: number;
  closed: boolean;
  label?: string;
  start: number;
  count: number;
}

export interface JhbHeader {
  protocolVersion: string;
  status: string;
  cache_key: string;
  format: string;
  payloadKind: string;
  shapeKeys: string[];
  vertexCount: number;
  triangleCount: number;
  edgeLineVertexCount: number;
  edgesCount: number;
  facesCount: number;
  faceToShape: number[];
  edgeToShape: number[];
  buffers: {
    vertices: JhbBufferMeta;
    indices: JhbBufferMeta;
    normals: JhbBufferMeta;
    edges: JhbBufferMeta;
    edgeOffsets: JhbBufferMeta;
    faceIds: JhbBufferMeta;
  };
  bounds: { min: [number, number, number]; max: [number, number, number] };
  stats?: Record<string, number>;
  outputs?: Record<string, Record<string, unknown>>;
  args?: Record<string, number | string>;
  anchors?: Record<string, { origin: number[]; axis: number[]; refDirection?: number[] }>;
  edges?: JhbEdgeMeta[];
  cacheHit?: boolean;
}

export interface DecodedJhb {
  header: JhbHeader;
  positions: Float32Array;
  indices: Uint32Array;
  normals: Float32Array;
  faceIds: Uint16Array | Uint32Array;
  edges: Float32Array;
  edgeOffsets: Uint32Array;
  /** 是否走了零拷贝通道一 */
  zeroCopy: boolean;
  aligned: boolean;
  jsonLen: number;
  payloadStart: number;
  baseOffset: number;
  targetBuffer: ArrayBuffer;
  sourceBuffer: ArrayBuffer;
}

const TYPE_CTORS = {
  float32: Float32Array,
  uint32: Uint32Array,
  uint16: Uint16Array,
  uint8: Uint8Array,
} as const;

export class JhbDecodeError extends Error {
  constructor(
    message: string,
    readonly code: 'INVALID_FRAME' | 'OUT_OF_BOUNDS' | 'UNALIGNED_VIEW' | 'MISSING_BUFFER',
  ) {
    super(message);
    this.name = 'JhbDecodeError';
  }
}

function view<T extends ArrayBufferView>(
  target: ArrayBuffer,
  baseOffset: number,
  meta: JhbBufferMeta,
  field: string,
): T {
  const ctor = TYPE_CTORS[meta.type];
  if (!ctor) {
    throw new JhbDecodeError(`缓冲区 ${field} 类型非法: ${meta.type}`, 'MISSING_BUFFER');
  }
  const bytesPerElement = ctor.BYTES_PER_ELEMENT;
  const absolute = baseOffset + meta.offset;
  if (absolute + meta.length > target.byteLength) {
    throw new JhbDecodeError(
      `缓冲区 ${field} 越界: ${absolute}+${meta.length} > ${target.byteLength}`,
      'OUT_OF_BOUNDS',
    );
  }
  if (absolute % bytesPerElement !== 0 || meta.length % bytesPerElement !== 0) {
    throw new JhbDecodeError(
      `缓冲区 ${field} 未按 ${bytesPerElement} 字节对齐 (offset=${absolute})`,
      'UNALIGNED_VIEW',
    );
  }
  return new ctor(target, absolute, meta.length / bytesPerElement) as unknown as T;
}

export function decodeJhb(jhbBuffer: ArrayBuffer): DecodedJhb {
  if (jhbBuffer.byteLength < 8) {
    throw new JhbDecodeError('JHB 帧长度不足 8 字节', 'INVALID_FRAME');
  }
  const dataView = new DataView(jhbBuffer);
  const jsonLen = dataView.getUint32(0, true);
  if (jsonLen <= 0 || 4 + jsonLen > jhbBuffer.byteLength) {
    throw new JhbDecodeError(`JHB json_len 非法: ${jsonLen}`, 'INVALID_FRAME');
  }
  const headerText = new TextDecoder('utf-8').decode(new Uint8Array(jhbBuffer, 4, jsonLen));
  let header: JhbHeader;
  try {
    header = JSON.parse(headerText) as JhbHeader;
  } catch {
    throw new JhbDecodeError('JHB JSON Header 解析失败', 'INVALID_FRAME');
  }
  const required = ['vertices', 'indices', 'normals', 'edges', 'edgeOffsets', 'faceIds'] as const;
  for (const key of required) {
    if (!header.buffers?.[key]) {
      throw new JhbDecodeError(`JHB Header 缺少 buffers.${key}`, 'MISSING_BUFFER');
    }
  }

  const payloadStart = 4 + jsonLen;
  const aligned = payloadStart % 4 === 0;
  let targetBuffer = jhbBuffer;
  let baseOffset = payloadStart;

  if (!aligned) {
    const payloadByteLength = jhbBuffer.byteLength - payloadStart;
    const alignedBuffer = new ArrayBuffer(payloadByteLength);
    new Uint8Array(alignedBuffer).set(new Uint8Array(jhbBuffer, payloadStart, payloadByteLength));
    targetBuffer = alignedBuffer;
    baseOffset = 0;
  }

  const positions = view<Float32Array>(targetBuffer, baseOffset, header.buffers.vertices, 'vertices');
  const indices = view<Uint32Array>(targetBuffer, baseOffset, header.buffers.indices, 'indices');
  const normals = view<Float32Array>(targetBuffer, baseOffset, header.buffers.normals, 'normals');
  const edges = view<Float32Array>(targetBuffer, baseOffset, header.buffers.edges, 'edges');
  const edgeOffsets = view<Uint32Array>(targetBuffer, baseOffset, header.buffers.edgeOffsets, 'edgeOffsets');
  const faceIds = view<Uint16Array | Uint32Array>(targetBuffer, baseOffset, header.buffers.faceIds, 'faceIds');

  // 元素长度除法换算自检（断言 4）
  const faceIdType = header.buffers.faceIds.type;
  if (faceIdType !== 'uint16' && faceIdType !== 'uint32') {
    throw new JhbDecodeError(`buffers.faceIds.type 非法: ${faceIdType}`, 'MISSING_BUFFER');
  }
  const faceIdBytes = faceIdType === 'uint32' ? 4 : 2;
  const checks: [string, number, number][] = [
    ['vertices', positions.length, header.buffers.vertices.length / 4],
    ['indices', indices.length, header.buffers.indices.length / 4],
    ['normals', normals.length, header.buffers.normals.length / 4],
    ['edges', edges.length, header.buffers.edges.length / 4],
    ['edgeOffsets', edgeOffsets.length, header.buffers.edgeOffsets.length / 4],
    ['faceIds', faceIds.length, header.buffers.faceIds.length / faceIdBytes],
  ];
  for (const [name, actual, expected] of checks) {
    if (actual !== expected) {
      throw new JhbDecodeError(`缓冲区 ${name} 元素数不匹配: ${actual} != ${expected}`, 'OUT_OF_BOUNDS');
    }
  }
  if (typeof header.triangleCount === 'number' && faceIds.length !== header.triangleCount) {
    throw new JhbDecodeError(
      `faceIds 数量 ${faceIds.length} 与 triangleCount ${header.triangleCount} 不一致`,
      'OUT_OF_BOUNDS',
    );
  }

  return {
    header,
    positions,
    indices,
    normals,
    faceIds,
    edges,
    edgeOffsets,
    zeroCopy: aligned,
    aligned,
    jsonLen,
    payloadStart,
    baseOffset,
    targetBuffer,
    sourceBuffer: jhbBuffer,
  };
}
