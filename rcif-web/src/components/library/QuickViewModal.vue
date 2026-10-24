<script setup lang="ts">
import { computed, ref, watch } from 'vue';
import { useRouter } from 'vue-router';
import ModalShell from '@/components/ui/ModalShell.vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import { api } from '@/api/client';
import type { ComponentDetail, ComponentSummary } from '@/api/types';
import { useCompareStore } from '@/stores/compare';
import { useUiStore } from '@/stores/ui';
import {
  componentStatusMeta,
  formatNumber,
  formatVolume,
  sourceModeMeta,
  visibilityMeta,
} from '@/utils/format';

const props = defineProps<{ open: boolean; component: ComponentSummary | null }>();
const emit = defineEmits<{ close: []; favorite: [component: ComponentSummary] }>();

const router = useRouter();
const compare = useCompareStore();
const ui = useUiStore();
const detail = ref<ComponentDetail | null>(null);
const loading = ref(false);

const source = computed(() => (detail.value ? sourceModeMeta(detail.value.sourceMode) : null));
const status = computed(() => (detail.value ? componentStatusMeta(detail.value.status) : null));
const visibility = computed(() => (detail.value ? visibilityMeta(detail.value.visibility) : null));
const image = computed(() =>
  detail.value ? api.previewUrl(detail.value.familyId, 840, 560) : '',
);

watch(
  () => [props.open, props.component?.familyId],
  async () => {
    if (!props.open || !props.component) return;
    loading.value = true;
    detail.value = null;
    try {
      detail.value = await api.component(props.component.familyId);
    } catch (error) {
      ui.notify({ type: 'error', title: '预览加载失败', message: String(error) });
    } finally {
      loading.value = false;
    }
  },
  { immediate: true },
);

function goDetail() {
  if (!props.component) return;
  emit('close');
  router.push(`/components/${props.component.familyId}`);
}

function goConfigurator() {
  if (!props.component) return;
  emit('close');
  router.push(`/components/${props.component.familyId}/configurator`);
}

function addCompare() {
  if (!props.component) return;
  const added = compare.toggle(props.component);
  if (!added && !compare.has(props.component.familyId)) {
    ui.notify({ type: 'warning', title: '对比栏已满', message: '最多同时对比 4 个构件' });
  }
}
</script>

<template>
  <ModalShell
    :open="open"
    :title="component?.name ?? '构件快速预览'"
    :subtitle="component ? `${component.drawingCode} · ${component.category} · v${component.revision}` : ''"
    icon="cube"
    width="xl"
    @close="emit('close')"
  >
    <div v-if="component" class="grid grid-cols-1 gap-0 lg:grid-cols-[1.1fr_1fr]">
      <div class="relative flex min-h-[300px] items-center justify-center border-b border-line bg-[#F5F8FA] lg:border-b-0 lg:border-r">
        <div class="grid-bg pointer-events-none absolute inset-0 opacity-60" />
        <img :src="image" :alt="component.name" class="relative max-h-[380px] w-full object-contain p-4" />
        <div class="absolute left-3 top-3 flex gap-1.5">
          <StatusBadge v-if="source" :tone="source.tone" :label="source.label" :dot="false" />
          <StatusBadge v-if="status" :tone="status.tone" :label="status.label" />
          <StatusBadge v-if="visibility" :tone="visibility.tone" :label="visibility.label" :dot="false" />
        </div>
        <p class="absolute bottom-2 right-3 font-mono text-[9px] text-[#94A3B8]">
          {{ component.geometry.triangles.toLocaleString() }} tris · {{ component.geometry.vertices.toLocaleString() }} verts
        </p>
      </div>

      <div class="p-4">
        <p class="text-[12px] leading-relaxed text-[#4B5563]">{{ component.description }}</p>

        <div class="mt-3 grid grid-cols-2 gap-2">
          <div class="panel-inset px-2.5 py-2">
            <p class="text-[9.5px] text-muted">实体净体积</p>
            <p class="mono mt-0.5 text-[15px] font-semibold text-ink">{{ formatVolume(component.geometry.volumeM3) }}</p>
          </div>
          <div class="panel-inset px-2.5 py-2">
            <p class="text-[9.5px] text-muted">外包尺寸</p>
            <p class="mono mt-0.5 text-[12px] font-semibold text-ink">
              {{ formatNumber(component.dims.height / 1000, 2) }} × {{ formatNumber(component.dims.length / 1000, 2) }} ×
              {{ formatNumber(component.dims.width / 1000, 2) }} m
            </p>
          </div>
        </div>

        <div class="mt-3">
          <p class="eyebrow mb-1.5">标准型号预设</p>
          <div v-if="detail" class="space-y-1">
            <button
              v-for="preset in detail.parameters.presets"
              :key="preset.modelNumber"
              class="flex w-full items-center justify-between rounded border border-line bg-white px-2 py-1.5 text-left transition-colors hover:border-[#C7D2DA] hover:bg-[#FAFBFC]"
              @click="goConfigurator"
            >
              <span class="min-w-0">
                <span class="mono block text-[11px] font-semibold text-ink">{{ preset.modelNumber }}</span>
                <span class="block text-[10px] text-muted">{{ preset.label }}</span>
              </span>
              <span class="flex items-center gap-1">
                <span v-if="preset.isPrebaked" class="rounded bg-[#ECFDF3] px-1 py-[1px] text-[9px] text-success">预烘焙</span>
                <AppIcon name="chevronRight" :size="12" class="text-[#9CA3AF]" />
              </span>
            </button>
          </div>
          <div v-else class="space-y-1">
            <div v-for="i in 2" :key="i" class="skeleton h-9 w-full" />
          </div>
        </div>

        <dl class="mt-3 grid grid-cols-[76px_1fr] gap-x-2 gap-y-1 text-[11px]">
          <dt class="text-muted">语义码</dt>
          <dd class="mono truncate text-[10.5px] text-[#374151]">{{ component.semanticCode }}</dd>
          <dt class="text-muted">53 表分类</dt>
          <dd class="text-[#374151]">{{ component.taxonomy.crbim }} · {{ component.category }}</dd>
          <dt class="text-muted">权威算键</dt>
          <dd class="mono truncate text-[9.5px] text-[#4B5563]" :title="component.geometry.defaultCacheKey">
            {{ component.geometry.defaultCacheKey }}
          </dd>
        </dl>
      </div>
    </div>

    <template #footer>
      <div class="flex items-center justify-between gap-2">
        <button
          class="btn btn-outline btn-sm"
          :class="compare.has(component?.familyId ?? '') ? '!border-primary !text-primary' : ''"
          @click="addCompare"
        >
          <AppIcon name="gitCompare" :size="13" />{{ compare.has(component?.familyId ?? '') ? '移出对比' : '加入对比' }}
        </button>
        <div class="flex items-center gap-2">
          <button class="btn btn-ghost btn-sm" @click="emit('favorite', component!)">
            <AppIcon name="star" :size="13" />收藏
          </button>
          <button class="btn btn-primary btn-sm" @click="goDetail"><AppIcon name="eye" :size="13" />查看详情</button>
          <button
            v-if="component?.sourceMode === 'parametric'"
            class="btn btn-outline btn-sm"
            @click="goConfigurator"
          >
            <AppIcon name="sliders" :size="13" />配置参数
          </button>
        </div>
      </div>
    </template>
  </ModalShell>
</template>
