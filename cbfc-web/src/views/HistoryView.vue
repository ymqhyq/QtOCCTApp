<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import EmptyState from '@/components/ui/EmptyState.vue';
import { api } from '@/api/client';
import { useUiStore } from '@/stores/ui';
import { formatDateTime } from '@/utils/format';

const ui = useUiStore();
const items = ref<{ id: string; familyId: string; name: string; action: string; format?: string; at: string; detail?: string }[]>([]);
const loading = ref(true);
const filter = ref<'all' | 'download' | 'evaluate' | 'handoff' | 'open' | 'export'>('all');

const actionMeta: Record<string, { label: string; icon: string; tone: string }> = {
  download: { label: '下载', icon: 'download', tone: 'text-primary' },
  export: { label: '导出', icon: 'globe', tone: 'text-[#1D4ED8]' },
  evaluate: { label: '变参', icon: 'sliders', tone: 'text-success' },
  handoff: { label: '深链推送', icon: 'desktop', tone: 'text-warning' },
  open: { label: '查看', icon: 'eye', tone: 'text-muted' },
};

const filtered = computed(() =>
  filter.value === 'all' ? items.value : items.value.filter((item) => item.action === filter.value),
);

async function load() {
  loading.value = true;
  try {
    const response = await api.history();
    items.value = response.items;
  } catch (error) {
    ui.notify({ type: 'error', title: '历史加载失败', message: String(error) });
  } finally {
    loading.value = false;
  }
}

function setFilter(value: string) {
  filter.value = value as typeof filter.value;
}

onMounted(load);
</script>

<template>
  <div class="mx-auto max-w-[1300px] px-4 py-4 xl:px-6">
    <div class="flex flex-wrap items-end justify-between gap-2">
      <div>
        <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
          <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
          <AppIcon name="chevronRight" :size="10" />
          <span class="text-[#4B5563]">下载与使用历史</span>
        </div>
        <h1 class="mt-1 text-[20px] font-semibold tracking-tight text-ink">下载与使用历史</h1>
        <p class="mt-0.5 text-[12px] text-muted">构件获取、变参求解与桌面端深链推送的全链路记录。</p>
      </div>
      <div class="flex items-center gap-1">
        <button
          v-for="tab in [
            { value: 'all', label: '全部' },
            { value: 'download', label: '下载' },
            { value: 'evaluate', label: '变参' },
            { value: 'handoff', label: '深链' },
            { value: 'export', label: '导出' },
          ]"
          :key="tab.value"
          class="rounded px-2 py-1 text-[11px] transition-colors"
          :class="filter === tab.value ? 'bg-[#E8EFF4] font-medium text-primary' : 'text-muted hover:text-ink'"
          @click="setFilter(tab.value)"
        >
          {{ tab.label }}
        </button>
      </div>
    </div>

    <div class="panel mt-3 overflow-hidden">
      <table v-if="!loading && filtered.length" class="table-dense">
        <thead>
          <tr><th>时间</th><th>动作</th><th>构件</th><th>格式 / 详情</th><th class="text-right">操作</th></tr>
        </thead>
        <tbody>
          <tr v-for="entry in filtered" :key="entry.id">
            <td class="whitespace-nowrap font-mono text-[9.5px] text-muted">{{ formatDateTime(entry.at) }}</td>
            <td>
              <span class="inline-flex items-center gap-1 text-[10.5px]" :class="actionMeta[entry.action]?.tone">
                <AppIcon :name="actionMeta[entry.action]?.icon ?? 'history'" :size="12" />
                {{ actionMeta[entry.action]?.label ?? entry.action }}
              </span>
            </td>
            <td>
              <RouterLink :to="`/components/${entry.familyId}`" class="text-[11.5px] font-medium text-ink hover:text-primary">
                {{ entry.name }}
              </RouterLink>
              <p class="mono text-[8.5px] text-[#9CA3AF]">{{ entry.familyId }}</p>
            </td>
            <td>
              <span v-if="entry.format" class="mono rounded bg-[#F1F4F7] px-1 py-[1px] text-[9px] text-[#4B5563]">{{ entry.format }}</span>
              <span class="ml-1.5 text-[10px] text-muted">{{ entry.detail }}</span>
            </td>
            <td class="text-right">
              <RouterLink :to="`/components/${entry.familyId}`" class="btn btn-outline btn-sm">查看构件</RouterLink>
            </td>
          </tr>
        </tbody>
      </table>
      <div v-else-if="loading" class="space-y-2 p-3">
        <div v-for="i in 6" :key="i" class="skeleton h-8 w-full" />
      </div>
      <EmptyState v-else class="m-3" icon="history" title="暂无历史记录" description="检索并下载构件后，操作记录将出现在此处。" compact />
    </div>

    <p class="mt-2 text-[9.5px] text-[#9CA3AF]">
      演示环境历史记录按会话内存保存；生产环境将通过 PostgreSQL RLS 按租户隔离并保留完整审计链路。
    </p>
  </div>
</template>
