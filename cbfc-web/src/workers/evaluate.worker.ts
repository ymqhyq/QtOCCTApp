/// <reference lib="webworker" />
/**
 * 变参求值 Worker：所有网络请求 + JHB 帧头解析在独立线程完成，
 * 仅向主线程 transfer 单个 jhbBuffer（报告 §5.3 闭环 B4）。
 */
const ctx = self as unknown as {
  postMessage: (message: unknown, transfer?: Transferable[]) => void;
  onmessage: ((event: MessageEvent) => void) | null;
};

interface EvaluateMessage {
  type: 'EVALUATE';
  requestId: string;
  familyId: string;
  body: unknown;
  token: string;
  legacy?: boolean;
}

interface AbortMessage {
  type: 'ABORT';
  requestId: string;
}

interface LoadMessage {
  type: 'LOAD';
  requestId: string;
  familyId: string;
  token: string;
  modelNumber?: string;
}

const controllers = new Map<string, AbortController>();
const cancelled = new Set<string>();

function post(message: unknown, transfer?: Transferable[]) {
  ctx.postMessage(message, transfer);
}

function sleep(ms: number, signal: AbortSignal) {
  return new Promise<void>((resolve, reject) => {
    if (signal.aborted) return reject(new DOMException('Aborted', 'AbortError'));
    const timer = setTimeout(() => {
      signal.removeEventListener('abort', onAbort);
      resolve();
    }, ms);
    const onAbort = () => {
      clearTimeout(timer);
      reject(new DOMException('Aborted', 'AbortError'));
    };
    signal.addEventListener('abort', onAbort, { once: true });
  });
}

async function readHeader(buffer: ArrayBuffer) {
  const jsonLen = new DataView(buffer).getUint32(0, true);
  const text = new TextDecoder('utf-8').decode(new Uint8Array(buffer, 4, jsonLen));
  return { jsonLen, header: JSON.parse(text) as Record<string, any> };
}

async function sendSuccess(
  response: Response,
  requestId: string,
  mode: 'sync' | 'async',
  startedAt: number,
) {
  const buffer = await response.arrayBuffer();
  const { jsonLen, header } = await readHeader(buffer);
  post(
    {
      type: 'SUCCESS',
      requestId,
      buffer,
      header,
      meta: {
        mode,
        jsonLen,
        payloadStart: 4 + jsonLen,
        cacheHit: response.headers.get('X-Cache-Hit') === 'true',
        cacheKey: response.headers.get('X-Cache-Key') ?? header.cache_key,
        legacy: response.headers.get('X-Legacy-Frame') === 'unaligned',
        elapsedMs: Math.round(performance.now() - startedAt),
        byteLength: buffer.byteLength,
      },
    },
    [buffer],
  );
}

async function pollJob(
  requestId: string,
  statusUrl: string,
  resultUrl: string | null,
  token: string,
  signal: AbortSignal,
  startedAt: number,
  initialEstimated: number,
) {
  let estimated = initialEstimated;
  while (!signal.aborted) {
    await sleep(1000, signal);
    const response = await fetch(statusUrl, {
      headers: { Authorization: `Bearer ${token}` },
      signal,
    });
    if (!response.ok) throw new Error(`任务状态查询失败 HTTP ${response.status}`);
    const job = await response.json();
    post({ type: 'JOB_STATUS', requestId, job });
    if (job.estimatedMs) estimated = job.estimatedMs;
    if (job.status === 'succeeded') {
      const target = resultUrl ?? job.resultUrl;
      const result = await fetch(target, {
        headers: { Authorization: `Bearer ${token}` },
        signal,
      });
      if (!result.ok) throw new Error(`任务结果获取失败 HTTP ${result.status}`);
      await sendSuccess(result, requestId, 'async', startedAt);
      return;
    }
    if (job.status === 'failed' || job.status === 'cancelled_by_user') {
      post({
        type: 'ERROR',
        requestId,
        code: job.status === 'failed' ? 'GEOM_SOLVE_FAILED' : 'JOB_CANCELLED',
        message:
          job.error ??
          (job.status === 'failed' ? '几何求解失败，请查看任务中心日志' : '任务已取消'),
      });
      return;
    }
    if (job.status === 'detached_computing') {
      post({ type: 'JOB_STATUS', requestId, job, detached: true });
    }
  }
}

ctx.onmessage = async (event: MessageEvent) => {
  const message = event.data as EvaluateMessage | AbortMessage | LoadMessage;
  if (message.type === 'ABORT') {
    if (controllers.has(message.requestId)) {
      cancelled.add(message.requestId);
      controllers.get(message.requestId)?.abort();
    }
    post({ type: 'ABORTED', requestId: message.requestId });
    return;
  }
  if (message.type === 'LOAD') {
    const controller = new AbortController();
    controllers.set(message.requestId, controller);
    const startedAt = performance.now();
    try {
      const query = message.modelNumber ? `?modelNumber=${encodeURIComponent(message.modelNumber)}` : '';
      const response = await fetch(
        `/api/v1/components/${encodeURIComponent(message.familyId)}/geometry${query}`,
        {
          headers: { Authorization: `Bearer ${message.token}` },
          signal: controller.signal,
        },
      );
      if (!response.ok) {
        let detail: any = null;
        try {
          detail = await response.json();
        } catch {
          /* ignore */
        }
        throw new Error(detail?.message ?? `几何加载失败 HTTP ${response.status}`);
      }
      await sendSuccess(response, message.requestId, 'sync', startedAt);
    } catch (error) {
      if (cancelled.has(message.requestId) || (error instanceof DOMException && error.name === 'AbortError')) {
        post({ type: 'ABORTED', requestId: message.requestId });
      } else {
        post({
          type: 'ERROR',
          requestId: message.requestId,
          code: 'GEOMETRY_LOAD_FAILED',
          message: error instanceof Error ? error.message : String(error),
        });
      }
    } finally {
      controllers.delete(message.requestId);
      cancelled.delete(message.requestId);
    }
    return;
  }
  if (message.type !== 'EVALUATE') return;
  const { requestId, familyId, body, token, legacy } = message;
  const controller = new AbortController();
  controllers.set(requestId, controller);
  const startedAt = performance.now();
  try {
    const response = await fetch(
      `/api/v1/components/${encodeURIComponent(familyId)}/evaluate${legacy ? '?legacy=1' : ''}`,
      {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          Authorization: `Bearer ${token}`,
        },
        body: JSON.stringify(body),
        signal: controller.signal,
      },
    );
    if (response.status === 200) {
      await sendSuccess(response, requestId, 'sync', startedAt);
      return;
    }
    if (response.status === 202) {
      const accepted = await response.json();
      post({
        type: 'TASK_QUEUED',
        requestId,
        jobId: accepted.jobId,
        statusUrl: accepted.statusUrl,
        estimatedMs: accepted.estimatedMs,
        cacheKey: accepted.cacheKey,
        legacyFrame: Boolean(accepted.legacyFrame),
      });
      await pollJob(
        requestId,
        accepted.statusUrl,
        null,
        token,
        controller.signal,
        startedAt,
        accepted.estimatedMs,
      );
      return;
    }
    let detail: any = null;
    try {
      detail = await response.json();
    } catch {
      /* ignore */
    }
    post({
      type: 'ERROR',
      requestId,
      code: detail?.code ?? `HTTP_${response.status}`,
      message: detail?.message ?? `变参求解失败 HTTP ${response.status}`,
    });
  } catch (error) {
    if (cancelled.has(requestId) || (error instanceof DOMException && error.name === 'AbortError')) {
      post({ type: 'ABORTED', requestId });
    } else {
      post({
        type: 'ERROR',
        requestId,
        code: 'NETWORK_ERROR',
        message: error instanceof Error ? error.message : String(error),
      });
    }
  } finally {
    controllers.delete(requestId);
    cancelled.delete(requestId);
  }
};
