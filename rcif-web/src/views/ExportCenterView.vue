<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import ExportFlowModal from '@/components/export/ExportFlowModal.vue';
import { api } from '@/api/client';
import type { ComponentDetail, ComponentSummary, DownloadFormat } from '@/api/types';
import { useUiStore } from '@/stores/ui';
import {
  componentStatusMeta,
  formatDate,
  formatVolume,
  sourceModeMeta,
  triggerDownload,
} from '@/utils/format';

type FormatRow = DownloadFormat & { available: boolean; reason: string | null };

const ui = useUiStore();

const components = ref<ComponentSummary[]>([]);
const selectedFamily = ref('');
const selectedModel = ref('');
const detail = ref<ComponentDetail | null>(null);
const formats = ref<FormatRow[]>([]);
const search = ref('');
const categoryFilter = ref('all');
const statusFilter = ref<'all' | 'published' | 'draft' | 'reviewing' | 'archived'>('all');
const exportOpen = ref(false);
const exportFormat = ref('rcif');
const loading = ref(true);
const history = ref<{ id: string; familyId: string; name: string; action: string; format?: string; at: string; detail?: string }[]>([]);
const ifcBusy = ref(false);

const categories = computed(() =>
  [...new Set(components.value.map((component) => component.category))].sort((a, b) => a.localeCompare(b, 'zh-Hans-CN')),
);
const filteredComponents = computed(() => {
  const term = search.value.trim().toLowerCase();
  return components.value.filter((component) => {
    if (categoryFilter.value !== 'all' && component.category !== categoryFilter.value) return false;
    if (statusFilter.value !== 'all' && component.status !== statusFilter.value) return false;
    if (!term) return true;
    return (
      component.name.toLowerCase().includes(term) ||
      component.familyId.toLowerCase().includes(term) ||
      component.drawingCode.toLowerCase().includes(term) ||
      component.taxonomy.crbim.toLowerCase().includes(term)
    );
  });
});
const groupedComponents = computed(() => {
  const groups = new Map<string, ComponentSummary[]>();
  for (const component of filteredComponents.value) {
    const list = groups.get(component.category) ?? [];
    list.push(component);
    groups.set(component.category, list);
  }
  return [...groups.entries()].map(([category, items]) => ({ category, items }));
});
const exportHistory = computed(() =>
  history.value.filter((h) => ['download', 'export', 'handoff'].includes(h.action)).slice(0, 10),
);
const presetOptions = computed(() => detail.value?.parameters.presets ?? []);
const selectedPreset = computed(
  () => presetOptions.value.find((preset) => preset.modelNumber === selectedModel.value) ?? null,
);
const modelParamPreview = computed(() => {
  const preset = selectedPreset.value;
  if (!preset) return [];
  return (detail.value?.parameters.schema ?? [])
    .filter((field) => field.access !== 'out')
    .slice(0, 4)
    .map((field) => ({
      name: field.name,
      label: field.label,
      value: preset.params[field.name],
      unit: field.unit ?? '',
    }));
});
function isFormatAvailable(id: string) {
  return formats.value.find((format) => format.id === id)?.available ?? true;
}
function formatHint(id: string) {
  const format = formats.value.find((item) => item.id === id);
  return format ? `${format.title} · ${format.scenario} · 约 ${format.sizeMB.toFixed(1)} MB` : '';
}

function clearComponentFilters() {
  search.value = '';
  categoryFilter.value = 'all';
  statusFilter.value = 'all';
}

async function selectComponent(familyId: string) {  selectedFamily.value = familyId;
  try {
    const [component, formatResponse] = await Promise.all([
      api.component(familyId),
      api.formats(familyId),
    ]);
    detail.value = component;
    formats.value = formatResponse.formats;
    const defaultModel = String(component.parameters.schema[0]?.default ?? '');
    selectedModel.value =
      component.parameters.presets.find((preset) => preset.modelNumber === defaultModel)?.modelNumber ??
      component.parameters.presets[0]?.modelNumber ??
      '';
  } catch (error) {
    ui.notify({ type: 'error', title: '构件加载失败', message: String(error) });
  }
}

function openExport(format = 'rcif') {
  exportFormat.value = format;
  exportOpen.value = true;
}

async function exportIfc() {
  if (!detail.value) return;
  ifcBusy.value = true;
  try {
    const result = await api.exportIfc(detail.value.familyId, selectedModel.value || undefined);
    triggerDownload(result.downloadUrl, result.fileName);
    ui.notify({
      type: result.unmapped.length ? 'warning' : 'success',
      title: result.unmapped.length ? `IFC 导出完成 · ${result.unmapped.length} 项 unmapped` : 'IFC 4.3 交付物已生成',
      message: result.fileName,
    });
  } catch (error) {
    ui.notify({ type: 'error', title: 'IFC 导出失败', message: String(error) });
  } finally {
    ifcBusy.value = false;
  }
}

async function reExport(familyId: string) {
  await selectComponent(familyId);
  openExport('rcif');
}

onMounted(async () => {
  loading.value = true;
  try {
    const [searchResponse, historyResponse] = await Promise.all([
      api.search({ pageSize: 60 }),
      api.history(),
    ]);
    components.value = searchResponse.items;
    history.value = historyResponse.items;
    if (searchResponse.items[0]) await selectComponent(searchResponse.items[0].familyId);
  } catch (error) {
    ui.notify({ type: 'error', title: '导出中心初始化失败', message: String(error) });
  } finally {
    loading.value = false;
  }
});
</script>

<template>
  <div class="mx-auto max-w-[1400px] px-4 py-4 xl:px-6">
    <div class="flex flex-wrap items-end justify-between gap-3">
      <div>
        <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
          <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
          <AppIcon name="chevronRight" :size="10" />
          <span class="text-[#4B5563]">导出中心</span>
        </div>
        <h1 class="mt-1 text-[20px] font-semibold tracking-tight text-ink">导出中心</h1>
        <p class="mt-0.5 text-[12px] text-muted">
          选择构件与格式，完成权限校验后下载。
        </p>
      </div>
      <div class="flex items-center gap-1.5">
        <span class="flex items-center gap-1 rounded-full border border-line bg-surface px-2 py-1 text-[10px] text-muted">
          <AppIcon name="clock" :size="11" />凭据 15 分钟有效
        </span>
      </div>
    </div>

    <div class="mt-3 grid gap-3 lg:grid-cols-[300px_minmax(0,1fr)]">
        <!-- 构件选择 -->
        <aside class="panel flex max-h-[390px] flex-col overflow-hidden">
          <div class="border-b border-line p-2.5">
            <div class="mb-1.5 flex items-center justify-between">
              <p class="eyebrow">选择构件</p>
              <button
                v-if="search || categoryFilter !== 'all' || statusFilter !== 'all'"
                class="text-[9px] text-primary hover:text-primary-hover"
                @click="clearComponentFilters"
              >清空筛选</button>
            </div>
            <div class="flex items-center gap-1.5 rounded border border-line bg-[#FAFBFC] px-2 py-1">
              <AppIcon name="search" :size="12" class="text-[#9CA3AF]" />
              <input
                v-model="search"
                class="min-w-0 flex-1 border-0 bg-transparent text-[11px] focus:outline-none"
                placeholder="名称 / 图号 / familyId / CRBIM"
              />
            </div>
            <div class="mt-1.5 grid grid-cols-2 gap-1.5">
              <select v-model="categoryFilter" class="select !w-full !py-1 !text-[10.5px]" aria-label="按分类筛选">
                <option value="all">全部分类</option>
                <option v-for="category in categories" :key="category" :value="category">{{ category }}</option>
              </select>
              <select v-model="statusFilter" class="select !w-full !py-1 !text-[10.5px]" aria-label="按状态筛选">
                <option value="all">全部状态</option>
                <option value="published">已发布</option>
                <option value="reviewing">审核中</option>
                <option value="draft">草稿</option>
                <option value="archived">已归档</option>
              </select>
            </div>
            <p class="mt-1.5 text-[9px] text-muted">{{ filteredComponents.length }} 个构件 · 按分类分组</p>
          </div>
          <div class="min-h-0 flex-1 overflow-y-auto scroll-thin">
            <template v-for="group in groupedComponents" :key="group.category">
              <p class="sticky top-0 z-10 flex items-center justify-between border-b border-[#E8EDF2] bg-[#F7F9FB] px-2.5 py-1.5 text-[9.5px] font-semibold text-[#6B7280]">
                <span>{{ group.category }}</span>
                <span class="mono text-[8.5px] text-[#9CA3AF]">{{ group.items.length }}</span>
              </p>
              <button
                v-for="item in group.items"
                :key="item.familyId"
                class="flex w-full items-center gap-2 border-b border-[#F0F2F5] px-2.5 py-2 text-left transition-colors"
                :class="selectedFamily === item.familyId ? 'bg-[#EDF4F8]' : 'hover:bg-[#FAFBFC]'"
                @click="selectComponent(item.familyId)"
              >
                <span class="h-9 w-12 shrink-0 overflow-hidden rounded border border-[#EDF0F3] bg-[#F7F9FB]">
                  <img :src="api.previewUrl(item.familyId, 96, 72)" alt="" class="h-full w-full object-contain" loading="lazy" />
                </span>
                <span class="min-w-0 flex-1">
                  <span class="block truncate text-[11px] font-medium" :class="selectedFamily === item.familyId ? 'text-primary' : 'text-ink'">
                    {{ item.name }}
                  </span>
                  <span class="mono block truncate text-[8.5px] text-muted">{{ item.drawingCode }}</span>
                </span>
                <StatusBadge :tone="sourceModeMeta(item.sourceMode).tone" :label="sourceModeMeta(item.sourceMode).label" :dot="false" />
              </button>
            </template>
            <div v-if="!groupedComponents.length" class="px-3 py-8 text-center">
              <p class="text-[11px] text-muted">当前筛选无匹配构件</p>
              <button class="mt-2 text-[10px] text-primary" @click="clearComponentFilters">清空筛选</button>
            </div>
          </div>
        </aside>

        <!-- 导出详情 -->
        <section v-if="detail" class="panel overflow-hidden lg:min-h-[390px]">
          <div class="grid h-full gap-0 md:grid-cols-[minmax(0,1.05fr)_minmax(0,1fr)]">
            <div class="relative flex min-h-[260px] items-center justify-center border-b border-line bg-[#F5F8FA] md:min-h-[390px] md:border-b-0 md:border-r">
              <div class="grid-bg absolute inset-0 opacity-50" />
              <img :src="api.previewUrl(detail.familyId, 720, 480)" :alt="detail.name" class="relative max-h-[350px] w-full object-contain p-3" />
              <div class="absolute left-3 top-3 flex gap-1">
                <StatusBadge :tone="componentStatusMeta(detail.status).tone" :label="componentStatusMeta(detail.status).label" />
                <StatusBadge :tone="sourceModeMeta(detail.sourceMode).tone" :label="sourceModeMeta(detail.sourceMode).label" :dot="false" />
              </div>
            </div>

            <div class="flex flex-col p-4">
              <div>
                <h2 class="text-[16px] font-semibold text-ink">{{ detail.name }}</h2>
                <p class="mt-0.5 font-mono text-[10px] text-muted">
                  {{ detail.familyId }} · {{ detail.drawingCode }} · v{{ detail.revision }}
                </p>
                <div class="mt-2 flex flex-wrap gap-1.5">
                  <span class="tag">体积 {{ formatVolume(detail.geometry.volumeM3) }}</span>
                  <span class="tag">{{ detail.geometry.triangles.toLocaleString() }} 面片</span>
                  <span class="tag">{{ detail.geometry.exactBrep ? 'exactBrep' : 'mesh-only' }}</span>
                  <span class="tag mono">{{ detail.taxonomy.crbim }}</span>
                </div>
                <p class="mt-2 line-clamp-2 text-[11px] leading-relaxed text-[#4B5563]">{{ detail.description }}</p>

                <div v-if="presetOptions.length" class="mt-3 rounded border border-line bg-[#FAFBFC] px-2.5 py-2">
                  <div class="flex flex-wrap items-center gap-2">
                    <label class="text-[10px] font-medium text-[#374151]" for="export-model">导出型号</label>
                    <select id="export-model" v-model="selectedModel" class="select !w-auto min-w-[230px] !py-1 !text-[11px]">
                      <option v-for="preset in presetOptions" :key="preset.modelNumber" :value="preset.modelNumber">
                        {{ preset.modelNumber }} · {{ preset.label }}
                      </option>
                    </select>
                  </div>
                  <p v-if="selectedPreset" class="mt-1 text-[9px] text-muted">
                    {{ selectedPreset.label }} · {{ selectedPreset.isPrebaked ? '预烘焙' : '按需求解' }}
                  </p>
                  <dl v-if="modelParamPreview.length" class="mt-2 grid grid-cols-2 gap-x-3 gap-y-1">
                    <div v-for="field in modelParamPreview" :key="field.name" class="flex min-w-0 items-baseline justify-between gap-1">
                      <dt class="truncate text-[9px] text-[#9CA3AF]">{{ field.label }}</dt>
                      <dd class="mono shrink-0 text-[9.5px] text-[#374151]">
                        {{ field.value ?? '—' }}{{ field.unit }}
                      </dd>
                    </div>
                  </dl>
                </div>
              </div>

              <div class="mt-auto pt-3">
                <div class="flex flex-wrap gap-1.5">
                  <button class="btn btn-primary btn-sm" :title="formatHint('rcif')" @click="openExport('rcif')">
                    <AppIcon name="package" :size="13" />下载 .rcif
                  </button>
                  <button class="btn btn-outline btn-sm" :title="formatHint('ifc')" :disabled="ifcBusy || !detail.geometry.exactBrep" @click="exportIfc">
                    <AppIcon name="globe" :size="13" />IFC 4.3
                  </button>
                  <button class="btn btn-outline btn-sm" :title="formatHint('glb')" :disabled="!isFormatAvailable('glb')" @click="openExport('glb')">
                    <AppIcon name="cube" :size="13" />GLB
                  </button>
                  <button class="btn btn-outline btn-sm" :title="formatHint('step')" :disabled="!isFormatAvailable('step')" @click="openExport('step')">
                    <AppIcon name="layers" :size="13" />STEP
                  </button>
                  <button class="btn btn-outline btn-sm" :title="formatHint('cbf')" :disabled="!isFormatAvailable('cbf')" @click="openExport('cbf')">
                    <AppIcon name="box" :size="13" />CBF
                  </button>
                </div>
                <p class="mt-2 text-[9px] leading-relaxed text-muted">
                  文件按当前型号生成；IFC / STEP 使用精确几何。
                </p>
              </div>
            </div>
          </div>
        </section>
        <div v-else class="panel flex items-center justify-center p-10 text-[12px] text-muted">请选择左侧构件</div>
      </div>

      <!-- 最近导出 -->
      <div class="panel mt-3 overflow-hidden">
        <div class="flex items-center justify-between border-b border-line px-3 py-2">
          <p class="section-title">最近下载与导出</p>
          <RouterLink to="/history" class="text-[12px] text-primary hover:underline">查看完整历史 →</RouterLink>
        </div>
        <table class="table-dense">
          <thead>
            <tr><th>时间</th><th>构件</th><th>动作</th><th>格式</th><th>详情</th><th class="text-right">操作</th></tr>
          </thead>
          <tbody>
            <tr v-for="entry in exportHistory" :key="entry.id">
              <td class="font-mono text-[12px] text-muted">{{ formatDate(entry.at) }}</td>
              <td class="text-[12.5px] font-medium text-ink">{{ entry.name }}</td>
              <td><span class="chip">{{ entry.action }}</span></td>
              <td class="mono text-[11.5px] text-[#374151]">{{ entry.format ?? '—' }}</td>
              <td class="max-w-[420px] truncate text-[12px] text-muted" :title="entry.detail">{{ entry.detail }}</td>
              <td class="text-right">
                <button class="btn btn-outline btn-sm" :disabled="!entry.familyId" @click="reExport(entry.familyId)">再次导出</button>
              </td>
            </tr>
            <tr v-if="!exportHistory.length"><td colspan="6" class="py-8 text-center text-[12px] text-muted">暂无导出记录</td></tr>
          </tbody>
        </table>
      </div>

    <ExportFlowModal
      :open="exportOpen"
      :component="detail"
      :initial-format="exportFormat"
      :model-number="selectedModel"
      @close="exportOpen = false"
    />
  </div>
</template>
