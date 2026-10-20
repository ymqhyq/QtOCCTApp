<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { useRouter } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import LineChart from '@/components/charts/LineChart.vue';
import BarList from '@/components/charts/BarList.vue';
import { api } from '@/api/client';
import type { AnalyticsOverview } from '@/api/types';
import { useUiStore } from '@/stores/ui';
import {
  formatNumber,
  formatPercent,
  jobStatusMeta,
  relativeTime,
  triggerDownload,
} from '@/utils/format';

const router = useRouter();
const ui = useUiStore();
const data = ref<AnalyticsOverview | null>(null);
const audit = ref<any[]>([]);
const showAdvanced = ref(false);
const loading = ref(true);
const error = ref('');

const cacheTarget = 90;
const reuseItems = computed(() =>
  (data.value?.reuseTrend ?? []).map((point) => ({ label: point.date, value: point.value })),
);
const latencyItems = computed(() =>
  (data.value?.workerLatency ?? []).map((item) => ({
    label: `${item.bucket} · P95 ${item.p95}ms`,
    value: item.count,
    hint: `P50 ${item.p50}ms / P95 ${item.p95}ms / 调用 ${item.count} 次`,
  })),
);
const ingestionItems = computed(() =>
  (data.value?.ingestionTrend ?? []).map((point) => ({ label: point.date, value: point.value })),
);

async function load() {
  loading.value = true;
  error.value = '';
  try {
    const [overview, auditResponse] = await Promise.all([
      api.analytics(),
      api.audit().catch(() => ({ items: [], total: 0 })),
    ]);
    data.value = overview;
    audit.value = auditResponse.items.slice(0, 8);
  } catch (loadError) {
    error.value = loadError instanceof Error ? loadError.message : '看板数据加载失败';
  } finally {
    loading.value = false;
  }
}

function exportSnapshot() {
  if (!data.value) return;
  const blob = new Blob(
    [JSON.stringify({ generatedAt: new Date().toISOString(), ...data.value }, null, 2)],
    { type: 'application/json' },
  );
  const url = URL.createObjectURL(blob);
  triggerDownload(url, `cbfc_analytics_${new Date().toISOString().slice(0, 10)}.json`);
  setTimeout(() => URL.revokeObjectURL(url), 2000);
  ui.notify({ type: 'success', title: '看板快照已导出', message: '包含 KPI、趋势、任务与存储指标。' });
}

function openJob(job: any) {
  ui.jobDrawerOpen = true;
}

function retryJob(job: any) {
  router.push(`/components/${job.familyId}/configurator`);
}

onMounted(load);
</script>

<template>
  <div class="mx-auto max-w-[1500px] px-4 py-4 xl:px-6">
    <div class="flex flex-wrap items-end justify-between gap-3">
      <div>
        <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
          <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
          <AppIcon name="chevronRight" :size="10" />
          <span class="text-[#4B5563]">监控看板</span>
        </div>
        <h1 class="mt-1 text-[20px] font-semibold tracking-tight text-ink">数据监控与资产分析</h1>
        <p class="mt-0.5 text-[12px] text-muted">构件库运行监控、资产流转与质检分析指标</p>
      </div>
      <div class="flex items-center gap-2">
        <span class="flex items-center gap-1.5 rounded border border-line bg-surface px-2.5 py-1 text-[10.5px] text-muted">
          <AppIcon name="clock" :size="12" />近 14 天
        </span>
        <button class="btn btn-outline btn-sm" @click="showAdvanced = !showAdvanced">
          {{ showAdvanced ? '收起高级信息' : '高级信息' }}
        </button>
        <button class="btn btn-outline btn-sm" @click="load"><AppIcon name="refresh" :size="12" />刷新</button>
        <button class="btn btn-primary btn-sm" @click="exportSnapshot"><AppIcon name="download" :size="12" />导出快照</button>
      </div>
    </div>

    <div v-if="loading" class="mt-4 grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-4">
      <div v-for="i in 4" :key="i" class="skeleton h-[112px] w-full" />
    </div>
    <div v-else-if="error" class="panel mt-4 p-6 text-center">
      <p class="text-[13px] text-danger">{{ error }}</p>
      <button class="btn btn-outline btn-sm mt-2" @click="load">重试</button>
    </div>

    <template v-else-if="data">
      <!-- KPI -->
      <div class="mt-4 grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-4">
        <div class="panel p-3.5">
          <div class="flex items-start justify-between">
            <div>
              <p class="text-[10.5px] font-medium text-muted">构件总资产数</p>
              <p class="mono mt-1 text-[26px] font-semibold leading-none tracking-tight text-ink">{{ data.kpi.totalAssets }}</p>
            </div>
            <span class="flex h-8 w-8 items-center justify-center rounded bg-[#E8EFF4] text-primary"><AppIcon name="cube" :size="16" /></span>
          </div>
          <div class="mt-2.5 flex flex-wrap gap-1">
            <span class="tag">参数化 {{ data.kpi.bySourceMode.parametric }}</span>
            <span class="tag">BRep {{ data.kpi.bySourceMode['native-brep'] }}</span>
            <span class="tag">IFC {{ data.kpi.bySourceMode['imported-ifc'] }}</span>
            <span class="tag">网格 {{ data.kpi.bySourceMode['mesh-only'] }}</span>
          </div>
          <div class="mt-2 flex items-center justify-between border-t border-[#F0F2F5] pt-1.5 text-[9.5px] text-muted">
            <span>已发布 {{ data.kpi.published }} · 审核中 {{ data.kpi.reviewing }}</span>
            <span class="text-success">草稿 {{ data.kpi.draft }}</span>
          </div>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-start justify-between">
            <div>
              <p class="text-[10.5px] font-medium text-muted">确定性缓存命中率</p>
              <p class="mono mt-1 text-[26px] font-semibold leading-none tracking-tight text-ink">{{ formatPercent(data.kpi.cacheHitRate) }}</p>
            </div>
            <span class="flex h-8 w-8 items-center justify-center rounded bg-[#ECFDF3] text-success"><AppIcon name="zap" :size="16" /></span>
          </div>
          <div class="mt-2 h-1.5 overflow-hidden rounded-full bg-[#EEF1F4]">
            <div class="h-full rounded-full bg-success" :style="{ width: `${data.kpi.cacheHitRate}%` }" />
          </div>
          <div class="mt-1.5 flex items-center justify-between text-[9.5px]">
            <span class="text-muted">目标 ≥ {{ cacheTarget }}%</span>
            <span :class="data.kpi.cacheHitRate >= cacheTarget ? 'text-success' : 'text-warning'">
              {{ data.kpi.cacheHitRate >= cacheTarget ? '达标' : '未达标' }}
            </span>
          </div>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-start justify-between">
            <div>
              <p class="text-[10.5px] font-medium text-muted">入库质检一次通过率</p>
              <p class="mono mt-1 text-[26px] font-semibold leading-none tracking-tight text-ink">{{ formatPercent(data.kpi.gateFirstPassRate) }}</p>
            </div>
            <span class="flex h-8 w-8 items-center justify-center rounded bg-[#EFF6FF] text-[#1D4ED8]"><AppIcon name="shield" :size="16" /></span>
          </div>
          <div class="mt-2 h-1.5 overflow-hidden rounded-full bg-[#EEF1F4]">
            <div class="h-full rounded-full bg-[#2563EB]" :style="{ width: `${data.kpi.gateFirstPassRate}%` }" />
          </div>
          <p class="mt-1.5 text-[9.5px] text-muted">6 道门禁 · GATE-01 → GATE-06 全链校验</p>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-start justify-between">
            <div>
              <p class="text-[10.5px] font-medium text-muted">未映射语义码存量</p>
              <p class="mono mt-1 text-[26px] font-semibold leading-none tracking-tight" :class="data.kpi.unmappedCount ? 'text-warning' : 'text-success'">
                {{ data.kpi.unmappedCount }}
              </p>
            </div>
            <span class="flex h-8 w-8 items-center justify-center rounded bg-[#FFFBEB] text-warning"><AppIcon name="alertTriangle" :size="16" /></span>
          </div>
          <p class="mt-2 text-[9.5px] leading-relaxed text-muted">
            企业标准覆盖健全度目标清零；未映射部位保留基础几何并在导出报告中告警。
          </p>
          <RouterLink to="/semantics" class="mt-1.5 flex items-center gap-1 text-[10px] font-medium text-primary hover:text-primary-hover">
            前往语义中枢治理<AppIcon name="arrowRight" :size="10" />
          </RouterLink>
        </div>
      </div>

      <!-- 趋势 -->
      <div class="mt-3 grid gap-3 lg:grid-cols-2">
        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <div>
              <p class="section-title">构件跨项目复用调用频次</p>
              <p class="mt-0.5 text-[10.5px] text-muted">近 14 天 · 全库构件下载 / 变参 / 深链推送合计</p>
            </div>
            <StatusBadge tone="success" label="+18.6% 环比" :dot="false" />
          </div>
          <div class="mt-2">
            <LineChart :data="reuseItems" :height="188" unit=" 次" />
          </div>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <div>
              <p class="section-title">几何求解时延分布</p>
              <p class="mt-0.5 text-[10.5px] text-muted">条形长度按调用次数 · 标签显示各区间 P95 耗时</p>
            </div>
            <StatusBadge tone="info" label="876 次任务" :dot="false" />
          </div>
          <div class="mt-3">
            <BarList :items="latencyItems" unit=" 次" color="#1F5A7A" />
          </div>
          <div class="mt-2 flex items-center justify-between border-t border-[#F0F2F5] pt-1.5 text-[9px] text-muted">
            <span>T1 2.0s 性能水位</span>
            <span>T2 3.0s 异步分流 · T3 8.0s 前台上限 · T4 30.0s 熔断</span>
          </div>
        </div>
      </div>

      <!-- 待办与存储 -->
      <div class="mt-3 grid gap-3 lg:grid-cols-3">
        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <p class="section-title">异常失败任务排查</p>
            <button class="text-[10px] text-primary" @click="ui.jobDrawerOpen = true">任务中心 →</button>
          </div>
          <div class="mt-2 space-y-1.5">
            <div v-for="job in data.failures" :key="job.jobId" class="rounded border border-[#FECACA] bg-[#FEF2F2] px-2.5 py-2">
              <div class="flex items-start justify-between gap-1">
                <p class="text-[11px] font-medium text-ink">{{ job.componentName }}</p>
                <StatusBadge tone="danger" :label="jobStatusMeta(job.status).label" />
              </div>
              <p class="mono mt-1 break-words text-[9px] leading-relaxed text-danger">{{ job.error }}</p>
              <div class="mt-1.5 flex items-center justify-between">
                <span class="mono text-[8.5px] text-[#9CA3AF]">{{ job.jobId }} · {{ relativeTime(job.updatedAt) }}</span>
                <div class="flex gap-1">
                  <button class="btn btn-outline btn-sm !py-0.5" @click="retryJob(job)">重试</button>
                  <button class="btn btn-outline btn-sm !py-0.5" @click="openJob(job)">日志</button>
                </div>
              </div>
            </div>
          </div>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <p class="section-title">待复核资产</p>
            <RouterLink to="/reviews" class="text-[10px] text-primary">进入审核台 →</RouterLink>
          </div>
          <div class="mt-2 space-y-1.5">
            <button
              v-for="review in data.reviewBacklog"
              :key="review.reviewId"
              class="flex w-full items-center gap-2 rounded border border-line bg-white px-2.5 py-2 text-left transition-colors hover:border-[#C7D2DA] hover:bg-[#FAFBFC]"
              @click="router.push('/reviews')"
            >
              <span class="h-1.5 w-1.5 shrink-0 rounded-full" :class="review.urgency === 'high' ? 'bg-danger' : 'bg-warning'" />
              <span class="min-w-0 flex-1">
                <span class="block truncate text-[11px] font-medium text-ink">{{ review.componentName }}</span>
                <span class="mono block text-[8.5px] text-[#9CA3AF]">{{ review.reviewId }} · {{ review.submittedBy }}</span>
              </span>
              <span class="shrink-0 text-[9px] text-muted">{{ relativeTime(review.submittedAt) }}</span>
            </button>
            <div v-if="!data.reviewBacklog.length" class="py-6 text-center text-[10.5px] text-muted">暂无待复核资产</div>
          </div>
        </div>

        <div v-if="showAdvanced" class="panel p-3.5">
          <p class="section-title">存储与算力</p>
          <div class="mt-2 grid grid-cols-2 gap-1.5">
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[9px] text-muted">CAS 去重率</p>
              <p class="mono mt-0.5 text-[15px] font-semibold text-success">{{ data.storage.casDedupRate }}%</p>
            </div>
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[9px] text-muted">制品总数</p>
              <p class="mono mt-0.5 text-[15px] font-semibold text-ink">{{ formatNumber(data.storage.artifacts) }}</p>
            </div>
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[9px] text-muted">对象存储</p>
              <p class="mono mt-0.5 text-[13px] font-semibold text-ink">{{ data.storage.totalGB }} GB</p>
            </div>
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[9px] text-muted">权威几何</p>
              <p class="mono mt-0.5 text-[13px] font-semibold text-ink">{{ data.storage.authoritativeGB }} GB</p>
            </div>
          </div>
          <p class="mt-2 text-[9.5px] text-muted">近 7 天入库质检量</p>
          <LineChart :data="ingestionItems" :height="104" color="#15803D" unit=" 件" />
        </div>
      </div>

      <!-- 审计 -->
      <div v-if="showAdvanced" class="panel mt-3 overflow-hidden">
        <div class="flex items-center justify-between border-b border-line px-3 py-2">
          <p class="section-title">全链路审计日志（最近 8 条）</p>
          <span class="font-mono text-[11px] text-muted">不可篡改 · 仅资产/标准管理员可见</span>
        </div>
        <table class="table-dense">
          <thead>
            <tr>
              <th>时间</th>
              <th>操作人</th>
              <th>动作</th>
              <th>资源</th>
              <th>详情</th>
              <th>IP</th>
            </tr>
          </thead>
          <tbody>
            <tr v-for="entry in audit" :key="entry.id">
              <td class="font-mono text-[12px] text-muted">{{ entry.at.slice(0, 19).replace('T', ' ') }}</td>
              <td class="text-[12.5px] font-medium text-ink">{{ entry.actor }}</td>
              <td><span class="chip mono">{{ entry.action }}</span></td>
              <td class="mono text-[12px] text-[#374151]">{{ entry.resource }}</td>
              <td class="max-w-[520px] truncate text-[12px] text-muted" :title="entry.detail">{{ entry.detail }}</td>
              <td class="mono text-[11.5px] text-[#9CA3AF]">{{ entry.ip }}</td>
            </tr>
          </tbody>
        </table>
      </div>
    </template>
  </div>
</template>
