import type { SessionStore } from '@/stores/session';

export type NavPermission = 'ingest' | 'review' | 'semantics' | 'analytics' | 'families' | 'internal';

export interface NavItem {
  to: string;
  label: string;
  icon: string;
  permission?: NavPermission;
  hint?: string;
  badge?: number | string;
}

export const HEADER_NAV: NavItem[] = [
  { to: '/', label: '工作台', icon: 'compass' },
  { to: '/library', label: '资产大厅', icon: 'grid' },
  { to: '/ingestion', label: '构件入库', icon: 'upload', permission: 'ingest' },
  { to: '/reviews', label: '审核质检', icon: 'shield', permission: 'review' },
  { to: '/exports', label: '导出中心', icon: 'download' },
];

export const SIDEBAR_PRIMARY: NavItem[] = [
  { to: '/', label: '工作台', icon: 'compass' },
  { to: '/library', label: '全部构件', icon: 'grid' },
  { to: '/favorites', label: '个人收藏', icon: 'star' },
  { to: '/history', label: '下载历史', icon: 'history', permission: 'internal' },
];

export const SIDEBAR_TOOLS: NavItem[] = [
  { to: '/ingestion', label: '构件入库', icon: 'upload', permission: 'ingest' },
  { to: '/families', label: '族库管理', icon: 'grid', permission: 'families' },
  { to: '/reviews', label: '审核质检', icon: 'shield', permission: 'review' },
  { to: '/semantics', label: '构件语义', icon: 'layers', permission: 'semantics' },
  { to: '/analytics', label: '监控看板', icon: 'activity', permission: 'analytics' },
  { to: '/exports', label: '导出中心', icon: 'download' },
];

export function canSeeNav(session: SessionStore, item: NavItem): boolean {
  switch (item.permission) {
    case 'ingest':
      return session.canIngest;
    case 'review':
      return session.canReview;
    case 'semantics':
      return session.canManageSemantics;
    case 'analytics':
      return session.canAnalytics;
    case 'families':
      return session.canManageFamilies;
    case 'internal':
      return session.isInternal;
    default:
      return true;
  }
}

export function visibleNav(session: SessionStore, items: NavItem[]): NavItem[] {
  return items.filter((item) => canSeeNav(session, item));
}
