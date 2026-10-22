<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import CadViewport from '@/components/viewer/CadViewport.vue';
import ExportFlowModal from '@/components/export/ExportFlowModal.vue';
import { api } from '@/api/client';
import type { ComponentDetail, SemanticMapping } from '@/api/types';
import { useEvaluation } from '@/composables/useEvaluation';
import { useUiStore } from '@/stores/ui';
import { useJobsStore } from '@/stores/jobs';
import { useSessionStore } from '@/stores/session';
import {
  componentStatusMeta,
  formatArea,
  formatNumber,
  formatVolume,
  maskKey,
  sourceModeMeta,
} from '@/utils/format';

const route = useRoute();
const router = useRouter();
const ui = useUiStore();
const session = useSessionStore();
const showTechnical = computed(() => session.canReview || session.canManageFamilies);
const jobs = useJobsStore();
const { jobDrawerOpen } = storeToRefs(ui);

const familyId = computed(() => String(route.params.familyId ?? 'PierBody'));
const detail = ref<ComponentDetail | null>(null);
const loadingDetail = ref(true);
const loadError = ref('');
const semantics = ref<SemanticMapping[]>([]);
const values = ref<Record<string, number | string | boolean>>({});
const currentModel = ref('');
const selectedShape = ref<{ index: number; key: string } | null>(null);
const hiddenShapes = ref<Set<string>>(new Set());
const exportOpen = ref(false);
const mobilePanel = ref<'tree' | 'params' | null>(null);
const autoDetached = ref(false);
const viewerRef = ref<InstanceType<typeof CadViewport> | null>(null);

const evaluation = useEvaluation();
const { decoded, status, progress, jobId, error: evalError, meta, elapsedMs } = evaluation;

let evaluateTimer: ReturnType<typeof setTimeout> | null = null;

const isParametric = computed(() => detail.value?.sourceMode === 'parametric');
const statusMeta = computed(() => (detail.value ? componentStatusMeta(detail.value.status) : null));
const sourceMeta = computed(() => (detail.value ? sourceModeMeta(detail.value.sourceMode) : null));
const shapeEntries = computed(() => {
  if (!detail.value) return [];
  const keys = Object.keys(detail.value.shapes);
  return keys.map((key, index) => {
    const shape = detail.value!.shapes[key];
    let depth = 0;
    let parent = shape.parent;
    while (parent && keys.includes(parent)) {
      depth += 1;
      parent = detail.value!.shapes[parent]?.parent;
    }
    return { key, index, ...shape, depth };
  });
});
const inoutParams = computed(
  () =>
    detail.value?.parameters.schema.filter(
      (p) => p.type !== 'enum' && p.access !== 'out' && p.name !== 'ModelNumber',
    ) ?? [],
);
const enumParams = computed(
  () => detail.value?.parameters.schema.filter((p) => p.type === 'enum' && p.name !== 'ModelNumber') ?? [],
);
const outParams = computed(() => detail.value?.parameters.schema.filter((p) => p.access === 'out') ?? []);
const selectedMapping = computed(() => {
  if (!selectedShape.value) return null;
  return semantics.value.find((m) => m.shapeKey === selectedShape.value!.key) ?? null;
});
const selectedOutput = computed(() => {
  if (!selectedShape.value || !decoded.value) return null;
  return decoded.value.header.outputs?.[selectedShape.value.key] ?? null;
});
const derivedVolume = computed(() => decoded.value?.header.stats?.volumeM3 ?? detail.value?.geometry.volumeM3 ?? 0);
const derivedArea = computed(() => decoded.value?.header.stats?.surfaceM2 ?? detail.value?.geometry.surfaceM2 ?? 0);
const bounds = computed(() => decoded.value?.header.bounds ?? detail.value?.geometry.bounds ?? null);
const bottomWidth = computed(() => {
  if (!detail.value || detail.value.geometryKind !== 'pier') return null;
  const height = Number(values.value.PierHeight ?? 0);
  const topWidth = Number(values.value.TopWidth ?? 0);
  const slope = Math.max(1, Number(values.value.Slope ?? 35));
  return topWidth + (height / slope) * 2;
});
const bottomLength = computed(() => {
  if (!detail.value || detail.value.geometryKind !== 'pier') return null;
  const height = Number(values.value.PierHeight ?? 0);
  const topLength = Number(values.value.TopLength ?? 0);
  const slope = Math.max(1, Number(values.value.Slope ?? 35));
  return topLength + (height / slope) * 2;
});

const busy = computed(() => ['loading', 'queued', 'running', 'detached_computing'].includes(status.value));
const statusLabel = computed(() => {
  switch (status.value) {
    case 'loading':
      return '请求权威几何…';
    case 'queued':
      return '任务排队中';
    case 'running':
      return 'Worker 求解中';
    case 'detached_computing':
      return '后台脱机计算';
    case 'succeeded':
      return meta.value?.cacheHit ? '预烘焙缓存命中' : '求解完成';
    case 'failed':
      return '求解失败';
    case 'aborted':
      return '已解除前台等待';
    default:
      return '待求值';
  }
});
const cacheKey = computed(() => decoded.value?.header.cache_key ?? detail.value?.geometry.defaultCacheKey ?? '');
const shapeLabels = computed<Record<string, string>>(() => {
  const map: Record<string, string> = {};
  for (const entry of shapeEntries.value) map[entry.key] = entry.label;
  return map;
});

function initValues(component: ComponentDetail, preferred?: Record<string, number | string | boolean>) {
  const preset =
    component.parameters.presets.find((p) => p.modelNumber === String(component.parameters.schema[0].default)) ??
    component.parameters.presets[0];
  const next: Record<string, number | string | boolean> = { ...preset.params, ...(preferred ?? {}) };
  for (const schema of component.parameters.schema) {
    if (next[schema.name] === undefined && schema.access !== 'out') next[schema.name] = schema.default;
  }
  values.value = next;
  currentModel.value = String(next.ModelNumber ?? preset.modelNumber);
}

function numericOverrides() {
  const overrides: Record<string, number | string | boolean> = {};
  for (const [key, value] of Object.entries(values.value)) {
    if (key === 'ModelNumber' || key === 'NetVolume') continue;
    overrides[key] = value;
  }
  return overrides;
}

function runEvaluate() {
  if (!detail.value || !isParametric.value) return;
  autoDetached.value = false;
  evaluation.evaluate({
    familyId: detail.value.familyId,
    modelNumber: currentModel.value,
    parameterOverrides: numericOverrides(),
  });
}

function scheduleEvaluate() {
  if (evaluateTimer) clearTimeout(evaluateTimer);
  evaluateTimer = setTimeout(runEvaluate, 340);
}

function onParamInput(name: string, value: number) {
  values.value = { ...values.value, [name]: value };
  currentModel.value = 'CUSTOM';
  scheduleEvaluate();
}

function onEnumChange(name: string, value: string) {
  values.value = { ...values.value, [name]: value };
  scheduleEvaluate();
}

function selectPreset(preset: { modelNumber: string; params: Record<string, number | string | boolean> }) {
  if (evaluateTimer) clearTimeout(evaluateTimer);
  currentModel.value = preset.modelNumber;
  values.value = { ...preset.params };
  if (!isParametric.value) {
    evaluation.loadGeometry(familyId.value, preset.modelNumber);
    return;
  }
  runEvaluate();
}

function onPick(info: { shapeIndex: number; shapeKey: string } | null) {
  if (!info || !detail.value) {
    selectedShape.value = null;
    return;
  }
  const key = info.shapeKey;
  if (!detail.value.shapes[key]) {
    selectedShape.value = { index: info.shapeIndex, key };
    return;
  }
  selectedShape.value = { index: info.shapeIndex, key };
}

function selectShape(entry: { key: string; index: number }) {
  selectedShape.value = { index: entry.index, key: entry.key };
  viewerRef.value?.highlightShape(entry.index);
}

function toggleShape(entry: { key: string; index: number }) {
  const next = new Set(hiddenShapes.value);
  if (next.has(entry.key)) next.delete(entry.key);
  else next.add(entry.key);
  hiddenShapes.value = next;
  viewerRef.value?.setShapeHidden(entry.index, next.has(entry.key));
}

async function cancelWaiting() {
  const message = await evaluation.cancelWaiting();
  ui.notify({ type: 'warning', title: '已取消前台等待', message });
  autoDetached.value = true;
  void jobs.refresh();
}

async function saveAndDownload() {
  exportOpen.value = true;
}

function openJobCenter() {
  jobDrawerOpen.value = true;
  void jobs.refresh();
}

watch(
  () => evaluation.elapsedMs.value,
  (ms) => {
    if (
      ms > 8000 &&
      !autoDetached.value &&
      (status.value === 'running' || status.value === 'queued')
    ) {
      autoDetached.value = true;
      void evaluation.cancelWaiting().then((message) => {
        ui.notify({
          type: 'warning',
          title: '超过 8.0s 前台等待上限',
          message: `${message}\n任务已收拢至计算任务中心，视口恢复自由操作。`,
          actionLabel: '打开任务中心',
          action: openJobCenter,
        });
      });
    }
  },
);

watch(decoded, (value) => {
  if (!value) return;
  if (selectedShape.value && !detail.value?.shapes[selectedShape.value.key]) selectedShape.value = null;
  viewerRef.value?.fit();
});

onMounted(async () => {
  try {
    detail.value = await api.component(familyId.value);
    loadingDetail.value = false;
    const semanticResponse = await api.semantics(familyId.value).catch(() => null);
    semantics.value = semanticResponse?.mappings ?? [];
    const jobQuery = String(route.query.job ?? '');
    const modelQuery = String(route.query.model ?? '');
    const preferredPreset = detail.value.parameters.presets.find(
      (preset) => preset.modelNumber === modelQuery,
    );
    if (jobQuery) {
      const job = await api.job(jobQuery).catch(() => null);
      initValues(detail.value, job?.params);
      await evaluation.loadJob(jobQuery);
      viewerRef.value?.fit();
      ui.notify({ type: 'info', title: '已从任务中心恢复计算结果', message: `任务 ${jobQuery}` });
    } else if (detail.value.sourceMode === 'parametric') {
      initValues(detail.value, preferredPreset?.params);
      runEvaluate();
    } else {
      initValues(detail.value, preferredPreset?.params);
      evaluation.loadGeometry(detail.value.familyId, currentModel.value);
    }
  } catch (loadFailure) {
    loadingDetail.value = false;
    loadError.value = loadFailure instanceof Error ? loadFailure.message : '构件加载失败';
  }
});

onBeforeUnmount(() => {
  if (evaluateTimer) clearTimeout(evaluateTimer);
});
</script>

<template>
  <div class="flex h-full min-h-0 flex-col bg-app">
    <!-- 顶栏 -->
    <header class="flex h-12 shrink-0 items-center gap-2 border-b border-line bg-surface px-3">
      <button class="btn btn-ghost btn-sm" @click="router.push(`/components/${familyId}`)">
        <AppIcon name="arrowLeft" :size="14" />返回详情
      </button>
      <span class="h-5 w-px bg-line" />
      <div class="flex min-w-0 items-center gap-2">
        <span class="flex h-6 w-6 items-center justify-center rounded bg-[#E8EFF4] text-primary">
          <AppIcon name="sliders" :size="13" />
        </span>
        <div class="min-w-0">
          <div class="flex items-center gap-2">
            <h1 class="truncate text-[13px] font-semibold text-ink">{{ detail?.name ?? '参数配置器' }}</h1>
            <StatusBadge v-if="statusMeta" :tone="statusMeta.tone" :label="statusMeta.label" />
            <span v-if="sourceMeta" class="hidden rounded bg-[#F1F4F7] px-1.5 py-[1px] text-[9px] text-[#6B7280] sm:inline">
              {{ sourceMeta.label }}
            </span>
          </div>
          <p class="truncate font-mono text-[9.5px] text-muted">
            FamilyId: {{ familyId }} · 版本 v{{ detail?.revision ?? '—' }}<template v-if="showTechnical"> · cacheKey {{ maskKey(cacheKey, 10, 4) }}</template>
          </p>
        </div>
      </div>

      <div class="flex-1" />

      <button class="btn btn-outline btn-sm lg:hidden" @click="mobilePanel = mobilePanel === 'tree' ? null : 'tree'">
        <AppIcon name="tree" :size="13" />部位树
      </button>
      <button class="btn btn-outline btn-sm lg:hidden" @click="mobilePanel = mobilePanel === 'params' ? null : 'params'">
        <AppIcon name="sliders" :size="13" />参数
      </button>

      <button
        v-if="jobId"
        class="hidden items-center gap-1.5 rounded border border-line bg-[#FAFBFC] px-2 py-1 text-[10px] text-[#4B5563] hover:border-[#C7D2DA] md:flex"
        @click="openJobCenter"
      >
        <AppIcon name="cpu" :size="11" />
        <span class="mono">{{ jobId.slice(-8) }}</span>
        <AppIcon name="chevronRight" :size="10" />
      </button>

      <button class="btn btn-outline btn-sm" @click="exportOpen = true">
        <AppIcon name="desktop" :size="13" />推送到桌面端
      </button>
      <button
        class="btn btn-primary btn-sm"
        title="下载当前标准型号的基准几何；自定义变参结果暂不打包"
        @click="saveAndDownload"
      >
        <AppIcon name="download" :size="13" />下载基准型号
        <AppIcon name="chevronDown" :size="11" />
      </button>
    </header>

    <div v-if="loadError" class="flex flex-1 items-center justify-center">
      <div class="panel p-6 text-center">
        <p class="text-[13px] font-medium text-danger">{{ loadError }}</p>
        <button class="btn btn-outline btn-sm mt-2" @click="router.push('/library')">返回构件大厅</button>
      </div>
    </div>

    <div v-else class="flex min-h-0 flex-1">
      <!-- 部位树 -->
      <aside
        class="min-h-0 shrink-0 flex-col border-r border-line bg-surface"
        :class="[
          mobilePanel === 'tree' ? 'fixed inset-y-0 left-0 z-50 flex w-[280px] shadow-drawer' : 'hidden xl:flex',
          'xl:w-[272px]',
        ]"
      >
        <div class="flex items-center justify-between border-b border-line px-3 py-2">
          <p class="text-[11px] font-semibold text-ink">构件结构与部位树</p>
          <button class="text-[#9CA3AF] hover:text-ink xl:hidden" @click="mobilePanel = null"><AppIcon name="close" :size="13" /></button>
        </div>

        <div class="min-h-0 flex-1 overflow-y-auto px-2 py-2 scroll-thin">
          <div class="mb-1 flex items-center gap-1.5 rounded bg-[#F3F6F8] px-2 py-1.5">
            <AppIcon name="cube" :size="13" class="text-primary" />
            <span class="min-w-0 flex-1 truncate text-[11.5px] font-semibold text-ink">{{ familyId }}</span>
            <span class="rounded bg-white px-1 py-[1px] text-[8.5px] text-muted">根</span>
          </div>

          <div
            v-for="entry in shapeEntries"
            :key="entry.key"
            class="group mt-0.5 flex items-center gap-1.5 rounded px-1.5 py-1.5 transition-colors"
            :class="selectedShape?.key === entry.key ? 'bg-[#E8EFF4]' : 'hover:bg-[#F5F7F9]'"
            :style="{ paddingLeft: `${8 + entry.depth * 14}px` }"
            @click="selectShape(entry)"
          >
            <button
              class="flex h-4 w-4 shrink-0 items-center justify-center rounded text-[#94A3B8] hover:text-ink"
              :title="hiddenShapes.has(entry.key) ? '显示部位' : '隐藏部位'"
              @click.stop="toggleShape(entry)"
            >
              <AppIcon :name="hiddenShapes.has(entry.key) ? 'eyeOff' : 'eye'" :size="12" :class="hiddenShapes.has(entry.key) ? 'opacity-30' : ''" />
            </button>
            <span
              class="flex h-4 w-4 shrink-0 items-center justify-center rounded-sm"
              :class="
                entry.geomType === 'solid'
                  ? 'bg-[#E8EFF4] text-primary'
                  : entry.geomType === 'face'
                    ? 'bg-[#F1F4F7] text-[#6B7280]'
                    : 'bg-[#F1F4F7] text-[#6B7280]'
              "
            >
              <AppIcon :name="entry.geomType === 'solid' ? 'cube' : entry.geomType === 'face' ? 'box' : 'route'" :size="9" />
            </span>
            <div class="min-w-0 flex-1">
              <p class="truncate text-[11px] font-medium" :class="selectedShape?.key === entry.key ? 'text-primary' : 'text-[#374151]'">
                {{ entry.key }}
              </p>
              <p class="truncate text-[9px] text-[#9CA3AF]">{{ entry.label }}</p>
            </div>
            <span class="mono shrink-0 text-[8.5px] text-[#9CA3AF]">
              <template v-if="decoded?.header.outputs?.[entry.key]?.area">
                {{ formatArea(Number(decoded.header.outputs[entry.key].area) / 1e6) }}
              </template>
              <template v-else-if="decoded?.header.outputs?.[entry.key]?.volume">
                {{ formatVolume(Number(decoded.header.outputs[entry.key].volume) / 1e9) }}
              </template>
            </span>
          </div>
        </div>

        <div v-if="showTechnical" class="border-t border-line px-3 py-2">
          <p class="eyebrow">端口定义 (Ports)</p>
          <div class="mt-1 space-y-1">
            <div v-for="(port, key) in detail?.ports ?? {}" :key="key" class="rounded border border-line bg-[#FAFBFC] px-2 py-1.5">
              <div class="flex items-center gap-1.5">
                <span class="h-1.5 w-1.5 rounded-full bg-hl" />
                <span class="mono text-[9.5px] font-semibold text-[#374151]">{{ key }}</span>
              </div>
              <p class="mt-0.5 truncate text-[9px] text-muted">{{ port.label }}</p>
              <p class="mono mt-0.5 text-[8.5px] text-[#9CA3AF]">
                O({{ port.origin.map((v) => Math.round(v)).join(', ') }}) · Z({{ port.axis.join(',') }})
              </p>
            </div>
          </div>
        </div>
      </aside>

      <!-- 视口 -->
      <section class="relative min-w-0 flex-1">
        <CadViewport
          ref="viewerRef"
          :decoded="decoded"
          :status="status"
          :progress="progress"
          :error="evalError"
          :highlight-shape="selectedShape?.index ?? null"
          :shape-labels="shapeLabels"
          :hud="showTechnical"
          @pick="onPick"
        />

        <!-- 任务状态条 -->
        <div class="pointer-events-none absolute left-3 top-3 z-20 flex flex-col gap-1.5">
          <div class="pointer-events-auto flex items-center gap-2 rounded border border-white/10 bg-[#0F1A2B]/88 px-2.5 py-1.5 backdrop-blur">
            <span
              class="h-1.5 w-1.5 rounded-full"
              :class="
                status === 'succeeded'
                  ? 'bg-[#4ADE80]'
                  : status === 'failed'
                    ? 'bg-[#F87171]'
                    : status === 'aborted' || status === 'detached_computing'
                      ? 'bg-[#FBBF24]'
                      : 'animate-pulse bg-hl'
              "
            />
            <span class="text-[10.5px] font-medium text-[#CBD5E1]">{{ statusLabel }}</span>
            <span v-if="busy" class="mono text-[9px] text-[#64748B]">{{ (elapsedMs / 1000).toFixed(1) }}s</span>
            <span v-if="showTechnical && meta?.mode === 'async'" class="rounded bg-[#1E3A5F] px-1.5 py-[1px] text-[8.5px] text-[#93C5FD]">异步管线</span>
            <button v-if="busy" class="ml-1 border-l border-white/10 pl-2 text-[9.5px] text-[#FCA5A5] hover:text-[#FECACA]" @click="cancelWaiting">
              取消等待
            </button>
            <button
              v-else-if="status === 'aborted' || status === 'detached_computing'"
              class="ml-1 border-l border-white/10 pl-2 text-[9.5px] text-[#FCD34D] hover:text-[#FDE68A]"
              @click="openJobCenter"
            >
              任务中心
            </button>
          </div>

          <div v-if="detail && !isParametric" class="pointer-events-auto max-w-[300px] rounded border border-[#78350F]/60 bg-[#451A03]/70 px-2.5 py-1.5 backdrop-blur">
            <p class="text-[10px] leading-relaxed text-[#FCD34D]">
              {{ sourceMeta?.hint }}：该构件为固定几何，仅提供只读预览与参数查看，不开放在线变参。
            </p>
          </div>
        </div>
      </section>

      <!-- 参数面板 -->
      <aside
        class="min-h-0 shrink-0 flex-col border-l border-line bg-surface"
        :class="[
          mobilePanel === 'params' ? 'fixed inset-y-0 right-0 z-50 flex w-[320px] shadow-drawer' : 'hidden lg:flex',
          'lg:w-[312px]',
        ]"
      >
        <div class="flex items-center justify-between border-b border-line px-3 py-2">
          <p class="text-[11px] font-semibold text-ink">参数配置与规格预设</p>
          <button class="text-[#9CA3AF] hover:text-ink lg:hidden" @click="mobilePanel = null"><AppIcon name="close" :size="13" /></button>
        </div>

        <div class="min-h-0 flex-1 overflow-y-auto scroll-thin">
          <div class="border-b border-[#F0F2F5] p-3">
            <div class="flex items-center justify-between">
              <p class="eyebrow">常用标准规格 (Presets)</p>
              <span class="rounded bg-[#ECFDF3] px-1.5 py-[1px] text-[8.5px] text-success">预烘焙</span>
            </div>
            <div class="mt-2 space-y-1">
              <button
                v-for="preset in detail?.parameters.presets ?? []"
                :key="preset.modelNumber"
                class="flex w-full items-center gap-2 rounded border px-2 py-1.5 text-left transition-all"
                :class="
                  currentModel === preset.modelNumber
                    ? 'border-primary bg-[#F3F8FB] ring-2 ring-primary/10'
                    : 'border-line bg-white hover:border-[#C7D2DA] hover:bg-[#FAFBFC]'
                "
                @click="selectPreset(preset)"
              >
                <span
                  class="flex h-3.5 w-3.5 items-center justify-center rounded-full border"
                  :class="currentModel === preset.modelNumber ? 'border-primary bg-primary text-white' : 'border-[#CBD5E1]'"
                >
                  <AppIcon v-if="currentModel === preset.modelNumber" name="check" :size="8" :stroke-width="3" />
                </span>
                <span class="min-w-0 flex-1">
                  <span class="mono block truncate text-[10.5px] font-semibold text-ink">{{ preset.modelNumber }}</span>
                  <span class="block truncate text-[9.5px] text-muted">{{ preset.label }}</span>
                </span>
              </button>
            </div>
          </div>

          <div class="p-3">
            <div class="flex items-center justify-between">
              <p class="eyebrow">几何尺寸调节</p>
              <span v-if="currentModel === 'CUSTOM'" class="rounded bg-[#FFFBEB] px-1.5 py-[1px] text-[8.5px] text-warning">自定义</span>
            </div>

            <div
              v-if="!isParametric"
              class="mt-2 rounded border border-line bg-[#FAFBFC] px-2.5 py-2 text-[10.5px] leading-relaxed text-muted"
            >
              该构件为导入几何，可在上方切换预定义规格；自定义参数不会重新生成几何。
            </div>

            <div v-for="param in inoutParams" :key="param.name" class="mt-3">
              <div class="flex items-baseline justify-between gap-2">
                <label class="text-[12px] font-medium text-[#374151]" :for="`p-${param.name}`">
                  {{ param.label }}
                  <span class="mono ml-0.5 text-[11px] font-normal text-[#9CA3AF]">{{ param.name }}</span>
                </label>
                <div class="flex items-center gap-1">
                  <input
                    :id="`p-${param.name}`"
                    type="number"
                    class="input mono !w-[84px] !px-2 !py-0.5 !text-right !text-[12px]"
                    :min="param.min"
                    :max="param.max"
                    :step="param.step ?? 1"
                    :value="Number(values[param.name] ?? param.default)"
                    :disabled="!isParametric"
                    @change="onParamInput(param.name, Number(($event.target as HTMLInputElement).value))"
                  />
                  <span class="w-[28px] text-[11px] text-muted">{{ param.unit }}</span>
                </div>
              </div>
              <input
                type="range"
                class="mt-1.5 h-1 w-full cursor-pointer appearance-none rounded-full bg-[#E5E7EB] accent-[#1F5A7A] disabled:cursor-not-allowed disabled:opacity-40"
                :min="param.min"
                :max="param.max"
                :step="param.step ?? 1"
                :value="Number(values[param.name] ?? param.default)"
                :disabled="!isParametric"
                @input="onParamInput(param.name, Number(($event.target as HTMLInputElement).value))"
              />
              <div class="mt-0.5 flex justify-between font-mono text-[10.5px] text-[#9CA3AF]">
                <span>{{ param.min }}</span>
                <span>{{ param.max }}</span>
              </div>
            </div>

            <div v-for="param in enumParams" :key="param.name" class="mt-3">
              <label class="text-[12px] font-medium text-[#374151]">{{ param.label }}</label>
              <select
                class="select mt-1 !py-1.5 !text-[12px]"
                :value="String(values[param.name] ?? param.default)"
                :disabled="!isParametric"
                @change="onEnumChange(param.name, ($event.target as HTMLSelectElement).value)"
              >
                <option v-for="option in param.options ?? []" :key="option" :value="option">{{ option }}</option>
              </select>
            </div>
          </div>

          <div class="border-t border-[#F0F2F5] p-3">
            <p class="eyebrow">派生计算（只读）</p>
            <div class="mt-1.5 grid grid-cols-2 gap-1.5">
              <div class="panel-inset px-2 py-1.5">
                <p class="text-[9px] text-muted">实体净体积</p>
                <p class="mono mt-0.5 text-[13px] font-semibold text-ink">{{ formatVolume(derivedVolume) }}</p>
              </div>
              <div class="panel-inset px-2 py-1.5">
                <p class="text-[9px] text-muted">表面积</p>
                <p class="mono mt-0.5 text-[13px] font-semibold text-ink">{{ formatArea(derivedArea) }}</p>
              </div>
              <template v-if="bottomWidth && bottomLength">
                <div class="panel-inset px-2 py-1.5">
                  <p class="text-[9px] text-muted">墩底宽度</p>
                  <p class="mono mt-0.5 text-[11px] font-semibold text-ink">{{ formatNumber(bottomWidth, 1) }} mm</p>
                </div>
                <div class="panel-inset px-2 py-1.5">
                  <p class="text-[9px] text-muted">墩底长度</p>
                  <p class="mono mt-0.5 text-[11px] font-semibold text-ink">{{ formatNumber(bottomLength, 1) }} mm</p>
                </div>
              </template>
              <div v-else-if="bounds" class="panel-inset col-span-2 px-2 py-1.5">
                <p class="text-[9px] text-muted">包围盒 (H × L × W)</p>
                <p class="mono mt-0.5 text-[10.5px] font-semibold text-ink">
                  {{ formatNumber((bounds.max[2] - bounds.min[2]) / 1000, 2) }} ×
                  {{ formatNumber((bounds.max[0] - bounds.min[0]) / 1000, 2) }} ×
                  {{ formatNumber((bounds.max[1] - bounds.min[1]) / 1000, 2) }} m
                </p>
              </div>
            </div>
            <div v-for="param in outParams" v-show="param.name !== 'NetVolume'" :key="param.name" class="mt-1.5 flex items-center justify-between rounded bg-[#FAFBFC] px-2 py-1">
              <span class="text-[9.5px] text-muted">{{ param.label }}</span>
              <span class="mono text-[10px] text-[#374151]">
                {{ values[param.name] ?? param.default }}
                {{ param.unit }}
              </span>
            </div>
          </div>

          <div v-if="showTechnical" class="border-t border-[#F0F2F5] p-3">
            <p class="eyebrow">确定性缓存</p>
            <div class="mt-1.5 rounded border border-line bg-[#FAFBFC] px-2 py-1.5">
              <p class="text-[9px] text-muted">权威 cacheKey</p>
              <p class="mono mt-0.5 break-all text-[9px] leading-relaxed text-[#374151]" :title="cacheKey">{{ cacheKey }}</p>
            </div>
            <div class="mt-1.5 flex flex-wrap gap-1">
              <span class="tag mono">{{ meta?.cacheHit ? 'Prebaked 命中' : meta?.mode === 'async' ? 'Worker 动态求解' : '待求值' }}</span>
              <span class="tag mono" :class="decoded?.zeroCopy ? 'text-success' : decoded ? 'text-warning' : ''">
                {{ decoded?.zeroCopy ? 'Zero-Copy' : decoded ? 'Aligned Copy' : '—' }}
              </span>
              <span v-if="decoded" class="tag mono">{{ decoded.header.protocolVersion }}</span>
            </div>
            <p class="mt-1.5 text-[9px] leading-relaxed text-[#9CA3AF]">
              参数提交后自动刷新缓存键。
            </p>
          </div>
        </div>
      </aside>

      <div v-if="mobilePanel" class="fixed inset-0 z-40 bg-[#0B1220]/40 lg:hidden" @click="mobilePanel = null" />
    </div>

    <!-- 底部映射栏 -->
    <footer class="flex min-h-[38px] shrink-0 flex-wrap items-center gap-x-3 gap-y-1 border-t border-line bg-surface px-3 py-1.5">
      <template v-if="selectedShape && selectedMapping">
        <span class="flex items-center gap-1 text-[10px] font-semibold uppercase tracking-wider text-primary">
          <AppIcon :name="selectedMapping.status === 'unmapped' ? 'alertTriangle' : 'link'" :size="11" />
          属性与外部标准映射
        </span>
        <span class="h-3 w-px bg-line" />
        <span class="text-[11px] font-medium text-ink">{{ selectedShape.key }}</span>
        <span class="mono text-[10px] text-muted">→ {{ selectedMapping.code }}</span>
        <span
          class="rounded px-1.5 py-[1px] text-[9.5px]"
          :class="selectedMapping.status === 'unmapped' ? 'bg-[#FFFBEB] text-warning' : 'bg-[#EFF6FF] text-[#1D4ED8]'"
        >
          {{ selectedMapping.status === 'unmapped' ? 'unmapped 未映射' : `${selectedMapping.ifc?.entity} · ${selectedMapping.ifc?.shapeAspect}` }}
        </span>
        <span class="text-[10px] text-[#6B7280]">CRBIM: {{ selectedMapping.crbim }}</span>
        <span v-if="selectedOutput?.area" class="mono text-[10px] text-[#374151]">
          面积 {{ formatArea(Number(selectedOutput.area) / 1e6) }}
        </span>
        <span v-else-if="selectedOutput?.volume" class="mono text-[10px] text-[#374151]">
          体积 {{ formatVolume(Number(selectedOutput.volume) / 1e9) }}
        </span>
        <button class="ml-auto text-[9.5px] text-[#9CA3AF] hover:text-ink" @click="selectedShape = null">
          清除选择
        </button>
      </template>
      <template v-else>
        <span class="flex items-center gap-1.5 text-[10px] text-muted">
          <AppIcon name="crosshair" :size="11" class="text-[#9CA3AF]" />
          点击视口拓扑面 / 特征边，或点击左侧部位树节点 —— 毫秒级反查 shapeKeys 与 IFC 算量属性
        </span>
        <span class="ml-auto hidden font-mono text-[9px] text-[#B0B8C1] sm:inline">
          Raycast → 面拾取 → 几何面 / 特征线映射
        </span>
      </template>
    </footer>

    <ExportFlowModal
      :open="exportOpen"
      :component="detail"
      :model-number="String(values.ModelNumber ?? '')"
      @close="exportOpen = false"
    />
  </div>
</template>
