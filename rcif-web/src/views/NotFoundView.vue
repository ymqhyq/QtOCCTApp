<script setup lang="ts">
import AppIcon from '@/components/ui/AppIcon.vue';
</script>

<template>
  <div class="flex min-h-[70vh] items-center justify-center px-4">
    <div class="max-w-md text-center">
      <div class="mx-auto flex h-14 w-14 items-center justify-center rounded-full border border-line bg-surface text-[#94A3B8] shadow-panel">
        <AppIcon name="compass" :size="26" :stroke-width="1.4" />
      </div>
      <p class="mt-4 font-mono text-[11px] tracking-[0.2em] text-[#9CA3AF]">404 · NOT FOUND</p>
      <h1 class="mt-1 text-[20px] font-semibold text-ink">页面不存在</h1>
      <p class="mt-1.5 text-[12px] leading-relaxed text-muted">
        您访问的路由未在 RCIF 构件库中登记，可能已被归档或链接已失效。
      </p>
      <div class="mt-4 flex items-center justify-center gap-2">
        <RouterLink to="/" class="btn btn-primary btn-sm">返回工作台</RouterLink>
        <RouterLink to="/library" class="btn btn-outline btn-sm">构件资产大厅</RouterLink>
      </div>
    </div>
  </div>
</template>
