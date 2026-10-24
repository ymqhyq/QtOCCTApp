import * as THREE from 'three';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';
import type { DecodedJhb, JhbHeader } from '@/jhb/decode';

export interface NamedPartInput {
  key: string;
  label: string;
  node?: string;
  instances?: string[];
}

/**
 * 将上传包内的 preview.glb 转换为 CadViewport 可消费的 DecodedJhb 形状。
 * 该对象只用于诊断预览；体积/面积保留 GLB 三角网格统计。
 */
export async function decodeGlbAsJhb(
  buffer: ArrayBuffer,
  parts: NamedPartInput[],
  meta: { cacheKey?: string; name?: string },
): Promise<DecodedJhb> {
  const loader = new GLTFLoader();
  const gltf = await new Promise<{ scene: THREE.Group }>((resolve, reject) => {
    loader.parse(buffer, '', (result) => resolve(result as { scene: THREE.Group }), reject);
  });
  const named = parts.length ? parts : [{ key: 'ImportedBody', label: '导入实体' }];
  const indexByNode = new Map<string, number>();
  named.forEach((part, index) => {
    for (const key of [part.key, part.node, ...(part.instances ?? [])].filter(Boolean) as string[]) {
      indexByNode.set(key.toLowerCase(), index);
      indexByNode.set(key.split('/').pop()!.toLowerCase(), index);
    }
  });
  const resolveIndex = (object: THREE.Object3D): number => {
    let current: THREE.Object3D | null = object;
    while (current) {
      const found = indexByNode.get(current.name.toLowerCase());
      if (found !== undefined) return found;
      current = current.parent;
    }
    return 0;
  };

  const positions: number[] = [];
  const normals: number[] = [];
  const indices: number[] = [];
  const faceIds: number[] = [];
  const faceToShape: number[] = [];
  let triangleCount = 0;
  gltf.scene.updateMatrixWorld(true);

  gltf.scene.traverse((object) => {
    const mesh = object as THREE.Mesh;
    if (!mesh.isMesh || !mesh.geometry) return;
    const shapeIndex = resolveIndex(mesh);
    const geometry = mesh.geometry.clone();
    geometry.applyMatrix4(mesh.matrixWorld);
    if (!geometry.getAttribute('normal')) geometry.computeVertexNormals();
    const position = geometry.getAttribute('position');
    const normal = geometry.getAttribute('normal');
    const index = geometry.getIndex();
    const baseVertex = positions.length / 3;
    for (let i = 0; i < position.count; i += 1) {
      positions.push(position.getX(i), position.getY(i), position.getZ(i));
      normals.push(normal.getX(i), normal.getY(i), normal.getZ(i));
    }
    const triIndices: number[] = [];
    if (index) {
      for (let i = 0; i < index.count; i += 1) triIndices.push(baseVertex + index.getX(i));
    } else {
      for (let i = 0; i < position.count; i += 1) triIndices.push(baseVertex + i);
    }
    for (let i = 0; i + 2 < triIndices.length; i += 3) {
      faceIds.push(triangleCount);
      faceToShape.push(shapeIndex);
      triangleCount += 1;
    }
    indices.push(...triIndices);
    geometry.dispose();
  });

  const positionArray = new Float32Array(positions);
  const normalArray = new Float32Array(normals);
  const indexArray = new Uint32Array(indices);
  const faceIdArray = faceIds.length > 65535
    ? new Uint32Array(faceIds)
    : new Uint16Array(faceIds);
  const bounds = { min: [Infinity, Infinity, Infinity] as [number, number, number], max: [-Infinity, -Infinity, -Infinity] as [number, number, number] };
  for (let i = 0; i < positionArray.length; i += 3) {
    bounds.min[0] = Math.min(bounds.min[0], positionArray[i]);
    bounds.min[1] = Math.min(bounds.min[1], positionArray[i + 1]);
    bounds.min[2] = Math.min(bounds.min[2], positionArray[i + 2]);
    bounds.max[0] = Math.max(bounds.max[0], positionArray[i]);
    bounds.max[1] = Math.max(bounds.max[1], positionArray[i + 1]);
    bounds.max[2] = Math.max(bounds.max[2], positionArray[i + 2]);
  }
  if (!Number.isFinite(bounds.min[0])) {
    bounds.min = [0, 0, 0];
    bounds.max = [1, 1, 1];
  }

  const buffers = {
    vertices: { offset: 0, length: positionArray.byteLength, type: 'float32' as const, stride: 3 },
    indices: { offset: 0, length: indexArray.byteLength, type: 'uint32' as const, stride: 1 },
    normals: { offset: 0, length: normalArray.byteLength, type: 'float32' as const, stride: 3 },
    edges: { offset: 0, length: 0, type: 'float32' as const, stride: 3 },
    edgeOffsets: { offset: 0, length: 0, type: 'uint32' as const, stride: 1 },
    faceIds: { offset: 0, length: faceIdArray.byteLength, type: faceIds.length > 65535 ? 'uint32' as const : 'uint16' as const, stride: 1 },
  };
  const header: JhbHeader = {
    protocolVersion: 'GLB',
    status: 'ok',
    cache_key: meta.cacheKey ?? '',
    format: 'glb-preview',
    payloadKind: 'UploadedGlb',
    shapeKeys: named.map((part) => part.key),
    vertexCount: positionArray.length / 3,
    triangleCount,
    edgeLineVertexCount: 0,
    edgesCount: 0,
    facesCount: named.length,
    faceToShape,
    edgeToShape: [],
    buffers,
    bounds,
    stats: { triangles: triangleCount, facesCount: named.length, volumeM3: 0 },
    outputs: Object.fromEntries(named.map((part) => [part.key, { geomType: 'solid', volume: 0 }])),
    edges: [],
    cacheHit: false,
  };
  return {
    header,
    positions: positionArray,
    indices: indexArray,
    normals: normalArray,
    faceIds: faceIdArray,
    edges: new Float32Array(0),
    edgeOffsets: new Uint32Array([0]),
    zeroCopy: false,
    aligned: true,
    jsonLen: 0,
    payloadStart: 0,
    baseOffset: 0,
    targetBuffer: new ArrayBuffer(0),
    sourceBuffer: new ArrayBuffer(0),
  };
}
