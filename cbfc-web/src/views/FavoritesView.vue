<script setup lang="ts">
import { onMounted, ref } from 'vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import EmptyState from '@/components/ui/EmptyState.vue';
import ComponentCard from '@/components/library/ComponentCard.vue';
import QuickViewModal from '@/components/library/QuickViewModal.vue';
import { api } from '@/api/client';
import type { ComponentSummary } from '@/api/types';
import { useUiStore } from '@/stores/ui';

const ui = useUiStore();
const items = ref<ComponentSummary[]>([]);
const loading = ref(true);
const favorites = ref<Set<string>>(new Set());
const quickView = ref<ComponentSummary | null>(null);
const quickOpen = ref(false);

async function load() {
  loading.value = true;
  try {
    const response = await api.favorites();
    items.value = response.items;
    favorites.value = new Set(response.items.map((item: any) => item.familyId));
  } catch (error) {
    ui.notify({ type: 'error', title: '收藏加载失败', message: String(error) });
  } finally {
    loading.value = false;
  }
}

async function toggleFavorite(component: ComponentSummary) {
  try {
    const result = await api.toggleFavorite(component.familyId, true);
    favorites.value = new Set([...favorites.value].filter((id) => id !== component.familyId));
    items.value = items.value.filter((item) => item.familyId !== component.familyId);
    ui.notify({ type: 'success', title: '已取消收藏', message: component.name });
  } catch (error) {
    ui.notify({ type: 'error', title: '操作失败', message: String(error) });
  }
}

onMounted(load);
</script>

<template>
  <div class="mx-auto max-w-[1500px] px-4 py-4 xl:px-6">
    <div class="flex flex-wrap items-end justify-between gap-2">
      <div>
        <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
          <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
          <AppIcon name="chevronRight" :size="10" />
          <span class="text-[#4B5563]">个人收藏</span>
        </div>
        <h1 class="mt-1 flex items-center gap-2 text-[20px] font-semibold tracking-tight text-ink">
          个人收藏
          <span class="mono rounded bg-[#F1F4F7] px-1.5 py-[2px] text-[11px] font-normal text-muted">{{ items.length }}</span>
        </h1>
        <p class="mt-0.5 text-[12px] text-muted">跨项目高频复用的构件置于此，支持快速变参与对比。</p>
      </div>
      <RouterLink to="/library" class="btn btn-outline btn-sm"><AppIcon name="grid" :size="12" />前往构件大厅</RouterLink>
    </div>

    <div v-if="loading" class="mt-3 grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-3 min-[1900px]:grid-cols-4">
      <div v-for="i in 4" :key="i" class="skeleton h-[330px] w-full" />
    </div>
    <div v-else-if="items.length" class="mt-3 grid grid-cols-1 gap-3 sm:grid-cols-2 xl:grid-cols-3 min-[1900px]:grid-cols-4">
      <ComponentCard
        v-for="item in items"
        :key="item.familyId"
        :component="item"
        :favorited="true"
        @preview="quickView = $event; quickOpen = true"
        @favorite="toggleFavorite"
      />
    </div>
    <EmptyState
      v-else
      class="mt-3"
      icon="star"
      title="尚无收藏构件"
      description="在构件大厅或详情页点击星形按钮，将高频复用的构件加入收藏。"
    >
      <RouterLink to="/library" class="btn btn-primary btn-sm">浏览构件大厅</RouterLink>
    </EmptyState>

    <QuickViewModal :open="quickOpen" :component="quickView" @close="quickOpen = false" @favorite="toggleFavorite" />
  </div>
</template>
