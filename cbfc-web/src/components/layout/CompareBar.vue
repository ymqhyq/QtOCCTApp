<script setup lang="ts">
import { useRouter } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppIcon from '@/components/ui/AppIcon.vue';
import { useCompareStore } from '@/stores/compare';

const router = useRouter();
const compare = useCompareStore();
const { items, open } = storeToRefs(compare);

function goDetail(familyId: string) {
  router.push(`/components/${familyId}`);
}
</script>

<template>
  <Teleport to="body">
    <Transition
      enter-active-class="transition duration-200 ease-industrial"
      enter-from-class="translate-y-4 opacity-0"
      leave-active-class="transition duration-150"
      leave-to-class="translate-y-4 opacity-0"
    >
      <div v-if="items.length && !open" class="fixed bottom-4 left-1/2 z-[60] w-[min(760px,calc(100vw-32px))] -translate-x-1/2">
        <div class="flex items-center gap-3 rounded-lg border border-[#334155] bg-[#0F172A]/95 px-3 py-2 text-white shadow-[0_12px_32px_-8px_rgba(2,6,23,0.6)] backdrop-blur">
          <span class="flex h-7 w-7 items-center justify-center rounded bg-white/10 text-hl">
            <AppIcon name="gitCompare" :size="15" />
          </span>
          <div class="hidden shrink-0 sm:block">
            <p class="text-[11px] font-semibold text-white">已选 {{ items.length }} 项构件</p>
            <p class="text-[9.5px] text-[#94A3B8]">最多 4 项 · 参数与算量深度对比</p>
          </div>
          <div class="flex min-w-0 flex-1 items-center gap-1.5 overflow-x-auto no-scrollbar">
            <button
              v-for="item in items"
              :key="item.familyId"
              class="group flex shrink-0 items-center gap-1 rounded border border-white/10 bg-white/5 py-1 pl-1.5 pr-1 text-[10.5px] transition-colors hover:border-hl/40"
              @click="goDetail(item.familyId)"
            >
              <span class="max-w-[116px] truncate">{{ item.name }}</span>
              <span
                class="flex h-3.5 w-3.5 items-center justify-center rounded-full text-[#94A3B8] hover:bg-white/10 hover:text-white"
                @click.stop="compare.remove(item.familyId)"
              >
                <AppIcon name="close" :size="9" />
              </span>
            </button>
          </div>
          <button class="btn btn-primary btn-sm shrink-0" @click="open = true">
            启动深度对比
            <AppIcon name="arrowRight" :size="13" />
          </button>
          <button class="shrink-0 text-[#94A3B8] transition-colors hover:text-white" title="清空" @click="compare.clear()">
            <AppIcon name="close" :size="14" />
          </button>
        </div>
      </div>
    </Transition>
  </Teleport>
</template>
