<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from 'vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import { ViewerEngine, type ClipAxis, type MeasureResult, type PickInfo, type ProjectionMode, type RenderMode, type ViewPreset } from '@/three/ViewerEngine';
import type { DecodedJhb } from '@/jhb/decode';
import { formatNumber, maskKey } from '@/utils/format';

const props = withDefaults(
  defineProps<{
    decoded: DecodedJhb | null;
    status: string;
    progress?: number;
    error?: string;
    compact?: boolean;
    toolbar?: boolean;
    hud?: boolean;
    highlightShape?: number | null;
    compareDecoded?: DecodedJhb | null;
    diffChanges?: { shapeKey: string; kind: 'added' | 'removed' | 'modified' }[];
    shapeLabels?: Record<string, string>;
    namedParts?: { key: string; label: string }[];
    selectedPart?: string | null;
    waitingText?: string;
  }>(),
  {
    progress: 0,
    compact: false,
    toolbar: true,
    hud: true,
    highlightShape: null,
    compareDecoded: null,
    diffChanges: () => [],
    namedParts: () => [],
    selectedPart: null,
    waitingText: '正在请求权威几何…',
  },
);

const emit = defineEmits<{
  pick: [info: PickInfo | null];
  measure: [result: MeasureResult | null];
  reinit: [];
  selectPart: [key: string];
}>();

const host = ref<HTMLDivElement | null>(null);
const shell = ref<HTMLDivElement | null>(null);
const engine = shallowRef<ViewerEngine | null>(null);
const view = ref<ViewPreset>('iso');
const projection = ref<ProjectionMode>('perspective');
const renderMode = ref<RenderMode>('shaded');
const edgesVisible = ref(true);
const lineWidth = ref(1.7);
const clipAxis = ref<ClipAxis | null>(null);
const clipRatio = ref(76);
const measureMode = ref(false);
const measure = ref<MeasureResult | null>(null);
const pick = ref<PickInfo | null>(null);
const axes2d = ref<{ x: [number, number]; y: [number, number]; z: [number, number] } | null>(null);
const contextLost = ref(false);
const fullscreen = ref(false);
const showTools = ref(true);
const showNamedParts = ref(true);
const hiddenPartKeys = ref<Set<string>>(new Set());
function partShapeIndex(key: string) {
  return props.decoded?.header.shapeKeys.indexOf(key) ?? -1;
}
function isPartVisible(key: string) {
  return !hiddenPartKeys.value.has(key);
}
function applyPartVisibility() {
  if (!props.decoded) return;
  for (const part of props.namedParts) {
    const index = partShapeIndex(part.key);
    if (index >= 0) engine.value?.setShapeHidden(index, hiddenPartKeys.value.has(part.key));
  }
}
function setPartVisible(key: string, visible: boolean) {
  const next = new Set(hiddenPartKeys.value);
  if (visible) next.delete(key);
  else next.add(key);
  hiddenPartKeys.value = next;
  const index = partShapeIndex(key);
  if (index >= 0) engine.value?.setShapeHidden(index, !visible);
}
const showDetails = ref(false);
const ready = ref(false);

const busy = computed(() =>
  ['loading', 'queued', 'running', 'detached_computing'].includes(props.status),
);
const shapeLabel = computed(() => {
  if (!pick.value) return '';
  return props.shapeLabels?.[pick.value.shapeKey] ?? pick.value.shapeKey;
});
const gizmoAxes = computed(() => {
  const axes = axes2d.value;
  if (!axes) return [];
  return [
    { key: 'X', color: '#F87171', dx: axes.x[0], dy: -axes.x[1] },
    { key: 'Y', color: '#4ADE80', dx: axes.y[0], dy: -axes.y[1] },
    { key: 'Z', color: '#60A5FA', dx: axes.z[0], dy: -axes.z[1] },
  ];
});
const clipRangeLabel = computed(() => {
  if (!props.decoded || !clipAxis.value) return '';
  const axis = clipAxis.value;
  const index = axis === 'x' ? 0 : axis === 'y' ? 1 : 2;
  const min = props.decoded.header.bounds.min[index];
  const max = props.decoded.header.bounds.max[index];
  return `${Math.round(min + (max - min) * (clipRatio.value / 100))} mm`;
});

function buildEngine() {
  if (!host.value) return;
  engine.value?.dispose();
  engine.value = new ViewerEngine(host.value, {
    onPick: (info) => {
      pick.value = info;
      emit('pick', info);
    },
    onMeasure: (result) => {
      measure.value = result;
      emit('measure', result);
    },
    onContextLost: () => {
      contextLost.value = true;
    },
    onContextRestored: () => {
      contextLost.value = false;
    },
    onGeometryLoaded: () => {
      ready.value = true;
      applyPartVisibility();
      if (props.highlightShape !== null) engine.value?.highlightShape(props.highlightShape);
    },
    onAxesChange: (axes) => {
      axes2d.value = axes;
    },
    onError: (message) => {
      console.warn('[viewer]', message);
    },
  });
}

function applyDecoded(decoded: DecodedJhb | null) {
  if (!decoded || !engine.value) return;
  if (props.compareDecoded) {
    engine.value.setCompareGeometry(decoded, props.compareDecoded, props.diffChanges);
  } else {
    engine.value.setGeometry(decoded);
  }
  engine.value.setEdgesVisible(edgesVisible.value);
  engine.value.setLineWidth(lineWidth.value);
  engine.value.setRenderMode(renderMode.value);
  if (clipAxis.value) engine.value.setClippingAxis(clipAxis.value);
  if (props.highlightShape !== null) engine.value.highlightShape(props.highlightShape);
  applyPartVisibility();
}

function setView(next: ViewPreset) {
  view.value = next;
  engine.value?.setView(next);
}

function toggleProjection() {
  projection.value = projection.value === 'perspective' ? 'orthographic' : 'perspective';
  engine.value?.setProjection(projection.value);
}

function toggleEdges() {
  edgesVisible.value = !edgesVisible.value;
  engine.value?.setEdgesVisible(edgesVisible.value);
}

const renderModeLabel = computed(() => (renderMode.value === 'shaded' ? '着色' : renderMode.value === 'wireframe' ? '线框' : 'X-Ray'));

function cycleRenderMode() {
  renderMode.value =
    renderMode.value === 'shaded' ? 'wireframe' : renderMode.value === 'wireframe' ? 'xray' : 'shaded';
  engine.value?.setRenderMode(renderMode.value);
}

function cycleLineWidth() {
  const options = [1.2, 1.7, 2.2, 2.8];
  const index = options.indexOf(lineWidth.value);
  lineWidth.value = options[(index + 1) % options.length];
  engine.value?.setLineWidth(lineWidth.value);
}

function setClip(axis: ClipAxis | null) {
  clipAxis.value = axis;
  engine.value?.setClippingAxis(axis);
  if (axis) engine.value?.setClippingRatio(clipRatio.value / 100);
}

function onClipInput(event: Event) {
  clipRatio.value = Number((event.target as HTMLInputElement).value);
  engine.value?.setClippingRatio(clipRatio.value / 100);
}

function toggleMeasure() {
  measureMode.value = !measureMode.value;
  engine.value?.setMeasureMode(measureMode.value);
  if (measureMode.value) {
    pick.value = null;
    emit('pick', null);
  } else {
    engine.value?.clearMeasurements();
    measure.value = null;
  }
}

function clearMeasure() {
  engine.value?.clearMeasurements();
  measure.value = null;
}

function fit() {
  engine.value?.setView(view.value);
}

function toggleFullscreen() {
  if (!shell.value) return;
  if (!document.fullscreenElement) void shell.value.requestFullscreen?.();
  else void document.exitFullscreen?.();
}

function onFullscreenChange() {
  fullscreen.value = document.fullscreenElement === shell.value;
  setTimeout(() => engine.value?.resize(), 80);
}

function reinit() {
  contextLost.value = false;
  engine.value?.hardReinit();
  ready.value = false;
  emit('reinit');
  setTimeout(() => {
    if (props.decoded) applyDecoded(props.decoded);
  }, 100);
}

onMounted(() => {
  buildEngine();
  document.addEventListener('fullscreenchange', onFullscreenChange);
  if (props.decoded) applyDecoded(props.decoded);
});

onBeforeUnmount(() => {
  document.removeEventListener('fullscreenchange', onFullscreenChange);
  engine.value?.dispose();
  engine.value = null;
});

watch(
  () => props.decoded,
  (decoded) => {
    ready.value = false;
    applyDecoded(decoded);
  },
);

watch(
  () => props.highlightShape,
  (shape) => {
    if (!ready.value) return;
    engine.value?.highlightShape(shape ?? null);
  },
);

watch(
  () => props.decoded,
  () => {
    hiddenPartKeys.value = new Set();
    applyPartVisibility();
  },
);

watch(
  () => props.namedParts,
  () => applyPartVisibility(),
  { deep: true },
);

watch(
  [() => props.compareDecoded, () => props.diffChanges],
  () => {
    if (!props.decoded) return;
    applyDecoded(props.decoded);
  },
);

defineExpose({
  setView,
  fit,
  reinit,
  engine,
  setShapeHidden(shapeIndex: number, hidden: boolean) {
    engine.value?.setShapeHidden(shapeIndex, hidden);
  },
  highlightShape(shapeIndex: number | null) {
    engine.value?.highlightShape(shapeIndex);
  },
});
</script>

<template>
  <div ref="shell" class="viewer-dark relative h-full w-full overflow-hidden">

    <!-- 坐标轴罗盘：随相机姿态旋转 -->
    <div
      v-if="gizmoAxes.length"
      class="pointer-events-none absolute bottom-3 left-3 z-20 flex h-[82px] w-[82px] items-center justify-center rounded-full border border-white/10 bg-[#0F1A2B]/55 backdrop-blur-sm"
      title="世界坐标轴：X 红 / Y 绿 / Z 蓝"
    >
      <svg width="76" height="76" viewBox="-38 -38 76 76" aria-label="三维坐标轴">
        <circle cx="0" cy="0" r="34" fill="none" stroke="rgba(148,163,184,0.16)" />
        <line
          v-for="axis in gizmoAxes"
          :key="axis.key"
          x1="0"
          y1="0"
          :x2="axis.dx * 27"
          :y2="axis.dy * 27"
          :stroke="axis.color"
          stroke-width="2"
          stroke-linecap="round"
        />
        <text
          v-for="axis in gizmoAxes"
          :key="`${axis.key}-label`"
          :x="axis.dx * 32"
          :y="axis.dy * 32 + 3"
          :fill="axis.color"
          font-size="9"
          font-weight="700"
          text-anchor="middle"
        >{{ axis.key }}</text>
        <circle cx="0" cy="0" r="2.2" fill="#CBD5E1" />
      </svg>
    </div>
    <div ref="host" class="absolute inset-0" />

    <!-- 空态 -->
    <div
      v-if="!decoded && !busy"
      class="pointer-events-none absolute inset-0 flex flex-col items-center justify-center gap-3 text-center"
    >
      <span class="flex h-16 w-16 items-center justify-center rounded-full border border-white/10 bg-white/5 text-[#64748B]">
        <AppIcon name="cube" :size="30" :stroke-width="1.3" />
      </span>
      <p class="text-[13px] font-medium text-[#CBD5E1]">三维视口待加载</p>
      <p class="max-w-xs text-[11px] leading-relaxed text-[#64748B]">
        选择标准型号或调整变参后，系统将通过 JHB 二进制流零拷贝送显
      </p>
    </div>

    <!-- 加载遮罩 -->
    <Transition
      enter-active-class="transition duration-200"
      enter-from-class="opacity-0"
      leave-active-class="transition duration-150"
      leave-to-class="opacity-0"
    >
      <div v-if="busy" class="absolute inset-0 z-20 flex items-center justify-center bg-[#0B1220]/55 backdrop-blur-[1px]">
        <div class="w-[280px] rounded-md border border-white/10 bg-[#0F1A2B]/90 p-4 shadow-[0_16px_40px_-12px_rgba(0,0,0,0.6)]">
          <div class="flex items-center gap-2 text-[#CBD5E1]">
            <span class="h-3.5 w-3.5 animate-spin rounded-full border-2 border-white/15 border-t-hl" />
            <span class="text-[12px] font-medium">
              {{ status === 'queued' ? '任务排队中…' : status === 'running' || status === 'detached_computing' ? '几何 Worker 求解中…' : waitingText }}
            </span>
          </div>
          <div class="mt-3 h-1 overflow-hidden rounded-full bg-white/10">
            <div class="h-full rounded-full bg-hl transition-all duration-500" :style="{ width: `${Math.max(6, progress)}%` }" />
          </div>
          <div class="mt-1.5 flex items-center justify-between font-mono text-[9.5px] text-[#64748B]">
            <span>{{ status === 'detached_computing' ? '后台计算' : '几何求解' }}</span>
            <span>{{ progress }}%</span>
          </div>
        </div>
      </div>
    </Transition>

    <!-- 错误 -->
    <div v-if="status === 'failed'" class="absolute inset-x-4 bottom-4 z-30 flex items-start gap-2 rounded-md border border-[#7F1D1D] bg-[#2A1215]/95 px-3 py-2">
      <AppIcon name="alertTriangle" :size="15" class="mt-0.5 text-[#FCA5A5]" />
      <div class="min-w-0 flex-1">
        <p class="text-[11.5px] font-semibold text-[#FECACA]">几何求解失败</p>
        <p class="mt-0.5 break-words text-[10.5px] leading-relaxed text-[#FCA5A5]">{{ error }}</p>
      </div>
    </div>

    <!-- 顶部工具条 -->
    <div v-if="toolbar && showTools" class="pointer-events-none absolute inset-x-0 top-0 z-20 flex items-start justify-end gap-2 p-2.5">
      <div class="pointer-events-auto flex max-w-full flex-wrap items-center gap-1 rounded-[6px] border border-white/10 bg-[#0F1A2B]/88 p-1 shadow-[0_8px_24px_-8px_rgba(0,0,0,0.6)] backdrop-blur">
        <template v-if="!compact">
          <button class="toolbar-btn" :class="{ 'toolbar-btn-active': view === 'iso' }" @click="setView('iso')">轴测</button>
          <button class="toolbar-btn" :class="{ 'toolbar-btn-active': view === 'front' }" @click="setView('front')">正视</button>
          <button class="toolbar-btn" :class="{ 'toolbar-btn-active': view === 'side' }" @click="setView('side')">侧视</button>
          <button class="toolbar-btn" :class="{ 'toolbar-btn-active': view === 'top' }" @click="setView('top')">俯视</button>
          <span class="mx-0.5 h-4 w-px bg-white/10" />
        </template>
        <button class="toolbar-btn" :class="{ 'toolbar-btn-active': projection === 'orthographic' }" :title="projection === 'perspective' ? '切换为正交投影' : '切换为透视投影'" @click="toggleProjection">
          <AppIcon name="maximize" :size="12" />
          <span class="hidden sm:inline">{{ projection === 'perspective' ? '透视' : '正交' }}</span>
        </button>
        <button
          class="toolbar-btn"
          :class="{ 'toolbar-btn-active': edgesVisible, 'opacity-40': renderMode === 'wireframe' }"
          :disabled="renderMode === 'wireframe'"
          :title="renderMode === 'wireframe' ? '线框模式固定显示特征边' : 'CAD 矢量黑边'"
          @click="toggleEdges"
        >
          <AppIcon name="box" :size="12" /><span class="hidden sm:inline">黑边</span>
        </button>
        <button
          class="toolbar-btn"
          :class="{ 'toolbar-btn-active': renderMode !== 'shaded' }"
          :title="`渲染模式：${renderModeLabel}（点击切换 着色 → 线框 → X-Ray）`"
          @click="cycleRenderMode"
        >
          <AppIcon :name="renderMode === 'wireframe' ? 'grid' : renderMode === 'xray' ? 'layers' : 'box'" :size="12" />
          <span class="hidden sm:inline">{{ renderModeLabel }}</span>
        </button>
        <button v-if="!compact" class="toolbar-btn" :title="`线宽 ${lineWidth.toFixed(1)}px，点击切换`" @click="cycleLineWidth">
          <span class="mono">{{ lineWidth.toFixed(1) }}</span>
        </button>
        <span class="mx-0.5 h-4 w-px bg-white/10" />
        <div class="flex items-center gap-0.5">
          <button
            v-for="axis in (['x', 'y', 'z'] as ClipAxis[])"
            :key="axis"
            class="toolbar-btn uppercase"
            :class="{ 'toolbar-btn-active': clipAxis === axis }"
            :title="`沿 ${axis.toUpperCase()} 轴剖切`"
            @click="setClip(clipAxis === axis ? null : axis)"
          >
            {{ axis }}
          </button>
          <button class="toolbar-btn" :class="{ 'toolbar-btn-active': clipAxis === null }" title="关闭剖切" @click="setClip(null)">
            <AppIcon name="close" :size="11" />
          </button>
        </div>
        <button class="toolbar-btn" :class="{ 'toolbar-btn-active': measureMode }" title="点对点测距（端点吸附）" @click="toggleMeasure">
          <AppIcon name="measure" :size="12" /><span class="hidden sm:inline">测距</span>
        </button>
        <button
          v-if="namedParts.length"
          class="toolbar-btn"
          :class="{ 'toolbar-btn-active': showNamedParts }"
          :title="showNamedParts ? '隐藏命名部位' : '显示命名部位'"
          @click="showNamedParts = !showNamedParts"
        >
          <AppIcon name="list" :size="12" /><span class="hidden sm:inline">部位</span>
        </button>
        <span class="mx-0.5 h-4 w-px bg-white/10" />
        <button class="toolbar-btn" title="适配视口" @click="fit"><AppIcon name="crosshair" :size="12" /></button>
        <button class="toolbar-btn" :title="fullscreen ? '退出全屏' : '全屏查看'" @click="toggleFullscreen">
          <AppIcon :name="fullscreen ? 'minimize' : 'maximize'" :size="12" />
        </button>
        <button class="toolbar-btn" title="收起工具条" @click="showTools = false">
          <AppIcon name="chevronUp" :size="12" />
        </button>
      </div>
    </div>

    <!-- 剖切滑杆 -->
    <div
      v-if="clipAxis"
      class="absolute left-1/2 top-14 z-20 flex -translate-x-1/2 items-center gap-2 rounded-full border border-white/10 bg-[#0F1A2B]/92 px-3 py-1.5 shadow-lift backdrop-blur"
    >
      <span class="shrink-0 rounded bg-hl/15 px-1.5 py-0.5 font-mono text-[9.5px] font-semibold uppercase text-hl">{{ clipAxis }} 剖切</span>
      <input
        type="range"
        min="2"
        max="98"
        :value="clipRatio"
        class="h-1 w-[230px] cursor-pointer appearance-none rounded-full bg-white/15 accent-[#38BDF8]"
        @input="onClipInput"
      />
      <span class="mono w-[58px] shrink-0 text-right text-[10px] font-medium tabular-nums text-[#CBD5E1]">{{ clipRangeLabel }}</span>
    </div>

    <!-- 展开工具条 -->
    <button
      v-if="toolbar && !showTools"
      class="absolute right-3 top-3 z-20 rounded border border-white/10 bg-[#0F1A2B]/90 px-2 py-1 text-[10px] text-[#94A3B8] hover:text-white"
      @click="showTools = true"
    >
      显示工具条
    </button>

    <!-- 双模型对比图例 -->
    <div
      v-if="compareDecoded"
      class="absolute left-1/2 top-3 z-10 flex -translate-x-1/2 flex-wrap items-center justify-center gap-3 rounded-full border border-white/10 bg-[#0F1A2B]/90 px-3 py-1.5 text-[9.5px] text-[#CBD5E1] backdrop-blur"
    >
      <span class="flex items-center gap-1"><span class="h-2 w-2 rounded-sm bg-[#94A3B8]/50" />旧版（灰度半透明）</span>
      <span class="flex items-center gap-1"><span class="h-2 w-2 rounded-sm bg-[#22C55E]" />新增部位</span>
      <span class="flex items-center gap-1"><span class="h-2 w-2 rounded-sm bg-[#F59E0B]" />尺寸变形</span>
      <span class="flex items-center gap-1"><span class="h-2 w-2 rounded-sm bg-[#EF4444]" />删除部位</span>
    </div>

    <!-- 命名部位列表 -->
    <div
      v-if="showNamedParts && namedParts.length"
      class="absolute left-3 top-14 z-20 max-h-[min(300px,calc(100%-72px))] w-[232px] overflow-hidden rounded-md border border-white/10 bg-[#0F1A2B]/92 shadow-lift backdrop-blur"
    >
      <div class="flex items-center justify-between border-b border-white/10 px-2.5 py-1.5">
        <p class="text-[9px] font-semibold uppercase tracking-wider text-[#94A3B8]">命名部位</p>
        <span class="mono text-[8px] text-[#64748B]">{{ namedParts.length }}</span>
      </div>
      <div class="max-h-[240px] overflow-y-auto p-1 scroll-thin">
        <div
          v-for="part in namedParts"
          :key="part.key"
          class="flex w-full items-center gap-1.5 rounded px-1.5 py-1 transition-colors"
          :class="[
            selectedPart === part.key ? 'bg-hl/15' : 'hover:bg-white/10',
            isPartVisible(part.key) ? '' : 'opacity-45',
          ]"
        >
          <input
            type="checkbox"
            class="h-3 w-3 shrink-0 accent-[#38BDF8]"
            :checked="isPartVisible(part.key)"
            :aria-label="`显示 ${part.label}`"
            @change="setPartVisible(part.key, ($event.target as HTMLInputElement).checked)"
          />
          <button
            class="flex min-w-0 flex-1 items-center gap-1.5 text-left"
            :class="selectedPart === part.key ? 'text-white' : 'text-[#CBD5E1]'"
            :title="part.label"
            @click="emit('selectPart', part.key)"
          >
            <span class="h-1 w-1 shrink-0 rounded-full" :class="selectedPart === part.key ? 'bg-hl' : 'bg-[#64748B]'" />
            <span class="min-w-0 flex-1 truncate text-[10px]">{{ part.label }}</span>
          </button>
        </div>
      </div>
    </div>

    <!-- 底部扩展工具（由不同页面注入，如审核模式切换） -->
    <div
      v-if="$slots['bottom-tools']"
      class="pointer-events-none absolute inset-x-0 bottom-3 z-20 flex justify-center"
    >
      <slot name="bottom-tools" />
    </div>

    <!-- HUD -->
    <div v-if="hud && decoded" class="pointer-events-none absolute right-3 z-10 flex flex-col items-end gap-1" :class="pick || measure ? 'bottom-[96px]' : 'bottom-3'">
      <div class="flex flex-wrap items-center gap-1.5 text-[9.5px]">
        <span class="rounded border border-white/10 bg-[#0F1A2B]/80 px-1.5 py-[3px] font-mono text-[#94A3B8]">
          {{ formatNumber(decoded.header.triangleCount) }} 三角面
        </span>
        <span class="rounded border border-white/10 bg-[#0F1A2B]/80 px-1.5 py-[3px] font-mono text-[#94A3B8]">
          {{ formatNumber(decoded.header.vertexCount) }} 顶点
        </span>
        <span class="rounded border border-white/10 bg-[#0F1A2B]/80 px-1.5 py-[3px] font-mono text-[#94A3B8]">
          {{ decoded.header.facesCount }} 拓扑面
        </span>
        <span
          class="rounded border px-1.5 py-[3px] font-mono"
          :class="decoded.zeroCopy ? 'border-[#14532D] bg-[#052E16]/80 text-[#86EFAC]' : 'border-[#78350F] bg-[#451A03]/80 text-[#FCD34D]'"
        >
          {{ decoded.zeroCopy ? '数据直读' : '兼容读取' }}
        </span>
        <span
          v-if="decoded.header.cacheHit"
          class="rounded border border-[#14532D] bg-[#052E16]/80 px-1.5 py-[3px] font-mono text-[#86EFAC]"
        >预烘焙命中</span>
      </div>
      <p class="font-mono text-[9px] text-[#64748B]">
        cacheKey {{ maskKey(decoded.header.cache_key, 10, 6) }} · JHB {{ decoded.header.protocolVersion }} ·
        payload@{{ decoded.payloadStart }}
      </p>
    </div>

    <!-- 拾取结果 -->
    <Transition
      enter-active-class="transition duration-150 ease-industrial"
      enter-from-class="translate-y-2 opacity-0"
      leave-active-class="transition duration-100"
      leave-to-class="opacity-0"
    >
      <div
        v-if="pick && !measure"
        class="absolute bottom-3 right-3 z-20 w-[250px] rounded-md border border-hl/30 bg-[#0F1A2B]/95 px-3 py-2 shadow-lift"
      >
        <div class="flex items-start justify-between gap-2">
          <div class="min-w-0">
            <p class="flex items-center gap-1 text-[9px] font-semibold uppercase tracking-wider text-hl">
              <AppIcon :name="pick.kind === 'face' ? 'box' : 'route'" :size="10" />
              {{ pick.kind === 'face' ? `拓扑面 #${pick.faceId}` : `特征边 #${pick.edgeIndex}` }}
            </p>
            <p class="mt-0.5 truncate text-[12px] font-medium text-white">{{ shapeLabel }}</p>
            <p class="truncate font-mono text-[9.5px] text-[#94A3B8]">{{ pick.shapeKey }}</p>
          </div>
          <button class="text-[#64748B] hover:text-white" @click="pick = null; emit('pick', null)">
            <AppIcon name="close" :size="12" />
          </button>
        </div>
        <p class="mt-1 font-mono text-[9px] text-[#64748B]">
          P({{ pick.point.map((v) => Math.round(v)).join(', ') }})
        </p>
      </div>
    </Transition>

    <!-- 测距结果 -->
    <Transition
      enter-active-class="transition duration-150 ease-industrial"
      enter-from-class="translate-y-2 opacity-0"
      leave-active-class="transition duration-100"
      leave-to-class="opacity-0"
    >
      <div
        v-if="measure"
        class="absolute bottom-3 right-3 z-20 w-[224px] rounded-md border border-hl/30 bg-[#0F1A2B]/95 px-3 py-2 shadow-lift"
      >
        <div class="flex items-center justify-between">
          <p class="flex items-center gap-1 text-[9px] font-semibold uppercase tracking-wider text-hl">
            <AppIcon name="measure" :size="10" />端点吸附测距
          </p>
          <button class="text-[#64748B] hover:text-white" @click="clearMeasure">
            <AppIcon name="close" :size="12" />
          </button>
        </div>
        <p class="mt-1 font-mono text-[19px] font-semibold leading-none text-white">
          {{ formatNumber(measure.distance, 0) }} <span class="text-[10px] font-normal text-[#94A3B8]">mm</span>
        </p>
        <div class="mt-1.5 space-y-0.5 font-mono text-[9.5px] text-[#94A3B8]">
          <p class="flex justify-between"><span>水平投影</span><span>{{ formatNumber(measure.horizontal, 1) }} mm</span></p>
          <p class="flex justify-between"><span>高差 ΔZ</span><span>{{ formatNumber(measure.deltaZ, 1) }} mm</span></p>
        </div>
      </div>
    </Transition>

    <!-- 测距模式提示 -->
    <div v-if="measureMode && !measure" class="absolute bottom-[76px] right-3 z-20 rounded-full border border-hl/30 bg-[#0F1A2B]/92 px-3 py-1.5 text-[10.5px] text-[#CBD5E1]">
      点击模型拾取 A 点，再次点击拾取 B 点 · 自动吸附端点/锚点
    </div>

    <!-- WebGL 上下文丢失 -->
    <div v-if="contextLost" class="absolute inset-0 z-40 flex items-center justify-center bg-[#0B1220]/85 p-6 backdrop-blur">
      <div class="max-w-sm rounded-lg border border-white/10 bg-[#111C2B] p-5 text-center">
        <div class="mx-auto flex h-11 w-11 items-center justify-center rounded-full border border-[#78350F] bg-[#451A03]/60 text-[#FCD34D]">
          <AppIcon name="alertTriangle" :size="20" />
        </div>
        <p class="mt-3 text-[13px] font-semibold text-white">三维渲染上下文重置</p>
        <p class="mt-1 text-[11px] leading-relaxed text-[#94A3B8]">
          WebGL Context Lost（显卡驱动或资源回收）。构件几何已安全保留，可一键重建视口。
        </p>
        <button class="btn btn-primary btn-sm mt-4" @click="reinit">
          <AppIcon name="refresh" :size="13" />一键重新初始化 WebGL 视口
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.viewer-dark {
  background:
    radial-gradient(ellipse at 50% 36%, rgba(38, 64, 88, 0.72) 0%, rgba(15, 28, 43, 0.96) 52%, #070d14 100%),
    #0b1220;
}
</style>
