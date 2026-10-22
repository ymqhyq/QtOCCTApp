<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import { useRoute } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import EmptyState from '@/components/ui/EmptyState.vue';
import ModalShell from '@/components/ui/ModalShell.vue';
import CadViewport from '@/components/viewer/CadViewport.vue';
import { api, getToken } from '@/api/client';
import { decodeJhb, type DecodedJhb } from '@/jhb/decode';
import type { ReviewQueueItem } from '@/api/types';
import { useUiStore } from '@/stores/ui';
import {
  formatDateTime,
  formatVolume,
  relativeTime,
} from '@/utils/format';

const route = useRoute();
const ui = useUiStore();

const reviews = ref<ReviewQueueItem[]>([]);
const selectedId = ref<string | null>(null);
const oldDecoded = ref<DecodedJhb | null>(null);
const newDecoded = ref<DecodedJhb | null>(null);
const reviewParts = ref<{ key: string; label: string }[]>([]);
const selectedReviewPart = ref<string | null>(null);
const selectedReviewPartIndex = computed(() => {
  const index = reviewParts.value.findIndex((part) => part.key === selectedReviewPart.value);
  return index >= 0 ? index : null;
});
const reviewViewMode = ref<'old' | 'new' | 'overlay'>('overlay');
const displayedDecoded = computed(() => (reviewViewMode.value === 'old' ? oldDecoded.value : newDecoded.value));
const displayedCompare = computed(() => (reviewViewMode.value === 'overlay' ? oldDecoded.value : null));
const geometryLoading = ref(false);
const geometryError = ref('');
const queueFilter = ref('');
const rejecting = ref(false);
const rejectReason = ref('');
const rejectDefects = ref<string[]>([]);
const deciding = ref(false);
const decisionResult = ref<string | null>(null);

const current = computed(() => reviews.value.find((r) => r.reviewId === selectedId.value) ?? null);
const filtered = computed(() => {
  const term = queueFilter.value.trim().toLowerCase();
  if (!term) return reviews.value;
  return reviews.value.filter(
    (r) =>
      r.componentName.toLowerCase().includes(term) ||
      r.familyId.toLowerCase().includes(term) ||
      r.reviewId.toLowerCase().includes(term),
  );
});
const volumeDelta = computed(() => {
  if (!current.value) return { diff: 0, percent: 0 };
  const diff = current.value.volume.to - current.value.volume.from;
  return {
    diff,
    percent: current.value.volume.from ? (diff / current.value.volume.from) * 100 : 0,
  };
});
const changeCounts = computed(() => {
  const counts = { added: 0, modified: 0, removed: 0 };
  for (const change of current.value?.changedShapes ?? []) counts[change.kind] += 1;
  return counts;
});
const urgentCount = computed(() => reviews.value.filter((r) => r.urgency === 'high').length);

async function fetchGeometry(reviewId: string, side: 'old' | 'new'): Promise<DecodedJhb> {
  const response = await fetch(api.reviewGeometryUrl(reviewId, side), {
    headers: { Authorization: `Bearer ${getToken()}` },
  });
  if (!response.ok) throw new Error(`几何快照加载失败 (${side}) HTTP ${response.status}`);
  return decodeJhb(await response.arrayBuffer());
}

async function selectReview(reviewId: string) {
  if (!reviews.value.some((r) => r.reviewId === reviewId)) return;
  selectedId.value = reviewId;
  reviewViewMode.value = 'overlay';
  oldDecoded.value = null;
  newDecoded.value = null;
  reviewParts.value = [];
  selectedReviewPart.value = null;
  geometryError.value = '';
  geometryLoading.value = true;
  try {
    const item = reviews.value.find((r) => r.reviewId === reviewId);
    const [oldGeom, newGeom, detail] = await Promise.all([
      fetchGeometry(reviewId, 'old'),
      fetchGeometry(reviewId, 'new'),
      item ? api.component(item.familyId).catch(() => null) : Promise.resolve(null),
    ]);
    if (selectedId.value !== reviewId) return;
    oldDecoded.value = oldGeom;
    newDecoded.value = newGeom;
    if (detail) {
      reviewParts.value = Object.entries(detail.shapes).map(([key, shape]) => ({
        key,
        label: shape.label || key,
      }));
    } else {
      reviewParts.value = newGeom.header.shapeKeys.map((key) => ({ key, label: key }));
    }
  } catch (error) {
    geometryError.value = error instanceof Error ? error.message : '几何快照加载失败';
  } finally {
    if (selectedId.value === reviewId) geometryLoading.value = false;
  }
}

async function loadQueue() {
  try {
    const data = await api.reviews();
    reviews.value = data.items;
    const preferred = String(route.query.familyId ?? '');
    const target =
      data.items.find((r) => r.familyId === preferred) ??
      data.items.find((r) => r.reviewId === selectedId.value) ??
      data.items[0];
    if (target) await selectReview(target.reviewId);
    else selectedId.value = null;
  } catch (error) {
    ui.notify({ type: 'error', title: '审核队列加载失败', message: String(error) });
  }
}

function nextReview(direction: 1 | -1) {
  if (!reviews.value.length) return;
  const index = reviews.value.findIndex((r) => r.reviewId === selectedId.value);
  const next = (index + direction + reviews.value.length) % reviews.value.length;
  void selectReview(reviews.value[next].reviewId);
}

async function approve() {
  if (!current.value || deciding.value) return;
  deciding.value = true;
  try {
    const result = await api.approveReview(current.value.reviewId);
    decisionResult.value = `已通过 ${current.value.reviewId}`;
    ui.notify({
      type: 'success',
      title: '审核通过 · 构件已发布',
      message: `${current.value.componentName} 已转入 published；剩余 ${result.remaining} 项待审。`,
    });
    const finishedId = current.value.reviewId;
    reviews.value = reviews.value.filter((r) => r.reviewId !== finishedId);
    const next = reviews.value[0];
    if (next) await selectReview(next.reviewId);
    else {
      selectedId.value = null;
      oldDecoded.value = null;
      newDecoded.value = null;
    }
  } catch (error) {
    ui.notify({ type: 'error', title: '签批失败', message: String(error) });
  } finally {
    deciding.value = false;
  }
}

function openReject() {
  if (!current.value) return;
  rejecting.value = true;
  rejectReason.value = '';
  rejectDefects.value = [];
}

async function submitReject() {
  if (!current.value) return;
  if (rejectReason.value.trim().length < 5) {
    ui.notify({ type: 'warning', title: '驳回意见必填', message: '请填写不少于 5 个字的实质性缺陷意见。' });
    return;
  }
  deciding.value = true;
  try {
    const result = await api.rejectReview(current.value.reviewId, rejectReason.value.trim(), rejectDefects.value);
    rejecting.value = false;
    ui.notify({
      type: 'info',
      title: '已驳回并生成修改工单',
      message: `${current.value.componentName} 已退回 draft；剩余 ${result.remaining} 项待审。`,
    });
    const finishedId = current.value.reviewId;
    reviews.value = reviews.value.filter((r) => r.reviewId !== finishedId);
    const next = reviews.value[0];
    if (next) await selectReview(next.reviewId);
    else {
      selectedId.value = null;
      oldDecoded.value = null;
      newDecoded.value = null;
    }
  } catch (error) {
    ui.notify({ type: 'error', title: '驳回失败', message: String(error) });
  } finally {
    deciding.value = false;
  }
}

function toggleDefect(value: string) {
  rejectDefects.value = rejectDefects.value.includes(value)
    ? rejectDefects.value.filter((v) => v !== value)
    : [...rejectDefects.value, value];
}

function onKeydown(event: KeyboardEvent) {
  const target = event.target as HTMLElement;
  if (['INPUT', 'TEXTAREA', 'SELECT'].includes(target.tagName) || target.isContentEditable) return;
  if (rejecting.value || deciding.value) return;
  const key = event.key.toLowerCase();
  if (key === 'j') {
    event.preventDefault();
    nextReview(1);
  } else if (key === 'k') {
    event.preventDefault();
    nextReview(-1);
  } else if (key === 'a') {
    event.preventDefault();
    void approve();
  } else if (key === 'r') {
    event.preventDefault();
    openReject();
  }
}

onMounted(async () => {
  window.addEventListener('keydown', onKeydown);
  await loadQueue();
});

onBeforeUnmount(() => {
  window.removeEventListener('keydown', onKeydown);
});
</script>

<template>
  <div class="flex h-full min-h-0 flex-col">
    <header class="flex h-12 shrink-0 items-center gap-3 border-b border-line bg-surface px-4">
      <div class="flex items-center gap-2">
        <span class="flex h-7 w-7 items-center justify-center rounded bg-[#E8EFF4] text-primary">
          <AppIcon name="shield" :size="15" />
        </span>
        <div>
          <h1 class="text-[13.5px] font-semibold text-ink">审核质检</h1>
          <p class="text-[9.5px] text-muted">双模型差异 · 门禁复核 · 结构化签批</p>
        </div>
      </div>
      <span class="h-5 w-px bg-line" />
      <div class="flex items-center gap-2 text-[10.5px]">
        <span class="rounded bg-[#FFF7ED] px-1.5 py-[2px] text-warning">待审 {{ reviews.length }}</span>
        <span class="rounded bg-[#FEF2F2] px-1.5 py-[2px] text-danger">加急 {{ urgentCount }}</span>
      </div>
      <div class="flex-1" />
      <button class="btn btn-outline btn-sm" @click="loadQueue">
        <AppIcon name="refresh" :size="12" />刷新队列
      </button>
    </header>

    <div class="flex min-h-0 flex-1">
      <!-- 队列 -->
      <aside class="flex w-[264px] shrink-0 flex-col border-r border-line bg-surface">
        <div class="border-b border-line p-2.5">
          <div class="flex items-center gap-1.5 rounded border border-line bg-[#FAFBFC] px-2 py-1">
            <AppIcon name="search" :size="12" class="text-[#9CA3AF]" />
            <input
              v-model="queueFilter"
              class="min-w-0 flex-1 border-0 bg-transparent text-[11px] focus:outline-none"
              placeholder="筛选待审任务…"
            />
          </div>
          <div class="mt-2 flex items-center justify-between text-[9.5px] text-muted">
            <span>按紧急度 / 提交时间排序</span>
            <span class="mono">{{ filtered.length }} 项</span>
          </div>
        </div>

        <div class="min-h-0 flex-1 overflow-y-auto scroll-thin">
          <button
            v-for="item in filtered"
            :key="item.reviewId"
            class="relative w-full border-b border-[#F0F2F5] px-3 py-2.5 text-left transition-colors"
            :class="selectedId === item.reviewId ? 'bg-[#EDF4F8]' : 'hover:bg-[#FAFBFC]'"
            @click="selectReview(item.reviewId)"
          >
            <span
              class="absolute inset-y-0 left-0 w-[2px]"
              :class="item.urgency === 'high' ? 'bg-danger' : item.urgency === 'normal' ? 'bg-warning' : 'bg-[#CBD5E1]'"
            />
            <div class="flex items-start justify-between gap-1">
              <p class="truncate text-[12px] font-medium" :class="selectedId === item.reviewId ? 'text-primary' : 'text-ink'">
                {{ item.componentName }}
              </p>
              <span
                class="shrink-0 rounded px-1 py-[1px] text-[8.5px]"
                :class="item.urgency === 'high' ? 'bg-[#FEF2F2] text-danger' : item.urgency === 'normal' ? 'bg-[#FFFBEB] text-warning' : 'bg-[#F3F4F6] text-muted'"
              >
                {{ item.urgency === 'high' ? '加急' : item.urgency === 'normal' ? '常规' : '低' }}
              </span>
            </div>
            <p class="mt-0.5 font-mono text-[9px] text-[#9CA3AF]">{{ item.reviewId }}</p>
            <div class="mt-1 flex items-center gap-1.5 text-[9.5px] text-muted">
              <span class="mono">v{{ item.fromRevision }}</span>
              <AppIcon name="arrowRight" :size="9" />
              <span class="mono font-semibold text-[#374151]">v{{ item.toRevision }}</span>
            </div>
            <div class="mt-1.5 flex flex-wrap items-center gap-1">
              <span class="rounded bg-[#ECFDF3] px-1 py-[1px] text-[8.5px] text-success">GATE {{ item.gatesPassed }}/{{ item.gatesTotal }}</span>
              <span v-if="item.unmapped.length" class="rounded bg-[#FFFBEB] px-1 py-[1px] text-[8.5px] text-warning">
                unmapped {{ item.unmapped.length }}
              </span>
              <span class="ml-auto text-[8.5px] text-[#9CA3AF]">{{ relativeTime(item.submittedAt) }}</span>
            </div>
          </button>

          <div v-if="!filtered.length" class="p-4">
            <EmptyState icon="shield" title="审核队列已清空" description="所有送审构件均已完成签批。" compact />
          </div>
        </div>
      </aside>

      <!-- 双模型视口 -->
      <section class="relative min-w-0 flex-1 bg-[#0B1220]">
        <template v-if="current && newDecoded">
          <CadViewport
            :key="`${current.reviewId}-${reviewViewMode}`"
            :decoded="displayedDecoded"
            :compare-decoded="displayedCompare"
            :diff-changes="current.changedShapes"
            status="succeeded"
            :progress="100"
            :hud="false"
            :shape-labels="{}"
            :named-parts="reviewParts"
            :selected-part="selectedReviewPart"
            :highlight-shape="selectedReviewPartIndex"
            @select-part="selectedReviewPart = $event"
          >
            <template #bottom-tools>
              <div class="pointer-events-auto flex gap-1 rounded-md border border-white/10 bg-[#0F1A2B]/90 p-1 backdrop-blur">
                <button
                  v-for="mode in ([
                    { id: 'old', label: '基准旧版' },
                    { id: 'new', label: '送审新版' },
                    { id: 'overlay', label: '重叠对比' },
                  ] as const)"
                  :key="mode.id"
                  class="rounded px-2 py-1 text-[10px] transition-colors"
                  :class="reviewViewMode === mode.id ? 'bg-white/15 font-semibold text-white' : 'text-[#94A3B8] hover:text-white'"
                  @click="reviewViewMode = mode.id"
                >
                  {{ mode.label }}
                </button>
              </div>
            </template>
          </CadViewport>
          <div class="pointer-events-none absolute bottom-20 left-3 z-10 rounded border border-white/10 bg-[#0F1A2B]/85 px-2.5 py-1.5 backdrop-blur">
            <p class="text-[8.5px] font-semibold uppercase tracking-wider text-[#94A3B8]">双模型重叠对比</p>
            <p class="mt-0.5 text-[10px] text-[#CBD5E1]">
              新版 v{{ current.toRevision }} · 旧版 v{{ current.fromRevision }} 半透明灰度
            </p>
            <p class="mono mt-0.5 text-[8.5px] text-[#64748B]">{{ current.familyId }}</p>
          </div>
        </template>
        <div v-else-if="geometryLoading" class="flex h-full items-center justify-center">
          <div class="text-center">
            <span class="mx-auto block h-6 w-6 animate-spin rounded-full border-2 border-white/15 border-t-hl" />
            <p class="mt-2 text-[11px] text-[#94A3B8]">正在加载新旧双模型几何快照…</p>
          </div>
        </div>
        <div v-else-if="geometryError" class="flex h-full items-center justify-center">
          <div class="rounded-md border border-[#7F1D1D] bg-[#2A1215]/80 p-4 text-center">
            <p class="text-[11px] text-[#FCA5A5]">{{ geometryError }}</p>
            <button class="btn btn-outline btn-sm mt-2" @click="current && selectReview(current.reviewId)">重试</button>
          </div>
        </div>
        <EmptyState
          v-else
          class="m-8"
          icon="shield"
          title="请选择待审构件"
          description="左侧队列中选择任务后，系统将加载新旧版几何并自动标红/标绿差异部位。"
        />
      </section>

      <!-- 差异抽屉 -->
      <aside v-if="current" class="flex w-[352px] shrink-0 flex-col border-l border-line bg-surface">
        <div class="border-b border-line px-3 py-2.5">
          <div class="flex items-start justify-between gap-2">
            <div class="min-w-0">
              <p class="truncate text-[13px] font-semibold text-ink">{{ current.componentName }}</p>
              <p class="mt-0.5 font-mono text-[9.5px] text-muted">
                {{ current.reviewId }} · {{ current.category }}
              </p>
            </div>
            <StatusBadge :tone="current.urgency === 'high' ? 'danger' : 'warning'" :label="current.urgency === 'high' ? '加急' : '待审'" />
          </div>
          <div class="mt-1.5 flex items-center gap-1.5 text-[10px] text-muted">
            <AppIcon name="user" :size="11" />{{ current.submittedBy }} 提交 · {{ formatDateTime(current.submittedAt) }}
          </div>
        </div>

        <div class="min-h-0 flex-1 overflow-y-auto scroll-thin">
          <div class="grid grid-cols-3 gap-1.5 border-b border-line p-3">
            <div class="rounded border border-[#FECACA] bg-[#FEF2F2] px-2 py-1.5">
              <p class="text-[8.5px] text-[#991B1B]">删除部位</p>
              <p class="mono mt-0.5 text-[15px] font-semibold text-danger">{{ changeCounts.removed }}</p>
            </div>
            <div class="rounded border border-[#FDE68A] bg-[#FFFBEB] px-2 py-1.5">
              <p class="text-[8.5px] text-[#92400E]">尺寸变形</p>
              <p class="mono mt-0.5 text-[15px] font-semibold text-warning">{{ changeCounts.modified }}</p>
            </div>
            <div class="rounded border border-[#BBF7D0] bg-[#F0FDF4] px-2 py-1.5">
              <p class="text-[8.5px] text-[#166534]">新增部位</p>
              <p class="mono mt-0.5 text-[15px] font-semibold text-success">{{ changeCounts.added }}</p>
            </div>
          </div>

          <div class="border-b border-line p-3">
            <p class="eyebrow">几何与算量对比</p>
            <div class="mt-1.5 flex items-end justify-between rounded border border-line bg-[#FAFBFC] px-2.5 py-2">
              <div>
                <p class="text-[8.5px] text-muted">旧版体积</p>
                <p class="mono text-[12px] text-[#6B7280]">{{ formatVolume(current.volume.from) }}</p>
              </div>
              <AppIcon name="arrowRight" :size="14" class="mb-1 text-[#9CA3AF]" />
              <div class="text-right">
                <p class="text-[8.5px] text-muted">新版体积</p>
                <p class="mono text-[13px] font-semibold text-ink">{{ formatVolume(current.volume.to) }}</p>
              </div>
              <span
                class="mono mb-0.5 rounded px-1.5 py-[1px] text-[9.5px]"
                :class="Math.abs(volumeDelta.percent) < 0.01 ? 'bg-[#F3F4F6] text-muted' : 'bg-[#FFFBEB] text-warning'"
              >
                {{ volumeDelta.percent >= 0 ? '+' : '' }}{{ volumeDelta.percent.toFixed(2) }}%
              </span>
            </div>
            <div class="mt-1.5 flex items-center gap-1.5">
              <span class="rounded bg-[#ECFDF3] px-1.5 py-[1px] text-[9px] text-success">6/6 门禁</span>
              <span class="text-[9px] text-muted">散度体积偏差 &lt; 1.0%</span>
            </div>
          </div>

          <div v-if="current.paramDiff.length" class="border-b border-line p-3">
            <p class="eyebrow">参数默认值 Diff</p>
            <table class="mt-1.5 w-full text-[12px]">
              <thead>
                <tr class="text-[11px] uppercase tracking-wider text-[#9CA3AF]">
                  <th class="pb-1 text-left font-medium">参数</th>
                  <th class="pb-1 text-right font-medium">旧版</th>
                  <th class="pb-1 text-right font-medium">新版</th>
                </tr>
              </thead>
              <tbody>
                <tr
                  v-for="param in current.paramDiff"
                  :key="param.name"
                  class="border-t border-[#F0F2F5]"
                  :class="param.changed ? 'bg-[#FFFBEB]' : ''"
                >
                  <td class="py-1">
                    <p class="text-[12px] text-[#374151]">{{ param.label }}</p>
                    <p class="font-mono text-[11px] text-[#9CA3AF]">{{ param.name }}</p>
                  </td>
                  <td class="py-1 text-right font-mono text-[12px]" :class="param.changed ? 'text-[#92400E] line-through' : 'text-muted'">
                    {{ param.from }}
                  </td>
                  <td class="py-1 text-right font-mono text-[12px] font-medium" :class="param.changed ? 'text-warning' : 'text-ink'">
                    {{ param.to }}
                  </td>
                </tr>
              </tbody>
            </table>
          </div>

          <div class="border-b border-line p-3">
            <p class="eyebrow">部位变更清单</p>
            <div class="mt-1 space-y-1">
              <div
                v-for="change in current.changedShapes"
                :key="change.shapeKey"
                class="flex items-start gap-1.5 rounded border border-line bg-white px-2 py-1.5"
              >
                <span
                  class="mt-0.5 h-2 w-2 shrink-0 rounded-sm"
                  :class="change.kind === 'added' ? 'bg-[#22C55E]' : change.kind === 'removed' ? 'bg-[#EF4444]' : 'bg-[#F59E0B]'"
                />
                <div class="min-w-0">
                  <p class="mono truncate text-[10px] font-medium text-ink">{{ change.shapeKey }}</p>
                  <p class="mt-0.5 text-[9px] leading-relaxed text-muted">{{ change.note }}</p>
                </div>
                <span class="ml-auto shrink-0 text-[8.5px] text-[#9CA3AF]">
                  {{ change.kind === 'added' ? '新增' : change.kind === 'removed' ? '删除' : '变形' }}
                </span>
              </div>
            </div>
          </div>

          <div v-if="current.unmapped.length" class="border-b border-line bg-[#FFFBEB] p-3">
            <p class="flex items-center gap-1 text-[10.5px] font-semibold text-[#92400E]">
              <AppIcon name="alertTriangle" :size="12" />语义未映射预警
            </p>
            <p v-for="item in current.unmapped" :key="item.shapeKey" class="mono mt-1 text-[9px] text-[#92400E]">
              {{ item.shapeKey }} → {{ item.code }}
            </p>
          </div>

          <div class="border-b border-line p-3">
            <p class="eyebrow">提交说明</p>
            <p class="mt-1 text-[10.5px] leading-relaxed text-[#4B5563]">{{ current.rationale }}</p>
          </div>
        </div>

        <div class="border-t border-line bg-[#FAFBFC] p-3">
          <div class="flex items-center gap-2">
            <button class="btn btn-outline flex-1" :disabled="deciding" @click="openReject">
              <AppIcon name="x" :size="14" />驳回修改
              <span class="kbd ml-auto">R</span>
            </button>
            <button class="btn btn-primary flex-1" :disabled="deciding" @click="approve">
              <AppIcon name="check" :size="14" />通过审核
              <span class="kbd ml-auto">A</span>
            </button>
          </div>
          <p class="mt-1.5 text-center text-[8.5px] text-[#9CA3AF]">
            签批动作将写入审计日志，并触发构件状态机流转
          </p>
        </div>
      </aside>
    </div>

    <!-- 驳回弹窗 -->
    <ModalShell
      :open="rejecting"
      title="驳回修改 · 填写结构化缺陷意见"
      :subtitle="current ? `${current.componentName} · ${current.reviewId}` : ''"
      icon="alertTriangle"
      width="md"
      @close="rejecting = false"
    >
      <div class="space-y-3 p-4">
        <div>
          <p class="eyebrow mb-1.5">缺陷分类（可多选）</p>
          <div class="flex flex-wrap gap-1.5">
            <button
              v-for="defect in ['几何非流形', '参数越界', '语义码未映射', '脚本安全', '算量偏差', '标准符合性']"
              :key="defect"
              class="chip transition-colors"
              :class="rejectDefects.includes(defect) ? '!border-danger !bg-[#FEF2F2] !text-danger' : 'hover:border-[#C7D2DA]'"
              @click="toggleDefect(defect)"
            >
              <AppIcon v-if="rejectDefects.includes(defect)" name="check" :size="10" />{{ defect }}
            </button>
          </div>
        </div>
        <div>
          <label class="label" for="reject-reason">实质性缺陷意见 <span class="text-danger">*</span></label>
          <textarea
            id="reject-reason"
            v-model="rejectReason"
            rows="5"
            class="input resize-none"
            placeholder="请指出具体部位、参数或标准条款问题，例如：墩顶垫石安装面凹槽深度 20mm 与 53-02-20 图集要求不一致，请核对后重新提交。"
          />
          <p class="mt-1 text-right font-mono text-[9px]" :class="rejectReason.trim().length < 5 ? 'text-danger' : 'text-success'">
            {{ rejectReason.trim().length }} / ≥5 字
          </p>
        </div>
      </div>
      <template #footer>
        <div class="flex items-center justify-between">
          <p class="text-[9.5px] text-muted">驳回后构件自动退回 draft 状态</p>
          <div class="flex items-center gap-2">
            <button class="btn btn-ghost btn-sm" @click="rejecting = false">取消</button>
            <button class="btn btn-danger btn-sm" :disabled="deciding || rejectReason.trim().length < 5" @click="submitReject">
              <AppIcon name="x" :size="12" />确认驳回
            </button>
          </div>
        </div>
      </template>
    </ModalShell>
  </div>
</template>
