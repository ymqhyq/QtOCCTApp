<template>
  <div class="cad-layout">
    <header class="ribbon">
      <div class="ribbon-item active">文件</div>
      <div class="ribbon-item">编辑</div>
      <div class="ribbon-item">视图</div>
      <div class="ribbon-item">建模</div>
      <div class="ribbon-item">帮助</div>
    </header>
    
    <aside class="sidebar">
      <div class="sidebar-header">模型树</div>
      <div class="sidebar-content">
        <div class="tree-item">默认场景</div>
      </div>
      <div class="sidebar-header">属性面板</div>
      <div class="sidebar-content">
        <p style="font-size: 12px; color: #888;">未选中对象</p>
      </div>
    </aside>

    <main class="viewer-area">
      <Suspense>
        <template #default>
          <Viewer ref="viewerRef" />
        </template>
        <template #fallback>
          <div style="color: white; padding: 20px;">加载 3D 核心中...</div>
        </template>
      </Suspense>
    </main>

    <footer class="status-bar">
      <span>就绪</span>
      <span>UI 测试模式</span>
    </footer>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import Viewer from './components/Viewer.vue';
import './style.css';

const viewerRef = ref(null);
</script>

<style>
/* Global resets are in style.css */
.tree-item {
  padding: 5px 8px;
  font-size: 13px;
  cursor: pointer;
}
.tree-item:hover {
  background-color: var(--bg-tertiary);
}
</style>
