<script setup lang="ts">
import { storeToRefs } from 'pinia';
import { useUiStore } from '@/stores/ui';
import AppIcon from './AppIcon.vue';

const ui = useUiStore();
const { toasts } = storeToRefs(ui);

const meta = {
  success: { icon: 'checkCircle', ring: 'text-success', bar: 'bg-success' },
  error: { icon: 'alertCircle', ring: 'text-danger', bar: 'bg-danger' },
  warning: { icon: 'alertTriangle', ring: 'text-warning', bar: 'bg-warning' },
  info: { icon: 'info', ring: 'text-[#2563EB]', bar: 'bg-[#2563EB]' },
  job: { icon: 'cpu', ring: 'text-primary', bar: 'bg-primary' },
} as const;
</script>

<template>
  <Teleport to="body">
    <div class="pointer-events-none fixed bottom-5 right-5 z-[100] flex w-[360px] max-w-[calc(100vw-40px)] flex-col gap-2">
      <TransitionGroup
        enter-active-class="transition duration-200 ease-industrial"
        enter-from-class="translate-y-2 opacity-0"
        leave-active-class="transition duration-150 ease-in absolute"
        leave-to-class="translate-x-4 opacity-0"
        move-class="transition duration-200"
      >
        <div
          v-for="toast in toasts"
          :key="toast.id"
          class="pointer-events-auto relative overflow-hidden rounded-md border border-line bg-surface p-3 pl-3.5 shadow-lift"
        >
          <span class="absolute inset-y-0 left-0 w-[3px]" :class="meta[toast.type].bar" />
          <div class="flex items-start gap-2.5">
            <AppIcon
              :name="meta[toast.type].icon"
              :size="17"
              class="mt-0.5"
              :class="meta[toast.type].ring"
            />
            <div class="min-w-0 flex-1">
              <p class="text-[13px] font-semibold leading-5 text-ink">{{ toast.title }}</p>
              <p v-if="toast.message" class="mt-0.5 whitespace-pre-line text-[12px] leading-relaxed text-muted">
                {{ toast.message }}
              </p>
              <button
                v-if="toast.action"
                class="mt-1.5 text-[12px] font-medium text-primary hover:text-primary-hover"
                @click="toast.action?.(); ui.dismiss(toast.id)"
              >
                {{ toast.actionLabel ?? '查看' }} →
              </button>
            </div>
            <button
              class="btn btn-ghost btn-icon -mr-1 -mt-1 h-6 w-6 text-[#9CA3AF]"
              aria-label="关闭提示"
              @click="ui.dismiss(toast.id)"
            >
              <AppIcon name="close" :size="13" />
            </button>
          </div>
        </div>
      </TransitionGroup>
    </div>
  </Teleport>
</template>
