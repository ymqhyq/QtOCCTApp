<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import EmptyState from '@/components/ui/EmptyState.vue';
import ComponentCard from '@/components/library/ComponentCard.vue';
import { api } from '@/api/client';
import type {
  CatalogFamily,
  ComponentDetail,
  ComponentStatus,
  ComponentSummary,
  Preset,
  SearchResponse,
  SourceMode,
  TaxonomyScheme,
} from '@/api/types';
import { useCompareStore } from '@/stores/compare';
import { useUiStore } from '@/stores/ui';
import { useSessionStore } from '@/stores/session';
import {
  cleanDisplayName,
  componentStatusMeta,
  formatDate,
  formatNumber,
  formatVolume,
  maskKey,
  sourceModeMeta,
} from '@/utils/format';

const route = useRoute();
const router = useRouter();
const compare = useCompareStore();
const ui = useUiStore();
const session = useSessionStore();
const defaultStatus = computed<ComponentStatus | 'all'>(() =>
  session.canManageFamilies ? 'all' : 'published',
);

type SearchMode = 'text' | 'shape';
type SchemeKey = 'crbim' | 'gb51269' | 'jtg';
const DISCIPLINES = ['桥梁工程', '隧道工程', '路基与防护', '涵洞与通道', '通用'];

const loading = ref(true);
const error = ref('');
const data = ref<SearchResponse | null>(null);
const taxonomies = ref<TaxonomyScheme[]>([]);
const catalog = ref<CatalogFamily[]>([]);
const favorites = ref<Set<string>>(new Set());
const viewMode = ref<'grid' | 'table'>('grid');
const showTreeMobile = ref(false);
const dragOver = ref(false);

const searchInput = ref('');
const q = ref('');
const searchMode = ref<SearchMode>('text');
const similarTo = ref('');
const scheme = ref<SchemeKey>('crbim');
const discipline = ref('all');
const familyFilter = ref('');
const selectedModel = ref('');
const sourceMode = ref<SourceMode | 'all'>('all');
const status = ref<ComponentStatus | 'all'>(defaultStatus.value);
const sort = ref<'latest' | 'name' | 'volume' | 'popular'>('latest');
const page = ref(1);
const pageSize = ref(12);
const expandedTree = ref<Set<string>>(new Set());
const expandedFamilies = ref<Set<string>>(new Set());
const familyDetail = ref<ComponentDetail | null>(null);
const familyLoading = ref(false);
const familyCache = new Map<string, ComponentDetail>();
const focusTaxonomy = ref('');
const taxonomyFilter = ref('');
const taxonomyFilterLabel = ref('');

const items = computed(() => data.value?.items ?? []);
const total = computed(() => data.value?.total ?? 0);
const totalPages = computed(() => Math.max(1, Math.ceil(total.value / pageSize.value)));
const scopedTotal = computed(() => {
  const facets = data.value?.facets;
  if (!facets) return total.value;
  return facets.sourceModes.reduce((sum, item) => sum + item.count, 0);
});
const sourceCounts = computed<Record<string, number>>(() => {
  const map: Record<string, number> = { all: scopedTotal.value };
  for (const item of data.value?.facets.sourceModes ?? []) map[item.value] = item.count;
  return map;
});
const disciplineCounts = computed<Record<string, number>>(() => {
  const map: Record<string, number> = { all: scopedTotal.value };
  for (const item of data.value?.facets.disciplines ?? []) map[item.value] = item.count;
  return map;
});
const sourceTabs: { value: SourceMode | 'all'; label: string }[] = [
  { value: 'all', label: '全部能力' },
  { value: 'parametric', label: '参数化构件' },
  { value: 'native-brep', label: '原生 BRep' },
  { value: 'imported-ifc', label: 'IFC 导入' },
  { value: 'mesh-only', label: '网格体' },
];

const currentScheme = computed(() => taxonomies.value.find((t) => t.id === scheme.value));
const schemeKey = computed<SchemeKey>(() =>
  scheme.value === 'gb51269' || scheme.value === 'jtg' ? scheme.value : 'crbim',
);
const selectedFamily = computed(() => catalog.value.find((item) => item.familyId === familyFilter.value) ?? null);
const similarReference = computed(() => catalog.value.find((item) => item.familyId === similarTo.value) ?? null);
const catalogPresetCount = computed(() => catalog.value.reduce((sum, item) => sum + item.presets.length, 0));
const catalogParametricCount = computed(() => catalog.value.filter((item) => item.sourceMode === 'parametric').length);
const resultHeading = computed(() => {
  if (familyFilter.value && selectedFamily.value) return selectedFamily.value.name;
  if (taxonomyFilter.value) return taxonomyFilterLabel.value || '分类节点构件';
  if (similarTo.value) return `与「${similarReference.value?.name ?? similarTo.value}」几何相似的构件`;
  return '全部构件资产';
});
const presetList = computed<Preset[]>(() => {
  const presets = familyDetail.value?.parameters.presets ?? [];
  return selectedModel.value ? presets.filter((preset) => preset.modelNumber === selectedModel.value) : presets;
});
const familyInputs = computed(() =>
  (familyDetail.value?.parameters.schema ?? []).filter(
    (field) => field.access !== 'out' && field.name !== 'ModelNumber' && field.name !== 'NetVolume',
  ),
);

interface AssetTreeNode {
  code: string;
  label: string;
  families: CatalogFamily[];
  children: AssetTreeNode[];
  familyCount: number;
  presetCount: number;
}

interface TreeRow {
  type: 'category' | 'family' | 'preset';
  depth: number;
  code: string;
  label: string;
  family?: CatalogFamily;
  preset?: { modelNumber: string; label: string; volumeM3: number; isPrebaked: boolean };
  familyCount?: number;
  presetCount?: number;
}

function buildAssetTree(nodes: any[], families: CatalogFamily[], key: SchemeKey): AssetTreeNode[] {
  const walk = (node: any): AssetTreeNode => {
    const children: AssetTreeNode[] = (node.children ?? []).map(walk);
    const direct = families.filter((family) => family.taxonomy[key] === node.code);
    const familyCount =
      direct.length + children.reduce((sum: number, child: AssetTreeNode) => sum + child.familyCount, 0);
    const presetCount =
      direct.reduce((sum, family) => sum + family.presets.length, 0) +
      children.reduce((sum: number, child: AssetTreeNode) => sum + child.presetCount, 0);
    return { code: node.code, label: node.label, families: direct, children, familyCount, presetCount };
  };
  return nodes.map(walk);
}

const assetTree = computed(() => buildAssetTree(currentScheme.value?.nodes ?? [], catalog.value, schemeKey.value));

function treeLabel(value: string): string {
  return value.replace(/\s*[（(]构件库2[）)]\s*/g, '').trim();
}

function flattenTree(nodes: AssetTreeNode[], depth: number, out: TreeRow[]) {
  for (const node of nodes) {
    if (!node.familyCount) continue;
    out.push({
      type: 'category',
      depth,
      code: node.code,
      label: node.label,
      familyCount: node.familyCount,
      presetCount: node.presetCount,
    });
    if (!expandedTree.value.has(node.code)) continue;
    flattenTree(node.children, depth + 1, out);
    for (const family of node.families) {
      if (family.presets.length === 1) {
        const preset = family.presets[0];
        out.push({
          type: 'preset',
          depth: depth + 1,
          code: preset.modelNumber,
          label: treeLabel(family.name),
          family,
          preset,
        });
        continue;
      }
      out.push({
        type: 'family',
        depth: depth + 1,
        code: family.familyId,
        label: treeLabel(family.name),
        family,
      });
      if (!expandedFamilies.value.has(family.familyId)) continue;
      for (const preset of family.presets) {
        out.push({
          type: 'preset',
          depth: depth + 2,
          code: preset.modelNumber,
          label: treeLabel(preset.label),
          family,
          preset,
        });
      }
    }
  }
}

const treeRows = computed<TreeRow[]>(() => {
  const rows: TreeRow[] = [];
  flattenTree(assetTree.value, 0, rows);
  return rows;
});

const activeChips = computed(() => {
  const chips: { key: string; label: string; clear: () => void }[] = [];
  if (q.value) chips.push({ key: 'q', label: `名称/编码 “${q.value}”`, clear: () => { searchInput.value = ''; q.value = ''; page.value = 1; reload(); } });
  if (taxonomyFilter.value) {
    chips.push({ key: 'taxonomy', label: `分类：${taxonomyFilterLabel.value || taxonomyFilter.value}`, clear: () => {
      taxonomyFilter.value = '';
      taxonomyFilterLabel.value = '';
      status.value = defaultStatus.value;
      page.value = 1;
      reload();
    } });
  }
  if (similarTo.value) chips.push({ key: 'similar', label: `几何相似：${similarReference.value?.name ?? similarTo.value}`, clear: () => { similarTo.value = ''; page.value = 1; reload(); } });
  if (discipline.value !== 'all') chips.push({ key: 'discipline', label: `专业：${discipline.value}`, clear: () => { discipline.value = 'all'; page.value = 1; reload(); } });
  if (sourceMode.value !== 'all') chips.push({ key: 'source', label: sourceModeMeta(sourceMode.value).label, clear: () => { sourceMode.value = 'all'; page.value = 1; reload(); } });
  if (status.value !== 'all' && (session.canManageFamilies || status.value !== defaultStatus.value)) {
    chips.push({ key: 'status', label: componentStatusMeta(status.value).label, clear: () => { status.value = defaultStatus.value; page.value = 1; reload(); } });
  }
  return chips;
});

let searchTimer: ReturnType<typeof setTimeout> | null = null;
let fetchSeq = 0;

function queryFromState() {
  const query: Record<string, string> = {};
  if (searchMode.value === 'text' && searchInput.value.trim()) query.q = searchInput.value.trim();
  if (taxonomyFilter.value) query.taxonomy = taxonomyFilter.value;
  if (similarTo.value) query.similarTo = similarTo.value;
  if (discipline.value !== 'all') query.discipline = discipline.value;
  if (sourceMode.value !== 'all') query.sourceMode = sourceMode.value;
  if (status.value !== 'all') query.status = status.value;
  if (sort.value !== 'latest') query.sort = sort.value;
  if (page.value > 1) query.page = String(page.value);
  if (pageSize.value !== 12) query.pageSize = String(pageSize.value);
  return query;
}

function parseRoute() {
  const query = route.query;
  searchInput.value = String(query.q ?? '');
  q.value = searchInput.value;
  similarTo.value = String(query.similarTo ?? '');
  searchMode.value = similarTo.value ? 'shape' : 'text';
  const schemeQuery = String(query.scheme ?? 'crbim');
  if (schemeQuery === 'gb51269' || schemeQuery === 'jtg' || schemeQuery === 'crbim') {
    scheme.value = schemeQuery;
  }
  focusTaxonomy.value = String(query.taxonomy ?? '');
  taxonomyFilter.value = String(query.taxonomy ?? '');
  taxonomyFilterLabel.value = '';
  discipline.value = String(query.discipline ?? 'all');
  sourceMode.value = (String(query.sourceMode ?? 'all') as SourceMode | 'all') ?? 'all';
  status.value = query.status
    ? (String(query.status) as ComponentStatus | 'all')
    : query.taxonomy
      ? 'all'
      : defaultStatus.value;
  sort.value = (['latest', 'name', 'volume', 'popular'].includes(String(query.sort))
    ? String(query.sort)
    : 'latest') as typeof sort.value;
  page.value = Math.max(1, Number(query.page ?? 1));
  pageSize.value = [12, 24, 36].includes(Number(query.pageSize ?? 12)) ? Number(query.pageSize ?? 12) : 12;
}

async function fetchData() {
  const seq = ++fetchSeq;
  loading.value = true;
  error.value = '';
  try {
    const response = await api.search({
      q: searchMode.value === 'text' ? q.value || undefined : undefined,
      scheme: scheme.value,
      taxonomy: taxonomyFilter.value || undefined,
      similarTo: similarTo.value || undefined,
      discipline: discipline.value !== 'all' ? discipline.value : undefined,
      sourceMode: sourceMode.value,
      status: status.value,
      sort: sort.value,
      page: page.value,
      pageSize: pageSize.value,
    });
    if (seq !== fetchSeq) return;
    data.value = response;
  } catch (fetchError) {
    if (seq !== fetchSeq) return;
    error.value = fetchError instanceof Error ? fetchError.message : '构件检索失败';
  } finally {
    if (seq === fetchSeq) loading.value = false;
  }
}

function reload() {
  const target = queryFromState();
  if (JSON.stringify(route.query) !== JSON.stringify(target)) {
    void router.replace({ query: target });
  }
  void fetchData();
}

function setSearchMode(mode: SearchMode) {
  searchMode.value = mode;
  if (mode === 'text') {
    similarTo.value = '';
    page.value = 1;
    reload();
  }
}

function onSearchInput() {
  searchMode.value = 'text';
  similarTo.value = '';
  if (searchTimer) clearTimeout(searchTimer);
  searchTimer = setTimeout(() => {
    q.value = searchInput.value.trim();
    page.value = 1;
    reload();
  }, 320);
}

function onSearchEnter() {
  searchMode.value = 'text';
  similarTo.value = '';
  if (searchTimer) clearTimeout(searchTimer);
  q.value = searchInput.value.trim();
  page.value = 1;
  reload();
}

function applySimilar() {
  if (!similarTo.value) {
    ui.notify({ type: 'warning', title: '请选择参考构件', message: '几何相似检索需要先选择一个参考族。' });
    return;
  }
  searchMode.value = 'shape';
  q.value = '';
  searchInput.value = '';
  page.value = 1;
  reload();
}

function clearSimilar() {
  similarTo.value = '';
  page.value = 1;
  reload();
}

function selectSource(value: SourceMode | 'all') {
  sourceMode.value = value;
  page.value = 1;
  reload();
}

function expandTreeAll() {
  const codes = new Set<string>();
  const walk = (nodes: AssetTreeNode[]) => {
    for (const node of nodes) {
      if (!node.familyCount) continue;
      codes.add(node.code);
      walk(node.children);
    }
  };
  walk(assetTree.value);
  expandedTree.value = codes;
}

function collapseTree() {
  expandedTree.value = new Set();
}

function selectTaxonomy(code: string, label: string) {
  taxonomyFilter.value = code;
  taxonomyFilterLabel.value = label;
  familyFilter.value = '';
  selectedModel.value = '';
  status.value = 'all';
  const next = new Set(expandedTree.value);
  next.add(code);
  expandedTree.value = next;
  page.value = 1;
  reload();
}

function toggleFamilyExpand(familyId: string) {
  const next = new Set(expandedFamilies.value);
  if (next.has(familyId)) next.delete(familyId);
  else next.add(familyId);
  expandedFamilies.value = next;
}

function onSchemeChange() {
  taxonomyFilter.value = '';
  taxonomyFilterLabel.value = '';
  familyFilter.value = '';
  selectedModel.value = '';
  expandedTree.value = new Set();
  page.value = 1;
  void nextTick().then(expandTreeAll);
  reload();
}

function findTreePath(nodes: AssetTreeNode[], code: string, path: string[] = []): string[] | null {
  for (const node of nodes) {
    const next = [...path, node.code];
    if (node.code === code) return next;
    const found = findTreePath(node.children, code, next);
    if (found) return found;
  }
  return null;
}

function findTreeLabel(nodes: AssetTreeNode[], code: string): string | null {
  for (const node of nodes) {
    if (node.code === code) return node.label;
    const found = findTreeLabel(node.children, code);
    if (found) return found;
  }
  return null;
}

function focusTreeCode(code: string) {
  const path = findTreePath(assetTree.value, code);
  if (!path) return;
  const next = new Set(expandedTree.value);
  for (const item of path) next.add(item);
  expandedTree.value = next;
  void nextTick().then(() => {
    document
      .querySelector(`[data-taxonomy-code="${CSS.escape(code)}"]`)
      ?.scrollIntoView({ block: 'center', behavior: 'smooth' });
  });
}

async function loadFamilyDetail(familyId: string) {
  if (familyCache.has(familyId)) {
    familyDetail.value = familyCache.get(familyId)!;
    return;
  }
  familyLoading.value = true;
  familyDetail.value = null;
  try {
    const detail = await api.component(familyId);
    familyCache.set(familyId, detail);
    if (familyFilter.value === familyId) familyDetail.value = detail;
  } catch (loadError) {
    ui.notify({ type: 'error', title: '族型号加载失败', message: String(loadError) });
  } finally {
    familyLoading.value = false;
  }
}

function syncRouteQuery() {
  const target = queryFromState();
  if (JSON.stringify(route.query) !== JSON.stringify(target)) {
    void router.replace({ query: target });
  }
}

async function selectFamily(family: CatalogFamily) {
  familyFilter.value = family.familyId;
  taxonomyFilter.value = '';
  taxonomyFilterLabel.value = '';
  selectedModel.value = '';
  syncRouteQuery();
  expandTreeAll();
  await loadFamilyDetail(family.familyId);
}

async function selectPreset(family: CatalogFamily, preset: { modelNumber: string }) {
  familyFilter.value = family.familyId;
  taxonomyFilter.value = '';
  taxonomyFilterLabel.value = '';
  selectedModel.value = preset.modelNumber;
  syncRouteQuery();
  expandTreeAll();
  await loadFamilyDetail(family.familyId);
  await nextTick();
  document
    .querySelector(`[data-preset-model="${CSS.escape(preset.modelNumber)}"]`)
    ?.scrollIntoView({ block: 'center', behavior: 'smooth' });
}

function clearFamilyFilter() {
  familyFilter.value = '';
  selectedModel.value = '';
  familyDetail.value = null;
  taxonomyFilter.value = '';
  taxonomyFilterLabel.value = '';
  status.value = defaultStatus.value;
  page.value = 1;
  reload();
}

function openPresetPage(family: CatalogFamily, preset: Preset) {
  if (family.sourceMode === 'parametric') {
    void router.push(`/components/${family.familyId}/configurator?model=${encodeURIComponent(preset.modelNumber)}`);
  } else {
    openPresetDetail(family);
  }
}

function openPresetDetail(family: CatalogFamily) {
  void router.push(`/components/${family.familyId}`);
}

function presetSummary(preset: Preset) {
  return familyInputs.value
    .slice(0, 4)
    .map((field) => {
      const value = preset.params[field.name];
      return typeof value === 'number'
        ? `${field.label} ${formatNumber(value)}${field.unit ?? ''}`
        : `${field.label} ${String(value ?? '—')}`;
    })
    .join(' · ');
}

function selectScheme(value: string) {
  scheme.value = value as SchemeKey;
  onSchemeChange();
}

function goPage(next: number) {
  page.value = Math.min(totalPages.value, Math.max(1, next));
  reload();
}

function changeSort(value: string) {
  sort.value = value as typeof sort.value;
  page.value = 1;
  reload();
}

function openPreview(component: ComponentSummary) {
  router.push({ path: `/components/${encodeURIComponent(component.familyId)}` });
}

async function toggleFavorite(component: ComponentSummary) {
  const isFav = favorites.value.has(component.familyId);
  try {
    const result = await api.toggleFavorite(component.familyId, isFav);
    const next = new Set(favorites.value);
    if (result.favorited) next.add(component.familyId);
    else next.delete(component.familyId);
    favorites.value = next;
    ui.notify({ type: 'success', title: result.favorited ? '已收藏构件' : '已取消收藏', message: component.name });
  } catch (favoriteError) {
    ui.notify({ type: 'error', title: '收藏失败', message: String(favoriteError) });
  }
}

function onDrop(event: DragEvent) {
  dragOver.value = false;
  const file = event.dataTransfer?.files?.[0];
  if (!file) return;
  ui.notify({
    type: 'info',
    title: '已接收几何文件',
    message: `${file.name}（${(file.size / 1024 / 1024).toFixed(2)} MB）\n外部文件特征提取需 OCCT Worker；当前请选择参考构件执行几何相似检索。`,
  });
}

function submitCustomRequest() {
  ui.notify({
    type: 'info',
    title: '已生成定制需求草稿',
    message: '需求将提交至标准化委员会，含当前筛选条件与空结果诊断信息。',
  });
}

function clearAll() {
  searchInput.value = '';
  q.value = '';
  similarTo.value = '';
  searchMode.value = 'text';
  discipline.value = 'all';
  sourceMode.value = 'all';
  status.value = defaultStatus.value;
  taxonomyFilter.value = '';
  taxonomyFilterLabel.value = '';
  familyFilter.value = '';
  selectedModel.value = '';
  familyDetail.value = null;
  page.value = 1;
  reload();
}

watch(
  () => route.query,
  () => {
    if (JSON.stringify(route.query) === JSON.stringify(queryFromState())) return;
    parseRoute();
    void fetchData();
    if (focusTaxonomy.value) {
      void nextTick().then(() => focusTreeCode(focusTaxonomy.value));
    }
  },
);

onMounted(async () => {
  parseRoute();
  await fetchData();
  try {
    const [tax, fav, cat] = await Promise.all([api.taxonomies(), api.favorites(), api.catalog()]);
    taxonomies.value = tax.items;
    favorites.value = new Set(fav.items.map((item: any) => item.familyId));
    catalog.value = cat.items;
    expandedFamilies.value = new Set(cat.items.map((family) => family.familyId));
    expandTreeAll();
    if (taxonomyFilter.value && !taxonomyFilterLabel.value) {
      taxonomyFilterLabel.value = findTreeLabel(assetTree.value, taxonomyFilter.value) ?? '';
    }
    if (focusTaxonomy.value) {
      await nextTick();
      focusTreeCode(focusTaxonomy.value);
    }
  } catch {
    /* 辅助数据失败不阻断主检索 */
  }
});

onBeforeUnmount(() => {
  if (searchTimer) clearTimeout(searchTimer);
});
</script>

<template>
  <div class="mx-auto flex max-w-[1640px] gap-3 pb-0 pl-0 pr-4 xl:pr-5">
    <!-- 资产主目录：分类标准 → 分类层级 → 族 → 预定义型号 -->
    <aside
      class="w-[292px] shrink-0"
      :class="showTreeMobile ? 'fixed inset-x-0 bottom-0 top-14 z-40 overflow-y-auto bg-app p-3 lg:static lg:z-auto lg:bg-transparent lg:p-0' : 'hidden lg:block'"
    >
      <div class="panel flex max-h-[calc(100vh-3.5rem)] flex-col overflow-hidden lg:sticky lg:top-0">
        <div class="border-b border-line bg-[#FAFBFC] px-3 py-2.5">
          <div class="flex items-center justify-between gap-2">
            <div>
              <p class="eyebrow flex items-center gap-1"><AppIcon name="tree" :size="11" />资产主目录</p>
              <p class="mt-0.5 text-[10.5px] text-muted">分类标准 → 分类 → 族 → 型号</p>
            </div>
            <button class="text-[#9CA3AF] lg:hidden" aria-label="关闭目录" @click="showTreeMobile = false">
              <AppIcon name="close" :size="15" />
            </button>
          </div>
          <label class="mt-2 flex items-center gap-1.5 text-[10px] text-[#6B7280]">
            <span class="shrink-0 whitespace-nowrap">分类标准</span>
            <select class="select h-7 min-w-0 flex-1 !py-0.5 text-[11px]" :value="scheme" @change="selectScheme(($event.target as HTMLSelectElement).value)">
              <option v-for="item in taxonomies" :key="item.id" :value="item.id">{{ item.name }}</option>
              <option v-if="!taxonomies.length" value="crbim">CRBIM 53 表</option>
            </select>
          </label>
          <div class="mt-2 flex items-center gap-1.5">
            <button class="btn btn-outline btn-sm flex-1" @click="expandTreeAll">展开全部</button>
            <button class="btn btn-ghost btn-sm flex-1" @click="collapseTree">收起</button>
          </div>
        </div>

        <div class="min-h-0 flex-1 overflow-y-auto p-1.5 scroll-thin">
          <button
            class="flex w-full items-center gap-2 rounded-[4px] px-2 py-2 text-left transition-colors"
            :class="!familyFilter && !taxonomyFilter ? 'bg-[#E8EFF4] text-primary' : 'text-[#374151] hover:bg-[#F3F6F8]'"
            @click="clearFamilyFilter"
          >
            <AppIcon name="layers" :size="13" />
            <span class="min-w-0 flex-1 truncate text-[11.5px] font-semibold">全部构件资产</span>
            <span class="mono text-[9.5px] text-[#9CA3AF]">{{ catalog.length }}</span>
          </button>

          <template v-for="row in treeRows" :key="`${row.type}-${row.code}-${row.depth}`">
            <button
              v-if="row.type === 'category'"
              :data-taxonomy-code="row.code"
              class="flex w-full items-center gap-1 rounded-[4px] px-1 py-1.5 text-left transition-colors hover:bg-[#F3F6F8]"
              :class="taxonomyFilter === row.code ? 'bg-[#EDF4F8] font-semibold text-primary' : 'text-[#4B5563]'"
              :style="{ paddingLeft: `${6 + row.depth * 12}px` }"
              @click="selectTaxonomy(row.code, row.label)"
            >
              <span class="flex h-4 w-4 shrink-0 items-center justify-center text-[#9CA3AF]">
                <AppIcon :name="expandedTree.has(row.code) ? 'chevronDown' : 'chevronRight'" :size="10" />
              </span>
              <span class="min-w-0 flex-1 truncate text-[10.5px] font-medium text-[#4B5563]">{{ row.label }}</span>
              <span class="mono shrink-0 text-[8.5px] text-[#B0BAC3]">{{ row.familyCount }}族</span>
            </button>

            <div
              v-else-if="row.type === 'family'"
              :data-family-id="row.code"
              class="flex w-full items-center gap-1.5 rounded-[4px] py-1.5 pr-1 text-left transition-colors"
              :class="familyFilter === row.code ? 'bg-[#EDF4F8]' : 'hover:bg-[#F6F8FA]'"
              :style="{ paddingLeft: `${10 + row.depth * 12}px` }"
              @click="row.family && selectFamily(row.family)"
            >
              <button
                class="flex h-4 w-4 shrink-0 items-center justify-center text-[#9CA3AF] hover:text-primary"
                :aria-label="expandedFamilies.has(row.code) ? '收起型号' : '展开型号'"
                @click.stop="toggleFamilyExpand(row.code)"
              >
                <AppIcon :name="expandedFamilies.has(row.code) ? 'chevronDown' : 'chevronRight'" :size="9" />
              </button>
              <AppIcon :name="row.family?.sourceMode === 'parametric' ? 'sliders' : 'box'" :size="11" class="shrink-0 text-[#6B8A9B]" />
              <span class="min-w-0 flex-1">
                <span class="block truncate text-[10.5px] font-medium" :class="familyFilter === row.code ? 'text-primary' : 'text-[#374151]'">{{ row.label }}</span>
                <span class="mono block truncate text-[8px] text-[#9CA3AF]">{{ row.code }}</span>
              </span>
              <span class="mono shrink-0 text-[8px] text-[#B0BAC3]">{{ row.family?.presets.length }}</span>
            </div>

            <button
              v-else-if="row.type === 'preset'"
              :data-tree-preset="row.code"
              class="flex w-full items-center gap-1.5 rounded-[3px] py-1.5 pr-1 text-left transition-colors"
              :class="selectedModel === row.code ? 'bg-[#E8EFF4]' : 'hover:bg-[#F3F6F8]'"
              :style="{ paddingLeft: `${24 + row.depth * 12}px` }"
              @click="row.family && row.preset && selectPreset(row.family, row.preset)"
            >
              <span class="h-1 w-1 shrink-0 rounded-full" :class="selectedModel === row.code ? 'bg-primary' : 'bg-[#A8B8C4]'" />
              <span class="mono min-w-0 flex-1 truncate text-[10.5px]" :class="selectedModel === row.code ? 'font-semibold text-primary' : 'text-[#4B5563]'">{{ row.code }}</span>
              <span class="hidden truncate text-[9.5px] text-[#9CA3AF] xl:block">{{ row.label }}</span>
            </button>
          </template>

          <div v-if="!catalog.length" class="px-2 py-6 text-center text-[10.5px] text-[#9CA3AF]">目录加载中…</div>
        </div>

        <div class="border-t border-line bg-[#FAFBFC] px-3 py-2 text-[9px] leading-relaxed text-[#9CA3AF]">
          <p>{{ catalog.length }} 个族 · {{ catalogPresetCount }} 个预定义型号 · {{ catalogParametricCount }} 个参数化族</p>
          <p class="mt-0.5">分类标准为主目录；专业、几何能力为右侧过滤项。</p>
        </div>
      </div>
    </aside>

    <!-- 主区 -->
    <section class="min-w-0 flex-1">
      <div class="mb-3 flex flex-wrap items-end justify-between gap-2">
        <div>
          <div class="flex flex-wrap items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
            <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
            <AppIcon name="chevronRight" :size="10" />
            <button class="hover:text-primary" @click="clearFamilyFilter">构件资产大厅</button>
            <template v-if="familyFilter && selectedFamily">
              <AppIcon name="chevronRight" :size="10" />
              <span class="text-[#4B5563]">{{ selectedFamily.name }}</span>
              <template v-if="selectedModel">
                <AppIcon name="chevronRight" :size="10" />
                <span class="mono text-primary">{{ selectedModel }}</span>
              </template>
            </template>
          </div>
          <h1 class="mt-1 text-[21px] font-semibold tracking-tight text-ink">{{ resultHeading }}</h1>
          <p class="mt-0.5 text-[12px] text-muted">
            <template v-if="familyFilter && selectedFamily">
              {{ selectedFamily.familyId }} · {{ selectedFamily.category }} ·
              <span class="mono font-semibold text-ink">{{ selectedFamily.presets.length }}</span> 个预定义型号
            </template>
            <template v-else>
              已纳管 <span class="mono font-semibold text-ink">{{ scopedTotal || catalog.length }}</span> 个族资产 ·
              <span class="mono font-semibold text-ink">{{ catalogPresetCount }}</span> 个预定义型号 · CBF-C v1.4.2
            </template>
          </p>
        </div>
        <div class="flex items-center gap-1.5">
          <button class="btn btn-outline btn-sm lg:hidden" @click="showTreeMobile = true">
            <AppIcon name="tree" :size="13" />资产主目录
          </button>
          <button v-if="familyFilter" class="btn btn-outline btn-sm" @click="clearFamilyFilter">
            <AppIcon name="arrowLeft" :size="12" />返回资产目录
          </button>
          <template v-else>
            <div class="flex items-center rounded-[5px] border border-line bg-surface p-0.5">
              <button
                class="flex items-center gap-1 rounded-[3px] px-2 py-1 text-[11px]"
                :class="viewMode === 'grid' ? 'bg-[#E8EFF4] font-medium text-primary' : 'text-[#6B7280]'"
                @click="viewMode = 'grid'"
              >
                <AppIcon name="grid" :size="12" />网格
              </button>
              <button
                class="flex items-center gap-1 rounded-[3px] px-2 py-1 text-[11px]"
                :class="viewMode === 'table' ? 'bg-[#E8EFF4] font-medium text-primary' : 'text-[#6B7280]'"
                @click="viewMode = 'table'"
              >
                <AppIcon name="list" :size="12" />表格
              </button>
            </div>
            <select class="select !w-auto !py-1 !text-[11.5px]" :value="sort" :disabled="Boolean(similarTo)" @change="changeSort(($event.target as HTMLSelectElement).value)">
              <option value="latest">最新发布</option>
              <option value="popular">复用频次</option>
              <option value="volume">体积排序</option>
              <option value="name">名称排序</option>
            </select>
          </template>
        </div>
      </div>

      <!-- 族下型号视图 -->
      <template v-if="familyFilter">
        <div v-if="familyLoading" class="grid gap-3 sm:grid-cols-2 xl:grid-cols-3">
          <div v-for="i in 3" :key="i" class="panel h-52 animate-pulse bg-[#F3F6F8]" />
        </div>
        <div v-else-if="presetList.length" class="grid gap-3 sm:grid-cols-2 xl:grid-cols-3">
          <article
            v-for="preset in presetList"
            :key="preset.modelNumber"
            :data-preset-model="preset.modelNumber"
            class="panel overflow-hidden transition-shadow"
            :class="selectedModel === preset.modelNumber ? 'ring-2 ring-primary ring-offset-2' : ''"
          >
            <header class="flex items-start justify-between gap-2 border-b border-line bg-[#FAFBFC] px-3 py-2.5">
              <div class="min-w-0">
                <p class="mono truncate text-[11px] font-semibold text-ink">{{ preset.modelNumber }}</p>
                <p class="mt-0.5 truncate text-[12px] text-[#374151]">{{ preset.label }}</p>
              </div>
              <span class="rounded bg-[#ECFDF5] px-1.5 py-[1px] text-[9px] font-medium text-success">{{ preset.isPrebaked ? '预烘焙' : '按需求解' }}</span>
            </header>
            <div class="px-3 py-2.5">
              <p class="text-[10px] leading-relaxed text-[#64748B]">{{ sourceModeMeta(selectedFamily?.sourceMode ?? 'mesh-only').label }} · {{ selectedFamily?.category }}</p>
              <dl class="mt-2 grid grid-cols-2 gap-x-3 gap-y-1.5 text-[10px]">
                <div class="flex justify-between gap-1"><dt class="text-[#9CA3AF]">净体积</dt><dd class="mono">{{ formatNumber(Number(preset.params.NetVolume), 2) }} m³</dd></div>
                <div class="flex justify-between gap-1"><dt class="text-[#9CA3AF]">图号</dt><dd class="mono truncate">{{ preset.drawingCode ?? '—' }}</dd></div>
                <div class="col-span-2 flex justify-between gap-1"><dt class="text-[#9CA3AF]">关键参数</dt><dd class="mono truncate text-right">{{ presetSummary(preset) || '—' }}</dd></div>
                <div class="col-span-2 flex justify-between gap-1"><dt class="text-[#9CA3AF]">算键</dt><dd class="mono truncate text-[9px] text-[#64748B]">{{ maskKey(preset.cacheKey, 10, 6) }}</dd></div>
              </dl>
            </div>
            <footer class="flex items-center justify-between gap-2 border-t border-line px-3 py-2">
              <span class="text-[9px] text-[#9CA3AF]">型号级独立缓存键</span>
              <div class="flex items-center gap-1">
                <button class="btn btn-primary btn-sm" @click="openPresetDetail(selectedFamily!)">
                  <AppIcon name="eye" :size="12" />查看详情
                </button>
                <button
                  v-if="selectedFamily?.sourceMode === 'parametric'"
                  class="btn btn-outline btn-sm"
                  @click="openPresetPage(selectedFamily!, preset)"
                >
                  <AppIcon name="sliders" :size="12" />配置参数
                </button>
              </div>
            </footer>
          </article>
        </div>
        <EmptyState
          v-else
          icon="box"
          title="该族尚未定义预定义型号"
          description="可在族库管理中新增型号，或切换其他族查看。"
        />
      </template>

      <!-- 全部资产 / 检索 / 过滤 -->
      <template v-else>
        <!-- 检索条件：名称 / 编码 / 几何相似 -->
        <div class="panel mb-3 p-2.5">
          <div class="flex flex-wrap items-center gap-2">
            <div class="flex rounded-[5px] border border-line bg-[#F7F9FB] p-0.5">
              <button
                class="rounded-[3px] px-2.5 py-1.5 text-[11px] transition-colors"
                :class="searchMode === 'text' ? 'bg-white font-semibold text-primary shadow-sm' : 'text-[#6B7280]'"
                @click="setSearchMode('text')"
              >
                <AppIcon name="search" :size="11" class="mr-1 inline" />名称 / 编码
              </button>
              <button
                class="rounded-[3px] px-2.5 py-1.5 text-[11px] transition-colors"
                :class="searchMode === 'shape' ? 'bg-white font-semibold text-primary shadow-sm' : 'text-[#6B7280]'"
                @click="setSearchMode('shape')"
              >
                <AppIcon name="cube" :size="11" class="mr-1 inline" />几何相似
              </button>
            </div>

            <template v-if="searchMode === 'text'">
              <div class="flex min-w-[240px] flex-1 items-center gap-1.5">
                <AppIcon name="search" :size="14" class="text-[#9CA3AF]" />
                <input
                  v-model="searchInput"
                  class="min-w-0 flex-1 border-0 bg-transparent text-[12.5px] text-ink placeholder:text-[#9CA3AF] focus:outline-none"
                  placeholder="构件名称、familyId、图号、语义码（Bridge.Pier）…"
                  spellcheck="false"
                  @input="onSearchInput"
                  @keydown.enter.prevent="onSearchEnter"
                />
                <button v-if="searchInput" class="text-[#9CA3AF] hover:text-ink" @click="searchInput = ''; q = ''; reload()">
                  <AppIcon name="close" :size="13" />
                </button>
                <button class="btn btn-primary btn-sm" @click="onSearchEnter">检索</button>
              </div>
            </template>

            <template v-else>
              <div class="flex min-w-[280px] flex-1 flex-wrap items-center gap-1.5">
                <select v-model="similarTo" class="select h-8 min-w-[210px] flex-1 text-[11.5px]">
                  <option value="">选择参考构件族…</option>
                  <option v-for="family in catalog" :key="family.familyId" :value="family.familyId">
                    {{ family.name }} · {{ family.familyId }}
                  </option>
                </select>
                <button class="btn btn-primary btn-sm" :disabled="!similarTo" @click="applySimilar">
                  <AppIcon name="gitCompare" :size="12" />查找相似
                </button>
                <button v-if="similarTo" class="btn btn-ghost btn-sm" @click="clearSimilar">清除</button>
              </div>
            </template>
          </div>

          <div v-if="searchMode === 'shape'" class="mt-2 flex flex-wrap items-center gap-2 border-t border-[#F0F2F5] pt-2 text-[10px] text-[#9CA3AF]">
            <span>相似度 = 尺寸 / 体积 / 拓扑规模归一化距离 + 类别加权</span>
            <span class="mono">参考：{{ similarReference?.familyId || '未选择' }}</span>
            <div
              class="ml-auto flex items-center gap-1 rounded border border-dashed px-2 py-1 text-[9.5px]"
              :class="dragOver ? 'border-primary bg-[#EDF4F8] text-primary' : 'border-[#D8DEE5] text-[#9CA3AF]'"
              @dragover.prevent="dragOver = true"
              @dragleave="dragOver = false"
              @drop.prevent="onDrop"
            >
              <AppIcon name="upload" :size="11" />拖入 .step/.cbf 需 OCCT Worker
            </div>
          </div>

          <div class="mt-2 flex flex-wrap items-center gap-1.5 border-t border-[#F0F2F5] pt-2">
            <span class="text-[10px] text-[#9CA3AF]">热门：</span>
            <button
              v-for="hot in ['桥墩', '箱梁', '支座', '承台', '隧道衬砌']"
              :key="hot"
              class="chip hover:border-[#C7D2DA] hover:text-primary"
              @click="setSearchMode('text'); searchInput = hot; onSearchEnter()"
            >
              {{ hot }}
            </button>
            <span class="mx-1 h-3 w-px bg-line" />
            <div class="flex flex-wrap items-center gap-1">
              <span
                v-for="chip in activeChips"
                :key="chip.key"
                class="inline-flex items-center gap-1 rounded-full border border-[#C9DCE6] bg-[#EDF4F8] px-2 py-[2px] text-[10px] text-[#1F4E66]"
              >
                {{ chip.label }}
                <button class="text-[#6B8A9B] hover:text-[#1F5A7A]" @click="chip.clear()"><AppIcon name="close" :size="9" /></button>
              </span>
              <button v-if="activeChips.length" class="text-[10px] text-primary hover:underline" @click="clearAll">
                清空全部
              </button>
            </div>
          </div>
        </div>

        <!-- 过滤条件：专业 / 能力 / 状态 -->
        <div class="panel mb-3 p-3">
          <div class="flex flex-wrap items-center gap-2">
            <span class="flex items-center gap-1 text-[10px] font-semibold uppercase tracking-wider text-[#6B7280]"><AppIcon name="filter" :size="11" />过滤</span>
            <label class="flex shrink-0 items-center gap-1.5 whitespace-nowrap text-[10.5px] text-[#6B7280]">
              专业
              <select v-model="discipline" class="select h-7 !py-0.5 text-[11px]" @change="page = 1; reload()">
                <option value="all">全部专业</option>
                <option v-for="item in DISCIPLINES" :key="item" :value="item">{{ item }}（{{ disciplineCounts[item] ?? 0 }}）</option>
              </select>
            </label>
            <label class="flex shrink-0 items-center gap-1.5 whitespace-nowrap text-[10.5px] text-[#6B7280]">
              状态
              <select v-model="status" class="select h-7 !py-0.5 text-[11px]" @change="page = 1; reload()">
                <option value="all">全部状态</option>
                <option value="published">已发布</option>
                <option value="reviewing">审核中</option>
                <option value="draft">草稿</option>
                <option value="archived">已归档</option>
              </select>
            </label>
            <button class="btn btn-ghost btn-sm ml-auto text-[#9CA3AF]" @click="clearAll">
              <AppIcon name="refresh" :size="11" />重置全部
            </button>
          </div>

          <div class="mt-2 flex flex-wrap items-center gap-1.5 border-t border-[#F0F2F5] pt-2">
            <span class="mr-1 text-[10px] text-[#9CA3AF]">几何能力</span>
            <button
              v-for="tab in sourceTabs"
              :key="tab.value"
              :data-source="tab.value"
              class="flex items-center gap-1 rounded-full border px-2 py-1 text-[10.5px] transition-colors"
              :class="sourceMode === tab.value ? 'border-primary bg-[#EDF4F8] font-semibold text-primary' : 'border-line text-[#6B7280] hover:border-[#C9DCE6]'"
              @click="selectSource(tab.value)"
            >
              {{ tab.label }}
              <span class="mono text-[9px] text-[#9CA3AF]">{{ sourceCounts[tab.value] ?? 0 }}</span>
            </button>
          </div>
        </div>

        <div class="mb-2 flex items-center justify-between gap-2">
          <div class="flex min-w-0 items-center gap-2">
            <p class="truncate text-[11px] text-muted">
              <span class="font-semibold text-ink">{{ total }}</span> 个结果
              <template v-if="similarTo"> · 按几何相似度排序</template>
              <template v-else> · 全部资产</template>
            </p>
            <span v-if="data?.similarityReference" class="hidden rounded bg-[#EDF4F8] px-1.5 py-[1px] text-[9px] text-primary sm:inline">
              参考 {{ data.similarityReference.name }}
            </span>
          </div>
          <p class="shrink-0 font-mono text-[10px] text-[#9CA3AF]">P95 {{ data?.tookMs ?? 0 }}ms</p>
        </div>

        <div v-if="error" class="panel p-6 text-center">
          <p class="text-[13px] font-medium text-danger">{{ error }}</p>
          <button class="btn btn-outline btn-sm mt-2" @click="fetchData">重试</button>
        </div>

        <div v-else-if="loading && !data" class="grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-3 min-[1900px]:grid-cols-4">
          <div v-for="i in 8" :key="i" class="panel overflow-hidden">
            <div class="skeleton h-[168px] w-full rounded-none" />
            <div class="space-y-2 p-3">
              <div class="skeleton h-4 w-2/3" />
              <div class="skeleton h-3 w-1/2" />
              <div class="skeleton h-12 w-full" />
              <div class="skeleton h-7 w-full" />
            </div>
          </div>
        </div>

        <template v-else-if="items.length">
          <div v-if="viewMode === 'grid'" class="grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-3 min-[1900px]:grid-cols-4">
            <ComponentCard
              v-for="item in items"
              :key="item.familyId"
              :component="item"
              :favorited="favorites.has(item.familyId)"
              :similarity="item.similarity"
              @preview="openPreview"
              @favorite="toggleFavorite"
            />
          </div>

          <div v-else class="panel overflow-hidden">
            <div class="overflow-x-auto">
              <table class="table-dense">
                <thead>
                  <tr>
                    <th class="w-[46px]">预览</th>
                    <th>构件名称 / 图号</th>
                    <th>分类编码</th>
                    <th>专业</th>
                    <th>能力</th>
                    <th v-if="similarTo" class="text-right">相似度</th>
                    <th class="text-right">体积</th>
                    <th class="text-right">高度</th>
                    <th>状态</th>
                    <th>更新</th>
                    <th class="w-[236px] text-right">操作</th>
                  </tr>
                </thead>
                <tbody>
                  <tr v-for="item in items" :key="item.familyId">
                    <td>
                      <button class="block h-8 w-11 overflow-hidden rounded border border-line bg-white" @click="openPreview(item)">
                        <img :src="api.previewUrl(item.familyId, 88, 64)" alt="" class="h-full w-full object-contain" loading="lazy" />
                      </button>
                    </td>
                    <td>
                      <RouterLink :to="`/components/${item.familyId}`" class="font-medium text-ink hover:text-primary">{{ cleanDisplayName(item.name) }}</RouterLink>
                      <p class="mono text-[10px] text-muted">{{ item.drawingCode }} · v{{ item.revision }}</p>
                    </td>
                    <td>
                      <p class="font-mono text-[10.5px] text-[#374151]">{{ item.taxonomy.crbim }}</p>
                      <p class="text-[10px] text-muted">{{ item.category }}</p>
                    </td>
                    <td class="text-[10.5px]">{{ item.discipline ?? '—' }}</td>
                    <td>
                      <StatusBadge :tone="sourceModeMeta(item.sourceMode).tone" :label="sourceModeMeta(item.sourceMode).label" :dot="false" />
                    </td>
                    <td v-if="similarTo" class="text-right">
                      <span class="mono rounded bg-[#EDF4F8] px-1.5 py-[1px] text-[10px] font-semibold text-primary">{{ item.similarity ?? 0 }}%</span>
                    </td>
                    <td class="text-right font-mono text-[11px]">{{ formatNumber(item.geometry.volumeM3, 2) }} m³</td>
                    <td class="text-right font-mono text-[11px]">{{ formatNumber(item.dims.height / 1000, 2) }} m</td>
                    <td>
                      <StatusBadge :tone="componentStatusMeta(item.status).tone" :label="componentStatusMeta(item.status).label" />
                    </td>
                    <td class="font-mono text-[10px] text-muted">{{ formatDate(item.updatedAt) }}</td>
                    <td>
                      <div class="flex items-center justify-end gap-1">
                        <button class="btn btn-ghost btn-sm w-7 justify-center !px-0" title="加入对比" @click="compare.toggle(item)">
                          <AppIcon name="gitCompare" :size="12" />
                        </button>
                        <RouterLink :to="`/components/${item.familyId}`" class="btn btn-primary btn-sm w-[92px] justify-center">
                          <AppIcon name="eye" :size="11" />查看详情
                        </RouterLink>
                        <button
                          class="btn btn-outline btn-sm w-[92px] justify-center"
                          :disabled="item.sourceMode !== 'parametric'"
                          :title="item.sourceMode === 'parametric' ? '配置参数' : '该构件为只读几何，不支持参数配置'"
                          @click="item.sourceMode === 'parametric' && router.push(`/components/${item.familyId}/configurator`)"
                        >
                          <AppIcon name="sliders" :size="11" />配置参数
                        </button>
                      </div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>

          <div class="mt-3 flex flex-wrap items-center justify-between gap-2">
            <div class="flex items-center gap-2 text-[11px] text-muted">
              <span>每页</span>
              <select class="select !w-auto !py-1 !text-[11px]" :value="pageSize" @change="pageSize = Number(($event.target as HTMLSelectElement).value); page = 1; reload()">
                <option :value="12">12</option>
                <option :value="24">24</option>
                <option :value="36">36</option>
              </select>
              <span>条 · 共 {{ total }} 条</span>
            </div>
            <div class="flex items-center gap-1">
              <button class="btn btn-outline btn-sm" :disabled="page <= 1" @click="goPage(page - 1)">
                <AppIcon name="chevronLeft" :size="12" />上一页
              </button>
              <button
                v-for="p in Math.min(totalPages, 6)"
                :key="p"
                class="flex h-7 min-w-7 items-center justify-center rounded-[4px] border px-1.5 font-mono text-[11px]"
                :class="p === page ? 'border-primary bg-primary font-semibold text-white' : 'border-line bg-surface text-[#4B5563] hover:bg-app'"
                @click="goPage(p)"
              >
                {{ p }}
              </button>
              <span v-if="totalPages > 6" class="text-[11px] text-[#9CA3AF]">… {{ totalPages }}</span>
              <button class="btn btn-outline btn-sm" :disabled="page >= totalPages" @click="goPage(page + 1)">
                下一页<AppIcon name="chevronRight" :size="12" />
              </button>
            </div>
          </div>
        </template>

        <EmptyState
          v-else
          icon="search"
          title="当前组合无匹配构件"
          :description="data?.diagnostics ? `系统诊断：${data.diagnostics.suggestion}` : '请尝试放宽参数区间或切换分类标准。'"
        >
          <button v-if="activeChips.length" class="btn btn-primary btn-sm" @click="clearAll">
            <AppIcon name="refresh" :size="13" />一键清空全部筛选
          </button>
          <button class="btn btn-outline btn-sm" @click="submitCustomRequest">
            <AppIcon name="flag" :size="13" />提交构件定制需求
          </button>
        </EmptyState>
      </template>
    </section>


  </div>
</template>
