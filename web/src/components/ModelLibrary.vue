<template>
  <div class="model-library">
    <div class="sidebar-header">模型组件库</div>
    <div class="scroll-area">
      <ul class="model-list">
        <li 
          v-for="(schema, key) in store.schemas" 
          :key="key"
          class="model-item"
          :class="{ active: store.selectedModelKey === key }"
          @click="store.selectModel(key as string)"
        >
          {{ (schema as any).name || key }}
        </li>
      </ul>
    </div>
  </div>
</template>

<script setup lang="ts">
import { onMounted } from 'vue';
import { CadService } from '../services/cad-service';
import { store } from '../services/store';

onMounted(async () => {
  try {
    const schemas = await CadService.fetchSchemas();
    console.log("Fetched schemas response:", schemas);
    store.setSchemas(schemas);
  } catch (e) {
    console.error('Failed to fetch schemas:', e);
  }
});
</script>

<style scoped>
.model-library {
  height: 50%;
  display: flex;
  flex-direction: column;
  border-bottom: 1px solid var(--border-primary);
}

.model-list {
  list-style: none;
  padding: 0;
  margin: 0;
}

.model-item {
  padding: 8px 15px;
  cursor: pointer;
  font-size: 13px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
  transition: background 0.2s;
}

.model-item:hover {
  background: rgba(255, 255, 255, 0.05);
}

.model-item.active {
  background: rgba(74, 158, 255, 0.1);
  color: var(--cad-blue);
  border-left: 3px solid var(--cad-blue);
}

.scroll-area {
  flex: 1;
  overflow-y: auto;
}
</style>
