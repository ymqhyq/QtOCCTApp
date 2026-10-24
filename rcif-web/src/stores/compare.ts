import { computed, ref } from 'vue';
import { defineStore } from 'pinia';
import type { ComponentSummary } from '@/api/types';

export const useCompareStore = defineStore('compare', () => {
  const items = ref<ComponentSummary[]>([]);
  const open = ref(false);

  const count = computed(() => items.value.length);
  const has = (familyId: string) => items.value.some((c) => c.familyId === familyId);

  function toggle(component: ComponentSummary) {
    if (has(component.familyId)) {
      items.value = items.value.filter((c) => c.familyId !== component.familyId);
      return false;
    }
    if (items.value.length >= 4) return false;
    items.value = [...items.value, component];
    return true;
  }

  function remove(familyId: string) {
    items.value = items.value.filter((c) => c.familyId !== familyId);
  }

  function clear() {
    items.value = [];
    open.value = false;
  }

  return { items, open, count, has, toggle, remove, clear };
});
