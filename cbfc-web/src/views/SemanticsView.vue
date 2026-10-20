<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import { useRoute } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import ModalShell from '@/components/ui/ModalShell.vue';
import { api } from '@/api/client';
import type { ComponentSummary, SemanticMapping, SemanticProfile, SemanticProfileRule } from '@/api/types';
import { useSessionStore } from '@/stores/session';
import { useUiStore } from '@/stores/ui';
import { formatDate, triggerDownload } from '@/utils/format';

const session = useSessionStore();
const route = useRoute();
const ui = useUiStore();

const profiles = ref<SemanticProfile[]>([]);
const selectedId = ref('');
const profile = ref<SemanticProfile | null>(null);
const sandboxMappings = ref<SemanticMapping[]>([]);
const sandboxProfile = ref<{ profileId: string; version: string; name: string } | null>(null);
const sandboxAt = ref('');
const sandboxLoading = ref(false);
const components = ref<ComponentSummary[]>([]);
const selectedFamily = ref('');
const activePanel = ref<'rules' | 'sandbox'>('rules');
const sandboxFilter = ref<'all' | 'unmapped'>('all');
const ruleFilter = ref('');
const editing = ref<SemanticProfileRule | null>(null);
const saving = ref(false);
const loading = ref(true);
const systemConfirmOpen = ref(false);
const pendingRules = ref<SemanticProfileRule[] | null>(null);

const isEditable = computed(
  () => !!profile.value && session.canManageSemantics && profile.value.tenantId !== 'system',
);
const unmapped = computed(() => sandboxMappings.value.filter((m) => m.status === 'unmapped'));
const displaySandboxMappings = computed(() =>
  sandboxFilter.value === 'unmapped' ? unmapped.value : sandboxMappings.value,
);
const filteredRules = computed(() => {
  if (!profile.value) return [];
  const term = ruleFilter.value.trim().toLowerCase();
  if (!term) return profile.value.rules;
  return profile.value.rules.filter(
    (r) =>
      r.code.toLowerCase().includes(term) ||
      r.name.toLowerCase().includes(term) ||
      (r.crbim ?? '').toLowerCase().includes(term) ||
      (r.ifc?.entity ?? '').toLowerCase().includes(term),
  );
});
const coverage = computed(() => {
  const total = sandboxMappings.value.length;
  const mapped = total - unmapped.value.length;
  return total ? Math.round((mapped / total) * 1000) / 10 : 0;
});

async function loadProfiles() {
  loading.value = true;
  try {
    const data = await api.profiles();
    profiles.value = data.items as unknown as SemanticProfile[];
    const target =
      profiles.value.find((p) => p.profileId === selectedId.value) ??
      profiles.value.find((p) => p.isDefault) ??
      profiles.value[0];
    if (target) await selectProfile(target.profileId);
  } catch (error) {
    ui.notify({ type: 'error', title: '语义模板加载失败', message: String(error) });
  } finally {
    loading.value = false;
  }
}

async function selectProfile(profileId: string) {
  selectedId.value = profileId;
  try {
    profile.value = await api.profile(profileId);
    await runSandbox();
  } catch (error) {
    ui.notify({ type: 'error', title: '模板详情加载失败', message: String(error) });
  }
}

async function runSandbox() {
  if (!profile.value) return;
  sandboxLoading.value = true;
  try {
    const result = await api.previewSemantics({
      profileCode: `${profile.value.profileId}@${profile.value.version}`,
      shapeKeys: [],
    });
    sandboxMappings.value = result.mappings;
    sandboxProfile.value = result.profile;
    sandboxAt.value = result.resolvedAt;
  } catch (error) {
    ui.notify({ type: 'error', title: '推演失败', message: String(error) });
  } finally {
    sandboxLoading.value = false;
  }
}

async function previewComponent() {
  if (!profile.value) return;
  sandboxLoading.value = true;
  try {
    const result = await api.previewSemantics({
      profileCode: `${profile.value.profileId}@${profile.value.version}`,
      familyId: selectedFamily.value || undefined,
      shapeKeys: [],
    });
    sandboxMappings.value = result.mappings;
    sandboxProfile.value = result.profile;
    sandboxAt.value = result.resolvedAt;
    if (selectedFamily.value) {
      ui.notify({
        type: 'info',
        title: '权威推演完成',
        message: `${result.mappings.length} 个部位，${result.unmapped.length} 项 unmapped（由标准解析引擎执行）。`,
      });
    }
  } catch (error) {
    ui.notify({ type: 'error', title: '推演失败', message: String(error) });
  } finally {
    sandboxLoading.value = false;
  }
}

function openEdit(rule: SemanticProfileRule) {
  if (!isEditable.value) {
    ui.notify({
      type: 'warning',
      title: '系统公共模板只读',
      message: '系统级模板修改需资产管理员二次确认。请切换到企业专用覆盖模板编辑。',
    });
    return;
  }
  editing.value = JSON.parse(JSON.stringify(rule));
}

async function saveRule() {
  if (!profile.value || !editing.value) return;
  const nextRules = profile.value.rules.map((r) => (r.code === editing.value!.code ? editing.value! : r));
  if (profile.value.tenantId === 'system') {
    pendingRules.value = nextRules;
    systemConfirmOpen.value = true;
    return;
  }
  await commitRules(nextRules);
}

async function commitRules(rules: SemanticProfileRule[]) {
  if (!profile.value) return;
  saving.value = true;
  try {
    profile.value = await api.updateProfileRules(profile.value.profileId, rules);
    const index = profiles.value.findIndex((p) => p.profileId === profile.value!.profileId);
    if (index >= 0) profiles.value[index] = { ...profiles.value[index], ...profile.value };
    editing.value = null;
    systemConfirmOpen.value = false;
    await runSandbox();
    ui.notify({ type: 'success', title: '映射规则已保存', message: `解析规则已刷新，当前存在 ${profile.value.unmappedCount} 项 unmapped。` });
  } catch (error) {
    ui.notify({ type: 'error', title: '保存失败', message: String(error) });
  } finally {
    saving.value = false;
  }
}

function exportUnmapped() {
  const payload = {
    profile: sandboxProfile.value,
    generatedAt: new Date().toISOString(),
    unmapped: unmapped.value.map((m) => ({ code: m.code, name: m.name, shapeKey: m.shapeKey })),
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  triggerDownload(url, `unmapped_${profile.value?.profileId ?? 'profile'}.json`);
  setTimeout(() => URL.revokeObjectURL(url), 2000);
  ui.notify({ type: 'success', title: 'unmapped 清单已导出', message: `${unmapped.value.length} 项未映射语义码` });
}

watch(selectedId, () => {
  editing.value = null;
});

onMounted(async () => {
  await loadProfiles();
  try {
    const search = await api.search({ pageSize: 60 });
    components.value = search.items;
    selectedFamily.value = search.items[0]?.familyId ?? '';
  } catch {
    /* ignore */
  }
  if (String(route.query.panel ?? '') === 'sandbox') {
    activePanel.value = 'sandbox';
    sandboxFilter.value = String(route.query.filter ?? '') === 'unmapped' ? 'unmapped' : 'all';
  }
});

watch(
  () => route.query,
  (query) => {
    if (String(query.panel ?? '') !== 'sandbox') return;
    activePanel.value = 'sandbox';
    sandboxFilter.value = String(query.filter ?? '') === 'unmapped' ? 'unmapped' : 'all';
  },
);
</script>

<template>
  <div class="mx-auto max-w-[1500px] px-4 py-4 xl:px-6">
    <div class="flex flex-wrap items-end justify-between gap-3">
      <div>
        <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
          <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
          <AppIcon name="chevronRight" :size="10" />
          <span class="text-[#4B5563]">语义标准中枢</span>
        </div>
        <h1 class="mt-1 text-[20px] font-semibold tracking-tight text-ink">外部语义标准库中枢</h1>
        <p class="mt-0.5 max-w-3xl text-[12px] leading-relaxed text-muted">
          维护 CRBIM 53 表 / JTG / GB/T 51269 与 IFC 4.3 的映射规则，映射结果由工程标准服务集中统一计算。
        </p>
      </div>
      <div class="flex items-center gap-2">
        <span class="flex items-center gap-1 rounded-full border border-[#BBF7D0] bg-[#F0FDF4] px-2 py-1 text-[10px] text-success">
          <AppIcon name="shield" :size="11" />规则统一校验
        </span>
        <button class="btn btn-outline btn-sm" @click="exportUnmapped">
          <AppIcon name="download" :size="12" />导出 unmapped 清单
        </button>
      </div>
    </div>

    <div class="mt-3 grid gap-3 lg:grid-cols-[300px_minmax(0,1fr)]">
      <!-- 模板列表 -->
      <aside class="space-y-2">
        <div class="flex items-center justify-between">
          <p class="eyebrow">映射模板</p>
          <span class="mono text-[9px] text-[#9CA3AF]">{{ profiles.length }} 个</span>
        </div>
        <button
          v-for="item in profiles"
          :key="item.profileId"
          class="w-full rounded-md border p-3 text-left transition-all"
          :class="
            selectedId === item.profileId
              ? 'border-primary bg-[#F3F8FB] ring-2 ring-primary/10'
              : 'border-line bg-surface hover:border-[#C7D2DA] hover:bg-[#FAFBFC]'
          "
          @click="selectProfile(item.profileId)"
        >
          <div class="flex items-start justify-between gap-1.5">
            <p class="min-w-0 flex-1 text-[12px] font-semibold leading-snug text-ink">{{ item.name }}</p>
            <span
              class="shrink-0 rounded px-1.5 py-[1px] text-[8.5px]"
              :class="item.tenantId === 'system' ? 'bg-[#EFF6FF] text-[#1D4ED8]' : 'bg-[#E8EFF4] text-primary'"
            >
              {{ item.tenantId === 'system' ? '系统模板' : '租户覆盖' }}
            </span>
          </div>
          <p class="mt-1 font-mono text-[9px] text-muted">{{ item.profileId }}@{{ item.version }}</p>
          <p class="mt-1 line-clamp-2 text-[10px] leading-relaxed text-[#6B7280]">{{ item.description }}</p>
          <div class="mt-2 flex items-center justify-between text-[9.5px]">
            <span class="text-muted">{{ item.rules?.length ?? item.ruleCount ?? 0 }} 条规则</span>
            <span :class="item.unmappedCount ? 'text-warning' : 'text-success'">
              unmapped {{ item.unmappedCount }}
            </span>
          </div>
          <div class="mt-1.5 flex items-center gap-1.5">
            <span v-if="item.isDefault" class="rounded bg-[#ECFDF3] px-1 py-[1px] text-[8.5px] text-success">默认激活</span>
            <span class="rounded bg-[#F3F4F6] px-1 py-[1px] text-[8.5px] text-muted">{{ item.boundProjects }} 个项目</span>
            <span class="ml-auto text-[8.5px] text-[#9CA3AF]">{{ formatDate(item.updatedAt) }}</span>
          </div>
        </button>
      </aside>

      <!-- 规则 / 沙盒 -->
      <section class="panel min-w-0 overflow-hidden">
        <div class="flex items-center gap-1 border-b border-line bg-[#FAFBFC] px-2">
          <button
            class="flex items-center gap-1.5 px-3 py-2.5 text-[12px] transition-colors"
            :class="activePanel === 'rules' ? 'font-semibold text-primary' : 'text-[#6B7280] hover:text-ink'"
            @click="activePanel = 'rules'"
          >
            <AppIcon name="layers" :size="13" />映射规则
            <span class="mono rounded bg-[#EEF1F4] px-1 text-[9px]">{{ profile?.rules.length ?? 0 }}</span>
          </button>
          <button
            class="flex items-center gap-1.5 px-3 py-2.5 text-[12px] transition-colors"
            :class="activePanel === 'sandbox' ? 'font-semibold text-primary' : 'text-[#6B7280] hover:text-ink'"
            @click="activePanel = 'sandbox'"
          >
            <AppIcon name="terminal" :size="13" />在线推演沙盒
            <span v-if="unmapped.length" class="mono rounded bg-[#FFFBEB] px-1 text-[9px] text-warning">{{ unmapped.length }}</span>
          </button>
          <div class="flex-1" />
          <span class="hidden items-center gap-1 pr-2 text-[9px] text-muted sm:flex">
            <span class="h-1.5 w-1.5 rounded-full bg-success" />标准解析引擎
          </span>
        </div>

        <div v-if="loading" class="flex items-center justify-center py-20">
          <span class="h-5 w-5 animate-spin rounded-full border-2 border-[#CBD5E1] border-t-primary" />
        </div>

        <template v-else-if="profile">
          <div class="grid grid-cols-2 gap-2 border-b border-[#F0F2F5] p-3 sm:grid-cols-4">
            <div class="panel-inset px-2.5 py-2">
              <p class="text-[9px] text-muted">规则总数</p>
              <p class="mono mt-0.5 text-[16px] font-semibold text-ink">{{ profile.rules.length }}</p>
            </div>
            <div class="panel-inset px-2.5 py-2">
              <p class="text-[9px] text-muted">未映射语义码</p>
              <p class="mono mt-0.5 text-[16px] font-semibold" :class="profile.unmappedCount ? 'text-warning' : 'text-success'">
                {{ profile.unmappedCount }}
              </p>
            </div>
            <div class="panel-inset px-2.5 py-2">
              <p class="text-[9px] text-muted">覆盖率</p>
              <p class="mono mt-0.5 text-[16px] font-semibold text-ink">{{ coverage }}%</p>
            </div>
            <div class="panel-inset px-2.5 py-2">
              <p class="text-[9px] text-muted">绑定项目</p>
              <p class="mono mt-0.5 text-[16px] font-semibold text-ink">{{ profile.boundProjects }}</p>
            </div>
          </div>

          <!-- 规则表 -->
          <div v-if="activePanel === 'rules'">
            <div class="flex flex-wrap items-center justify-between gap-2 border-b border-[#F0F2F5] px-3 py-2">
              <div class="flex items-center gap-1.5 rounded border border-line bg-[#FAFBFC] px-2 py-1">
                <AppIcon name="search" :size="12" class="text-[#9CA3AF]" />
                <input v-model="ruleFilter" class="w-[220px] border-0 bg-transparent text-[11px] focus:outline-none" placeholder="检索语义码 / IFC 实体…" />
              </div>
              <div class="flex items-center gap-2">
                <span class="text-[9.5px] text-muted">三层优先级：构件内覆盖 → 项目模板 → hint 回退</span>
                <span v-if="!isEditable" class="flex items-center gap-1 rounded bg-[#F3F4F6] px-1.5 py-[1px] text-[9px] text-muted">
                  <AppIcon name="lock" :size="10" />只读
                </span>
              </div>
            </div>
            <div class="max-h-[540px] overflow-auto scroll-thin">
              <table class="table-dense min-w-[1180px]">
                <thead class="sticky top-0 z-10">
                  <tr>
                    <th>语义码 (code)</th>
                    <th>名称</th>
                    <th>CRBIM 53 表</th>
                    <th>GB/T 51269</th>
                    <th>IFC 实体</th>
                    <th>IfcShapeAspect</th>
                    <th>算量集</th>
                    <th class="w-[60px]"></th>
                  </tr>
                </thead>
                <tbody>
                  <tr v-for="rule in filteredRules" :key="rule.code">
                    <td class="mono whitespace-nowrap text-[12px] font-medium text-ink">{{ rule.code }}</td>
                    <td class="min-w-[130px] text-[12px] text-[#374151]">{{ rule.name }}</td>
                    <td class="whitespace-nowrap text-[11.5px] text-[#374151]">{{ rule.crbim }}</td>
                    <td class="whitespace-nowrap text-[11.5px] text-[#374151]">{{ rule.gb51269 ?? '—' }}</td>
                    <td class="mono whitespace-nowrap text-[11.5px] font-medium text-[#1D4ED8]">{{ rule.ifc.entity }}</td>
                    <td class="mono whitespace-nowrap text-[11px] text-[#374151]">{{ rule.ifc.shapeAspect }}</td>
                    <td class="mono whitespace-nowrap text-[11px] text-muted">{{ rule.qto ?? '—' }}</td>
                    <td class="text-right">
                      <button
                        class="btn btn-ghost btn-sm"
                        :class="isEditable ? 'text-primary' : 'text-[#9CA3AF]'"
                        title="编辑映射"
                        @click="openEdit(rule)"
                      >
                        <AppIcon :name="isEditable ? 'settings' : 'lock'" :size="12" />
                      </button>
                    </td>
                  </tr>
                  <tr v-if="!filteredRules.length">
                    <td colspan="8" class="py-8 text-center text-[12px] text-muted">未找到匹配规则</td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>

          <!-- 沙盒 -->
          <div v-else class="p-3">
            <div class="rounded-md border border-line bg-[#FAFBFC] p-3">
              <div class="flex flex-wrap items-end gap-2">
                <div class="min-w-[220px] flex-1">
                  <label class="label">选择构件进行权威推演</label>
                  <select v-model="selectedFamily" class="select !py-1.5 !text-[11.5px]">
                    <option v-for="item in components" :key="item.familyId" :value="item.familyId">
                      {{ item.name }} · {{ item.familyId }}
                    </option>
                  </select>
                </div>
                <button class="btn btn-primary btn-sm" :disabled="sandboxLoading" @click="previewComponent">
                  <span v-if="sandboxLoading" class="h-3 w-3 animate-spin rounded-full border-2 border-white/30 border-t-white" />
                  <AppIcon v-else name="play" :size="12" />执行语义推演
                </button>
                <button class="btn btn-outline btn-sm" :disabled="sandboxLoading" @click="runSandbox">
                  <AppIcon name="refresh" :size="12" />全量语义码扫描
                </button>
              </div>
              <div class="mt-2 flex flex-wrap items-center gap-2 text-[9.5px] text-muted">
                <span v-if="sandboxAt" class="mono">解析时间 {{ sandboxAt.slice(11, 19) }}</span>
                <span class="ml-auto rounded bg-[#ECFDF3] px-1.5 py-[2px] text-success">规则一致性已校验</span>
              </div>
            </div>

            <div class="mt-3 grid gap-3 lg:grid-cols-[1.4fr_1fr]">
              <div class="rounded-md border border-line">
                <div class="flex items-center justify-between border-b border-[#F0F2F5] px-3 py-2">
                  <p class="text-[11px] font-semibold text-ink">部位映射结果</p>
                  <div class="flex items-center gap-2">
                    <button
                      class="text-[9.5px]"
                      :class="sandboxFilter === 'all' ? 'font-semibold text-primary' : 'text-muted'"
                      @click="sandboxFilter = 'all'"
                    >全部</button>
                    <button
                      class="text-[9.5px]"
                      :class="sandboxFilter === 'unmapped' ? 'font-semibold text-warning' : 'text-muted'"
                      @click="sandboxFilter = 'unmapped'"
                    >仅未映射</button>
                    <span class="mono text-[9px] text-muted">{{ displaySandboxMappings.length }} 项</span>
                  </div>
                </div>
                <div class="max-h-[360px] overflow-auto scroll-thin">
                  <table class="table-dense">
                    <thead class="sticky top-0">
                      <tr>
                        <th>shapeKey / code</th>
                        <th>IFC 4.3</th>
                        <th>CRBIM</th>
                        <th>状态</th>
                      </tr>
                    </thead>
                    <tbody>
                      <tr v-for="mapping in displaySandboxMappings" :key="mapping.shapeKey" :class="mapping.status === 'unmapped' ? 'bg-[#FFFBEB]' : ''">
                        <td>
                          <p class="mono text-[12px] font-medium text-ink">{{ mapping.shapeKey }}</p>
                          <p class="mono text-[11px] text-[#9CA3AF]">{{ mapping.code }}</p>
                        </td>
                        <td class="mono text-[11.5px] text-[#374151]">{{ mapping.ifc?.entity ?? '—' }}</td>
                        <td class="text-[11.5px] text-[#374151]">{{ mapping.crbim ?? '—' }}</td>
                        <td>
                          <StatusBadge
                            :tone="mapping.status === 'unmapped' ? 'warning' : 'success'"
                            :label="mapping.status === 'unmapped' ? 'unmapped' : mapping.layer === 'local' ? '局部覆盖' : 'mapped'"
                          />
                        </td>
                      </tr>
                      <tr v-if="!displaySandboxMappings.length">
                        <td colspan="4" class="py-6 text-center text-[12px] text-muted">当前筛选无映射记录</td>
                      </tr>
                    </tbody>
                  </table>
                </div>
              </div>

              <div class="rounded-md border border-[#FDE68A] bg-[#FFFBEB]">
                <div class="flex items-center justify-between border-b border-[#FDE68A] px-3 py-2">
                  <p class="flex items-center gap-1.5 text-[11px] font-semibold text-[#92400E]">
                    <AppIcon name="alertTriangle" :size="12" />unmapped 预警
                  </p>
                  <span class="mono text-[9px] text-[#92400E]">{{ unmapped.length }} 项</span>
                </div>
                <div class="max-h-[360px] overflow-auto p-2 scroll-thin">
                  <div v-if="!unmapped.length" class="py-6 text-center">
                    <AppIcon name="checkCircle" :size="20" class="mx-auto text-success" />
                    <p class="mt-1 text-[10.5px] text-[#166534]">全部语义码均已映射</p>
                  </div>
                  <div v-for="item in unmapped" :key="item.code" class="mb-1 rounded border border-[#FDE68A] bg-white/70 px-2 py-1.5">
                    <p class="mono text-[9.5px] font-medium text-[#92400E]">{{ item.code }}</p>
                    <p class="mt-0.5 text-[9px] text-[#B45309]">{{ item.name }}</p>
                  </div>
                  <p class="mt-2 text-[9px] leading-relaxed text-[#92400E]/80">
                    未映射部位保留基础几何，导出 IFC 时列入报告并告警，不阻断交付。
                  </p>
                </div>
              </div>
            </div>
          </div>
        </template>
      </section>
    </div>

    <!-- 规则编辑 -->
    <ModalShell
      :open="!!editing"
      title="编辑语义映射规则"
      :subtitle="editing ? `${editing.code} · ${profile?.profileId}@${profile?.version}` : ''"
      icon="settings"
      width="md"
      @close="editing = null"
    >
      <div v-if="editing" class="grid gap-3 p-4 sm:grid-cols-2">
        <div class="sm:col-span-2">
          <label class="label">语义码 (code)</label>
          <input v-model="editing.code" class="input mono !text-[12.5px]" readonly />
        </div>
        <div>
          <label class="label">部位名称</label>
          <input v-model="editing.name" class="input" />
        </div>
        <div>
          <label class="label">CRBIM 53 表映射</label>
          <input v-model="editing.crbim" class="input" />
        </div>
        <div>
          <label class="label">GB/T 51269 映射</label>
          <input v-model="editing.gb51269" class="input" />
        </div>
        <div>
          <label class="label">IFC 实体</label>
          <input v-model="editing.ifc.entity" class="input mono !text-[12.5px]" placeholder="IfcColumn" />
        </div>
        <div>
          <label class="label">IfcShapeAspect</label>
          <input v-model="editing.ifc.shapeAspect" class="input mono !text-[12.5px]" placeholder="IfcColumnBody#PierBodySolid" />
        </div>
        <div>
          <label class="label">算量集 (Qto)</label>
          <input v-model="editing.qto" class="input mono !text-[12.5px]" placeholder="Qto_ColumnBaseQuantities" />
        </div>
        <p class="sm:col-span-2 rounded border border-[#FDE68A] bg-[#FFFBEB] px-2 py-1.5 text-[11px] leading-relaxed text-[#92400E]">
          修改将立即影响该模板下所有构件的语义呈现与 IFC 导出，不要求构件工程师重新打包 .cbfc。
        </p>
      </div>
      <template #footer>
        <div class="flex items-center justify-between">
          <p class="text-[11px] text-muted">保存后立即生效</p>
          <div class="flex items-center gap-2">
            <button class="btn btn-ghost btn-sm" @click="editing = null">取消</button>
            <button class="btn btn-primary btn-sm" :disabled="saving" @click="saveRule">
              <AppIcon name="check" :size="12" />保存规则
            </button>
          </div>
        </div>
      </template>
    </ModalShell>

    <ModalShell
      :open="systemConfirmOpen"
      title="系统级模板二次确认"
      subtitle="系统公共标准模板影响全租户，需资产管理员确认"
      icon="alertTriangle"
      width="sm"
      @close="systemConfirmOpen = false"
    >
      <div class="p-4">
        <p class="text-[11.5px] leading-relaxed text-[#4B5563]">
          您正在修改系统级模板 <span class="mono font-semibold">{{ profile?.profileId }}@{{ profile?.version }}</span>。
          该变更将在全公司范围内生效，并写入审计日志。建议先在租户覆盖模板中验证。
        </p>
      </div>
      <template #footer>
        <div class="flex items-center justify-end gap-2">
          <button class="btn btn-ghost btn-sm" @click="systemConfirmOpen = false">取消</button>
          <button class="btn btn-danger btn-sm" :disabled="saving" @click="pendingRules && commitRules(pendingRules)">
            确认全公司生效
          </button>
        </div>
      </template>
    </ModalShell>
  </div>
</template>
