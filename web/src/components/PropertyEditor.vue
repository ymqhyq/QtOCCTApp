<template>
  <div class="property-editor">
    <div class="sidebar-header">
      参数面板 <span v-if="store.selectedModelKey" class="model-name">[{{ (store.schemas[store.selectedModelKey] as any).name }}]</span>
    </div>
    <div class="scroll-area">
      <div v-if="store.selectedModelKey" class="property-form">
        <div v-for="(propMeta, propKey) in currentSchema" :key="propKey" class="form-group">
          <label :for="'input_' + propKey">{{ (propMeta as any).name || propKey }}</label>
          <input 
            :id="'input_' + propKey" 
            type="text" 
            v-model="store.currentArgs[propKey]"
            @change="handleUpdate(propKey, $event)"
          />
        </div>
      </div>
      <div v-else class="empty-state">
        选择模型后编辑参数
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { store } from '../services/store';

const currentSchema = computed(() => {
  if (!store.selectedModelKey) return {};
  const schema = { ...store.schemas[store.selectedModelKey] };
  delete (schema as any).name;
  return schema;
});

const handleUpdate = (key: string, event: Event) => {
  const target = event.target as HTMLInputElement;
  const val = target.value;
  
  // Try to preserve type
  const original = (store.schemas[store.selectedModelKey!] as any)[key].value;
  if (typeof original === 'number') {
    store.updateArg(key, parseFloat(val));
  } else {
    store.updateArg(key, val);
  }
};
</script>

<style scoped>
.property-editor {
  height: 50%;
  display: flex;
  flex-direction: column;
}

.model-name {
  color: var(--cad-blue);
  font-size: 11px;
}

.property-form {
  padding: 15px;
}

.form-group {
  margin-bottom: 12px;
}

.form-group label {
  display: block;
  font-size: 11px;
  color: var(--text-secondary);
  margin-bottom: 4px;
}

.form-group input {
  width: 100%;
  background: var(--bg-tertiary);
  border: 1px solid var(--border-primary);
  color: var(--text-primary);
  padding: 5px 8px;
  border-radius: 4px;
  font-size: 13px;
  box-sizing: border-box;
}

.form-group input:focus {
  outline: none;
  border-color: var(--cad-blue);
}

.empty-state {
  text-align: center;
  color: var(--text-secondary);
  margin-top: 40px;
  font-size: 12px;
}

.scroll-area {
  flex: 1;
  overflow-y: auto;
}
</style>
