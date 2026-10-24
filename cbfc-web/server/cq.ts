import { spawn, type ChildProcessWithoutNullStreams } from 'node:child_process';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { createInterface } from 'node:readline';
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
  error?: string;
}

interface CqJob {
  payload: Record<string, unknown>;
  resolve: (mesh: MeshData | null) => void;
  timer: ReturnType<typeof setTimeout>;
}

function pythonPath() {
  if (process.env.CQ_PYTHON) return process.env.CQ_PYTHON;
  const local = resolve(process.cwd(), '.venv-cq/Scripts/python.exe');
  return existsSync(local) ? local : '';
}

function toMesh(parsed: CqMeshPayload): MeshData {
  return {
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
  };
}

let child: ChildProcessWithoutNullStreams | null = null;
let queue: CqJob[] = [];
let active: CqJob | null = null;
let unavailable = false;

function failAll() {
  if (active) {
    clearTimeout(active.timer);
    active.resolve(null);
    active = null;
  }
  for (const job of queue) {
    clearTimeout(job.timer);
    job.resolve(null);
  }
  queue = [];
}

function stopChild() {
  if (child) {
    child.removeAllListeners();
    child.kill();
    child = null;
  }
}

function spawnWorker(): ChildProcessWithoutNullStreams | null {
  const python = pythonPath();
  const server = resolve(process.cwd(), 'server/cq/abutment_server.py');
  if (!python || !existsSync(server) || unavailable) return null;
  const next = spawn(python, [server], {
    cwd: process.cwd(),
    stdio: ['pipe', 'pipe', 'pipe'],
    windowsHide: true,
  });
  const lines = createInterface({ input: next.stdout });
  lines.on('line', (line) => {
    if (!active) return;
    const job = active;
    active = null;
    clearTimeout(job.timer);
    try {
      const parsed = JSON.parse(line) as CqMeshPayload;
      job.resolve(parsed.error ? null : toMesh(parsed));
    } catch {
      job.resolve(null);
    }
    runNext();
  });
  next.stderr.on('data', (chunk) => {
    console.warn('[cadquery]', String(chunk).slice(0, 500));
  });
  next.on('error', () => {
    unavailable = true;
    stopChild();
    failAll();
  });
  next.on('exit', () => {
    child = null;
    unavailable = true;
    failAll();
  });
  child = next;
  return next;
}

function runNext() {
  if (active || !queue.length) return;
  const worker = child ?? spawnWorker();
  if (!worker) {
    failAll();
    return;
  }
  active = queue.shift() ?? null;
  if (!active) return;
  worker.stdin.write(`${JSON.stringify(active.payload)}\n`);
  active.timer = setTimeout(() => {
    stopChild();
    failAll();
  }, 60_000);
}

/**
 * Runs the authoritative CadQuery generator through a persistent worker.
 * The worker keeps CadQuery/OCP imported between requests, so only the first
 * request pays the Python startup cost.
 */
export async function generateAbutmentMesh(
  params: Record<string, number | string | boolean>,
  options: { generatorPath?: string; shapeKeys?: string[]; partSlots?: Record<string, number> } = {},
): Promise<MeshData | null> {
  if (!pythonPath() || unavailable) return null;
  return new Promise((resolvePromise) => {
    const job: CqJob = {
      payload: {
        params,
        generatorPath: options.generatorPath,
        shapeKeys: options.shapeKeys,
        partSlots: options.partSlots,
      },
      resolve: resolvePromise,
      timer: setTimeout(() => undefined, 0),
    };
    clearTimeout(job.timer);
    queue.push(job);
    runNext();
  });
}

export async function generateAbutmentStep(
  params: Record<string, number | string | boolean>,
  options: { generatorPath?: string } = {},
): Promise<Uint8Array | null> {
  const python = pythonPath();
  const worker = resolve(process.cwd(), 'server/cq/step_worker.py');
  if (!python || unavailable || !existsSync(worker)) return null;
  return new Promise((resolveStep) => {
    const child = spawn(python, [worker], {
      cwd: process.cwd(),
      stdio: ['pipe', 'pipe', 'pipe'],
      windowsHide: true,
    });
    const chunks: Buffer[] = [];
    let stderr = '';
    const timer = setTimeout(() => {
      child.kill();
      resolveStep(null);
    }, 120_000);
    child.stdout.on('data', (chunk: Buffer) => chunks.push(chunk));
    child.stderr.on('data', (chunk) => {
      stderr += String(chunk);
    });
    child.on('error', () => {
      clearTimeout(timer);
      resolveStep(null);
    });
    child.on('close', (code) => {
      clearTimeout(timer);
      if (code !== 0 || !chunks.length) {
        if (stderr) console.warn('[cadquery step]', stderr.slice(0, 600));
        resolveStep(null);
        return;
      }
      resolveStep(new Uint8Array(Buffer.concat(chunks)));
    });
    child.stdin.end(JSON.stringify({ params, generatorPath: options.generatorPath }));
  });
}

export function shutdownCqWorker() {
  stopChild();
}

process.once('exit', shutdownCqWorker);
