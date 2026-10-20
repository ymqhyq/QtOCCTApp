<script setup lang="ts">
import { computed } from 'vue';
import { useRouter } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import { api } from '@/api/client';
import { useCompareStore } from '@/stores/compare';
import { useUiStore } from '@/stores/ui';
import type { ComponentSummary } from '@/api/types';
import {
  cleanDisplayName,
  componentStatusMeta,
  formatNumber,
  formatVolume,
  sourceModeMeta,
} from '@/utils/format';

const props = defineProps<{
  component: ComponentSummary;
  favorited?: boolean;
  dense?: boolean;
  similarity?: number;
}>();

const emit = defineEmits<{
  preview: [component: ComponentSummary];
  favorite: [component: ComponentSummary];
}>();

const router = useRouter();
const compare = useCompareStore();
const ui = useUiStore();

const source = computed(() => sourceModeMeta(props.component.sourceMode));
const status = computed(() => componentStatusMeta(props.component.status));
const isParametric = computed(() => props.component.sourceMode === 'parametric');
const heightMm = computed(() => props.component.dims.height);

function openDetail() {
  router.push(`/components/${props.component.familyId}`);
}

function openConfigurator() {
  if (!isParametric.value) {
    openDetail();
    return;
  }
  router.push(`/components/${props.component.familyId}/configurator`);
}

function toggleCompare() {
  const added = compare.toggle(props.component);
  if (!added && !compare.has(props.component.familyId)) {
    ui.notify({ type: 'warning', title: '对比栏已满', message: '最多同时对比 4 个构件' });
  }
}

const previewUrl = computed(() => api.previewUrl(props.component.familyId, 640, 400));
const thumbUrl = computed(() => api.previewUrl(props.component.familyId, 360, 240));
</script>

<template>
  <article class="group panel card-hover relative flex flex-col overflow-hidden" :class="dense ? '' : ''">
    <div class="relative h-[168px] overflow-hidden border-b border-[#EDF0F3] bg-[#F7F9FB]">
      <img
        :src="thumbUrl"
        :alt="`${component.name} 等轴测预览`"
        class="h-full w-full cursor-zoom-in object-contain transition-transform duration-500 ease-industrial group-hover:scale-[1.04]"
        loading="lazy"
        @click="emit('preview', component)"
      />

      <div class="pointer-events-none absolute inset-x-2 top-2 flex items-start justify-between gap-1">
        <div class="flex flex-wrap gap-1">
          <StatusBadge :tone="source.tone" :label="source.label" :dot="false" class="!bg-white/95 backdrop-blur" />
          <StatusBadge
            :tone="status.tone"
            :label="status.label"
            class="!bg-white/95 backdrop-blur"
          />
          <span
            v-if="similarity !== undefined"
            class="rounded border border-[#C9DCE6] bg-[#EDF4F8]/95 px-1.5 py-[2px] font-mono text-[9.5px] font-semibold text-primary backdrop-blur"
          >
            相似 {{ similarity }}%
          </span>
        </div>
        <button
          class="pointer-events-auto flex h-6 w-6 items-center justify-center rounded-full border border-white/70 bg-white/90 text-[#9CA3AF] shadow-panel backdrop-blur transition-colors hover:text-warning"
          :class="favorited ? '!text-warning' : ''"
          :title="favorited ? '取消收藏' : '收藏构件'"
          @click.stop="emit('favorite', component)"
        >
          <AppIcon name="star" :size="13" :stroke-width="favorited ? 2.2 : 1.6" :class="favorited ? 'fill-warning' : ''" />
        </button>
      </div>

      <div
        class="pointer-events-none absolute inset-x-0 bottom-0 flex translate-y-1 items-center justify-between bg-gradient-to-t from-[#0B1220]/75 via-[#0B1220]/30 to-transparent px-2.5 pb-1.5 pt-6 opacity-0 transition-all duration-200 group-hover:translate-y-0 group-hover:opacity-100"
      >
        <span class="flex items-center gap-1 text-[9.5px] font-medium text-white/85">
          <AppIcon name="sparkles" :size="10" />悬停查看 · 点击放大
        </span>
        <button
          class="pointer-events-auto rounded border border-white/25 bg-white/10 px-1.5 py-[2px] text-[9.5px] text-white backdrop-blur hover:bg-white/20"
          @click.stop="emit('preview', component)"
        >
          快速预览
        </button>
      </div>

      <span
        v-if="component.qa.passed"
        class="absolute bottom-2 left-2 flex items-center gap-1 rounded bg-white/90 px-1.5 py-[2px] text-[9px] font-medium text-success backdrop-blur"
        title="6 道门禁全部通过"
      >
        <AppIcon name="checkCircle" :size="10" />GATE 6/6
      </span>
    </div>

    <div class="flex flex-1 flex-col p-3">
      <div class="flex items-start justify-between gap-2">
        <button class="min-w-0 text-left" @click="openDetail">
          <h3 class="truncate text-[13.5px] font-semibold leading-5 text-ink transition-colors group-hover:text-primary">
            {{ cleanDisplayName(component.name) }}
          </h3>
        </button>
        <span class="shrink-0 rounded bg-[#F1F4F7] px-1.5 py-[2px] font-mono text-[9px] text-[#6B7280]">
          v{{ component.revision }}
        </span>
      </div>

      <p class="mt-0.5 flex items-center gap-1.5 truncate font-mono text-[10px] text-muted">
        <span class="truncate">{{ component.drawingCode }}</span>
        <span class="text-[#D1D5DB]">/</span>
        <span class="truncate">{{ component.engine }}</span>
      </p>

      <div class="mt-2.5 grid grid-cols-3 gap-1 rounded-[5px] border border-[#F0F2F5] bg-[#FAFBFC] px-2 py-1.5">
        <div>
          <p class="text-[9px] text-[#9CA3AF]">净体积</p>
          <p class="mono mt-0.5 text-[11px] font-semibold text-ink">
            {{ formatNumber(component.geometry.volumeM3, component.geometry.volumeM3 < 10 ? 2 : 1) }}
            <span class="text-[8px] font-normal text-muted">m³</span>
          </p>
        </div>
        <div>
          <p class="text-[9px] text-[#9CA3AF]">高度</p>
          <p class="mono mt-0.5 text-[11px] font-semibold text-ink">
            {{ formatNumber(heightMm / 1000, 2) }}<span class="text-[8px] font-normal text-muted">m</span>
          </p>
        </div>
        <div>
          <p class="text-[9px] text-[#9CA3AF]">预置规格</p>
          <p class="mono mt-0.5 text-[11px] font-semibold text-ink">
            {{ component.presetCount }}<span class="text-[8px] font-normal text-muted"> 型</span>
          </p>
        </div>
      </div>

      <div class="mt-2 flex flex-wrap gap-1">
        <span v-for="tag in component.tags.slice(0, 2)" :key="tag" class="tag">{{ tag }}</span>
        <span class="tag mono">53 表 {{ component.taxonomy.crbim }}</span>
      </div>

      <div class="mt-2.5 flex flex-wrap items-center gap-1.5 pt-0.5">
        <button class="btn btn-primary btn-sm flex-1" @click="openDetail">
          <AppIcon name="eye" :size="12" />查看详情
        </button>
        <button v-if="isParametric" class="btn btn-outline btn-sm" @click="openConfigurator">
          <AppIcon name="sliders" :size="12" />配置参数
        </button>
        <button
          class="btn btn-sm border"
          :class="compare.has(component.familyId) ? 'border-primary bg-[#E8EFF4] text-primary' : 'btn-outline'"
          @click="toggleCompare"
        >
          <AppIcon :name="compare.has(component.familyId) ? 'check' : 'gitCompare'" :size="12" />
          对比
        </button>
      </div>
    </div>
  </article>
</template>
