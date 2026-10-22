import type {
  AnalyticsOverview,
  CatalogResponse,
  ComponentDetail,
  ComponentSearchQuery,
  ComputeJob,
  DownloadFormat,
  DownloadTicket,
  FamilyAdminView,
  GateResult,
  JobStatus,
  Paginated,
  ProjectRef,
  ReviewQueueItem,
  SearchResponse,
  SemanticMapping,
  SemanticProfile,
  SemanticProfileRule,
  TaxonomyScheme,
  UploadGateResponse,
  UserSession,
} from './types';

const TOKEN_KEY = 'cbfc.web.token';

export function getToken(): string {
  return localStorage.getItem(TOKEN_KEY) ?? '';
}
export function setToken(token: string) {
  if (token) localStorage.setItem(TOKEN_KEY, token);
  else localStorage.removeItem(TOKEN_KEY);
}

export class ApiError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
    readonly details?: unknown,
  ) {
    super(message);
    this.name = 'ApiError';
  }
}

function authHeaders(extra: Record<string, string> = {}): Record<string, string> {
  const token = getToken();
  return {
    ...(token ? { Authorization: `Bearer ${token}` } : {}),
    ...extra,
  };
}

async function request<T>(path: string, init: RequestInit = {}): Promise<T> {
  const headers = authHeaders({
    ...(init.body ? { 'Content-Type': 'application/json' } : {}),
    ...((init.headers as Record<string, string>) ?? {}),
  });
  const response = await fetch(path, { ...init, headers });
  const text = await response.text();
  let payload: any = null;
  if (text) {
    try {
      payload = JSON.parse(text);
    } catch {
      payload = text;
    }
  }
  if (!response.ok) {
    throw new ApiError(
      response.status,
      payload?.code ?? `HTTP_${response.status}`,
      payload?.message ?? response.statusText,
      payload?.details,
    );
  }
  return payload as T;
}

function qs(params: Record<string, string | number | undefined | null>): string {
  const search = new URLSearchParams();
  for (const [key, value] of Object.entries(params)) {
    if (value === undefined || value === null || value === '') continue;
    search.set(key, String(value));
  }
  const text = search.toString();
  return text ? `?${text}` : '';
}

export const api = {
  async login(role: string) {
    const data = await request<{ token: string; user: UserSession; expiresIn: number }>(
      '/api/v1/auth/login',
      { method: 'POST', body: JSON.stringify({ role }) },
    );
    setToken(data.token);
    return data;
  },
  me() {
    return request<{ user: UserSession }>('/api/v1/auth/me');
  },
  health() {
    return request<{ status: string; version: string; spec: string; components: number }>('/api/v1/health');
  },
  search(query: ComponentSearchQuery & Record<string, any>) {
    return request<SearchResponse>(`/api/v1/components${qs(query)}`);
  },
  component(familyId: string) {
    return request<ComponentDetail>(`/api/v1/components/${encodeURIComponent(familyId)}`);
  },
  taxonomies() {
    return request<{ items: TaxonomyScheme[] }>('/api/v1/taxonomies');
  },
  catalog() {
    return request<CatalogResponse>('/api/v1/catalog');
  },
  formats(familyId: string) {
    return request<{ familyId: string; formats: (DownloadFormat & { available: boolean; reason: string | null })[] }>(
      `/api/v1/components/${encodeURIComponent(familyId)}/formats`,
    );
  },
  semantics(familyId: string, profileCode?: string) {
    return request<{
      familyId: string;
      profile: { profileId: string; version: string; name: string } | null;
      mappings: SemanticMapping[];
      unmapped: SemanticMapping[];
    }>(`/api/v1/components/${encodeURIComponent(familyId)}/semantics${qs({ profile: profileCode })}`);
  },
  download(familyId: string, format: string, modelNumber?: string) {
    return request<DownloadTicket>(`/api/v1/components/${encodeURIComponent(familyId)}/download`, {
      method: 'POST',
      body: JSON.stringify({ format, modelNumber }),
    });
  },
  exportIfc(familyId: string) {
    return request<{
      downloadUrl: string;
      fileName: string;
      unmapped: { shapeKey: string; code: string }[];
      mappings: SemanticMapping[];
      profile: string | null;
    }>(`/api/v1/components/${encodeURIComponent(familyId)}/export`, {
      method: 'POST',
      body: JSON.stringify({}),
    });
  },
  jobs() {
    return request<Paginated<ComputeJob>>('/api/v1/jobs');
  },
  job(jobId: string) {
    return request<ComputeJob>(`/api/v1/jobs/${encodeURIComponent(jobId)}`);
  },
  cancelJob(jobId: string) {
    return request<{ jobId: string; status: string; backendStatus: JobStatus; message: string }>(
      `/api/v1/jobs/${encodeURIComponent(jobId)}`,
      { method: 'DELETE' },
    );
  },
  jobResultUrl(jobId: string) {
    return `/api/v1/jobs/${encodeURIComponent(jobId)}/result`;
  },
  async jobResult(jobId: string): Promise<ArrayBuffer> {
    const response = await fetch(api.jobResultUrl(jobId), { headers: authHeaders() });
    if (!response.ok) throw new ApiError(response.status, 'JOB_RESULT_FAILED', '任务结果获取失败');
    return response.arrayBuffer();
  },
  async versionGeometry(familyId: string, version: string): Promise<ArrayBuffer> {
    const response = await fetch(
      `/api/v1/components/${encodeURIComponent(familyId)}/versions/${encodeURIComponent(version)}/geometry`,
      { headers: authHeaders() },
    );
    if (!response.ok) throw new ApiError(response.status, 'VERSION_GEOMETRY_FAILED', '版本几何加载失败');
    return response.arrayBuffer();
  },
  reviews() {
    return request<{ items: ReviewQueueItem[]; total: number }>('/api/v1/reviews');
  },
  reviewGeometryUrl(reviewId: string, side: 'old' | 'new') {
    return `/api/v1/reviews/${encodeURIComponent(reviewId)}/geometry?side=${side}`;
  },
  approveReview(reviewId: string) {
    return request<{ ok: boolean; newStatus: string; remaining: number }>(
      `/api/v1/reviews/${encodeURIComponent(reviewId)}/approve`,
      { method: 'POST', body: JSON.stringify({}) },
    );
  },
  rejectReview(reviewId: string, reason: string, defects: string[] = []) {
    return request<{ ok: boolean; newStatus: string; remaining: number }>(
      `/api/v1/reviews/${encodeURIComponent(reviewId)}/reject`,
      { method: 'POST', body: JSON.stringify({ reason, defects }) },
    );
  },
  profiles() {
    return request<{
      items: (SemanticProfile & { ruleCount: number })[];
      active: SemanticProfile;
    }>('/api/v1/semantic-profiles');
  },
  profile(profileId: string) {
    return request<SemanticProfile>(`/api/v1/semantic-profiles/${encodeURIComponent(profileId)}`);
  },
  previewSemantics(body: { shapeKeys?: string[]; profileCode: string; familyId?: string }) {
    return request<{
      profile: { profileId: string; version: string; name: string };
      mappings: SemanticMapping[];
      unmapped: SemanticMapping[];
      resolvedBy: string;
      resolvedAt: string;
    }>('/api/v1/semantic-profiles/preview', { method: 'POST', body: JSON.stringify(body) });
  },
  updateProfileRules(profileId: string, rules: SemanticProfileRule[]) {
    return request<SemanticProfile>(`/api/v1/semantic-profiles/${encodeURIComponent(profileId)}`, {
      method: 'PUT',
      body: JSON.stringify({ rules }),
    });
  },
  analytics() {
    return request<AnalyticsOverview>('/api/v1/analytics/overview');
  },
  projects() {
    return request<{
      items: (ProjectRef & {
        componentCount: number;
        components: { familyId: string; name: string; category: string; status: string; volumeM3: number }[];
      })[];
      total: number;
    }>('/api/v1/projects');
  },
  favorites() {
    return request<{ items: any[]; total: number }>('/api/v1/me/favorites');
  },
  toggleFavorite(familyId: string, remove = false) {
    return request<{ familyId: string; favorited: boolean; total: number }>('/api/v1/me/favorites', {
      method: 'POST',
      body: JSON.stringify({ familyId, action: remove ? 'remove' : 'add' }),
    });
  },
  history() {
    return request<{
      items: { id: string; familyId: string; name: string; action: string; format?: string; at: string; detail?: string }[];
      total: number;
    }>('/api/v1/me/history');
  },
  audit() {
    return request<{ items: any[]; total: number }>('/api/v1/audit');
  },
  handoffToken(familyId: string, cacheKey: string) {
    return request<{ handoffToken: string; expiresIn: number; deepLink: string }>(
      '/api/v1/auth/handoff-token',
      { method: 'POST', body: JSON.stringify({ familyId, cacheKey }) },
    );
  },
  consumeHandoff(handoffToken: string) {
    return request<{ downloadUrl: string; familyId: string; cacheKey: string; tokenType: string }>(
      '/api/v1/auth/handoff-token/consume',
      { method: 'POST', body: JSON.stringify({ handoffToken }) },
    );
  },
  upload(body: {
    fileName: string;
    fileSize: number;
    familyId: string;
    revision?: string;
    declaredCacheKey?: string;
    sourceDigest?: string;
    defects?: string[];
  }) {
    return request<UploadGateResponse>('/api/v1/components/upload', {
      method: 'POST',
      body: JSON.stringify(body),
    });
  },
  submitUpload(uploadId: string) {
    return request<{
      ok: boolean;
      reviewQueued: boolean;
      updated?: boolean;
      reviewId?: string;
      familyId: string;
      toRevision?: string;
    }>(`/api/v1/uploads/${encodeURIComponent(uploadId)}/submit`, {
      method: 'POST',
      body: JSON.stringify({}),
    });
  },
  previewUrl(familyId: string, width = 360, height = 260) {
    const token = getToken();
    return `/api/v1/components/${encodeURIComponent(familyId)}/preview.svg?w=${width}&h=${height}${token ? `&token=${encodeURIComponent(token)}` : ''}`;
  },
  listFamilies() {
    return request<{ items: FamilyAdminView[]; total: number }>('/api/v1/families');
  },
  createFamily(input: {
    templateFamilyId: string;
    familyId: string;
    name: string;
    category?: string;
    semanticCode?: string;
  }) {
    return request<FamilyAdminView>('/api/v1/families', {
      method: 'POST',
      body: JSON.stringify(input),
    });
  },
  importParametricFamily(input: {
    templateFamilyId: string;
    familyId: string;
    name: string;
    revision?: string;
    category?: string;
    semanticCode?: string;
    description?: string;
    inputs?: string[];
    schema: unknown[];
    presets: unknown[];
    shapes?: unknown[];
    generatorSource?: string;
  }) {
    return request<ComponentDetail>('/api/v1/families/import', {
      method: 'POST',
      body: JSON.stringify(input),
    });
  },
  updateFamily(
    familyId: string,
    input: { name?: string; category?: string; semanticCode?: string; status?: string },
  ) {
    return request<FamilyAdminView>(`/api/v1/families/${encodeURIComponent(familyId)}`, {
      method: 'PATCH',
      body: JSON.stringify(input),
    });
  },
  archiveFamily(familyId: string) {
    return request<FamilyAdminView>(`/api/v1/families/${encodeURIComponent(familyId)}`, {
      method: 'DELETE',
    });
  },
  addPreset(
    familyId: string,
    input: {
      modelNumber: string;
      label: string;
      overrides?: Record<string, number | string>;
      drawingCode?: string;
    },
  ) {
    return request<FamilyAdminView>(`/api/v1/families/${encodeURIComponent(familyId)}/presets`, {
      method: 'POST',
      body: JSON.stringify(input),
    });
  },
  updatePreset(
    familyId: string,
    modelNumber: string,
    input: { label?: string; overrides?: Record<string, number | string>; drawingCode?: string },
  ) {
    return request<FamilyAdminView>(
      `/api/v1/families/${encodeURIComponent(familyId)}/presets/${encodeURIComponent(modelNumber)}`,
      { method: 'PATCH', body: JSON.stringify(input) },
    );
  },
  deletePreset(familyId: string, modelNumber: string) {
    return request<FamilyAdminView>(
      `/api/v1/families/${encodeURIComponent(familyId)}/presets/${encodeURIComponent(modelNumber)}`,
      { method: 'DELETE' },
    );
  },
  projectionUrl(
    familyId: string,
    view: 'sheet' | 'front' | 'top' | 'side' | 'iso' = 'sheet',
    options: { hidden?: boolean; dimensions?: boolean; download?: boolean; cacheBust?: number | string } = {},
  ) {
    const params = new URLSearchParams({
      view,
      hidden: options.hidden === false ? '0' : '1',
      dims: options.dimensions === false ? '0' : '1',
      token: getToken(),
    });
    if (options.download) params.set('download', '1');
    if (options.cacheBust !== undefined) params.set('_', String(options.cacheBust));
    return `/api/v1/components/${encodeURIComponent(familyId)}/projection.svg?${params.toString()}`;
  },
};

export type { GateResult };
