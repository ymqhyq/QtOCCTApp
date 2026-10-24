import { createRouter, createWebHistory } from 'vue-router';
import { useSessionStore } from '@/stores/session';

const routes = [
  {
    path: '/login',
    name: 'login',
    component: () => import('@/views/LoginView.vue'),
    meta: { title: '登录', bare: true },
  },
  {
    path: '/',
    name: 'home',
    component: () => import('@/views/HomeView.vue'),
    meta: { title: '工作台' },
  },
  {
    path: '/library',
    name: 'library',
    component: () => import('@/views/LibraryView.vue'),
    meta: { title: '构件资产大厅' },
  },
  {
    path: '/families',
    name: 'families',
    component: () => import('@/views/FamiliesView.vue'),
    meta: { title: '族库管理', permission: 'families' },
  },
  {
    path: '/components/:familyId',
    name: 'component-detail',
    component: () => import('@/views/ComponentDetailView.vue'),
    meta: { title: '构件详情' },
  },
  {
    path: '/components/:familyId/configurator',
    name: 'configurator',
    component: () => import('@/views/ConfiguratorView.vue'),
    meta: { title: '参数配置器', fullBleed: true },
  },
  {
    path: '/exports',
    name: 'exports',
    component: () => import('@/views/ExportCenterView.vue'),
    meta: { title: '导出中心' },
  },
  {
    path: '/ingestion',
    name: 'ingestion',
    component: () => import('@/views/IngestionView.vue'),
    meta: { title: '构件入库', permission: 'ingest' },
  },
  {
    path: '/reviews',
    name: 'reviews',
    component: () => import('@/views/ReviewView.vue'),
    meta: { title: '审核质检', permission: 'review', fullBleed: true },
  },
  {
    path: '/semantics',
    name: 'semantics',
    component: () => import('@/views/SemanticsView.vue'),
    meta: { title: '构件语义', permission: 'semantics-view' },
  },
  {
    path: '/analytics',
    name: 'analytics',
    component: () => import('@/views/AnalyticsView.vue'),
    meta: { title: '监控看板', permission: 'analytics' },
  },
  {
    path: '/favorites',
    name: 'favorites',
    component: () => import('@/views/FavoritesView.vue'),
    meta: { title: '个人收藏' },
  },
  {
    path: '/history',
    name: 'history',
    component: () => import('@/views/HistoryView.vue'),
    meta: { title: '下载与使用历史', permission: 'internal' },
  },
  {
    path: '/:pathMatch(.*)*',
    name: 'not-found',
    component: () => import('@/views/NotFoundView.vue'),
    meta: { title: '页面不存在' },
  },
];

export const router = createRouter({
  history: createWebHistory(),
  routes,
  scrollBehavior(to, from, saved) {
    if (saved) return saved;
    if (to.path === from.path) return false;
    return { top: 0 };
  },
});

router.beforeEach(async (to) => {
  const session = useSessionStore();
  if (!session.ready) await session.bootstrap();
  if (to.meta.bare) {
    if (to.name === 'login' && session.user && !to.query.switch) return { name: 'home' };
    return true;
  }
  if (!session.user) return { name: 'login', query: { redirect: to.fullPath } };
  const permission = to.meta.permission as string | undefined;
  if (permission) {
    const allowed =
      permission === 'review'
        ? session.canReview
        : permission === 'semantics'
          ? session.canManageSemantics
          : permission === 'semantics-view'
            ? session.canViewSemantics
            : permission === 'ingest'
            ? session.canIngest
            : permission === 'analytics'
              ? session.canAnalytics
              : permission === 'families'
                ? session.canManageFamilies
                : permission === 'internal'
                  ? session.isInternal
                  : true;
    if (!allowed) return { name: 'home', query: { denied: to.fullPath } };
  }
  return true;
});

router.afterEach((to) => {
  const title = to.meta.title ? `${to.meta.title} · 铁路工程数字构件库` : '铁路工程数字构件库';
  document.title = title;
});
