<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useRouter } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppIcon from '@/components/ui/AppIcon.vue';
import { api } from '@/api/client';
import type { ComponentSummary } from '@/api/types';
import { useUiStore } from '@/stores/ui';
import { useSessionStore } from '@/stores/session';
import { componentStatusMeta } from '@/utils/format';

const router = useRouter();
const ui = useUiStore();
const session = useSessionStore();
const { commandOpen, searchHistory } = storeToRefs(ui);

const query = ref('');
const results = ref<ComponentSummary[]>([]);
const popular = ref<ComponentSummary[]>([]);
const loading = ref(false);
const activeIndex = ref(0);
const inputEl = ref<HTMLInputElement | null>(null);
let debounceTimer: ReturnType<typeof setTimeout> | null = null;

const quickActions = computed(() =>
  [
    { label: '前往构件资产大厅', hint: '分类树 / 名称编码 / 几何相似', icon: 'grid', to: '/library', show: true },
    { label: '构件入库与门禁质检', hint: '上传 .cbfc · GATE-01 → GATE-06', icon: 'upload', to: '/ingestion', show: session.canIngest },
    { label: '外部语义标准中枢', hint: 'CRBIM / JTG / 国标映射与沙盒', icon: 'layers', to: '/semantics', show: session.canManageSemantics },
    { label: '审核质检', hint: '双模型差异与结构化签批', icon: 'shield', to: '/reviews', show: session.canReview },
    { label: '打开计算任务中心', hint: '断点恢复与脱机计算', icon: 'cpu', action: () => (ui.jobDrawerOpen = true), show: true },
  ].filter((a) => a.show !== false),
);

const shown = computed(() => (query.value.trim() ? results.value : popular.value));

function reset() {
  query.value = '';
  results.value = [];
  activeIndex.value = 0;
}

async function runSearch(term: string) {
  if (!term.trim()) {
    results.value = [];
    return;
  }
  loading.value = true;
  try {
    const data = await api.search({ q: term, pageSize: 7, sort: 'popular' });
    results.value = data.items;
    activeIndex.value = 0;
  } catch {
    results.value = [];
  } finally {
    loading.value = false;
  }
}

watch(query, (value) => {
  if (debounceTimer) clearTimeout(debounceTimer);
  debounceTimer = setTimeout(() => void runSearch(value), 220);
});

watch(commandOpen, async (open) => {
  if (open) {
    reset();
    await nextTick();
    inputEl.value?.focus();
    if (!popular.value.length) {
      try {
        const data = await api.search({ pageSize: 5, sort: 'popular' });
        popular.value = data.items;
      } catch {
        popular.value = [];
      }
    }
  }
});

function close() {
  ui.commandOpen = false;
}

function openComponent(component: ComponentSummary) {
  if (query.value.trim()) ui.rememberSearch(query.value);
  close();
  router.push(`/components/${component.familyId}`);
}

function runAction(action: (typeof quickActions.value)[number]) {
  close();
  if ('to' in action && action.to) router.push(action.to);
  else action.action?.();
}

function submitSearch() {
  const term = query.value.trim();
  if (shown.value[activeIndex.value]) {
    openComponent(shown.value[activeIndex.value]);
    return;
  }
  if (term) {
    ui.rememberSearch(term);
    close();
    router.push({ path: '/library', query: { q: term } });
  }
}

function onKeydown(event: KeyboardEvent) {
  if (!commandOpen.value) return;
  if (event.key === 'ArrowDown') {
    event.preventDefault();
    activeIndex.value = Math.min(shown.value.length - 1, activeIndex.value + 1);
  } else if (event.key === 'ArrowUp') {
    event.preventDefault();
    activeIndex.value = Math.max(0, activeIndex.value - 1);
  } else if (event.key === 'Enter') {
    event.preventDefault();
    submitSearch();
  } else if (event.key === 'Escape') {
    close();
  }
}

function onGlobalKey(event: KeyboardEvent) {
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
    event.preventDefault();
    ui.commandOpen = !ui.commandOpen;
  }
}

onMounted(() => window.addEventListener('keydown', onGlobalKey));
onBeforeUnmount(() => window.removeEventListener('keydown', onGlobalKey));
</script>

<template>
  <Teleport to="body">
    <Transition
      enter-active-class="transition duration-150 ease-industrial"
      enter-from-class="opacity-0"
      leave-active-class="transition duration-100"
      leave-to-class="opacity-0"
    >
      <div v-if="commandOpen" class="fixed inset-0 z-[90] overflow-y-auto bg-[#0B1220]/45 p-4 backdrop-blur-[2px]" @click.self="close">
        <div class="mx-auto mt-[9vh] w-full max-w-[660px] overflow-hidden rounded-lg border border-line bg-surface shadow-[0_24px_64px_-16px_rgba(15,23,42,0.5)]" @keydown="onKeydown">
          <div class="flex items-center gap-2.5 border-b border-line px-4 py-3">
            <AppIcon name="search" :size="17" class="text-[#9CA3AF]" />
            <input
              ref="inputEl"
              v-model="query"
              class="min-w-0 flex-1 border-0 bg-transparent text-[14px] text-ink placeholder:text-[#9CA3AF] focus:outline-none"
              placeholder="检索构件名称、图号、语义码 (code) 或 53 表分类…"
              spellcheck="false"
            />
            <span v-if="loading" class="h-3.5 w-3.5 animate-spin rounded-full border-2 border-[#CBD5E1] border-t-primary" />
            <span class="kbd">ESC</span>
          </div>

          <div class="max-h-[54vh] overflow-y-auto scroll-thin">
            <template v-if="query.trim()">
              <div v-if="results.length" class="p-2">
                <p class="px-2 py-1.5 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">
                  构件 · {{ results.length }} 个结果
                </p>
                <button
                  v-for="(item, index) in results"
                  :key="item.familyId"
                  class="flex w-full items-center gap-3 rounded-[5px] px-2 py-2 text-left transition-colors"
                  :class="index === activeIndex ? 'bg-[#EDF3F7]' : 'hover:bg-[#F5F7F9]'"
                  @mouseenter="activeIndex = index"
                  @click="openComponent(item)"
                >
                  <span class="flex h-9 w-11 shrink-0 items-center justify-center overflow-hidden rounded border border-line bg-white">
                    <img :src="api.previewUrl(item.familyId, 88, 64)" alt="" class="h-full w-full object-contain" loading="lazy" />
                  </span>
                  <span class="min-w-0 flex-1">
                    <span class="flex items-center gap-2">
                      <span class="truncate text-[13px] font-medium text-ink">{{ item.name }}</span>
                      <span class="mono text-[10px] text-[#9CA3AF]">{{ item.drawingCode }}</span>
                    </span>
                    <span class="mt-0.5 flex items-center gap-2 text-[10.5px] text-muted">
                      <span>{{ item.taxonomy.crbim }} · {{ item.category }}</span>
                      <span class="text-[#D1D5DB]">/</span>
                      <span class="mono">{{ item.semanticCode }}</span>
                    </span>
                  </span>
                  <span
                    class="shrink-0 rounded-full border px-1.5 py-[2px] text-[9.5px] font-medium"
                    :class="
                      componentStatusMeta(item.status).tone === 'success'
                        ? 'border-[#BBF7D0] bg-[#ECFDF3] text-success'
                        : componentStatusMeta(item.status).tone === 'warning'
                          ? 'border-[#FDE68A] bg-[#FFFBEB] text-warning'
                          : 'border-line bg-[#F3F4F6] text-[#6B7280]'
                    "
                  >
                    {{ componentStatusMeta(item.status).label }}
                  </span>
                  <AppIcon name="arrowRight" :size="13" class="text-[#CBD5E1]" />
                </button>
              </div>
              <div v-else class="px-4 py-10 text-center">
                <p class="text-[13px] font-medium text-ink">未找到匹配构件</p>
                <p class="mt-1 text-[12px] text-muted">试试图号（如 PierBody）或语义码（Bridge.Pier）</p>
              </div>
              <div class="border-t border-line bg-[#FAFBFC] px-3 py-2">
                <button class="flex items-center gap-2 text-[11.5px] text-primary hover:text-primary-hover" @click="submitSearch">
                  <AppIcon name="search" :size="12" />在资产大厅中检索 “{{ query }}” 的全部结果
                </button>
              </div>
            </template>

            <template v-else>
              <div v-if="searchHistory.length" class="border-b border-[#F0F2F5] p-2">
                <p class="px-2 py-1.5 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">最近检索</p>
                <div class="flex flex-wrap gap-1.5 px-2 pb-1">
                  <button
                    v-for="term in searchHistory"
                    :key="term"
                    class="chip hover:border-[#C7D2DA]"
                    @click="query = term"
                  >
                    <AppIcon name="clock" :size="11" />{{ term }}
                  </button>
                </div>
              </div>

              <div class="p-2">
                <p class="px-2 py-1.5 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">快捷动作</p>
                <button
                  v-for="action in quickActions"
                  :key="action.label"
                  class="flex w-full items-center gap-3 rounded-[5px] px-2 py-2 text-left transition-colors hover:bg-[#F5F7F9]"
                  @click="runAction(action)"
                >
                  <span class="flex h-7 w-7 items-center justify-center rounded border border-line bg-[#FAFBFC] text-[#4B5563]">
                    <AppIcon :name="action.icon" :size="14" />
                  </span>
                  <span class="flex-1">
                    <span class="block text-[12.5px] font-medium text-ink">{{ action.label }}</span>
                    <span class="block text-[10.5px] text-muted">{{ action.hint }}</span>
                  </span>
                  <span class="kbd">↵</span>
                </button>
              </div>

              <div v-if="popular.length" class="border-t border-[#F0F2F5] p-2">
                <p class="px-2 py-1.5 text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">高频复用构件</p>
                <button
                  v-for="item in popular"
                  :key="item.familyId"
                  class="flex w-full items-center gap-2.5 rounded-[5px] px-2 py-1.5 text-left transition-colors hover:bg-[#F5F7F9]"
                  @click="openComponent(item)"
                >
                  <AppIcon name="cube" :size="14" class="text-[#94A3B8]" />
                  <span class="flex-1 truncate text-[12.5px] text-ink">{{ item.name }}</span>
                  <span class="mono text-[10px] text-[#9CA3AF]">{{ item.familyId }}</span>
                  <span class="text-[10px] text-muted">{{ item.stats.downloads }} 次下载</span>
                </button>
              </div>
            </template>
          </div>

          <footer class="flex items-center justify-between border-t border-line bg-[#FAFBFC] px-4 py-2 text-[10.5px] text-[#9CA3AF]">
            <div class="flex items-center gap-3">
              <span class="flex items-center gap-1"><span class="kbd">↑</span><span class="kbd">↓</span>选择</span>
              <span class="flex items-center gap-1"><span class="kbd">↵</span>打开</span>
            </div>
            <span class="mono">CBF-C · 全局检索</span>
          </footer>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>
