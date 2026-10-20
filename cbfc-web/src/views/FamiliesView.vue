<script setup lang="ts">
import { computed, onMounted, reactive, ref } from 'vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import ModalShell from '@/components/ui/ModalShell.vue';
import { api } from '@/api/client';
import type { FamilyAdminView, Preset } from '@/api/types';
import { useUiStore } from '@/stores/ui';
import { componentStatusMeta, formatNumber, formatVolume, maskKey } from '@/utils/format';

const ui = useUiStore();
const families = ref<FamilyAdminView[]>([]);
const loading = ref(true);
const query = ref('');
const statusFilter = ref('all');
const busy = ref(false);

const createOpen = ref(false);
const editOpen = ref(false);
const presetOpen = ref(false);
const createForm = reactive({
  templateFamilyId: 'PierBody',
  familyId: '',
  name: '',
  category: '',
  semanticCode: '',
});
const editTarget = ref<FamilyAdminView | null>(null);
const editForm = reactive({ name: '', category: '', semanticCode: '', status: 'draft' });
const presetTarget = ref<FamilyAdminView | null>(null);
const presetEditing = ref<Preset | null>(null);
const presetForm = reactive({
  modelNumber: '',
  label: '',
  drawingCode: '',
  overrides: {} as Record<string, number>,
});

const parametricFamilies = computed(() => families.value.filter((family) => family.sourceMode === 'parametric'));
const templates = computed(() => [...families.value].sort((a, b) => a.name.localeCompare(b.name)));

const filtered = computed(() => {
  const keyword = query.value.trim().toLowerCase();
  return families.value.filter((family) => {
    if (statusFilter.value !== 'all' && family.status !== statusFilter.value) return false;
    if (!keyword) return true;
    return [family.name, family.familyId, family.category, family.semanticCode]
      .join(' ')
      .toLowerCase()
      .includes(keyword);
  });
});

const stats = computed(() => ({
  families: families.value.length,
  presets: families.value.reduce((sum, family) => sum + family.presets.length, 0),
  drafts: families.value.filter((family) => family.status === 'draft' || family.status === 'reviewing').length,
  parametric: families.value.filter((family) => family.sourceMode === 'parametric').length,
}));

function statusTone(status: string) {
  if (status === 'published') return 'bg-[#ECFDF5] text-[#047857] border-[#A7F3D0]';
  if (status === 'reviewing') return 'bg-[#FFFBEB] text-[#B45309] border-[#FDE68A]';
  if (status === 'archived') return 'bg-[#F1F5F9] text-[#64748B] border-[#CBD5E1]';
  return 'bg-[#EFF6FF] text-[#1D4ED8] border-[#BFDBFE]';
}

function replaceFamily(next: FamilyAdminView) {
  const index = families.value.findIndex((family) => family.familyId === next.familyId);
  if (index >= 0) families.value.splice(index, 1, next);
  else families.value.unshift(next);
}

async function load() {
  loading.value = true;
  try {
    const data = await api.listFamilies();
    families.value = data.items;
    if (!createForm.templateFamilyId || !data.items.some((f) => f.familyId === createForm.templateFamilyId)) {
      createForm.templateFamilyId = data.items.find((f) => f.sourceMode === 'parametric')?.familyId ?? data.items[0]?.familyId ?? '';
    }
  } catch (error) {
    ui.notify({ type: 'error', title: '族库加载失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    loading.value = false;
  }
}

onMounted(load);

function openCreate() {
  Object.assign(createForm, {
    templateFamilyId: parametricFamilies.value[0]?.familyId ?? templates.value[0]?.familyId ?? '',
    familyId: '',
    name: '',
    category: parametricFamilies.value[0]?.category ?? '',
    semanticCode: parametricFamilies.value[0]?.semanticCode ?? '',
  });
  createOpen.value = true;
}

async function createFamily() {
  if (!createForm.templateFamilyId || !createForm.familyId.trim() || !createForm.name.trim()) {
    ui.notify({ type: 'warning', title: '信息不完整', message: '模板、族 ID 与族名称必填。' });
    return;
  }
  busy.value = true;
  try {
    const created = await api.createFamily({
      templateFamilyId: createForm.templateFamilyId,
      familyId: createForm.familyId.trim(),
      name: createForm.name.trim(),
      category: createForm.category.trim() || undefined,
      semanticCode: createForm.semanticCode.trim() || undefined,
    });
    replaceFamily(created);
    createOpen.value = false;
    ui.notify({ type: 'success', title: '族已创建', message: `${created.name}（${created.familyId}）已进入草稿状态，可继续维护型号。` });
  } catch (error) {
    ui.notify({ type: 'error', title: '创建失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    busy.value = false;
  }
}

function openEdit(family: FamilyAdminView) {
  editTarget.value = family;
  Object.assign(editForm, {
    name: family.name,
    category: family.category,
    semanticCode: family.semanticCode,
    status: family.status,
  });
  editOpen.value = true;
}

async function saveFamily() {
  if (!editTarget.value || !editForm.name.trim()) return;
  busy.value = true;
  try {
    const next = await api.updateFamily(editTarget.value.familyId, {
      name: editForm.name.trim(),
      category: editForm.category.trim(),
      semanticCode: editForm.semanticCode.trim(),
      status: editForm.status,
    });
    replaceFamily(next);
    editOpen.value = false;
    ui.notify({ type: 'success', title: '族定义已更新', message: `${next.name} 的元数据已保存。` });
  } catch (error) {
    ui.notify({ type: 'error', title: '保存失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    busy.value = false;
  }
}

async function archiveFamily(family: FamilyAdminView) {
  if (!window.confirm(`确认归档族 ${family.name}？归档后仍可检索，但不再作为可用资产推荐。`)) return;
  busy.value = true;
  try {
    replaceFamily(await api.archiveFamily(family.familyId));
    ui.notify({ type: 'success', title: '族已归档', message: family.familyId });
  } catch (error) {
    ui.notify({ type: 'error', title: '归档失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    busy.value = false;
  }
}

function openPreset(family: FamilyAdminView, preset?: Preset) {
  if (family.sourceMode !== 'parametric') {
    ui.notify({ type: 'warning', title: '固定几何族', message: '仅 parametric 族支持维护预定义型号。' });
    return;
  }
  presetTarget.value = family;
  presetEditing.value = preset ?? null;
  const base = preset ?? family.presets.find((item) => item.modelNumber === String(family.presets[0]?.modelNumber)) ?? family.presets[0];
  const overrides: Record<string, number> = {};
  for (const field of family.inputs) {
    const value = preset?.params[field.name] ?? base?.params[field.name];
    if (typeof value === 'number') overrides[field.name] = value;
  }
  Object.assign(presetForm, {
    modelNumber: preset?.modelNumber ?? '',
    label: preset?.label ?? '',
    drawingCode: preset?.drawingCode ?? '',
    overrides,
  });
  presetOpen.value = true;
}

async function savePreset() {
  const family = presetTarget.value;
  if (!family) return;
  if (!presetForm.modelNumber.trim() || !presetForm.label.trim()) {
    ui.notify({ type: 'warning', title: '信息不完整', message: '型号编号与型号名称必填。' });
    return;
  }
  busy.value = true;
  try {
    const payload = {
      label: presetForm.label.trim(),
      drawingCode: presetForm.drawingCode.trim() || undefined,
      overrides: { ...presetForm.overrides },
    };
    const next = presetEditing.value
      ? await api.updatePreset(family.familyId, presetEditing.value.modelNumber, payload)
      : await api.addPreset(family.familyId, { ...payload, modelNumber: presetForm.modelNumber.trim() });
    replaceFamily(next);
    presetOpen.value = false;
    ui.notify({
      type: 'success',
      title: presetEditing.value ? '型号已更新' : '型号已新增',
      message: `${next.name} 当前共 ${next.presets.length} 个预定义型号。`,
    });
  } catch (error) {
    ui.notify({ type: 'error', title: '型号保存失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    busy.value = false;
  }
}

async function deletePreset(family: FamilyAdminView, preset: Preset) {
  if (!window.confirm(`确认删除型号 ${preset.modelNumber}？`)) return;
  busy.value = true;
  try {
    replaceFamily(await api.deletePreset(family.familyId, preset.modelNumber));
    ui.notify({ type: 'success', title: '型号已删除', message: preset.modelNumber });
  } catch (error) {
    ui.notify({ type: 'error', title: '删除失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    busy.value = false;
  }
}

function presetSummary(family: FamilyAdminView, preset: Preset) {
  return family.inputs
    .slice(0, 3)
    .map((field) => `${field.label} ${formatNumber(Number(preset.params[field.name]))}${field.unit ?? ''}`)
    .join(' · ');
}
</script>

<template>
  <div class="m-3 space-y-3">
    <section class="panel flex flex-wrap items-end justify-between gap-3 px-4 py-3.5">
      <div>
        <p class="eyebrow flex items-center gap-1.5"><AppIcon name="grid" :size="12" />FAMILY LIBRARY GOVERNANCE</p>
        <h1 class="mt-1 text-[20px] font-semibold tracking-tight text-ink">族库管理</h1>
        <p class="mt-1 text-[11.5px] text-muted">
          族定义 · 预定义型号 · 参数覆盖 · 权威算键 · 发布状态
        </p>
        <p class="mt-1.5 flex flex-wrap gap-x-3 gap-y-1 font-mono text-[10px] text-muted">
          <span>族 {{ stats.families }}</span>
          <span>型号 {{ stats.presets }}</span>
          <span class="text-warning">草稿/待审 {{ stats.drafts }}</span>
          <span class="text-primary">参数化 {{ stats.parametric }}</span>
        </p>
      </div>
      <div class="flex items-center gap-2">
        <RouterLink to="/library" class="btn btn-outline btn-sm"><AppIcon name="search" :size="12" />资产大厅</RouterLink>
        <button class="btn btn-primary btn-sm" @click="openCreate"><AppIcon name="plus" :size="13" />基于模板新建族</button>
      </div>
    </section>

    <section class="panel overflow-hidden">
      <div class="flex flex-wrap items-center gap-2 border-b border-line bg-[#FAFBFC] px-3 py-2.5">
        <label class="relative min-w-[220px] flex-1">
          <AppIcon name="search" :size="13" class="pointer-events-none absolute left-2.5 top-1/2 -translate-y-1/2 text-[#9CA3AF]" />
          <input v-model="query" class="input h-8 pl-7 text-[11.5px]" placeholder="检索族名称、familyId、分类、语义码…" />
        </label>
        <select v-model="statusFilter" class="input h-8 w-[132px] text-[11px]">
          <option value="all">全部状态</option>
          <option value="published">已发布</option>
          <option value="reviewing">审核中</option>
          <option value="draft">草稿</option>
          <option value="archived">已归档</option>
        </select>
        <span class="ml-auto text-[10px] text-[#9CA3AF]">{{ filtered.length }} / {{ families.length }} 个族</span>
      </div>

      <div v-if="loading" class="space-y-2 p-4">
        <div v-for="i in 3" :key="i" class="h-24 animate-pulse rounded-md bg-[#F3F6F8]" />
      </div>

      <div v-else-if="!filtered.length" class="p-10 text-center">
        <AppIcon name="grid" :size="28" class="mx-auto text-[#CBD5E1]" />
        <p class="mt-2 text-[13px] font-medium text-ink">没有匹配的族</p>
        <p class="mt-1 text-[11px] text-muted">调整检索词或状态筛选，或基于模板新建族。</p>
      </div>

      <div v-else class="grid gap-3 p-3 xl:grid-cols-2">
        <article v-for="family in filtered" :key="family.familyId" class="rounded-md border border-line bg-white">
          <header class="flex items-start gap-2 border-b border-[#EDF0F3] px-3 py-2.5">
            <div class="mt-0.5 flex h-8 w-8 shrink-0 items-center justify-center rounded-[5px] bg-[#E8EFF4] text-primary">
              <AppIcon :name="family.sourceMode === 'parametric' ? 'sliders' : 'box'" :size="15" />
            </div>
            <div class="min-w-0 flex-1">
              <div class="flex flex-wrap items-center gap-1.5">
                <h2 class="truncate text-[13.5px] font-semibold text-ink">{{ family.name }}</h2>
                <span class="rounded border px-1.5 py-[1px] text-[9px]" :class="statusTone(family.status)">
                  {{ componentStatusMeta(family.status).label }}
                </span>
                <span class="mono rounded bg-[#F1F4F7] px-1.5 py-[1px] text-[9px] text-[#475569]">v{{ family.revision }}</span>
              </div>
              <p class="mono mt-0.5 text-[9.5px] text-[#9CA3AF]">{{ family.familyId }} · {{ family.category }} · {{ family.semanticCode }}</p>
            </div>
            <div class="flex shrink-0 items-center gap-1">
              <button class="btn btn-ghost btn-sm" title="编辑族定义" @click="openEdit(family)"><AppIcon name="edit" :size="12" /></button>
              <button class="btn btn-ghost btn-sm" title="打开构件详情" @click="$router.push(`/components/${family.familyId}`)"><AppIcon name="arrowRight" :size="12" /></button>
            </div>
          </header>

          <div class="flex items-center justify-between gap-2 px-3 py-2">
            <p class="text-[10px] text-muted">
              <span class="font-semibold text-ink">{{ family.presets.length }}</span> 个预定义型号 ·
              {{ family.triangleCount.toLocaleString() }} 三角面
            </p>
            <button
              class="btn btn-ghost btn-sm text-primary"
              :class="{ 'cursor-not-allowed opacity-40': family.sourceMode !== 'parametric' }"
              :disabled="family.sourceMode !== 'parametric'"
              @click="openPreset(family)"
            >
              <AppIcon name="plus" :size="11" />新增型号
            </button>
          </div>

          <div class="space-y-1 px-3 pb-3">
            <div
              v-for="preset in family.presets"
              :key="preset.modelNumber"
              class="group flex items-center gap-2 rounded border border-transparent px-2 py-1.5 hover:border-line hover:bg-[#FAFBFC]"
            >
              <span class="mono w-[112px] shrink-0 truncate text-[10px] font-semibold text-[#334155]">{{ preset.modelNumber }}</span>
              <div class="min-w-0 flex-1">
                <p class="truncate text-[10.5px] text-[#374151]">{{ preset.label }}</p>
                <p class="truncate text-[9px] text-[#9CA3AF]">{{ presetSummary(family, preset) }}</p>
              </div>
              <span class="mono hidden w-[64px] shrink-0 text-right text-[9px] text-[#64748B] sm:block">{{ formatVolume(Number(preset.params.NetVolume)) }}</span>
              <span class="mono hidden w-[62px] shrink-0 truncate text-[8px] text-[#9CA3AF] lg:block">{{ maskKey(preset.cacheKey, 7, 4) }}</span>
              <div class="flex shrink-0 items-center gap-0.5 opacity-0 transition-opacity group-hover:opacity-100">
                <button class="btn btn-ghost btn-sm" title="编辑型号" @click="openPreset(family, preset)"><AppIcon name="edit" :size="11" /></button>
                <button class="btn btn-ghost btn-sm text-[#B91C1C]" title="删除型号" @click="deletePreset(family, preset)"><AppIcon name="trash" :size="11" /></button>
              </div>
            </div>
          </div>

          <footer class="flex items-center justify-between gap-2 border-t border-[#EDF0F3] px-3 py-2">
            <span class="mono truncate text-[8.5px] text-[#9CA3AF]">权威 {{ maskKey(family.defaultCacheKey, 10, 6) }}</span>
            <button
              v-if="family.status !== 'archived'"
              class="text-[9.5px] text-[#9CA3AF] hover:text-[#B91C1C]"
              @click="archiveFamily(family)"
            >
              归档
            </button>
            <span v-else class="text-[9.5px] text-[#9CA3AF]">已归档</span>
          </footer>
        </article>
      </div>
    </section>

    <ModalShell
      :open="createOpen"
      title="基于模板新建族"
      subtitle="克隆模板的几何契约、参数 schema 与预定义型号，再按新族身份发布"
      icon="grid"
      width="md"
      @close="createOpen = false"
    >
      <div class="space-y-3 p-5">
        <label class="block">
          <span class="label">模板族 <span class="text-danger">*</span></span>
          <select v-model="createForm.templateFamilyId" class="input mt-1 w-full text-[12px]">
            <option v-for="family in templates" :key="family.familyId" :value="family.familyId">
              {{ family.name }} · {{ family.familyId }} · {{ family.presets.length }} 型号
            </option>
          </select>
        </label>
        <div class="grid gap-3 sm:grid-cols-2">
          <label class="block">
            <span class="label">新族 ID <span class="text-danger">*</span></span>
            <input v-model="createForm.familyId" class="input mono mt-1 w-full text-[12px]" placeholder="如 PierBodyV2" />
            <span class="mt-1 block text-[9px] text-[#9CA3AF]">字母开头，3–32 位字母/数字/下划线，创建后不可修改</span>
          </label>
          <label class="block">
            <span class="label">族名称 <span class="text-danger">*</span></span>
            <input v-model="createForm.name" class="input mt-1 w-full text-[12px]" placeholder="如 圆端型实体桥墩墩身 V2" />
          </label>
          <label class="block">
            <span class="label">分类</span>
            <input v-model="createForm.category" class="input mt-1 w-full text-[12px]" placeholder="默认继承模板" />
          </label>
          <label class="block">
            <span class="label">语义码</span>
            <input v-model="createForm.semanticCode" class="input mono mt-1 w-full text-[12px]" placeholder="默认继承模板" />
          </label>
        </div>
        <p class="rounded border border-[#BFDBFE] bg-[#EFF6FF] px-2.5 py-2 text-[10px] leading-relaxed text-[#1D4ED8]">
          新族以 <b>draft</b> 状态创建，几何与型号来自模板，权威算键会按新族名/脚本重新计算；需在审核台签批后才进入已发布目录。
        </p>
      </div>
      <template #footer>
        <div class="flex justify-end gap-2">
          <button class="btn btn-outline btn-sm" @click="createOpen = false">取消</button>
          <button class="btn btn-primary btn-sm" :disabled="busy" @click="createFamily">
            <span v-if="busy" class="h-3 w-3 animate-spin rounded-full border-2 border-white/40 border-t-white" />
            创建族
          </button>
        </div>
      </template>
    </ModalShell>

    <ModalShell :open="editOpen" title="编辑族定义" :subtitle="editTarget?.familyId" icon="edit" width="md" @close="editOpen = false">
      <div class="space-y-3 p-5">
        <div class="grid gap-3 sm:grid-cols-2">
          <label class="block">
            <span class="label">族名称</span>
            <input v-model="editForm.name" class="input mt-1 w-full text-[12px]" />
          </label>
          <label class="block">
            <span class="label">发布状态</span>
            <select v-model="editForm.status" class="input mt-1 w-full text-[12px]">
              <option value="draft">草稿</option>
              <option value="reviewing">审核中</option>
              <option value="published">已发布</option>
              <option value="archived">已归档</option>
            </select>
          </label>
          <label class="block">
            <span class="label">分类</span>
            <input v-model="editForm.category" class="input mt-1 w-full text-[12px]" />
          </label>
          <label class="block">
            <span class="label">语义码</span>
            <input v-model="editForm.semanticCode" class="input mono mt-1 w-full text-[12px]" />
          </label>
        </div>
        <p class="text-[10px] leading-relaxed text-muted">族 ID 作为 URL 与缓存契约的一部分不可重命名；型号维护请回到族卡片操作。</p>
      </div>
      <template #footer>
        <div class="flex justify-end gap-2">
          <button class="btn btn-outline btn-sm" @click="editOpen = false">取消</button>
          <button class="btn btn-primary btn-sm" :disabled="busy" @click="saveFamily">保存</button>
        </div>
      </template>
    </ModalShell>

    <ModalShell
      :open="presetOpen"
      :title="presetEditing ? '编辑预定义型号' : '新增预定义型号'"
      :subtitle="presetTarget ? `${presetTarget.name} · ${presetTarget.familyId}` : ''"
      icon="sliders"
      width="lg"
      @close="presetOpen = false"
    >
      <div class="space-y-4 p-5">
        <div class="grid gap-3 sm:grid-cols-3">
          <label class="block">
            <span class="label">型号编号 <span class="text-danger">*</span></span>
            <input v-model="presetForm.modelNumber" class="input mono mt-1 w-full text-[12px]" :disabled="Boolean(presetEditing)" placeholder="如 STD-PIER-20M" />
          </label>
          <label class="block sm:col-span-2">
            <span class="label">型号名称 <span class="text-danger">*</span></span>
            <input v-model="presetForm.label" class="input mt-1 w-full text-[12px]" placeholder="如 20m 标准墩" />
          </label>
          <label class="block sm:col-span-3">
            <span class="label">图号</span>
            <input v-model="presetForm.drawingCode" class="input mono mt-1 w-full text-[12px]" placeholder="留空自动生成 familyId-modelNumber" />
          </label>
        </div>

        <div>
          <p class="eyebrow mb-2">参数覆盖（未修改项继承族默认值）</p>
          <div class="grid gap-3 sm:grid-cols-2 lg:grid-cols-3">
            <label v-for="field in presetTarget?.inputs ?? []" :key="field.name" class="block">
              <span class="label flex items-center justify-between">
                <span>{{ field.label }}</span>
                <span class="mono text-[9px] text-[#9CA3AF]">{{ field.unit || '—' }}</span>
              </span>
              <input
                v-model.number="presetForm.overrides[field.name]"
                type="number"
                class="input mono mt-1 w-full text-[12px]"
                :min="field.min"
                :max="field.max"
                :step="field.step ?? 1"
              />
              <span v-if="field.min !== undefined && field.max !== undefined" class="mt-0.5 block text-[8.5px] text-[#9CA3AF]">
                {{ field.min }} – {{ field.max }}
              </span>
            </label>
          </div>
          <p v-if="!presetTarget?.inputs.length" class="text-[11px] text-muted">该族没有可编辑的输入参数。</p>
        </div>
      </div>
      <template #footer>
        <div class="flex items-center justify-between gap-2">
          <span class="text-[9.5px] text-[#9CA3AF]">保存时服务端重新构建几何、体积与权威缓存键</span>
          <div class="flex gap-2">
            <button class="btn btn-outline btn-sm" @click="presetOpen = false">取消</button>
            <button class="btn btn-primary btn-sm" :disabled="busy" @click="savePreset">
              <span v-if="busy" class="h-3 w-3 animate-spin rounded-full border-2 border-white/40 border-t-white" />
              保存型号
            </button>
          </div>
        </div>
      </template>
    </ModalShell>
  </div>
</template>
