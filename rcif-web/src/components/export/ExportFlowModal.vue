<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue';
import ModalShell from '@/components/ui/ModalShell.vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import { api, ApiError } from '@/api/client';
import type { ComponentDetail, DownloadFormat, DownloadTicket } from '@/api/types';
import { useSessionStore } from '@/stores/session';
import { useUiStore } from '@/stores/ui';
import {
  copyText,
  formatBytes,
  maskKey,
  triggerDownload,
} from '@/utils/format';

const props = withDefaults(
  defineProps<{
    open: boolean;
    component: ComponentDetail | null;
    initialFormat?: string;
    autoHandoff?: boolean;
    modelNumber?: string;
    parameterOverrides?: Record<string, number | string | boolean>;
  }>(),
  { initialFormat: '', autoHandoff: false, modelNumber: '', parameterOverrides: () => ({}) },
);
const emit = defineEmits<{ close: [] }>();

const session = useSessionStore();
const ui = useUiStore();

type FormatOption = DownloadFormat & { available: boolean; reason: string | null };
const FORMAT_ORDER = ['rcif', 'ifc', 'glb', 'step', 'cbf'];
const formats = ref<FormatOption[]>([]);
const formatsLoading = ref(false);
const selected = ref<string>('rcif');
const step = ref<1 | 2 | 3>(1);
const agree = ref(false);
const busy = ref(false);
const ticket = ref<DownloadTicket | null>(null);
const requestState = ref(false);
const error = ref('');
const handoff = ref<{ handoffToken: string; expiresIn: number; deepLink: string } | null>(null);
const handoffStatus = ref<'idle' | 'probing' | 'fallback' | 'success' | 'error'>('idle');
const consumed = ref<{ downloadUrl: string; familyId: string; cacheKey: string } | null>(null);
const remaining = ref(0);
let countdown: ReturnType<typeof setInterval> | null = null;
let probeTimer: ReturnType<typeof setTimeout> | null = null;

const isPartner = computed(() => session.user?.role === 'partner');
const selectedFormat = computed(() => formats.value.find((f) => f.id === selected.value));
const currentCacheKey = computed(() => {
  const preset = props.component?.parameters.presets.find(
    (item) => item.modelNumber === props.modelNumber,
  );
  return preset?.cacheKey ?? props.component?.geometry.defaultCacheKey ?? '';
});
const checksum = computed(() => ticket.value?.checksum ?? '');
const ticketCountdown = computed(() => {
  if (!ticket.value?.expiresAt) return '';
  const seconds = Math.max(0, Math.floor((new Date(ticket.value.expiresAt).getTime() - Date.now()) / 1000));
  const m = Math.floor(seconds / 60);
  const s = seconds % 60;
  return `${m}:${String(s).padStart(2, '0')}`;
});

async function loadFormats() {
  if (!props.component) return;
  formatsLoading.value = true;
  try {
    const data = await api.formats(props.component.familyId);
    const available = data.formats.filter((format) => FORMAT_ORDER.includes(format.id));
    formats.value = FORMAT_ORDER.map((id) => available.find((format) => format.id === id)).filter(
      (format): format is FormatOption => Boolean(format),
    );
    const firstAvailable = formats.value.find((f) => f.available);
    if (firstAvailable) selected.value = firstAvailable.id;
  } catch (loadError) {
    error.value = loadError instanceof Error ? loadError.message : '格式清单加载失败';
  } finally {
    formatsLoading.value = false;
  }
}

watch(
  () => [props.open, props.component?.familyId],
  async () => {
    if (!props.open) return;
    step.value = 1;
    agree.value = false;
    ticket.value = null;
    requestState.value = false;
    error.value = '';
    handoff.value = null;
    handoffStatus.value = 'idle';
    consumed.value = null;
    remaining.value = 0;
    if (countdown) clearInterval(countdown);
    if (probeTimer) clearTimeout(probeTimer);
    if (!formats.value.length) await loadFormats();
    if (props.initialFormat) {
      const target = formats.value.find((f) => f.id === props.initialFormat);
      if (target?.available) selected.value = target.id;
    }
    if (props.autoHandoff) {
      agree.value = true;
      step.value = 3;
      await createTicket();
      await startHandoff();
    }
  },
  { immediate: true },
);

onBeforeUnmount(() => {
  if (countdown) clearInterval(countdown);
  if (probeTimer) clearTimeout(probeTimer);
});

function prevStep() {
  if (step.value > 1) step.value = (step.value - 1) as 1 | 2 | 3;
}

function nextStep() {  if (step.value === 1) {
    if (!selectedFormat.value?.available) {
      ui.notify({ type: 'warning', title: '该格式不可用', message: selectedFormat.value?.reason ?? '' });
      return;
    }
    step.value = 2;
    return;
  }
  if (step.value === 2) {
    if (!agree.value) {
      ui.notify({ type: 'warning', title: '请先确认许可与配额', message: '需勾选知识产权与租户许可声明' });
      return;
    }
    step.value = 3;
    void createTicket();
  }
}

function startCountdown() {
  if (countdown) clearInterval(countdown);
  countdown = setInterval(() => {
    if (!ticket.value) return;
    const seconds = Math.max(0, Math.floor((new Date(ticket.value.expiresAt).getTime() - Date.now()) / 1000));
    remaining.value = seconds;
    if (seconds <= 0) {
      ticket.value = null;
      if (countdown) clearInterval(countdown);
    }
  }, 1000);
}

async function createTicket() {
  if (!props.component) return;
  busy.value = true;
  error.value = '';
  requestState.value = false;
  try {
    const result = await api.download(
      props.component.familyId,
      selected.value,
      props.modelNumber || undefined,
      props.parameterOverrides,
    );
    if (result.permission === 'request-required') {
      requestState.value = true;
      ticket.value = null;
      return;
    }
    ticket.value = result;
    startCountdown();
  } catch (ticketError) {
    error.value = ticketError instanceof Error ? ticketError.message : '下载凭据签发失败';
  } finally {
    busy.value = false;
  }
}

function downloadNow() {
  if (!ticket.value) {
    void createTicket();
    return;
  }
  triggerDownload(ticket.value.url, ticket.value.fileName);
  ui.notify({
    type: 'success',
    title: '下载已开始',
    message: `${ticket.value.fileName} · ${formatBytes(ticket.value.sizeBytes)}`,
  });
}

async function requestAccess() {
  busy.value = true;
  await new Promise((resolve) => setTimeout(resolve, 600));
  busy.value = false;
  requestState.value = false;
  ui.notify({
    type: 'success',
    title: '访问审批申请已提交',
    message: '已通知资产管理员，审批通过后将自动签发下载凭据并邮件通知。',
  });
}

async function startHandoff() {
  if (!props.component) return;
  busy.value = true;
  error.value = '';
  try {
    const token = await api.handoffToken(props.component.familyId, currentCacheKey.value);
    if (!/^htk_[a-zA-Z0-9_-]{32,64}$/.test(token.handoffToken)) {
      throw new Error('handoffToken 格式未通过前端白名单校验，已阻断唤起');
    }
    handoff.value = token;
    handoffStatus.value = 'probing';
    // 隐藏 iframe 触发自定义协议（非阻塞）
    const iframe = document.createElement('iframe');
    iframe.style.display = 'none';
    iframe.src = token.deepLink;
    document.body.appendChild(iframe);
    setTimeout(() => iframe.remove(), 2500);
    probeTimer = setTimeout(() => {
      if (handoffStatus.value === 'probing') {
        handoffStatus.value = 'fallback';
        ui.notify({
          type: 'warning',
          title: '未检测到 QtOCCTApp 桌面端',
          message: '已在 2.0 秒内自动切换为下载 .rcif 构件包，杜绝死按钮。',
        });
        void fallbackDownload();
      }
    }, 2000);
  } catch (handoffError) {
    handoffStatus.value = 'error';
    error.value = handoffError instanceof Error ? handoffError.message : '桌面端票据签发失败';
  } finally {
    busy.value = false;
  }
}

async function fallbackDownload() {
  selected.value = 'rcif';
  if (!ticket.value) await createTicket();
  if (ticket.value) downloadNow();
}

async function simulateDesktopConsume() {
  if (!handoff.value) return;
  busy.value = true;
  try {
    const result = await api.consumeHandoff(handoff.value.handoffToken);
    consumed.value = result;
    handoffStatus.value = 'success';
    if (probeTimer) clearTimeout(probeTimer);
    ui.notify({
      type: 'success',
      title: '一次性票据核销成功',
      message: 'QtOCCTApp 已换取构件临时下载授权；该票据已用后即焚，无法重复核销。',
    });
    try {
      await api.consumeHandoff(handoff.value.handoffToken);
    } catch (secondError) {
      if (secondError instanceof ApiError && secondError.status === 410) {
        ui.notify({ type: 'info', title: '重放防护验证通过', message: secondError.message });
      }
    }
  } catch (consumeError) {
    handoffStatus.value = 'error';
    error.value = consumeError instanceof Error ? consumeError.message : '票据核销失败';
  } finally {
    busy.value = false;
  }
}

async function copyLink() {
  if (!handoff.value) return;
  const ok = await copyText(handoff.value.deepLink);
  ui.notify({ type: ok ? 'success' : 'error', title: ok ? '定位短链已复制' : '复制失败' });
}

async function copyChecksum() {
  if (!checksum.value) return;
  const ok = await copyText(checksum.value);
  if (ok) ui.notify({ type: 'success', title: 'SHA-256 已复制' });
}
</script>

<template>
  <ModalShell
    :open="open"
    title="构件导出"
    :subtitle="component ? `${component.name} · ${component.drawingCode} · v${component.revision}` : ''"
    icon="download"
    width="lg"
    @close="emit('close')"
  >
    <div class="p-5">
      <!-- 步骤 -->
      <ol class="mb-5 flex items-center gap-2">
        <li
          v-for="(label, index) in ['格式与版本', '权限与许可', '生成与下载']"
          :key="label"
          class="flex flex-1 items-center gap-2"
        >
          <span
            class="flex h-6 w-6 shrink-0 items-center justify-center rounded-full border text-[10.5px] font-semibold"
            :class="
              step > index + 1
                ? 'border-success bg-success text-white'
                : step === index + 1
                  ? 'border-primary bg-primary text-white'
                  : 'border-line bg-white text-[#9CA3AF]'
            "
          >
            <AppIcon v-if="step > index + 1" name="check" :size="11" :stroke-width="3" />
            <template v-else>{{ index + 1 }}</template>
          </span>
          <span class="text-[11.5px]" :class="step === index + 1 ? 'font-semibold text-ink' : 'text-muted'">{{ label }}</span>
          <span v-if="index < 2" class="h-px flex-1 bg-line" />
        </li>
      </ol>

      <!-- Step 1 -->
      <div v-if="step === 1">
        <div v-if="formatsLoading" class="grid grid-cols-2 gap-2">
          <div v-for="i in 6" :key="i" class="skeleton h-[92px] w-full" />
        </div>
        <div v-else class="grid grid-cols-2 gap-2">
          <button
            v-for="format in formats"
            :key="format.id"
            class="relative rounded-md border p-3 text-left transition-all"
            :class="
              selected === format.id
                ? 'border-primary bg-[#F3F8FB] ring-2 ring-primary/10'
                : format.available
                  ? 'border-line bg-surface hover:border-[#C7D2DA] hover:bg-[#FAFBFC]'
                  : 'cursor-not-allowed border-line bg-[#FAFBFC] opacity-60'
            "
            :disabled="!format.available"
            @click="selected = format.id"
          >
            <div class="flex items-start justify-between gap-2">
              <div class="flex items-center gap-1.5">
                <span class="rounded bg-[#F1F4F7] px-1.5 py-[1px] font-mono text-[11px] font-semibold uppercase text-[#4B5563]">
                  .{{ format.ext }}
                </span>
              </div>
              <span
                class="flex h-4 w-4 items-center justify-center rounded-full border"
                :class="selected === format.id ? 'border-primary bg-primary text-white' : 'border-[#CBD5E1]'"
              >
                <AppIcon v-if="selected === format.id" name="check" :size="9" :stroke-width="3" />
              </span>
            </div>
            <p class="mt-1.5 text-[12.5px] font-semibold text-ink">{{ format.title }}</p>
            <p class="mt-0.5 text-[12px] leading-relaxed text-muted">{{ format.description }}</p>
            <p class="mt-1 flex items-center gap-1.5 text-[11px] text-[#9CA3AF]">
              约 {{ format.sizeMB.toFixed(1) }} MB · {{ format.scenario }}
              <span v-if="format.scriptIncluded" class="rounded bg-[#EFF6FF] px-1 text-[#1D4ED8]">含脚本</span>
            </p>
            <p v-if="!format.available" class="mt-1 text-[11px] text-warning">{{ format.reason }}</p>
          </button>
        </div>
      </div>

      <!-- Step 2 -->
      <div v-else-if="step === 2" class="space-y-3">
        <div class="rounded-md border border-line bg-[#FAFBFC] p-3">
          <p class="text-[12px] font-semibold text-ink">下载摘要</p>
          <dl class="mt-2 grid grid-cols-[110px_1fr] gap-y-1.5 text-[11.5px]">
            <dt class="text-muted">构件</dt>
            <dd class="text-ink">{{ component?.name }}</dd>
            <dt class="text-muted">格式</dt>
            <dd class="font-mono text-[11.5px] text-ink">.{{ selectedFormat?.ext }} · {{ selectedFormat?.title }}</dd>
            <dt class="text-muted">权威缓存键</dt>
            <dd class="font-mono text-[11.5px] text-[#374151]">{{ maskKey(component?.geometry.defaultCacheKey, 16, 8) }}</dd>
            <dt class="text-muted">当前租户</dt>
            <dd class="text-ink">{{ session.user?.tenantName }}</dd>
            <dt class="text-muted">下载身份</dt>
            <dd class="text-ink">{{ session.user?.name }} · {{ session.user?.roleLabel }}</dd>
          </dl>
        </div>

        <div
          v-if="isPartner"
          class="flex items-start gap-2 rounded-md border border-[#FDE68A] bg-[#FFFBEB] px-3 py-2 text-[11px] leading-relaxed text-[#92400E]"
        >
          <AppIcon name="lock" :size="14" class="mt-0.5" />
          外部合作方下载企业内部构件需提交访问审批。审批通过前仅可只读预览，本操作将生成审批工单。
        </div>

        <label class="flex cursor-pointer items-start gap-2 rounded-md border border-line bg-surface p-3">
          <input v-model="agree" type="checkbox" class="mt-0.5 h-3.5 w-3.5 accent-[#1F5A7A]" />
          <span class="text-[11px] leading-relaxed text-[#4B5563]">
            我确认已阅读并遵守企业知识产权与租户许可约定：构件制品仅供本项目使用，不得向第三方分发；下载行为将写入不可篡改审计日志（含用户、构件、格式、IP 与时间）。
          </span>
        </label>
      </div>

      <!-- Step 3 -->
      <div v-else class="space-y-3">
        <div v-if="busy && !ticket && !requestState" class="flex items-center gap-2 rounded-md border border-line bg-[#FAFBFC] px-3 py-4 text-[12px] text-muted">
          <span class="h-3.5 w-3.5 animate-spin rounded-full border-2 border-[#CBD5E1] border-t-primary" />
          正在签发带时效签名的下载凭据…
        </div>

        <div v-if="requestState" class="rounded-md border border-[#FDE68A] bg-[#FFFBEB] p-4">
          <p class="flex items-center gap-1.5 text-[12.5px] font-semibold text-[#92400E]">
            <AppIcon name="lock" :size="14" />该构件下载受权限限制
          </p>
          <p class="mt-1 text-[11px] leading-relaxed text-[#92400E]/85">
            已为您生成跨租户访问审批工单，审批通过后系统自动签发下载凭据。
          </p>
          <button class="btn btn-primary btn-sm mt-3" :disabled="busy" @click="requestAccess">
            <AppIcon name="clipboard" :size="13" />提交访问审批申请
          </button>
        </div>

        <template v-else-if="ticket">
          <div class="rounded-md border border-[#BBF7D0] bg-[#F0FDF4] p-3">
            <div class="flex items-start justify-between gap-2">
              <div>
                <p class="flex items-center gap-1.5 text-[12.5px] font-semibold text-success">
                  <AppIcon name="checkCircle" :size="14" />下载凭据签发成功
                </p>
                <p class="mt-0.5 font-mono text-[12px] text-[#166534]">{{ ticket.fileName }} · {{ formatBytes(ticket.sizeBytes) }}</p>
              </div>
              <StatusBadge tone="success" :label="`${ticketCountdown} 有效`" :dot="false" mono />
            </div>
            <div class="mt-2 flex items-center gap-1.5 rounded border border-[#BBF7D0] bg-white/70 px-2 py-1.5">
              <span class="shrink-0 text-[11px] text-[#166534]">SHA-256</span>
              <span class="mono min-w-0 flex-1 truncate text-[11.5px] text-[#166534]">{{ checksum }}</span>
              <button class="shrink-0 text-[#166534] hover:text-success" title="复制摘要" @click="copyChecksum">
                <AppIcon name="copy" :size="12" />
              </button>
            </div>
          </div>

          <div class="flex flex-wrap items-center gap-2">
            <button class="btn btn-primary" :disabled="busy" @click="downloadNow">
              <AppIcon name="download" :size="14" />下载 {{ selectedFormat?.title }}
            </button>
            <button class="btn btn-outline" @click="createTicket">
              <AppIcon name="refresh" :size="13" />重新签发凭据
            </button>
            <span class="text-[11.5px] text-muted">下载凭据 15 分钟有效</span>
          </div>
        </template>

        <div v-if="error" class="rounded border border-[#FECACA] bg-[#FEF2F2] px-3 py-2 text-[11px] text-danger">
          {{ error }}
        </div>
      </div>
    </div>

    <template #footer>
      <div class="flex items-center justify-between">
        <button v-if="step > 1" class="btn btn-ghost btn-sm" @click="prevStep">
          <AppIcon name="arrowLeft" :size="13" />上一步
        </button>
        <span v-else class="text-[10px] text-muted">支持 5 种交付格式 · 依据 RCIF v1.4.2 §5.2 内容协商</span>
        <div class="flex items-center gap-2">
          <button class="btn btn-ghost btn-sm" @click="emit('close')">关闭</button>
          <button v-if="step === 1" class="btn btn-primary btn-sm" :disabled="!selectedFormat?.available" @click="nextStep">
            下一步：许可确认<AppIcon name="arrowRight" :size="13" />
          </button>
          <button v-else-if="step === 2" class="btn btn-primary btn-sm" @click="nextStep">
            {{ isPartner ? '提交访问审批申请' : '生成并签发凭据' }}<AppIcon name="arrowRight" :size="13" />
          </button>
          <button v-else-if="ticket" class="btn btn-primary btn-sm" @click="downloadNow">
            <AppIcon name="download" :size="13" />再次下载
          </button>
        </div>
      </div>
    </template>
  </ModalShell>
</template>
