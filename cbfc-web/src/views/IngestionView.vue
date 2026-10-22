<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { strFromU8, unzipSync } from 'fflate';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import CadViewport from '@/components/viewer/CadViewport.vue';
import UploadedModelView from '@/components/ingestion/UploadedModelView.vue';
import { api } from '@/api/client';
import type { ComponentSummary, GateResult, TaxonomyNode, TaxonomyScheme, UploadGateResponse } from '@/api/types';
import type { DecodedJhb } from '@/jhb/decode';
import { decodeGlbAsJhb } from '@/utils/glbToJhb';
import { useEvaluation } from '@/composables/useEvaluation';
import { useSessionStore } from '@/stores/session';
import { useUiStore } from '@/stores/ui';
import {
  formatArea,
  formatBytes,
  formatNumber,
  formatVolume,
  maskKey,
  sourceModeMeta,
  triggerDownload,
} from '@/utils/format';

const session = useSessionStore();
const ui = useUiStore();
const route = useRoute();
const evaluation = useEvaluation();
const { decoded, status, progress, error: evalError } = evaluation;
const router = useRouter();
const debugMode = computed(() => route.query.debug === '1');

const components = ref<ComponentSummary[]>([]);
const schemes = ref<TaxonomyScheme[]>([]);
const templateCode = ref('');
const familyId = ref('PierBody');
const uploadedMeta = ref<{
  familyId: string;
  name: string;
  revision: string;
  category: string;
  semanticCode: string;
  sourceMode: string;
  engine: string;
  volumeM3: number;
  vertices: number;
  triangles: number;
  shapes: { key: string; label: string; node?: string }[];
  presetCount: number;
} | null>(null);
const uploadedDecoded = ref<DecodedJhb | null>(null);
const uploadedPart = ref<string | null>(null);
const uploadedPreviewUrl = ref('');
const packageError = ref('');
const revision = ref('1.4.2');
const file = ref<File | null>(null);
const fileHash = ref('');
const hashing = ref(false);
const dragOver = ref(false);
const running = ref(false);
const result = ref<UploadGateResponse | null>(null);
const revealed = ref(0);
const revealTimer = ref<ReturnType<typeof setInterval> | null>(null);
const submitting = ref(false);
const submittedReview = ref<{
  reviewId?: string;
  familyId: string;
  toRevision?: string;
  updated?: boolean;
} | null>(null);
const defects = ref<string[]>([]);
const showDefects = ref(false);
const selectedGate = ref<string | null>(null);

const defectOptions = [
  { id: 'checksums', label: 'checksums.json 失配', gate: 'GATE-02' },
  { id: 'script', label: '脚本引用 subprocess', gate: 'GATE-03' },
  { id: 'schema', label: 'Schema 缺必填字段', gate: 'GATE-04' },
  { id: 'manifold', label: '几何非流形开边', gate: 'GATE-05' },
  { id: 'cachekey', label: '算键与声明不一致', gate: 'GATE-06' },
  { id: 'zip', label: '目录穿越风险', gate: 'GATE-01' },
];

const target = computed(() => components.value.find((c) => c.familyId === familyId.value) ?? null);
const diagnosticDecoded = computed(() => uploadedDecoded.value ?? decoded.value);
const diagnosticStatus = computed(() => (uploadedDecoded.value ? 'succeeded' : status.value));
const diagnosticParts = computed(() => uploadedMeta.value?.shapes ?? []);
type TemplateOption = { code: string; label: string; depth: number };
function flattenTemplateNodes(nodes: TaxonomyNode[], depth = 0, output: TemplateOption[] = []): TemplateOption[] {
  for (const node of nodes) {
    if (node.children?.length) flattenTemplateNodes(node.children, depth + 1, output);
    else output.push({ code: node.code, label: node.label, depth });
  }
  return output;
}
const templateOptions = computed(() => {
  const crbim = schemes.value.find((scheme) => scheme.id === 'crbim');
  return crbim ? flattenTemplateNodes(crbim.nodes) : [];
});
const templateLabel = computed(() => templateOptions.value.find((item) => item.code === templateCode.value)?.label ?? '');
const templateHasFamily = computed(
  () => !templateCode.value || components.value.some((component) => component.taxonomy.crbim === templateCode.value),
);
const passedCount = computed(() => result.value?.gates.filter((g) => g.status === 'passed').length ?? 0);
const firstFailure = computed(() => result.value?.gates.find((g) => g.status === 'failed') ?? null);
const pipelineProgress = computed(() => (result.value ? Math.round((revealed.value / 6) * 100) : 0));
const verdict = computed(() => {
  if (!result.value || revealed.value < 6) return null;
  return result.value.passed ? 'passed' : 'failed';
});

function gateView(gate: GateResult, index: number): GateResult {
  if (revealed.value > index) return gate;
  if (running.value && revealed.value === index) {
    return { ...gate, status: 'running', message: '正在执行自动化校验…', details: [] };
  }
  return { ...gate, status: 'pending', message: '等待上游门禁完成', details: [] };
}

function toggleDefect(id: string) {
  defects.value = defects.value.includes(id)
    ? defects.value.filter((d) => d !== id)
    : [...defects.value, id];
}

function onTemplateChange() {
  const matched = components.value.find((component) => component.taxonomy.crbim === templateCode.value);
  if (matched) familyId.value = matched.familyId;
}

const TEMPLATE_ALIASES: { keyword: string; label: string }[] = [
  { keyword: 'abutment', label: '桥台' },
  { keyword: 'pier', label: '桥墩' },
  { keyword: 'pile', label: '桩基' },
  { keyword: 'cap', label: '承台' },
  { keyword: 'girder', label: '梁' },
  { keyword: 'bearing', label: '支座' },
  { keyword: 'culvert', label: '涵洞' },
  { keyword: 'tunnel', label: '隧道' },
  { keyword: 'retaining', label: '挡土墙' },
  { keyword: 'foundation', label: '基础' },
];

function matchTemplateFromPackage(meta: NonNullable<typeof uploadedMeta.value>) {
  const text = [meta.familyId, meta.name, meta.semanticCode, meta.category].join(' ').toLowerCase();
  let best: { code: string; score: number } | null = null;
  for (const option of templateOptions.value) {
    let score = 0;
    if (text.includes(option.label.toLowerCase())) score += 12;
    for (const alias of TEMPLATE_ALIASES) {
      if (text.includes(alias.keyword) && option.label.includes(alias.label)) score += 8;
    }
    if (score > (best?.score ?? 0)) best = { code: option.code, score };
  }
  if (best) {
    templateCode.value = best.code;
    const matched = components.value.find((component) => component.taxonomy.crbim === best!.code);
    if (matched) familyId.value = matched.familyId;
  }
}

async function parsePackage(buffer: ArrayBuffer, selected: File) {
  packageError.value = '';
  uploadedMeta.value = null;
  uploadedDecoded.value = null;
  uploadedPart.value = null;
  if (uploadedPreviewUrl.value) URL.revokeObjectURL(uploadedPreviewUrl.value);
  uploadedPreviewUrl.value = '';
  try {
    const entries = unzipSync(new Uint8Array(buffer));
    const componentEntry = entries['component.json'];
    if (!componentEntry) {
      packageError.value = '制品包缺少 component.json，无法读取构件内容。';
      return;
    }
    const componentJson = JSON.parse(strFromU8(componentEntry));
    const identity = componentJson.identity ?? {};
    const geometry = componentJson.geometry ?? {};
    const parameters = componentJson.parameters ?? {};
    const shapes = componentJson.shapes ?? {};
    const shapeList = Object.entries(shapes).map(([key, value]) => {
      const shape = value as { label?: string; node?: string };
      return { key, label: shape.label ?? key, node: shape.node };
    });
    const meta = {
      familyId: String(identity.familyId ?? selected.name.replace(/_[\d.]+\.cbfc$/i, '')),
      name: String(identity.name ?? identity.familyId ?? selected.name),
      revision: String(identity.revision ?? '1.0.0'),
      category: String(identity.category ?? ''),
      semanticCode: String(identity.semanticCode ?? ''),
      sourceMode: String(identity.sourceMode ?? ''),
      engine: String(componentJson.recipe?.engine ?? ''),
      volumeM3: Number(geometry.volumeM3 ?? 0),
      vertices: Number(parameters.vertices ?? 0),
      triangles: Number(parameters.triangles ?? 0),
      shapes: shapeList,
      presetCount: Array.isArray(parameters.presets) ? parameters.presets.length : 0,
    };
    uploadedMeta.value = meta;
    matchTemplateFromPackage(meta);
    const glb = entries['display/preview.glb'] ?? entries['preview.glb'];
    if (glb) {
      uploadedPreviewUrl.value = URL.createObjectURL(new Blob([glb], { type: 'model/gltf-binary' }));
      try {
        const glbBuffer = glb.buffer.slice(glb.byteOffset, glb.byteOffset + glb.byteLength);
        uploadedDecoded.value = await decodeGlbAsJhb(glbBuffer, shapeList, {
          cacheKey: String(geometry.defaultCacheKey ?? ''),
          name: meta.name,
        });
      } catch {
        /* GLB 解析失败时退回独立查看器 */
      }
    }
  } catch (error) {
    packageError.value = error instanceof Error ? `制品包解析失败：${error.message}` : '制品包解析失败';
  }
}

async function readFile(selected: File) {
  file.value = selected;
  revision.value = '1.4.2';
  result.value = null;
  submittedReview.value = null;
  revealed.value = 0;
  hashing.value = true;
  fileHash.value = '';
  try {
    const buffer = await selected.arrayBuffer();
    const digest = await crypto.subtle.digest('SHA-256', buffer);
    fileHash.value = [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, '0')).join('');
    await parsePackage(buffer, selected);
  } catch {
    fileHash.value = '';
    packageError.value = '文件读取失败';
  } finally {
    hashing.value = false;
    await runGates();
  }
}

function onFileChange(event: Event) {
  const selected = (event.target as HTMLInputElement).files?.[0];
  if (selected) void readFile(selected);
}

function onDrop(event: DragEvent) {
  dragOver.value = false;
  const selected = event.dataTransfer?.files?.[0];
  if (selected) void readFile(selected);
}

function startReveal() {
  if (revealTimer.value) clearInterval(revealTimer.value);
  revealed.value = 0;
  revealTimer.value = setInterval(() => {
    if (revealed.value >= 6) {
      if (revealTimer.value) clearInterval(revealTimer.value);
      revealTimer.value = null;
      return;
    }
    revealed.value += 1;
  }, 340);
}

async function runGates() {
  if (!file.value) {
    ui.notify({ type: 'warning', title: '请先拖入构件包', message: '支持 .cbfc 制品包（ZIP）。' });
    return;
  }
  running.value = true;
  result.value = null;
  submittedReview.value = null;
  try {
    result.value = await api.upload({
      fileName: file.value.name,
      fileSize: file.value.size,
      familyId: familyId.value,
      revision: revision.value,
      sourceDigest: fileHash.value || undefined,
      defects: defects.value,
    });
    startReveal();
    const first = result.value.gates.find((g) => g.status === 'failed');
    if (first) {
      ui.notify({ type: 'error', title: `${first.code} 门禁阻断`, message: first.message });
    } else {
      ui.notify({ type: 'success', title: '6 道门禁全部通过', message: '可提交标准化委员会进行技术发布审核。' });
    }
  } catch (error) {
    ui.notify({ type: 'error', title: '门禁诊断失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    running.value = false;
  }
}

async function submitReview() {
  if (!result.value) return;
  submitting.value = true;
  try {
    const response = await api.submitUpload(result.value.uploadId);
    submittedReview.value = {
      reviewId: response.reviewId,
      familyId: response.familyId,
      toRevision: response.toRevision,
      updated: response.updated,
    };
    ui.notify({
      type: 'success',
      title: response.updated ? '已刷新既有审核任务' : session.canReview ? '已提交标准化委员会' : '已提交发布审核',
      message: response.updated
        ? `${response.familyId} 的待审模型、版本与权威算键已更新为本次上传，审查专家将重新加载最新几何。`
        : `${response.familyId} 已进入待审队列，审核专家将进行双模型 3D 对比与签批。`,
      actionLabel: session.canReview ? '前往审核台' : '查看构件',
      action: () =>
        void router.push(
          session.canReview ? `/reviews?familyId=${encodeURIComponent(response.familyId)}` : `/components/${response.familyId}`,
        ),
    });
  } catch (error) {
    ui.notify({ type: 'error', title: '提交失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    submitting.value = false;
  }
}

function downloadReport() {
  if (!result.value) return;
  const blob = new Blob([JSON.stringify({ ...result.value, report: { ...result.value.report, gates: result.value.gates } }, null, 2)], {
    type: 'application/json',
  });
  const url = URL.createObjectURL(blob);
  triggerDownload(url, `${result.value.familyId}_GATE_REPORT.json`);
  setTimeout(() => URL.revokeObjectURL(url), 2000);
  ui.notify({ type: 'success', title: '完整质检报告已下载' });
}

function clearFile() {
  file.value = null;
  fileHash.value = '';
  result.value = null;
  submittedReview.value = null;
  revealed.value = 0;
  defects.value = [];
  uploadedMeta.value = null;
  uploadedDecoded.value = null;
  uploadedPart.value = null;
  packageError.value = '';
  if (uploadedPreviewUrl.value) URL.revokeObjectURL(uploadedPreviewUrl.value);
  uploadedPreviewUrl.value = '';
}

function loadTargetGeometry() {
  evaluation.loadGeometry(familyId.value);
}

watch(familyId, () => {
  if (components.value.length) loadTargetGeometry();
});

onMounted(async () => {
  try {
    const [search, taxonomyResponse] = await Promise.all([api.search({ pageSize: 60 }), api.taxonomies()]);
    components.value = search.items;
    schemes.value = taxonomyResponse.items;
    const current = components.value.find((component) => component.familyId === familyId.value);
    if (current) templateCode.value = current.taxonomy.crbim;
    loadTargetGeometry();
  } catch {
    /* ignore */
  }
});

onBeforeUnmount(() => {
  if (revealTimer.value) clearInterval(revealTimer.value);
  if (uploadedPreviewUrl.value) URL.revokeObjectURL(uploadedPreviewUrl.value);
});
</script>

<template>
  <div class="mx-auto max-w-[1500px] px-4 py-4 xl:px-6">
    <div class="flex flex-wrap items-end justify-between gap-3">
      <div>
        <div class="flex items-center gap-1.5 text-[10.5px] text-[#9CA3AF]">
          <RouterLink to="/" class="hover:text-primary">工作台</RouterLink>
          <AppIcon name="chevronRight" :size="10" />
          <span class="text-[#4B5563]">构件入库</span>
        </div>
        <h1 class="mt-1 flex items-center gap-2 text-[20px] font-semibold tracking-tight text-ink">
          构件入库
        </h1>
        <p class="mt-0.5 text-[12px] text-muted">
          上传 .cbfc 制品包，完成 6 道自动化门禁校验后提交发布审核。
        </p>
      </div>
      <div class="flex items-center gap-1.5">
        <span class="flex items-center gap-1 rounded-full border border-line bg-surface px-2 py-1 text-[10px] text-muted">
          <AppIcon name="shield" :size="11" />6 道自动化门禁
        </span>
      </div>
    </div>

      <div class="mt-3 rounded-md border border-[#FDE68A] bg-[#FFFBEB] px-3 py-2 text-[10.5px] leading-relaxed text-[#92400E]">
        当前为 Mock 门禁演示：服务端只接收文件元数据并模拟 6 道检查，不会解压或解析真实 .cbfc/ZIP。生产环境必须接入服务端文件校验。
      </div>

      <div v-if="submittedReview" class="mt-3 rounded-md border border-[#BBF7D0] bg-[#F0FDF4] px-3 py-2.5">
        <div class="flex flex-wrap items-start justify-between gap-3">
          <div class="min-w-0">
            <p class="flex items-center gap-1.5 text-[12px] font-semibold text-[#166534]">
              <AppIcon name="checkCircle" :size="14" />{{ submittedReview.updated ? '已刷新既有审核任务' : '已提交发布审核' }}
            </p>
            <p class="mt-1 text-[10.5px] leading-relaxed text-[#166534]/80">
              <span class="mono">{{ submittedReview.familyId }}</span>
              <template v-if="submittedReview.reviewId"> · 审核任务 <span class="mono">{{ submittedReview.reviewId }}</span></template>
              <template v-if="submittedReview.toRevision"> · 目标版本 v{{ submittedReview.toRevision }}</template>
            </p>
            <p class="mt-1 text-[10.5px] leading-relaxed text-[#4B5563]">
              <template v-if="session.canReview">下一步：前往「审核质检」找到该构件，完成双模型对比与签批。</template>
              <template v-else>下一步：等待标准化审查专家签批；审核通过后，构件状态会更新为「已发布」。</template>
            </p>
          </div>
          <div class="flex shrink-0 flex-wrap gap-1.5">
            <RouterLink v-if="session.canReview" :to="`/reviews?familyId=${encodeURIComponent(submittedReview.familyId)}`" class="btn btn-primary btn-sm">
              <AppIcon name="shield" :size="12" />前往审核质检
            </RouterLink>
            <RouterLink :to="`/components/${submittedReview.familyId}`" class="btn btn-outline btn-sm">
              <AppIcon name="eye" :size="12" />查看构件
            </RouterLink>
            <button class="btn btn-ghost btn-sm" @click="clearFile">继续入库</button>
          </div>
        </div>
      </div>

      <div class="mt-3 grid gap-3 xl:grid-cols-[minmax(0,1.25fr)_minmax(0,1fr)]">
      <div class="space-y-3">
        <!-- 上传区 -->
        <div class="panel p-3.5">
          <div class="flex flex-wrap items-center justify-between gap-2">
            <p class="section-title">1 · 上传构件制品包</p>
            <button v-if="debugMode" class="flex items-center gap-1 text-[10px] text-muted hover:text-ink" @click="showDefects = !showDefects">
              <AppIcon name="settings" :size="11" />演示故障注入
              <AppIcon :name="showDefects ? 'chevronUp' : 'chevronDown'" :size="10" />
            </button>
          </div>

          <div
            class="relative mt-2 rounded-md border-2 border-dashed px-4 py-6 text-center transition-colors"
            :class="dragOver ? 'border-primary bg-[#EDF4F8]' : file ? 'border-[#BBF7D0] bg-[#F7FDF9]' : 'border-[#D8DEE5] bg-[#FAFBFC] hover:border-[#C7D2DA]'"
            @dragover.prevent="dragOver = true"
            @dragleave="dragOver = false"
            @drop.prevent="onDrop"
          >
            <input type="file" accept=".cbfc,.zip" class="absolute inset-0 cursor-pointer opacity-0" @change="onFileChange" />
            <template v-if="!file">
              <div class="mx-auto flex h-10 w-10 items-center justify-center rounded-full border border-line bg-white text-[#94A3B8]">
                <AppIcon name="upload" :size="18" />
              </div>
              <p class="mt-2 text-[12px] font-medium text-ink">拖拽 .cbfc 制品包至此区域上传</p>
              <p class="mt-0.5 text-[10px] text-muted">或点击选择文件 · 单包 ≤ 200MB</p>
            </template>
            <template v-else>
              <div class="flex items-center justify-center gap-3">
                <span class="flex h-9 w-9 items-center justify-center rounded bg-[#E8EFF4] text-primary">
                  <AppIcon name="package" :size="17" />
                </span>
                <div class="min-w-0 text-left">
                  <p class="truncate text-[12px] font-medium text-ink">{{ file.name }}</p>
                  <p class="mt-0.5 text-[9.5px] text-muted">
                    {{ formatBytes(file.size) }} ·
                    <span class="mono" v-if="hashing">正在计算 SHA-256…</span>
                    <span class="mono" v-else>{{ maskKey(fileHash, 16, 6) }}</span>
                  </p>
                </div>
                <button class="btn btn-ghost btn-sm" @click.stop.prevent="clearFile">移除</button>
              </div>
            </template>
          </div>

          <div v-if="uploadedMeta" class="mt-2 rounded border border-[#C9DCE6] bg-[#F7FBFD] p-2.5">
            <div class="flex flex-wrap items-start justify-between gap-2">
              <div class="min-w-0">
                <p class="truncate text-[12px] font-semibold text-ink">{{ uploadedMeta.name }}</p>
                <p class="mono mt-0.5 text-[9px] text-muted">
                  {{ uploadedMeta.familyId }} · v{{ uploadedMeta.revision }} · {{ uploadedMeta.sourceMode }}
                </p>
              </div>
              <StatusBadge tone="info" :label="`${uploadedMeta.presetCount} 个预定义型号`" :dot="false" />
            </div>
            <dl class="mt-2 grid grid-cols-2 gap-x-3 gap-y-1 text-[9.5px] sm:grid-cols-4">
              <div><dt class="text-muted">分类</dt><dd class="truncate text-ink">{{ uploadedMeta.category || '—' }}</dd></div>
              <div><dt class="text-muted">语义码</dt><dd class="mono truncate text-ink" :title="uploadedMeta.semanticCode">{{ uploadedMeta.semanticCode || '—' }}</dd></div>
              <div><dt class="text-muted">体积</dt><dd class="mono text-ink">{{ formatVolume(uploadedMeta.volumeM3) }}</dd></div>
              <div><dt class="text-muted">顶点 / 面片</dt><dd class="mono text-ink">{{ diagnosticDecoded?.header.vertexCount ?? 0 }} / {{ diagnosticDecoded?.header.triangleCount ?? 0 }}</dd></div>
              <div><dt class="text-muted">部位</dt><dd class="text-ink">{{ uploadedMeta.shapes.length }} 个</dd></div>
            </dl>
            <p v-if="uploadedPreviewUrl" class="mt-1.5 text-[9px] text-success">已从制品包加载 preview.glb，可在「构件技术诊断」中查看。</p>
          </div>
          <p v-if="packageError" class="mt-2 rounded border border-[#FECACA] bg-[#FEF2F2] px-2 py-1.5 text-[10px] text-danger">{{ packageError }}</p>

          <div class="mt-2 space-y-2">
            <div>
              <label class="label">目标模板（CRBIM 53 表）</label>
              <select v-model="templateCode" class="select !py-1.5 !text-[12.5px]" @change="onTemplateChange">
                <option value="">自动/不指定</option>
                <option v-for="template in templateOptions" :key="template.code" :value="template.code">
                  {{ '　'.repeat(template.depth) }}{{ template.label }} · {{ template.code }}
                </option>
              </select>
              <p class="mt-1 text-[9px] leading-relaxed text-[#92400E]">
                全量 CRBIM 53 表模板；上传包会按名称、语义码与分类自动模糊匹配。{{ templateLabel ? `当前匹配：${templateLabel}` : '' }}
              </p>
              <p v-if="!templateHasFamily" class="mt-1 text-[9px] leading-relaxed text-danger">
                该模板当前没有库存构件族，请先在「族库管理」创建对应族后再入库。
              </p>
            </div>
            <div class="grid gap-2 sm:grid-cols-[1fr_110px_auto]">
              <div>
                <label class="label">目标族模型</label>
                <select v-model="familyId" class="select !py-1.5 !text-[12.5px]">
                  <option v-for="item in components" :key="item.familyId" :value="item.familyId">
                    {{ item.name }} · {{ item.familyId }}
                  </option>
                </select>
              </div>
              <div>
                <label class="label">送审版本</label>
                <input v-model="revision" class="input mono !py-1.5 !text-[12.5px]" />
              </div>
              <div class="flex items-end">
                <button class="btn btn-primary w-full" :disabled="running || !file" @click="runGates">
                  <span v-if="running" class="h-3.5 w-3.5 animate-spin rounded-full border-2 border-white/30 border-t-white" />
                  <AppIcon v-else name="play" :size="14" />
                  开始门禁诊断
                </button>
              </div>
            </div>
          </div>

          <div v-if="target" class="mt-2 flex flex-wrap items-center gap-1.5 rounded border border-line bg-[#FAFBFC] px-2 py-1.5">
            <StatusBadge :tone="sourceModeMeta(target.sourceMode).tone" :label="sourceModeMeta(target.sourceMode).label" :dot="false" />
            <span class="text-[9.5px] text-muted">{{ target.engine }} · exactBrep {{ target.geometry.exactBrep }}</span>
          </div>

          <div v-if="debugMode && showDefects" class="mt-2 rounded border border-[#FDE68A] bg-[#FFFBEB] p-2.5">
            <p class="text-[10px] font-semibold text-[#92400E]">演示故障注入（仅用于验证门禁阻断与红色告警体验）</p>
            <div class="mt-1.5 grid grid-cols-2 gap-1 sm:grid-cols-3">
              <label
                v-for="defect in defectOptions"
                :key="defect.id"
                class="flex cursor-pointer items-center gap-1.5 rounded bg-white/70 px-1.5 py-1 text-[9.5px] text-[#92400E]"
              >
                <input
                  type="checkbox"
                  class="h-3 w-3 accent-[#B45309]"
                  :checked="defects.includes(defect.id)"
                  @change="toggleDefect(defect.id)"
                />
                <span class="mono text-[8px] text-[#B45309]">{{ defect.gate }}</span>
                {{ defect.label }}
              </label>
            </div>
          </div>
        </div>

        <!-- 门禁流水线 -->
        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <p class="section-title">2 · 6 道入库自动化安全门禁</p>
            <div class="flex items-center gap-2">
              <span v-if="result" class="mono text-[9.5px] text-muted">{{ passedCount }}/6 PASSED</span>
              <span class="h-1.5 w-20 overflow-hidden rounded-full bg-[#EEF1F4]">
                <span class="block h-full rounded-full bg-primary transition-all" :style="{ width: `${pipelineProgress}%` }" />
              </span>
            </div>
          </div>

          <div
            v-if="result && !result.passed && revealed >= result.gates.length"
            class="mt-3 rounded border border-[#FECACA] bg-[#FEF2F2] px-3 py-2.5"
          >
            <div class="flex flex-wrap items-start justify-between gap-2">
              <div class="min-w-0">
                <p class="flex items-center gap-1.5 text-[11.5px] font-semibold text-danger">
                  <AppIcon name="alertTriangle" :size="13" />门禁阻断：{{ firstFailure?.name ?? '未通过校验' }}
                </p>
                <p class="mt-1 text-[10.5px] leading-relaxed text-[#7F1D1D]">{{ firstFailure?.message ?? '请检查构件包后重新检测。' }}</p>
              </div>
              <div class="flex shrink-0 gap-1.5">
                <button class="btn btn-outline btn-sm !py-1" @click="downloadReport">下载报告</button>
                <button class="btn btn-primary btn-sm !py-1" :disabled="running" @click="runGates">重新检测</button>
              </div>
            </div>
          </div>

          <div v-if="!result" class="mt-3 rounded border border-dashed border-[#D8DEE5] bg-[#FAFBFC] px-4 py-8 text-center">
            <AppIcon name="shield" :size="22" class="mx-auto text-[#CBD5E1]" />
            <p class="mt-2 text-[11.5px] text-muted">上传制品包后，将逐道执行 GATE-01 → GATE-06 校验</p>
          </div>

          <div v-else class="relative mt-3 space-y-0">
            <div class="absolute bottom-3 left-[17px] top-3 w-px bg-line" />
            <div v-for="(gate, index) in result.gates" :key="gate.code" class="relative flex gap-2.5 pb-2">
              <span
                class="relative z-10 flex h-9 w-9 shrink-0 items-center justify-center rounded-full border-2 bg-white"
                :class="
                  gateView(gate, index).status === 'passed'
                    ? 'border-success text-success'
                    : gateView(gate, index).status === 'failed'
                      ? 'border-danger bg-[#FEF2F2] text-danger'
                      : gateView(gate, index).status === 'running'
                        ? 'border-primary text-primary'
                        : 'border-line text-[#9CA3AF]'
                "
              >
                <span v-if="gateView(gate, index).status === 'running'" class="h-3.5 w-3.5 animate-spin rounded-full border-2 border-[#CBD5E1] border-t-primary" />
                <AppIcon
                  v-else
                  :name="gateView(gate, index).status === 'passed' ? 'check' : gateView(gate, index).status === 'failed' ? 'x' : 'clock'"
                  :size="15"
                  :stroke-width="2.4"
                />
              </span>

              <button
                class="min-w-0 flex-1 rounded-md border text-left transition-colors"
                :class="
                  gateView(gate, index).status === 'failed'
                    ? 'border-[#FECACA] bg-[#FEF2F2]'
                    : gateView(gate, index).status === 'passed'
                      ? 'border-[#BBF7D0] bg-[#F9FDFA] hover:border-success/40'
                      : 'border-line bg-[#FAFBFC]'
                "
                @click="selectedGate = selectedGate === gate.code ? null : gate.code"
              >
                <div class="flex items-center gap-2 px-2.5 py-2">
                  <span class="mono shrink-0 rounded bg-white/80 px-1.5 py-[1px] text-[8.5px] font-semibold text-[#6B7280]">{{ gate.code }}</span>
                  <span class="min-w-0 flex-1 truncate text-[11px] font-medium text-ink">{{ gate.name }}</span>
                  <span
                    class="shrink-0 rounded px-1.5 py-[1px] text-[8.5px] font-semibold"
                    :class="
                      gateView(gate, index).status === 'passed'
                        ? 'bg-[#ECFDF3] text-success'
                        : gateView(gate, index).status === 'failed'
                          ? 'bg-[#FEE2E2] text-danger'
                          : gateView(gate, index).status === 'running'
                            ? 'bg-[#E8EFF4] text-primary'
                            : 'bg-[#F3F4F6] text-[#6B7280]'
                    "
                  >
                    {{ gateView(gate, index).status === 'passed' ? 'PASSED' : gateView(gate, index).status === 'failed' ? 'BLOCKED' : gateView(gate, index).status === 'running' ? 'RUNNING' : 'PENDING' }}
                  </span>
                  <span v-if="gateView(gate, index).durationMs" class="mono shrink-0 text-[8.5px] text-[#9CA3AF]">{{ gateView(gate, index).durationMs }}ms</span>
                  <AppIcon :name="selectedGate === gate.code ? 'chevronUp' : 'chevronDown'" :size="11" class="shrink-0 text-[#9CA3AF]" />
                </div>
                <p
                  class="px-2.5 pb-1.5 text-[9.5px] leading-relaxed"
                  :class="gateView(gate, index).status === 'failed' ? 'text-danger' : 'text-muted'"
                >
                  {{ gateView(gate, index).message }}
                </p>
                <div v-if="selectedGate === gate.code && gateView(gate, index).details?.length" class="border-t border-black/5 px-2.5 py-2">
                  <div v-for="item in gateView(gate, index).details" :key="item.label" class="flex items-start justify-between gap-2 py-0.5 text-[9px]">
                    <span class="shrink-0 text-[#9CA3AF]">{{ item.label }}</span>
                    <span class="mono text-right" :class="item.status === 'error' ? 'text-danger' : 'text-[#374151]'">{{ item.value }}</span>
                  </div>
                </div>
              </button>
            </div>
          </div>

          <div
            v-if="verdict"
            class="mt-2 flex flex-wrap items-center gap-2 rounded-md border px-3 py-2"
            :class="verdict === 'passed' ? 'border-[#BBF7D0] bg-[#F0FDF4]' : 'border-[#FECACA] bg-[#FEF2F2]'"
          >
            <AppIcon :name="verdict === 'passed' ? 'checkCircle' : 'xCircle'" :size="16" :class="verdict === 'passed' ? 'text-success' : 'text-danger'" />
            <div class="min-w-0 flex-1">
              <p class="text-[11.5px] font-semibold" :class="verdict === 'passed' ? 'text-success' : 'text-danger'">
                {{ verdict === 'passed' ? '6 道门禁全部通过，构件包可用于发布审核' : `${firstFailure?.code} 门禁阻断：${firstFailure?.name}` }}
              </p>
              <p class="mt-0.5 text-[9.5px] text-muted">
                {{ verdict === 'passed' ? '可提交标准化委员会进行技术发布审核。' : '请修复后重新打包上传，或下载失败明细表比对本地摘要。' }}
              </p>
            </div>
            <button class="btn btn-outline btn-sm" @click="downloadReport"><AppIcon name="file" :size="12" />下载报告</button>
            <button
              v-if="verdict === 'passed'"
              class="btn btn-primary btn-sm"
              :disabled="submitting || Boolean(submittedReview)"
              @click="submitReview"
            >
              <AppIcon :name="submittedReview ? 'checkCircle' : 'flag'" :size="12" />{{ submittedReview ? '已提交审核' : '提交发布审核' }}
            </button>
          </div>
        </div>
      </div>

      <!-- 构件技术诊断 -->
      <div class="space-y-3">
        <div class="panel px-3 py-2">
          <p class="text-[11px] font-semibold text-ink">构件技术诊断</p>
          <p class="mt-0.5 text-[9.5px] text-muted">展示上传包内的构件几何、属性和门禁检验数据。</p>
        </div>

        <div class="panel overflow-hidden">
          <div class="flex items-center justify-between border-b border-line px-3 py-2">
            <div class="flex items-center gap-1.5">
              <AppIcon name="cube" :size="13" class="text-primary" />
              <p class="text-[11px] font-semibold text-ink">{{ uploadedPreviewUrl ? '上传构件几何' : '3D 验真诊断视口' }}</p>
            </div>
            <StatusBadge
              :tone="verdict === 'failed' ? 'danger' : verdict === 'passed' ? 'success' : 'muted'"
              :label="verdict === 'failed' ? '几何缺陷定位' : verdict === 'passed' ? '几何封闭正常' : '待诊断'"
            />
          </div>
          <div class="relative h-[340px]">
            <CadViewport
              v-if="diagnosticDecoded"
              :decoded="diagnosticDecoded"
              :status="diagnosticStatus"
              :progress="uploadedDecoded ? 100 : progress"
              :error="uploadedDecoded ? '' : evalError"
              :compact="true"
              :shape-labels="{}"
              :named-parts="diagnosticParts"
              :selected-part="uploadedPart"
              @select-part="uploadedPart = $event"
            />
            <UploadedModelView v-else-if="uploadedPreviewUrl" :url="uploadedPreviewUrl" />
            <div
              class="pointer-events-none absolute inset-0"
              :class="verdict === 'failed' ? 'shadow-[inset_0_0_60px_rgba(185,28,28,0.35)]' : 'shadow-[inset_0_0_60px_rgba(34,197,94,0.18)]'"
            />
            <div class="pointer-events-none absolute left-3 top-3 rounded border border-white/10 bg-[#0F1A2B]/80 px-2 py-1">
              <p class="text-[8.5px] font-semibold uppercase tracking-wider" :class="verdict === 'failed' ? 'text-[#FCA5A5]' : 'text-[#86EFAC]'">
                {{ verdict === 'failed' ? 'RED ALERT · 非流形定位' : 'UPLOAD QA VIEWPORT' }}
              </p>
              <p class="mono mt-0.5 text-[8px] text-[#64748B]">{{ uploadedMeta?.familyId ?? target?.familyId }} · {{ diagnosticDecoded?.header.triangleCount ?? uploadedMeta?.triangles ?? '—' }} tris</p>
            </div>
          </div>
        </div>

        <div class="panel p-3.5">
          <div class="flex items-center justify-between">
            <p class="section-title">门禁检验数据诊断</p>
            <span class="mono text-[11px] text-muted">{{ result ? `${result.durationMs}ms` : '—' }}</span>
          </div>
          <dl class="mt-2 grid grid-cols-[104px_1fr] gap-y-1.5 text-[11.5px]">
            <dt class="text-muted">构件名称</dt>
            <dd class="text-[#374151]">{{ uploadedMeta?.name ?? target?.name ?? '—' }}</dd>
            <dt class="text-muted">FamilyId / 版本</dt>
            <dd class="mono text-[#374151]">{{ uploadedMeta?.familyId ?? target?.familyId ?? '—' }} · v{{ uploadedMeta?.revision ?? revision }}</dd>
            <dt class="text-muted">分类 / 语义码</dt>
            <dd class="mono break-all text-[#374151]">{{ uploadedMeta?.category ?? '—' }} · {{ uploadedMeta?.semanticCode ?? '—' }}</dd>
            <dt class="text-muted">几何流形状态</dt>
            <dd class="text-[#374151]">{{ result?.report.bRepCheck ?? '—' }}</dd>
            <dt class="text-muted">顶点 / 面片</dt>
            <dd class="mono text-[#374151]">
              {{ formatNumber(uploadedMeta?.vertices || diagnosticDecoded?.header.vertexCount || result?.report.mesh.vertices || target?.geometry.vertices || 0) }} /
              {{ formatNumber(uploadedMeta?.triangles || diagnosticDecoded?.header.triangleCount || result?.report.mesh.triangles || target?.geometry.triangles || 0) }}
            </dd>
            <dt class="text-muted">部位数量</dt>
            <dd class="text-[#374151]">{{ uploadedMeta?.shapes.length ?? result?.detected.shapes.length ?? 0 }} 个</dd>
            <dt class="text-muted">自交 / 自重叠</dt>
            <dd class="mono" :class="(result?.report.mesh.selfIntersections ?? 0) ? 'text-danger' : 'text-success'">
              {{ result?.report.mesh.selfIntersections ?? 0 }} 处
            </dd>
            <dt class="text-muted">散度体积偏差</dt>
            <dd class="mono" :class="(result?.report.volumeDivergence ?? 0) > 1 ? 'text-danger' : 'text-success'">
              {{ (result?.report.volumeDivergence ?? 0).toFixed(2) }}% (&lt; 1.0%)
            </dd>
            <dt class="text-muted">脚本 SHA-256</dt>
            <dd class="mono break-all text-[11px] text-[#4B5563]">{{ maskKey(result?.report.scriptDigest, 24, 8) }}</dd>
            <dt class="text-muted">实体净体积</dt>
            <dd class="mono text-[#374151]">{{ formatVolume(uploadedMeta?.volumeM3 || target?.geometry.volumeM3 || 0) }}</dd>
            <dt class="text-muted">检测引擎</dt>
            <dd class="text-[#374151]">{{ result?.detected.engine ?? uploadedMeta?.engine ?? target?.engine ?? '—' }}</dd>
          </dl>

          <div class="mt-2.5 border-t border-[#F0F2F5] pt-2">
            <p class="eyebrow mb-1">极端参数边界压测</p>
            <div class="space-y-1">
              <div v-for="test in result?.report.extremeTests ?? []" :key="test.label" class="flex items-start gap-1.5">
                <AppIcon :name="test.ok ? 'checkCircle' : 'xCircle'" :size="11" :class="test.ok ? 'mt-0.5 text-success' : 'mt-0.5 text-danger'" />
                <div>
                  <p class="text-[9.5px] text-[#374151]">{{ test.label }}</p>
                  <p class="text-[8.5px] text-muted">{{ test.note }}</p>
                </div>
              </div>
            </div>
          </div>

          <div v-if="result" class="mt-2 grid grid-cols-3 gap-1.5">
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[8.5px] text-muted">sourceMode</p>
              <p class="mono mt-0.5 text-[10px] font-semibold text-ink">{{ result.detected.sourceMode }}</p>
            </div>
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[8.5px] text-muted">exactBrep</p>
              <p class="mono mt-0.5 text-[10px] font-semibold text-ink">{{ result.detected.exactBrep }}</p>
            </div>
            <div class="panel-inset px-2 py-1.5">
              <p class="text-[8.5px] text-muted">部位数量</p>
              <p class="mono mt-0.5 text-[10px] font-semibold text-ink">{{ result.detected.shapes.length }}</p>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>
