<template>
  <div class="viewer-container">
    <canvas ref="canvasRef" class="render-canvas"></canvas>
    <div v-if="loading" class="loading-overlay">
      <div class="spinner"></div>
      <span>正在初始化 3D 内核...</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue';
import { bbbService } from '../services/bitbybit-service';

const canvasRef = ref<HTMLCanvasElement | null>(null);
const loading = ref(true);

onMounted(async () => {
  if (canvasRef.value) {
    try {
      await bbbService.init(canvasRef.value);
      loading.value = false;
      
      // Handle resize
      window.addEventListener('resize', handleResize);
    } catch (e) {
      console.error('Failed to init bitbybit:', e);
    }
  }
});

onUnmounted(() => {
  window.removeEventListener('resize', handleResize);
});

const handleResize = () => {
    // Babylon handles resize internally but we can trigger it if needed
};

// Expose methods for parent
defineExpose({
    renderBrep: async (data: Uint8Array) => {
        if (!bbbService.bitbybit) return;
        
        try {
            loading.value = true;
            // Use bitbybit to load BREP
            const shape = await bbbService.bitbybit.occt.io.loadBrep({
                data: data,
                filename: 'model.brep'
            });
            
            // Draw the shape with enhanced options
            await bbbService.bitbybit.babylon.draw.drawAnyAsync({
                entity: shape,
                options: {
                    faceOpacity: 1,
                    edgeOpacity: 1,
                    edgeWidth: 2,
                    faceColour: '#4a9eff',
                    edgeColour: '#ffffff'
                }
            });

            // Zoom to extend
            await bbbService.bitbybit.babylon.camera.adjustActiveArcRotateCameraToSceneExtents();
        } catch (e) {
            console.error('Error rendering BREP:', e);
        } finally {
            loading.value = false;
        }
    },
    clear: async () => {
        await bbbService.clearScene();
    }
});
</script>

<style scoped>
.viewer-container {
  position: relative;
  width: 100%;
  height: 100%;
  background-color: #1a1a1b;
}

.render-canvas {
  width: 100%;
  height: 100%;
  outline: none;
}

.loading-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0,0,0,0.7);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: white;
  z-index: 10;
}

.spinner {
  width: 40px;
  height: 40px;
  border: 4px solid #f3f3f3;
  border-top: 4px solid #4a9eff;
  border-radius: 50%;
  animation: spin 1s linear infinite;
  margin-bottom: 15px;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}
</style>
