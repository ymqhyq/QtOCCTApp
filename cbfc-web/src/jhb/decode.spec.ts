import { describe, expect, it } from 'vitest';
import { decodeJhb } from './decode';
import { buildMesh } from '../../server/geometry';
import { encodeJhb } from '../../server/jhb';

function makeFrame(legacy = false) {
  const mesh = buildMesh('box', { LengthX: 2000, WidthY: 1200, HeightZ: 800, Taper: 0 });
  const frame = encodeJhb({
    mesh,
    cacheKey: 'a'.repeat(64),
    shapeKeys: ['BodySolid', 'TopFace', 'BaseContourWire/1'],
    faceToShape: mesh.faceGroups.map(() => 0),
    edgeToShape: mesh.edgeLines.map(() => 0),
    outputs: {},
    args: {},
    legacyUnaligned: legacy,
  });
  return { mesh, frame };
}

/** 构造 json_len 为 4k+1（未对齐）的遗留未 padding 帧 */
function makeLegacy1001Frame() {
  const { mesh, frame } = makeFrame(false);
  const header = JSON.parse(
    new TextDecoder().decode(new Uint8Array(frame.buffer, 4, frame.jsonLen)),
  );
  header.pad = '';
  let text = JSON.stringify(header);
  const baseLen = new TextEncoder().encode(text).length;
  const target = baseLen + ((1 - (baseLen % 4) + 4) % 4);
  header.pad = 'x'.repeat(Math.max(0, target - baseLen));
  text = JSON.stringify(header);
  const bytes = new TextEncoder().encode(text);
  expect(bytes.length).toBe(target);
  expect(bytes.length % 4).not.toBe(0);
  const payload = new Uint8Array(frame.buffer, frame.payloadStart);
  const out = new ArrayBuffer(4 + bytes.length + payload.byteLength);
  const dv = new DataView(out);
  dv.setUint32(0, bytes.length, true);
  new Uint8Array(out).set(bytes, 4);
  new Uint8Array(out).set(payload, 4 + bytes.length);
  return { mesh, buffer: out, payloadLen: payload.byteLength, jsonLen: bytes.length };
}

describe('JHB 头部完整性守卫', () => {
  function rebuild(mutate: (header: any) => void) {
    const { frame } = makeFrame(false);
    const header = JSON.parse(
      new TextDecoder().decode(new Uint8Array(frame.buffer, 4, frame.jsonLen)),
    );
    mutate(header);
    let json = JSON.stringify(header);
    const encoded = () => new TextEncoder().encode(json);
    const padding = (4 - (encoded().length % 4)) % 4;
    json += ' '.repeat(padding);
    const bytes = encoded();
    const payload = new Uint8Array(frame.buffer, frame.payloadStart);
    const out = new ArrayBuffer(4 + bytes.length + payload.byteLength);
    new DataView(out).setUint32(0, bytes.length, true);
    new Uint8Array(out).set(bytes, 4);
    new Uint8Array(out).set(payload, 4 + bytes.length);
    return out;
  }

  it('faceIds 类型必须为 uint16 / uint32', () => {
    const buffer = rebuild((header) => {
      header.buffers.faceIds.type = 'uint8';
    });
    expect(() => decodeJhb(buffer)).toThrow(/faceIds/);
  });

  it('faceIds 字节长度必须与三角形数量严格一致', () => {
    const buffer = rebuild((header) => {
      header.buffers.faceIds.length -= 2;
    });
    expect(() => decodeJhb(buffer)).toThrow(/faceIds|元素数/);
  });

  it('faceIds 元素数量必须等于 triangleCount', () => {
    const buffer = rebuild((header) => {
      header.triangleCount += 1;
    });
    expect(() => decodeJhb(buffer)).toThrow(/triangleCount/);
  });
});

describe('JHB 双通道对齐解码引擎 · 五条测试守卫', () => {
  it('断言 1：标准包零拷贝对齐安全性', () => {
    const { frame } = makeFrame(false);
    const decoded = decodeJhb(frame.buffer);
    expect(decoded.payloadStart).toBe(4 + decoded.jsonLen);
    expect(decoded.payloadStart % 4).toBe(0);
    expect(decoded.targetBuffer).toBe(frame.buffer);
    expect(decoded.zeroCopy).toBe(true);
    expect(decoded.baseOffset).toBe(decoded.payloadStart);
  });

  it('断言 2：遗留未对齐包安全降级，绝无字节丢失', () => {
    const { buffer, payloadLen, jsonLen } = makeLegacy1001Frame();
    const decoded = decodeJhb(buffer);
    expect(decoded.jsonLen).toBe(jsonLen);
    expect(decoded.payloadStart).toBe(4 + jsonLen);
    expect(decoded.payloadStart % 4).not.toBe(0);
    expect(decoded.aligned).toBe(false);
    expect(decoded.zeroCopy).toBe(false);
    expect(decoded.baseOffset).toBe(0);
    expect(decoded.targetBuffer.byteLength).toBe(buffer.byteLength - (4 + jsonLen));
    expect(decoded.targetBuffer.byteLength).toBe(payloadLen);
  });

  it('断言 3：缓冲区绝对边界安全性（无 RangeError）', () => {
    const { frame } = makeFrame(false);
    const decoded = decodeJhb(frame.buffer);
    const target = decoded.targetBuffer;
    for (const meta of Object.values(decoded.header.buffers)) {
      expect(decoded.baseOffset + meta.offset + meta.length).toBeLessThanOrEqual(target.byteLength);
    }
  });

  it('断言 4：元素长度除法换算吻合', () => {
    const { frame } = makeFrame(false);
    const decoded = decodeJhb(frame.buffer);
    expect(decoded.positions.length).toBe(decoded.header.buffers.vertices.length / 4);
    expect(decoded.indices.length).toBe(decoded.header.buffers.indices.length / 4);
    expect(decoded.normals.length).toBe(decoded.header.buffers.normals.length / 4);
    expect(decoded.edges.length).toBe(decoded.header.buffers.edges.length / 4);
    expect(decoded.edgeOffsets.length).toBe(decoded.header.buffers.edgeOffsets.length / 4);
    expect(decoded.faceIds.length).toBe(
      decoded.header.buffers.faceIds.length /
        (decoded.header.buffers.faceIds.type === 'uint32' ? 4 : 2),
    );
  });

  it('断言 5：首顶点与首三角形真实几何数值吻合', () => {
    const { mesh, frame } = makeFrame(false);
    const decoded = decodeJhb(frame.buffer);
    expect([decoded.positions[0], decoded.positions[1], decoded.positions[2]]).toEqual([
      mesh.positions[0],
      mesh.positions[1],
      mesh.positions[2],
    ]);
    const firstTriangle = Array.from(decoded.indices.slice(0, 3));
    expect(firstTriangle).toEqual([0, 1, 2]);
    expect(decoded.positions[0]).toBe(-1000);
    expect(decoded.positions[2]).toBe(0);
  });
});
