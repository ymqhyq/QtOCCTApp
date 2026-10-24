/**
 * Mock 微服务运行时状态（内存态，服务重启即重置）
 * 覆盖：构件库 / 语义模板 / 计算任务状态机 / 下载票据 / 一次性握手票据 /
 *       审核队列 / 项目集合 / 收藏 / 历史 / 审计。
 */
import type {
  ComponentDetail,
  ComponentSummary,
  ComputeJob,
  JobStatus,
  ProjectRef,
  ReviewQueueItem,
  SemanticProfile,
  TaxonomyScheme,
  UserSession,
} from '../shared/types';
import { createComponents, TENANT_ID, type InternalComponent } from './seed';
import { buildSemanticProfiles } from './semantics';
import { buildTaxonomySchemes } from './taxonomy';
import type { JhbEncodeResult } from './jhb';

export interface DownloadTicketRecord {
  token: string;
  userId: string;
  familyId: string;
  format: string;
  expiresAt: number;
  sizeBytes: number;
  checksum: string;
  cacheKey?: string;
  params?: Record<string, number | string | boolean>;
  modelNumber?: string;
}

export interface HandoffRecord {
  token: string;
  userId: string;
  tenantId: string;
  familyId: string;
  cacheKey: string;
  nonce: string;
  expiresAt: number;
  consumed: boolean;
}

export interface InternalJob extends ComputeJob {
  buffer?: JhbEncodeResult;
  ownerId: string;
  timer?: ReturnType<typeof setTimeout>;
  ticker?: ReturnType<typeof setInterval>;
  startedAt?: number;
  completedAt?: number;
}

export interface ReviewRecord extends ReviewQueueItem {
  oldParams: Record<string, number | string | boolean>;
  newParams: Record<string, number | string | boolean>;
  componentFamilyId: string;
  modelNumber: string;
}

export interface HistoryEntry {
  id: string;
  familyId: string;
  name: string;
  action: 'download' | 'evaluate' | 'open' | 'export' | 'handoff';
  format?: string;
  at: string;
  detail?: string;
}

export interface AuditEntry {
  id: string;
  at: string;
  actor: string;
  action: string;
  resource: string;
  detail: string;
  ip: string;
}

export interface AppState {
  components: InternalComponent[];
  profiles: SemanticProfile[];
  taxonomies: TaxonomyScheme[];
  users: UserSession[];
  jobs: Map<string, InternalJob>;
  cache: Map<string, JhbEncodeResult>;
  tickets: Map<string, DownloadTicketRecord>;
  handoffs: Map<string, HandoffRecord>;
  reviews: ReviewRecord[];
  projects: ProjectRef[];
  favorites: Map<string, Set<string>>;
  history: Map<string, HistoryEntry[]>;
  audit: AuditEntry[];
  startedAt: number;
}

export const USERS: UserSession[] = [
  {
    userId: 'u-designer',
    name: '张工',
    email: 'zhang.gong@cr17j.example',
    role: 'designer',
    roleLabel: '设计工程师',
    tenantId: TENANT_ID,
    tenantName: '中铁十七局 · 数字工程中心',
    avatarInitials: '张',
    permissions: ['components:read', 'components:evaluate', 'components:download', 'components:upload', 'projects:read'],
  },
  {
    userId: 'u-reviewer',
    name: '李总工',
    email: 'li.zonggong@cr17j.example',
    role: 'reviewer',
    roleLabel: '标准化审查专家',
    tenantId: TENANT_ID,
    tenantName: '中铁十七局 · 数字工程中心',
    avatarInitials: '李',
    permissions: ['components:read', 'components:evaluate', 'reviews:read', 'reviews:decide', 'components:download'],
  },
  {
    userId: 'u-standard',
    name: '王标准',
    email: 'wang.biaozhun@cr17j.example',
    role: 'standard-admin',
    roleLabel: '标准管理员',
    tenantId: TENANT_ID,
    tenantName: '中铁十七局 · 数字工程中心',
    avatarInitials: '王',
    permissions: ['components:read', 'semantics:read', 'semantics:write', 'semantics:preview', 'components:download'],
  },
  {
    userId: 'u-asset',
    name: '赵管理',
    email: 'zhao.guanli@cr17j.example',
    role: 'asset-admin',
    roleLabel: '资产管理员',
    tenantId: TENANT_ID,
    tenantName: '中铁十七局 · 数字工程中心',
    avatarInitials: '赵',
    permissions: [
      'components:read',
      'components:evaluate',
      'components:download',
      'components:upload',
      'semantics:read',
      'reviews:read',
      'analytics:read',
      'tenants:manage',
      'components:archive',
    ],
  },
  {
    userId: 'u-partner',
    name: '陈合作',
    email: 'chen.hezuo@partner.example',
    role: 'partner',
    roleLabel: '外部合作方',
    tenantId: 'tenant-partner-01',
    tenantName: '中交合作单位（受限只读）',
    avatarInitials: '陈',
    permissions: ['components:read:public', 'components:access-request'],
  },
];

export function toSummary(c: InternalComponent): ComponentSummary {
  const {
    mesh,
    recipeInputs,
    presetCacheKeys,
    declaredCacheKey,
    geoKind,
    legacyUnaligned,
    parameters,
    shapes,
    ports,
    recipe,
    changelog,
    qaReport,
    geometryKind,
    ...summary
  } = c;
  return summary;
}

export function toDetail(c: InternalComponent): ComponentDetail {
  const summary = toSummary(c);
  return {
    ...summary,
    parameters: c.parameters,
    shapes: c.shapes,
    ports: c.ports,
    recipe: c.recipe,
    changelog: c.changelog,
    qaReport: c.qaReport,
    geometryKind: c.geometryKind,
    recipeSource: c.recipeSource ?? c.recipe?.source,
  };
}

function makeReview(
  component: InternalComponent,
  index: number,
  fromRevision: string,
  toRevision: string,
  urgency: 'high' | 'normal' | 'low',
  changedShapes: ReviewQueueItem['changedShapes'],
  paramDiff: ReviewQueueItem['paramDiff'],
  rationale: string,
  oldParams: Record<string, number | string | boolean>,
  newParams: Record<string, number | string | boolean>,
): ReviewRecord {
  return {
    reviewId: `RV-2026-${String(101 + index)}`,
    familyId: component.familyId,
    componentName: component.name,
    submittedBy: ['王工', '李工', '张工'][index % 3],
    submittedAt: new Date(Date.UTC(2026, 9, 19 - index, 9 + index, 20)).toISOString(),
    urgency,
    category: component.category,
    fromRevision,
    toRevision,
    gatesPassed: 6,
    gatesTotal: 6,
    changedShapes,
    paramDiff,
    oldCacheKey: component.changelog[1]?.cacheKey ?? component.geometry.defaultCacheKey,
    newCacheKey: component.geometry.defaultCacheKey,
    volume: { from: Math.round(component.geometry.volumeM3 * 0.94 * 100) / 100, to: component.geometry.volumeM3 },
    unmapped: Object.values(component.shapes)
      .filter((s) => s.code.includes('Monitor') || s.code.includes('Drainage'))
      .map((s) => ({ shapeKey: Object.keys(component.shapes).find((k) => component.shapes[k] === s) ?? '', code: s.code })),
    rationale,
    componentFamilyId: component.familyId,
    modelNumber: String(component.parameters.schema[0].default),
    oldParams,
    newParams,
  };
}

export async function createState(): Promise<AppState> {
  const components = await createComponents();
  const profiles = buildSemanticProfiles(components);
  const taxonomies = buildTaxonomySchemes(components);
  const byId = new Map(components.map((c) => [c.familyId, c]));

  const pierBody = byId.get('PierBody')!;
  const boredPile = byId.get('BoredPile')!;
  const signGantry = byId.get('SignGantry')!;
  const boxGirder = byId.get('BoxGirder')!;
  const pierTray = byId.get('PierTray')!;

  const reviewPier = makeReview(
    pierBody,
    0,
    '1.4.2',
    '1.5.0',
    'high',
    [
      { shapeKey: 'TopBearingFace', kind: 'modified', note: '墩顶垫石安装面新增 20mm 找平凹槽' },
      { shapeKey: 'PierContourWire/1', kind: 'modified', note: '墩底轮廓随坡度 35→34 微调' },
    ],
    [
      { name: 'PierHeight', label: '墩身总高度', from: '12000 mm', to: '12500 mm', changed: true },
      { name: 'Slope', label: '侧面坡度', from: '35.0', to: '34.0', changed: true },
      { name: 'TopWidth', label: '墩顶顺桥向宽度', from: '2400 mm', to: '2400 mm', changed: false },
    ],
    '新版通过 6 道门禁，新增找平凹槽以满足支座安装平整度要求，请复核几何变更与算量差异。',
    { ...pierBody.parameters.presets[1].params },
    { ...pierBody.parameters.presets[1].params, PierHeight: 12500, Slope: 34 },
  );
  const reviewPile = makeReview(
    boredPile,
    1,
    '1.4.2',
    '1.5.0',
    'high',
    [
      { shapeKey: 'ShaftSolid', kind: 'modified', note: '新增桩底扩孔段（直径 1800 → 2200）' },
      { shapeKey: 'TopFace', kind: 'added', note: '新增桩顶凿除面标记' },
    ],
    [
      { name: 'Diameter', label: '桩径', from: '1500 mm', to: '1800 mm', changed: true },
      { name: 'HeightZ', label: '桩长', from: '24000 mm', to: '26000 mm', changed: true },
    ],
    'v1.5 送审：新增桩底扩孔参数与混凝土方量自动校核，请重点复核 GATE-05 散度体积偏差。',
    { ...boredPile.parameters.presets[0].params },
    { ...boredPile.parameters.presets[0].params, Diameter: 1800, HeightZ: 26000 },
  );
  const reviewGantry = makeReview(
    signGantry,
    2,
    '1.2.0',
    '1.3.0',
    'normal',
    [
      { shapeKey: 'BodySolid', kind: 'modified', note: '门架横梁截面加高 200mm' },
      { shapeKey: 'MonitorBracket', kind: 'added', note: '新增监测设备支架（语义未映射）' },
    ],
    [
      { name: 'HeightZ', label: '门架净高', from: '6500 mm', to: '6800 mm', changed: true },
      { name: 'LengthX', label: '门架跨度', from: '12000 mm', to: '12000 mm', changed: false },
    ],
    '新增监测设备支架尚未纳入企业语义映射表，发布前需与标准管理员确认映射码；几何本身通过门禁。',
    { ...signGantry.parameters.presets[0].params },
    { ...signGantry.parameters.presets[0].params, HeightZ: 6800 },
  );
  const reviewGirder = makeReview(
    boxGirder,
    3,
    '1.4.2',
    '1.5.0',
    'normal',
    [{ shapeKey: 'InnerVoidFace', kind: 'modified', note: '内腔壁厚 320 → 300mm，减小自重' }],
    [
      { name: 'WallThickness', label: '壁厚', from: '320 mm', to: '300 mm', changed: true },
      { name: 'HeightZ', label: '梁高', from: '2200 mm', to: '2200 mm', changed: false },
    ],
    '优化箱梁壁厚以降低自重，需复核截面承载力与预拱度计算书（附件已上传）。',
    { ...boxGirder.parameters.presets[0].params },
    { ...boxGirder.parameters.presets[0].params, WallThickness: 300 },
  );
  const reviewTray = makeReview(
    pierTray,
    4,
    '1.4.2',
    '1.5.0',
    'low',
    [{ shapeKey: 'BodySolid', kind: 'modified', note: '托盘收分比例 16% → 18%' }],
    [{ name: 'Taper', label: '收分比例', from: '0.16', to: '0.18', changed: true }],
    '外观与模板匹配性微调，不影响结构计算，建议快速通过。',
    { ...pierTray.parameters.presets[0].params },
    { ...pierTray.parameters.presets[0].params, Taper: 0.18 },
  );

  const projects: ProjectRef[] = [
    {
      projectId: 'PRJ-XY-2026-011',
      name: '西渝高铁安康汉江特大桥',
      code: 'XY-HJ-011',
      description: '主跨 (48+80+48)m 连续梁，引桥 32m 简支梁，含 42 个桥墩构件引用。',
      updatedAt: '2026-10-17',
      owner: '张工',
      componentIds: ['PierBody', 'PierCap', 'PileCap', 'BoredPile', 'BoxGirder', 'BearingPad', 'Guardrail'],
      profile: { profileId: 'Railway-CRBIM-IFC4X3', version: '1.4' },
      rdeFile: 'XY-HJ-011_v12.rde',
      status: 'active',
    },
    {
      projectId: 'PRJ-CK-2026-004',
      name: '成昆复线隧道群',
      code: 'CK-TN-004',
      description: '4 座双线隧道，二次衬砌环与电缆槽构件集，绑定公路标准映射做跨标段交付。',
      updatedAt: '2026-10-12',
      owner: '王工',
      componentIds: ['TunnelLining', 'CableTray', 'RetainingWall'],
      profile: { profileId: 'Highway-JTG-IFC4X3', version: '1.4' },
      rdeFile: 'CK-TN-004_v8.rde',
      status: 'active',
    },
    {
      projectId: 'PRJ-LG-2025-019',
      name: '兰广线路基防护提升工程',
      code: 'LG-RW-019',
      description: '既有线路基边坡与挡土墙加固，引用重力式挡土墙与边坡防护单元。',
      updatedAt: '2026-09-28',
      owner: '李工',
      componentIds: ['RetainingWall', 'SlopeProtection', 'DrainageGrate'],
      profile: { profileId: 'Tenant-Railway-Override', version: '1.2' },
      rdeFile: 'LG-RW-019_v3.rde',
      status: 'active',
    },
  ];

  const favorites = new Map<string, Set<string>>();
  favorites.set('u-designer', new Set(['PierBody', 'BoxGirder', 'PileCap', 'TGirder']));
  favorites.set('u-reviewer', new Set(['PierBody', 'BoredPile']));
  favorites.set('u-standard', new Set(['CulvertBox']));
  favorites.set('u-asset', new Set(['PierBody', 'TunnelLining', 'SignGantry']));
  favorites.set('u-partner', new Set(['BearingPad']));

  const history = new Map<string, HistoryEntry[]>();
  history.set('u-designer', [
    { id: 'h1', familyId: 'PierBody', name: '圆端型实体桥墩墩身', action: 'evaluate', format: 'STD-PIER-12M', at: '2026-10-19T02:40:00Z', detail: '在线变参 · 命中预烘焙' },
    { id: 'h2', familyId: 'BoxGirder', name: '预应力混凝土箱梁', action: 'download', format: 'rcif', at: '2026-10-18T08:12:00Z', detail: '构件制品包 2.4 MB' },
    { id: 'h3', familyId: 'PileCap', name: '钢筋混凝土承台', action: 'handoff', format: 'rcif://', at: '2026-10-17T14:05:00Z', detail: '深链推送至 QtOCCTApp' },
    { id: 'h4', familyId: 'TGirder', name: '预制 T 梁', action: 'open', at: '2026-10-16T09:30:00Z' },
    { id: 'h5', familyId: 'BoredPile', name: '钻孔灌注桩', action: 'export', format: 'ifc', at: '2026-10-15T16:22:00Z', detail: 'IFC 4.3 含 2 项 unmapped' },
  ]);

  const audit: AuditEntry[] = [
    {
      id: 'a1',
      at: '2026-10-19T02:40:00Z',
      actor: '张工',
      action: 'evaluate',
      resource: 'PierBody',
      detail: 'STD-PIER-12M 预烘焙命中，cacheKey 8361c4b4…，耗时 86ms',
      ip: '10.24.6.18',
    },
    {
      id: 'a2',
      at: '2026-10-18T08:12:00Z',
      actor: '张工',
      action: 'download',
      resource: 'BoxGirder',
      detail: '下载 .rcif 构件制品包，票据 TTL 15min',
      ip: '10.24.6.18',
    },
    {
      id: 'a3',
      at: '2026-10-17T14:05:00Z',
      actor: '张工',
      action: 'handoff',
      resource: 'PileCap',
      detail: '签发一次性 handoffToken htk_89f1***，TTL 45s',
      ip: '10.24.6.18',
    },
    {
      id: 'a4',
      at: '2026-10-17T09:44:00Z',
      actor: '李总工',
      action: 'review',
      resource: 'BoredPile',
      detail: '审核队列签收，进入 3D 双模型对比',
      ip: '10.24.7.32',
    },
    {
      id: 'a5',
      at: '2026-10-16T11:20:00Z',
      actor: '王标准',
      action: 'semantic_update',
      resource: 'Tenant-Railway-Override@1.2',
      detail: '更新 3 条企业覆盖映射规则',
      ip: '10.24.8.11',
    },
  ];

  return {
    components,
    profiles,
    taxonomies,
    users: USERS,
    jobs: new Map(),
    cache: new Map(),
    tickets: new Map(),
    handoffs: new Map(),
    reviews: [reviewPier, reviewPile, reviewGantry, reviewGirder, reviewTray],
    projects,
    favorites,
    history,
    audit,
    startedAt: Date.now(),
  };
}

export function touchJob(job: InternalJob, status: JobStatus, progress?: number) {
  job.status = status;
  if (typeof progress === 'number') job.progress = progress;
  job.updatedAt = new Date().toISOString();
}

export function pushAudit(state: AppState, actor: string, action: string, resource: string, detail: string) {
  state.audit.unshift({
    id: crypto.randomUUID(),
    at: new Date().toISOString(),
    actor,
    action,
    resource,
    detail,
    ip: '10.24.6.18',
  });
  state.audit = state.audit.slice(0, 200);
}

export function pushHistory(state: AppState, userId: string, entry: Omit<HistoryEntry, 'id' | 'at'>) {
  const list = state.history.get(userId) ?? [];
  list.unshift({ ...entry, id: crypto.randomUUID(), at: new Date().toISOString() });
  state.history.set(userId, list.slice(0, 60));
}
