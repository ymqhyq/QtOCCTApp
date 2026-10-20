<script setup lang="ts">
import { onBeforeUnmount, watch } from 'vue';
import AppIcon from './AppIcon.vue';

const props = withDefaults(
  defineProps<{
    open: boolean;
    title: string;
    subtitle?: string;
    icon?: string;
    width?: 'sm' | 'md' | 'lg' | 'xl' | 'full';
    closeOnBackdrop?: boolean;
  }>(),
  { width: 'md', closeOnBackdrop: true },
);
const emit = defineEmits<{ close: [] }>();

const widths: Record<string, string> = {
  sm: 'max-w-md',
  md: 'max-w-2xl',
  lg: 'max-w-4xl',
  xl: 'max-w-6xl',
  full: 'max-w-[min(1400px,96vw)]',
};

function onKey(event: KeyboardEvent) {
  if (event.key === 'Escape' && props.open) emit('close');
}

watch(
  () => props.open,
  (open) => {
    document.body.style.overflow = open ? 'hidden' : '';
    if (open) window.addEventListener('keydown', onKey);
    else window.removeEventListener('keydown', onKey);
  },
);

onBeforeUnmount(() => {
  document.body.style.overflow = '';
  window.removeEventListener('keydown', onKey);
});
</script>

<template>
  <Teleport to="body">
    <Transition
      enter-active-class="transition duration-200 ease-industrial"
      enter-from-class="opacity-0"
      leave-active-class="transition duration-150 ease-in"
      leave-to-class="opacity-0"
    >
      <div v-if="open" class="fixed inset-0 z-[80] flex items-center justify-center p-4 sm:p-6">
        <div
          class="absolute inset-0 bg-[#0B1220]/45 backdrop-blur-[2px]"
          @click="closeOnBackdrop && emit('close')"
        />
        <div
          class="relative flex max-h-[90vh] w-full flex-col overflow-hidden rounded-lg border border-line bg-surface shadow-[0_24px_64px_-16px_rgba(15,23,42,0.45)]"
          :class="widths[width]"
          role="dialog"
          aria-modal="true"
        >
          <header class="flex items-start gap-3 border-b border-line px-5 py-3.5">
            <div v-if="icon" class="mt-0.5 flex h-8 w-8 items-center justify-center rounded-[5px] bg-[#E8EFF4] text-primary">
              <AppIcon :name="icon" :size="16" />
            </div>
            <div class="min-w-0 flex-1">
              <h2 class="truncate text-[15px] font-semibold text-ink">{{ title }}</h2>
              <p v-if="subtitle" class="mt-0.5 truncate text-[12px] text-muted">{{ subtitle }}</p>
            </div>
            <button class="btn btn-ghost btn-icon -mr-1" aria-label="关闭" @click="emit('close')">
              <AppIcon name="close" :size="16" />
            </button>
          </header>
          <div class="min-h-0 flex-1 overflow-auto scroll-thin">
            <slot />
          </div>
          <footer v-if="$slots.footer" class="border-t border-line bg-[#FAFBFC] px-5 py-3">
            <slot name="footer" />
          </footer>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>
