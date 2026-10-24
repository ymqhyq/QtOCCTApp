import { onBeforeUnmount, ref, shallowRef } from 'vue';
import { api, getToken } from '@/api/client';
import { decodeJhb, type DecodedJhb, type JhbHeader } from '@/jhb/decode';
import type { JobStatus } from '@/api/types';

export type EvaluationStatus =
  | 'idle'
  | 'loading'
  | 'queued'
  | 'running'
  | 'detached_computing'
  | 'succeeded'
  | 'failed'
  | 'aborted';

export interface EvaluationMeta {
  mode: 'sync' | 'async';
  cacheHit: boolean;
  cacheKey: string;
  legacy: boolean;
  elapsedMs: number;
  byteLength: number;
  jsonLen: number;
  payloadStart: number;
}

export interface EvaluateOptions {
  familyId: string;
  modelNumber?: string;
  parameterOverrides?: Record<string, number | string | boolean>;
  legacy?: boolean;
  acceptFormats?: string[];
}

export function useEvaluation() {
  const status = ref<EvaluationStatus>('idle');
  const progress = ref(0);
  const jobId = ref<string | null>(null);
  const error = ref('');
  const decoded = shallowRef<DecodedJhb | null>(null);
  const header = shallowRef<JhbHeader | null>(null);
  const meta = shallowRef<EvaluationMeta | null>(null);
  const elapsedMs = ref(0);

  let worker: Worker | null = null;
  let requestId = '';
  let startedAt = 0;
  let elapsedTimer: ReturnType<typeof setInterval> | null = null;

  function startElapsed() {
    stopElapsed();
    startedAt = performance.now();
    elapsedMs.value = 0;
    elapsedTimer = setInterval(() => {
      elapsedMs.value = Math.round(performance.now() - startedAt);
    }, 100);
  }
  function stopElapsed() {
    if (elapsedTimer) clearInterval(elapsedTimer);
    elapsedTimer = null;
  }

  function ensureWorker(): Worker {
    if (worker) return worker;
    worker = new Worker(new URL('../workers/evaluate.worker.ts', import.meta.url), {
      type: 'module',
    });
    worker.onmessage = (event: MessageEvent) => {
      const data = event.data;
      if (data.requestId !== requestId) return;
      switch (data.type) {
        case 'TASK_QUEUED':
          status.value = 'queued';
          progress.value = 0;
          jobId.value = data.jobId;
          break;
        case 'JOB_STATUS': {
          const jobStatus = data.job.status as JobStatus;
          progress.value = data.job.progress ?? progress.value;
          if (jobStatus === 'queued') status.value = 'queued';
          else if (jobStatus === 'running') status.value = 'running';
          else if (jobStatus === 'detached_computing') status.value = 'detached_computing';
          else if (jobStatus === 'failed') {
            status.value = 'failed';
            error.value = data.job.error ?? '几何求解失败';
          }
          break;
        }
        case 'SUCCESS': {
          try {
            const result = decodeJhb(data.buffer);
            decoded.value = result;
            header.value = result.header;
            meta.value = data.meta as EvaluationMeta;
            status.value = 'succeeded';
            progress.value = 100;
            error.value = '';
            stopElapsed();
            elapsedMs.value = data.meta.elapsedMs;
          } catch (decodeError) {
            status.value = 'failed';
            error.value =
              decodeError instanceof Error ? decodeError.message : 'JHB 解码失败';
            stopElapsed();
          }
          break;
        }
        case 'ERROR':
          status.value = 'failed';
          error.value = data.message;
          stopElapsed();
          break;
        case 'ABORTED':
          status.value = 'aborted';
          stopElapsed();
          break;
        default:
          break;
      }
    };
    return worker;
  }

  function evaluate(options: EvaluateOptions): string {
    const id = crypto.randomUUID();
    requestId = id;
    error.value = '';
    progress.value = 0;
    jobId.value = null;
    status.value = 'loading';
    startElapsed();
    ensureWorker().postMessage({
      type: 'EVALUATE',
      requestId: id,
      familyId: options.familyId,
      token: getToken(),
      legacy: options.legacy ?? false,
      body: {
        modelNumber: options.modelNumber ?? 'CUSTOM',
        parameterOverrides: options.parameterOverrides ?? {},
        acceptFormats: options.acceptFormats ?? ['cbf-jhb@1.4', 'cbf-binxcaf'],
      },
    });
    return id;
  }

  /** 只读几何加载（native-brep / imported-ifc / mesh-only） */
  function loadGeometry(familyId: string, modelNumber?: string): string {
    const id = crypto.randomUUID();
    requestId = id;
    error.value = '';
    progress.value = 0;
    jobId.value = null;
    status.value = 'loading';
    startElapsed();
    ensureWorker().postMessage({
      type: 'LOAD',
      requestId: id,
      familyId,
      token: getToken(),
      modelNumber,
    });
    return id;
  }

  /** 中止前台等待：worker 停止轮询；同时 DELETE 任务，服务端转入脱机计算 */
  async function cancelWaiting(): Promise<string> {
    const id = jobId.value;
    worker?.postMessage({ type: 'ABORT', requestId });
    stopElapsed();
    status.value = 'aborted';
    if (id) {
      try {
        const result = await api.cancelJob(id);
        return result.message;
      } catch {
        return '已解除前台等待';
      }
    }
    return '已解除前台等待';
  }

  async function loadJob(targetJobId: string): Promise<void> {
    requestId = crypto.randomUUID();
    error.value = '';
    status.value = 'loading';
    startElapsed();
    try {
      const buffer = await api.jobResult(targetJobId);
      const result = decodeJhb(buffer);
      decoded.value = result;
      header.value = result.header;
      jobId.value = targetJobId;
      status.value = 'succeeded';
      progress.value = 100;
      meta.value = {
        mode: 'async',
        cacheHit: false,
        cacheKey: result.header.cache_key,
        legacy: !result.aligned,
        elapsedMs: elapsedMs.value,
        byteLength: buffer.byteLength,
        jsonLen: result.jsonLen,
        payloadStart: result.payloadStart,
      };
      stopElapsed();
    } catch (loadError) {
      status.value = 'failed';
      error.value = loadError instanceof Error ? loadError.message : '任务结果载入失败';
      stopElapsed();
    }
  }

  function reset() {
    worker?.postMessage({ type: 'ABORT', requestId });
    status.value = 'idle';
    progress.value = 0;
    jobId.value = null;
    error.value = '';
    decoded.value = null;
    header.value = null;
    meta.value = null;
    elapsedMs.value = 0;
    stopElapsed();
  }

  function dispose() {
    stopElapsed();
    worker?.terminate();
    worker = null;
  }

  onBeforeUnmount(dispose);

  return {
    status,
    progress,
    jobId,
    error,
    decoded,
    header,
    meta,
    elapsedMs,
    evaluate,
    loadGeometry,
    cancelWaiting,
    loadJob,
    reset,
    dispose,
  };
}
