<script setup lang="ts">
import { computed, onMounted } from 'vue';
import { useRoute } from 'vue-router';
import { storeToRefs } from 'pinia';
import AppHeader from '@/components/layout/AppHeader.vue';
import AppSidebar from '@/components/layout/AppSidebar.vue';
import CommandPalette from '@/components/layout/CommandPalette.vue';
import JobCenter from '@/components/layout/JobCenter.vue';
import CompareBar from '@/components/layout/CompareBar.vue';
import CompareDrawer from '@/components/layout/CompareDrawer.vue';
import ToastHost from '@/components/ui/ToastHost.vue';
import { useJobsStore } from '@/stores/jobs';
import { useUiStore } from '@/stores/ui';

const route = useRoute();
const jobs = useJobsStore();
const ui = useUiStore();
const { jobDrawerOpen } = storeToRefs(ui);

const bare = computed(() => Boolean(route.meta.bare));
const fullBleed = computed(() => Boolean(route.meta.fullBleed));

onMounted(() => {
  jobs.startPolling(3000);
});
</script>

<template>
  <div class="flex h-full min-h-0 flex-col bg-app">
    <AppHeader v-if="!bare" />
    <div class="flex min-h-0 flex-1">
      <AppSidebar v-if="!bare" />
      <main
        class="relative min-w-0 flex-1"
        :class="fullBleed && !bare ? 'overflow-hidden' : 'overflow-y-auto scroll-thin'"
      >
        <RouterView v-slot="{ Component, route: current }">
          <Transition
            mode="out-in"
            enter-active-class="transition duration-150 ease-industrial"
            enter-from-class="opacity-0 translate-y-1"
            leave-active-class="transition duration-100 ease-in"
            leave-to-class="opacity-0"
          >
            <component :is="Component" :key="current.path" />
          </Transition>
        </RouterView>
      </main>
    </div>

    <CommandPalette />
    <JobCenter />
    <CompareBar />
    <CompareDrawer />
    <ToastHost />
  </div>
</template>
