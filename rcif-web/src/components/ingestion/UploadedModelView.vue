<script setup lang="ts">
import { onBeforeUnmount, onMounted, ref, watch } from 'vue';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { GLTFLoader } from 'three/examples/jsm/loaders/GLTFLoader.js';

const props = defineProps<{ url: string }>();
const host = ref<HTMLDivElement | null>(null);
const loading = ref(false);
const error = ref('');

let renderer: THREE.WebGLRenderer | null = null;
let camera: THREE.PerspectiveCamera | null = null;
let scene: THREE.Scene | null = null;
let controls: OrbitControls | null = null;
let root: THREE.Object3D | null = null;
let resizeObserver: ResizeObserver | null = null;
let frame = 0;

function disposeRoot() {
  if (!root) return;
  scene?.remove(root);
  root.traverse((child) => {
    const mesh = child as THREE.Mesh;
    mesh.geometry?.dispose();
    const material = mesh.material;
    if (Array.isArray(material)) material.forEach((item) => item.dispose());
    else material?.dispose();
  });
  root = null;
}

function fit() {
  if (!camera || !controls || !root) return;
  const box = new THREE.Box3().setFromObject(root);
  const size = box.getSize(new THREE.Vector3());
  const center = box.getCenter(new THREE.Vector3());
  const radius = Math.max(size.x, size.y, size.z, 1);
  camera.position.set(center.x + radius * 1.3, center.y + radius * 0.9, center.z + radius * 1.4);
  camera.near = Math.max(0.1, radius / 1000);
  camera.far = radius * 50;
  camera.updateProjectionMatrix();
  controls.target.copy(center);
  controls.update();
}

async function load(url: string) {
  if (!scene || !url) return;
  loading.value = true;
  error.value = '';
  disposeRoot();
  try {
    const gltf = await new GLTFLoader().loadAsync(url);
    root = gltf.scene;
    root.traverse((child) => {
      const mesh = child as THREE.Mesh;
      if (mesh.isMesh) {
        mesh.castShadow = false;
        mesh.receiveShadow = false;
        const material = mesh.material as THREE.MeshStandardMaterial;
        material.side = THREE.DoubleSide;
      }
    });
    scene.add(root);
    fit();
  } catch (loadError) {
    error.value = loadError instanceof Error ? loadError.message : 'GLB 加载失败';
  } finally {
    loading.value = false;
  }
}

function init() {
  if (!host.value) return;
  scene = new THREE.Scene();
  scene.background = new THREE.Color('#0B1220');
  camera = new THREE.PerspectiveCamera(35, 1, 1, 100000);
  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: false });
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  renderer.domElement.style.display = 'block';
  renderer.domElement.style.width = '100%';
  renderer.domElement.style.height = '100%';
  host.value.appendChild(renderer.domElement);

  scene.add(new THREE.HemisphereLight(0xffffff, 0x1e293b, 1.7));
  const key = new THREE.DirectionalLight(0xffffff, 2.2);
  key.position.set(1, 1.5, 2);
  scene.add(key);
  const fill = new THREE.DirectionalLight(0x93c5fd, 0.7);
  fill.position.set(-2, 1, -1);
  scene.add(fill);
  const grid = new THREE.GridHelper(10000, 20, 0x334155, 0x1e293b);
  grid.position.y = -0.01;
  scene.add(grid);

  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  controls.dampingFactor = 0.08;

  const resize = () => {
    if (!host.value || !renderer || !camera) return;
    const width = host.value.clientWidth;
    const height = host.value.clientHeight;
    renderer.setSize(width, height, false);
    camera.aspect = width / Math.max(height, 1);
    camera.updateProjectionMatrix();
  };
  resizeObserver = new ResizeObserver(resize);
  resizeObserver.observe(host.value);
  resize();

  const animate = () => {
    frame = requestAnimationFrame(animate);
    controls?.update();
    if (renderer && scene && camera) renderer.render(scene, camera);
  };
  animate();
  void load(props.url);
}

watch(
  () => props.url,
  (url) => void load(url),
);

onMounted(init);
onBeforeUnmount(() => {
  cancelAnimationFrame(frame);
  resizeObserver?.disconnect();
  controls?.dispose();
  disposeRoot();
  renderer?.dispose();
  renderer?.domElement.remove();
  renderer = null;
  scene = null;
  camera = null;
});
</script>

<template>
  <div class="relative h-full w-full overflow-hidden bg-[#0B1220]">
    <div ref="host" class="h-full w-full" />
    <div v-if="loading" class="absolute inset-0 flex items-center justify-center bg-[#0B1220]/60">
      <span class="h-5 w-5 animate-spin rounded-full border-2 border-white/20 border-t-[#38BDF8]" />
      <span class="ml-2 text-[10px] text-[#94A3B8]">加载上传构件几何…</span>
    </div>
    <div v-if="error" class="absolute inset-x-3 bottom-3 rounded border border-[#7F1D1D] bg-[#2A1215]/90 px-2 py-1.5 text-[10px] text-[#FCA5A5]">
      {{ error }}
    </div>
  </div>
</template>
