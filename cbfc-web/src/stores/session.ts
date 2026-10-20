import { computed, ref } from 'vue';
import { defineStore } from 'pinia';
import { api, getToken, setToken } from '@/api/client';
import type { UserSession } from '@/api/types';

const USER_KEY = 'cbfc.web.user';

export const ROLE_OPTIONS = [
  {
    role: 'designer',
    label: '设计工程师',
    name: '张工',
    desc: '检索、变参、下载与入库制备',
    icon: 'compass',
  },
  {
    role: 'reviewer',
    label: '标准化审查专家',
    name: '李总工',
    desc: '门禁复核、双模型对比与签批',
    icon: 'shield',
  },
  {
    role: 'standard-admin',
    label: '标准管理员',
    name: '王标准',
    desc: '分类树编排、语义映射与沙盒推演',
    icon: 'layers',
  },
  {
    role: 'asset-admin',
    label: '资产管理员',
    name: '赵管理',
    desc: '租户治理、算力监控与审计',
    icon: 'activity',
  },
  {
    role: 'partner',
    label: '外部合作方',
    name: '陈合作',
    desc: '公开目录只读预览与授权申请',
    icon: 'lock',
  },
] as const;

export const useSessionStore = defineStore('session', () => {
  const stored = localStorage.getItem(USER_KEY);
  const user = ref<UserSession | null>(stored ? JSON.parse(stored) : null);
  const ready = ref(false);
  const loading = ref(false);
  const error = ref('');

  const isInternal = computed(() => !!user.value && user.value.role !== 'partner');

  function persist(next: UserSession | null) {
    user.value = next;
    if (next) localStorage.setItem(USER_KEY, JSON.stringify(next));
    else localStorage.removeItem(USER_KEY);
  }

  async function login(role: string) {
    loading.value = true;
    error.value = '';
    try {
      const data = await api.login(role);
      persist(data.user);
      ready.value = true;
      return data.user;
    } finally {
      loading.value = false;
    }
  }

  async function bootstrap() {
    if (ready.value) return;
    try {
      if (getToken()) {
        const { user: me } = await api.me();
        persist(me);
      } else {
        await login('designer');
      }
    } catch {
      try {
        await login('designer');
      } catch (bootError) {
        error.value = bootError instanceof Error ? bootError.message : '无法连接构件库服务';
      }
    } finally {
      ready.value = true;
    }
  }

  function logout() {
    setToken('');
    persist(null);
    ready.value = false;
  }

  function can(permission: string): boolean {
    return !!user.value?.permissions.includes(permission);
  }

  const canReview = computed(
    () => user.value?.role === 'reviewer' || user.value?.role === 'asset-admin',
  );
  const canManageSemantics = computed(
    () => user.value?.role === 'standard-admin' || user.value?.role === 'asset-admin',
  );
  const canViewSemantics = computed(
    () => !!user.value && user.value.role !== 'partner',
  );
  const canIngest = computed(
    () => user.value?.role === 'designer' || user.value?.role === 'asset-admin',
  );
  const canAnalytics = computed(
    () => user.value?.role === 'asset-admin' || user.value?.role === 'standard-admin',
  );
  const canManageFamilies = computed(() => user.value?.role === 'asset-admin');

  return {
    user,
    ready,
    loading,
    error,
    isInternal,
    canReview,
    canManageSemantics,
    canViewSemantics,
    canIngest,
    canAnalytics,
    canManageFamilies,
    login,
    bootstrap,
    logout,
    can,
  };
});

export type SessionStore = ReturnType<typeof useSessionStore>;
