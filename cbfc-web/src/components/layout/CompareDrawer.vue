<script setup lang="ts">
import { computed } from 'vue';
import { useRouter } from 'vue-router';
import { storeToRefs } from 'pinia';
import ModalShell from '@/components/ui/ModalShell.vue';
import AppIcon from '@/components/ui/AppIcon.vue';
import StatusBadge from '@/components/ui/StatusBadge.vue';
import { api } from '@/api/client';
import { useCompareStore } from '@/stores/compare';
import { componentStatusMeta, formatNumber, formatVolume, maskKey, sourceModeMeta } from '@/utils/format';

const router = useRouter();
const compare = useCompareStore();
const { items, open } = storeToRefs(compare);

const paramKeys = computed(() => {
  const keys = new Set<string>();
  for (const item of items.value) {
    for (const key of Object.keys(item.keyParams)) {
      if (!['StrengthGrade', 'ModelNumber'].includes(key)) keys.add(key);
    }
  }
  return [...keys].slice(0, 6);
});

const volumeMax = computed(() => Math.max(...items.value.map((i) => i.geometry.volumeM3)));

function labelFor(key: string) {
  const labels: Record<string, string> = {
    PierHeight: '墩高',
    HeightZ: '高度',
    LengthX: '长度',
    WidthY: '宽度',
    TopWidth: '墩顶宽',
    TopLength: '墩顶长',
    Diameter: '直径',
    Depth: '梁高',
    OuterDiameter: '外径',
  };
  return labels[key] ?? key;
}

function openDetail(familyId: string) {
  open.value = false;
  router.push(`/components/${familyId}`);
}
</script>

<template>
  <ModalShell
    :open="open"
    title="多模型参数与算量深度对比"
    :subtitle="`已选 ${items.length} 个构件 · 体积、关键参数、拓扑与质检指标横向对齐`"
    icon="gitCompare"
    width="full"
    @close="open = false"
  >
    <div class="p-4">
      <div v-if="!items.length" class="py-12 text-center text-[13px] text-muted">对比栏为空</div>

      <div v-else class="overflow-x-auto">
        <table class="w-full border-collapse text-[12px]">
          <thead>
            <tr>
              <th class="w-[150px] border-b border-line px-3 py-2 text-left text-[10px] font-semibold uppercase tracking-wider text-[#9CA3AF]">
                对比维度
              </th>
              <th
                v-for="item in items"
                :key="item.familyId"
                class="min-w-[220px] border-b border-l border-line px-3 py-2 text-left align-top"
              >
                <div class="flex items-start gap-2.5">
                  <span class="flex h-14 w-[72px] shrink-0 items-center justify-center overflow-hidden rounded border border-line bg-[#F5F7F9]">
                    <img :src="api.previewUrl(item.familyId, 144, 108)" alt="" class="h-full w-full object-contain" />
                  </span>
                  <span class="min-w-0">
                    <button class="block truncate text-left text-[12.5px] font-semibold text-ink hover:text-primary" @click="openDetail(item.familyId)">
                      {{ item.name }}
                    </button>
                    <span class="mt-0.5 block font-mono text-[10px] text-muted">{{ item.drawingCode }}</span>
                    <StatusBadge
                      class="mt-1"
                      :tone="componentStatusMeta(item.status).tone"
                      :label="componentStatusMeta(item.status).label"
                    />
                  </span>
                </div>
              </th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">分类 / 编码</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2">
                <p class="text-[11.5px] text-ink">{{ item.taxonomy.crbim }} · {{ item.category }}</p>
                <p class="mt-0.5 font-mono text-[10px] text-muted">{{ item.semanticCode }}</p>
              </td>
            </tr>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">构件能力</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2">
                <StatusBadge :tone="sourceModeMeta(item.sourceMode).tone" :label="sourceModeMeta(item.sourceMode).label" :dot="false" />
                <span class="ml-1.5 text-[10.5px] text-muted">{{ item.engine }}</span>
              </td>
            </tr>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">实体净体积</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2">
                <span
                  class="mono rounded px-1 py-0.5 text-[13px] font-semibold"
                  :class="item.geometry.volumeM3 === volumeMax ? 'bg-[#ECFDF3] text-success' : 'text-ink'"
                >
                  {{ formatVolume(item.geometry.volumeM3) }}
                </span>
                <span v-if="item.geometry.volumeM3 === volumeMax" class="ml-1 text-[9.5px] text-success">最大</span>
              </td>
            </tr>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">包围盒 高 / 长 / 宽</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2 font-mono text-[11px] text-[#374151]">
                {{ formatNumber(item.dims.height / 1000, 2) }} / {{ formatNumber(item.dims.length / 1000, 2) }} /
                {{ formatNumber(item.dims.width / 1000, 2) }} m
              </td>
            </tr>
            <tr v-for="key in paramKeys" :key="key">
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">
                {{ labelFor(key) }}
                <span class="ml-1 font-mono text-[9.5px] text-[#B0B8C1]">{{ key }}</span>
              </td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2 font-mono text-[11.5px] text-ink">
                {{ item.keyParams[key] ?? '—' }}
              </td>
            </tr>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">拓扑规模</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2 font-mono text-[11px] text-[#374151]">
                {{ formatNumber(item.geometry.vertices) }} 顶点 / {{ formatNumber(item.geometry.triangles) }} 面片
              </td>
            </tr>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">质检 / 命中</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2">
                <span class="inline-flex items-center gap-1 text-[11.5px]" :class="item.qa.passed ? 'text-success' : 'text-danger'">
                  <AppIcon :name="item.qa.passed ? 'checkCircle' : 'xCircle'" :size="13" />
                  {{ item.qa.passed ? '6/6 门禁通过' : '门禁未通过' }}
                </span>
                <span class="ml-2 mono text-[10.5px] text-muted">{{ item.qa.score }} 分</span>
              </td>
            </tr>
            <tr>
              <td class="border-b border-[#F0F2F5] px-3 py-2 text-muted">权威 cacheKey</td>
              <td v-for="item in items" :key="item.familyId" class="border-b border-l border-[#F0F2F5] px-3 py-2">
                <span class="mono text-[10px] text-[#4B5563]" :title="item.geometry.defaultCacheKey">
                  {{ maskKey(item.geometry.defaultCacheKey, 12, 6) }}
                </span>
              </td>
            </tr>
            <tr>
              <td class="px-3 py-2.5 text-muted">操作</td>
              <td v-for="item in items" :key="item.familyId" class="border-l border-[#F0F2F5] px-3 py-2.5">
                <div class="flex flex-wrap gap-1.5">
                  <button class="btn btn-primary btn-sm" @click="openDetail(item.familyId)">
                    查看详情
                  </button>
                  <RouterLink
                    v-if="item.sourceMode === 'parametric'"
                    :to="`/components/${item.familyId}/configurator`"
                    class="btn btn-outline btn-sm"
                  >
                    配置参数
                  </RouterLink>
                </div>
              </td>
            </tr>
          </tbody>
        </table>
      </div>
    </div>

    <template #footer>
      <div class="flex items-center justify-between">
        <p class="text-[10.5px] text-muted">对比最多 4 个构件；体积为散度定理精确计算值。</p>
        <div class="flex items-center gap-2">
          <button class="btn btn-ghost btn-sm" @click="compare.clear()">
            <AppIcon name="close" :size="12" />清空选择
          </button>
          <button class="btn btn-primary btn-sm" @click="open = false">完成对比</button>
        </div>
      </div>
    </template>
  </ModalShell>
</template>
