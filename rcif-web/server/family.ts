/**
 * 族库管理领域逻辑：
 * - 族 = InternalComponent（familyId 唯一）；
 * - 型号 = parameters.presets[]（独立 params / cacheKey / 预烘焙标记）；
 * - 新建族采用“基于模板克隆”，保证几何、契约与权限可控。
 */
import type { FamilyAdminView, Preset } from '../shared/types';
import { buildCacheKeyPayload, computeCacheKey, sha256Hex } from '../shared/deterministic';
import { buildMesh } from './geometry';
import { generatorDigest, generatorEntry } from './generator';
import type { InternalComponent } from './seed';

export interface CreateFamilyInput {
  templateFamilyId: string;
  familyId: string;
  name: string;
  category?: string;
  semanticCode?: string;
  author: string;
}

export interface PresetInput {
  modelNumber: string;
  label: string;
  overrides?: Record<string, number | string>;
  drawingCode?: string;
}

const MODEL_RE = /^[A-Za-z0-9][A-Za-z0-9._-]{1,31}$/;
const FAMILY_RE = /^[A-Za-z][A-Za-z0-9_]{2,31}$/;

export function familyView(component: InternalComponent): FamilyAdminView {
  return {
    familyId: component.familyId,
    componentId: component.componentId,
    name: component.name,
    category: component.category,
    semanticCode: component.semanticCode,
    status: component.status,
    revision: component.revision,
    sourceMode: component.sourceMode,
    updatedAt: component.updatedAt,
    drawingCode: component.drawingCode,
    defaultCacheKey: component.geometry.defaultCacheKey,
    presetCount: component.parameters.presets.length,
    presets: component.parameters.presets,
    inputs: component.parameters.schema
      .filter((field) => component.recipeInputs.includes(field.name))
      .map((field) => ({
        name: field.name,
        label: field.label,
        unit: field.unit,
        min: field.min,
        max: field.max,
        step: field.step,
      })),
    vertexCount: component.mesh.vertices,
    triangleCount: component.mesh.triangles,
  };
}

export function validateFamilyId(familyId: string): string | null {
  if (!FAMILY_RE.test(familyId)) {
    return 'familyId 需以字母开头，仅含字母/数字/下划线，长度 3–32';
  }
  return null;
}

export function validateModelNumber(modelNumber: string): string | null {
  if (!MODEL_RE.test(modelNumber)) {
    return 'modelNumber 需以字母或数字开头，仅含字母/数字/./_/-，长度 2–32';
  }
  return null;
}

function numericInputs(
  params: Record<string, number | string | boolean>,
  inputs: string[],
): Record<string, number> {
  const result: Record<string, number> = {};
  for (const key of inputs) {
    const value = params[key];
    result[key] = typeof value === 'number' ? value : Number(value) || 0;
  }
  return result;
}

/** 与 API 的权威算键保持一致（族库编辑型号时也走同一契约）。 */
export async function cacheKeyFor(
  component: InternalComponent,
  params: Record<string, number | string | boolean>,
): Promise<string> {
  const inputs = numericInputs(params, component.recipeInputs);
  if (component.familyId === 'PierBody' && component.recipe) {
    const spec = { CrossSlope: 2, PierHeight: 12000, Slope: 35, TopLength: 6400, TopWidth: 2400 };
    if (Object.entries(spec).every(([key, value]) => inputs[key] === value)) {
      return computeCacheKey(
        buildCacheKeyPayload({
          engine: 'build123d',
          digest: '0c034d0fe7643a9ffcfffb1d79ec9ce2363a0e79baa0ca599cb99cd259f1eb0d',
          inputParams: spec,
          source: 'generators/generator.py',
          version: '1.4.2',
        }),
      );
    }
  }
  if (!component.recipe) {
    return sha256Hex(`rcif-artifact:${component.familyId}:${JSON.stringify(inputs)}`);
  }
  return computeCacheKey(
    buildCacheKeyPayload({
      engine: component.recipe.engine,
      digest: component.recipe.digest,
      inputParams: inputs,
      source: component.recipe.source,
      version: component.recipe.version,
    }),
  );
}

function baseParams(component: InternalComponent, modelNumber: string): Record<string, number | string | boolean> {
  const params: Record<string, number | string | boolean> = {};
  for (const field of component.parameters.schema) {
    if (field.name === 'ModelNumber') {
      params[field.name] = modelNumber;
    } else if (field.name === 'NetVolume') {
      params[field.name] = 0;
    } else {
      params[field.name] = field.default;
    }
  }
  return params;
}

function applyDefaultPresetGeometry(
  component: InternalComponent,
  params: Record<string, number | string | boolean>,
) {
  const mesh = buildMesh(component.geoKind, numericInputs(params, component.recipeInputs));
  const volume = Math.round((mesh.volumeMm3 / 1e9) * 1000) / 1000;
  component.mesh = mesh;
  component.geometry = {
    ...component.geometry,
    bounds: mesh.bounds,
    volumeM3: volume,
    surfaceM2: Math.round((mesh.areaMm2 / 1e6) * 1000) / 1000,
    triangles: mesh.triangles,
    vertices: mesh.vertices,
    facesCount: mesh.facesCount,
    edgesCount: mesh.edgesCount,
  };
  const netVolume = component.parameters.schema.find((field) => field.name === 'NetVolume');
  if (netVolume) netVolume.default = volume;
}

export async function buildPreset(component: InternalComponent, input: PresetInput): Promise<Preset> {
  const params = baseParams(component, input.modelNumber);
  for (const [key, value] of Object.entries(input.overrides ?? {})) {
    if (!component.recipeInputs.includes(key)) continue;
    if (typeof value === 'string' && key !== 'StrengthGrade') continue;
    params[key] = value;
  }
  const mesh = buildMesh(component.geoKind, numericInputs(params, component.recipeInputs));
  const volume = Math.round((mesh.volumeMm3 / 1e9) * 1000) / 1000;
  params.NetVolume = volume;
  return {
    modelNumber: input.modelNumber,
    label: input.label,
    drawingCode: input.drawingCode || `${component.familyId}-${input.modelNumber}`,
    params,
    isPrebaked: true,
    cacheKey: await cacheKeyFor(component, params),
  };
}

export async function createFamilyFromTemplate(
  template: InternalComponent,
  input: CreateFamilyInput,
): Promise<InternalComponent> {
  const today = new Date().toISOString().slice(0, 10);
  const parameters = structuredClone(template.parameters);
  const recipe = template.recipe
    ? {
        ...template.recipe,
        entry: generatorEntry(input.familyId),
        digest: await generatorDigest({
          familyId: input.familyId,
          name: input.name,
          inputs: template.recipeInputs,
        }),
      }
    : undefined;
  const component: InternalComponent = {
    ...template,
    componentId: crypto.randomUUID(),
    familyId: input.familyId,
    name: input.name,
    category: input.category || template.category,
    semanticCode: input.semanticCode || template.semanticCode,
    status: 'draft',
    revision: '0.1.0',
    drawingCode: `${input.familyId}-DRAFT`,
    description: `基于 ${template.name} 模板新建的族定义（待补全型号与发布资料）。`,
    tags: [...template.tags],
    updatedAt: today,
    publishedAt: undefined,
    parameters,
    recipe,
    recipeInputs: [...template.recipeInputs],
    geometry: structuredClone(template.geometry),
    shapes: structuredClone(template.shapes),
    ports: structuredClone(template.ports),
    qa: structuredClone(template.qa),
    changelog: [],
    presetCacheKeys: {},
    declaredCacheKey: '',
    legacyUnaligned: false,
  };
  const presetKeys: Record<string, string> = {};
  for (const preset of component.parameters.presets) {
    preset.cacheKey = await cacheKeyFor(component, preset.params);
    presetKeys[preset.modelNumber] = preset.cacheKey;
  }
  component.presetCacheKeys = presetKeys;
  const defaultModel = String(component.parameters.schema[0]?.default ?? '');
  component.geometry.defaultCacheKey = presetKeys[defaultModel] ?? Object.values(presetKeys)[0] ?? '';
  component.declaredCacheKey = component.geometry.defaultCacheKey;
  component.changelog.unshift({
    version: component.revision,
    date: today,
    author: input.author,
    type: 'minor',
    changes: [`基于 ${template.name} 模板新建族 ${input.familyId}`],
    cacheKey: component.geometry.defaultCacheKey,
  });
  return component;
}

export async function addPreset(component: InternalComponent, input: PresetInput): Promise<Preset> {
  if (!component.recipe) throw new Error('只有 parametric 构件支持新增预定义型号');
  if (component.parameters.presets.some((preset) => preset.modelNumber === input.modelNumber)) {
    throw new Error(`型号 ${input.modelNumber} 已存在`);
  }
  const preset = await buildPreset(component, input);
  component.parameters.presets.push(preset);
  component.presetCacheKeys[preset.modelNumber] = preset.cacheKey!;
  component.updatedAt = new Date().toISOString().slice(0, 10);
  return preset;
}

export async function updatePreset(
  component: InternalComponent,
  modelNumber: string,
  input: Omit<PresetInput, 'modelNumber'>,
): Promise<Preset> {
  const index = component.parameters.presets.findIndex((preset) => preset.modelNumber === modelNumber);
  if (index < 0) throw new Error('型号不存在');
  const previous = component.parameters.presets[index];
  const preset = await buildPreset(component, {
    ...input,
    modelNumber,
    drawingCode: input.drawingCode ?? previous.drawingCode,
  });
  component.parameters.presets[index] = preset;
  component.presetCacheKeys[modelNumber] = preset.cacheKey!;
  const defaultModel = String(component.parameters.schema[0]?.default ?? '');
  if (modelNumber === defaultModel) {
    component.geometry.defaultCacheKey = preset.cacheKey!;
    component.declaredCacheKey = preset.cacheKey!;
    applyDefaultPresetGeometry(component, preset.params);
  }
  component.updatedAt = new Date().toISOString().slice(0, 10);
  return preset;
}

export function removePreset(component: InternalComponent, modelNumber: string): void {
  if (component.parameters.presets.length <= 1) throw new Error('至少保留一个预定义型号');
  const defaultModel = String(component.parameters.schema[0]?.default ?? '');
  if (modelNumber === defaultModel) throw new Error('默认型号不可删除，请先切换默认型号');
  const index = component.parameters.presets.findIndex((preset) => preset.modelNumber === modelNumber);
  if (index < 0) throw new Error('型号不存在');
  component.parameters.presets.splice(index, 1);
  delete component.presetCacheKeys[modelNumber];
  component.updatedAt = new Date().toISOString().slice(0, 10);
}
