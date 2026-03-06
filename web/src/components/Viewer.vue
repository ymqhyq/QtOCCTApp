<template>
  <div class="viewer-container">
    <canvas ref="canvasRef" class="render-canvas"></canvas>
    <div v-if="loading" class="loading-overlay">
      <div class="spinner"></div>
      <span>{{ loadingText }}</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue';
import { bbbService } from '../services/bitbybit-service';
import { Inputs } from '@bitbybit-dev/babylonjs';
import * as BABYLON from '@babylonjs/core';

const canvasRef = ref<HTMLCanvasElement | null>(null);
const loading = ref(true);
const loadingText = ref('正在初始化 3D 内核...');
let currentModelMeshes: any[] = [];
let currentMaterial: BABYLON.StandardMaterial | null = null;

onMounted(async () => {
  if (canvasRef.value) {
    try {
      console.log('Viewer: Starting 3D core initialization...');
      await bbbService.init(canvasRef.value);
      console.log('Viewer: 3D core initialized.');
      window.addEventListener('resize', handleResize);
    } catch (e) {
      console.error('Failed to init bitbybit:', e);
    } finally {
      loading.value = false;
    }
  }
});

onUnmounted(() => {
  window.removeEventListener('resize', handleResize);
  disposeCurrentModel();
  // Fully dispose bitbybit engine when Viewer is destroyed (App.vue remounts this component for each model load)
  if (bbbService.bitbybit && bbbService.bitbybit.context && (bbbService.bitbybit.context as any).engine) {
      (bbbService.bitbybit.context as any).engine.dispose();
      (bbbService as any).bitbybit = null;
  }
});

const handleResize = () => {
    if (bbbService.bitbybit && bbbService.bitbybit.context && (bbbService.bitbybit.context as any).engine) {
        (bbbService.bitbybit.context as any).engine.resize();
    }
};

const disposeCurrentModel = () => {
    if (currentModelMeshes && currentModelMeshes.length > 0) {
        currentModelMeshes.forEach((mesh: any) => {
            if (mesh && typeof mesh.dispose === 'function') {
                mesh.dispose(false, true);
            }
        });
        currentModelMeshes = [];
    }
    if (currentMaterial) {
        currentMaterial.dispose();
        currentMaterial = null;
    }
};

// Expose methods for parent
defineExpose({
    renderModel: async (data: Uint8Array) => {
        console.log('--- renderModel start ---');
        if (!bbbService.bitbybit) {
             console.error('bitbybit not initialized!');
             return;
        }
        
        try {
            loadingText.value = '正在处理并渲染新模型...';
            loading.value = true;
            
            const stepText = new TextDecoder().decode(data);
            
            console.log('Loading STEP into OCCT...');
            const shape = await bbbService.bitbybit.occt.io.loadSTEPorIGESFromText({
                text: stepText,
                fileType: Inputs.OCCT.fileTypeEnum.step,
                adjustZtoY: true
            });
            console.log('OCCT load result:', shape ? 'Success' : 'Undefined null');
            
            if (shape) {
                disposeCurrentModel();

                const bbbScene = bbbService.bitbybit.context.scene as unknown as BABYLON.Scene;

                const matName = "cadMaterial_" + Date.now();
                const cadMaterial = new BABYLON.StandardMaterial(matName, bbbScene);
                cadMaterial.diffuseColor = new BABYLON.Color3(0.65, 0.65, 0.65);
                cadMaterial.specularColor = new BABYLON.Color3(0.4, 0.4, 0.4);
                cadMaterial.ambientColor = new BABYLON.Color3(0.3, 0.3, 0.3);
                cadMaterial.emissiveColor = new BABYLON.Color3(0.15, 0.15, 0.15);
                cadMaterial.backFaceCulling = false;
                
                currentMaterial = cadMaterial;
                
                console.log('Drawing CAD shape via bitbybit...');
                const modelMesh = await bbbService.bitbybit.draw.drawAnyAsync({
                    entity: shape,
                    options: {
                        faceColour: '#e0e0e0',
                        edgeColour: '#000000',
                        drawEdges: true,
                        edgeWidth: 2
                    } as any
                });
                
                console.log('CAD shape drawn:', !!modelMesh);
                
                if (modelMesh) {
                    const targetMeshes = Array.isArray(modelMesh) ? modelMesh : [modelMesh];
                    
                    let allMeshes: BABYLON.AbstractMesh[] = [];
                    targetMeshes.forEach(node => {
                        if (node instanceof BABYLON.Mesh) {
                            allMeshes.push(node);
                        }
                        if (node.getChildMeshes) {
                            allMeshes = allMeshes.concat(node.getChildMeshes());
                        }
                    });
                    
                    currentModelMeshes = ([...targetMeshes, ...allMeshes] as any[]);
                    
                    targetMeshes.forEach(mesh => {
                        if (mesh && typeof mesh.material !== 'undefined') {
                            mesh.material = cadMaterial;
                        }
                    });

                    const bbbCamera = bbbScene.activeCamera as import("@babylonjs/core").ArcRotateCamera;
                    if (bbbCamera) {
                        bbbCamera.useFramingBehavior = true;
                        try {
                            const framingBehavior = bbbCamera.getBehaviorByName("Framing") as any;
                            if (framingBehavior) {
                                setTimeout(() => {
                                    framingBehavior.zoomOnMeshesHierarchy(targetMeshes);
                                }, 50);
                            }
                        } catch (camErr) {
                            console.warn('Ignored error in camera framing:', camErr);
                        }
                    }
                }
            } else {
                console.error('Failed to parse STEP file: result is undefined');
            }
        } catch (e: any) {
            console.error('Error parsing and rendering STEP model:', e);
            alert("模型加载失败：" + String(e));
        } finally {
            console.log('--- renderModel finished ---');
            loading.value = false;
        }
    },
    fitAll: async () => {
        if (!bbbService.bitbybit) return;
        const bbbScene = bbbService.bitbybit.context.scene as unknown as import("@babylonjs/core").Scene;
        const bbbCamera = bbbScene.activeCamera as import("@babylonjs/core").ArcRotateCamera;
        if (bbbCamera && bbbScene.meshes && bbbScene.meshes.length > 0) {
            bbbCamera.useFramingBehavior = true;
            const framingBehavior = bbbCamera.getBehaviorByName("Framing") as any;
            if (framingBehavior) {
                framingBehavior.zoomOnMeshesHierarchy(bbbScene.meshes);
            }
        }
    },
    clear: async () => {
        disposeCurrentModel();
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
