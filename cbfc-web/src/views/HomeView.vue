<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { useRouter } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import ExportFlowModal from '@/components/export/ExportFlowModal.vue';
import { api } from '@/api/client';
import type { ComponentDetail, ComponentSummary, SemanticProfile } from '@/api/types';
import { useSessionStore } from '@/stores/session';
import { useUiStore } from '@/stores/ui';
import {
  formatDate,
  formatNumber,
  formatVolume,
  sourceModeMeta,
} from '@/utils/format';

const router = useRouter();
const session = useSessionStore();
const ui = useUiStore();

const keyword = ref('');
const favorites = ref<ComponentSummary[]>([]);
const history = ref<{ id: string; familyId: string; name: string; action: string; format?: string; at: string; detail?: string }[]>([]);
const reviewCount = ref(0);
const totalAssets = ref(0);
const publishedCount = ref(0);
const profile = ref<SemanticProfile | null>(null);
const loading = ref(true);
const recentTab = ref<'history' | 'favorites'>('history');
const exportOpen = ref(false);
const exportComponent = ref<ComponentDetail | null>(null);
const autoHandoff = ref(false);
const loadingComponent = ref(false);

const quickCards = computed(() => {
  const cards = [
    {
      key: 'library',
      icon: 'grid',
      title: '检索浏览构件库',
      desc: '分类树 + 名称编码 + 语义码检索',
      metric: `${totalAssets.value} 个构件`,
      sub: `已发布 ${publishedCount.value} 个`,
      to: '/library',
      tone: 'primary',
    },
  ];
  if (session.canReview) {
    cards.push({
      key: 'reviews',
      icon: 'shield',
      title: '待我审核',
      desc: '双模型差异与结构化签批',
      metric: `${reviewCount.value} 项待办`,
      sub: reviewCount.value ? '加急任务优先' : '队列已清空',
      to: '/reviews',
      tone: 'warning',
    });
  } else if (session.canIngest) {
    cards.push({
      key: 'ingestion',
      icon: 'upload',
      title: '构件入库',
      desc: '上传 .cbfc，完成 6 道门禁',
      metric: 'GATE-01→06',
      sub: '门禁通过后提交审核',
      to: '/ingestion',
      tone: 'warning',
    });
  } else {
    cards.push({
      key: 'favorites',
      icon: 'star',
      title: '个人收藏',
      desc: '快速回到已收藏构件',
      metric: `${favorites.value.length} 项`,
      sub: '仅显示当前账号可见资产',
      to: '/favorites',
      tone: 'muted',
    });
  }
  return cards;
});
const recentItems = computed(() => {
  if (recentTab.value === 'favorites') {
    return favorites.value.slice(0, 4).map((item) => ({
      familyId: item.familyId,
      name: item.name,
      drawingCode: item.drawingCode,
      volumeM3: item.geometry.volumeM3,
      status: item.status,
      sourceMode: item.sourceMode,
      at: item.updatedAt,
      detail: '个人收藏',
    }));
  }
  const seen = new Set<string>();
  return history.value
    .filter((item) => item.familyId !== 'session')
    .filter((item) => {
      if (seen.has(item.familyId)) return false;
      seen.add(item.familyId);
      return true;
    })
    .slice(0, 4)
    .map((item) => ({
      familyId: item.familyId,
      name: item.name,
      drawingCode: item.familyId,
      volumeM3: null as number | null,
      status: 'published',
      sourceMode: 'parametric',
      at: item.at,
      detail: `${historyActionLabel(item.action)}${item.detail ? ' · ' + item.detail : ''}`,
    }));
});

const recentHistory = computed(() => {
  const seen = new Set<string>();
  return history.value
    .filter((item) => item.familyId !== 'session')
    .filter((item) => {
      if (seen.has(item.familyId)) return false;
      seen.add(item.familyId);
      return true;
    })
    .slice(0, 3);
});

function historyActionLabel(action: string) {
  const map: Record<string, string> = {
    download: '下载',
    evaluate: '在线变参',
    handoff: '深链推送',
    export: '导出',
    open: '查看详情',
  };
  return map[action] ?? action;
}

async function load() {
  loading.value = true;
  try {
    const [search, favoriteResponse, historyResponse, reviewResponse, profileResponse] = await Promise.all([
      api.search({ pageSize: 6, sort: 'popular' }),
      api.favorites(),
      api.history(),
      api.reviews().catch(() => ({ items: [], total: 0 })),
      api.profiles().catch(() => null),
    ]);
    totalAssets.value = search.total;
    publishedCount.value = search.facets.statuses.find((s) => s.value === 'published')?.count ?? 0;
    favorites.value = favoriteResponse.items;
    history.value = historyResponse.items;
    reviewCount.value = reviewResponse.items.length;
    profile.value = profileResponse?.active ?? null;
  } catch (error) {
    ui.notify({ type: 'error', title: '工作台数据加载失败', message: String(error) });
  } finally {
    loading.value = false;
  }
}

function submitSearch() {
  const term = keyword.value.trim();
  if (term) ui.rememberSearch(term);
  router.push({ path: '/library', query: term ? { q: term } : {} });
}

function openRecent(item: { familyId: string }) {
  router.push(`/components/${item.familyId}`);
}

async function openDesktop(item: { familyId: string }) {
  loadingComponent.value = true;
  try {
    const detail = await api.component(item.familyId);
    exportComponent.value = detail;
    autoHandoff.value = true;
    exportOpen.value = true;
  } catch (error) {
    ui.notify({ type: 'error', title: '构件加载失败', message: String(error) });
  } finally {
    loadingComponent.value = false;
  }
}

onMounted(load);
</script>

<template>
  <div class="mx-auto max-w-[1400px] px-4 py-4 xl:px-6">
    <!-- 工作台：搜索与快捷入口 -->
    <section class="panel px-4 py-3.5">
      <div class="flex flex-wrap items-center justify-between gap-2">
        <div>
          <p class="eyebrow">工作台</p>
          <h1 class="mt-0.5 text-[18px] font-semibold tracking-tight text-ink">构件检索与任务入口</h1>
          <p class="mt-0.5 text-[11px] text-muted">检索构件、查看待办，或直接进入入库与审核流程。</p>
        </div>
        <button class="btn btn-outline btn-sm" @click="ui.openCommand()">
          <AppIcon name="command" :size="12" />全局命令
          <span class="kbd ml-1">Ctrl K</span>
        </button>
      </div>

      <div class="mt-3 flex items-center gap-2 rounded-md border border-line bg-[#FAFBFC] p-1.5">
        <AppIcon name="search" :size="17" class="ml-1.5 text-[#94A3B8]" />
        <input
          v-model="keyword"
          class="min-w-0 flex-1 border-0 bg-transparent px-1 text-[13px] text-ink placeholder:text-[#9CA3AF] focus:outline-none"
          placeholder="检索构件名称、图号、语义码（code）或行业分类…"
          @keydown.enter="submitSearch"
        />
        <button class="btn btn-primary btn-sm !px-3.5" @click="submitSearch">
          检索<AppIcon name="arrowRight" :size="13" />
        </button>
      </div>

      <div class="mt-2 flex flex-wrap items-center gap-1.5">
        <span class="text-[10px] text-muted">热门分类</span>
        <button
          v-for="chip in ['桥墩', '箱梁', '支座', '承台', '隧道衬砌']"
          :key="chip"
          class="chip"
          @click="keyword = chip; submitSearch()"
        >
          {{ chip }}
        </button>
      </div>
    </section>

    <!-- 角色快捷入口 -->
    <div class="mt-3 grid gap-3 sm:grid-cols-2">
      <button
        v-for="card in quickCards"
        :key="card.key"
        class="panel group flex items-start gap-3 p-3.5 text-left card-hover"
        @click="router.push(card.to)"
      >
        <span
          class="flex h-9 w-9 shrink-0 items-center justify-center rounded-[6px]"
          :class="
            card.tone === 'primary'
              ? 'bg-[#E8EFF4] text-primary'
              : card.tone === 'warning'
                ? 'bg-[#FFFBEB] text-warning'
                : card.tone === 'info'
                  ? 'bg-[#EFF6FF] text-[#1D4ED8]'
                  : 'bg-[#F3F4F6] text-[#4B5563]'
          "
        >
          <AppIcon :name="card.icon" :size="17" />
        </span>
        <span class="min-w-0 flex-1">
          <span class="flex items-center justify-between gap-2">
            <span class="truncate text-[12.5px] font-semibold text-ink">{{ card.title }}</span>
            <AppIcon name="arrowRight" :size="13" class="text-[#CBD5E1] transition-transform group-hover:translate-x-0.5 group-hover:text-primary" />
          </span>
          <span class="mt-0.5 block text-[10.5px] text-muted">{{ card.desc }}</span>
          <span class="mt-2 flex items-baseline gap-2">
            <span class="mono text-[13px] font-semibold text-ink">{{ card.metric }}</span>
            <span class="truncate text-[9.5px] text-[#9CA3AF]">{{ card.sub }}</span>
          </span>
        </span>
      </button>
    </div>

    <div class="mt-3 grid gap-3 lg:grid-cols-[1.55fr_1fr]">
      <!-- 最近 / 收藏 -->
      <section class="panel overflow-hidden">
        <div class="flex items-center justify-between border-b border-line px-3 py-2">
          <div class="flex items-center gap-1">
            <button
              class="rounded px-2 py-1 text-[11.5px] transition-colors"
              :class="recentTab === 'history' ? 'bg-[#E8EFF4] font-semibold text-primary' : 'text-muted hover:text-ink'"
              @click="recentTab = 'history'"
            >
              最近使用
            </button>
            <button
              class="rounded px-2 py-1 text-[11.5px] transition-colors"
              :class="recentTab === 'favorites' ? 'bg-[#E8EFF4] font-semibold text-primary' : 'text-muted hover:text-ink'"
              @click="recentTab = 'favorites'"
            >
              我的收藏
              <span class="mono ml-0.5 text-[9px] text-[#9CA3AF]">{{ favorites.length }}</span>
            </button>
          </div>
          <RouterLink to="/history" class="flex items-center gap-1 text-[10px] text-primary hover:text-primary-hover">
            查看全部<AppIcon name="arrowRight" :size="10" />
          </RouterLink>
        </div>

        <div v-if="loading" class="grid gap-2 p-3 sm:grid-cols-2">
          <div v-for="i in 4" :key="i" class="skeleton h-[76px] w-full" />
        </div>
        <div v-else-if="recentItems.length" class="grid gap-2 p-3 sm:grid-cols-2">
          <div
            v-for="item in recentItems"
            :key="`${item.familyId}-${item.at}`"
            class="group flex items-center gap-2.5 rounded-md border border-line bg-white p-2 transition-colors hover:border-[#C7D2DA] hover:bg-[#FAFBFC]"
          >
            <button class="h-[54px] w-[76px] shrink-0 overflow-hidden rounded border border-[#EDF0F3] bg-[#F7F9FB]" @click="openRecent(item)">
              <img :src="api.previewUrl(item.familyId, 152, 108)" alt="" class="h-full w-full object-contain" loading="lazy" />
            </button>
            <div class="min-w-0 flex-1">
              <button class="block w-full truncate text-left text-[11.5px] font-medium text-ink hover:text-primary" @click="openRecent(item)">
                {{ item.name }}
              </button>
              <p class="mt-0.5 truncate text-[9px] text-muted">{{ item.detail }}</p>
              <p class="mt-1 flex items-center gap-1.5">
                <span v-if="item.volumeM3" class="mono text-[9.5px] text-[#374151]">{{ formatVolume(item.volumeM3) }}</span>
                <span class="text-[8.5px] text-[#9CA3AF]">{{ formatDate(item.at) }}</span>
              </p>
            </div>
            <button
              class="btn btn-ghost btn-icon h-6 w-6 opacity-0 transition-opacity group-hover:opacity-100"
              title="在桌面端打开"
              :disabled="loadingComponent"
              @click="openDesktop(item)"
            >
              <AppIcon name="desktop" :size="13" />
            </button>
          </div>
        </div>
        <div v-else class="px-4 py-10 text-center">
          <AppIcon name="folder" :size="22" class="mx-auto text-[#CBD5E1]" />
          <p class="mt-2 text-[11.5px] text-muted">暂无{{ recentTab === 'history' ? '使用' : '收藏' }}记录</p>
          <RouterLink to="/library" class="mt-2 inline-flex text-[11px] font-medium text-primary">去构件大厅看看 →</RouterLink>
        </div>
      </section>

      <!-- 健康度 + 最近下载 -->
      <section class="space-y-3">
        <div class="panel flex flex-wrap items-center gap-2 px-3 py-2.5">
          <AppIcon name="layers" :size="13" class="text-primary" />
          <span class="text-[11px] font-medium text-ink">标准映射</span>
          <span class="font-mono text-[10px] text-muted">
            {{ profile ? `${profile.profileId}@${profile.version}` : 'Railway-CRBIM-IFC4X3@1.4' }}
          </span>
          <StatusBadge
            :tone="profile && profile.unmappedCount > 0 ? 'warning' : 'success'"
            :label="profile && profile.unmappedCount > 0 ? `${profile.unmappedCount} 项未映射` : '全部映射'"
          />
          <RouterLink
            :to="session.isInternal ? '/semantics?panel=sandbox&filter=unmapped' : '/library'"
            class="ml-auto flex items-center gap-1 text-[10px] font-medium text-primary hover:text-primary-hover"
          >
            查看<AppIcon name="arrowRight" :size="10" />
          </RouterLink>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <p class="eyebrow">最近下载 / 导出</p>
            <RouterLink to="/history" class="text-[10px] text-primary">全部</RouterLink>
          </div>
          <div class="mt-2 space-y-1">
            <div
              v-for="entry in recentHistory"
              :key="entry.id"
              class="flex items-center gap-2 rounded px-1 py-1 hover:bg-[#FAFBFC]"
            >
              <AppIcon
                :name="entry.action === 'download' ? 'download' : entry.action === 'handoff' ? 'desktop' : entry.action === 'export' ? 'globe' : 'sliders'"
                :size="12"
                class="text-[#94A3B8]"
              />
              <span class="min-w-0 flex-1 truncate text-[10.5px] text-[#374151]">{{ entry.name }}</span>
              <span class="mono shrink-0 rounded bg-[#F1F4F7] px-1 text-[8.5px] text-[#6B7280]">{{ entry.format ?? historyActionLabel(entry.action) }}</span>
              <span class="shrink-0 text-[8.5px] text-[#9CA3AF]">{{ formatDate(entry.at) }}</span>
            </div>
            <div v-if="!history.length" class="py-3 text-center text-[10px] text-muted">暂无下载记录</div>
          </div>
        </div>

        <div class="panel overflow-hidden">
          <div class="flex items-center gap-2 border-b border-line bg-[#FAFBFC] px-3 py-2">
            <AppIcon name="route" :size="12" class="text-primary" />
            <p class="text-[10.5px] font-semibold text-ink">主操作路径</p>
            <span class="ml-auto font-mono text-[8.5px] text-[#9CA3AF]">4 STEPS</span>
          </div>
          <div class="flex items-center justify-between px-3 py-2.5">
            <span v-for="(step, index) in ['搜索/分类', '3D 预览', '规格确认', '下载/插入']" :key="step" class="flex items-center gap-1">
              <span class="flex h-4 w-4 items-center justify-center rounded-full bg-[#E8EFF4] font-mono text-[8px] text-primary">{{ index + 1 }}</span>
              <span class="text-[9.5px] text-[#4B5563]">{{ step }}</span>
              <AppIcon v-if="index < 3" name="chevronRight" :size="9" class="text-[#CBD5E1]" />
            </span>
          </div>
        </div>
      </section>
    </div>

    <div class="mt-4 flex flex-wrap items-center justify-between gap-2 px-1 text-[9.5px] text-[#9CA3AF]">
      <p>
        当前会话：{{ session.user?.name }} · {{ session.user?.roleLabel }} ·
        {{ session.user?.tenantName }}
      </p>
      <p class="font-mono">CBF-C Component Foundation · 规范 CBF-C v1.4.2</p>
    </div>

    <ExportFlowModal
      :open="exportOpen"
      :component="exportComponent"
      initial-format="cbfc"
      :auto-handoff="autoHandoff"
      @close="exportOpen = false"
    />
  </div>
</template>
