<script setup lang="ts">
import AppIcon from './AppIcon.vue';

withDefaults(
  defineProps<{
    title: string;
    description?: string;
    icon?: string;
    compact?: boolean;
  }>(),
  { icon: 'search', compact: false },
);
</script>

<template>
  <div
    class="flex flex-col items-center justify-center rounded-md border border-dashed border-[#D8DEE5] bg-[#FBFCFD] px-6 text-center"
    :class="compact ? 'py-8' : 'py-14'"
  >
    <div class="mb-3 flex h-11 w-11 items-center justify-center rounded-full border border-line bg-white text-muted shadow-panel">
      <AppIcon :name="icon" :size="20" />
    </div>
    <p class="text-[14px] font-semibold text-ink">{{ title }}</p>
    <p v-if="description" class="mt-1 max-w-lg text-[12.5px] leading-relaxed text-muted">{{ description }}</p>
    <div v-if="$slots.default" class="mt-4 flex flex-wrap items-center justify-center gap-2">
      <slot />
    </div>
  </div>
</template>
