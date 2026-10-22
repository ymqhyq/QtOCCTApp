import { spawn } from 'node:child_process';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';
import type { MeshData } from './geometry';

interface CqMeshPayload {
  shapeKeys: string[];
  positions: number[];
  indices: number[];
  normals: number[];
  faceIds: number[];
  faceGroups: { faceId: number; shapeIndex: number; areaMm2: number }[];
  faceToShape: number[];
  bounds: { min: [number, number, number]; max: [number, number, number] };
  volumeMm3: number;
  areaMm2: number;
  vertices: number;
  triangles: number;
  facesCount: number;
}

function pythonPath() {
  if (process.env.CQ_PYTHON) return process.env.CQ_PYTHON;
  const local = resolve(process.cwd(), '.venv-cq/Scripts/python.exe');
  return existsSync(local) ? local : '';
}

let cqUnavailable = false;

/**
 * Runs the authoritative CadQuery generator for the abutment.
 * Returns null when CadQuery is not installed in the current environment,
 * in which case the API falls back to the packaged preview mesh.
 */
export async function generateAbutmentMesh(params: Record<string, number | string | boolean>): Promise<MeshData | null> {
  const python = pythonPath();
  if (!python || cqUnavailable) return null;
  const worker = resolve(process.cwd(), 'server/cq/abutment_worker.py');
  if (!existsSync(worker)) return null;

  return new Promise((resolvePromise) => {
    const child = spawn(python, [worker], {
      cwd: process.cwd(),
      stdio: ['pipe', 'pipe', 'pipe'],
      windowsHide: true,
    });
    let stdout = '';
    let stderr = '';
    const timer = setTimeout(() => {
      child.kill();
      resolvePromise(null);
    }, 60_000);
    child.stdout.on('data', (chunk) => {
      stdout += String(chunk);
    });
    child.stderr.on('data', (chunk) => {
      stderr += String(chunk);
    });
    child.on('error', () => {
      clearTimeout(timer);
      cqUnavailable = true;
      resolvePromise(null);
    });
    child.on('close', (code) => {
      clearTimeout(timer);
      if (code !== 0 || !stdout.trim()) {
        if (stderr) console.warn('[cadquery]', stderr.slice(0, 600));
        cqUnavailable = true;
        resolvePromise(null);
        return;
      }
      try {
        const parsed = JSON.parse(stdout) as CqMeshPayload;
        resolvePromise({
          positions: Float32Array.from(parsed.positions),
          indices: Uint32Array.from(parsed.indices),
          normals: Float32Array.from(parsed.normals),
          faceIds: Uint16Array.from(parsed.faceIds),
          edges: new Float32Array(0),
          edgeOffsets: new Uint32Array([0]),
          edgeShapes: [],
          faceGroups: parsed.faceGroups.map((group) => ({
            ...group,
            label: 'CadQuery 拓扑面',
            triangles: 1,
            centroid: [0, 0, 0] as [number, number, number],
          })),
          edgeLines: [],
          bounds: parsed.bounds,
          volumeMm3: parsed.volumeMm3,
          areaMm2: parsed.areaMm2,
          vertices: parsed.vertices,
          triangles: parsed.triangles,
          facesCount: parsed.facesCount,
          edgesCount: 0,
        });
      } catch (error) {
        console.warn('[cadquery] invalid worker output', error);
        resolvePromise(null);
      }
    });
    child.stdin.end(JSON.stringify({ params }));
  });
}
