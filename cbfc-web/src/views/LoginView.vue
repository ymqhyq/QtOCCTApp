<script setup lang="ts">
import { computed, onMounted, ref } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import AppIcon from '@/components/ui/AppIcon.vue';
import { useSessionStore, ROLE_OPTIONS } from '@/stores/session';
import { useUiStore } from '@/stores/ui';

const router = useRouter();
const route = useRoute();
const session = useSessionStore();
const ui = useUiStore();
const busyRole = ref('');

onMounted(() => {
  if (route.query.switch && session.user) {
    session.logout();
  }
});

const features = [
  { icon: 'layers', title: '语义映射透明化', desc: 'CRBIM 53 表 / JTG / 国标 51269 与 IFC 4.3 IfcShapeAspect 逐部位可追溯' },
  { icon: 'zap', title: '标准型号可追溯', desc: '标准型号、预烘焙结果与质量门禁全程可追溯，构件加载快速稳定' },
  { icon: 'crosshair', title: '几何部位快速定位', desc: '点击三维模型即可定位对应几何面、特征线与工程算量属性' },
];

const stats = [
  { value: '24', label: '纳管构件' },
  { value: '6/6', label: '门禁通过' },
  { value: '≤ 300ms', label: '检索 P95' },
  { value: '100%', label: '零拷贝解码' },
];

async function enter(role: string) {
  busyRole.value = role;
  try {
    await session.login(role);
    const redirect = typeof route.query.redirect === 'string' ? route.query.redirect : '/';
    await router.push(redirect);
  } catch (error) {
    ui.notify({ type: 'error', title: '登录失败', message: error instanceof Error ? error.message : String(error) });
  } finally {
    busyRole.value = '';
  }
}

const currentYear = computed(() => new Date().getFullYear());
</script>

<template>
  <div class="flex min-h-screen bg-[#0B1220]">
    <!-- 品牌侧 -->
    <section class="relative hidden flex-1 overflow-hidden border-r border-white/5 lg:block">
      <div class="grid-bg absolute inset-0 opacity-40" />
      <div class="absolute -left-24 top-1/4 h-72 w-72 rounded-full bg-[#1F5A7A]/30 blur-3xl" />
      <div class="absolute bottom-0 right-0 h-80 w-80 rounded-full bg-[#38BDF8]/10 blur-3xl" />
      <div class="relative flex h-full flex-col p-10 xl:p-14">
        <div class="flex items-center gap-2.5">
          <span class="flex h-9 w-9 items-center justify-center rounded-[6px] bg-[#1F5A7A] text-white">
            <AppIcon name="cube" :size="20" />
          </span>
          <div>
            <p class="text-[14px] font-semibold text-white">铁路工程数字构件库</p>
            <p class="text-[9px] uppercase tracking-[0.18em] text-[#64748B]">CBF-C Component Foundation</p>
          </div>
        </div>

        <div class="mt-auto max-w-xl">
          <p class="font-mono text-[10px] uppercase tracking-[0.2em] text-[#38BDF8]">Industrial Titanium · v2.5</p>
          <h1 class="mt-3 text-[32px] font-semibold leading-[1.25] tracking-tight text-white xl:text-[38px]">
            企业级三维数字构件<br />资产管理中枢
          </h1>
          <p class="mt-3 text-[13px] leading-relaxed text-[#94A3B8]">
            基于 CBF-C v1.4.2 构件格式规范与 B/S 微服务架构 v1.4，
            将语义映射、确定性缓存与拓扑级三维反查全面产品化。
          </p>

          <div class="mt-7 space-y-4">
            <div v-for="feature in features" :key="feature.title" class="flex gap-3">
              <span class="mt-0.5 flex h-7 w-7 shrink-0 items-center justify-center rounded border border-white/10 bg-white/5 text-[#7DD3FC]">
                <AppIcon :name="feature.icon" :size="14" />
              </span>
              <div>
                <p class="text-[12.5px] font-medium text-white">{{ feature.title }}</p>
                <p class="mt-0.5 text-[10.5px] leading-relaxed text-[#64748B]">{{ feature.desc }}</p>
              </div>
            </div>
          </div>

          <div class="mt-8 grid grid-cols-4 gap-3 border-t border-white/10 pt-5">
            <div v-for="stat in stats" :key="stat.label">
              <p class="mono text-[17px] font-semibold text-white">{{ stat.value }}</p>
              <p class="mt-0.5 text-[9px] text-[#64748B]">{{ stat.label }}</p>
            </div>
          </div>
        </div>

        <p class="mt-10 font-mono text-[9px] text-[#334155]">© {{ currentYear }} CBF-C · 演示环境 · 数据为程序化生成</p>
      </div>
    </section>

    <!-- 登录侧 -->
    <section class="flex w-full flex-col justify-center bg-app px-5 py-8 lg:w-[520px] xl:w-[560px]">
      <div class="mx-auto w-full max-w-[400px]">
        <div class="mb-6 flex items-center gap-2 lg:hidden">
          <span class="flex h-8 w-8 items-center justify-center rounded-[6px] bg-primary text-white"><AppIcon name="cube" :size="18" /></span>
          <span class="text-[14px] font-semibold text-ink">铁路工程数字构件库</span>
        </div>

        <p class="eyebrow">演示身份选择</p>
        <h2 class="mt-1 text-[22px] font-semibold tracking-tight text-ink">进入构件库工作台</h2>
        <p class="mt-1 text-[12px] leading-relaxed text-muted">
          系统按 RBAC + RLS 行级多租户隔离。选择一个角色以体验对应权限与工作台。
        </p>

        <div class="mt-4 space-y-2">
          <button
            v-for="role in ROLE_OPTIONS"
            :key="role.role"
            class="group flex w-full items-center gap-3 rounded-md border border-line bg-surface p-3 text-left transition-all hover:border-primary hover:bg-[#F6FAFC] hover:shadow-panel disabled:opacity-60"
            :disabled="!!busyRole"
            @click="enter(role.role)"
          >
            <span class="flex h-9 w-9 shrink-0 items-center justify-center rounded-[6px] bg-[#E8EFF4] text-primary transition-colors group-hover:bg-primary group-hover:text-white">
              <span v-if="busyRole === role.role" class="h-4 w-4 animate-spin rounded-full border-2 border-current/30 border-t-current" />
              <AppIcon v-else :name="role.icon" :size="17" />
            </span>
            <span class="min-w-0 flex-1">
              <span class="flex items-center gap-2">
                <span class="text-[12.5px] font-semibold text-ink">{{ role.label }}</span>
                <span class="rounded bg-[#F1F4F7] px-1.5 py-[1px] text-[9px] text-[#6B7280]">{{ role.name }}</span>
              </span>
              <span class="mt-0.5 block text-[10.5px] text-muted">{{ role.desc }}</span>
            </span>
            <AppIcon name="arrowRight" :size="14" class="text-[#CBD5E1] transition-transform group-hover:translate-x-0.5 group-hover:text-primary" />
          </button>
        </div>

        <div class="mt-4 rounded-md border border-line bg-[#FAFBFC] p-3">
          <p class="flex items-center gap-1.5 text-[10px] font-medium text-[#4B5563]">
            <AppIcon name="info" :size="11" />演示说明
          </p>
          <ul class="mt-1.5 space-y-1 text-[9.5px] leading-relaxed text-muted">
            <li>· 无需密码，身份仅用于演示 RBAC 权限边界与差异化工作台。</li>
            <li>· 构件几何、体积与 cacheKey 均由服务端实时计算，非静态截图。</li>
            <li>· 可在顶栏头像菜单中随时切换身份。</li>
          </ul>
        </div>

        <p class="mt-6 text-center font-mono text-[9px] text-[#9CA3AF]">
          CBF-C v1.4.2 · JHB v1.4.3 · B/S Architecture v1.4
        </p>
      </div>
    </section>
  </div>
</template>
