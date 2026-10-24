<script setup lang="ts">
import { computed, nextTick, onMounted, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import CadViewport from '@/components/viewer/CadViewport.vue';
import ExportFlowModal from '@/components/export/ExportFlowModal.vue';
import { api } from '@/api/client';
import type { ComponentDetail, SemanticMapping, SemanticProfile } from '@/api/types';
import { useEvaluation } from '@/composables/useEvaluation';
import { decodeJhb, type DecodedJhb } from '@/jhb/decode';
import { useCompareStore } from '@/stores/compare';
import { useSessionStore } from '@/stores/session';
import { useUiStore } from '@/stores/ui';
import {
  componentStatusMeta,
  formatArea,
  formatDate,
  formatLength,
  formatMm,
  formatNumber,
  formatVolume,
  maskKey,
  sourceModeMeta,
  triggerDownload,
  visibilityMeta,
} from '@/utils/format';

const route = useRoute();
const router = useRouter();
const ui = useUiStore();
const isPartner = computed(() => session.user?.role === 'partner');
const compare = useCompareStore();
const session = useSessionStore();

const familyId = computed(() => String(route.params.familyId ?? ''));
const canSeeGovernance = computed(() => session.canReview || session.canManageFamilies);
const detail = ref<ComponentDetail | null>(null);
const loading = ref(true);
const loadError = ref('');
const favorite = ref(false);
const activeTab = ref<'tree' | 'semantics' | 'cache' | 'projection' | 'versions'>('tree');
const projectionView = ref<'sheet' | 'front' | 'top' | 'side' | 'iso'>('sheet');
const projectionVersion = Date.now();
const projectionOptions = [
  { id: 'sheet', label: '三视图' },
  { id: 'front', label: '正视' },
  { id: 'top', label: '俯视' },
  { id: 'side', label: '侧视' },
  { id: 'iso', label: '轴测' },
] as const;
function setProjectionView(id: (typeof projectionOptions)[number]['id']) {
  projectionView.value = id;
}
const projectionHidden = ref(true);
const projectionDims = ref(true);
const projectionSrc = computed(() =>
  detail.value ? api.projectionUrl(detail.value.familyId, projectionView.value, {
    hidden: projectionHidden.value,
    dimensions: projectionDims.value,
    cacheBust: projectionVersion,
  }) : '',
);
const projectionDownload = computed(() =>
  detail.value ? api.projectionUrl(detail.value.familyId, projectionView.value, {
    hidden: projectionHidden.value,
    dimensions: projectionDims.value,
    download: true,
    cacheBust: projectionVersion,
  }) : '',
);
const selectedShapeKey = ref<string | null>(null);
const expandedGates = ref<Set<string>>(new Set(['GATE-01']));
const semantics = ref<SemanticMapping[]>([]);
const profiles = ref<SemanticProfile[]>([]);
const selectedProfile = ref('');
const semanticsLoading = ref(false);
const exportOpen = ref(false);
const exportFormat = ref('rcif');
const selectedVersion = ref<string | null>(null);
const versionCompareActive = ref(false);
const versionCompareLoading = ref(false);
const versionCompareOld = ref<DecodedJhb | null>(null);
const versionCompareNew = ref<DecodedJhb | null>(null);
const selectedModel = ref('');

const evaluation = useEvaluation();
const { decoded, status, progress, error: evalError } = evaluation;

const statusMeta = computed(() => (detail.value ? componentStatusMeta(detail.value.status) : null));
const sourceMeta = computed(() => (detail.value ? sourceModeMeta(detail.value.sourceMode) : null));
const visibility = computed(() => (detail.value ? visibilityMeta(detail.value.visibility) : null));
const isParametric = computed(() => detail.value?.sourceMode === 'parametric');
const canOpenConfigurator = computed(
  () => isParametric.value || (detail.value?.parameters.presets.length ?? 0) > 1,
);
const presetOptions = computed(() => detail.value?.parameters.presets ?? []);
const selectedPreset = computed(() =>
  presetOptions.value.find((preset) => preset.modelNumber === selectedModel.value) ?? presetOptions.value[0],
);
const displayDims = computed(() => {
  const params = selectedPreset.value?.params ?? {};
  const fallback = detail.value?.dims;
  return {
    height: Number(params.HeightZ ?? params.PierHeight ?? fallback?.height ?? 0),
    length: Number(params.LengthX ?? params.TopLength ?? fallback?.length ?? 0),
    width: Number(params.WidthY ?? params.TopWidth ?? fallback?.width ?? 0),
  };
});
const displayVolume = computed(() => {
  const value = Number(selectedPreset.value?.params.NetVolume);
  return Number.isFinite(value) && value > 0 ? value : detail.value?.geometry.volumeM3 ?? 0;
});
const displayStrength = computed(
  () => String(selectedPreset.value?.params.StrengthGrade ?? detail.value?.parameters.schema.find((p) => p.name === 'StrengthGrade')?.default ?? '—'),
);

function selectModel() {
  if (!detail.value) return;
  if (isParametric.value) {
    evaluation.evaluate({
      familyId: detail.value.familyId,
      modelNumber: selectedModel.value,
      parameterOverrides: {},
    });
  } else {
    evaluation.loadGeometry(detail.value.familyId, selectedModel.value);
  }
}
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
const shapeLabels = computed<Record<string, string>>(() => {
  const map: Record<string, string> = {};
  for (const entry of shapeEntries.value) map[entry.key] = entry.label;
  return map;
});
const namedPartOptions = computed(() =>
  shapeEntries.value.map((entry) => ({ key: entry.key, label: entry.label })),
);
const selectedShape = computed(() => shapeEntries.value.find((s) => s.key === selectedShapeKey.value) ?? null);
const selectedMapping = computed(() => semantics.value.find((m) => m.shapeKey === selectedShapeKey.value) ?? null);
const shapeOutput = computed(() => {
  if (!selectedShapeKey.value || !decoded.value) return null;
  return decoded.value.header.outputs?.[selectedShapeKey.value] ?? null;
});
const unmapped = computed(() => semantics.value.filter((m) => m.status === 'unmapped'));
const tabs = computed(() => [
  { id: 'tree', label: '几何部位与装配树', icon: 'tree', count: shapeEntries.value.length },
  { id: 'semantics', label: '外部语义映射', icon: 'layers', count: semantics.value.length, warn: unmapped.value.length > 0 },
  ...(canSeeGovernance.value
    ? [{ id: 'cache', label: '确定性缓存与 QA', icon: 'database', count: 6 }]
    : []),
  { id: 'projection', label: '二维投影图', icon: 'grid', count: 4 },
  { id: 'versions', label: '版本记录与变更', icon: 'history', count: detail.value?.changelog.length ?? 0 },
]);
const defaultProfileCode = computed(() => {
  const profile = profiles.value.find((p) => p.isDefault) ?? profiles.value[0];
  return profile ? `${profile.profileId}@${profile.version}` : '';
});
const currentVersionEntry = computed(() => detail.value?.changelog[0] ?? null);
const comparedVersion = computed(
  () => detail.value?.changelog.find((c) => c.version === selectedVersion.value) ?? null,
);
const canCompareVersion = computed(
  () => Boolean(comparedVersion.value && currentVersionEntry.value && comparedVersion.value.version !== currentVersionEntry.value.version),
);
const viewportDecoded = computed(() =>
  versionCompareActive.value ? versionCompareNew.value ?? decoded.value : decoded.value,
);
const viewportCompareDecoded = computed(() => (versionCompareActive.value ? versionCompareOld.value : null));
const cacheKey = computed(() => decoded.value?.header.cache_key ?? detail.value?.geometry.defaultCacheKey ?? '');
const activeDecoded = computed(() => decoded.value);

function shapeStat(key: string, kind: 'volume' | 'area' | 'length') {
  const output = decoded.value?.header.outputs?.[key];
  if (!output) return null;
  const value = output[kind];
  if (typeof value !== 'number') return null;
  if (kind === 'volume') return formatVolume(value / 1e9);
  if (kind === 'area') return formatArea(value / 1e6);
  return formatLength(value);
}

function selectShape(key: string) {
  selectedShapeKey.value = key;
  if (!isParametric.value) return;
}

function openExport(format = 'rcif') {
  exportFormat.value = format;
  exportOpen.value = true;
}

async function loadSemantics() {
  if (!detail.value) return;
  semanticsLoading.value = true;
  try {
    const response = await api.semantics(detail.value.familyId, selectedProfile.value || undefined);
    semantics.value = response.mappings;
  } catch (error) {
    ui.notify({ type: 'error', title: '语义映射加载失败', message: String(error) });
  } finally {
    semanticsLoading.value = false;
  }
}

function toggleGate(code: string) {
  const next = new Set(expandedGates.value);
  if (next.has(code)) next.delete(code);
  else next.add(code);
  expandedGates.value = next;
}

function downloadQa() {
  if (!detail.value) return;
  const payload = {
    format: 'cbf-qa-report',
    version: '1.0',
    familyId: detail.value.familyId,
    revision: detail.value.revision,
    cacheKey: cacheKey.value,
    generatedAt: new Date().toISOString(),
    ...detail.value.qaReport,
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  triggerDownload(url, `${detail.value.familyId}_${detail.value.revision}_QA.json`);
  setTimeout(() => URL.revokeObjectURL(url), 2000);
  ui.notify({ type: 'success', title: '质检报告已下载', message: 'JSON 报告包含 6 道门禁明细、散度体积与脚本摘要。' });
}

function setTab(id: string) {
  activeTab.value = id as typeof activeTab.value;
}

function toggleFavorite() {
  if (!detail.value) return;
  const next = !favorite.value;
  api
    .toggleFavorite(detail.value.familyId, !next)
    .then((result) => {
      favorite.value = result.favorited;
      ui.notify({ type: 'success', title: result.favorited ? '已收藏' : '已取消收藏', message: detail.value?.name });
    })
    .catch((error) => ui.notify({ type: 'error', title: '收藏操作失败', message: String(error) }));
}

function addCompare() {
  if (!detail.value) return;
  const added = compare.toggle(detail.value);
  if (!added && !compare.has(detail.value.familyId)) {
    ui.notify({ type: 'warning', title: '对比栏已满', message: '最多同时对比 4 个构件' });
  }
}

async function startGeometryDiff() {
  if (!detail.value || !comparedVersion.value || !currentVersionEntry.value || !canCompareVersion.value) return;
  versionCompareLoading.value = true;
  try {
    const [oldBuffer, newBuffer] = await Promise.all([
      api.versionGeometry(detail.value.familyId, comparedVersion.value.version),
      api.versionGeometry(detail.value.familyId, currentVersionEntry.value.version),
    ]);
    versionCompareOld.value = decodeJhb(oldBuffer);
    versionCompareNew.value = decodeJhb(newBuffer);
    versionCompareActive.value = true;
    await nextTick();
    document.getElementById('detail-viewport')?.scrollIntoView({ behavior: 'smooth', block: 'center' });
  } catch (error) {
    ui.notify({ type: 'error', title: '版本几何加载失败', message: String(error) });
  } finally {
    versionCompareLoading.value = false;
  }
}

function exitGeometryDiff() {
  versionCompareActive.value = false;
  versionCompareOld.value = null;
  versionCompareNew.value = null;
}

watch(selectedProfile, () => void loadSemantics());
watch(selectedShapeKey, (key) => {
  if (key) {
    ui.rememberSearch(key);
  }
});

onMounted(async () => {
  loading.value = true;
  try {
    const [component, profileResponse, favoriteResponse] = await Promise.all([
      api.component(familyId.value),
      api.profiles().catch(() => null),
      api.favorites().catch(() => ({ items: [], total: 0 })),
    ]);
    detail.value = component;
    profiles.value = profileResponse?.items ?? [];
    selectedProfile.value = defaultProfileCode.value;
    const requestedModel = String(route.query.model ?? '');
    selectedModel.value =
      component.parameters.presets.find((preset) => preset.modelNumber === requestedModel)?.modelNumber ??
      String(component.parameters.schema[0].default);
    favorite.value = favoriteResponse.items.some((item: any) => item.familyId === component.familyId);
    await loadSemantics();
    if (component.sourceMode === 'parametric') {
      evaluation.evaluate({
        familyId: component.familyId,
        modelNumber: selectedModel.value,
        parameterOverrides: {},
      });
    } else {
      evaluation.loadGeometry(component.familyId, selectedModel.value);
    }
  } catch (error) {
    loadError.value = error instanceof Error ? error.message : '构件加载失败';
  } finally {
    loading.value = false;
  }
});
</script>

<template>
  <div class="mx-auto max-w-[1500px] px-4 py-4 xl:px-6">
    <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
      <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
      <AppIcon name="chevronRight" :size="10" />
      <RouterLink to="/library" class="hover:text-primary">构件资产大厅</RouterLink>
      <AppIcon name="chevronRight" :size="10" />
      <span class="text-[#4B5563]">{{ detail?.name ?? familyId }}</span>
    </div>

    <div v-if="loadError" class="panel mt-4 p-8 text-center">
      <p class="text-[13px] font-medium text-danger">{{ loadError }}</p>
      <button class="btn btn-outline btn-sm mt-2" @click="router.push('/library')">返回构件大厅</button>
    </div>

    <div v-else-if="!detail" class="mt-3 space-y-3">
      <div class="skeleton h-8 w-72" />
      <div class="grid gap-4 lg:grid-cols-[minmax(0,1fr)_360px]">
        <div class="skeleton h-[520px] w-full" />
        <div class="space-y-3">
          <div class="skeleton h-40 w-full" />
          <div class="skeleton h-64 w-full" />
        </div>
      </div>
    </div>

    <template v-else>
      <div class="mt-2 flex flex-wrap items-start justify-between gap-3">
        <div class="min-w-0">
          <div class="flex flex-wrap items-center gap-2">
            <h1 class="text-[21px] font-semibold tracking-tight text-ink">{{ detail.name }}</h1>
            <span class="rounded border border-line bg-[#FAFBFC] px-1.5 py-[2px] font-mono text-[10px] text-[#4B5563]">v{{ detail.revision }}</span>
            <StatusBadge v-if="statusMeta" :tone="statusMeta.tone" :label="statusMeta.label" size="md" />
            <StatusBadge v-if="visibility" :tone="visibility.tone" :label="visibility.label" :dot="false" size="md" />
          </div>
          <p class="mt-1 flex flex-wrap items-center gap-x-3 gap-y-1 text-[11.5px] text-muted">
            <span class="mono text-[#374151]">{{ detail.drawingCode }}</span>
            <span class="text-[#D1D5DB]">/</span>
            <span class="mono">FamilyId: {{ detail.familyId }}</span>
            <span class="text-[#D1D5DB]">/</span>
            <span>更新于 {{ formatDate(detail.updatedAt) }}</span>
            <span class="rounded bg-[#F1F4F7] px-1.5 py-[1px] font-mono text-[9.5px]">{{ detail.taxonomy.crbim }} · {{ detail.category }}</span>
          </p>
        </div>
        <div class="flex items-center gap-1.5">
          <button
            class="btn btn-outline btn-sm"
            :class="favorite ? '!border-warning !text-warning' : ''"
            @click="toggleFavorite"
          >
            <AppIcon name="star" :size="13" :class="favorite ? 'fill-warning' : ''" />{{ favorite ? '已收藏' : '收藏' }}
          </button>
          <button
            class="btn btn-outline btn-sm"
            :class="compare.has(detail.familyId) ? '!border-primary !text-primary' : ''"
            @click="addCompare"
          >
            <AppIcon name="gitCompare" :size="13" />{{ compare.has(detail.familyId) ? '已加入对比' : '加入对比' }}
          </button>
          <RouterLink to="/exports" class="btn btn-ghost btn-sm"><AppIcon name="download" :size="13" />导出中心</RouterLink>
        </div>
      </div>

      <!-- 首屏：视口 + Sticky 动作栏 -->
      <div class="mt-3 grid gap-3 lg:grid-cols-[minmax(0,1fr)_360px]">
        <div class="min-w-0">
          <div id="detail-viewport" class="panel relative h-[420px] overflow-hidden lg:h-[540px]">
            <CadViewport
              :decoded="viewportDecoded"
              :compare-decoded="viewportCompareDecoded"
              :status="versionCompareActive ? 'succeeded' : status"
              :progress="versionCompareActive ? 100 : progress"
              :error="evalError"
              :highlight-shape="selectedShape?.index ?? null"
              :shape-labels="shapeLabels"
              :named-parts="versionCompareActive ? [] : namedPartOptions"
              :selected-part="selectedShapeKey"
              :hud="canSeeGovernance && !versionCompareActive"
              @pick="(info) => (selectedShapeKey = info?.shapeKey ?? null)"
              @select-part="selectedShapeKey = $event"
            />
            <div
              v-if="versionCompareActive"
              class="pointer-events-auto absolute left-3 top-14 z-20 max-w-[300px] rounded-md border border-hl/30 bg-[#0F1A2B]/95 px-3 py-2 shadow-lift"
            >
              <p class="flex items-center gap-1.5 text-[9px] font-semibold uppercase tracking-wider text-hl">
                <AppIcon name="gitCompare" :size="11" />版本几何对比中
              </p>
              <p class="mt-1 text-[11px] text-white">
                v{{ comparedVersion?.version }} <span class="text-[#64748B]">→</span> v{{ currentVersionEntry?.version }}
              </p>
              <p class="mt-0.5 text-[9px] leading-relaxed text-[#94A3B8]">旧版半透明 · 新版实体叠加</p>
              <button class="mt-2 rounded border border-white/15 px-2 py-1 text-[10px] text-[#CBD5E1] hover:border-white/30 hover:text-white" @click="exitGeometryDiff">
                退出对比
              </button>
            </div>
          </div>

          <div v-if="selectedShape" class="mt-2 flex flex-wrap items-center gap-2 rounded-md border border-[#C9DCE6] bg-[#F3F8FB] px-3 py-2">
            <AppIcon name="crosshair" :size="13" class="text-primary" />
            <span class="text-[11px] font-semibold text-ink">{{ selectedShape.key }}</span>
            <span class="text-[10.5px] text-muted">{{ selectedShape.label }}</span>
            <span class="mono rounded bg-white px-1.5 py-[1px] text-[9.5px] text-[#4B5563]">{{ selectedShape.code }}</span>
            <span
              v-if="selectedMapping"
              class="rounded px-1.5 py-[1px] text-[9.5px]"
              :class="selectedMapping.status === 'unmapped' ? 'bg-[#FFFBEB] text-warning' : 'bg-[#EFF6FF] text-[#1D4ED8]'"
            >
              {{ selectedMapping.status === 'unmapped' ? 'unmapped' : `${selectedMapping.ifc?.entity} · ${selectedMapping.ifc?.shapeAspect}` }}
            </span>
            <span v-if="selectedMapping?.crbim" class="text-[10px] text-[#4B5563]">CRBIM {{ selectedMapping.crbim }}</span>
            <span v-if="shapeOutput?.area" class="mono ml-auto text-[10px] text-[#374151]">面积 {{ formatArea(Number(shapeOutput.area) / 1e6) }}</span>
            <span v-else-if="shapeOutput?.volume" class="mono ml-auto text-[10px] text-[#374151]">体积 {{ formatVolume(Number(shapeOutput.volume) / 1e9) }}</span>
          </div>
        </div>

        <!-- 右侧 Sticky -->
        <aside class="min-w-0 lg:sticky lg:top-4 lg:self-start">
          <div class="panel p-3.5">
            <div class="flex items-start justify-between gap-2">
              <div>
                <p class="eyebrow">核心操作</p>
                <p class="mt-0.5 text-[12px] font-semibold text-ink">构件获取与格式导出</p>
              </div>
              <span class="rounded-full border border-[#BBF7D0] bg-[#ECFDF3] px-2 py-[2px] text-[9.5px] font-medium text-success">
                6/6 门禁通过
              </span>
            </div>

            <label class="mt-3 block">
              <span class="mb-1 block text-[9.5px] font-medium text-muted">标准型号</span>
              <select v-model="selectedModel" class="select h-8 text-[11px]" @change="selectModel">
                <option v-for="preset in presetOptions" :key="preset.modelNumber" :value="preset.modelNumber">
                  {{ preset.modelNumber }} · {{ preset.label }}
                </option>
              </select>
              <span v-if="selectedPreset" class="mt-1 block text-[9px] text-[#9CA3AF]">
                {{ selectedPreset.label }} · {{ selectedPreset.isPrebaked ? '预烘焙' : '在线求解' }}
              </span>
            </label>

            <div class="mt-3 grid gap-1.5">
              <button class="btn btn-primary btn-lg" @click="openExport('rcif')">
                <AppIcon name="package" :size="15" />{{ isPartner ? '申请下载构件包 (.rcif)' : '下载构件包 (.rcif)' }}
              </button>
              <button
                class="btn btn-outline"
                :disabled="!canOpenConfigurator"
                :title="isParametric ? '进入在线参数配置器' : canOpenConfigurator ? '查看并切换预定义规格' : '该构件没有可选规格'"
                @click="canOpenConfigurator && router.push(`/components/${detail.familyId}/configurator`)"
              >
                <AppIcon name="sliders" :size="14" />{{ isParametric ? '配置参数' : '规格配置' }}
              </button>
              <button class="btn btn-outline" :disabled="!detail.geometry.exactBrep" @click="openExport('ifc')">
                <AppIcon name="globe" :size="14" />
                {{ isPartner ? '申请导出 IFC 4.3' : '导出 IFC 4.3' }}
                <span v-if="!detail.geometry.exactBrep" class="text-[9.5px] text-warning">（mesh-only）</span>
              </button>
              <div class="grid grid-cols-3 gap-1.5">
                <button class="btn btn-outline !px-2 text-[11px]" title="导出 GLB 可视化网格" @click="openExport('glb')">
                  <AppIcon name="cube" :size="13" />GLB
                </button>
                <button
                  class="btn btn-outline !px-2 text-[11px]"
                  :disabled="!detail.geometry.exactBrep"
                  title="导出 STEP AP242 精确几何"
                  @click="openExport('step')"
                >
                  <AppIcon name="layers" :size="13" />STEP
                </button>
                <button
                  class="btn btn-outline !px-2 text-[11px]"
                  :disabled="!detail.geometry.exactBrep"
                  title="导出 CBF 精确几何容器"
                  @click="openExport('cbf')"
                >
                  <AppIcon name="box" :size="13" />CBF
                </button>
              </div>
            </div>

            <p
              v-if="!isParametric"
              class="mt-2 rounded border border-[#FDE68A] bg-[#FFFBEB] px-2 py-1.5 text-[9.5px] leading-relaxed text-[#92400E]"
            >
              <template v-if="canOpenConfigurator">
                该构件为导入几何，可切换 {{ detail.parameters.presets.length }} 个预定义规格，不支持自定义参数生成。
              </template>
              <template v-else>
                该构件能力为 <span class="font-semibold">{{ sourceMeta?.label }}</span>，仅提供只读预览与参数查看。
              </template>
            </p>

            <div v-if="canSeeGovernance" class="mt-3 flex items-center justify-between rounded border border-line bg-[#FAFBFC] px-2 py-1.5">
              <span class="text-[9.5px] text-muted">权威缓存键</span>
              <span class="mono truncate text-[9px] text-[#374151]" :title="cacheKey">{{ maskKey(cacheKey, 14, 6) }}</span>
            </div>
            <p class="mt-1.5 text-center text-[9px] text-[#9CA3AF]">
              下载动作将写入全链路审计日志 · 凭据 15 分钟有效
            </p>
          </div>

          <div class="panel mt-3 p-3.5">
            <div class="flex items-center justify-between">
              <p class="eyebrow">关键技术规格</p>
              <button v-if="canSeeGovernance" class="text-[10px] text-primary hover:text-primary-hover" @click="activeTab = 'cache'">缓存详情 →</button>
            </div>
            <table class="mt-2 w-full text-[12.5px]">
              <tbody>
                <tr class="border-b border-[#F0F2F5]">
                  <td class="py-1.5 text-muted">外包尺寸 H×L×W</td>
                  <td class="py-1.5 text-right font-mono text-[12.5px] text-ink">
                    {{ formatNumber(displayDims.height / 1000, 2) }} × {{ formatNumber(displayDims.length / 1000, 2) }} ×
                    {{ formatNumber(displayDims.width / 1000, 2) }} m
                  </td>
                </tr>
                <tr class="border-b border-[#F0F2F5]">
                  <td class="py-1.5 text-muted">实体净体积</td>
                  <td class="py-1.5 text-right font-mono text-[12.5px] font-semibold text-ink">{{ formatVolume(displayVolume) }}</td>
                </tr>
                <tr class="border-b border-[#F0F2F5]">
                  <td class="py-1.5 text-muted">表面积</td>
                  <td class="py-1.5 text-right font-mono text-[12.5px] text-ink">{{ formatArea(detail.geometry.surfaceM2) }}</td>
                </tr>
                <tr class="border-b border-[#F0F2F5]">
                  <td class="py-1.5 text-muted">混凝土强度等级</td>
                  <td class="py-1.5 text-right font-mono text-[12.5px] text-ink">
                    {{ displayStrength }}
                  </td>
                </tr>
                <tr class="border-b border-[#F0F2F5]">
                  <td class="py-1.5 text-muted">建模引擎</td>
                  <td class="py-1.5 text-right font-mono text-[12.5px] text-ink">{{ detail.engine }}</td>
                </tr>
                <tr class="border-b border-[#F0F2F5]">
                  <td class="py-1.5 text-muted">精确实体</td>
                  <td class="py-1.5 text-right">
                    <span class="font-mono text-[12.5px]" :class="detail.geometry.exactBrep ? 'text-success' : 'text-warning'">
                      {{ detail.geometry.exactBrep ? '是' : '否' }}
                    </span>
                  </td>
                </tr>
                <tr>
                  <td class="py-1.5 text-muted">拓扑规模</td>
                  <td class="py-1.5 text-right font-mono text-[11.5px] text-ink">
                    {{ formatNumber(detail.geometry.vertices) }} 顶点 / {{ formatNumber(detail.geometry.triangles) }} 面片
                  </td>
                </tr>
              </tbody>
            </table>
            <p class="mt-2 text-[10.5px] leading-relaxed text-[#4B5563]">{{ detail.description }}</p>
            <div class="mt-2 flex flex-wrap gap-1">
              <span v-for="tag in detail.tags" :key="tag" class="tag">{{ tag }}</span>
            </div>
          </div>

          <div v-if="canSeeGovernance" class="panel mt-3 p-3.5">
            <p class="eyebrow">治理快照</p>
            <div class="mt-2 grid grid-cols-3 gap-1.5">
              <div class="panel-inset px-2 py-1.5">
                <p class="text-[9px] text-muted">QA 评分</p>
                <p class="mono mt-0.5 text-[15px] font-semibold text-success">{{ detail.qa.score }}</p>
              </div>
              <div class="panel-inset px-2 py-1.5">
                <p class="text-[9px] text-muted">语义未映射</p>
                <p class="mono mt-0.5 text-[15px] font-semibold" :class="unmapped.length ? 'text-warning' : 'text-success'">
                  {{ unmapped.length }}
                </p>
              </div>
              <div class="panel-inset px-2 py-1.5">
                <p class="text-[9px] text-muted">跨项目复用</p>
                <p class="mono mt-0.5 text-[15px] font-semibold text-ink">{{ detail.stats.references }}</p>
              </div>
            </div>
            <div class="mt-2 flex items-center gap-1.5 text-[10px] text-muted">
              <AppIcon name="shield" :size="12" class="text-success" />
              脚本摘要 <span class="mono text-[9px] text-[#4B5563]">{{ maskKey(detail.recipe?.digest ?? detail.qaReport.scriptDigest, 10, 4) }}</span>
            </div>
          </div>
        </aside>
      </div>

      <!-- 多维信息 Tabs -->
      <div class="panel mt-3 overflow-hidden">
        <div class="flex items-center gap-0 overflow-x-auto border-b border-line bg-[#FAFBFC] px-2 no-scrollbar">
          <button
            v-for="tab in tabs"
            :key="tab.id"
            class="relative flex shrink-0 items-center gap-1.5 px-3 py-2.5 text-[12px] transition-colors"
            :class="activeTab === tab.id ? 'font-semibold text-primary' : 'text-[#6B7280] hover:text-ink'"
            @click="setTab(tab.id)"
          >
            <AppIcon :name="tab.icon" :size="13" />{{ tab.label }}
            <span
              class="mono rounded px-1 text-[9px]"
              :class="tab.warn ? 'bg-[#FFFBEB] text-warning' : 'bg-[#EEF1F4] text-[#6B7280]'"
            >{{ tab.count }}</span>
            <span v-if="activeTab === tab.id" class="absolute inset-x-2 -bottom-px h-[2px] rounded-full bg-primary" />
          </button>
        </div>

        <!-- Tab 1 -->
        <div v-if="activeTab === 'tree'" class="p-3">
          <div class="mb-2 flex flex-wrap items-center justify-between gap-2">
            <p class="text-[11px] text-muted">
              <span class="font-medium text-ink">{{ shapeEntries.length }}</span> 个几何槽位 ·
              <span class="font-medium text-ink">{{ detail.geometry.facesCount }}</span> 个拓扑面 ·
              <span class="font-medium text-ink">{{ detail.geometry.edgesCount }}</span> 条特征边
            </p>
            <p class="font-mono text-[11px] text-[#9CA3AF]">点击部位在视口中高亮对应几何面与特征线</p>
          </div>
          <div class="overflow-x-auto">
            <table class="table-dense">
              <thead>
                <tr>
                  <th>部位 (shapeKey)</th>
                  <th>语义码 (code)</th>
                  <th>几何类型</th>
                  <th>父级 (parent)</th>
                  <th class="text-right">算量</th>
                  <th>拓扑映射</th>
                  <th class="w-[80px]"></th>
                </tr>
              </thead>
              <tbody>
                <tr
                  v-for="entry in shapeEntries"
                  :key="entry.key"
                  class="cursor-pointer"
                  :class="selectedShapeKey === entry.key ? 'bg-[#F3F8FB]' : ''"
                  @click="selectShape(entry.key)"
                >
                  <td>
                    <div class="flex items-center gap-1.5" :style="{ paddingLeft: `${entry.depth * 16}px` }">
                      <span
                        class="flex h-4 w-4 items-center justify-center rounded-sm"
                        :class="entry.geomType === 'solid' ? 'bg-[#E8EFF4] text-primary' : 'bg-[#F1F4F7] text-[#6B7280]'"
                      >
                        <AppIcon :name="entry.geomType === 'solid' ? 'cube' : entry.geomType === 'face' ? 'box' : 'route'" :size="9" />
                      </span>
                      <span class="mono text-[12px] font-medium text-ink">{{ entry.key }}</span>
                    </div>
                    <p class="mt-0.5 text-[11px] text-muted" :style="{ paddingLeft: `${entry.depth * 16 + 22}px` }">{{ entry.label }}</p>
                  </td>
                  <td class="mono text-[11.5px] text-[#374151]">{{ entry.code }}</td>
                  <td><span class="chip mono">{{ entry.geomType }}</span></td>
                  <td class="mono text-[11px] text-muted">{{ entry.parent ?? '—' }}</td>
                  <td class="text-right font-mono text-[11.5px] text-[#374151]">
                    {{ shapeStat(entry.key, 'volume') ?? shapeStat(entry.key, 'area') ?? '—' }}
                  </td>
                  <td>
                    <span class="mono text-[11px] text-muted">
                      {{ entry.geomType === 'wire' ? '边映射' : '面映射' }}
                    </span>
                  </td>
                  <td class="text-right">
                    <button class="btn btn-ghost btn-sm" @click.stop="selectShape(entry.key)">高亮</button>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </div>

        <!-- Tab 2 -->
        <div v-else-if="activeTab === 'semantics'" class="p-3">
          <div class="mb-2 flex flex-wrap items-center justify-between gap-2">
            <div class="flex items-center gap-2">
              <span class="text-[11px] text-muted">激活映射模板</span>
              <select v-model="selectedProfile" class="select !w-auto !py-1 !text-[11px]">
                <option v-for="profile in profiles" :key="`${profile.profileId}@${profile.version}`" :value="`${profile.profileId}@${profile.version}`">
                  {{ profile.name }} @{{ profile.version }}{{ profile.isDefault ? ' · 默认' : '' }}
                </option>
              </select>
              <span v-if="semanticsLoading" class="h-3 w-3 animate-spin rounded-full border-2 border-[#CBD5E1] border-t-primary" />
            </div>
            <RouterLink v-if="session.canManageSemantics" to="/semantics" class="btn btn-outline btn-sm">
              <AppIcon name="layers" :size="12" />在语义中枢中维护
            </RouterLink>
          </div>

          <div v-if="unmapped.length" class="mb-2 flex items-start gap-2 rounded border border-[#FDE68A] bg-[#FFFBEB] px-3 py-2">
            <AppIcon name="alertTriangle" :size="14" class="mt-0.5 text-warning" />
            <p class="text-[10.5px] leading-relaxed text-[#92400E]">
              当前模板下存在 <span class="font-semibold">{{ unmapped.length }}</span> 个未映射部位：基础几何将保留，导出报告标记为 <span class="mono">unmapped</span> 并告警。
              未映射项：<span class="mono">{{ unmapped.map((m) => m.shapeKey).join(', ') }}</span>
            </p>
          </div>

          <div class="overflow-x-auto">
            <table class="table-dense">
              <thead>
                <tr>
                  <th>shapeKey</th>
                  <th>语义码</th>
                  <th>CRBIM 53 表</th>
                  <th>GB/T 51269</th>
                  <th>IFC 4.3 实体</th>
                  <th>IfcShapeAspect</th>
                  <th>算量集</th>
                  <th>状态</th>
                </tr>
              </thead>
              <tbody>
                <tr v-for="mapping in semantics" :key="mapping.shapeKey" :class="mapping.status === 'unmapped' ? 'bg-[#FFFBEB]' : ''">
                  <td class="mono text-[12px] font-medium text-ink">{{ mapping.shapeKey }}</td>
                  <td class="mono text-[11.5px] text-[#374151]">{{ mapping.code }}</td>
                  <td class="text-[11.5px] text-[#374151]">{{ mapping.crbim ?? '—' }}</td>
                  <td class="text-[11.5px] text-[#374151]">{{ mapping.gb51269 ?? '—' }}</td>
                  <td class="mono text-[11.5px] font-medium text-[#1D4ED8]">{{ mapping.ifc?.entity ?? '—' }}</td>
                  <td class="mono text-[11px] text-[#374151]">{{ mapping.ifc?.shapeAspect ?? '—' }}</td>
                  <td class="mono text-[11px] text-muted">{{ mapping.qto ?? mapping.ifc?.qto ?? '—' }}</td>
                  <td>
                    <StatusBadge
                      :tone="mapping.status === 'unmapped' ? 'warning' : 'success'"
                      :label="mapping.status === 'unmapped' ? 'unmapped' : mapping.layer === 'local' ? '构件内覆盖' : '已映射'"
                    />
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
          <p class="mt-2 rounded bg-[#FAFBFC] px-2 py-1.5 text-[11px] leading-relaxed text-muted">
            映射优先级：构件专项覆盖 → 项目/租户激活规则 → 标准语义提示 → 未映射兜底；最终结果由工程标准服务统一计算。
          </p>
        </div>

        <!-- Tab 3 -->
        <div v-else-if="activeTab === 'cache'" class="grid gap-3 p-3 lg:grid-cols-2">
          <div>
            <div class="rounded-md border border-line bg-[#FAFBFC] p-3">
              <div class="flex items-center justify-between">
                <p class="text-[12px] font-semibold text-ink">权威确定性缓存键</p>
                <span class="rounded bg-[#ECFDF3] px-1.5 py-[1px] text-[9px] text-success">DeterministicJSON</span>
              </div>
              <p class="mono mt-1.5 break-all text-[10px] leading-relaxed text-[#374151]">{{ cacheKey }}</p>
              <div class="mt-2 grid grid-cols-2 gap-1.5">
                <div class="rounded bg-white px-2 py-1.5">
                  <p class="text-[9px] text-muted">JHB 协议版本</p>
                  <p class="mono mt-0.5 text-[11px] font-semibold text-ink">{{ activeDecoded?.header.protocolVersion ?? '—' }}</p>
                </div>
                <div class="rounded bg-white px-2 py-1.5">
                  <p class="text-[9px] text-muted">4 字节对齐</p>
                  <p class="mono mt-0.5 text-[11px] font-semibold" :class="activeDecoded?.aligned ? 'text-success' : 'text-warning'">
                    {{ activeDecoded ? (activeDecoded.aligned ? 'PASSED' : 'LEGACY COPY') : '—' }}
                  </p>
                </div>
                <div class="rounded bg-white px-2 py-1.5">
                  <p class="text-[9px] text-muted">payloadStart</p>
                  <p class="mono mt-0.5 text-[11px] font-semibold text-ink">{{ activeDecoded?.payloadStart ?? '—' }}</p>
                </div>
                <div class="rounded bg-white px-2 py-1.5">
                  <p class="text-[9px] text-muted">解码通道</p>
                  <p class="mono mt-0.5 text-[11px] font-semibold text-ink">{{ activeDecoded ? (activeDecoded.zeroCopy ? '通道一 · 零拷贝' : '通道二 · 对齐拷贝') : '—' }}</p>
                </div>
              </div>
            </div>

            <div class="mt-3 rounded-md border border-line p-3">
              <p class="text-[12px] font-semibold text-ink">脚本真实性与几何自检</p>
              <dl class="mt-2 grid grid-cols-[96px_1fr] gap-y-1.5 text-[10.5px]">
                <dt class="text-muted">脚本 SHA-256</dt>
                <dd class="mono break-all text-[9px] text-[#374151]">{{ detail.recipe?.digest ?? detail.qaReport.scriptDigest }}</dd>
                <dt class="text-muted">源脚本</dt>
                <dd class="mono break-all text-[9px] text-[#374151]">{{ detail.recipeSource ?? '—' }}</dd>
                <dt class="text-muted">BRepCheck</dt>
                <dd class="text-[#374151]">{{ detail.qaReport.bRepCheck }}</dd>
                <dt class="text-muted">散度体积偏差</dt>
                <dd class="mono text-success">{{ detail.qaReport.volumeDivergence.toFixed(2) }}%（&lt; 1.0%）</dd>
                <dt class="text-muted">自交检测</dt>
                <dd class="text-[#374151]">{{ detail.qaReport.mesh.selfIntersections }} 处</dd>
                <dt class="text-muted">极值压测</dt>
                <dd class="space-y-0.5">
                  <p v-for="test in detail.qaReport.extremeTests" :key="test.label" class="flex items-center gap-1">
                    <AppIcon :name="test.ok ? 'checkCircle' : 'xCircle'" :size="11" :class="test.ok ? 'text-success' : 'text-danger'" />
                    {{ test.label }}
                  </p>
                </dd>
              </dl>
            </div>

            <div class="mt-3 flex items-center gap-2">
              <button class="btn btn-outline btn-sm" @click="downloadQa">
                <AppIcon name="file" :size="12" />下载完整质检报告 (JSON)
              </button>
              <RouterLink v-if="session.canIngest" to="/ingestion" class="btn btn-ghost btn-sm">前往入库质检工作台</RouterLink>
            </div>
          </div>

          <div>
            <p class="mb-2 text-[12px] font-semibold text-ink">6 道入库门禁质检流水线</p>
            <div class="space-y-1.5">
              <div
                v-for="gate in detail.qaReport.gates"
                :key="gate.code"
                class="overflow-hidden rounded-md border"
                :class="gate.status === 'passed' ? 'border-[#BBF7D0]' : gate.status === 'failed' ? 'border-[#FECACA]' : 'border-line'"
              >
                <button
                  class="flex w-full items-center gap-2 px-2.5 py-2 text-left"
                  :class="gate.status === 'failed' ? 'bg-[#FEF2F2]' : 'bg-[#F9FDFA]'"
                  @click="toggleGate(gate.code)"
                >
                  <AppIcon
                    :name="gate.status === 'passed' ? 'checkCircle' : gate.status === 'failed' ? 'xCircle' : 'clock'"
                    :size="14"
                    :class="gate.status === 'passed' ? 'text-success' : gate.status === 'failed' ? 'text-danger' : 'text-muted'"
                  />
                  <span class="mono shrink-0 text-[9px] font-semibold text-[#6B7280]">{{ gate.code }}</span>
                  <span class="min-w-0 flex-1 truncate text-[11px] font-medium text-ink">{{ gate.name }}</span>
                  <span class="mono text-[9px] text-[#9CA3AF]">{{ gate.durationMs ?? 0 }}ms</span>
                  <AppIcon :name="expandedGates.has(gate.code) ? 'chevronDown' : 'chevronRight'" :size="11" class="text-[#9CA3AF]" />
                </button>
                <div v-if="expandedGates.has(gate.code)" class="border-t border-[#EDF0F3] bg-white px-2.5 py-2">
                  <p class="text-[10px] leading-relaxed text-[#4B5563]">{{ gate.message }}</p>
                  <div v-if="gate.details" class="mt-1.5 space-y-1">
                    <div v-for="item in gate.details" :key="item.label" class="flex items-start justify-between gap-2 text-[9.5px]">
                      <span class="shrink-0 text-[#9CA3AF]">{{ item.label }}</span>
                      <span class="mono text-right text-[#374151]">{{ item.value }}</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>

        <!-- Tab 4：二维投影图 -->
        <div v-else-if="activeTab === 'projection'" class="p-3">
          <div class="flex flex-wrap items-center justify-between gap-2">
            <p class="text-[11px] text-muted">
              第一角法 · 主/俯/左视图与轴测图
            </p>
            <div class="flex flex-wrap items-center gap-1.5">
              <button
                v-for="option in projectionOptions"
                :key="option.id"
                :data-projection-view="option.id"
                class="rounded border px-2 py-1 text-[10px] transition-colors"
                :class="projectionView === option.id ? 'border-primary bg-primary/5 font-semibold text-primary' : 'border-line text-[#6B7280] hover:border-primary/40 hover:text-primary'"
                @click="setProjectionView(option.id)"
              >
                {{ option.label }}
              </button>
              <span class="mx-1 h-4 w-px bg-line" />
              <button
                class="rounded border px-2 py-1 text-[10px] transition-colors"
                :class="projectionHidden ? 'border-primary bg-primary/5 text-primary' : 'border-line text-[#6B7280]'"
                @click="projectionHidden = !projectionHidden"
              >
                隐藏线
              </button>
              <button
                class="rounded border px-2 py-1 text-[10px] transition-colors"
                :class="projectionDims ? 'border-primary bg-primary/5 text-primary' : 'border-line text-[#6B7280]'"
                @click="projectionDims = !projectionDims"
              >
                尺寸
              </button>
              <button class="btn btn-primary btn-sm" @click="triggerDownload(projectionDownload)">
                <AppIcon name="download" :size="12" />下载 SVG
              </button>
            </div>
          </div>
          <div class="mt-2 overflow-auto rounded-md border border-line bg-white">
            <img
              v-if="detail"
              :src="projectionSrc"
              :alt="`${detail.name} 二维投影图`"
              class="block w-full"
              loading="lazy"
            />
          </div>
        </div>

        <!-- Tab 5 -->
        <div v-else class="p-3">
          <p class="text-[11px] text-muted">SemVer 版本演进与权威缓存键变更审计</p>

          <div class="mt-3 grid gap-3 lg:grid-cols-[1.3fr_1fr]">
            <div class="relative space-y-0">
              <div class="absolute bottom-2 left-[74px] top-2 w-px bg-line" />
              <button
                v-for="(entry, index) in detail.changelog"
                :key="entry.version"
                class="relative flex w-full gap-3 rounded-md px-1 py-2 text-left transition-colors hover:bg-[#FAFBFC]"
                :class="selectedVersion === entry.version ? 'bg-[#F3F8FB]' : ''"
                @click="selectedVersion = entry.version"
              >
                <div class="w-[62px] shrink-0 text-right">
                  <p class="mono text-[11px] font-semibold text-ink">v{{ entry.version }}</p>
                  <p class="mt-0.5 text-[9px] text-[#9CA3AF]">{{ formatDate(entry.date) }}</p>
                </div>
                <span
                  class="relative z-10 mt-1 flex h-3 w-3 shrink-0 items-center justify-center rounded-full border-2 bg-white"
                  :class="index === 0 ? 'border-primary' : 'border-[#CBD5E1]'"
                >
                  <span v-if="index === 0" class="h-1 w-1 rounded-full bg-primary" />
                </span>
                <div class="min-w-0 flex-1 rounded-md border border-line bg-white px-2.5 py-2">
                  <div class="flex items-center gap-1.5">
                    <span
                      class="rounded px-1.5 py-[1px] text-[9px] font-medium"
                      :class="
                        entry.type === 'major'
                          ? 'bg-[#EFF6FF] text-[#1D4ED8]'
                          : entry.type === 'minor'
                            ? 'bg-[#F0FDF4] text-success'
                            : 'bg-[#F3F4F6] text-[#4B5563]'
                      "
                    >
                      {{ entry.type }}
                    </span>
                    <span class="text-[10px] text-muted">{{ entry.author }}</span>
                    <span v-if="index === 0" class="rounded bg-[#E8EFF4] px-1 py-[1px] text-[9px] text-primary">当前版本</span>
                  </div>
                  <ul class="mt-1 space-y-0.5">
                    <li v-for="change in entry.changes" :key="change" class="flex items-start gap-1 text-[10px] leading-relaxed text-[#4B5563]">
                      <span class="mt-[5px] h-1 w-1 shrink-0 rounded-full bg-[#CBD5E1]" />{{ change }}
                    </li>
                  </ul>
                  <p class="mono mt-1 truncate text-[8.5px] text-[#9CA3AF]">cacheKey {{ maskKey(entry.cacheKey, 16, 6) }}</p>
                </div>
              </button>
            </div>

            <div class="panel-inset h-fit p-3">
              <p class="text-[12px] font-semibold text-ink">版本差异对比</p>
              <div v-if="comparedVersion" class="mt-2 space-y-2">
                <div class="flex items-center justify-between rounded bg-white px-2 py-1.5 text-[10.5px]">
                  <span class="mono text-muted">v{{ comparedVersion.version }}</span>
                  <AppIcon name="arrowRight" :size="12" class="text-[#9CA3AF]" />
                  <span class="mono font-semibold text-primary">v{{ currentVersionEntry?.version }}</span>
                </div>
                <dl class="space-y-1.5 text-[10.5px]">
                  <div class="flex justify-between"><dt class="text-muted">发布日期</dt><dd class="mono">{{ formatDate(comparedVersion.date) }} → {{ formatDate(currentVersionEntry?.date ?? '') }}</dd></div>
                  <div class="flex justify-between"><dt class="text-muted">变更类型</dt><dd class="mono">{{ comparedVersion.type }} → {{ currentVersionEntry?.type }}</dd></div>
                  <div class="flex justify-between"><dt class="text-muted">变更条目</dt><dd class="mono">{{ comparedVersion.changes.length }} → {{ currentVersionEntry?.changes.length }}</dd></div>
                  <div><dt class="text-muted">旧缓存键</dt><dd class="mono mt-0.5 break-all text-[8.5px]">{{ comparedVersion.cacheKey }}</dd></div>
                  <div><dt class="text-muted">新缓存键</dt><dd class="mono mt-0.5 break-all text-[8.5px]">{{ currentVersionEntry?.cacheKey }}</dd></div>
                </dl>
                <button
                  class="btn btn-primary btn-sm w-full"
                  :disabled="!canCompareVersion || versionCompareLoading"
                  @click="startGeometryDiff"
                >
                  <span v-if="versionCompareLoading" class="h-3 w-3 animate-spin rounded-full border-2 border-white/40 border-t-white" />
                  <AppIcon v-else name="gitCompare" :size="12" />
                  {{ versionCompareLoading ? '正在加载版本几何…' : '几何对比' }}
                </button>
              </div>
              <p v-else class="mt-2 text-[10.5px] leading-relaxed text-muted">
                选择左侧任意历史版本，查看缓存键与变更摘要；点击“几何对比”进入 3D 视图比较该版本与当前版本。
              </p>
            </div>
          </div>
        </div>
      </div>
    </template>

    <ExportFlowModal
      :open="exportOpen"
      :component="detail"
      :initial-format="exportFormat"
      :model-number="selectedModel"
      @close="exportOpen = false"
    />
  </div>
</template>
