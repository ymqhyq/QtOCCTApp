<script setup lang="ts">
import { computed } from 'vue';

const props = withDefaults(
  defineProps<{
    items: { label: string; value: number; hint?: string }[];
    max?: number;
    color?: string;
    unit?: string;
    digits?: number;
  }>(),
  { color: '#1F5A7A', unit: '', digits: 0 },
);

const maxValue = computed(() => props.max ?? Math.max(...props.items.map((i) => i.value), 1));

function width(value: number) {
  return `${Math.max(2, (value / maxValue.value) * 100)}%`;
}

function format(value: number) {
  return new Intl.NumberFormat('zh-CN', { maximumFractionDigits: props.digits }).format(value);
}
</script>

<template>
  <div class="space-y-2">
    <div v-for="item in items" :key="item.label" class="group">
      <div class="mb-1 flex items-baseline justify-between gap-2">
        <span class="text-[10.5px] text-[#4B5563]">{{ item.label }}</span>
        <span class="mono text-[10.5px] font-medium text-ink">{{ format(item.value) }}{{ unit }}</span>
      </div>
      <div class="h-1.5 overflow-hidden rounded-full bg-[#EEF1F4]" :title="item.hint">
        <div
          class="h-full rounded-full transition-all duration-500"
          :style="{ width: width(item.value), backgroundColor: color }"
        />
      </div>
    </div>
  </div>
</template>
