<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppIcon from '@/components/ui/AppIcon.vue';
import { useSessionStore, ROLE_OPTIONS } from '@/stores/session';
import { HEADER_NAV, visibleNav } from '@/navigation';
import { useUiStore } from '@/stores/ui';
import { useJobsStore } from '@/stores/jobs';

const route = useRoute();
const router = useRouter();
const session = useSessionStore();
const ui = useUiStore();
const jobs = useJobsStore();
const { user } = storeToRefs(session);
const { jobDrawerOpen } = storeToRefs(ui);

const menuOpen = ref(false);
const navOpen = ref(false);

const navItems = computed(() =>
  visibleNav(session, HEADER_NAV).map((item) =>
    item.to === '/reviews' ? { ...item, badge: jobs.active.length + 5 } : item,
  ),
);

function isActive(path: string) {
  if (path === '/library') return route.path === '/library' || route.path.startsWith('/components');
  return route.path === path;
}

async function switchRole(role: string) {
  menuOpen.value = false;
  if (session.user?.role === role) return;
  await session.login(role);
  window.location.assign('/');
}

function logout() {
  menuOpen.value = false;
  session.logout();
  router.push('/login');
}

function closeMenus(event: MouseEvent) {
  const target = event.target as HTMLElement;
  if (!target.closest('[data-user-menu]')) menuOpen.value = false;
}

onMounted(() => document.addEventListener('click', closeMenus));
onBeforeUnmount(() => document.removeEventListener('click', closeMenus));
</script>

<template>
  <header class="sticky top-0 z-50 flex h-14 items-center gap-3 border-b border-line bg-surface/95 px-4 backdrop-blur">
    <button class="btn btn-ghost btn-icon lg:hidden" aria-label="打开导航" @click="ui.mobileSidebarOpen = !ui.mobileSidebarOpen">
      <AppIcon name="menu" :size="18" />
    </button>

    <RouterLink to="/" class="flex items-center gap-2.5">
      <span class="flex h-8 w-8 items-center justify-center rounded-[6px] bg-primary text-white shadow-[0_2px_6px_rgba(31,90,122,0.35)]">
        <AppIcon name="cube" :size="18" />
      </span>
      <span class="hidden flex-col leading-tight sm:flex">
        <span class="text-[14px] font-semibold tracking-tight text-ink">铁路工程数字构件库</span>
        <span class="text-[9px] font-medium uppercase tracking-[0.16em] text-[#94A3B8]">CBF-C · Component Foundation</span>
      </span>
    </RouterLink>

    <div class="mx-1 hidden h-6 w-px bg-line lg:block" />

    <nav class="hidden min-w-0 items-center gap-0.5 lg:flex">
      <RouterLink
        v-for="item in navItems"
        :key="item.to"
        :to="item.to"
        class="relative flex h-14 items-center gap-1.5 px-2.5 text-[13px] font-medium transition-colors"
        :class="isActive(item.to) ? 'text-primary' : 'text-[#4B5563] hover:text-ink'"
      >
        <AppIcon :name="item.icon" :size="15" :stroke-width="1.6" />
        <span class="whitespace-nowrap">{{ item.label }}</span>
        <span
          v-if="item.badge"
          class="ml-0.5 rounded-full bg-[#FFF7ED] px-1.5 py-[1px] text-[9.5px] font-semibold text-warning"
        >
          {{ item.badge }}
        </span>
        <span
          v-if="isActive(item.to)"
          class="absolute inset-x-2 -bottom-px h-[2px] rounded-full bg-primary"
        />
      </RouterLink>
    </nav>

    <div class="flex-1" />

    <button
      class="hidden h-8 w-[248px] items-center gap-2 rounded-[5px] border border-line bg-[#F8F9FB] px-2.5 text-left text-[12px] text-[#9CA3AF] transition-colors hover:border-[#C7D2DA] hover:bg-surface md:flex"
      @click="ui.openCommand()"
    >
      <AppIcon name="search" :size="14" />
      <span class="flex-1 truncate">检索构件、图号、语义码…</span>
      <span class="kbd">Ctrl K</span>
    </button>
    <button class="btn btn-ghost btn-icon md:hidden" aria-label="搜索" @click="ui.openCommand()">
      <AppIcon name="search" :size="17" />
    </button>

    <button
      class="relative flex h-8 items-center gap-1.5 rounded-[5px] px-2 text-[#4B5563] transition-colors hover:bg-[#EEF1F4] hover:text-ink"
      :class="{ 'bg-[#EEF1F4] text-ink': jobDrawerOpen }"
      title="计算任务中心"
      @click="ui.jobDrawerOpen = !ui.jobDrawerOpen"
    >
      <AppIcon name="cpu" :size="17" />
      <span class="hidden text-[12.5px] font-medium xl:inline">计算任务</span>
      <span
        v-if="jobs.active.length"
        class="absolute -right-0.5 -top-0.5 flex h-4 min-w-4 items-center justify-center rounded-full bg-warning px-1 text-[9.5px] font-bold text-white ring-2 ring-white"
      >
        {{ jobs.active.length }}
      </span>
    </button>

    <div class="hidden h-6 w-px bg-line xl:block" />

    <div class="hidden items-center gap-1.5 rounded-[5px] border border-line bg-[#FAFBFC] px-2 py-1 xl:flex">
      <AppIcon name="pin" :size="12" class="text-[#94A3B8]" />
      <span class="text-[11.5px] font-medium text-[#374151]">{{ user?.tenantName?.split('·')[0]?.trim() }}</span>
      <span class="rounded bg-[#E8EFF4] px-1 text-[9px] font-semibold text-primary">租户</span>
    </div>

    <div class="relative" data-user-menu>
      <button class="flex items-center gap-2 rounded-[5px] py-0.5 pl-0.5 pr-1.5 transition-colors hover:bg-[#F3F5F7]" @click.stop="menuOpen = !menuOpen">
        <span class="flex h-7 w-7 items-center justify-center rounded-full bg-primary text-[12px] font-semibold text-white">
          {{ user?.avatarInitials }}
        </span>
        <span class="hidden flex-col items-start leading-tight xl:flex">
          <span class="text-[12px] font-medium text-ink">{{ user?.name }}</span>
          <span class="text-[9.5px] text-muted">{{ user?.roleLabel }}</span>
        </span>
        <AppIcon name="chevronDown" :size="12" class="text-[#9CA3AF]" />
      </button>

      <Transition
        enter-active-class="transition duration-150 ease-industrial"
        enter-from-class="opacity-0 -translate-y-1"
        leave-active-class="transition duration-100"
        leave-to-class="opacity-0"
      >
        <div
          v-if="menuOpen"
          class="absolute right-0 top-[calc(100%+8px)] w-[280px] overflow-hidden rounded-md border border-line bg-surface shadow-lift"
        >
          <div class="border-b border-line px-3 py-2.5">
            <p class="text-[12px] font-semibold text-ink">{{ user?.name }} · {{ user?.roleLabel }}</p>
            <p class="mt-0.5 font-mono text-[10px] text-muted">{{ user?.email }}</p>
          </div>
          <div class="border-b border-line px-3 py-2">
            <p class="mb-1.5 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">切换演示身份</p>
            <button
              v-for="role in ROLE_OPTIONS"
              :key="role.role"
              class="flex w-full items-center gap-2 rounded-[4px] px-1.5 py-1.5 text-left transition-colors hover:bg-[#F3F5F7]"
              @click="switchRole(role.role)"
            >
              <AppIcon :name="role.icon" :size="14" :class="session.user?.role === role.role ? 'text-primary' : 'text-[#9CA3AF]'" />
              <span class="flex-1">
                <span class="block text-[12px] font-medium text-ink">{{ role.label }}</span>
                <span class="block text-[10px] text-muted">{{ role.desc }}</span>
              </span>
              <AppIcon v-if="session.user?.role === role.role" name="check" :size="13" class="text-primary" />
            </button>
          </div>
          <div class="p-1">
            <RouterLink to="/" class="nav-link" @click="menuOpen = false">
              <AppIcon name="compass" :size="15" />工作台
            </RouterLink>
            <RouterLink to="/favorites" class="nav-link" @click="menuOpen = false">
              <AppIcon name="star" :size="15" />个人收藏
            </RouterLink>
            <RouterLink v-if="session.isInternal" to="/history" class="nav-link" @click="menuOpen = false">
              <AppIcon name="history" :size="15" />下载历史
            </RouterLink>
            <button class="nav-link w-full text-danger hover:bg-[#FEF2F2] hover:text-danger" @click="logout">
              <AppIcon name="logout" :size="15" />退出登录
            </button>
          </div>
        </div>
      </Transition>
    </div>
  </header>
</template>
