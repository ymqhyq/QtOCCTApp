<script setup lang="ts">
import { computed, onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppIcon from '@/components/ui/AppIcon.vue';
import EmptyState from '@/components/ui/EmptyState.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import { useUiStore } from '@/stores/ui';
import { useJobsStore } from '@/stores/jobs';
import { formatDateTime, jobStatusMeta, maskKey, relativeTime, shortId } from '@/utils/format';
import type { ComputeJob } from '@/api/types';

const router = useRouter();
const ui = useUiStore();
const jobs = useJobsStore();
const { jobDrawerOpen } = storeToRefs(ui);
const { active, history, loading } = storeToRefs(jobs);

const total = computed(() => active.value.length + history.value.length);

function openInViewport(job: ComputeJob) {
  ui.jobDrawerOpen = false;
  router.push({ path: `/components/${job.familyId}/configurator`, query: { job: job.jobId } });
}

function openComponent(job: ComputeJob) {
  ui.jobDrawerOpen = false;
  router.push(`/components/${job.familyId}`);
}

function paramSummary(job: ComputeJob) {
  const entries = Object.entries(job.params ?? {}).filter(([key]) => key !== 'ModelNumber' && key !== 'NetVolume' && key !== 'StrengthGrade');
  return entries.slice(0, 3).map(([key, value]) => `${key}=${typeof value === 'number' ? Math.round(value) : value}`);
}

onMounted(() => {
  if (jobDrawerOpen.value) void jobs.refresh();
});
</script>

<template>
  <Teleport to="body">
    <Transition
      enter-active-class="transition duration-200 ease-industrial"
      enter-from-class="opacity-0"
      leave-active-class="transition duration-150"
      leave-to-class="opacity-0"
    >
      <div v-if="jobDrawerOpen" class="fixed inset-0 z-[70]">
        <div class="absolute inset-0 bg-[#0B1220]/35" @click="jobDrawerOpen = false" />
        <Transition
          enter-active-class="transition duration-250 ease-industrial"
          enter-from-class="translate-x-full"
          leave-active-class="transition duration-150 ease-in"
          leave-to-class="translate-x-full"
          appear
        >
          <aside class="absolute bottom-0 right-0 top-0 flex w-full max-w-[440px] flex-col border-l border-line bg-surface shadow-drawer">
            <header class="flex items-start gap-2 border-b border-line px-4 py-3">
              <div class="flex h-8 w-8 items-center justify-center rounded-[5px] bg-[#E8EFF4] text-primary">
                <AppIcon name="cpu" :size="16" />
              </div>
              <div class="flex-1">
                <h2 class="text-[14px] font-semibold">计算任务中心</h2>
                <p class="mt-0.5 text-[11px] text-muted">
                  {{ active.length }} 个进行中 · {{ history.length }} 个历史 · 支持刷新后断点恢复
                </p>
              </div>
              <button class="btn btn-ghost btn-icon" title="刷新" @click="jobs.refresh()">
                <AppIcon name="refresh" :size="15" :class="loading ? 'animate-spin' : ''" />
              </button>
              <button class="btn btn-ghost btn-icon" aria-label="关闭" @click="jobDrawerOpen = false">
                <AppIcon name="close" :size="16" />
              </button>
            </header>

            <div class="min-h-0 flex-1 overflow-y-auto scroll-thin">
              <div v-if="!total" class="p-4">
                <EmptyState
                  icon="cpu"
                  title="暂无计算任务"
                  description="在参数配置器调整非标准尺寸时，若服务端预估求解超过 3 秒，任务将进入此处并支持断点恢复。"
                  compact
                />
              </div>

              <section v-if="active.length" class="border-b border-line">
                <p class="sticky top-0 z-10 bg-surface px-4 py-2 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">
                  进行中
                </p>
                <article
                  v-for="job in active"
                  :key="job.jobId"
                  class="border-t border-[#F0F2F5] px-4 py-3 first:border-t-0"
                >
                  <div class="flex items-start justify-between gap-2">
                    <div class="min-w-0">
                      <p class="truncate text-[13px] font-medium text-ink">{{ job.componentName }}</p>
                      <p class="mt-0.5 font-mono text-[10px] text-[#9CA3AF]">
                        {{ job.jobId }}
                      </p>
                    </div>
                    <StatusBadge :tone="jobStatusMeta(job.status).tone" :label="jobStatusMeta(job.status).label" />
                  </div>

                  <div class="mt-2 h-1.5 overflow-hidden rounded-full bg-[#EEF1F4]">
                    <div
                      class="h-full rounded-full transition-all duration-500"
                      :class="job.status === 'detached_computing' ? 'bg-warning' : 'bg-primary'"
                      :style="{ width: `${Math.max(4, job.progress)}%` }"
                    />
                  </div>
                  <div class="mt-1 flex items-center justify-between text-[10.5px] text-muted">
                    <span>
                      {{ job.status === 'queued' ? '等待 Worker 容器调度' : job.status === 'detached_computing' ? '已取消前台等待 · 后台脱机计算中' : '几何 Worker 求解中' }}
                    </span>
                    <span class="mono">{{ job.progress }}%</span>
                  </div>

                  <div v-if="paramSummary(job).length" class="mt-2 flex flex-wrap gap-1">
                    <span v-for="p in paramSummary(job)" :key="p" class="tag mono">{{ p }}</span>
                  </div>

                  <div class="mt-2.5 flex items-center justify-between">
                    <span class="font-mono text-[9.5px] text-[#9CA3AF]">
                      cacheKey {{ maskKey(job.cacheKey, 10, 4) }}
                    </span>
                    <button class="btn btn-outline btn-sm" @click="jobs.cancel(job.jobId)">
                      <AppIcon name="close" :size="12" />取消等待
                    </button>
                  </div>
                </article>
              </section>

              <section v-if="history.length">
                <p class="sticky top-0 z-10 bg-surface px-4 py-2 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">
                  历史任务
                </p>
                <article
                  v-for="job in history"
                  :key="job.jobId"
                  class="border-t border-[#F0F2F5] px-4 py-3"
                >
                  <div class="flex items-start justify-between gap-2">
                    <div class="min-w-0">
                      <p class="truncate text-[12.5px] font-medium text-ink">{{ job.componentName }}</p>
                      <p class="mt-0.5 text-[10px] text-[#9CA3AF]" :title="job.jobId">
                        {{ shortId(job.jobId, 22) }} · {{ relativeTime(job.updatedAt) }}
                      </p>
                    </div>
                    <StatusBadge :tone="jobStatusMeta(job.status).tone" :label="jobStatusMeta(job.status).label" />
                  </div>

                  <div v-if="job.status === 'failed'" class="mt-2 rounded border border-[#FECACA] bg-[#FEF2F2] px-2 py-1.5">
                    <p class="mono text-[10.5px] leading-relaxed text-danger">{{ job.error }}</p>
                  </div>

                  <div class="mt-2 flex items-center justify-between gap-2">
                    <span class="font-mono text-[9.5px] text-[#9CA3AF]">
                      {{ maskKey(job.cacheKey, 10, 4) }}
                    </span>
                    <div class="flex items-center gap-1.5">
                      <span class="text-[9.5px] text-[#9CA3AF]">{{ formatDateTime(job.finishedAt ?? job.updatedAt) }}</span>
                      <button
                        v-if="job.status === 'succeeded'"
                        class="btn btn-primary btn-sm"
                        @click="openInViewport(job)"
                      >
                        <AppIcon name="eye" :size="12" />调出视口
                      </button>
                      <button v-else class="btn btn-outline btn-sm" @click="openComponent(job)">
                        查看构件
                      </button>
                    </div>
                  </div>
                </article>
              </section>
            </div>

            <footer class="border-t border-line bg-[#FAFBFC] px-4 py-2.5 text-[10px] leading-relaxed text-[#9CA3AF]">
              四级时间阶梯：T1 2.0s 性能水位 · T2 3.0s 异步分流 · T3 8.0s 前台等待上限 · T4 30.0s 硬超时熔断
            </footer>
          </aside>
        </Transition>
      </div>
    </Transition>
  </Teleport>
</template>
