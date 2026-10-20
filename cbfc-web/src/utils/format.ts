export function formatNumber(value: number | null | undefined, digits = 0): string {
  if (value === null || value === undefined || Number.isNaN(value)) return '—';
  return new Intl.NumberFormat('zh-CN', {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits,
  }).format(value);
}

export function formatVolume(value: number | null | undefined): string {
  if (value === null || value === undefined) return '—';
  return `${formatNumber(value, value >= 100 ? 1 : 2)} m³`;
}

export function formatArea(value: number | null | undefined): string {
  if (value === null || value === undefined) return '—';
  return `${formatNumber(value, value >= 100 ? 1 : 2)} m²`;
}

export function formatLength(value: number | null | undefined): string {
  if (value === null || value === undefined) return '—';
  if (Math.abs(value) >= 1000) return `${formatNumber(value / 1000, value % 1000 === 0 ? 0 : 2)} m`;
  return `${formatNumber(value, 0)} mm`;
}

export function formatMm(value: number | null | undefined): string {
  if (value === null || value === undefined) return '—';
  return `${formatNumber(value, 0)} mm`;
}

export function formatBytes(bytes: number | null | undefined): string {
  if (!bytes) return '—';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
}

export function formatPercent(value: number | null | undefined, digits = 1): string {
  if (value === null || value === undefined) return '—';
  return `${value.toFixed(digits)}%`;
}

export function formatDate(iso: string | null | undefined): string {
  if (!iso) return '—';
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return iso;
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
}

export function formatDateTime(iso: string | null | undefined): string {
  if (!iso) return '—';
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return iso;
  return `${formatDate(iso)} ${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
}

export function relativeTime(iso: string | null | undefined): string {
  if (!iso) return '—';
  const d = new Date(iso).getTime();
  if (Number.isNaN(d)) return iso;
  const delta = Date.now() - d;
  if (delta < 60_000) return '刚刚';
  if (delta < 3_600_000) return `${Math.floor(delta / 60_000)} 分钟前`;
  if (delta < 86_400_000) return `${Math.floor(delta / 3_600_000)} 小时前`;
  if (delta < 30 * 86_400_000) return `${Math.floor(delta / 86_400_000)} 天前`;
  return formatDate(iso);
}

export function maskKey(key: string | null | undefined, head = 8, tail = 4): string {
  if (!key) return '—';
  if (key.length <= head + tail) return key;
  return `${key.slice(0, head)}…${key.slice(-tail)}`;
}

export function shortId(id: string | null | undefined, head = 12): string {
  if (!id) return '—';
  return id.length > head ? `${id.slice(0, head)}…` : id;
}

export type Tone = 'success' | 'warning' | 'danger' | 'info' | 'muted' | 'primary';

export function componentStatusMeta(status: string): { label: string; tone: Tone } {
  switch (status) {
    case 'published':
      return { label: '已发布', tone: 'success' };
    case 'reviewing':
      return { label: '审核中', tone: 'warning' };
    case 'draft':
      return { label: '草稿', tone: 'muted' };
    case 'archived':
      return { label: '已归档', tone: 'muted' };
    default:
      return { label: status, tone: 'muted' };
  }
}

export function jobStatusMeta(status: string): { label: string; tone: Tone } {
  switch (status) {
    case 'queued':
      return { label: '排队中', tone: 'info' };
    case 'running':
      return { label: '求解中', tone: 'primary' };
    case 'detached_computing':
      return { label: '后台脱机计算', tone: 'warning' };
    case 'succeeded':
      return { label: '已完成', tone: 'success' };
    case 'failed':
      return { label: '失败', tone: 'danger' };
    case 'cancelled_by_user':
      return { label: '已取消', tone: 'muted' };
    default:
      return { label: status, tone: 'muted' };
  }
}

export function sourceModeMeta(mode: string): { label: string; tone: Tone; hint: string } {
  switch (mode) {
    case 'parametric':
      return { label: '参数化构件', tone: 'primary', hint: '支持在线变参求解' };
    case 'native-brep':
      return { label: '原生 BRep', tone: 'info', hint: '固定精确几何，只读参数' };
    case 'imported-ifc':
      return { label: 'IFC 导入', tone: 'muted', hint: '外部标准导入，只读预览' };
    case 'mesh-only':
      return { label: '网格体', tone: 'muted', hint: '仅可视化，不支持精确算量' };
    default:
      return { label: mode, tone: 'muted', hint: '' };
  }
}

export function visibilityMeta(visibility: string): { label: string; tone: Tone } {
  switch (visibility) {
    case 'public':
      return { label: '公开', tone: 'success' };
    case 'tenant':
      return { label: '租户内', tone: 'info' };
    case 'confidential':
      return { label: '受保密', tone: 'danger' };
    default:
      return { label: visibility, tone: 'muted' };
  }
}

export function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value));
}

export function debounce<T extends (...args: any[]) => void>(fn: T, wait: number) {
  let timer: ReturnType<typeof setTimeout> | null = null;
  return (...args: Parameters<T>) => {
    if (timer) clearTimeout(timer);
    timer = setTimeout(() => fn(...args), wait);
  };
}

export async function copyText(text: string): Promise<boolean> {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch {
    const textarea = document.createElement('textarea');
    textarea.value = text;
    textarea.style.position = 'fixed';
    textarea.style.opacity = '0';
    document.body.appendChild(textarea);
    textarea.select();
    const ok = document.execCommand('copy');
    document.body.removeChild(textarea);
    return ok;
  }
}

export function triggerDownload(url: string, fileName?: string) {
  const anchor = document.createElement('a');
  anchor.href = url;
  if (fileName) anchor.download = fileName;
  anchor.rel = 'noopener';
  document.body.appendChild(anchor);
  anchor.click();
  document.body.removeChild(anchor);
}

export function cleanDisplayName(name: string | null | undefined): string {
  return String(name ?? '')
    .replace(/\s*[（(](?:CQ\s*脚本|构件库2)[）)]\s*/g, '')
    .trim();
}

export function parseNumberInput(value: string): number | null {
  if (value.trim() === '') return null;
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

export function downloadUrlFor(ticketUrl: string): string {
  return ticketUrl;
}
