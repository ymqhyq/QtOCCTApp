<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useRoute } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppIcon from '@/components/ui/AppIcon.vue';
import { api } from '@/api/client';
import { useSessionStore } from '@/stores/session';
import { SIDEBAR_PRIMARY, SIDEBAR_TOOLS, visibleNav } from '@/navigation';
import { useUiStore } from '@/stores/ui';
import { useJobsStore } from '@/stores/jobs';
import { formatDate } from '@/utils/format';

const route = useRoute();
const session = useSessionStore();
const ui = useUiStore();
const jobs = useJobsStore();
const { sidebarCollapsed: collapsed, mobileSidebarOpen } = storeToRefs(ui);

const totalComponents = ref(0);
const favoriteCount = ref(0);
const health = ref<{ unmapped: number; version: string; updatedAt: string } | null>(null);

const SIDEBAR_MIN = 184;
const SIDEBAR_MAX = 300;
const SIDEBAR_KEY = 'rcif.sidebarWidth';
function clampSidebar(value: number) {
  return Math.min(SIDEBAR_MAX, Math.max(SIDEBAR_MIN, Math.round(value)));
}
function initialSidebarWidth() {
  const stored = Number(window.localStorage.getItem(SIDEBAR_KEY));
  return clampSidebar(stored || 208);
}
const sidebarWidth = ref(initialSidebarWidth());
watch(sidebarWidth, (value) => window.localStorage.setItem(SIDEBAR_KEY, String(clampSidebar(value))));

let stopResize: (() => void) | null = null;
function startSidebarResize(event: PointerEvent) {
  if (collapsed.value) return;
  const startX = event.clientX;
  const startWidth = sidebarWidth.value;
  const move = (e: PointerEvent) => {
    sidebarWidth.value = clampSidebar(startWidth + e.clientX - startX);
  };
  const up = () => {
    window.removeEventListener('pointermove', move);
    window.removeEventListener('pointerup', up);
    stopResize = null;
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
  };
  stopResize = up;
  document.body.style.cursor = 'col-resize';
  document.body.style.userSelect = 'none';
  window.addEventListener('pointermove', move);
  window.addEventListener('pointerup', up);
}
onBeforeUnmount(() => stopResize?.());

const primaryNav = computed(() =>
  visibleNav(session, SIDEBAR_PRIMARY).map((item) => ({
    ...item,
    active:
      item.to === '/library'
        ? route.path === '/library' || route.path.startsWith('/components')
        : route.path === item.to,
    count:
      item.to === '/library'
        ? totalComponents.value
        : item.to === '/favorites'
          ? favoriteCount.value
          : undefined,
  })),
);

const secondaryNav = computed(() =>
  visibleNav(session, SIDEBAR_TOOLS).map((item) => ({
    ...item,
    badge: item.to === '/reviews' ? '待办' : undefined,
  })),
);

onMounted(async () => {
  try {
    const [search, favorites, profiles] = await Promise.all([
      api.search({ pageSize: 1 }),
      api.favorites().catch(() => ({ items: [], total: 0 })),
      api.profiles().catch(() => null),
    ]);
    totalComponents.value = search.total;
    favoriteCount.value = favorites.total;
    if (profiles?.active) {
      health.value = {
        unmapped: profiles.active.unmappedCount,
        version: `${profiles.active.profileId.split('-')[0]}@${profiles.active.version}`,
        updatedAt: profiles.active.updatedAt,
      };
    }
  } catch {
    /* sidebar 数据失败不阻断主流程 */
  }
  void jobs.refresh();
});
</script>

<template>
  <!-- 移动端遮罩 -->
  <div
    v-if="mobileSidebarOpen"
    class="fixed inset-0 z-30 bg-[#0B1220]/40 lg:hidden"
    @click="ui.mobileSidebarOpen = false"
  />

  <aside
    class="relative flex h-full shrink-0 flex-col border-r border-line bg-surface transition-[width,transform] duration-200 ease-industrial"
    :style="{ '--nav-width': `${sidebarWidth}px` }"
    :class="[
      collapsed ? 'w-[60px]' : 'w-[var(--nav-width)]',
      mobileSidebarOpen ? 'fixed bottom-0 left-0 top-14 z-40 !w-[240px] shadow-drawer' : 'hidden lg:flex',
    ]"
  >
    <div
      v-if="!collapsed"
      class="absolute -right-1 top-0 z-20 hidden h-full w-2 cursor-col-resize touch-none lg:block"
      role="separator"
      aria-orientation="vertical"
      aria-label="调整导航栏宽度"
      title="拖动调整导航栏宽度"
      @pointerdown.prevent="startSidebarResize"
    >
      <span class="absolute left-1/2 top-1/2 h-10 w-[2px] -translate-x-1/2 -translate-y-1/2 rounded-full bg-[#C9D5DE] opacity-60 transition-all hover:h-14 hover:bg-primary hover:opacity-100" />
    </div>
    <div class="flex items-center gap-1.5 px-2.5 pb-1 pt-3">
      <span v-if="!collapsed" class="eyebrow px-1">资产导航</span>
      <span v-else class="mx-auto h-1 w-1 rounded-full bg-line" />
    </div>

    <nav class="flex flex-col gap-0.5 px-2" aria-label="资产导航">
      <RouterLink
        v-for="item in primaryNav"
        :key="item.to"
        :to="item.to"
        class="nav-link"
        :class="[item.active ? 'nav-link-active' : '', collapsed ? 'justify-center px-0' : '']"
        :title="collapsed ? item.label : undefined"
        @click="ui.mobileSidebarOpen = false"
      >
        <AppIcon :name="item.icon" :size="16" :stroke-width="item.active ? 1.9 : 1.6" />
        <span v-if="!collapsed" class="flex-1 truncate">{{ item.label }}</span>
        <span
          v-if="!collapsed && item.count !== undefined"
          class="mono rounded bg-[#F1F4F7] px-1.5 py-[1px] text-[10px] text-[#6B7280]"
        >{{ item.count }}</span>
      </RouterLink>
    </nav>

    <div v-if="!collapsed" class="mt-2 border-t border-[#F0F2F5] px-2 pb-1 pt-2">
      <p class="mb-1 px-1 eyebrow">专业工具</p>
      <RouterLink
        v-for="item in secondaryNav"
        :key="item.to"
        :to="item.to"
        class="nav-link py-1.5"
        :class="route.path === item.to ? 'nav-link-active' : ''"
        @click="ui.mobileSidebarOpen = false"
      >
        <AppIcon :name="item.icon" :size="15" />
        <span class="flex-1 truncate">{{ item.label }}</span>
        <span v-if="item.badge" class="rounded bg-[#FFF7ED] px-1 py-[1px] text-[9px] font-semibold text-warning">
          {{ item.badge }}
        </span>
      </RouterLink>
    </div>

    <div v-if="!collapsed && health && session.canManageSemantics" class="mx-2 mb-2 mt-1 rounded-[5px] border border-line bg-[#FAFBFC] p-2.5">
      <div class="flex items-center justify-between">
        <span class="text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">映射健康度</span>
        <span class="h-1.5 w-1.5 rounded-full" :class="health.unmapped > 0 ? 'bg-warning' : 'bg-success'" />
      </div>
      <p class="mt-1 font-mono text-[10.5px] text-[#374151]">{{ health.version }}</p>
      <div class="mt-1.5 flex items-baseline justify-between">
        <span class="text-[10.5px] text-muted">未映射语义码</span>
        <span class="mono text-[14px] font-semibold" :class="health.unmapped > 0 ? 'text-warning' : 'text-success'">
          {{ health.unmapped }}
        </span>
      </div>
      <p class="mt-1 text-[9.5px] text-[#9CA3AF]">更新于 {{ formatDate(health.updatedAt) }}</p>
    </div>

    <div class="mt-auto flex items-center justify-between border-t border-[#F0F2F5] px-2.5 py-2">
      <button
        v-if="!collapsed"
        class="flex items-center gap-1.5 text-[11px] text-muted transition-colors hover:text-ink"
        @click="collapsed = true"
      >
        <AppIcon name="chevronLeft" :size="13" />收起导航
      </button>
      <button
        v-else
        class="mx-auto text-muted transition-colors hover:text-ink"
        title="展开导航"
        @click="collapsed = false"
      >
        <AppIcon name="chevronRight" :size="13" />
      </button>
      <button
        v-if="!collapsed && session.canAnalytics"
        class="text-muted transition-colors hover:text-ink"
        title="标注任务中心"
        @click="ui.jobDrawerOpen = true"
      >
        <AppIcon name="cpu" :size="14" />
      </button>
    </div>
  </aside>
</template>
