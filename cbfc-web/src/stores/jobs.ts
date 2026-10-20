import { computed, ref } from 'vue';
import { defineStore } from 'pinia';
import { api } from '@/api/client';
import type { ComputeJob } from '@/api/types';
import { useUiStore } from './ui';

export const useJobsStore = defineStore('jobs', () => {
  const items = ref<ComputeJob[]>([]);
  const loading = ref(false);
  const lastError = ref('');
  let timer: ReturnType<typeof setInterval> | null = null;
  let inFlight = false;

  const active = computed(() =>
    items.value.filter((j) =>
      ['queued', 'running', 'detached_computing'].includes(j.status),
    ),
  );
  const history = computed(() =>
    items.value.filter((j) => !['queued', 'running', 'detached_computing'].includes(j.status)),
  );
  const badge = computed(() => active.value.length);

  async function refresh() {
    if (inFlight) return;
    inFlight = true;
    try {
      const data = await api.jobs();
      items.value = data.items;
      lastError.value = '';
    } catch (error) {
      lastError.value = error instanceof Error ? error.message : '任务列表加载失败';
    } finally {
      inFlight = false;
      loading.value = false;
    }
  }

  function startPolling(intervalMs = 3000) {
    if (timer) return;
    void refresh();
    timer = setInterval(() => {
      void refresh();
    }, intervalMs);
  }

  function stopPolling() {
    if (timer) clearInterval(timer);
    timer = null;
  }

  async function cancel(jobId: string) {
    const ui = useUiStore();
    try {
      const result = await api.cancelJob(jobId);
      ui.notify({
        type: result.backendStatus === 'detached_computing' ? 'warning' : 'info',
        title: '任务取消请求已受理',
        message: result.message,
      });
      await refresh();
      return result;
    } catch (error) {
      ui.notify({
        type: 'error',
        title: '取消失败',
        message: error instanceof Error ? error.message : '未知错误',
      });
      return null;
    }
  }

  return { items, active, history, badge, loading, lastError, refresh, startPolling, stopPolling, cancel };
});
