/**
 * RCIF 构件创作工作台（DSH 宿主原生流程）
 *
 * 七个功能区：元数据 / 3D / 参数 / 构建脚本 / 命名部位 / 型号列表 / 构件入库
 * 底部对话框把自然语言、代码批注、3D 截图回传给当前 Agent 会话；
 * Agent 完成转换/重建后，通过 /api/state 自动把新构件包推送到本页。
 */

import * as THREE from 'three';
import { OrbitControls } from './vendor/controls/OrbitControls.js';
import { GLTFLoader } from './vendor/loaders/GLTFLoader.js';
import { LineSegments2 } from './vendor/lines/LineSegments2.js';
import { LineSegmentsGeometry } from './vendor/lines/LineSegmentsGeometry.js';
import { LineMaterial } from './vendor/lines/LineMaterial.js';

const API = '/rcif-studio/api';

const state = {
  sessionId: 'session-default',
  capToken: null,
  sessions: [],
  assistantUrl: '',
  backend: { ready: false, agent: false, ingest: [], libraryUrl: '', kind: '' },
  packagePath: null,
  meta: null,
  component: null,
  metadata: null,
  presets: [],
  schema: [],
  shapes: [],
  source: { available: false, path: null, text: '' },
  selectedLines: [1, 3],
  selectionAnchor: 1,
  dragSelecting: false,
  codeLines: [],
  annotationDrafts: [],
  annotationHistory: [],
  editingDraftId: null,
  activeAnnotationId: null,
  annotationDialogLines: [1, 1],
  annotationDialogEditId: null,
  selectedPreset: null,
  previewPreset: null,
  presetPreviewCache: new Map(),
  selectedPart: null,
  screenshots: [],
  screenshotModalIndex: null,
  screenshotMarkMode: false,
  screenshotMarkStart: null,
  screenshotMarkRect: null,
  screenshotMarks: [],
  screenshotMarkEditing: null,
  projection: 'perspective',
  renderMode: 'shaded',
  edgesVisible: true,
  lineWidth: 1.0,
  clipAxis: null,
  clipRatio: 76,
  clippingPlane: null,
  edgeLines: [],
  viewToolsHidden: false,
  modelRadius: 20000,
  measureMode: false,
  measurePoints: [],
  measureObjects: [],
  measureResult: null,
  faceHighlights: [],
  previewRequestId: 0,
  viewerPartsOpen: false,
  packageEditMode: null,
  packageEditData: null,
  scene: null,
  camera: null,
  renderer: null,
  controls: null,
  meshParts: {},
  modelRoot: null,
  axesHelper: null,
  pollTimer: null,
  loading: false,
  lastError: '',
  legacy: false,
  autoBound: false,
  treeCollapsed: false,
  annotationCollapsed: false,
  screenshotCollapsed: false,
  trace: [],
  tree: [],
  workspace: null,
  sessionInfo: null,
  assistantRoot: '',
  sessionDirty: false,
  collapsedWorkspaces: new Set(),
  pendingApproval: null,
};

const COLLAPSED_WORKSPACES_KEY = 'rcif:collapsed-workspaces';

function loadCollapsedWorkspaces() {
  try {
    const raw = window.localStorage.getItem(COLLAPSED_WORKSPACES_KEY);
    const list = raw ? JSON.parse(raw) : [];
    return new Set(Array.isArray(list) ? list.filter((id) => typeof id === 'string' && id) : []);
  } catch {
    return new Set();
  }
}

function saveCollapsedWorkspaces() {
  try {
    window.localStorage.setItem(
      COLLAPSED_WORKSPACES_KEY,
      JSON.stringify(Array.from(state.collapsedWorkspaces)),
    );
  } catch {
    /* localStorage 不可用 */
  }
}

function setWorkspaceCollapsed(workspaceId, collapsed) {
  if (!workspaceId) return;
  if (collapsed) state.collapsedWorkspaces.add(workspaceId);
  else state.collapsedWorkspaces.delete(workspaceId);
  saveCollapsedWorkspaces();
}

function expandWorkspace(workspaceId) {
  if (!workspaceId) return;
  if (state.collapsedWorkspaces.delete(workspaceId)) saveCollapsedWorkspaces();
}

state.collapsedWorkspaces = loadCollapsedWorkspaces();

// ------------------------------------------------------------------ helpers

function trace(...parts) {
  state.trace.push(`${new Date().toISOString().slice(11, 19)} ${parts.join(' ')}`);
  if (state.trace.length > 80) state.trace.shift();
}

const byId = (id) => document.getElementById(id);

function setText(id, value) {
  const node = byId(id);
  if (node) node.textContent = value === undefined || value === null || value === '' ? '—' : String(value);
}

function setHidden(id, hidden) {
  const node = byId(id);
  if (node) node.hidden = hidden;
}

function badge(id, text, kind = '') {
  const node = byId(id);
  if (!node) return;
  node.textContent = text;
  node.className = `badge${kind ? ` ${kind}` : ''}`;
}

function formatBytes(bytes) {
  const value = Number(bytes) || 0;
  if (value < 1024) return `${value} B`;
  if (value < 1024 * 1024) return `${(value / 1024).toFixed(1)} KB`;
  return `${(value / 1024 / 1024).toFixed(2)} MB`;
}

function formatNumber(value, digits = 3) {
  const number = Number(value);
  if (!Number.isFinite(number)) return '—';
  return number.toLocaleString('zh-CN', { maximumFractionDigits: digits });
}

function mono(text) {
  const code = document.createElement('code');
  code.textContent = String(text ?? '');
  code.className = 'mono';
  return code;
}

async function api(path, options = {}, allowTokenRetry = true) {
  const method = options.method || 'GET';
  let target = path;
  if (method === 'GET' && (target.startsWith('/state') || target.startsWith('/package'))) {
    const separator = target.includes('?') ? '&' : '?';
    target = `${target}${separator}sessionId=${encodeURIComponent(state.sessionId)}`;
  }
  const headers = {};
  if (state.capToken) headers['X-RCIF-Cap-Token'] = state.capToken;
  if (options.body !== undefined) headers['Content-Type'] = 'application/json';
  const response = await fetch(`${API}${target}`, {
    method,
    headers,
    body: options.body === undefined ? undefined : JSON.stringify(options.body),
  });
  const contentType = response.headers.get('content-type') || '';
  const data = contentType.includes('application/json') ? await response.json() : await response.text();
  if (!response.ok) {
    if (allowTokenRetry && response.status === 403 && data?.code === 'CAP_TOKEN_INVALID') {
      if (await connectSession(state.sessionId)) return api(path, options, false);
    }
    const message = typeof data === 'object' && data ? data.error || data.message : String(data);
    throw new Error(message || `HTTP ${response.status}`);
  }
  return data;
}

function setFeedback(text, kind = 'info') {
  const node = byId('feedbackStatus');
  if (!node) return;
  node.textContent = text;
  node.dataset.kind = kind;
}

function setStatus(id, text, kind = 'info') {
  const node = byId(id);
  if (!node) return;
  node.textContent = text;
  node.dataset.kind = kind;
  node.hidden = false;
}

// ---------------------------------------------------------------- navigation

function updateFullscreenButtons() {
  const active = Boolean(document.fullscreenElement);
  const threeActive = document.fullscreenElement === byId('view-3d');
  if (byId('btnFullscreen')) {
    byId('btnFullscreen').textContent = active ? '退出全屏' : '全屏';
    byId('btnFullscreen').title = active ? '退出全屏' : '工作台全屏';
  }
  if (byId('btnFullscreen3D')) {
    byId('btnFullscreen3D').textContent = threeActive ? '退出全屏' : '全屏';
    byId('btnFullscreen3D').title = threeActive ? '退出 3D 全屏' : '3D 视图全屏';
  }
}

async function toggleFullscreen(target) {
  try {
    if (document.fullscreenElement) {
      await document.exitFullscreen();
      return;
    }
    if (!document.fullscreenEnabled) {
      setFeedback('当前页面未授权全屏；请在独立浏览器标签页打开工作台，或允许 iframe 全屏', 'warn');
      return;
    }
    await target.requestFullscreen();
  } catch (error) {
    setFeedback(`全屏失败：${error.message}`, 'warn');
  }
}

async function toggle3DFullscreen() {
  const view = byId('view-3d');
  if (!view) return;
  try {
    if (document.fullscreenElement === view) {
      await document.exitFullscreen();
      return;
    }
    // 3D 全屏要求先退出工作台全屏，再请求 3D section 全屏。
    if (document.fullscreenElement) {
      await document.exitFullscreen();
      await new Promise((resolve) => window.setTimeout(resolve, 60));
    }
    if (!document.fullscreenEnabled) {
      setFeedback('当前页面未授权全屏；请在独立浏览器标签页打开工作台，或允许 iframe 全屏', 'warn');
      return;
    }
    await view.requestFullscreen();
  } catch (error) {
    setFeedback(`3D 全屏失败：${error.message}`, 'warn');
  }
}

function handleFullscreenChange() {
  updateFullscreenButtons();
  window.setTimeout(onWindowResize, 80);
  window.setTimeout(onWindowResize, 300);
  window.setTimeout(positionAnnotationButton, 320);
}

function switchView(viewId) {
  document.querySelectorAll('.wb-tab').forEach((button) => {
    button.classList.toggle('active', button.dataset.view === viewId);
  });
  document.querySelectorAll('.subview').forEach((view) => {
    view.classList.toggle('active', view.id === viewId);
  });
  if (viewId === 'view-3d' && state.renderer) {
    onWindowResize();
    requestAnimationFrame(() => {
      onWindowResize();
      if (state.renderer && state.scene && state.camera) state.renderer.render(state.scene, state.camera);
    });
  }
  if (viewId === 'view-script') {
    requestAnimationFrame(positionAnnotationButton);
  } else {
    setHidden('btnFloatAnnotation', true);
  }
}

// ---------------------------------------------------------------------- 3D

function cameraAspect() {
  const container = byId('cadCanvasContainer');
  return container ? container.clientWidth / Math.max(1, container.clientHeight) : 1;
}

function createCamera(mode) {
  const aspect = cameraAspect();
  const radius = Math.max(state.modelRadius || 20000, 1);
  let camera;
  if (mode === 'orthographic') {
    const frustum = radius * 1.15;
    camera = new THREE.OrthographicCamera(-frustum * aspect, frustum * aspect, frustum, -frustum, 1, radius * 100);
  } else {
    camera = new THREE.PerspectiveCamera(45, aspect, Math.max(radius / 500, 1), radius * 100);
  }
  camera.up.set(0, 0, 1);
  return camera;
}

function updateOrthoFrustum() {
  if (!state.camera?.isOrthographicCamera) return;
  const aspect = cameraAspect();
  const frustum = Math.max(state.modelRadius || 20000, 1) * 1.15;
  state.camera.left = -frustum * aspect;
  state.camera.right = frustum * aspect;
  state.camera.top = frustum;
  state.camera.bottom = -frustum;
  state.camera.updateProjectionMatrix();
}

function setProjection(mode) {
  if (!state.camera || state.projection === mode) return;
  const old = state.camera;
  const target = state.controls?.target.clone() || new THREE.Vector3();
  const position = old.position.clone();
  state.projection = mode;
  state.camera = createCamera(mode);
  state.camera.position.copy(position);
  state.camera.up.copy(old.up);
  state.controls?.dispose();
  state.controls = new OrbitControls(state.camera, byId('cadCanvas'));
  state.controls.enableDamping = true;
  state.controls.dampingFactor = 0.05;
  state.controls.target.copy(target);
  state.controls.update();
  updateOrthoFrustum();
  onWindowResize();
  setText('btnProjection', mode === 'orthographic' ? '投影 正交' : '投影 透视');
  byId('btnProjection')?.classList.toggle('active', mode === 'orthographic');
}

function raycastAt(event) {
  const canvas = byId('cadCanvas');
  if (!canvas || !state.camera) return [];
  const rect = canvas.getBoundingClientRect();
  const pointer = new THREE.Vector2(
    ((event.clientX - rect.left) / rect.width) * 2 - 1,
    -((event.clientY - rect.top) / rect.height) * 2 + 1,
  );
  const raycaster = new THREE.Raycaster();
  raycaster.setFromCamera(pointer, state.camera);
  const meshes = Object.values(state.meshParts).filter((mesh) => mesh.visible);
  return meshes.length ? raycaster.intersectObjects(meshes, false) : [];
}

function updateAxesHelper() {
  if (!state.axesHelper) return;
  state.axesHelper.scale.setScalar(Math.max(state.modelRadius || 20000, 1) * 0.8);
}

function drawAxesGizmo() {
  const canvas = byId('viewerAxes');
  if (!canvas || !state.camera) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;
  const width = canvas.width;
  const height = canvas.height;
  const center = width / 2;
  ctx.clearRect(0, 0, width, height);
  const inverse = state.camera.quaternion.clone().invert();
  const axes = [
    { axis: new THREE.Vector3(1, 0, 0), color: '#f87171', label: 'X' },
    { axis: new THREE.Vector3(0, 1, 0), color: '#4ade80', label: 'Y' },
    { axis: new THREE.Vector3(0, 0, 1), color: '#60a5fa', label: 'Z' },
  ];
  ctx.lineCap = 'round';
  for (const item of axes) {
    const direction = item.axis.clone().applyQuaternion(inverse);
    const dx = direction.x;
    const dy = -direction.y;
    const endX = center + dx * 29;
    const endY = center + dy * 29;
    ctx.strokeStyle = item.color;
    ctx.lineWidth = Math.max(1.4, 2.2 - Math.abs(direction.z));
    ctx.beginPath();
    ctx.moveTo(center, center);
    ctx.lineTo(endX, endY);
    ctx.stroke();
    ctx.fillStyle = item.color;
    ctx.font = 'bold 11px system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(item.label, endX + dx * 9, endY + dy * 9);
  }
  ctx.fillStyle = '#94a3b8';
  ctx.beginPath();
  ctx.arc(center, center, 2, 0, Math.PI * 2);
  ctx.fill();
}

function init3D() {
  const container = byId('cadCanvasContainer');
  const canvas = byId('cadCanvas');
  if (!container || !canvas) return;

  state.scene = new THREE.Scene();
  state.camera = createCamera('perspective');
  try {
    state.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, preserveDrawingBuffer: true });
  } catch (error) {
    setGeometryStatus('WebGL 不可用，3D 视图已禁用', 'warn');
    setFeedback(`WebGL 初始化失败：${error.message}`, 'error');
    return;
  }
  state.renderer.setSize(container.clientWidth, container.clientHeight);
  state.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  state.renderer.localClippingEnabled = true;

  state.controls = new OrbitControls(state.camera, canvas);
  state.controls.enableDamping = true;
  state.controls.dampingFactor = 0.05;

  state.scene.add(new THREE.AmbientLight(0xffffff, 0.75));
  const key = new THREE.DirectionalLight(0xffffff, 0.9);
  key.position.set(30000, -40000, 50000);
  state.scene.add(key);
  const fill = new THREE.DirectionalLight(0x8899aa, 0.35);
  fill.position.set(-30000, 20000, -20000);
  state.scene.add(fill);

  const grid = new THREE.GridHelper(60000, 30, 0x334455, 0x223344);
  grid.rotation.x = Math.PI / 2;
  state.scene.add(grid);
  const axesHelper = new THREE.AxesHelper(1);
  if (axesHelper.material) {
    axesHelper.material.depthTest = false;
    axesHelper.material.transparent = true;
    axesHelper.material.opacity = 0.95;
  }
  axesHelper.renderOrder = 100;
  state.scene.add(axesHelper);
  state.axesHelper = axesHelper;

  canvas.addEventListener('pointerdown', (event) => {
    if (event.button !== 0) return;
    if (state.measureMode) {
      handleMeasureClick(event);
      return;
    }
    const hits = raycastAt(event);
    if (!hits.length) return;
    const hit = hits[0];
    selectPart(hit.object.userData?.name || hit.object.name);
    setText('hudFace', hit.faceIndex != null ? `#${hit.faceIndex}` : '—');
    setText('hudPoint', `P(${Math.round(hit.point.x)}, ${Math.round(hit.point.y)}, ${Math.round(hit.point.z)})`);
  });

  window.addEventListener('resize', onWindowResize);
window.addEventListener('beforeunload', stashSessionAssets);
  setCameraView('iso');
  (function animate() {
    requestAnimationFrame(animate);
    if (document.hidden || !byId('view-3d')?.classList.contains('active')) return;
    state.controls?.update();
    drawAxesGizmo();
    if (state.renderer && state.scene && state.camera) state.renderer.render(state.scene, state.camera);
  })();
}

function onWindowResize() {
  const container = byId('cadCanvasContainer');
  if (!container || !state.renderer || !state.camera) return;
  state.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  state.renderer.setSize(container.clientWidth, container.clientHeight);
  if (state.camera.isPerspectiveCamera) {
    state.camera.aspect = container.clientWidth / Math.max(1, container.clientHeight);
  } else {
    updateOrthoFrustum();
  }
  state.camera.updateProjectionMatrix();
  const resolution = new THREE.Vector2(
    Math.max(1, container.clientWidth) * Math.min(window.devicePixelRatio || 1, 2),
    Math.max(1, container.clientHeight) * Math.min(window.devicePixelRatio || 1, 2),
  );
  for (const edge of state.edgeLines) {
    if (edge.material?.resolution) edge.material.resolution.copy(resolution);
  }
  drawAxesGizmo();
}

function disposeObject(object) {
  object.traverse((child) => {
    if (child.geometry) child.geometry.dispose();
    if (child.material) {
      const materials = Array.isArray(child.material) ? child.material : [child.material];
      for (const material of materials) material.dispose();
    }
  });
}

function clearModel() {
  state.previewRequestId += 1;
  resetMeasure();
  clearFaceHighlight();
  clearEdges();
  if (state.modelRoot) {
    state.scene.remove(state.modelRoot);
    disposeObject(state.modelRoot);
    state.modelRoot = null;
  }
  for (const mesh of Object.values(state.meshParts)) {
    if (mesh.parent === state.scene) state.scene.remove(mesh);
  }
  state.meshParts = {};
  state.clippingPlane = null;
  setHidden('viewerPartsPanel', true);
  setHidden('clipSliderWrap', true);
  setText('btnClipMenu', '剖切 ▾');
  byId('btnClipMenu')?.classList.remove('active');
  state.clipAxis = null;
  document.querySelectorAll('.clip-axis').forEach((button) => button.classList.remove('active'));
}

function materialsOf(mesh) {
  if (!mesh?.material) return [];
  return Array.isArray(mesh.material) ? mesh.material : [mesh.material];
}

function renderResolution() {
  const container = byId('cadCanvasContainer');
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  return new THREE.Vector2(
    Math.max(1, container?.clientWidth || 1) * dpr,
    Math.max(1, container?.clientHeight || 1) * dpr,
  );
}

function clearEdges() {
  for (const line of state.edgeLines) {
    line.parent?.remove(line);
    line.geometry?.dispose();
    line.material?.dispose();
  }
  state.edgeLines = [];
}

function buildEdges() {
  clearEdges();
  const resolution = renderResolution();
  for (const mesh of Object.values(state.meshParts)) {
    if (!mesh.geometry) continue;
    let edgeGeometry;
    try {
      const source = new THREE.EdgesGeometry(mesh.geometry, 28);
      const positions = source.attributes.position;
      if (!positions?.count) {
        source.dispose();
        continue;
      }
      edgeGeometry = new LineSegmentsGeometry();
      edgeGeometry.setPositions(Array.from(positions.array));
      source.dispose();
    } catch {
      continue;
    }
    const material = new LineMaterial({
      color: 0x0b0f14,
      linewidth: state.lineWidth,
      resolution,
    });
    if (state.clippingPlane) {
      material.clippingPlanes = [state.clippingPlane];
      material.clipping = true;
    }
    const line = new LineSegments2(edgeGeometry, material);
    line.userData.isEdge = true;
    line.raycast = () => {};
    mesh.add(line);
    state.edgeLines.push(line);
  }
  applyRenderMode(state.renderMode);
}

function applyRenderMode(mode) {
  state.renderMode = mode;
  const edgeColor = mode === 'wireframe' ? 0x9cc7e8 : mode === 'xray' ? 0xa8bfd4 : 0x0b0f14;
  for (const mesh of Object.values(state.meshParts)) {
    for (const material of materialsOf(mesh)) {
      material.visible = mode !== 'wireframe';
      material.wireframe = false;
      material.transparent = mode === 'xray';
      material.opacity = mode === 'xray' ? 0.18 : 1;
      material.depthWrite = mode !== 'xray';
      material.needsUpdate = true;
    }
  }
  for (const line of state.edgeLines) {
    line.material.color.setHex(edgeColor);
    line.material.linewidth = state.lineWidth;
    line.material.needsUpdate = true;
    line.visible = mode === 'wireframe' || state.edgesVisible;
  }
  const labels = { shaded: '着色', wireframe: '线框', xray: 'X-Ray' };
  setText('btnRenderMode', `显示 ${labels[mode] || mode} ▾`);
  byId('btnRenderMode')?.classList.toggle('active', mode !== 'shaded');
  document.querySelectorAll('[data-render-mode]').forEach((button) => {
    button.classList.toggle('active', button.dataset.renderMode === mode);
  });
  if (byId('btnEdges')) {
    byId('btnEdges').disabled = mode === 'wireframe';
    byId('btnEdges').textContent = state.edgesVisible ? '黑边 ✓' : '黑边';
    byId('btnEdges').classList.toggle('active', state.edgesVisible || mode === 'wireframe');
  }
}

function toggleEdges() {
  state.edgesVisible = !state.edgesVisible;
  for (const line of state.edgeLines) {
    line.visible = state.renderMode === 'wireframe' || state.edgesVisible;
  }
  if (byId('btnEdges')) byId('btnEdges').textContent = state.edgesVisible ? '黑边 ✓' : '黑边';
  byId('btnEdges')?.classList.toggle('active', state.edgesVisible || state.renderMode === 'wireframe');
}

function cycleRenderMode() {
  const order = ['shaded', 'wireframe', 'xray'];
  const index = order.indexOf(state.renderMode);
  applyRenderMode(order[(index + 1) % order.length]);
}

function cycleLineWidth() {
  const options = [1.0, 1.5, 2.0];
  const index = options.indexOf(state.lineWidth);
  state.lineWidth = options[(index + 1) % options.length];
  setText('btnLineWidth', `线宽 ${state.lineWidth.toFixed(1)}`);
  for (const line of state.edgeLines) {
    line.material.linewidth = state.lineWidth;
    line.material.needsUpdate = true;
  }
}

function modelBounds() {
  if (!state.modelRoot) return null;
  const box = new THREE.Box3().setFromObject(state.modelRoot);
  return box.isEmpty() ? null : box;
}

function applyClipping() {
  if (!state.renderer) return;
  if (!state.clipAxis) {
    state.clippingPlane = null;
    for (const mesh of Object.values(state.meshParts)) {
      for (const material of materialsOf(mesh)) {
        material.clippingPlanes = [];
        material.needsUpdate = true;
      }
    }
    for (const line of state.edgeLines) {
      line.material.clippingPlanes = [];
      line.material.needsUpdate = true;
    }
    setHidden('clipSliderWrap', true);
    setText('btnClipMenu', '剖切 ▾');
    byId('btnClipMenu')?.classList.remove('active');
    document.querySelectorAll('.clip-axis').forEach((button) => button.classList.remove('active'));
    return;
  }
  const box = modelBounds();
  if (!box) return;
  const axisIndex = state.clipAxis === 'x' ? 0 : state.clipAxis === 'y' ? 1 : 2;
  const min = box.min.getComponent(axisIndex);
  const max = box.max.getComponent(axisIndex);
  const value = min + (max - min) * (state.clipRatio / 100);
  const normal = new THREE.Vector3();
  normal.setComponent(axisIndex, -1);
  state.clippingPlane = new THREE.Plane(normal, value);
  for (const mesh of Object.values(state.meshParts)) {
    for (const material of materialsOf(mesh)) {
      material.clippingPlanes = [state.clippingPlane];
      material.clipShadows = true;
      material.needsUpdate = true;
    }
  }
  for (const line of state.edgeLines) {
    line.material.clippingPlanes = [state.clippingPlane];
    line.material.clipping = true;
    line.material.needsUpdate = true;
  }
  setHidden('clipSliderWrap', false);
  setText('btnClipMenu', `剖切 ${state.clipAxis.toUpperCase()} ▾`);
  byId('btnClipMenu')?.classList.add('active');
  setText('clipValue', `${state.clipRatio}%`);
  byId('clipRange').value = String(state.clipRatio);
  document.querySelectorAll('.clip-axis').forEach((button) => {
    button.classList.toggle('active', button.dataset.clipAxis === state.clipAxis);
  });
}

function setClipAxis(axis) {
  state.clipAxis = axis;
  applyClipping();
  if (!axis) setHidden('clipSliderWrap', true);
}

function setClipRatio(value) {
  state.clipRatio = Math.min(98, Math.max(2, Number(value) || 76));
  setText('clipValue', `${state.clipRatio}%`);
  applyClipping();
}

function clearMeasure() {
  state.measurePoints = [];
  state.measureResult = null;
  for (const object of state.measureObjects) {
    object.parent?.remove(object);
    object.geometry?.dispose();
    object.material?.dispose();
  }
  state.measureObjects = [];
  setHidden('measureResult', true);
  setHidden('measureHint', Boolean(!state.measureMode));
  const holder = byId('measureResult');
  if (holder) holder.hidden = true;
}

function snapPoint(hit) {
  const mesh = hit.object;
  const position = mesh?.geometry?.attributes?.position;
  const face = hit.face;
  if (!position || !face) return hit.point.clone();
  const candidates = [face.a, face.b, face.c]
    .filter((index) => Number.isInteger(index))
    .map((index) => new THREE.Vector3().fromBufferAttribute(position, index).applyMatrix4(mesh.matrixWorld));
  if (!candidates.length) return hit.point.clone();
  candidates.sort((a, b) => a.distanceToSquared(hit.point) - b.distanceToSquared(hit.point));
  return candidates[0];
}

function addMeasureMarker(point, color = 0x38bdf8) {
  const radius = Math.max(state.modelRadius * 0.008, 20);
  const marker = new THREE.Mesh(
    new THREE.SphereGeometry(radius, 12, 12),
    new THREE.MeshBasicMaterial({ color, depthTest: false }),
  );
  marker.position.copy(point);
  marker.renderOrder = 10;
  state.scene.add(marker);
  state.measureObjects.push(marker);
}

function handleMeasureClick(event) {
  const hits = raycastAt(event);
  if (!hits.length) return;
  const point = snapPoint(hits[0]);
  if (!state.measurePoints.length) {
    state.measurePoints = [point];
    addMeasureMarker(point);
    setHidden('measureHint', false);
    return;
  }
  const first = state.measurePoints[0];
  const second = point;
  state.measurePoints.push(second);
  addMeasureMarker(second, 0xf59e0b);
  const geometry = new THREE.BufferGeometry().setFromPoints([first, second]);
  const line = new THREE.Line(geometry, new THREE.LineBasicMaterial({ color: 0x38bdf8, depthTest: false }));
  line.renderOrder = 9;
  state.scene.add(line);
  state.measureObjects.push(line);
  const distance = first.distanceTo(second);
  state.measureResult = {
    distance,
    horizontal: Math.hypot(second.x - first.x, second.y - first.y),
    deltaZ: Math.abs(second.z - first.z),
  };
  setText('measureDistance', Math.round(distance).toLocaleString());
  setText('measureHorizontal', state.measureResult.horizontal.toFixed(1));
  setText('measureDeltaZ', state.measureResult.deltaZ.toFixed(1));
  setHidden('measureResult', false);
  setHidden('measureHint', true);
}

function toggleMeasure() {
  state.measureMode = !state.measureMode;
  byId('view-3d')?.classList.toggle('measuring', state.measureMode);
  byId('btnMeasure')?.classList.toggle('active', state.measureMode);
  if (state.controls) state.controls.enabled = !state.measureMode;
  if (state.measureMode) {
    clearMeasure();
    setHidden('measureHint', false);
  } else {
    setHidden('measureHint', true);
  }
}

function resetMeasure() {
  clearMeasure();
  state.measureMode = false;
  if (state.controls) state.controls.enabled = true;
  byId('view-3d')?.classList.remove('measuring');
  byId('btnMeasure')?.classList.remove('active');
  setHidden('measureHint', true);
}

function fitCamera() {
  if (!state.camera || !state.controls) return;
  const box = modelBounds();
  if (!box) return;
  const sphere = box.getBoundingSphere(new THREE.Sphere());
  state.modelRadius = Math.max(sphere.radius, 1);
  state.controls.target.copy(sphere.center);
  const direction = state.camera.position.clone().sub(sphere.center);
  if (direction.lengthSq() < 1e-6) direction.set(1, 1, 0.86);
  direction.normalize();
  const distance = state.modelRadius * 2.6;
  state.camera.position.copy(sphere.center).addScaledVector(direction, distance);
  state.camera.near = Math.max(state.modelRadius / 500, 1);
  state.camera.far = state.modelRadius * 100;
  state.camera.updateProjectionMatrix();
  updateOrthoFrustum();
  updateAxesHelper();
  drawAxesGizmo();
  state.controls.update();
  if (state.clippingPlane) applyClipping();
}

function setGeometryStatus(text, kind = 'pending') {
  const node = byId('geometryStatus');
  if (!node) return;
  node.textContent = text;
  node.dataset.kind = kind;
}

function partMetadata(name) {
  const shape = shapeForName(name);
  if (!shape) return { name, materialName: '未标注材质', vol: '—', code: '—' };
  return {
    name: shape.key || name,
    materialName: shape.material || shape.geomType || '—',
    vol: shape.volumeM3 ? `${shape.volumeM3} m³` : '—',
    code: shape.code || '—',
  };
}

function applyPreviewGltf(gltf, requestId, label) {
  if (requestId !== state.previewRequestId) return;
  clearModel();
  const root = gltf.scene;
  root.rotation.x = 0; // RCIF GLB 与 STEP 均为 Z-up，不再转到 Y-up
  root.updateMatrixWorld(true);
  state.modelRoot = root;
  state.scene.add(root);
  let count = 0;
  root.traverse((child) => {
    if (!child.isMesh) return;
    const key = child.name || `Part${count + 1}`;
    const extras = child.userData || {};
    child.userData = { ...extras, ...partMetadata(key) };
    child.material = Array.isArray(child.material)
      ? child.material.map((material) => material?.clone?.() || material)
      : (child.material?.clone?.() || child.material);
    child.userData.materialState = materialsOf(child).map((material) => ({
      opacity: material.opacity,
      transparent: material.transparent,
      depthWrite: material.depthWrite,
    }));
    if (child.material && 'metalness' in child.material) {
      child.material.metalness = 0.05;
      child.material.roughness = 0.78;
    }
    state.meshParts[key] = child;
    count += 1;
  });
  setHidden('viewportEmpty', true);
  setGeometryStatus(`${label} · ${count} 个部位`, 'ok');
  buildEdges();
  renderViewerParts();
  applyRenderMode(state.renderMode);
  setCameraView('iso');
  updateAxesHelper();
  updatePresetPreviewNotice();
  if (state.clipAxis) applyClipping();
  const first = state.shapes[0];
  if (first) selectPart(first.key);
}

async function loadPreviewMesh() {
  if (!state.packagePath) {
    clearModel();
    setHidden('viewportEmpty', false);
    setGeometryStatus('未加载构件包', 'pending');
    return;
  }
  const preview = state.metadata?.preview;
  if (!preview?.available) {
    clearModel();
    setHidden('viewportEmpty', false);
    byId('viewportEmpty').querySelector('span').textContent = '该构件包不包含 display/preview.glb';
    setGeometryStatus('无预览几何', 'warn');
    return;
  }
  setGeometryStatus('加载 preview.glb…', 'pending');
  const loader = new GLTFLoader();
  const requestId = state.previewRequestId + 1;
  state.previewRequestId = requestId;
  const url = `${API}/package/preview.glb?path=${encodeURIComponent(state.packagePath)}&v=${Date.now()}`;
  state.previewPreset = null;
  loader.load(
    url,
    (gltf) => applyPreviewGltf(gltf, requestId, '真实 GLB'),
    undefined,
    (error) => {
      if (requestId !== state.previewRequestId) return;
      clearModel();
      setHidden('viewportEmpty', false);
      byId('viewportEmpty').querySelector('span').textContent = `preview.glb 加载失败：${error?.message || '未知错误'}`;
      setGeometryStatus('预览加载失败', 'warn');
    },
  );
}

async function requestPresetPreview(preset) {
  const body = {
    sessionId: state.sessionId,
    packagePath: state.packagePath,
    modelNumber: preset.modelNumber,
    params: preset.params || {},
  };
  const send = () => fetch(`${API}/preview`, {
    method: 'POST',
    headers: {
      'content-type': 'application/json',
      ...(state.capToken ? { 'X-RCIF-Cap-Token': state.capToken } : {}),
    },
    body: JSON.stringify(body),
  });
  let response = await send();
  if (response.status === 403) {
    const text = await response.clone().text();
    if (text.includes('CAP_TOKEN')) {
      await connectSession(state.sessionId);
      response = await send();
    }
  }
  if (!response.ok) {
    const text = await response.text();
    let message = text;
    try {
      message = JSON.parse(text).error || text;
    } catch {
      /* keep text */
    }
    throw new Error(message || `HTTP ${response.status}`);
  }
  return response.arrayBuffer();
}

async function loadPresetPreview(modelNumber) {
  const preset = state.presets.find((item) => item.modelNumber === modelNumber);
  if (!preset || !state.packagePath) return false;
  const first = state.presets[0];
  if (first && preset.modelNumber === first.modelNumber) {
    await loadPreviewMesh();
    return true;
  }
  const cacheKey = `${state.packagePath}|${modelNumber}`;
  const requestId = state.previewRequestId + 1;
  state.previewRequestId = requestId;
  setGeometryStatus(`正在按型号 ${modelNumber} 重构预览…`, 'pending');
  try {
    let buffer = state.presetPreviewCache.get(cacheKey);
    if (!buffer) {
      buffer = await requestPresetPreview(preset);
      state.presetPreviewCache.set(cacheKey, buffer);
    }
    if (requestId !== state.previewRequestId) return true;
    const loader = new GLTFLoader();
    loader.parse(
      buffer,
      '',
      (gltf) => {
        if (requestId !== state.previewRequestId) return;
        state.previewPreset = modelNumber;
        applyPreviewGltf(gltf, requestId, `型号 ${modelNumber}`);
      },
      (error) => {
        if (requestId !== state.previewRequestId) return;
        setGeometryStatus(`型号预览解析失败：${error?.message || '未知错误'}`, 'warn');
      },
    );
    return true;
  } catch (error) {
    if (requestId === state.previewRequestId) {
      setGeometryStatus(`型号预览重建失败：${error.message}`, 'warn');
    }
    return false;
  }
}

function selectPreset(modelNumber, options = {}) {
  if (!modelNumber) return;
  state.selectedPreset = modelNumber;
  const presetSelect = byId('presetSelect');
  if (presetSelect) presetSelect.value = modelNumber;
  const paramSelect = byId('paramPresetSelect');
  if (paramSelect) paramSelect.value = modelNumber;
  renderPresetJson();
  updatePresetPreviewNotice();
  if (options.preview) loadPresetPreview(modelNumber);
}

function setCameraView(viewType) {
  if (!state.camera || !state.controls) return;
  const target = new THREE.Vector3(0, 0, 0);
  let distance = 20000;
  if (state.modelRoot) {
    const box = new THREE.Box3().setFromObject(state.modelRoot);
    if (!box.isEmpty()) {
      const sphere = box.getBoundingSphere(new THREE.Sphere());
      target.copy(sphere.center);
      state.modelRadius = Math.max(sphere.radius, 1);
      distance = Math.max(sphere.radius * 2.6, 1000);
    }
  }
  state.controls.target.copy(target);
  if (viewType === 'front') state.camera.position.set(target.x, target.y - distance, target.z);
  else if (viewType === 'side') state.camera.position.set(target.x + distance, target.y, target.z);
  else if (viewType === 'plan') state.camera.position.set(target.x, target.y, target.z + distance);
  else state.camera.position.set(target.x - distance * 0.7, target.y - distance * 0.8, target.z + distance * 0.7);
  state.camera.up.set(0, viewType === 'plan' ? 1 : 0, viewType === 'plan' ? 0 : 1);
  state.camera.near = Math.max(1, distance / 500);
  state.camera.far = distance * 80;
  state.camera.updateProjectionMatrix();
  updateOrthoFrustum();
  updateAxesHelper();
  state.controls.update();
  drawAxesGizmo();
  for (const [id, value] of [['btnViewIso', 'iso'], ['btnViewFront', 'front'], ['btnViewSide', 'side'], ['btnViewPlan', 'plan']]) {
    byId(id)?.classList.toggle('active', viewType === value);
  }
  const labels = { iso: '轴测', front: '正视', side: '侧视', plan: '俯视' };
  setText('btnViewMenu', `视图 ${labels[viewType] || '轴测'} ▾`);
  if (state.clippingPlane) applyClipping();
}

function nameMatchesBase(name, base) {
  return name === base || name.startsWith(`${base}/`) || name.startsWith(`${base}_`);
}

function shapeForName(name) {
  return state.shapes.find((item) => {
    const bases = [item.key, item.node].filter(Boolean);
    return bases.some((base) => nameMatchesBase(name, base));
  });
}

function meshesForShape(shape) {
  if (!shape) return [];
  const bases = [shape.key, shape.node].filter(Boolean);
  return Object.entries(state.meshParts)
    .filter(([key, mesh]) => (
      mesh.userData?.shapeKey === shape.key
      || mesh.userData?.shapeKey === shape.node
      || bases.some((base) => nameMatchesBase(key, base))
    ))
    .map(([, mesh]) => mesh);
}

function meshForShape(shape) {
  return meshesForShape(shape)[0] || null;
}

function clearFaceHighlight() {
  for (const group of state.faceHighlights) {
    if (group.parent) group.parent.remove(group);
    group.traverse((child) => {
      child.geometry?.dispose();
      child.material?.dispose();
    });
  }
  state.faceHighlights = [];
}

function createFaceHighlight(shape) {
  const parent = shapeForName(shape?.parent || '');
  const parents = meshesForShape(parent);
  if (!parents.length) return false;
  const group = new THREE.Group();
  group.name = `face-highlight:${shape.key}`;
  for (const mesh of parents) {
    if (!mesh.visible) continue;
    const box = new THREE.Box3().setFromObject(mesh);
    if (box.isEmpty()) continue;
    const size = box.getSize(new THREE.Vector3());
    const center = box.getCenter(new THREE.Vector3());
    const geometry = new THREE.PlaneGeometry(Math.max(size.x, 1), Math.max(size.y, 1));
    const material = new THREE.MeshBasicMaterial({
      color: 0x38bdf8,
      transparent: true,
      opacity: 0.42,
      side: THREE.DoubleSide,
      depthWrite: false,
      depthTest: false,
    });
    const face = new THREE.Mesh(geometry, material);
    face.position.set(center.x, center.y, box.max.z + 1.0);
    face.renderOrder = 20;
    face.userData.isFaceHighlight = true;
    group.add(face);
    const border = new THREE.LineSegments(
      new THREE.EdgesGeometry(geometry),
      new THREE.LineBasicMaterial({ color: 0x7dd3fc, transparent: true, opacity: 0.95, depthTest: false }),
    );
    border.position.copy(face.position);
    border.renderOrder = 21;
    group.add(border);
  }
  if (!group.children.length) return false;
  state.scene.add(group);
  state.faceHighlights.push(group);
  return true;
}

function selectPart(name) {
  if (!name) return;
  const shape = shapeForName(name);
  const isFace = Boolean(shape && (shape.geomType === 'face' || shape.selector));
  const meshes = isFace ? [] : meshesForShape(shape);
  const info = partsInfo(shape, name);
  const canonical = shape?.key || info.name || name;
  const selectedKeys = new Set(Object.entries(state.meshParts)
    .filter(([, mesh]) => meshes.includes(mesh))
    .map(([key]) => key));
  clearFaceHighlight();
  if (isFace) createFaceHighlight(shape);
  state.selectedPart = canonical;
  setText('hudPartName', info.name || canonical);
  setText('hudMaterial', info.materialName);
  setText('hudVolume', info.vol);
  setText('hudCode', info.code);
  for (const [key, item] of Object.entries(state.meshParts)) {
    for (const material of materialsOf(item)) {
      if (material && 'emissive' in material) {
        material.emissive = new THREE.Color(selectedKeys.has(key) ? 0x388bfd : 0x000000);
        material.emissiveIntensity = selectedKeys.has(key) ? 0.4 : 0;
        material.needsUpdate = true;
      }
    }
  }
  const rows = document.querySelectorAll('#partsTableBody tr');
  rows.forEach((row) => row.classList.toggle('selected', row.dataset.part === canonical));
  renderViewerParts();
}

function partsInfo(shape, fallbackName) {
  if (!shape) return partMetadata(fallbackName);
  return {
    name: shape.key || fallbackName,
    materialName: shape.material || shape.geomType || '—',
    vol: shape.volumeM3 ? `${shape.volumeM3} m³` : '—',
    code: shape.code || '—',
  };
}

function renderViewerParts() {
  const panel = byId('viewerPartsPanel');
  const list = byId('viewerPartsList');
  if (!panel || !list) return;
  const shapes = Array.isArray(state.shapes) ? state.shapes : [];
  setText('viewerPartsCount', String(shapes.length));
  panel.hidden = !state.viewerPartsOpen || !shapes.length;
  byId('btnViewerParts')?.classList.toggle('active', state.viewerPartsOpen);
  list.textContent = '';
  for (const shape of shapes) {
    const key = shape.key || shape.node;
    if (!key) continue;
    const isFace = shape.geomType === 'face' || Boolean(shape.selector);
    const parent = isFace ? shapeForName(shape.parent || '') : null;
    const meshes = isFace ? meshesForShape(parent) : meshesForShape(shape);
    const row = document.createElement('label');
    row.className = `viewer-part-row${state.selectedPart === key ? ' active' : ''}`;
    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    checkbox.checked = !isFace && meshes.length > 0 && meshes.every((mesh) => mesh.visible);
    checkbox.disabled = isFace || !meshes.length;
    checkbox.addEventListener('change', (event) => {
      event.stopPropagation();
      meshes.forEach((mesh) => { mesh.visible = checkbox.checked; });
      selectPart(key);
    });
    const label = document.createElement('span');
    label.textContent = shape.label || shape.key || key;
    label.title = isFace
      ? `语义面，引用 ${parent?.label || shape.parent || '父级'}`
      : (meshes.length ? label.textContent : `${label.textContent}（GLB 中未建模）`);
    row.append(checkbox, label);
    if (isFace) {
      const kind = document.createElement('span');
      kind.className = 'part-kind';
      kind.textContent = '面';
      kind.title = '语义面，选中时在运行时高亮';
      row.appendChild(kind);
    }
    row.addEventListener('click', (event) => {
      if (event.target === checkbox) return;
      if (meshes.length) selectPart(key);
    });
    list.appendChild(row);
  }
}

function toggleViewerParts() {
  state.viewerPartsOpen = !state.viewerPartsOpen;
  renderViewerParts();
}

function toggleWireframe() {
  cycleRenderMode();
}

// ------------------------------------------------------------------ package

function resetViews() {
  setHidden('metaEmpty', false);
  setHidden('metaContent', true);
  setHidden('paramsEmpty', false);
  setHidden('paramsContent', true);
  setHidden('partsEmpty', false);
  setHidden('partsContent', true);
  setHidden('modelsEmpty', false);
  setHidden('modelsContent', true);
  setHidden('ingestEmpty', false);
  setHidden('ingestContent', true);
  setHidden('sourceEmpty', false);
  setHidden('codeEditorWrap', true);
  setHidden('annotationSidebar', true);
  setHidden('annotationDialog', true);
  setHidden('annotationHistoryPanel', true);
  setHidden('btnEditMetadata', true);
  setHidden('btnEditParameters', true);
  setText('packageTitle', '未绑定构件');
  badge('packageRevision', '-');
  setText('chatContext', state.sessionId
    ? `${state.workspace?.title || '未命名工作区'} · 会话 ${shortSessionId(state.sessionId)} · 尚未绑定构件`
    : '尚未选择工作区/会话');
  setText('sessionPackageLabel', '-');
}

function clearViewerState() {
  setGeometryStatus('待加载', 'pending');
  setHidden('viewportEmpty', false);
  setText('hudPartName', '点击 3D 实体');
  setText('hudMaterial', '-');
  setText('hudVolume', '-');
  setText('hudCode', '-');
  setText('hudFace', '-');
  setText('hudPoint', '-');
  for (const id of ['metaFamily', 'metaName', 'metaRevision', 'metaSourceMode', 'metaEngine', 'metaVolume', 'metaBounds', 'metaCounts', 'metaPackageSha', 'ingestFamily', 'ingestPackage']) {
    setText(id, '-');
  }
}

function renderHeader() {
  const metadata = state.metadata || {};
  setText('packageTitle', metadata.name || metadata.familyId || '未绑定构件');
  badge('packageRevision', metadata.revision || '-');
  setText('chatContext', state.packagePath
    ? `${state.workspace?.title || ''} · ${shortSessionId(state.sessionId)} · ${metadata.familyId || ''} ${metadata.revision || ''}`
    : `${state.workspace?.title || '未命名工作区'} · 会话 ${shortSessionId(state.sessionId)} · 尚未绑定构件`);
  document.title = state.packagePath
    ? `RCIF · ${metadata.familyId || ''} ${metadata.revision || ''}`
    : '铁路构件智能助手 · 工作台';
}

async function loadPackage(path, options = {}) {
  if (!path || state.loading) return;
  const activeView = options.view || document.querySelector('.wb-tab.active')?.dataset.view || 'view-metadata';
  state.loading = true;
  setFeedback(`正在加载构件包 ${path}…`, 'pending');
  try {
    const meta = await api(`/package?path=${encodeURIComponent(path)}`);
    state.packagePath = meta.packagePath;
    state.meta = meta;
    state.component = meta.component;
    state.metadata = meta.metadata;
    state.presets = Array.isArray(meta.component?.parameters?.presets) ? meta.component.parameters.presets : [];
    state.schema = Array.isArray(meta.component?.parameters?.schema) ? meta.component.parameters.schema : [];
    state.shapes = Array.isArray(meta.metadata?.shapes) ? meta.metadata.shapes : [];
    state.selectedPreset = state.presets[0]?.modelNumber || null;
    state.previewPreset = null;
    state.presetPreviewCache.clear();
    state.selectedLines = [1, 3];
    state.activeAnnotationId = null;
    renderAnnotationSidebar();
    renderScreenshotList();
    updateChatCounts();

    renderHeader();
    renderMetadata();
    renderParams();
    renderParts();
    renderModels();
    renderIngest();
    if (activeView !== 'view-session') switchView(activeView === 'view-3d' ? 'view-3d' : 'view-metadata');
    else switchView('view-metadata');
    await loadSource();
    await loadPreviewMesh();
    if (activeView === 'view-3d' || options.view === 'view-3d') switchView('view-3d');
    setFeedback(`构件包已加载：${meta.metadata?.familyId || ''} ${meta.metadata?.revision || ''}`, 'ok');
  } catch (error) {
    state.lastError = error.message;
    setFeedback(`构件包加载失败：${error.message}`, 'error');
  } finally {
    state.loading = false;
  }
}

async function loadSource() {
  if (!state.packagePath) return;
  try {
    state.source = await api(`/package/source?path=${encodeURIComponent(state.packagePath)}`);
  } catch (error) {
    state.source = { available: false, path: null, text: '', error: error.message };
  }
  renderSource();
  await loadAnnotationHistory();
}

function renderMetadata() {
  const metadata = state.metadata;
  if (!metadata) return;
  setHidden('metaEmpty', true);
  setHidden('metaContent', false);
  setHidden('btnEditMetadata', false);

  setText('metaFamily', metadata.familyId);
  setText('metaName', metadata.name);
  setText('metaRevision', metadata.revision);
  setText('metaSourceMode', metadata.sourceMode);
  setText('metaEngine', metadata.engine || '（无 recipe，原生几何）');
  setText('metaExactBrep', metadata.exactBrep ? '是（精确 B-Rep）' : '否（mesh-only）');
  setText('metaVolume', metadata.volumeM3 === null ? '—' : `${formatNumber(metadata.volumeM3)} m³`);
  const b = metadata.bounds;
  setText('metaBounds', b ? `X ${b.x?.[0]}…${b.x?.[1]} / Y ${b.y?.[0]}…${b.y?.[1]} / Z ${b.z?.[0]}…${b.z?.[1]} mm` : '—');
  setText('metaCounts', `${metadata.parameterCount} / ${metadata.presetCount} / ${metadata.shapeCount}`);
  setText('metaSemantic', metadata.semanticCode);
  setText('metaDesignSpeed', metadata.designSpeed || '—');
  setText('metaApplicableSpan', Array.isArray(metadata.applicableSpan) && metadata.applicableSpan.length ? metadata.applicableSpan.join(' / ') : '—');
  setText('metaDescription', metadata.description || '—');
  setText('metaPackageSha', metadata.packageSha256);
  setText('metaFileCount', metadata.fileCount);
  const checksums = metadata.checksums || {};
  if (!checksums.present) setText('metaChecksum', '未提供 checksums.json');
  else if (checksums.verified) setText('metaChecksum', `✅ ${checksums.checked}/${checksums.fileCount} 文件哈希一致`);
  else setText('metaChecksum', `❌ ${checksums.mismatches?.join('；') || '校验失败'}`);
  const provenance = state.meta?.provenance;
  setText('metaProvenance', provenance ? `${provenance.signatureStatus || '未知'} · ${provenance.approvalAlgorithm || '无签名'}` : '未找到 .provenance.json');
  const extraction = metadata.qa?.extractionMode || metadata.extractionMode || '—';
  if (extraction === 'generic_heuristic_draft') {
    const confidence = metadata.qa?.confidence;
    const missing = metadata.qa?.missingParameters || [];
    setText(
      'metaExtraction',
      `通用体量草稿 · 置信度 ${confidence === undefined ? '—' : confidence}${missing.length ? ` · 缺失 ${missing.join('/')}` : ''}`,
    );
  } else if (extraction === 'rfa_partatom_heuristic' && metadata.qa?.grep?.brepDecoded) {
    const g = metadata.qa.grep;
    setText(
      'metaExtraction',
      `RFA GRep B-Rep 完整解码 + 参数化重构（${g.gElementCount || 0} GElement / ${g.brepSolidCount || 0} 实体 / ${g.faceCount || 0} 面 / ${g.edgeCount || 0} 边）`,
    );
  } else {
    setText('metaExtraction', extraction);
  }
  setText('metaLoadedAt', state.meta?.updatedAt || '—');

  const filesBody = byId('metaFiles');
  filesBody.textContent = '';
  for (const file of state.meta?.files || []) {
    const tr = document.createElement('tr');
    const pathCell = document.createElement('td');
    pathCell.appendChild(mono(file.path));
    const sizeCell = document.createElement('td');
    sizeCell.textContent = formatBytes(file.size);
    const compressedCell = document.createElement('td');
    compressedCell.textContent = formatBytes(file.compressedSize);
    tr.append(pathCell, sizeCell, compressedCell);
    filesBody.appendChild(tr);
  }

  const changelog = byId('metaChangelog');
  changelog.textContent = '';
  for (const entry of metadata.changelog || []) {
    const li = document.createElement('li');
    li.textContent = `${entry.version || ''} · ${entry.date || ''} · ${(entry.changes || []).join('；')}`;
    changelog.appendChild(li);
  }
  if (!(metadata.changelog || []).length) {
    const li = document.createElement('li');
    li.textContent = '包内未提供 changelog';
    changelog.appendChild(li);
  }
  byId('metaQa').textContent = JSON.stringify(metadata.qa || {}, null, 2);
}

function renderParams() {
  if (!state.schema.length) {
    setHidden('paramsEmpty', false);
    setHidden('paramsContent', true);
    setHidden('btnEditParameters', !state.packagePath);
    return;
  }
  setHidden('paramsEmpty', true);
  setHidden('paramsContent', false);
  setHidden('btnEditParameters', false);

  const body = byId('paramSchemaBody');
  body.textContent = '';
  for (const field of state.schema) {
    const tr = document.createElement('tr');
    const cells = [
      field.name,
      field.label,
      field.type,
      field.default,
      field.unit || '—',
      field.min !== undefined || field.max !== undefined ? `${field.min ?? '−∞'} … ${field.max ?? '+∞'}` : '—',
      field.access || '—',
    ];
    for (const value of cells) {
      const td = document.createElement('td');
      td.textContent = String(value ?? '—');
      tr.appendChild(td);
    }
    body.appendChild(tr);
  }

  const select = byId('paramPresetSelect');
  select.textContent = '';
  for (const preset of state.presets) {
    const option = document.createElement('option');
    option.value = preset.modelNumber;
    option.textContent = `${preset.modelNumber}${preset.label ? ` · ${preset.label}` : ''}`;
    select.appendChild(option);
  }
  if (state.selectedPreset) select.value = state.selectedPreset;
  renderPresetJson();
  renderGenericRebuild();
}

function isGenericDraft() {
  const mode = state.metadata?.qa?.extractionMode;
  return mode === 'generic_heuristic_draft' || mode === 'rfa_partatom_heuristic';
}

function renderGenericRebuild() {
  const panel = byId('genericRebuildPanel');
  if (!panel) return;
  if (!isGenericDraft()) {
    panel.hidden = true;
    return;
  }
  panel.hidden = false;
  const fields = byId('genericParamFields');
  fields.textContent = '';
  const preset = state.presets.find((item) => item.modelNumber === byId('paramPresetSelect')?.value)
    || state.presets.find((item) => item.modelNumber === state.selectedPreset)
    || state.presets[0];
  const values = preset?.params || {};
  for (const field of state.schema) {
    if (field.access === 'out') continue;
    const label = document.createElement('label');
    label.textContent = `${field.label || field.name}${field.unit ? `（${field.unit}）` : ''}`;
    const input = document.createElement('input');
    input.dataset.param = field.name;
    input.value = values[field.name] ?? field.default ?? '';
    input.type = field.type === 'real' ? 'number' : 'text';
    if (field.step) input.step = String(field.step);
    label.appendChild(input);
    fields.appendChild(label);
  }
  setStatus('genericRebuildStatus', '通用草稿可直接修改参数后重建');
}

function collectGenericParams() {
  const params = {};
  for (const input of document.querySelectorAll('#genericParamFields [data-param]')) {
    const raw = input.value.trim();
    params[input.dataset.param] = raw === '' ? '' : (input.type === 'number' ? Number(raw) : raw);
  }
  return params;
}

async function openPackageEdit(mode) {
  if (!state.packagePath) {
    setFeedback('当前会话尚未加载构件包', 'warn');
    return;
  }
  try {
    const data = await api(`/package/edit?path=${encodeURIComponent(state.packagePath)}`);
    state.packageEditMode = mode;
    state.packageEditData = data;
    const identity = data.identity || {};
    byId('packageEditTitle').textContent = mode === 'parameters' ? '编辑参数与型号' : '编辑构件元数据';
    setHidden('packageIdentityFields', mode !== 'identity');
    setHidden('packageParametersFields', mode !== 'parameters');
    byId('packageEditName').value = identity.name || '';
    byId('packageEditFamily').value = identity.familyId || '';
    byId('packageEditCategory').value = identity.category || '';
    byId('packageEditSemantic').value = identity.semanticCode || '';
    byId('packageEditDrawing').value = identity.drawingCode || '';
    byId('packageEditSpeed').value = identity.designSpeed || '';
    byId('packageEditSpan').value = (identity.applicableSpan || []).join('\n');
    byId('packageEditDescription').value = data.description || '';
    byId('packageParametersJson').value = JSON.stringify({
      schema: data.parameters?.schema || [],
      presets: data.parameters?.presets || [],
    }, null, 2);
    byId('packageEditNote').value = '';
    setStatus('packageEditStatus', '');
    setHidden('packageEditDialog', false);
  } catch (error) {
    setFeedback(`构件包编辑视图读取失败：${error.message}`, 'error');
  }
}

function closePackageEdit() {
  setHidden('packageEditDialog', true);
  state.packageEditMode = null;
  state.packageEditData = null;
}

async function savePackageEdit() {
  if (!state.packageEditMode || !state.packagePath) return;
  const note = (byId('packageEditNote')?.value || '').trim();
  let operations;
  if (state.packageEditMode === 'identity') {
    operations = [
      {
        op: 'identity.update',
        value: {
          name: byId('packageEditName').value.trim(),
          familyId: byId('packageEditFamily').value.trim(),
          category: byId('packageEditCategory').value.trim(),
          semanticCode: byId('packageEditSemantic').value.trim(),
          drawingCode: byId('packageEditDrawing').value.trim(),
          designSpeed: byId('packageEditSpeed').value.trim(),
          applicableSpan: byId('packageEditSpan').value.split(/\r?\n/).map((item) => item.trim()).filter(Boolean),
        },
      },
      { op: 'description.update', value: byId('packageEditDescription').value.trim() },
    ];
  } else {
    let parameters;
    try {
      parameters = JSON.parse(byId('packageParametersJson').value);
    } catch (error) {
      setStatus('packageEditStatus', `参数 JSON 不合法：${error.message}`, 'error');
      return;
    }
    if (!parameters || !Array.isArray(parameters.schema) || !Array.isArray(parameters.presets)) {
      setStatus('packageEditStatus', '参数 JSON 必须包含 schema 和 presets 数组', 'error');
      return;
    }
    operations = [{ op: 'parameters.replace', value: parameters }];
  }
  setStatus('packageEditStatus', '正在生成新版本构件包…', 'pending');
  try {
    const result = await api('/package/patch', {
      method: 'POST',
      body: {
        sessionId: state.sessionId,
        packagePath: state.packagePath,
        baseSha256: state.packageEditData?.baseSha256,
        operations,
        note: note || '工作台编辑',
      },
    });
    await loadTree();
    await loadPackage(result.packagePath);
    closePackageEdit();
    setFeedback(`构件包已更新：${result.revision}（旧包保留）`, 'ok');
  } catch (error) {
    setStatus('packageEditStatus', `保存失败：${error.message}`, 'error');
  }
}

async function rebuildGenericDraft(options = {}) {
  if (!state.packagePath) {
    setStatus(options.statusId || 'genericRebuildStatus', '当前会话没有通用草稿', 'warn');
    return false;
  }
  const statusId = options.statusId || 'genericRebuildStatus';
  setStatus(statusId, '正在重建：执行生成器、导出 STEP/GLB、更新 checksums…', 'pending');
  try {
    const result = await api('/generic/rebuild', {
      method: 'POST',
      body: {
        sessionId: state.sessionId,
        packagePath: state.packagePath,
        params: options.params || {},
        generatorSource: options.generatorSource || undefined,
        note: options.note || '工作台重建',
      },
    });
    if (!result.ok) throw new Error(result.error || '重建失败');
    await loadTree();
    await loadSessionMeta();
    await loadPackage(result.packagePath);
    switchView(options.view || 'view-params');
    setStatus(statusId, `✅ 已生成 ${result.revision}：${formatNumber(result.volumeM3)} m³`, 'ok');
    setFeedback(`通用草稿重建完成：${result.revision} · ${formatNumber(result.volumeM3)} m³`, 'ok');
    return true;
  } catch (error) {
    setStatus(statusId, `❌ ${error.message}`, 'error');
    return false;
  }
}

function updatePresetPreviewNotice() {
  const first = state.presets[0];
  if (!first || !state.metadata?.preview?.available) return;
  if (state.selectedPreset && state.selectedPreset !== first.modelNumber) {
    if (state.previewPreset === state.selectedPreset) {
      setGeometryStatus(`型号 ${state.selectedPreset} · ${Object.keys(state.meshParts).length} 个部位`, 'ok');
    } else {
      setGeometryStatus(`3D 为默认型号 ${first.modelNumber} 预览；切换型号会自动重构`, 'warn');
    }
  } else {
    setGeometryStatus(`真实 GLB · ${Object.keys(state.meshParts).length} 个部位`, 'ok');
  }
}

function renderPresetJson() {
  const preset = state.presets.find((item) => item.modelNumber === byId('paramPresetSelect')?.value)
    || state.presets.find((item) => item.modelNumber === state.selectedPreset)
    || state.presets[0];
  byId('paramPresetJson').textContent = preset ? JSON.stringify(preset, null, 2) : '—';
}

function renderParts() {
  if (!state.shapes.length) {
    setHidden('partsEmpty', false);
    setHidden('partsContent', true);
    return;
  }
  setHidden('partsEmpty', true);
  setHidden('partsContent', false);
  const body = byId('partsTableBody');
  body.textContent = '';
  for (const shape of state.shapes) {
    const tr = document.createElement('tr');
    tr.dataset.part = shape.key;

    const visibleCell = document.createElement('td');
    const checkbox = document.createElement('input');
    checkbox.type = 'checkbox';
    const meshes = meshesForShape(shape);
    checkbox.checked = meshes.length > 0 && meshes.every((mesh) => mesh.visible);
    checkbox.disabled = !meshes.length;
    checkbox.addEventListener('change', () => {
      meshes.forEach((mesh) => { mesh.visible = checkbox.checked; });
    });
    visibleCell.appendChild(checkbox);

    const keyCell = document.createElement('td');
    keyCell.appendChild(mono(shape.key));
    const labelCell = document.createElement('td');
    labelCell.textContent = shape.label || shape.key;
    const codeCell = document.createElement('td');
    codeCell.appendChild(mono(shape.code || '—'));
    const typeCell = document.createElement('td');
    typeCell.textContent = shape.geomType || '—';
    const materialCell = document.createElement('td');
    materialCell.textContent = shape.material || '—';
    const actionCell = document.createElement('td');
    const locate = document.createElement('button');
    locate.className = 'action-btn-small';
    locate.textContent = '定位';
    locate.addEventListener('click', () => {
      switchView('view-3d');
      selectPart(shape.key);
    });
    actionCell.appendChild(locate);

    tr.append(visibleCell, keyCell, labelCell, codeCell, typeCell, materialCell, actionCell);
    tr.addEventListener('click', (event) => {
      if (event.target.tagName === 'INPUT' || event.target.tagName === 'BUTTON') return;
      selectPart(shape.key);
    });
    body.appendChild(tr);
  }
}

function renderModels() {
  badge('modelsCount', `${state.presets.length} 个型号`);
  const preselect = byId('presetSelect');
  if (preselect) preselect.textContent = '';
  const paramSelect = byId('paramPresetSelect');
  if (paramSelect) paramSelect.textContent = '';
  if (!state.presets.length) {
    setHidden('modelsEmpty', false);
    setHidden('modelsContent', true);
    return;
  }
  setHidden('modelsEmpty', true);
  setHidden('modelsContent', false);

  const select = byId('presetSelect');
  select.textContent = '';
  for (const preset of state.presets) {
    const option = document.createElement('option');
    option.value = preset.modelNumber;
    option.textContent = preset.modelNumber;
    select.appendChild(option);
  }
  if (state.selectedPreset) select.value = state.selectedPreset;

  const body = byId('modelsTableBody');
  body.textContent = '';
  for (const preset of state.presets) {
    const tr = document.createElement('tr');
    const numberCell = document.createElement('td');
    numberCell.appendChild(mono(preset.modelNumber));
    const labelCell = document.createElement('td');
    labelCell.textContent = preset.label || '—';
    const drawingCell = document.createElement('td');
    drawingCell.textContent = preset.drawingCode || '—';
    const paramCell = document.createElement('td');
    paramCell.textContent = String(Object.keys(preset.params || {}).length);
    const keyCell = document.createElement('td');
    keyCell.appendChild(mono(preset.cacheKey ? `${String(preset.cacheKey).slice(0, 16)}…` : '—'));
    const actionCell = document.createElement('td');
    const view3d = document.createElement('button');
    view3d.className = 'action-btn-small';
    view3d.textContent = '3D';
    view3d.addEventListener('click', () => {
      selectPreset(preset.modelNumber, { preview: true });
      switchView('view-3d');
      setCameraView('iso');
    });
    const viewParams = document.createElement('button');
    viewParams.className = 'action-btn-small';
    viewParams.textContent = '参数';
    viewParams.addEventListener('click', () => {
      selectPreset(preset.modelNumber);
      switchView('view-params');
    });
    actionCell.append(view3d, viewParams);
    tr.append(numberCell, labelCell, drawingCell, paramCell, keyCell, actionCell);
    tr.addEventListener('click', (event) => {
      if (event.target.tagName === 'BUTTON') return;
      selectPreset(preset.modelNumber);
    });
    body.appendChild(tr);
  }
}

function renderSource() {
  const source = state.source;
  const generic = isGenericDraft();
  if (!source?.available) {
    setHidden('sourceEmpty', false);
    setHidden('codeEditorWrap', true);
    setHidden('annotationSidebar', true);
    setHidden('sourceEditorWrap', true);
    setHidden('annotationHistoryPanel', true);
    setHidden('btnFloatAnnotation', true);
    setHidden('annotationDialog', true);
    setHidden('btnToggleAnnotations', true);
    if (byId('btnEditSource')) byId('btnEditSource').hidden = true;
    byId('sourceEmpty').querySelector('h3').textContent = source?.error ? '构建脚本读取失败' : '该构件没有构建脚本';
    byId('sourceEmpty').querySelector('p').textContent = source?.error
      ? source.error
      : 'STEP / IFC 导入的构件为原生几何；参数化草稿请查看“构件参数”与“元数据”。';
    setText('sourcePathLabel', '📄 无构建脚本');
    return;
  }
  setHidden('sourceEmpty', true);
  setHidden('codeEditorWrap', false);
  setHidden('annotationSidebar', false);
  setHidden('sourceEditorWrap', true);
  setHidden('btnFloatAnnotation', true);
  setHidden('btnToggleAnnotations', false);
  if (byId('btnEditSource')) byId('btnEditSource').hidden = !generic;
  setText('sourcePathLabel', `📄 ${source.path}`);
  if (byId('sourceEditor')) byId('sourceEditor').value = source.text || '';
  const lines = source.text.split(/\r?\n/);
  state.selectedLines = [1, Math.min(3, lines.length || 1)];
  renderCodeLines(lines);
  updateSelectedLinesLabel();
  renderAnnotationSidebar();
}

const PY_KEYWORDS = new Set([
  'and', 'as', 'assert', 'async', 'await', 'break', 'class', 'continue',
  'def', 'del', 'elif', 'else', 'except', 'False', 'finally', 'for', 'from',
  'global', 'if', 'import', 'in', 'is', 'lambda', 'None', 'nonlocal', 'not',
  'or', 'pass', 'raise', 'return', 'True', 'try', 'while', 'with', 'yield',
]);

const PY_BUILTINS = new Set([
  'abs', 'all', 'any', 'bool', 'dict', 'enumerate', 'float', 'format',
  'int', 'len', 'list', 'max', 'min', 'open', 'print', 'range', 'round',
  'set', 'str', 'sum', 'tuple', 'zip', 'self', 'cq',
]);

let codeDocstringDelimiter = null;

function appendHighlightedLine(container, line) {
  if (codeDocstringDelimiter) {
    const closing = line.indexOf(codeDocstringDelimiter);
    const span = document.createElement('span');
    span.className = 'tok-string';
    span.textContent = closing < 0 ? line : line.slice(0, closing + 3);
    container.appendChild(span);
    if (closing >= 0) {
      codeDocstringDelimiter = null;
      if (closing + 3 < line.length) {
        container.appendChild(document.createTextNode(line.slice(closing + 3)));
      }
    }
    if (!line) container.appendChild(document.createTextNode('\u00a0'));
    return;
  }
  const triple = /("""|''')/.exec(line);
  if (triple) {
    if (triple.index > 0) {
      container.appendChild(document.createTextNode(line.slice(0, triple.index)));
    }
    const rest = line.slice(triple.index);
    const closing = rest.indexOf(triple[1], 3);
    const span = document.createElement('span');
    span.className = 'tok-string';
    if (closing < 0) {
      codeDocstringDelimiter = triple[1];
      span.textContent = rest;
    } else {
      span.textContent = rest.slice(0, closing + 3);
    }
    container.appendChild(span);
    if (closing >= 0 && triple.index + closing + 3 < line.length) {
      container.appendChild(document.createTextNode(line.slice(triple.index + closing + 3)));
    }
    if (!line) container.appendChild(document.createTextNode('\u00a0'));
    return;
  }
  const tokenPattern = /(#.*$)|('(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*")|(@[A-Za-z_]\w*)|\b(\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)\b|\b([A-Za-z_]\w*)\b/g;
  let cursor = 0;
  let match;
  while ((match = tokenPattern.exec(line)) !== null) {
    if (match.index > cursor) {
      container.appendChild(document.createTextNode(line.slice(cursor, match.index)));
    }
    const token = match[0];
    let className = '';
    if (match[1]) className = 'tok-comment';
    else if (match[2]) className = 'tok-string';
    else if (match[3]) className = 'tok-decorator';
    else if (match[4]) className = 'tok-number';
    else if (match[5]) {
      if (PY_KEYWORDS.has(token)) className = 'tok-keyword';
      else if (PY_BUILTINS.has(token)) className = 'tok-builtin';
      else if (/^\s*\(/.test(line.slice(match.index + token.length))) className = 'tok-function';
    }
    if (className) {
      const span = document.createElement('span');
      span.className = className;
      span.textContent = token;
      container.appendChild(span);
    } else {
      container.appendChild(document.createTextNode(token));
    }
    cursor = match.index + token.length;
  }
  if (cursor < line.length) {
    container.appendChild(document.createTextNode(line.slice(cursor)));
  }
  if (!line) container.appendChild(document.createTextNode('\u00a0'));
}

function renderCodeLines(lines) {
  const lineNumbers = byId('lineNumbers');
  const codeContent = byId('codeContent');
  if (!lineNumbers || !codeContent) return;
  state.codeLines = lines;
  lineNumbers.textContent = '';
  codeContent.textContent = '';
  codeDocstringDelimiter = null;
  const [start, end] = state.selectedLines;
  lines.forEach((line, index) => {
    const lineNo = index + 1;
    const selected = lineNo >= start && lineNo <= end;
    const number = document.createElement('div');
    number.textContent = String(lineNo);
    number.dataset.line = String(lineNo);
    if (selected) number.classList.add('selected');
    number.addEventListener('mousedown', (event) => {
      event.preventDefault();
      beginSelection(lineNo, event.shiftKey);
    });
    lineNumbers.appendChild(number);
    const lineSpan = document.createElement('span');
    lineSpan.className = 'code-line';
    lineSpan.dataset.line = String(lineNo);
    if (selected) lineSpan.classList.add('highlight');
    lineSpan.addEventListener('mousedown', (event) => {
      if (event.button !== 0) return;
      event.preventDefault();
      beginSelection(lineNo, event.shiftKey);
    });
    lineSpan.addEventListener('mouseenter', () => {
      if (state.dragSelecting) extendSelection(lineNo);
    });
    appendHighlightedLine(lineSpan, line);
    codeContent.appendChild(lineSpan);
  });
}

function beginSelection(lineNo, extend) {
  if (extend) {
    state.selectedLines = [Math.min(state.selectionAnchor, lineNo), Math.max(state.selectionAnchor, lineNo)];
  } else {
    state.selectionAnchor = lineNo;
    state.selectedLines = [lineNo, lineNo];
  }
  state.dragSelecting = true;
  renderCodeLines(state.codeLines);
  updateSelectedLinesLabel();
}

function extendSelection(lineNo) {
  const next = [Math.min(state.selectionAnchor, lineNo), Math.max(state.selectionAnchor, lineNo)];
  if (next[0] === state.selectedLines[0] && next[1] === state.selectedLines[1]) return;
  state.selectedLines = next;
  renderCodeLines(state.codeLines);
  updateSelectedLinesLabel();
}

function lineNumberFromEvent(event) {
  const node = event.target.closest?.('[data-line]');
  if (!node) return null;
  const value = Number(node.dataset.line);
  return Number.isFinite(value) ? value : null;
}

function positionAnnotationButton() {
  const button = byId('btnFloatAnnotation');
  const editor = byId('codeEditorWrap');
  const panel = button?.parentElement;
  if (!button || !panel) return;
  if (!state.source?.available || !state.codeLines.length || !editor?.offsetParent) {
    button.hidden = true;
    return;
  }
  const line = document.querySelector(`#codeContent .code-line[data-line="${state.selectedLines[1]}"]`);
  if (!line) {
    button.hidden = true;
    return;
  }
  const lineRect = line.getBoundingClientRect();
  const editorRect = editor.getBoundingClientRect();
  const panelRect = panel.getBoundingClientRect();
  if (lineRect.bottom < editorRect.top + 8 || lineRect.top > editorRect.bottom - 8) {
    button.hidden = true;
    return;
  }
  const top = Math.min(panelRect.height - 40, Math.max(8, lineRect.bottom - panelRect.top + 6));
  const left = Math.min(panelRect.width - 88, Math.max(8, lineRect.right - panelRect.left - 72));
  button.style.top = `${top}px`;
  button.style.left = `${left}px`;
  button.hidden = false;
}

function updateSelectedLinesLabel() {
  const [start, end] = state.selectedLines;
  const label = start === end ? `L${start}` : `L${start}-L${end}`;
  setText('selectedLinesLabel', label);
  setText('selectedLinesDisplay', label);
  positionAnnotationButton();
}

function renderIngest() {
  const metadata = state.metadata;
  if (!metadata) return;
  setHidden('ingestEmpty', true);
  setHidden('ingestContent', false);
  setText('ingestPackage', state.packagePath);
  setText('ingestFamily', `${metadata.familyId || '—'} / ${metadata.revision || '—'}`);
  setText('ingestSha', metadata.packageSha256);
  const checksums = metadata.checksums || {};
  setText('ingestChecksum', checksums.verified ? `✅ ${checksums.checked}/${checksums.fileCount}` : '❌ 校验未通过');
  const provenance = state.meta?.provenance;
  setText('ingestSignature', provenance?.signatureStatus || '未找到 provenance');
  setText('libraryUrlLabel', state.backend.libraryUrl || state.meta?.libraryUrl || 'http://127.0.0.1:5178');
}

// ---------------------------------------------------------------- feedback

function dataUrlToBlob(dataUrl) {
  const match = /^data:([^;,]+)?(;base64)?,(.*)$/s.exec(String(dataUrl || ''));
  if (!match) throw new Error('截图数据格式无效');
  const mime = match[1] || 'image/png';
  const payload = match[2] ? atob(match[3]) : decodeURIComponent(match[3]);
  const bytes = new Uint8Array(payload.length);
  for (let i = 0; i < payload.length; i += 1) bytes[i] = payload.charCodeAt(i);
  return new Blob([bytes], { type: mime });
}

function captureScreenshot() {
  if (!state.renderer || !state.scene || !state.camera) {
    setFeedback('3D 视口尚未就绪', 'warn');
    return null;
  }
  if (!byId('view-3d')?.classList.contains('active')) switchView('view-3d');
  onWindowResize();
  state.renderer.render(state.scene, state.camera);
  const canvas = state.renderer.domElement;
  if (!canvas.width || !canvas.height) {
    setFeedback('3D 画布尚未完成布局，请稍后重试', 'warn');
    return null;
  }
  const dataUrl = canvas.toDataURL('image/png');
  if (!dataUrl || dataUrl.length < 100) {
    setFeedback('截图失败：WebGL 画布为空', 'warn');
    return null;
  }
  if (state.screenshots.length >= 5) {
    setFeedback('一次最多 5 张 3D 截图，请先移除部分截图', 'warn');
    return null;
  }
  const id = `s${Date.now().toString(36)}${Math.random().toString(36).slice(2, 5)}`;
  state.screenshots.push({
    id,
    dataUrl,
    blob: dataUrlToBlob(dataUrl),
    problem: '',
    createdAt: new Date().toISOString(),
  });
  state.screenshotCollapsed = false;
  applyScreenshotCollapse();
  renderScreenshotList();
  setFeedback(`已添加第 ${state.screenshots.length} 张 3D 截图`, 'ok');
  return dataUrl;
}

function currentFeedbackAnnotations() {
  return state.annotationDrafts.map((draft) => ({
    file: state.source?.path || 'generators/generator.py',
    lines: draft.lines,
    comment: draft.comment,
  }));
}

const TASK_TEMPLATES = {
  param: [
    '参数调整：',
    '- 参数名：',
    '- 当前值：',
    '- 目标值：',
    '- 影响型号：',
    '- 原因/依据：',
  ].join('\n'),
  geometry: [
    '几何/构造修改：',
    '- 部位：',
    '- 现状：',
    '- 期望：',
    '- 参考尺寸或图纸：',
  ].join('\n'),
  fix: [
    '问题修复：',
    '- 现象：',
    '- 复现步骤：',
    '- 期望结果：',
  ].join('\n'),
  add: [
    '新增部位：',
    '- 名称：',
    '- 位置/连接关系：',
    '- 尺寸参数：',
    '- 材质：',
  ].join('\n'),
  review: [
    '检查/验收：',
    '- 检查对象：',
    '- 关注点：',
    '- 期望结论：',
  ].join('\n'),
};

function workspacePathForPrompt() {
  return state.workspace?.path
    || (state.packagePath ? state.packagePath.replace(/[\\/][^\\/]+$/, '') : '');
}

function constraintLine(id, workspacePath) {
  switch (id) {
    case 'interface':
      return '只修改 `generators/generator.py`（必要时修改 `component.json` 的参数 schema），保持 `build_parts(params)` 接口不变；';
    case 'parametric':
      return '所有尺寸必须由参数驱动，不要按型号硬编码；长度单位 mm；';
    case 'rebuild':
      return [
        '修改后在工作区执行重建：',
        '```bash',
        `cd ${workspacePath || '<工作区根目录>'}`,
        `python rcif-tools/generic_rebuild.py "${state.packagePath || '<构件包>'}" --output "<新的 .rcif 路径>" --params-json-file "<参数.json>" --source-file "<修改后的 generator.py>"`,
        '```',
      ].join('\n');
    case 'verify':
      return [
        '建议运行安全/几何检查：',
        '```bash',
        'python rcif-tools/verify_sandbox.py "<包含 generators 的目录>" --json',
        '```',
      ].join('\n');
    case 'acceptance':
      return '验收标准：`build_parts(params)` 可执行、尺寸随参数变化、能生成 `display/preview.glb`，包围盒/体积符合需求。';
    case 'summary':
      return '完成后给出修改摘要、参数变化和验收结果。';
    default:
      return '';
  }
}

const CONSTRAINT_IDS = ['interface', 'parametric', 'rebuild', 'verify', 'acceptance', 'summary'];

function constraintSteps(workspacePath) {
  return CONSTRAINT_IDS.map((id) => constraintLine(id, workspacePath)).filter(Boolean);
}

function annotationSourceExcerpt(lines, context = 2) {
  const all = state.codeLines?.length
    ? state.codeLines
    : String(state.source?.text || '').split('\n');
  if (!all.length) return '';
  const start = Math.max(1, Number(lines?.[0]) || 1);
  const end = Math.min(all.length, Number(lines?.[1]) || start);
  const from = Math.max(1, start - context);
  const to = Math.min(all.length, end + context);
  return all
    .slice(from - 1, to)
    .map((line, index) => `${String(from + index).padStart(4)}| ${line}`)
    .join('\n');
}

function promptAnnotationBlocks(annotations) {
  return annotations.map((annotation, index) => {
    const excerpt = annotationSourceExcerpt(annotation.lines);
    return [
      `${index + 1}. ${annotation.file}:${annotation.lines?.[0]}-${annotation.lines?.[1]}`,
      `   批注：${annotation.comment}`,
      excerpt ? `   代码：\n\`\`\`python\n${excerpt}\n\`\`\`` : '',
    ].filter(Boolean).join('\n');
  }).join('\n\n');
}

function promptScreenshotText(screenshots) {
  if (!screenshots.length) return '（无）';
  const workspace = workspacePathForPrompt();
  const sep = workspace.includes('\\') ? '\\' : '/';
  const dir = workspace
    ? `${workspace}${sep}rcif-tools${sep}out${sep}feedback`
    : `<工作区>${sep}rcif-tools${sep}out${sep}feedback`;
  const lines = [
    `共 ${screenshots.length} 张。复制出的提示词不包含图片；发送后图片会保存到以下位置：`,
  ];
  screenshots.forEach((shot, index) => {
    const problem = String(shot.problem || '').trim();
    lines.push(
      `${index + 1}. 截图 ${index + 1}${problem ? `（${problem}）` : ''} — ${dir}${sep}shot-${shot.id}.png`,
    );
  });
  return lines.join('\n');
}

function promptSessionInfo() {
  return state.sessionInfo || {};
}

function promptWorkspaceLines(workspacePath, sourcePath, preset) {
  const info = promptSessionInfo();
  return [
    `- 对话 ID：${state.sessionId || '未绑定'}`,
    `- 构件名称：${info.componentName || state.metadata?.name || '—'}`,
    `- 原文件：${info.inputFile || '—'}`,
    `- 输出构件包：${info.outputPath || state.packagePath || '—'}`,
    `- 工作区根目录：${workspacePath || '（未绑定）'}`,
    `- 当前构件包：${state.packagePath || '（尚未加载构件包）'}`,
    `- 包内构建脚本：${sourcePath}`,
    '- 包内参数定义：component.json',
  ];
}

function feedbackPromptText(textOverride) {
  const text = (textOverride ?? byId('feedbackInput')?.value ?? '').trim();
  const annotations = currentFeedbackAnnotations();
  const screenshots = state.screenshots.slice(0, 5);
  if (!text && !annotations.length && !screenshots.length) return '';

  const workspacePath = workspacePathForPrompt();
  const sourcePath = state.source?.path || 'generators/generator.py';
  const preset = state.presets.find((item) => item.modelNumber === state.selectedPreset)
    || state.presets[0]
    || null;
  const annotationText = promptAnnotationBlocks(annotations);
  const steps = constraintSteps(workspacePath);

  return [
    '# RCIF 参数化构件修改任务',
    '',
    '## 背景',
    '这是一个 RCIF 构件包（.rcif，本质是 zip）的参数化修改任务。构件几何由包内 `generators/generator.py` 的 `build_parts(params)` 生成；参数定义在 `component.json` 的 `parameters.schema` 与 `parameters.presets` 中。',
    '如果包内没有 `generators/generator.py`（例如 STEP/IFC 精确几何包），说明该包没有可修改的参数化构建脚本，请直接指出这一限制。',
    '',
    '## 任务',
    '### 需求',
    text || '（未填写，请以下面的代码批注为准）',
    '',
    '### 代码批注',
    annotationText || '（无）',
    '',
    '### 3D 截图',
    promptScreenshotText(screenshots),
    '',
    '### 约束与验收',
    ...(steps.length ? steps.map((step, index) => `${index + 1}. ${step}`) : ['（当前未选择约束）']),
    '',
    '### 如果你无法访问这些文件',
    '请直接回复“需要 generators/generator.py 和 component.json”，或给出修改后的 `generator.py` 全文与重建命令即可。',
    '',
    '## 工作区与对话',
    ...promptWorkspaceLines(workspacePath, sourcePath, preset),
  ].join('\n');
}

async function copyText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(text);
    return;
  }
  const area = document.createElement('textarea');
  area.value = text;
  area.style.position = 'fixed';
  area.style.opacity = '0';
  document.body.appendChild(area);
  area.select();
  document.execCommand('copy');
  document.body.removeChild(area);
}

const PROMPT_STORAGE_PREFIX = 'rcif:prompt:';
let promptSaveTimer = null;

function promptStorageKey() {
  return `${PROMPT_STORAGE_PREFIX}${state.sessionId || 'default'}`;
}

function loadSavedPrompt() {
  try {
    return window.localStorage.getItem(promptStorageKey()) || '';
  } catch {
    return '';
  }
}

function savePrompt(text) {
  if (!state.sessionId) return;
  try {
    if (text && text.trim()) window.localStorage.setItem(promptStorageKey(), text);
    else window.localStorage.removeItem(promptStorageKey());
  } catch {
    /* 忽略存储失败 */
  }
}

function schedulePromptSave() {
  clearTimeout(promptSaveTimer);
  promptSaveTimer = setTimeout(() => {
    savePrompt(byId('promptEditorText')?.value || '');
    setText('promptEditorStatus', '已保存到当前对话');
  }, 400);
}

function injectTaskIntoPrompt(prompt, taskText) {
  const text = String(taskText || '').trim();
  const base = String(prompt || '').trim();
  if (!text) return base;
  if (!base) return text;
  const lines = base.split('\n');
  const start = lines.findIndex((line) => line.trim() === '### 需求');
  if (start === -1) return `${base}\n\n### 需求\n${text}`;
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i += 1) {
    if (/^#{2,3}\s/.test(lines[i])) {
      end = i;
      break;
    }
  }
  return [...lines.slice(0, start + 1), text, '', ...lines.slice(end)]
    .join('\n')
    .replace(/\n{3,}/g, '\n\n');
}

function buildSendPrompt(taskText) {
  const saved = loadSavedPrompt();
  if (saved) return refreshPromptDynamicSections(saved, taskText);
  return feedbackPromptText(taskText);
}

function closePromptEditor() {
  clearTimeout(promptSaveTimer);
  savePrompt(byId('promptEditorText')?.value || '');
  setHidden('promptEditorModal', true);
}

function removePromptSection(prompt, heading) {
  const lines = String(prompt || '').split('\n');
  const start = lines.findIndex((line) => line.trim() === heading);
  if (start === -1) return prompt;
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i += 1) {
    if (/^#{2,3}\s/.test(lines[i])) {
      end = i;
      break;
    }
  }
  return [...lines.slice(0, start), ...lines.slice(end)]
    .join('\n')
    .replace(/\n{3,}/g, '\n\n');
}

function stripCurrentModelInfo(prompt) {
  const withoutSection = removePromptSection(prompt, '## 当前型号参数');
  return withoutSection
    .split('\n')
    .filter((line) => !/^- 当前型号：/.test(line.trim()))
    .join('\n');
}

function replacePromptSection(prompt, heading, body) {
  const lines = String(prompt || '').split('\n');
  const start = lines.findIndex((line) => line.trim() === heading);
  if (start === -1) return `${prompt}\n\n${heading}\n${body}`;
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i += 1) {
    if (/^#{2,3}\s/.test(lines[i])) {
      end = i;
      break;
    }
  }
  return [...lines.slice(0, start + 1), body, '', ...lines.slice(end)]
    .join('\n')
    .replace(/\n{3,}/g, '\n\n');
}

function refreshPromptDynamicSections(prompt, task) {
  const annotations = currentFeedbackAnnotations();
  const screenshots = state.screenshots.slice(0, 5);
  let next = stripCurrentModelInfo(prompt);
  next = injectTaskIntoPrompt(next, task);
  next = replacePromptSection(next, '### 代码批注', promptAnnotationBlocks(annotations) || '（无）');
  next = replacePromptSection(next, '### 3D 截图', promptScreenshotText(screenshots));
  return next;
}

function promptHeadingEntries(text) {
  return String(text || '')
    .split('\n')
    .map((line, index) => ({ line, index }))
    .filter((entry) => /^##\s+/.test(entry.line));
}

function scrollPromptEditorTo(lineIndex) {
  const area = byId('promptEditorText');
  if (!area) return;
  const lines = area.value.split('\n');
  let offset = 0;
  for (let i = 0; i < lineIndex && i < lines.length; i += 1) {
    offset += lines[i].length + 1;
  }
  area.focus();
  area.setSelectionRange(offset, offset);
  const style = window.getComputedStyle(area);
  const lineHeight = Number.parseFloat(style.lineHeight) || 18;
  area.scrollTop = Math.max(0, lineIndex * lineHeight - 8);
}

function renderPromptNav() {
  const nav = byId('promptNav');
  const area = byId('promptEditorText');
  if (!nav || !area) return;
  nav.textContent = '';
  for (const entry of promptHeadingEntries(area.value)) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'prompt-nav-btn';
    button.textContent = entry.line.replace(/^##\s+/, '').trim();
    button.addEventListener('click', () => scrollPromptEditorTo(entry.index));
    nav.appendChild(button);
  }
}

function openPromptEditor() {
  clearTimeout(promptSaveTimer);
  const saved = loadSavedPrompt();
  const currentTask = (byId('feedbackInput')?.value || '').trim();
  const prompt = saved
    ? refreshPromptDynamicSections(saved, currentTask)
    : feedbackPromptText(currentTask || '（待填写）');
  if (!prompt) {
    setFeedback('当前没有可生成的提示词：请先输入任务，或添加批注/截图', 'warn');
    return;
  }
  const area = byId('promptEditorText');
  if (area) {
    area.value = prompt;
    area.scrollTop = 0;
  }
  renderPromptNav();
  setText('promptEditorStatus', saved ? '已加载当前对话上次修改的提示词' : '已按默认值生成');
  const pane = byId('promptPreviewPane');
  if (pane) pane.hidden = true;
  const toggle = byId('btnTogglePromptPreview');
  if (toggle) toggle.textContent = '预览';
  setHidden('promptEditorModal', false);
  area?.focus();
}

function regeneratePrompt() {
  const task = (byId('feedbackInput')?.value || '').trim();
  const prompt = feedbackPromptText(task || '（待填写）');
  const area = byId('promptEditorText');
  if (!prompt) {
    setText('promptEditorStatus', '当前上下文为空，无法生成');
    return;
  }
  if (area) area.value = prompt;
  renderPromptNav();
  savePrompt(prompt);
  setText('promptEditorStatus', '已按默认值重新生成并保存到当前对话');
}

function insertPromptText(text) {
  const area = byId('promptEditorText');
  if (!area || !text) return;
  const start = area.selectionStart ?? area.value.length;
  const end = area.selectionEnd ?? start;
  const before = area.value.slice(0, start);
  const after = area.value.slice(end);
  const separator = !before
    ? ''
    : (before.endsWith('\n\n') ? '' : (before.endsWith('\n') ? '\n' : '\n\n'));
  const insertion = `${separator}${text}\n`;
  area.value = before + insertion + after;
  const caret = before.length + insertion.length;
  area.focus();
  area.setSelectionRange(caret, caret);
  renderPromptNav();
}

function applyMarkdownAction(action) {
  const area = byId('promptEditorText');
  if (!area) return;
  const start = area.selectionStart;
  const end = area.selectionEnd;
  const selected = area.value.slice(start, end);
  if (action === 'h2') {
    const lineStart = area.value.lastIndexOf('\n', Math.max(0, start - 1)) + 1;
    area.setRangeText('## ', lineStart, lineStart, 'end');
    area.focus();
    renderPromptNav();
    return;
  }
  let replacement;
  switch (action) {
    case 'bold':
      replacement = `**${selected || '粗体'}**`;
      break;
    case 'italic':
      replacement = `*${selected || '斜体'}*`;
      break;
    case 'ul':
      replacement = (selected || '列表项')
        .split('\n')
        .map((line) => `- ${line.replace(/^-\s*/, '')}`)
        .join('\n');
      break;
    case 'code':
      replacement = `\`\`\`python\n${selected || 'pass'}\n\`\`\``;
      break;
    case 'inline':
      replacement = `\`${selected || 'code'}\``;
      break;
    default:
      return;
  }
  area.setRangeText(replacement, start, end, 'end');
  area.focus();
  renderPromptNav();
}

function escapeHtml(text) {
  return text
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;');
}

function inlineMarkdown(text) {
  return text
    .replace(/`([^`]+)`/g, '<code>$1</code>')
    .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
    .replace(/\*([^*]+)\*/g, '<em>$1</em>')
    .replace(/\[([^\]]+)\]\((https?:[^)]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>');
}

function renderMarkdown(text) {
  const lines = escapeHtml(text).split('\n');
  const out = [];
  let inCode = false;
  let listType = null;
  const closeList = () => {
    if (listType) {
      out.push(`</${listType}>`);
      listType = null;
    }
  };
  for (const line of lines) {
    if (line.startsWith('```')) {
      if (inCode) {
        out.push('</code></pre>');
        inCode = false;
      } else {
        closeList();
        out.push('<pre><code>');
        inCode = true;
      }
      continue;
    }
    if (inCode) {
      out.push(line);
      continue;
    }
    const heading = /^(#{1,6})\s+(.*)$/.exec(line);
    if (heading) {
      closeList();
      const level = heading[1].length;
      out.push(`<h${level}>${inlineMarkdown(heading[2])}</h${level}>`);
      continue;
    }
    const bullet = /^-\s+(.*)$/.exec(line);
    if (bullet) {
      if (listType !== 'ul') {
        closeList();
        out.push('<ul>');
        listType = 'ul';
      }
      out.push(`<li>${inlineMarkdown(bullet[1])}</li>`);
      continue;
    }
    const numbered = /^(\d+)\.\s+(.*)$/.exec(line);
    if (numbered) {
      if (listType !== 'ol') {
        closeList();
        out.push('<ol>');
        listType = 'ol';
      }
      out.push(`<li>${inlineMarkdown(numbered[2])}</li>`);
      continue;
    }
    if (!line.trim()) {
      closeList();
      out.push('');
      continue;
    }
    closeList();
    out.push(`<p>${inlineMarkdown(line)}</p>`);
  }
  if (inCode) out.push('</code></pre>');
  closeList();
  return out.join('\n');
}

function togglePromptPreview() {
  const pane = byId('promptPreviewPane');
  const area = byId('promptEditorText');
  if (!pane || !area) return;
  if (pane.hidden) {
    pane.innerHTML = renderMarkdown(area.value);
    pane.hidden = false;
  } else {
    pane.hidden = true;
  }
  const toggle = byId('btnTogglePromptPreview');
  if (toggle) toggle.textContent = pane.hidden ? '预览' : '编辑';
}

async function copyPromptFromEditor() {
  const text = byId('promptEditorText')?.value || '';
  if (!text.trim()) {
    setText('promptEditorStatus', '提示词为空');
    return;
  }
  savePrompt(text);
  try {
    await copyText(text);
    setText('promptEditorStatus', `已复制（${text.length} 字）`);
    setFeedback(`✅ 提示词已复制（${text.length} 字，可直接粘贴给其他智能体）`, 'ok');
  } catch (error) {
    setText('promptEditorStatus', `复制失败：${error.message || '浏览器拒绝访问剪贴板'}`);
  }
}

function updateSendButtonState() {
  const button = byId('btnSendFeedback');
  if (!button) return;
  const hasText = Boolean((byId('feedbackInput')?.value || '').trim());
  const hasAnnotations = state.annotationDrafts.length > 0;
  const hasScreenshots = state.screenshots.length > 0;
  const enabled = hasText || hasAnnotations || hasScreenshots;
  button.disabled = !enabled;
  button.title = enabled ? '发送当前任务给智能助手' : '请输入任务、添加截图或代码批注后再发送';
}

async function sendFeedback() {
  if (!state.backend.ready) {
    setFeedback('尚未连接工作台后端，请使用 DSH 实例（3090）', 'error');
    return;
  }
  const text = (byId('feedbackInput')?.value || '').trim();
  const annotations = currentFeedbackAnnotations();
  const screenshotItems = state.screenshots.slice(0, 5);
  const screenshots = screenshotItems.map((shot) => shot.dataUrl);
  if (!text && !annotations.length && !screenshots.length) {
    setFeedback('当前会话内容为空：请添加代码批注、截图或输入文字', 'warn');
    return;
  }
  bringAssistantToFront();
  const kind = annotations.length ? 'annotation' : (screenshots.length ? 'screenshot' : 'message');
  const prompt = buildSendPrompt(text);
  setFeedback(`正在发送当前任务（提示词 ${prompt.length} 字 · 批注 ${annotations.length} · 截图 ${screenshots.length}）…`, 'pending');
  try {
    const result = await api('/feedback', {
      method: 'POST',
      body: {
        sessionId: state.sessionId,
        kind,
        text,
        prompt,
        annotations,
        screenshotDataUrls: screenshots,
        screenshotMeta: screenshotItems.map((shot) => ({ id: shot.id, problem: shot.problem || '' })),
        packagePath: state.packagePath,
      },
    });
    if (result.delivered) {
      setFeedback(`✅ 已送达：批注 ${annotations.length} · 截图 ${screenshots.length} · 文字 ${text ? 1 : 0}`, 'ok');
      byId('feedbackInput').value = '';
      state.annotationDrafts = [];
      state.activeAnnotationId = null;
      state.screenshots = [];
      renderAnnotationSidebar();
      renderScreenshotList();
      updateChatCounts();
      await loadAnnotationHistory();
    } else {
      setFeedback(`⚠️ 未送达会话：${result.deliveryReason || '宿主未提供会话注入'}`, 'warn');
    }
  } catch (error) {
    setFeedback(`发送失败：${error.message}`, 'error');
  }
}

function annotationId() {
  return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`;
}

function updateChatCounts() {
  setText('chatAnnotationCount', String(state.annotationDrafts.length));
  setText('chatScreenshotCount', String(state.screenshots.length));
}

function locateAnnotation(draft) {
  if (!draft) return;
  state.activeAnnotationId = draft.id;
  state.selectionAnchor = draft.lines[0];
  state.selectedLines = [...draft.lines];
  renderCodeLines(state.codeLines);
  updateSelectedLinesLabel();
  renderAnnotationSidebar();
  const line = document.querySelector(`#codeContent .code-line[data-line="${draft.lines[0]}"]`);
  line?.scrollIntoView({ block: 'center' });
}

function openAnnotationDialog(editId = null) {
  const draft = editId ? state.annotationDrafts.find((item) => item.id === editId) : null;
  state.editingDraftId = editId;
  state.annotationDialogLines = draft ? [...draft.lines] : [...state.selectedLines];
  byId('annotationDialogTitle').textContent = draft ? '编辑代码批注' : '添加代码批注';
  byId('annotationDialogRange').textContent = state.annotationDialogLines[0] === state.annotationDialogLines[1]
    ? `L${state.annotationDialogLines[0]}`
    : `L${state.annotationDialogLines[0]}-L${state.annotationDialogLines[1]}`;
  byId('annotationDialogInput').value = draft?.comment || '';
  setHidden('annotationDialog', false);
  byId('annotationDialogInput').focus();
}

function closeAnnotationDialog() {
  setHidden('annotationDialog', true);
  state.editingDraftId = null;
  byId('annotationDialogInput').value = '';
}

function saveAnnotationDialog() {
  const comment = (byId('annotationDialogInput')?.value || '').trim();
  if (!comment) {
    setStatus('annotationDialogStatus', '批注内容不能为空', 'warn');
    return;
  }
  const [start, end] = state.annotationDialogLines;
  if (state.editingDraftId) {
    const draft = state.annotationDrafts.find((item) => item.id === state.editingDraftId);
    if (draft) {
      draft.lines = [start, end];
      draft.comment = comment;
      state.activeAnnotationId = draft.id;
    }
  } else {
    if (state.annotationDrafts.length >= 10) {
      setStatus('annotationDialogStatus', '一次最多 10 段批注，请先发送或删除', 'warn');
      return;
    }
    const draft = { id: annotationId(), lines: [start, end], comment };
    state.annotationDrafts.push(draft);
    state.activeAnnotationId = draft.id;
  }
  closeAnnotationDialog();
  state.annotationCollapsed = false;
  renderAnnotationSidebar();
  applyAnnotationCollapse();
  updateChatCounts();
  setFeedback(`批注已加入当前会话（共 ${state.annotationDrafts.length} 段）`, 'ok');
}

function readUiFlag(key) {
  try {
    return window.localStorage.getItem(key) === '1';
  } catch {
    return false;
  }
}

function writeUiFlag(key, value) {
  try {
    window.localStorage.setItem(key, value ? '1' : '0');
  } catch {
    /* localStorage unavailable */
  }
}

function applyTreeCollapse() {
  byId('wbTree')?.classList.toggle('collapsed', state.treeCollapsed);
  const button = byId('btnToggleTree');
  if (button) {
    button.textContent = state.treeCollapsed ? '⟩' : '☰';
    button.title = state.treeCollapsed ? '展开工作区与会话' : '收起工作区与会话';
  }
  writeUiFlag('rcif:tree-collapsed', state.treeCollapsed);
  requestAnimationFrame(positionAnnotationButton);
  window.setTimeout(positionAnnotationButton, 220);
}

function applyAnnotationCollapse() {
  const sidebar = byId('annotationSidebar');
  if (!sidebar) return;
  sidebar.hidden = !state.source?.available;
  sidebar.classList.toggle('collapsed', state.annotationCollapsed);
  const toolbar = byId('btnToggleAnnotations');
  if (toolbar) {
    toolbar.hidden = !state.source?.available;
    toolbar.title = state.annotationCollapsed ? '展开批注栏' : '收起批注栏';
    toolbar.classList.toggle('collapsed', state.annotationCollapsed);
  }
  writeUiFlag('rcif:annotation-collapsed', state.annotationCollapsed);
  requestAnimationFrame(positionAnnotationButton);
  window.setTimeout(positionAnnotationButton, 220);
}

function renderAnnotationSidebar() {
  const sidebar = byId('annotationSidebar');
  const list = byId('annotationSidebarList');
  if (!sidebar || !list) return;
  sidebar.hidden = !state.source?.available;
  sidebar.classList.toggle('collapsed', state.annotationCollapsed);
  const toolbarButton = byId('btnToggleAnnotations');
  if (toolbarButton) {
    toolbarButton.hidden = !state.source?.available;
    toolbarButton.title = state.annotationCollapsed ? '展开批注栏' : '收起批注栏';
    toolbarButton.classList.toggle('collapsed', state.annotationCollapsed);
  }
  setText('annotationSidebarCount', String(state.annotationDrafts.length));
  updateSendButtonState();
  list.textContent = '';
  if (!state.annotationDrafts.length) {
    const empty = document.createElement('div');
    empty.className = 'annotation-side-empty';
    empty.textContent = '还没有批注。选中代码后点代码页上的“＋ 批注”。';
    list.appendChild(empty);
    return;
  }
  for (const draft of state.annotationDrafts) {
    const item = document.createElement('div');
    item.className = `annotation-side-item${state.activeAnnotationId === draft.id ? ' active' : ''}`;
    const range = document.createElement('div');
    range.className = 'side-range';
    range.textContent = draft.lines[0] === draft.lines[1] ? `L${draft.lines[0]}` : `L${draft.lines[0]}-L${draft.lines[1]}`;
    const comment = document.createElement('div');
    comment.className = 'side-comment';
    comment.textContent = draft.comment;
    const actions = document.createElement('div');
    actions.className = 'side-actions';
    const edit = document.createElement('button');
    edit.className = 'action-btn-small';
    edit.textContent = '编辑';
    edit.addEventListener('click', (event) => {
      event.stopPropagation();
      openAnnotationDialog(draft.id);
    });
    const remove = document.createElement('button');
    remove.className = 'action-btn-small';
    remove.textContent = '删除';
    remove.addEventListener('click', (event) => {
      event.stopPropagation();
      state.annotationDrafts = state.annotationDrafts.filter((entry) => entry.id !== draft.id);
      if (state.activeAnnotationId === draft.id) state.activeAnnotationId = null;
      renderAnnotationSidebar();
      updateChatCounts();
    });
    actions.append(edit, remove);
    item.append(range, comment, actions);
    item.addEventListener('click', () => locateAnnotation(draft));
    list.appendChild(item);
  }
}

function applyScreenshotCollapse() {
  const sidebar = byId('screenshotSidebar');
  if (!sidebar) return;
  sidebar.classList.toggle('collapsed', state.screenshotCollapsed);
  const toolbar = byId('btnToggleScreenshots');
  if (toolbar) {
    toolbar.title = state.screenshotCollapsed ? '展开截图栏' : '收起截图栏';
    toolbar.classList.toggle('collapsed', state.screenshotCollapsed);
  }
  writeUiFlag('rcif:screenshot-collapsed', state.screenshotCollapsed);
  requestAnimationFrame(onWindowResize);
  window.setTimeout(onWindowResize, 220);
}

function resetScreenshotMarkUi() {
  state.screenshotMarkMode = false;
  state.screenshotMarkStart = null;
  state.screenshotMarkRect = null;
  state.screenshotMarks = [];
  state.screenshotMarkEditing = null;
  const canvas = byId('screenshotMarkCanvas');
  if (canvas) canvas.hidden = true;
  const img = byId('screenshotModalImg');
  if (img) img.hidden = false;
  const input = byId('screenshotInlineInput');
  if (input) input.hidden = true;
  setHidden('btnSaveScreenshotMark', true);
  setHidden('btnCancelScreenshotMark', true);
  setHidden('btnMarkScreenshot', false);
  setHidden('btnCopyScreenshot', false);
}

function openScreenshotModal(index) {
  const shot = state.screenshots[index];
  const dataUrl = shot?.dataUrl;
  const modal = byId('screenshotModal');
  const viewer = byId('screenshotModalImg');
  if (!dataUrl || !modal || !viewer) return;
  state.screenshotModalIndex = index;
  resetScreenshotMarkUi();
  viewer.src = dataUrl;
  modal.hidden = false;
}

function closeScreenshotModal() {
  const viewer = byId('screenshotModalImg');
  resetScreenshotMarkUi();
  setHidden('screenshotModal', true);
  if (viewer) viewer.src = '';
  state.screenshotModalIndex = null;
}

async function copyScreenshotAt(index) {
  const shot = state.screenshots[index];
  const dataUrl = shot?.dataUrl;
  if (!dataUrl) return false;
  let apiError = null;
  try {
    window.focus();
  } catch {
    /* ignore */
  }
  if (navigator.clipboard && typeof window.ClipboardItem === 'function') {
    try {
      const blob = shot.blob instanceof Blob ? shot.blob : dataUrlToBlob(dataUrl);
      await navigator.clipboard.write([new ClipboardItem({ 'image/png': blob })]);
      setFeedback(`✅ 截图 ${index + 1} 已复制到剪贴板`, 'ok');
      return true;
    } catch (error) {
      apiError = error;
    }
  }
  // 保持用户点击手势，立即用选区复制，避免 await 丢失激活状态
  const holder = document.createElement('div');
  holder.contentEditable = 'true';
  holder.style.cssText = 'position:fixed;left:-10000px;top:0;';
  const img = document.createElement('img');
  img.src = dataUrl;
  holder.appendChild(img);
  document.body.appendChild(holder);
  try {
    const range = document.createRange();
    range.selectNode(img);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
    const ok = document.execCommand('copy');
    if (!ok) throw apiError || new Error('浏览器拒绝写入剪贴板');
    setFeedback(`✅ 截图 ${index + 1} 已复制到剪贴板`, 'ok');
    return true;
  } catch (error) {
    const detail = [error.name, error.message].filter(Boolean).join('：') || '浏览器拒绝访问剪贴板';
    const hint = window.self !== window.top
      ? '（当前在 DSH 侧栏内，剪贴板可能被浏览器策略禁止，可点“下载”或在新标签页打开工作台）'
      : '（可点“下载”保存图片）';
    setFeedback(`复制截图失败：${detail}${hint}`, 'error');
    return false;
  } finally {
    window.getSelection()?.removeAllRanges();
    holder.remove();
  }
}

function downloadScreenshotAt(index) {
  const shot = state.screenshots[index];
  if (!shot?.dataUrl) return false;
  const link = document.createElement('a');
  link.href = shot.dataUrl;
  link.download = `shot-${shot.id || index + 1}.png`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setFeedback(`已开始下载截图 ${index + 1}`, 'ok');
  return true;
}

function screenshotCanvasPoint(event) {
  const canvas = byId('screenshotMarkCanvas');
  if (!canvas) return null;
  const rect = canvas.getBoundingClientRect();
  return {
    x: Math.max(0, Math.min(canvas.width, (event.clientX - rect.left) * canvas.width / rect.width)),
    y: Math.max(0, Math.min(canvas.height, (event.clientY - rect.top) * canvas.height / rect.height)),
  };
}

function pointInsideMark(point, mark) {
  return point.x >= mark.x && point.x <= mark.x + mark.w && point.y >= mark.y && point.y <= mark.y + mark.h;
}

function markAtPoint(point) {
  for (let i = state.screenshotMarks.length - 1; i >= 0; i -= 1) {
    if (pointInsideMark(point, state.screenshotMarks[i])) return i;
  }
  return -1;
}

function drawScreenshotMark() {
  const canvas = byId('screenshotMarkCanvas');
  const img = byId('screenshotModalImg');
  if (!canvas || !img) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
  const lineWidth = Math.max(3, canvas.width / 320);
  const fontSize = Math.max(14, canvas.width / 48);
  const marks = state.screenshotMarks;
  const draft = state.screenshotMarkRect;
  const all = draft && draft.w >= 2 && draft.h >= 2
    ? [...marks, { ...draft, text: '' }]
    : marks;
  for (const mark of all) {
    ctx.fillStyle = 'rgba(255, 59, 48, 0.16)';
    ctx.fillRect(mark.x, mark.y, mark.w, mark.h);
    ctx.strokeStyle = '#ff3b30';
    ctx.lineWidth = lineWidth;
    ctx.strokeRect(mark.x, mark.y, mark.w, mark.h);
    const label = String(mark.text || '').trim();
    if (!label) continue;
    ctx.fillStyle = '#ff3b30';
    ctx.font = `${fontSize}px sans-serif`;
    ctx.fillText(label.slice(0, 80), mark.x + lineWidth + 4, Math.max(fontSize, mark.y - lineWidth - 4));
  }
}

function positionMarkInput(mark) {
  const input = byId('screenshotInlineInput');
  const canvas = byId('screenshotMarkCanvas');
  const card = canvas?.closest('.screenshot-modal-card');
  if (!input || !canvas || !card) return;
  const canvasRect = canvas.getBoundingClientRect();
  const cardRect = card.getBoundingClientRect();
  const scaleX = canvasRect.width / canvas.width;
  const scaleY = canvasRect.height / canvas.height;
  const width = Math.min(300, Math.max(160, mark.w * scaleX));
  let left = canvasRect.left - cardRect.left + mark.x * scaleX;
  left = Math.max(4, Math.min(cardRect.width - width - 8, left));
  let top = canvasRect.top - cardRect.top + (mark.y + mark.h) * scaleY + 4;
  if (top + 30 > canvasRect.bottom - cardRect.top) {
    top = canvasRect.top - cardRect.top + mark.y * scaleY - 30;
  }
  input.style.left = `${left}px`;
  input.style.top = `${Math.max(4, top)}px`;
  input.style.width = `${width}px`;
}

function showMarkInput(index) {
  const mark = state.screenshotMarks[index];
  const input = byId('screenshotInlineInput');
  if (!mark || !input) return;
  state.screenshotMarkEditing = index;
  input.value = mark.text || '';
  input.hidden = false;
  positionMarkInput(mark);
  input.focus();
  input.select();
}

function commitMarkInput() {
  const index = state.screenshotMarkEditing;
  if (index === null) return;
  const mark = state.screenshotMarks[index];
  const input = byId('screenshotInlineInput');
  if (mark && input) mark.text = input.value.trim();
  if (input) input.hidden = true;
  state.screenshotMarkEditing = null;
  drawScreenshotMark();
}

function removeMarkAtPoint(point) {
  const index = markAtPoint(point);
  if (index === -1) return false;
  state.screenshotMarks.splice(index, 1);
  if (state.screenshotMarkEditing === index) {
    state.screenshotMarkEditing = null;
    const input = byId('screenshotInlineInput');
    if (input) input.hidden = true;
  }
  drawScreenshotMark();
  return true;
}

async function startScreenshotMark() {
  const index = state.screenshotModalIndex;
  const img = byId('screenshotModalImg');
  const canvas = byId('screenshotMarkCanvas');
  const shot = state.screenshots[index];
  if (index === null || !img || !canvas || !shot) return;
  if (!shot.originalDataUrl) shot.originalDataUrl = shot.dataUrl;
  if (img.src !== shot.originalDataUrl) {
    img.src = shot.originalDataUrl;
    try {
      await img.decode();
    } catch {
      setFeedback('截图尚未加载完成，无法标记', 'warn');
      return;
    }
  }
  if (!img.naturalWidth || !img.naturalHeight) {
    setFeedback('截图尺寸无效，无法标记', 'warn');
    return;
  }
  canvas.width = img.naturalWidth;
  canvas.height = img.naturalHeight;
  canvas.style.width = `${img.clientWidth}px`;
  canvas.style.height = `${img.clientHeight}px`;
  img.hidden = true;
  canvas.hidden = false;
  state.screenshotMarkMode = true;
  state.screenshotMarkStart = null;
  state.screenshotMarkRect = null;
  state.screenshotMarks = Array.isArray(shot.marks)
    ? shot.marks.map((mark) => ({ ...mark }))
    : [];
  state.screenshotMarkEditing = null;
  setHidden('btnSaveScreenshotMark', false);
  setHidden('btnCancelScreenshotMark', false);
  setHidden('btnMarkScreenshot', true);
  setHidden('btnCopyScreenshot', false);
  setFeedback('拖拽可添加多个标记；点击标记修改文字，右键删除，完成后点“保存标记”', 'pending');
  drawScreenshotMark();
}

function cancelScreenshotMark() {
  const index = state.screenshotModalIndex;
  resetScreenshotMarkUi();
  if (index !== null && state.screenshots[index]) {
    byId('screenshotModalImg').src = state.screenshots[index].dataUrl;
  }
}

function saveScreenshotMark() {
  const index = state.screenshotModalIndex;
  const canvas = byId('screenshotMarkCanvas');
  const shot = state.screenshots[index];
  if (index === null || !canvas || !shot) return;
  commitMarkInput();
  if (!state.screenshotMarks.length) {
    setFeedback('请先在图上拖拽框选问题区域', 'warn');
    return;
  }
  state.screenshotMarkRect = null;
  drawScreenshotMark();
  const dataUrl = canvas.toDataURL('image/png');
  shot.dataUrl = dataUrl;
  shot.blob = dataUrlToBlob(dataUrl);
  shot.marks = state.screenshotMarks.map((mark) => ({ ...mark }));
  shot.problem = state.screenshotMarks
    .map((mark) => String(mark.text || '').trim())
    .filter(Boolean)
    .join('；');
  const markCount = state.screenshotMarks.length;
  renderScreenshotList();
  updateChatCounts();
  closeScreenshotModal();
  setFeedback(`✅ 截图 ${index + 1} 已保存 ${markCount} 处标记`, 'ok');
}

function renderScreenshotList() {
  const list = byId('screenshotSidebarList');
  if (!list) return;
  list.textContent = '';
  setText('screenshotSidebarCount', String(state.screenshots.length));
  if (!state.screenshots.length) {
    const empty = document.createElement('div');
    empty.className = 'annotation-side-empty';
    empty.textContent = '还没有 3D 截图。在 3D 视图中点击“📷 截图”。';
    list.appendChild(empty);
  }
  state.screenshots.forEach((shot, index) => {
    const dataUrl = shot.dataUrl;
    const card = document.createElement('div');
    card.className = 'screenshot-card';
    const img = document.createElement('img');
    img.src = dataUrl;
    img.alt = `3D 截图 ${index + 1}`;
    img.title = '点击查看大图';
    img.addEventListener('click', () => openScreenshotModal(index));
    const actions = document.createElement('div');
    actions.className = 'row-actions';
    const label = document.createElement('span');
    label.className = 'label';
    label.textContent = shot.problem ? `截图 ${index + 1} · ${shot.problem}` : `截图 ${index + 1}`;
    label.title = label.textContent;
    const copy = document.createElement('button');
    copy.className = 'action-btn-small';
    copy.textContent = '复制';
    copy.addEventListener('click', () => copyScreenshotAt(index));
    const mark = document.createElement('button');
    mark.className = 'action-btn-small';
    mark.textContent = '标记';
    mark.addEventListener('click', () => {
      openScreenshotModal(index);
      startScreenshotMark();
    });
    const remove = document.createElement('button');
    remove.className = 'action-btn-small';
    remove.textContent = '删除';
    remove.addEventListener('click', () => {
      state.screenshots.splice(index, 1);
      renderScreenshotList();
      updateChatCounts();
    });
    actions.append(label, copy, mark, remove);
    card.append(img, actions);
    list.appendChild(card);
  });
  updateChatCounts();
  updateSendButtonState();
}


async function loadAnnotationHistory() {
  if (!state.sessionId) return;
  try {
    const data = await api(`/annotation-history?sessionId=${encodeURIComponent(state.sessionId)}`);
    state.annotationHistory = Array.isArray(data.history) ? data.history : [];
    renderAnnotationHistory();
  } catch (error) {
    state.annotationHistory = [];
    setFeedback(`批注历史读取失败：${error.message}`, 'warn');
  }
}

function renderAnnotationHistory() {
  const panel = byId('annotationHistoryPanel');
  const list = byId('annotationHistoryList');
  if (!panel || !list) return;
  const history = state.annotationHistory || [];
  panel.hidden = !history.length;
  if (!history.length) return;
  list.textContent = '';
  for (const record of [...history].reverse()) {
    const card = document.createElement('div');
    card.className = 'history-card';
    const top = document.createElement('div');
    top.className = 'history-top';
    const time = document.createElement('span');
    time.className = 'history-time';
    time.textContent = new Date(record.at).toLocaleString();
    const count = document.createElement('span');
    count.className = 'history-count';
    count.textContent = `${record.annotationCount || record.annotations?.length || 0} 段`;
    const delivered = document.createElement('span');
    delivered.className = 'badge';
    delivered.textContent = record.delivered ? '已送达' : '未送达';
    const actions = document.createElement('span');
    actions.className = 'history-actions';
    const load = document.createElement('button');
    load.className = 'action-btn-small';
    load.textContent = '载入草稿';
    load.addEventListener('click', () => {
      state.annotationDrafts = (record.annotations || []).slice(0, 10).map((annotation) => ({
        id: annotationId(),
        lines: annotation.lines || [1, 1],
        comment: annotation.comment || '',
      }));
      state.editingDraftId = null;
      state.activeAnnotationId = null;
      renderAnnotationSidebar();
      updateChatCounts();
      if (state.annotationDrafts.length) {
        state.selectionAnchor = state.annotationDrafts[0].lines[0];
        state.selectedLines = [...state.annotationDrafts[0].lines];
        renderCodeLines(state.codeLines);
        updateSelectedLinesLabel();
      }
      switchView('view-script');
      setFeedback('已载入历史批注，可编辑后随当前会话一起发送', 'ok');
    });
    const remove = document.createElement('button');
    remove.className = 'action-btn-small';
    remove.textContent = '删除';
    remove.addEventListener('click', async () => {
      try {
        await api(`/annotation-history?sessionId=${encodeURIComponent(state.sessionId)}&id=${encodeURIComponent(record.id)}`, { method: 'DELETE' });
        await loadAnnotationHistory();
      } catch (error) {
        setFeedback(`删除失败：${error.message}`, 'error');
      }
    });
    actions.append(load, remove);
    top.append(time, count, delivered, actions);
    const details = document.createElement('details');
    const summary = document.createElement('summary');
    summary.textContent = `查看 ${record.annotations?.length || 0} 段批注内容`;
    details.appendChild(summary);
    for (const annotation of record.annotations || []) {
      const row = document.createElement('div');
      row.className = 'history-annotation';
      const range = document.createElement('span');
      range.className = 'range';
      range.textContent = annotation.lines?.[0] === annotation.lines?.[1]
        ? `L${annotation.lines?.[0]}`
        : `L${annotation.lines?.[0]}-L${annotation.lines?.[1]}`;
      const text = document.createElement('div');
      text.textContent = annotation.comment;
      row.append(range, text);
      details.appendChild(row);
    }
    card.append(top, details);
    list.appendChild(card);
  }
}

// ------------------------------------------------------------- current session

function inferKind(filePath) {
  const ext = String(filePath || '').toLowerCase().split('.').pop();
  if (ext === 'stp') return 'step';
  return ['pdf', 'step', 'ifc', 'rfa'].includes(ext) ? ext : '';
}

/** 会话/对话名称统一取源文件名（去掉扩展名）。 */
function sourceFileStem(filePath) {
  const raw = String(filePath || '').trim();
  if (!raw) return '';
  const base = raw.split(/[\\/]/).pop() || '';
  const dot = base.lastIndexOf('.');
  return dot > 0 ? base.slice(0, dot) : base;
}

function renderSessionPage() {
  const hasSession = Boolean(state.sessionId && state.workspace);
  setHidden('sessionEmpty', hasSession);
  setHidden('sessionContent', !hasSession);
  setHidden('btnCopySession', !hasSession);
  if (!hasSession) return;
  setText('sessionWorkspaceLabel', `${state.workspace.title || ''} · ${state.workspace.path || ''}`);
  setText('sessionIdLabel', state.sessionId);
  setText('sessionPathLabel', state.workspace.path || '-');
  setText('sessionInputsLabel', 'inputs/（相对会话文件夹）');
  setText('sessionPackageLabel', state.packagePath || '当前会话尚未绑定构件包');
}

function updateDirectAvailability() {
  const kind = inferKind(byId('sessionSourceFile')?.value || '');
  const direct = byId('btnSessionDirect');
  const agent = byId('btnSessionConvert');
  const canDirect = ['step', 'ifc', 'rfa'].includes(kind);
  const isPdf = kind === 'pdf';
  if (direct) {
    direct.disabled = !canDirect;
    direct.classList.toggle('primary-btn', canDirect);
    direct.classList.toggle('action-btn-small', !canDirect);
    direct.textContent = canDirect ? '直接转换（推荐）' : '直接转换';
    direct.title = isPdf
      ? 'PDF 只能由智能助手生成规则推断草稿，不支持直接转换'
      : (canDirect ? '直接调用本地转换器，推荐用于 STEP/IFC/RFA' : '');
  }
  if (agent) {
    agent.classList.toggle('primary-btn', !canDirect);
    agent.classList.toggle('action-btn-small', canDirect);
    agent.textContent = canDirect ? '交给智能助手' : (isPdf ? '让智能助手转换（推荐）' : '让智能助手转换');
    agent.title = canDirect
      ? '需要助手分析、改脚本或处理复杂转换时使用'
      : '';
  }
  const hint = byId('sourceHint');
  if (hint) {
    hint.innerHTML = isPdf
      ? 'PDF：仅支持“让智能助手转换”，由助手生成规则推断草稿（非 OCR、非图纸几何识别），必须复核型号与尺寸。'
      : kind === 'rfa'
        ? 'RFA：推荐直接转换；完整解析 Revit Partitions/* GRep B-Rep 几何与 PartAtom 参数，重构参数化 CadQuery 模型并打包 source/grep.json 与 grep-candidates.obj。'
        : canDirect
          ? '推荐直接转换：STEP/IFC/RFA 由本地转换器直接生成 RCIF；需要助手分析或改脚本时再交给智能助手。'
          : '可从会话文件夹的 <code>inputs/</code> 下拉选择；选择外部文件时会先复制到 <code>inputs/</code>。文件类型由后缀自动识别。';
  }
}

function populateSessionForm() {
  const meta = state.sessionInfo || {};
  const input = byId('sessionSourceFile');
  if (!input) return;
  input.value = meta.inputFile || '';
  byId('sessionComponentName').value = meta.componentName || state.metadata?.name || '';
  byId('sessionFamilyId').value = meta.familyId || (/^[A-Za-z][A-Za-z0-9_]{2,31}$/.test(meta.componentName || '') ? meta.componentName : (state.metadata?.familyId || ''));
  byId('sessionOutputPath').value = meta.outputPath || '';
  byId('sessionRevision').value = meta.revision || state.metadata?.revision || '1.0.0';
  state.sessionDirty = false;
  updateDirectAvailability();
}

async function loadSourceOptions() {
  if (!state.sessionId) return;
  try {
    const data = await api(`/session-files?sessionId=${encodeURIComponent(state.sessionId)}`);
    const list = byId('sessionSourceOptions');
    if (!list) return;
    list.textContent = '';
    for (const file of data.files || []) {
      const option = document.createElement('option');
      option.value = file.path;
      option.label = `${file.name} · ${formatBytes(file.bytes)}`;
      list.appendChild(option);
    }
  } catch (error) {
    // inputs 目录尚不存在时保持空列表，不打断当前会话
    state.lastError = error.message;
  }
}

async function loadSessionMeta() {
  if (!state.sessionId) return;
  try {
    const data = await api(`/session-meta?sessionId=${encodeURIComponent(state.sessionId)}`);
    state.sessionInfo = data.meta || null;
    if (data.workspace) {
      state.workspace = {
        ...(state.workspace || {}),
        title: data.workspace.title,
        path: data.workspace.path,
        workspaceId: data.workspace.workspaceId,
      };
    }
    populateSessionForm();
    renderSessionPage();
  } catch (error) {
    setStatus('sessionStatusText', `会话信息读取失败：${error.message}`, 'warn');
  }
}

function sessionFormValues() {
  const inputFile = (byId('sessionSourceFile')?.value || '').trim();
  return {
    sourceKind: inferKind(inputFile),
    inputFile,
    componentName: (byId('sessionComponentName')?.value || '').trim(),
    familyId: (byId('sessionFamilyId')?.value || '').trim(),
    revision: (byId('sessionRevision')?.value || '1.0.0').trim(),
    outputPath: (byId('sessionOutputPath')?.value || '').trim(),
  };
}

async function saveSessionMeta() {
  if (!state.sessionId || !state.workspace) {
    setStatus('sessionStatusText', '请先选择工作区与会话', 'warn');
    return false;
  }
  const values = sessionFormValues();
  try {
    const data = await api('/session-meta', {
      method: 'PUT',
      body: {
        sessionId: state.sessionId,
        inputFile: values.inputFile,
        componentName: values.componentName,
        // 对话名称统一使用源文件名；为空时不覆盖现有名称。
        title: sourceFileStem(values.inputFile) || undefined,
        familyId: values.familyId,
        outputPath: values.outputPath,
        sourceKind: values.sourceKind,
        revision: values.revision,
        packagePath: state.packagePath || '',
      },
    });
    state.sessionInfo = data.meta;
    state.sessionDirty = false;
    try {
      await loadTree();
    } catch {
      /* 会话已保存，树刷新失败不改变保存结果 */
    }
    setStatus('sessionStatusText', `✅ 会话信息已保存（会话名：${data.meta?.title || '—'}）`, 'ok');
    return true;
  } catch (error) {
    setStatus('sessionStatusText', `❌ 保存失败：${error.message}`, 'error');
    return false;
  }
}

async function uploadSessionFile(file) {
  if (!state.sessionId || !state.workspace) {
    setStatus('sessionStatusText', '请先选择工作区与会话', 'warn');
    return;
  }
  if (!file) return;
  setStatus('sessionStatusText', `正在上传 ${file.name}…`, 'pending');
  try {
    const upload = () => fetch(
      `${API}/upload?sessionId=${encodeURIComponent(state.sessionId)}&name=${encodeURIComponent(file.name)}`,
      {
        method: 'POST',
        headers: {
          'content-type': 'application/octet-stream',
          'X-RCIF-Cap-Token': state.capToken || '',
        },
        body: file,
      },
    );
    let response = await upload();
    let data = await response.json();
    if (!response.ok && response.status === 403 && data?.code === 'CAP_TOKEN_INVALID') {
      if (await connectSession(state.sessionId)) {
        response = await upload();
        data = await response.json();
      }
    }
    if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
    byId('sessionSourceFile').value = data.path;
    byId('sessionSourceFile').dispatchEvent(new Event('input', { bubbles: true }));
    state.sessionDirty = true;
    await saveSessionMeta();
    await loadSourceOptions();
    setStatus('sessionStatusText', `✅ 已复制到会话文件夹：${data.path}`, 'ok');
  } catch (error) {
    setStatus('sessionStatusText', `❌ 上传失败：${error.message}`, 'error');
  }
}

async function startSessionConvert(mode) {
  if (!state.sessionId || !state.workspace) {
    setStatus('sessionStatusText', '请先新建/选择工作区与会话', 'warn');
    return;
  }
  const values = sessionFormValues();
  if (!values.inputFile) {
    setStatus('sessionStatusText', '请选择或填写源文件', 'warn');
    return;
  }
  if (!values.sourceKind) {
    setStatus('sessionStatusText', '源文件后缀必须是 .step/.stp/.ifc/.pdf/.rfa', 'warn');
    return;
  }
  if (mode === 'direct' && values.sourceKind === 'pdf') {
    updateDirectAvailability();
    setStatus(
      'sessionStatusText',
      'PDF 不支持直接转换，请使用“让智能助手转换”生成规则推断草稿',
      'warn',
    );
    return;
  }
  if (!values.familyId && !/^[A-Za-z][A-Za-z0-9_]{2,31}$/.test(values.componentName)) {
    setStatus('sessionStatusText', '请填写英文族 ID（3–32 位，字母开头）', 'warn');
    return;
  }
  await saveSessionMeta();
  setHidden('sessionLog', true);

  if (mode === 'agent') {
    const tool = { step: 'rcif_ingest_step', ifc: 'rcif_ingest_ifc', pdf: 'rcif_ingest_pdf', rfa: 'rcif_ingest_rfa' }[values.sourceKind];
    const text = [
      `请使用 ${tool} 工具把源文件转换为 RCIF 构件。`,
      `会话文件夹：${state.workspace.path}`,
      `源文件：${values.inputFile}`,
      values.componentName ? `构件名称：${values.componentName}` : '',
      values.familyId ? `familyId：${values.familyId}` : '',
      `版本：${values.revision}`,
      values.outputPath ? `输出路径：${values.outputPath}` : '',
      '完成后推送到当前工作台会话。',
    ].filter(Boolean).join('\n');
    setStatus('sessionStatusText', '正在把转换任务交给智能助手…', 'pending');
    try {
      const result = await api('/command', {
        method: 'POST',
        body: { sessionId: state.sessionId, text },
      });
      setStatus(
        'sessionStatusText',
        result.delivered ? '✅ 指令已送达；助手完成后构件包会自动加载' : `⚠️ ${result.deliveryReason || '未送达会话'}`,
        result.delivered ? 'ok' : 'warn',
      );
    } catch (error) {
      setStatus('sessionStatusText', `❌ ${error.message}`, 'error');
    }
    return;
  }

  setStatus('sessionStatusText', '正在转换（STEP/IFC/PDF 可能需要 10–60 秒）…', 'pending');
  try {
    const result = await api('/ingest', {
      method: 'POST',
      body: {
        sessionId: state.sessionId,
        kind: values.sourceKind,
        sourcePath: values.inputFile,
        componentName: values.componentName || undefined,
        familyId: values.familyId || values.componentName || undefined,
        revision: values.revision,
        outputPath: values.outputPath || undefined,
      },
    });
    if (!result.ok) {
      setStatus('sessionStatusText', `❌ ${result.error}`, 'error');
      const log = byId('sessionLog');
      log.textContent = result.log || '';
      log.hidden = false;
      return;
    }
    setStatus('sessionStatusText', `✅ 转换完成：${result.packagePath}`, 'ok');
    // 转换后端可能回写会话元数据；这里把对话名称固定为源文件名。
    const sourceStem = sourceFileStem(values.inputFile);
    if (sourceStem) {
      try {
        await api('/session-meta', {
          method: 'PUT',
          body: { sessionId: state.sessionId, title: sourceStem },
        });
      } catch {
        /* 标题同步失败不影响转换结果 */
      }
    }
    await loadTree();
    await loadSessionMeta();
    await loadPackage(result.packagePath, values.sourceKind === 'rfa' ? { view: 'view-3d' } : {});
  } catch (error) {
    setStatus('sessionStatusText', `❌ ${error.message}`, 'error');
  }
}

async function ingestToLibrary() {
  if (!state.packagePath) {
    setStatus('ingestStatus', '尚未加载构件包', 'warn');
    return;
  }
  const note = (byId('ingestNote')?.value || '').trim();
  setStatus('ingestStatus', '正在写入入库清单并提交构件库…', 'pending');
  try {
    const result = await api('/library/ingest', {
      method: 'POST',
      body: { sessionId: state.sessionId, packagePath: state.packagePath, note },
    });
    const pre = byId('ingestResult');
    pre.textContent = JSON.stringify(result, null, 2);
    pre.hidden = false;
    if (result.mode === 'library') {
      setStatus('ingestStatus', `✅ 已入库送审：${result.reviewId || result.status}`, 'ok');
      if (result.libraryComponentUrl) window.open(result.libraryComponentUrl, '_blank');
    } else {
      setStatus('ingestStatus', `✅ 已生成入库清单：${result.inboxPath}（${result.message || ''}）`, 'ok');
    }
  } catch (error) {
    setStatus('ingestStatus', `❌ ${error.message}`, 'error');
  }
}

// ---------------------------------------------------------------- handshake

function shortSessionId(sessionId) {
  const text = String(sessionId || '').replace(/^session-/, '');
  return text.length > 12 ? `${text.slice(0, 4)}…${text.slice(-4)}` : text;
}

async function loadTree() {
  try {
    const data = await api('/tree');
    state.tree = Array.isArray(data.workspaces) ? data.workspaces : [];
    state.assistantRoot = data.root || '';
    const rootLabel = byId('assistantRootLabel');
    if (rootLabel) {
      const parts = state.assistantRoot.split(/[\\/]/).filter(Boolean);
      rootLabel.textContent = parts.length > 1
        ? `…\\${parts[parts.length - 1]}`
        : state.assistantRoot;
      rootLabel.dataset.full = state.assistantRoot;
      rootLabel.setAttribute('aria-label', state.assistantRoot);
      rootLabel.removeAttribute('title');
    }
    renderTree();
  } catch (error) {
    state.lastError = error.message;
    setFeedback(`工作区列表读取失败：${error.message}`, 'error');
  }
}

async function deleteSession(session, workspace) {
  const title = session.title || shortSessionId(session.sessionId);
  if (!window.confirm(`删除会话“${title}”？\n\n会话将从工作台移除并归档；DSH 会话日志和已生成的构件包文件保留。`)) return;
  try {
    await api('/sessions/delete', {
      method: 'POST',
      body: { sessionId: session.sessionId },
    });
    const wasCurrent = state.sessionId === session.sessionId;
    sessionAssets.delete(session.sessionId);
    try {
      window.localStorage.removeItem(`${SESSION_ASSETS_PREFIX}${session.sessionId}`);
    } catch {
      /* ignore */
    }
    await loadTree();
    if (wasCurrent) {
      const nextWorkspace = state.tree.find((item) => item.workspaceId === workspace.workspaceId) || state.tree[0];
      const nextSession = nextWorkspace?.sessions?.[0];
      if (nextSession) await selectSession(nextSession, nextWorkspace);
      else selectWorkspace(null);
    }
    setFeedback(`会话已删除：${title}（DSH 日志保留）`, 'ok');
  } catch (error) {
    setFeedback(`删除会话失败：${error.message}`, 'error');
  }
}

async function deleteWorkspace(workspace) {
  const title = workspace.title || workspace.path;
  const sessionCount = workspace.sessions?.length || 0;
  if (!window.confirm(
    `删除工作区“${title}”？\n\n将归档其 ${sessionCount} 个会话，并删除目录：\n${workspace.path}\n\n目录内的 RCIF、源文件和输出会被永久删除，此操作不可恢复。`,
  )) return;
  try {
    await api('/workspaces/delete', {
      method: 'POST',
      body: { workspaceId: workspace.workspaceId, deleteFiles: true },
    });
    if (workspace.workspaceId) {
      state.collapsedWorkspaces.delete(workspace.workspaceId);
      saveCollapsedWorkspaces();
    }
    await loadTree();
    const nextWorkspace = state.tree[0];
    const nextSession = nextWorkspace?.sessions?.[0];
    if (nextSession) await selectSession(nextSession, nextWorkspace);
    else selectWorkspace(null);
    setFeedback(`工作区已删除：${title}`, 'ok');
  } catch (error) {
    setFeedback(`删除工作区失败：${error.message}`, 'error');
  }
}

function renderTree() {
  const root = byId('treeRoot');
  if (!root) return;
  root.textContent = '';
  if (!state.tree.length) {
    const empty = document.createElement('div');
    empty.className = 'tree-empty';
    empty.textContent = '尚无工作区。点击右上角“＋工作区”在固定目录下创建第一个工作区。';
    root.appendChild(empty);
    return;
  }
  for (const workspace of state.tree) {
    const collapsed = state.collapsedWorkspaces.has(workspace.workspaceId);
    const box = document.createElement('div');
    box.className = `tree-workspace${collapsed ? ' collapsed' : ''}`;

    const head = document.createElement('div');
    head.className = 'tree-workspace-head';
    head.title = workspace.path;

    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'tree-toggle';
    toggle.textContent = collapsed ? '▸' : '▾';
    toggle.title = collapsed
      ? `展开工作区“${workspace.title || workspace.path}”`
      : `收起工作区“${workspace.title || workspace.path}”`;
    toggle.setAttribute('aria-expanded', collapsed ? 'false' : 'true');
    toggle.setAttribute('aria-label', `${collapsed ? '展开' : '收起'}工作区 ${workspace.title || workspace.path}`);
    toggle.addEventListener('click', (event) => {
      event.stopPropagation();
      setWorkspaceCollapsed(workspace.workspaceId, !collapsed);
      renderTree();
    });
    const title = document.createElement('span');
    title.textContent = workspace.title || workspace.path;
    const pathHint = document.createElement('span');
    pathHint.className = 'tree-path';
    pathHint.textContent = `${workspace.sessions?.length || 0} 个会话`;
    const newSession = document.createElement('button');
    newSession.type = 'button';
    newSession.className = 'tree-node-btn';
    newSession.textContent = '＋';
    newSession.title = `在“${workspace.title || workspace.path}”下新建会话`;
    newSession.setAttribute('aria-label', newSession.title);
    newSession.addEventListener('click', (event) => {
      event.stopPropagation();
      createSession(workspace);
    });
    const workspaceDelete = document.createElement('button');
    workspaceDelete.type = 'button';
    workspaceDelete.className = 'tree-delete';
    workspaceDelete.textContent = '×';
    workspaceDelete.title = '删除工作区及其目录';
    workspaceDelete.addEventListener('click', (event) => {
      event.stopPropagation();
      deleteWorkspace(workspace);
    });
    head.append(toggle, title, pathHint, newSession, workspaceDelete);
    head.addEventListener('click', () => {
      selectWorkspace(workspace);
    });

    const sessions = document.createElement('div');
    sessions.className = 'tree-sessions';
    if (!workspace.sessions?.length) {
      const empty = document.createElement('div');
      empty.className = 'tree-empty';
      empty.textContent = '暂无会话，点击工作区右侧的“＋”新建会话。';
      sessions.appendChild(empty);
    }
    for (const session of workspace.sessions || []) {
      const item = document.createElement('div');
      item.className = `tree-session${session.sessionId === state.sessionId ? ' active' : ''}`;
      item.title = session.sessionId;
      item.dataset.sessionId = session.sessionId;
      const name = document.createElement('span');
      const shortId = shortSessionId(session.sessionId);
      const displayTitle = String(session.meta?.title || '').trim() || session.title;
      name.textContent = displayTitle && displayTitle !== session.sessionId
        ? displayTitle
        : `会话 ${shortId}`;
      if (session.hasPackage) {
        const badgeNode = document.createElement('span');
        badgeNode.className = 'tree-badge';
        badgeNode.textContent = '● 已绑定构件';
        name.appendChild(badgeNode);
      }
      const idText = document.createElement('span');
      idText.className = 'tree-session-id';
      idText.textContent = session.meta?.componentName
        ? `${session.meta.componentName} · ${shortId}`
        : `未设置构件名称 · ${shortId}`;
      const sessionDelete = document.createElement('button');
      sessionDelete.type = 'button';
      sessionDelete.className = 'tree-delete tree-session-delete';
      sessionDelete.textContent = '×';
      sessionDelete.title = '删除会话';
      sessionDelete.addEventListener('click', (event) => {
        event.stopPropagation();
        deleteSession(session, workspace);
      });
      item.append(name, idText, sessionDelete);
      item.addEventListener('click', (event) => {
        event.stopPropagation();
        selectSession(session, workspace);
      });
      sessions.appendChild(item);
    }
    box.append(head, sessions);
    root.appendChild(box);
  }
}

const sessionAssets = new Map();
const SESSION_ASSETS_PREFIX = 'rcif:assets:';

function stashSessionAssets() {
  if (!state.sessionId) return;
  const payload = {
    drafts: state.annotationDrafts.map((draft) => ({ ...draft })),
    screenshots: state.screenshots.map((shot) => ({ ...shot })),
  };
  sessionAssets.set(state.sessionId, payload);
  try {
    const text = JSON.stringify(payload);
    if (text.length < 2000000) {
      window.localStorage.setItem(`${SESSION_ASSETS_PREFIX}${state.sessionId}`, text);
    } else {
      window.localStorage.removeItem(`${SESSION_ASSETS_PREFIX}${state.sessionId}`);
    }
  } catch {
    /* 配额不足时只用内存缓存 */
  }
}

function restoreSessionAssets(sessionId) {
  let payload = sessionAssets.get(sessionId);
  if (!payload) {
    try {
      const raw = window.localStorage.getItem(`${SESSION_ASSETS_PREFIX}${sessionId}`);
      if (raw) payload = JSON.parse(raw);
    } catch {
      payload = null;
    }
  }
  state.annotationDrafts = Array.isArray(payload?.drafts)
    ? payload.drafts.map((draft) => ({ ...draft }))
    : [];
  state.screenshots = Array.isArray(payload?.screenshots)
    ? payload.screenshots.map((shot) => ({ ...shot }))
    : [];
  state.activeAnnotationId = null;
}

function selectWorkspace(workspace) {
  stashSessionAssets();
  state.workspace = workspace || null;
  renderApprovalBanner(null);
  expandWorkspace(workspace?.workspaceId);
  const hasCurrent = (workspace?.sessions || []).some((item) => item.sessionId === state.sessionId);
  if (!hasCurrent) {
    state.sessionId = null;
    state.packagePath = null;
    state.meta = null;
    state.component = null;
    state.metadata = null;
    state.presets = [];
    state.schema = [];
    state.shapes = [];
    state.source = { available: false, path: null, text: '' };
    state.selectedPreset = null;
    state.sessionInfo = null;
    state.screenshots = [];
    state.annotationDrafts = [];
    state.activeAnnotationId = null;
    clearModel();
    resetViews();
    clearViewerState();
    renderAnnotationSidebar();
    renderScreenshotList();
    setFeedback('');
    setStatus('sessionStatusText', '');
  }
  renderTree();
  renderSessionPage();
}

function notifyAssistantSession(sessionId) {
  if (!sessionId) return;
  try {
    const channel = new BroadcastChannel('rcif-session-bridge');
    channel.postMessage({ type: 'rcif:open-session', sessionId });
    channel.close();
  } catch {
    /* BroadcastChannel 不可用 */
  }
  try {
    window.localStorage.setItem('rcif:open-session', sessionId);
    window.localStorage.setItem('rcif:last-session', sessionId);
  } catch {
    /* localStorage 不可用 */
  }
}

function bringAssistantToFront() {
  if (state.sessionId) notifyAssistantSession(state.sessionId);
  try {
    if (window.self !== window.top) {
      window.parent?.postMessage(
        { type: 'rcif:focus-assistant', sessionId: state.sessionId },
        window.location.origin,
      );
      window.parent?.focus?.();
      return;
    }
    const base = (state.assistantUrl
      || (window.location.port === '3090' ? `${window.location.origin}/` : 'http://127.0.0.1:3090/'))
      .replace(/\/?$/, '/');
    const win = window.open(base, 'rcif-assistant');
    if (win) {
      try {
        win.focus();
      } catch {
        /* 浏览器可能忽略 focus */
      }
    }
  } catch {
    /* 弹窗被浏览器拦截时忽略 */
  }
}

async function selectSession(session, workspace, options = {}) {
  if (!session?.sessionId) return;
  const { notify = true } = options;
  stashSessionAssets();
  state.autoBound = true;
  renderApprovalBanner(null);
  state.sessionId = session.sessionId;
  state.workspace = workspace || session.workspace || state.workspace;
  expandWorkspace(state.workspace?.workspaceId);
  state.sessionInfo = session.meta || null;
  state.packagePath = null;
  state.meta = null;
  state.component = null;
  state.metadata = null;
  state.presets = [];
  state.schema = [];
  state.shapes = [];
  state.source = { available: false, path: null, text: '' };
  state.selectedPreset = null;
  state.screenshots = [];
  state.annotationDrafts = [];
  state.activeAnnotationId = null;
  restoreSessionAssets(state.sessionId);
  state.sessionDirty = false;
  clearModel();
  resetViews();
  clearViewerState();
  renderAnnotationSidebar();
  renderScreenshotList();
  setFeedback('');
  setStatus('sessionStatusText', '');
  renderSessionPage();
  populateSessionForm();
  setHidden('viewportEmpty', false);

  const url = new URL(window.location.href);
  url.searchParams.set('sessionId', state.sessionId);
  window.history.replaceState(null, '', url.toString());

  await connectSession(state.sessionId);
  await refreshState();
  await loadSessionMeta();
  await loadSourceOptions();
  renderTree();
  if (notify) notifyAssistantSession(state.sessionId);
  switchView('view-session');
}

async function createWorkspace() {
  const name = window.prompt('新建工作区名称（会在“铁路构件智能助手”目录下创建同名文件夹）', '默认工作区');
  if (!name) return;
  try {
    const result = await api('/workspaces', { method: 'POST', body: { name } });
    await loadTree();
    const workspace = state.tree.find(
      (item) => item.workspaceId === result.workspace?.workspaceId || item.title === name,
    );
    if (workspace) {
      state.workspace = workspace;
      expandWorkspace(workspace.workspaceId);
      renderTree();
      await createSession(workspace);
    }
    setStatus('sessionStatusText', `✅ 工作区已创建：${result.workspace?.path || name}`, 'ok');
  } catch (error) {
    setStatus('sessionStatusText', `❌ 新建工作区失败：${error.message}`, 'error');
  }
}

async function createSession(workspaceOverride) {
  let workspace = workspaceOverride || state.workspace;
  if (!workspace) {
    workspace = state.tree[0];
    state.workspace = workspace;
  }
  if (!workspace) {
    await createWorkspace();
    return;
  }
  try {
    const result = await api('/sessions', {
      method: 'POST',
      body: { workspaceId: workspace.workspaceId },
    });
    await loadTree();
    const nextWorkspace = state.tree.find((item) => item.workspaceId === workspace.workspaceId) || workspace;
    const session = nextWorkspace.sessions?.find((item) => item.sessionId === result.sessionId);
    if (session) {
      await selectSession(session, nextWorkspace);
      switchView('view-session');
      setStatus('sessionStatusText', `✅ 新会话已创建：${shortSessionId(result.sessionId)}`, 'ok');
    }
  } catch (error) {
    setStatus('sessionStatusText', `❌ 新建会话失败：${error.message}`, 'error');
  }
}

async function openAssistant() {
  let url = state.assistantUrl;
  let running = true;
  try {
    const info = await api('/assistant');
    url = info.url || url;
    running = info.running !== false;
  } catch {
    /* keep fallback */
  }
  if (!url) {
    setFeedback('未获取到智能助手地址，请检查 DSH 实例', 'error');
    return;
  }
  if (!running) {
    try {
      setFeedback('智能助手未运行，正在启动…', 'pending');
      const launched = await api('/assistant/launch', { method: 'POST', body: { sessionId: state.sessionId } });
      url = launched.url || url;
      setFeedback('智能助手已启动', 'ok');
    } catch (error) {
      setFeedback(`启动智能助手失败：${error.message}`, 'error');
      return;
    }
  }
  if (state.sessionId) notifyAssistantSession(state.sessionId);
  window.open(url, '_blank', 'noopener');
  setFeedback(
    `已打开智能助手；当前工作台绑定会话 ${shortSessionId(state.sessionId)}。若助手未自动选中该会话，请用“复制 ID”核对后切换。`,
    'ok',
  );
}

async function copySessionId() {
  try {
    await navigator.clipboard.writeText(state.sessionId);
    setFeedback(`已复制会话 ID：${state.sessionId}`, 'ok');
  } catch {
    setFeedback(`会话 ID：${state.sessionId}（浏览器拒绝剪贴板，请手动复制）`, 'warn');
  }
}

function setConnectionStatus(text, connected, title = '') {
  const node = byId('sessionStatus');
  if (!node) return;
  node.textContent = text;
  node.className = `status-pill ${connected ? 'connected' : 'disconnected'}`;
  node.title = title;
}

async function connectSession(rawSessionId) {
  const sessionId = String(rawSessionId || state.sessionId || 'session-default').trim() || 'session-default';
  setConnectionStatus('连接中…', false);
  try {
    const response = await fetch(`${API}/init-token?sessionId=${encodeURIComponent(sessionId)}`);
    if (!response.ok) {
      setConnectionStatus(`未握手 (${response.status})`, false);
      return false;
    }
    const data = await response.json();
    state.capToken = data.capToken;
    state.sessionId = data.sessionId || sessionId;
    const url = new URL(window.location.href);
    if (url.searchParams.get('sessionId') !== state.sessionId) {
      url.searchParams.set('sessionId', state.sessionId);
      window.history.replaceState(null, '', url.toString());
    }
    setConnectionStatus('已连接', true, `session: ${state.sessionId}`);
    trace('connect', state.sessionId);
    return true;
  } catch (error) {
    setConnectionStatus('未连接', false, error.message);
    return false;
  }
}

function renderApprovalBanner(info) {
  const banner = byId('approvalBanner');
  if (!banner) return;
  state.pendingApproval = info || null;
  if (!info) {
    banner.hidden = true;
    return;
  }
  const detail = [info.toolName, info.reason].filter(Boolean).join(' · ');
  const text = byId('approvalBannerText');
  if (text) {
    text.textContent = detail
      ? `智能助手正在等待审批：${detail}`
      : '智能助手正在等待审批';
  }
  banner.hidden = false;
}

function jumpToApproval() {
  const sessionId = state.pendingApproval?.sessionId || state.sessionId;
  if (!sessionId) return;
  notifyAssistantSession(sessionId);
  // 工作台嵌在 DSH 右侧栏时，切换会话即可看到审批；独立标签页则另开助手页
  if (window.self !== window.top) {
    try {
      window.parent.postMessage({ type: 'rcif:focus-approval', sessionId }, window.location.origin);
    } catch {
      /* 跨源时忽略 */
    }
    return;
  }
  const base = state.assistantUrl
    || (window.location.port === '3090' ? `${window.location.origin}/` : 'http://127.0.0.1:3090/');
  window.open(base, '_blank', 'noopener');
}

async function refreshState() {
  if (!state.sessionId) {
    renderSessionPage();
    return;
  }
  if (state.loading) return;
  const requestedSession = state.sessionId;
  try {
    const data = await api('/state');
    // 轮询返回可能晚于用户切换会话，丢弃过期响应，避免把包/会话又切回去
    if (state.sessionId !== requestedSession || data.sessionId !== requestedSession) return;
    state.backend.ready = true;
    state.backend.agent = Boolean(data.capabilities?.agent);
    state.backend.ingest = data.capabilities?.ingest || [];
    state.backend.libraryUrl = data.libraryUrl || '';
    state.backend.kind = data.capabilities?.sessionBinding ? 'dsh' : data.backend || 'unknown';
    state.assistantUrl = data.assistantUrl || state.assistantUrl || '';
    if (data.workspace) {
      state.workspace = {
        ...(state.workspace || {}),
        ...data.workspace,
      };
    }
    if (data.sessionInfo) state.sessionInfo = data.sessionInfo;
    const pending = data.pendingApproval
      || (Array.isArray(data.pendingApprovals) ? data.pendingApprovals[0] : null);
    renderApprovalBanner(pending);
    if (!state.sessionDirty) populateSessionForm();
    renderSessionPage();
    if (data.packagePath && data.packagePath !== state.packagePath) {
      await loadPackage(data.packagePath);
    } else if (!data.packagePath && state.packagePath) {
      state.packagePath = null;
      state.meta = null;
      state.component = null;
      state.metadata = null;
      state.presets = [];
      state.schema = [];
      state.shapes = [];
      state.source = { available: false, path: null, text: '' };
      clearModel();
      resetViews();
      renderSessionPage();
      setHidden('viewportEmpty', false);
    } else if (!data.packagePath && !state.packagePath) {
      resetViews();
      renderSessionPage();
    }
  } catch (error) {
    state.backend.ready = false;
    state.legacy = true;
    if (!state.lastError || Date.now() - (state.lastErrorAt || 0) > 15000) {
      state.lastError = error.message;
      state.lastErrorAt = Date.now();
      setFeedback(`后端未就绪：${error.message}。请使用 DSH 实例 http://127.0.0.1:3090/rcif-studio/workbench/`, 'error');
    }
  }
}

// -------------------------------------------------------------------- init

function bindEvents() {
  document.querySelectorAll('.wb-tab').forEach((button) => {
    button.addEventListener('click', () => switchView(button.dataset.view));
  });
  byId('btnNewWorkspace')?.addEventListener('click', createWorkspace);
  byId('btnEmptyWorkspace')?.addEventListener('click', createWorkspace);
  byId('btnEmptySession')?.addEventListener('click', () => createSession());
  byId('btnPickFile')?.addEventListener('click', () => byId('sessionFilePicker')?.click());
  byId('sessionFilePicker')?.addEventListener('change', (event) => {
    uploadSessionFile(event.target.files?.[0]);
    event.target.value = '';
  });
  byId('btnSaveSession')?.addEventListener('click', saveSessionMeta);
  byId('btnSessionConvert')?.addEventListener('click', () => startSessionConvert('agent'));
  byId('btnSessionDirect')?.addEventListener('click', () => startSessionConvert('direct'));
  byId('btnOpenAssistant')?.addEventListener('click', openAssistant);
  byId('btnApprovalJump')?.addEventListener('click', jumpToApproval);
  byId('btnCopySession')?.addEventListener('click', copySessionId);

  for (const id of ['sessionSourceFile', 'sessionComponentName', 'sessionFamilyId', 'sessionRevision', 'sessionOutputPath']) {
    const node = byId(id);
    node?.addEventListener('input', () => {
      state.sessionDirty = true;
      if (id === 'sessionSourceFile') updateDirectAvailability();
    });
    node?.addEventListener('change', () => {
      state.sessionDirty = true;
      if (id === 'sessionSourceFile') updateDirectAvailability();
    });
  }

  byId('btnEditMetadata')?.addEventListener('click', () => openPackageEdit('identity'));
  byId('btnEditParameters')?.addEventListener('click', () => openPackageEdit('parameters'));
  byId('btnClosePackageEdit')?.addEventListener('click', closePackageEdit);
  byId('btnCancelPackageEdit')?.addEventListener('click', closePackageEdit);
  byId('btnSavePackageEdit')?.addEventListener('click', savePackageEdit);
  byId('packageEditDialog')?.addEventListener('click', (event) => {
    if (event.target === byId('packageEditDialog')) closePackageEdit();
  });
  byId('btnReloadPackage')?.addEventListener('click', () => state.packagePath && loadPackage(state.packagePath));
  byId('btnRefreshSource')?.addEventListener('click', loadSource);
  byId('btnGenericRebuild')?.addEventListener('click', () => rebuildGenericDraft({ params: collectGenericParams() }));
  byId('btnEditSource')?.addEventListener('click', () => {
    if (!state.source?.available) return;
    byId('sourceEditor').value = state.source.text || '';
    setHidden('codeEditorWrap', true);
    setHidden('btnFloatAnnotation', true);
    setHidden('annotationSidebar', true);
    setHidden('btnToggleAnnotations', true);
    setHidden('sourceEditorWrap', false);
  });
  byId('btnCancelSource')?.addEventListener('click', () => {
    setHidden('sourceEditorWrap', true);
    setHidden('codeEditorWrap', false);
    setHidden('annotationSidebar', !state.source?.available);
    setHidden('btnToggleAnnotations', !state.source?.available);
    positionAnnotationButton();
  });
  byId('btnRebuildSource')?.addEventListener('click', () => {
    rebuildGenericDraft({
      params: collectGenericParams(),
      generatorSource: byId('sourceEditor')?.value || '',
      note: '按脚本编辑重建',
      statusId: 'sourceRebuildStatus',
      view: 'view-script',
    });
  });
  byId('btnFloatAnnotation')?.addEventListener('click', () => openAnnotationDialog());
  byId('codeEditorWrap')?.addEventListener('scroll', positionAnnotationButton, { passive: true });
  window.addEventListener('resize', positionAnnotationButton);
  byId('btnToggleTree')?.addEventListener('click', () => {
    state.treeCollapsed = !state.treeCollapsed;
    applyTreeCollapse();
  });
  byId('btnFullscreen')?.addEventListener('click', () => toggleFullscreen(document.documentElement));
  byId('btnFullscreen3D')?.addEventListener('click', toggle3DFullscreen);
  byId('btnToggleAnnotations')?.addEventListener('click', () => {
    state.annotationCollapsed = !state.annotationCollapsed;
    applyAnnotationCollapse();
  });
  byId('btnClearAnnotations')?.addEventListener('click', () => {
    state.annotationDrafts = [];
    state.activeAnnotationId = null;
    state.editingDraftId = null;
    closeAnnotationDialog();
    renderAnnotationSidebar();
    updateChatCounts();
  });
  byId('btnAnnotationDialogSave')?.addEventListener('click', saveAnnotationDialog);
  byId('btnAnnotationDialogCancel')?.addEventListener('click', closeAnnotationDialog);
  byId('annotationDialogInput')?.addEventListener('keydown', (event) => {
    if (event.key !== 'Enter' || event.shiftKey || event.isComposing) return;
    event.preventDefault();
    saveAnnotationDialog();
  });
  byId('annotationDialog')?.addEventListener('click', (event) => {
    if (event.target === byId('annotationDialog')) closeAnnotationDialog();
  });

  document.querySelectorAll('[data-view]').forEach((button) => {
    button.addEventListener('click', () => {
      setCameraView(button.dataset.view);
      closeViewMenus();
    });
  });
  byId('btnViewMenu')?.addEventListener('click', () => toggleViewMenu('viewMenu'));
  byId('btnResetCamera')?.addEventListener('click', () => {
    setCameraView('iso');
    closeViewMenus();
  });
  byId('btnFit')?.addEventListener('click', () => {
    fitCamera();
    closeViewMenus();
  });
  byId('btnProjection')?.addEventListener('click', () => {
    setProjection(state.projection === 'perspective' ? 'orthographic' : 'perspective');
    closeViewMenus();
  });
  byId('btnEdges')?.addEventListener('click', toggleEdges);
  byId('btnRenderMode')?.addEventListener('click', () => toggleViewMenu('displayMenu'));
  byId('btnLineWidth')?.addEventListener('click', cycleLineWidth);
  document.querySelectorAll('[data-render-mode]').forEach((button) => {
    button.addEventListener('click', () => {
      applyRenderMode(button.dataset.renderMode);
      setHidden('displayMenu', true);
    });
  });
  document.querySelectorAll('[data-clip-axis]').forEach((button) => {
    button.addEventListener('click', () => {
      setClipAxis(state.clipAxis === button.dataset.clipAxis ? null : button.dataset.clipAxis);
    });
  });
  byId('btnClipMenu')?.addEventListener('click', () => toggleViewMenu('clipMenu'));
  byId('btnMoreMenu')?.addEventListener('click', () => toggleViewMenu('moreMenu'));
  byId('btnClipClear')?.addEventListener('click', () => setClipAxis(null));
  byId('clipRange')?.addEventListener('input', (event) => setClipRatio(event.target.value));
  byId('btnMeasure')?.addEventListener('click', toggleMeasure);
  byId('btnClearMeasure')?.addEventListener('click', resetMeasure);
  byId('btnViewerParts')?.addEventListener('click', toggleViewerParts);
  byId('btnCloseViewerParts')?.addEventListener('click', () => {
    state.viewerPartsOpen = false;
    renderViewerParts();
  });
  byId('btnHideViewTools')?.addEventListener('click', () => {
    closeViewMenus();
    state.viewToolsHidden = true;
    document.querySelector('.viewport-toolbar')?.setAttribute('hidden', '');
    setHidden('btnShowViewTools', false);
  });
  byId('btnShowViewTools')?.addEventListener('click', () => {
    state.viewToolsHidden = false;
    document.querySelector('.viewport-toolbar')?.removeAttribute('hidden');
    setHidden('btnShowViewTools', true);
  });
  document.addEventListener('click', (event) => {
    if (event.target.closest('.view-popover')) return;
    closeViewMenus();
  });
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape') {
      closeViewMenus();
      if (!byId('screenshotModal')?.hidden) closeScreenshotModal();
      else setHidden('screenshotModal', true);
    }
  });
  byId('btnScreenshot')?.addEventListener('click', () => captureScreenshot());
  byId('btnClearScreenshots')?.addEventListener('click', () => {
    state.screenshots = [];
    renderScreenshotList();
    updateChatCounts();
  });
  byId('btnCloseScreenshotModal')?.addEventListener('click', closeScreenshotModal);
  byId('screenshotModal')?.addEventListener('click', (event) => {
    if (event.target === byId('screenshotModal')) closeScreenshotModal();
  });
  byId('btnCopyScreenshot')?.addEventListener('click', () => {
    if (state.screenshotModalIndex !== null) copyScreenshotAt(state.screenshotModalIndex);
  });
  byId('btnDownloadScreenshot')?.addEventListener('click', () => {
    if (state.screenshotModalIndex !== null) downloadScreenshotAt(state.screenshotModalIndex);
  });
  byId('btnMarkScreenshot')?.addEventListener('click', () => startScreenshotMark());
  byId('btnSaveScreenshotMark')?.addEventListener('click', () => saveScreenshotMark());
  byId('btnCancelScreenshotMark')?.addEventListener('click', () => cancelScreenshotMark());
  const markCanvas = byId('screenshotMarkCanvas');
  markCanvas?.addEventListener('pointerdown', (event) => {
    if (!state.screenshotMarkMode) return;
    if (state.screenshotMarkEditing !== null) commitMarkInput();
    const point = screenshotCanvasPoint(event);
    if (!point) return;
    markCanvas.setPointerCapture?.(event.pointerId);
    state.screenshotMarkStart = point;
    state.screenshotMarkRect = { x: point.x, y: point.y, w: 0, h: 0 };
    drawScreenshotMark();
  });
  markCanvas?.addEventListener('pointermove', (event) => {
    if (!state.screenshotMarkMode || !state.screenshotMarkStart) return;
    const point = screenshotCanvasPoint(event);
    if (!point) return;
    const start = state.screenshotMarkStart;
    state.screenshotMarkRect = {
      x: Math.min(start.x, point.x),
      y: Math.min(start.y, point.y),
      w: Math.abs(point.x - start.x),
      h: Math.abs(point.y - start.y),
    };
    drawScreenshotMark();
  });
  markCanvas?.addEventListener('pointerup', (event) => {
    if (!state.screenshotMarkMode || !state.screenshotMarkStart) return;
    const point = screenshotCanvasPoint(event) || state.screenshotMarkStart;
    const rect = state.screenshotMarkRect;
    state.screenshotMarkStart = null;
    state.screenshotMarkRect = null;
    if (!rect || rect.w < 4 || rect.h < 4) {
      const hit = markAtPoint(point);
      if (hit !== -1) showMarkInput(hit);
      else drawScreenshotMark();
      return;
    }
    state.screenshotMarks.push({ ...rect, text: '' });
    drawScreenshotMark();
    showMarkInput(state.screenshotMarks.length - 1);
  });
  markCanvas?.addEventListener('pointercancel', () => {
    state.screenshotMarkStart = null;
    state.screenshotMarkRect = null;
    drawScreenshotMark();
  });
  markCanvas?.addEventListener('contextmenu', (event) => {
    if (!state.screenshotMarkMode) return;
    const point = screenshotCanvasPoint(event);
    if (point && removeMarkAtPoint(point)) event.preventDefault();
  });
  byId('screenshotInlineInput')?.addEventListener('input', () => {
    const index = state.screenshotMarkEditing;
    const input = byId('screenshotInlineInput');
    if (index === null || !input) return;
    const mark = state.screenshotMarks[index];
    if (!mark) return;
    mark.text = input.value;
    drawScreenshotMark();
    positionMarkInput(mark);
  });
  byId('screenshotInlineInput')?.addEventListener('keydown', (event) => {
    if (event.key === 'Enter') {
      event.preventDefault();
      commitMarkInput();
    } else if (event.key === 'Escape') {
      event.preventDefault();
      commitMarkInput();
      if (state.screenshotMarkMode) byId('screenshotMarkCanvas')?.focus?.();
    }
  });
  byId('screenshotInlineInput')?.addEventListener('blur', () => commitMarkInput());
  byId('btnToggleScreenshots')?.addEventListener('click', () => {
    state.screenshotCollapsed = !state.screenshotCollapsed;
    applyScreenshotCollapse();
  });

  byId('btnSendFeedback')?.addEventListener('click', sendFeedback);
  byId('feedbackInput')?.addEventListener('input', updateSendButtonState);
  updateSendButtonState();
  byId('btnPrompt')?.addEventListener('click', openPromptEditor);
  byId('btnClosePromptEditor')?.addEventListener('click', closePromptEditor);
  byId('btnRegeneratePrompt')?.addEventListener('click', regeneratePrompt);
  byId('btnCopyPromptFromEditor')?.addEventListener('click', copyPromptFromEditor);
  byId('btnTogglePromptPreview')?.addEventListener('click', togglePromptPreview);
  byId('promptEditorModal')?.addEventListener('click', (event) => {
    if (event.target === event.currentTarget) closePromptEditor();
  });
  byId('promptEditorText')?.addEventListener('input', () => {
    schedulePromptSave();
    renderPromptNav();
  });
  byId('promptTemplateSelect')?.addEventListener('change', (event) => {
    insertPromptText(TASK_TEMPLATES[event.target.value] || '');
    event.target.value = '';
  });
  byId('promptConstraintSelect')?.addEventListener('change', (event) => {
    insertPromptText(constraintLine(event.target.value, workspacePathForPrompt()));
    event.target.value = '';
  });
  for (const button of document.querySelectorAll('.md-btn[data-md]')) {
    button.addEventListener('click', () => applyMarkdownAction(button.dataset.md));
  }
  byId('btnRefreshAnnotationHistory')?.addEventListener('click', loadAnnotationHistory);
  const trackCodeSelection = (event) => {
    if (!state.dragSelecting) return;
    const lineNo = lineNumberFromEvent(event);
    if (lineNo) extendSelection(lineNo);
  };
  byId('codeContent')?.addEventListener('mousemove', trackCodeSelection);
  byId('lineNumbers')?.addEventListener('mousemove', trackCodeSelection);
  byId('codeEditorWrap')?.addEventListener('scroll', positionAnnotationButton);
  window.addEventListener('resize', positionAnnotationButton);
  window.addEventListener('resize', closeViewMenus);
  document.addEventListener('fullscreenchange', handleFullscreenChange);
  window.addEventListener('mouseup', () => { state.dragSelecting = false; });
  byId('btnLibraryIngest')?.addEventListener('click', ingestToLibrary);
  byId('btnOpenLibrary')?.addEventListener('click', () => {
    const url = state.backend.libraryUrl || 'http://127.0.0.1:5178';
    window.open(url, '_blank');
  });

  byId('presetSelect')?.addEventListener('change', (event) => {
    selectPreset(event.target.value, { preview: true });
  });
  byId('paramPresetSelect')?.addEventListener('change', (event) => {
    selectPreset(event.target.value, { preview: true });
  });
}

function closeViewMenus() {
  for (const id of ['viewMenu', 'displayMenu', 'clipMenu', 'moreMenu']) setHidden(id, true);
}

function positionPopoverMenu(menu) {
  const trigger = menu.parentElement?.querySelector('button');
  if (!trigger) return;
  const rect = trigger.getBoundingClientRect();
  const width = menu.offsetWidth || 160;
  const height = menu.offsetHeight || 120;
  const left = Math.max(8, Math.min(rect.left, window.innerWidth - width - 8));
  let top = rect.bottom + 5;
  if (top + height > window.innerHeight - 8) {
    top = Math.max(8, rect.top - height - 5);
  }
  menu.style.left = `${Math.round(left)}px`;
  menu.style.top = `${Math.round(top)}px`;
}

function toggleViewMenu(menuId) {
  const menu = byId(menuId);
  if (!menu) return;
  const willOpen = menu.hidden;
  closeViewMenus();
  menu.hidden = !willOpen;
  if (willOpen) requestAnimationFrame(() => positionPopoverMenu(menu));
}

function init() {
  state.treeCollapsed = readUiFlag('rcif:tree-collapsed');
  state.annotationCollapsed = readUiFlag('rcif:annotation-collapsed');
  state.screenshotCollapsed = readUiFlag('rcif:screenshot-collapsed');
  bindEvents();
  applyTreeCollapse();
  applyAnnotationCollapse();
  applyScreenshotCollapse();
  updateFullscreenButtons();
  resetViews();
  renderSessionPage();
  init3D();
  initHostHandshake().then(() => {
    refreshState();
    state.pollTimer = setInterval(refreshState, 2500);
    state.treeTimer = setInterval(loadTree, 10000);
  });
}

function findSessionInTree(sessionId) {
  for (const workspace of state.tree) {
    const session = (workspace.sessions || []).find((item) => item.sessionId === sessionId);
    if (session) return { workspace, session };
  }
  return null;
}

async function initHostHandshake() {
  const url = new URL(window.location.href);
  const querySession = url.searchParams.get('sessionId');
  const queryPackage = url.searchParams.get('package');

  window.addEventListener('message', (event) => {
    if (event.origin !== window.location.origin) return;
    if (!event.data || event.data.type !== 'rcif:init') return;
    if (typeof event.data.sessionId === 'string') {
      const found = findSessionInTree(event.data.sessionId);
      if (found) selectSession(found.session, found.workspace);
    }
  });

  await connectSession(querySession || state.sessionId || 'session-default');
  await loadTree();

  let target = querySession ? findSessionInTree(querySession) : null;
  if (!target) {
    try {
      const last = window.localStorage.getItem('rcif:last-session');
      if (last) target = findSessionInTree(last);
    } catch {
      /* ignore */
    }
  }
  if (!target) {
    for (const workspace of state.tree) {
      const session = (workspace.sessions || []).find((item) => item.meta?.packagePath || item.hasPackage);
      if (session) {
        target = { workspace, session };
        break;
      }
    }
  }
  if (!target) {
    for (const workspace of state.tree) {
      if (workspace.sessions?.length) {
        target = { workspace, session: workspace.sessions[0] };
        break;
      }
    }
  }
  if (target) {
    await selectSession(target.session, target.workspace, { notify: false });
  } else {
    state.workspace = state.tree[0] || null;
    state.sessionId = null;
    renderTree();
    renderSessionPage();
  }
  if (queryPackage) await loadPackage(queryPackage);
}

window.addEventListener('DOMContentLoaded', init);

