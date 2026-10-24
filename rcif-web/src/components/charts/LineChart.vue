<script setup lang="ts">
import { computed } from 'vue';

const props = withDefaults(
  defineProps<{
    data: { label: string; value: number }[];
    height?: number;
    color?: string;
    unit?: string;
    digits?: number;
  }>(),
  { height: 200, color: '#1F5A7A', unit: '', digits: 0 },
);

const W = 640;
const H = computed(() => props.height);
const pad = { left: 44, right: 14, top: 14, bottom: 26 };

const values = computed(() => props.data.map((d) => d.value));
const min = computed(() => {
  const raw = Math.min(...values.value);
  return Math.max(0, Math.floor(raw - (Math.max(...values.value) - raw) * 0.15));
});
const max = computed(() => {
  const raw = Math.max(...values.value);
  return raw === min.value ? raw + 1 : Math.ceil(raw + (raw - min.value) * 0.12);
});
const points = computed(() =>
  props.data.map((d, i) => {
    const x = pad.left + (i / Math.max(1, props.data.length - 1)) * (W - pad.left - pad.right);
    const y = pad.top + (1 - (d.value - min.value) / Math.max(1, max.value - min.value)) * (H.value - pad.top - pad.bottom);
    return { x, y, ...d };
  }),
);
const linePath = computed(() => points.value.map((p, i) => `${i === 0 ? 'M' : 'L'}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' '));
const areaPath = computed(() => {
  if (!points.value.length) return '';
  const last = points.value[points.value.length - 1];
  return `${linePath.value} L${last.x.toFixed(1)},${H.value - pad.bottom} L${pad.left},${H.value - pad.bottom} Z`;
});
const ticks = computed(() =>
  Array.from({ length: 4 }, (_, i) => {
    const value = min.value + ((max.value - min.value) * i) / 3;
    const y = pad.top + (1 - i / 3) * (H.value - pad.top - pad.bottom);
    return { value, y };
  }),
);
const labelStep = computed(() => Math.max(1, Math.ceil(props.data.length / 6)));

function format(value: number) {
  return new Intl.NumberFormat('zh-CN', { maximumFractionDigits: props.digits }).format(value);
}
</script>

<template>
  <svg :viewBox="`0 0 ${W} ${height}`" class="h-auto w-full" role="img" aria-label="趋势折线图">
    <defs>
      <linearGradient :id="`area-${color.replace('#', '')}`" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0%" :stop-color="color" stop-opacity="0.18" />
        <stop offset="100%" :stop-color="color" stop-opacity="0" />
      </linearGradient>
    </defs>
    <g>
      <line
        v-for="tick in ticks"
        :key="tick.value"
        :x1="pad.left"
        :x2="W - pad.right"
        :y1="tick.y"
        :y2="tick.y"
        stroke="#EEF1F4"
        stroke-width="1"
      />
      <text
        v-for="tick in ticks"
        :key="`label-${tick.value}`"
        :x="pad.left - 7"
        :y="tick.y + 3"
        text-anchor="end"
        font-size="9"
        fill="#9CA3AF"
        font-family="JetBrains Mono, Consolas, monospace"
      >
        {{ format(tick.value) }}
      </text>
    </g>
    <path :d="areaPath" :fill="`url(#area-${color.replace('#', '')})`" />
    <path :d="linePath" fill="none" :stroke="color" stroke-width="2" stroke-linejoin="round" stroke-linecap="round" />
    <g v-for="(point, index) in points" :key="point.label">
      <circle :cx="point.x" :cy="point.y" r="2.4" :fill="color" stroke="#fff" stroke-width="1.2" />
      <circle :cx="point.x" :cy="point.y" r="8" fill="transparent">
        <title>{{ point.label }} · {{ format(point.value) }}{{ unit }}</title>
      </circle>
      <text
        v-if="index % labelStep === 0 || index === points.length - 1"
        :x="point.x"
        :y="height - 8"
        text-anchor="middle"
        font-size="8.5"
        fill="#9CA3AF"
      >
        {{ point.label.slice(5) }}
      </text>
    </g>
  </svg>
</template>
