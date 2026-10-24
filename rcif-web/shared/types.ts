/**
 * RCIF 构件库 —— 前后端共享领域模型
 * 对齐《RCIF 构件数据格式与工程落地规范 v1.4.2》与《B/S 构件库建设方案 v1.4》
 */

export type SourceMode = 'parametric' | 'native-brep' | 'imported-ifc' | 'mesh-only';
export type ComponentStatus = 'draft' | 'reviewing' | 'published' | 'archived';
export type Visibility = 'public' | 'tenant' | 'confidential';
export type GeomType = 'solid' | 'face' | 'wire' | 'shell';

export interface ParamSchema {
  name: string;
  label: string;
  type: 'real' | 'int' | 'enum' | 'bool';
  default: number | string | boolean;
  unit?: string;
  min?: number;
  max?: number;
  step?: number;
  options?: string[];
  access?: 'in' | 'inout' | 'out';
  description?: string;
}

export interface ShapeDef {
  label: string;
  code: string;
  geomType: GeomType;
  parent?: string;
  /** 优先级 1：构件内局部覆盖（整字段替换） */
  ifc?: IfcMapping;
  classifications?: Record<string, string>;
}

export interface IfcMapping {
  entity: string;
  shapeAspect: string;
  predefinedType?: string;
  qto?: string;
}

export interface PortDef {
  label: string;
  origin: [number, number, number];
  axis: [number, number, number];
  refDirection?: [number, number, number];
}

export interface Preset {
  modelNumber: string;
  label: string;
  drawingCode?: string;
  params: Record<string, number | string | boolean>;
  isPrebaked: boolean;
  cacheKey?: string;
}

export interface RecipeInfo {
  engine: string;
  source: string;
  entry: string;
  version: string;
  digest: string;
  inputs: string[];
}

export interface GeometryInfo {
  exactBrep: boolean;
  defaultFormat: string;
  defaultFile: string;
  defaultCacheKey: string;
  bounds: { min: [number, number, number]; max: [number, number, number] };
  volumeM3: number;
  surfaceM2: number;
  triangles: number;
  vertices: number;
  facesCount: number;
  edgesCount: number;
}

export interface ChangelogEntry {
  version: string;
  date: string;
  author: string;
  type: 'major' | 'minor' | 'patch';
  changes: string[];
  cacheKey: string;
}

export interface QaReport {
  passed: boolean;
  executedAt: string;
  gates: GateResult[];
  volumeDivergence: number;
  bRepCheck: string;
  scriptDigest: string;
  extremeTests: { label: string; ok: boolean; note: string }[];
  mesh: { vertices: number; triangles: number; selfIntersections: number };
}

export interface GateResult {
  code: string;
  name: string;
  status: 'passed' | 'failed' | 'running' | 'pending';
  message: string;
  details?: { label: string; value: string; status?: 'ok' | 'warn' | 'error' }[];
  durationMs?: number;
}

export interface FamilyAdminView {
  familyId: string;
  componentId: string;
  name: string;
  category: string;
  semanticCode: string;
  status: ComponentStatus;
  revision: string;
  sourceMode: string;
  updatedAt: string;
  drawingCode: string;
  defaultCacheKey: string;
  presetCount: number;
  presets: Preset[];
  inputs: { name: string; label: string; unit?: string; min?: number; max?: number; step?: number }[];
  vertexCount: number;
  triangleCount: number;
}

export interface ComponentSummary {
  componentId: string;
  familyId: string;
  revision: string;
  name: string;
  drawingCode: string;
  category: string;
  taxonomy: TaxonomyRefs;
  semanticCode: string;
  semanticProfileHint: string;
  sourceMode: SourceMode;
  status: ComponentStatus;
  visibility: Visibility;
  engine: string;
  tags: string[];
  description: string;
  tenantId: string;
  updatedAt: string;
  publishedAt?: string;
  geometry: GeometryInfo;
  presetCount: number;
  keyParams: Record<string, number | string>;
  /** 归一化包围盒尺寸（mm），用于跨类别参数区间过滤 */
  dims: { height: number; length: number; width: number };
  discipline?: string;
  similarity?: number;
  qa: { passed: boolean; passedAt: string; score: number };
  stats: { downloads: number; references: number; rating: number };
  reviewCount?: number;
}

export interface ComponentDetail extends ComponentSummary {
  parameters: { schema: ParamSchema[]; presets: Preset[] };
  shapes: Record<string, ShapeDef>;
  ports: Record<string, PortDef>;
  recipe?: RecipeInfo;
  changelog: ChangelogEntry[];
  qaReport: QaReport;
  geometryKind: string;
  recipeSource?: string;
}

export interface TaxonomyRefs {
  crbim: string;
  gb51269: string;
  jtg: string;
  omniclass?: string;
}

export interface TaxonomyNode {
  key: string;
  code: string;
  label: string;
  count: number;
  children?: TaxonomyNode[];
}

export interface TaxonomyScheme {
  id: string;
  code: string;
  name: string;
  version: string;
  description: string;
  nodes: TaxonomyNode[];
}

export interface FacetBucket {
  value: string;
  label: string;
  count: number;
}

export interface CatalogPreset {
  modelNumber: string;
  label: string;
  volumeM3: number;
  isPrebaked: boolean;
}

export interface CatalogFamily {
  familyId: string;
  name: string;
  category: string;
  discipline: string;
  semanticCode: string;
  drawingCode: string;
  sourceMode: SourceMode;
  status: ComponentStatus;
  taxonomy: TaxonomyRefs;
  presets: CatalogPreset[];
}

export interface CatalogResponse {
  items: CatalogFamily[];
  total: number;
}

export interface SearchResponse {
  items: ComponentSummary[];
  total: number;
  page: number;
  pageSize: number;
  facets: {
    sourceModes: FacetBucket[];
    categories: FacetBucket[];
    disciplines: FacetBucket[];
    statuses: FacetBucket[];
    tags: FacetBucket[];
    parameterRanges: Record<string, { min: number; max: number; unit: string }>;
  };
  similarityReference?: { familyId: string; name: string };
  diagnostics?: {
    blockedBy: string;
    suggestion: string;
    relaxableCount: number;
  };
  tookMs: number;
}

export interface SemanticMapping {
  shapeKey: string;
  code: string;
  name: string;
  status: 'mapped' | 'unmapped';
  crbim?: string;
  gb51269?: string;
  ifc?: IfcMapping;
  qto?: string;
  layer: 'local' | 'project' | 'hint' | 'unmapped';
}

export interface SemanticProfileRule {
  code: string;
  name: string;
  crbim: string;
  gb51269?: string;
  jtg?: string;
  ifc: IfcMapping;
  qto?: string;
}

export interface SemanticProfile {
  profileId: string;
  version: string;
  tenantId: 'system' | string;
  name: string;
  description: string;
  isDefault: boolean;
  boundProjects: number;
  unmappedCount: number;
  updatedAt: string;
  rules: SemanticProfileRule[];
  ruleCount?: number;
}

export type JobStatus =
  | 'queued'
  | 'running'
  | 'detached_computing'
  | 'succeeded'
  | 'failed'
  | 'cancelled_by_user';

export interface ComputeJob {
  jobId: string;
  familyId: string;
  componentName: string;
  status: JobStatus;
  progress: number;
  estimatedMs: number;
  createdAt: string;
  updatedAt: string;
  params?: Record<string, number | string | boolean>;
  cacheKey?: string;
  resultUrl?: string;
  error?: string;
  cancelRequested?: boolean;
  isCacheHit?: boolean;
  finishedAt?: string;
}

export interface JobAccepted {
  jobId: string;
  statusUrl: string;
  estimatedMs: number;
}

export interface EvaluateRequest {
  modelNumber: string;
  parameterOverrides: Record<string, number | string | boolean>;
  acceptFormats?: string[];
}

export interface DownloadFormat {
  id: string;
  ext: string;
  title: string;
  mime: string;
  sizeMB: number;
  description: string;
  scenario: string;
  scriptIncluded: boolean;
  priority: 'P0' | 'P1';
}

export interface DownloadTicket {
  token: string;
  url: string;
  expiresAt: string;
  fileName: string;
  format: string;
  sizeBytes: number;
  checksum: string;
  permission: 'granted' | 'request-required' | 'denied';
  message?: string;
}

export interface HandoffToken {
  handoffToken: string;
  expiresIn: number;
  deepLink: string;
}

export interface ReviewQueueItem {
  reviewId: string;
  familyId: string;
  componentName: string;
  submittedBy: string;
  submittedAt: string;
  urgency: 'high' | 'normal' | 'low';
  category: string;
  fromRevision: string;
  toRevision: string;
  gatesPassed: number;
  gatesTotal: number;
  changedShapes: { shapeKey: string; kind: 'added' | 'removed' | 'modified'; note: string }[];
  paramDiff: { name: string; label: string; from: string; to: string; changed: boolean }[];
  oldCacheKey: string;
  newCacheKey: string;
  volume: { from: number; to: number };
  unmapped: { shapeKey: string; code: string }[];
  rationale: string;
}

export interface KpiSet {
  totalAssets: number;
  bySourceMode: Record<SourceMode, number>;
  cacheHitRate: number;
  gateFirstPassRate: number;
  unmappedCount: number;
  published: number;
  reviewing: number;
  draft: number;
}

export interface TimeseriesPoint {
  date: string;
  value: number;
}

export interface AnalyticsOverview {
  kpi: KpiSet;
  reuseTrend: TimeseriesPoint[];
  workerLatency: { bucket: string; p50: number; p95: number; count: number }[];
  failures: ComputeJob[];
  reviewBacklog: ReviewQueueItem[];
  storage: { casDedupRate: number; artifacts: number; totalGB: number; authoritativeGB: number };
  ingestionTrend: TimeseriesPoint[];
}

export interface ProjectRef {
  projectId: string;
  name: string;
  code: string;
  description: string;
  updatedAt: string;
  owner: string;
  componentIds: string[];
  profile: { profileId: string; version: string };
  rdeFile: string;
  status: 'active' | 'archived';
}

export interface UserSession {
  userId: string;
  name: string;
  email: string;
  role: 'designer' | 'reviewer' | 'standard-admin' | 'asset-admin' | 'partner';
  roleLabel: string;
  tenantId: string;
  tenantName: string;
  avatarInitials: string;
  permissions: string[];
}

export interface Paginated<T> {
  items: T[];
  total: number;
  page: number;
  pageSize: number;
}

export interface UploadGateResponse {
  uploadId: string;
  fileName: string;
  fileSize: number;
  familyId: string;
  revision: string;
  gates: GateResult[];
  passed: boolean;
  demo?: boolean;
  report: QaReport;
  detected: {
    engine: string;
    sourceMode: SourceMode;
    exactBrep: boolean;
    shapes: string[];
    cacheKey: string;
    declaredCacheKey: string;
  };
  durationMs: number;
}

export interface ApiErrorBody {
  code: string;
  message: string;
  details?: unknown;
}

export interface ComponentSearchQuery {
  q?: string;
  scheme?: string;
  taxonomy?: string;
  discipline?: string;
  familyId?: string;
  similarTo?: string;
  sourceMode?: SourceMode | 'all';
  status?: ComponentStatus | 'all';
  paramFilters?: Record<string, { min?: number; max?: number }>;
  sort?: 'latest' | 'name' | 'volume' | 'popular';
  page?: number;
  pageSize?: number;
}
