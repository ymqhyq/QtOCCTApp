/**
 * 外部语义标准中枢：三级优先级解析（构件内覆盖 -> 项目/租户模板 -> hint 回退 -> unmapped）
 * 服务端为唯一真实源，前端沙盒仅呈现。
 */
import type {
  IfcMapping,
  SemanticMapping,
  SemanticProfile,
  SemanticProfileRule,
} from '../shared/types';
import type { InternalComponent } from './seed';
import { TAXONOMY_TREES } from './taxonomy';

const TREE_LABELS = new Map<string, string>();
function walk(nodes: any[], scheme: string) {
  for (const n of nodes) {
    TREE_LABELS.set(`${scheme}:${n.code}`, n.label);
    if (n.children) walk(n.children, scheme);
  }
}
walk(TAXONOMY_TREES.crbim, 'crbim');
walk(TAXONOMY_TREES.gb51269, 'gb51269');
walk(TAXONOMY_TREES.jtg, 'jtg');

function entityFor(code: string, geomType: string): IfcMapping {
  if (geomType === 'wire') {
    return { entity: 'IfcShapeAspect', shapeAspect: 'IfcContourWire', qto: 'Qto_Length' };
  }
  if (geomType === 'face') {
    return { entity: 'IfcShapeAspect', shapeAspect: 'IfcFaceSurface', qto: 'Qto_Area' };
  }
  if (/Girder|BoxGirder|TGirder/.test(code)) {
    return { entity: 'IfcBeam', shapeAspect: 'IfcBeamBody', predefinedType: 'BEAM', qto: 'Qto_BeamBaseQuantities' };
  }
  if (/Slab|Deck/.test(code)) {
    return { entity: 'IfcSlab', shapeAspect: 'IfcSlabBody', predefinedType: 'FLOOR', qto: 'Qto_SlabBaseQuantities' };
  }
  if (/Pile/.test(code)) {
    return { entity: 'IfcPile', shapeAspect: 'IfcPileBody', predefinedType: 'BORED', qto: 'Qto_PileBaseQuantities' };
  }
  if (/PileCap|Footing/.test(code)) {
    return { entity: 'IfcFooting', shapeAspect: 'IfcFootingBody', predefinedType: 'PILE_CAP', qto: 'Qto_FootingBaseQuantities' };
  }
  if (/RetainingWall|Wall/.test(code)) {
    return { entity: 'IfcWall', shapeAspect: 'IfcWallBody', predefinedType: 'SOLIDWALL', qto: 'Qto_WallBaseQuantities' };
  }
  if (/Lining|Tunnel/.test(code)) {
    return { entity: 'IfcTunnel', shapeAspect: 'IfcTunnelLining', qto: 'Qto_TunnelBaseQuantities' };
  }
  if (/Culvert/.test(code)) {
    return { entity: 'IfcPipeSegment', shapeAspect: 'IfcCulvertBody', qto: 'Qto_PipeSegmentBaseQuantities' };
  }
  if (/Bearing/.test(code)) {
    return { entity: 'IfcDiscreteAccessory', shapeAspect: 'IfcBearingBody', qto: 'Qto_DiscreteAccessoryBaseQuantities' };
  }
  return { entity: 'IfcColumn', shapeAspect: 'IfcColumnBody', predefinedType: 'PIER', qto: 'Qto_ColumnBaseQuantities' };
}

function makeRule(component: InternalComponent, shapeKey: string): SemanticProfileRule {
  const shape = component.shapes[shapeKey];
  const ifc = shape.ifc ?? entityFor(shape.code, shape.geomType);
  const crbimLabel = TREE_LABELS.get(`crbim:${component.taxonomy.crbim}`) ?? component.taxonomy.crbim;
  return {
    code: shape.code,
    name: shape.label,
    crbim: `${component.taxonomy.crbim} ${crbimLabel}`,
    gb51269: `${component.taxonomy.gb51269} ${TREE_LABELS.get(`gb51269:${component.taxonomy.gb51269}`) ?? ''}`.trim(),
    jtg: `${component.taxonomy.jtg} ${TREE_LABELS.get(`jtg:${component.taxonomy.jtg}`) ?? ''}`.trim(),
    ifc: { ...ifc, shapeAspect: `${ifc.shapeAspect}#${shapeKey}` },
    qto: ifc.qto,
  };
}

const EXCLUDED_CODES = new Set([
  'Bridge.Accessory.MonitorBracket',
  'Bridge.Accessory.DrainageInlet',
]);

export function buildSemanticProfiles(components: InternalComponent[]): SemanticProfile[] {
  const ruleMap = new Map<string, SemanticProfileRule>();
  for (const component of components) {
    for (const shapeKey of Object.keys(component.shapes)) {
      const shape = component.shapes[shapeKey];
      if (!ruleMap.has(shape.code)) ruleMap.set(shape.code, makeRule(component, shapeKey));
    }
  }
  const allRules = [...ruleMap.values()].sort((a, b) => a.code.localeCompare(b.code));
  const baseRules = allRules.filter((r) => !EXCLUDED_CODES.has(r.code));
  const highwayRules = baseRules.filter((r) => r.code !== 'Bridge.Accessory.DrainageInlet');
  const tenantRules = baseRules
    .filter((r) => !r.code.endsWith('.BottomContour') && !r.code.endsWith('.EndContour'))
    .map((r) =>
      /BodySolid|GirderBody/.test(r.code)
        ? { ...r, ifc: { ...r.ifc, entity: 'IfcMember', shapeAspect: r.ifc.shapeAspect.replace(/IfcColumnBody|IfcBeamBody/, 'IfcMemberBody') } }
        : r,
    );

  const profiles: SemanticProfile[] = [
    {
      profileId: 'Railway-CRBIM-IFC4X3',
      version: '1.4',
      tenantId: 'system',
      name: '铁路工程 CRBIM · IFC 4.3 标准映射',
      description: '中铁 CRBIM 53 表与 buildingSMART IFC 4.3 官方基准模板，全公司默认激活。',
      isDefault: true,
      boundProjects: 3,
      unmappedCount: 0,
      updatedAt: '2026-09-30',
      rules: baseRules,
    },
    {
      profileId: 'Highway-JTG-IFC4X3',
      version: '1.4',
      tenantId: 'system',
      name: '公路工程 JTG · IFC 4.3 标准映射',
      description: 'JTG 公路工程标准体系映射模板，供公路项目一键切换，不影响构件包元数据。',
      isDefault: false,
      boundProjects: 1,
      unmappedCount: 0,
      updatedAt: '2026-08-18',
      rules: highwayRules,
    },
    {
      profileId: 'Tenant-Railway-Override',
      version: '1.2',
      tenantId: 'tenant-railway-17',
      name: '中铁十七局 · 企业专用覆盖模板',
      description: '企业级覆盖：主体实体按 IfcMember 交付，轮廓线不参与外部映射（触发 unmapped 预警）。',
      isDefault: false,
      boundProjects: 2,
      unmappedCount: 0,
      updatedAt: '2026-10-09',
      rules: tenantRules,
    },
  ];

  for (const profile of profiles) {
    const known = new Set(profile.rules.map((r) => r.code));
    profile.unmappedCount = allRules.filter((r) => !known.has(r.code)).length;
  }
  return profiles;
}

export function resolveMappings(
  component: InternalComponent,
  profile: SemanticProfile | undefined,
): SemanticMapping[] {
  const rules = new Map(profile?.rules.map((r) => [r.code, r]) ?? []);
  return Object.entries(component.shapes).map(([shapeKey, shape]) => {
    // 优先级 1：构件内局部覆盖
    if (shape.ifc) {
      return {
        shapeKey,
        code: shape.code,
        name: shape.label,
        status: 'mapped',
        crbim: shape.classifications?.crbim ?? component.taxonomy.crbim,
        gb51269: shape.classifications?.gb51269 ?? component.taxonomy.gb51269,
        ifc: shape.ifc,
        qto: shape.ifc.qto,
        layer: 'local',
      };
    }
    const rule = rules.get(shape.code);
    if (!rule) {
      return {
        shapeKey,
        code: shape.code,
        name: shape.label,
        status: 'unmapped',
        layer: 'unmapped',
      };
    }
    return {
      shapeKey,
      code: shape.code,
      name: shape.label,
      status: 'mapped',
      crbim: rule.crbim,
      gb51269: rule.gb51269,
      ifc: rule.ifc,
      qto: rule.qto,
      layer: profile?.tenantId === 'system' ? 'hint' : 'project',
    };
  });
}

export function allSemanticCodes(components: InternalComponent[]): { code: string; label: string }[] {
  const map = new Map<string, string>();
  for (const c of components) {
    for (const shape of Object.values(c.shapes)) map.set(shape.code, shape.label);
  }
  return [...map.entries()].map(([code, label]) => ({ code, label }));
}
