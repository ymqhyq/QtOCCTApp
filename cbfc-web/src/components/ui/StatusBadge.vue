<script setup lang="ts">
import { computed } from 'vue';
import type { Tone } from '@/utils/format';

const props = withDefaults(
  defineProps<{
    tone?: Tone;
    label?: string;
    dot?: boolean;
    size?: 'sm' | 'md';
    mono?: boolean;
  }>(),
  { tone: 'muted', dot: true, size: 'sm', mono: false },
);

const toneClasses = computed<Record<Tone, string>>(() => ({
  success: 'bg-[#ECFDF3] text-success border-[#BBF7D0]',
  warning: 'bg-[#FFFBEB] text-warning border-[#FDE68A]',
  danger: 'bg-[#FEF2F2] text-danger border-[#FECACA]',
  info: 'bg-[#EFF6FF] text-[#1D4ED8] border-[#BFDBFE]',
  muted: 'bg-[#F3F4F6] text-[#4B5563] border-[#E5E7EB]',
  primary: 'bg-[#E8EFF4] text-primary border-[#C9DCE6]',
}));

const dotClasses = computed<Record<Tone, string>>(() => ({
  success: 'bg-success',
  warning: 'bg-warning',
  danger: 'bg-danger',
  info: 'bg-[#2563EB]',
  muted: 'bg-[#9CA3AF]',
  primary: 'bg-primary',
}));
</script>

<template>
  <span
    class="inline-flex items-center gap-1.5 rounded-full border font-medium leading-none"
    :class="[
      toneClasses[tone],
      size === 'sm' ? 'px-1.5 py-[3px] text-[10.5px]' : 'px-2 py-1 text-[11.5px]',
      mono ? 'mono' : '',
    ]"
  >
    <span v-if="dot" class="h-1.5 w-1.5 rounded-full" :class="dotClasses[tone]" />
    <slot>{{ label }}</slot>
  </span>
</template>
