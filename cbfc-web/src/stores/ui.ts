import { ref } from 'vue';
import { defineStore } from 'pinia';

export type ToastType = 'success' | 'error' | 'info' | 'warning' | 'job';

export interface Toast {
  id: string;
  type: ToastType;
  title: string;
  message?: string;
  actionLabel?: string;
  action?: () => void;
  sticky?: boolean;
}

export const useUiStore = defineStore('ui', () => {
  const toasts = ref<Toast[]>([]);
  const commandOpen = ref(false);
  const jobDrawerOpen = ref(false);
  const sidebarCollapsed = ref(false);
  const searchHistory = ref<string[]>([]);
  const mobileSidebarOpen = ref(false);

  function notify(toast: Omit<Toast, 'id'>) {
    const id = crypto.randomUUID();
    toasts.value.push({ ...toast, id });
    if (!toast.sticky) {
      setTimeout(() => dismiss(id), toast.type === 'error' ? 7000 : 4500);
    }
    return id;
  }

  function dismiss(id: string) {
    toasts.value = toasts.value.filter((t) => t.id !== id);
  }

  function rememberSearch(term: string) {
    const cleaned = term.trim();
    if (!cleaned) return;
    searchHistory.value = [cleaned, ...searchHistory.value.filter((s) => s !== cleaned)].slice(0, 8);
  }

  function openCommand() {
    commandOpen.value = true;
  }

  return {
    toasts,
    commandOpen,
    jobDrawerOpen,
    sidebarCollapsed,
    searchHistory,
    mobileSidebarOpen,
    notify,
    dismiss,
    rememberSearch,
    openCommand,
  };
});
