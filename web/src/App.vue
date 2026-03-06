<template>
  <div class="cad-layout">
    <header class="ribbon">
      <div class="ribbon-item active">文件</div>
      <div class="ribbon-item">编辑</div>
      <div class="ribbon-item">视图</div>
      <div class="ribbon-item">建模</div>
      
      <button class="btn-generate" :disabled="!store.selectedModelKey || store.isLoading" @click="handleGenerate">
        🔨 生成预览
      </button>

      <div style="margin-left: auto; display: flex; gap: 10px;">
        <button class="ribbon-item" @click="handleFitAll">📺 全部适应</button>
        <button class="ribbon-item" @click="handleClear">🗑️ 清空</button>
      </div>
    </header>
    
    <aside class="sidebar">
      <ModelLibrary />
      <PropertyEditor />
    </aside>

    <main class="viewer-area">
      <Suspense>
        <template #default>
          <Viewer :key="viewerKey" ref="viewerRef" />
        </template>
        <template #fallback>
          <div style="color: white; padding: 20px;">加载 3D 核心中...</div>
        </template>
      </Suspense>

      <div v-if="store.isLoading" class="loading-overlay-app">
        正在执行建模脚本...
      </div>
    </main>

    <footer class="status-bar">
      <span>{{ store.statusInfo }}</span>
      <span>ActiCAD Engine | BitByBit | Babylon.js</span>
    </footer>
  </div>
</template>

<script setup lang="ts">
import { ref, watch, nextTick } from 'vue';
import Viewer from './components/Viewer.vue';
import ModelLibrary from './components/ModelLibrary.vue';
import PropertyEditor from './components/PropertyEditor.vue';
import { store } from './services/store';
import { CadService } from './services/cad-service';
import './style.css';

const viewerRef = ref<any>(null);
const viewerKey = ref(0);
let generateTimer: ReturnType<typeof setTimeout> | null = null;

watch(() => store.currentArgs, () => {
  if (generateTimer) clearTimeout(generateTimer);
  generateTimer = setTimeout(() => {
    if (store.selectedModelKey) {
      handleGenerate();
    }
  }, 500); // 500ms 防抖
}, { deep: true });

const handleGenerate = async () => {
  if (!store.selectedModelKey) return;
  
  store.isLoading = true;
  store.statusInfo = '正在生成模型...';
  
  // 强制销毁并重建 Viewer 组件，清除 BitByBit 引擎及 OCCT Worker 状态，避免第二次 drawAnyAsync 死锁
  viewerKey.value++;
  // 等待 Vue 完成 DOM 更新（unmount 旧组件，mount 新组件）
  await nextTick();
  // 再多等一个宏任务，确保 onMounted 内的 bbbService.init() 有机会完成
  await new Promise(resolve => setTimeout(resolve, 100));
  
  try {
    const result = await CadService.generateModel(store.selectedModelKey, store.currentArgs);
    if (viewerRef.value) {
      await viewerRef.value.renderModel(result.modelData);
    }
    store.statusInfo = '生成成功';
  } catch (e: any) {
    console.error('Generation failed:', e);
    store.statusInfo = '生成失败: ' + e.message;
    alert('错误: ' + e.message);
  } finally {
    store.isLoading = false;
  }
};

const handleFitAll = () => {
    // Current bitbybit integration handles this or we can expose via Viewer
};

const handleClear = () => {
  if (viewerRef.value) {
    viewerRef.value.clear();
  }
};
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
