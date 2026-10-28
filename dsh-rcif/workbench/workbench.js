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
  selectedPart: null,
  screenshots: [],
  scene: null,
  camera: null,
  renderer: null,
  controls: null,
  meshParts: {},
  modelRoot: null,
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
};

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

async function api(path, options = {}) {
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

function init3D() {
  const container = byId('cadCanvasContainer');
  const canvas = byId('cadCanvas');
  if (!container || !canvas) return;

  state.scene = new THREE.Scene();
  state.camera = new THREE.PerspectiveCamera(45, container.clientWidth / container.clientHeight, 10, 500000);
  state.camera.up.set(0, 0, 1);
  try {
    state.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, alpha: true, preserveDrawingBuffer: true });
  } catch (error) {
    setGeometryStatus('WebGL 不可用，3D 视图已禁用', 'warn');
    setFeedback(`WebGL 初始化失败：${error.message}`, 'error');
    return;
  }
  state.renderer.setSize(container.clientWidth, container.clientHeight);
  state.renderer.setPixelRatio(window.devicePixelRatio || 1);

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

  const raycaster = new THREE.Raycaster();
  const pointer = new THREE.Vector2();
  canvas.addEventListener('pointerdown', (event) => {
    const rect = canvas.getBoundingClientRect();
    pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
    raycaster.setFromCamera(pointer, state.camera);
    const meshes = Object.values(state.meshParts).filter((mesh) => mesh.visible);
    if (!meshes.length) return;
    const hits = raycaster.intersectObjects(meshes, false);
    if (hits.length) selectPart(hits[0].object.userData?.name);
  });

  window.addEventListener('resize', onWindowResize);
  setCameraView('iso');
  (function animate() {
    requestAnimationFrame(animate);
    state.controls.update();
    state.renderer.render(state.scene, state.camera);
  })();
}

function onWindowResize() {
  const container = byId('cadCanvasContainer');
  if (!container || !state.renderer) return;
  state.camera.aspect = container.clientWidth / Math.max(1, container.clientHeight);
  state.camera.updateProjectionMatrix();
  state.renderer.setSize(container.clientWidth, container.clientHeight);
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
  if (state.modelRoot) {
    state.scene.remove(state.modelRoot);
    disposeObject(state.modelRoot);
    state.modelRoot = null;
  }
  for (const mesh of Object.values(state.meshParts)) {
    if (mesh.parent === state.scene) state.scene.remove(mesh);
  }
  state.meshParts = {};
}

function setGeometryStatus(text, kind = 'pending') {
  const node = byId('geometryStatus');
  if (!node) return;
  node.textContent = text;
  node.dataset.kind = kind;
}

function partMetadata(name) {
  const shape = state.shapes.find((item) => item.key === name || item.node === name);
  if (!shape) return { name, materialName: '未标注材质', vol: '—', code: '—' };
  return {
    name: shape.key || name,
    materialName: shape.material || shape.geomType || '—',
    vol: shape.volumeM3 ? `${shape.volumeM3} m³` : '—',
    code: shape.code || '—',
  };
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
  const url = `${API}/package/preview.glb?path=${encodeURIComponent(state.packagePath)}`;
  loader.load(
    url,
    (gltf) => {
      clearModel();
      const root = gltf.scene;
      root.rotation.x = -Math.PI / 2; // RCIF GLB 为 Z-up
      root.updateMatrixWorld(true);
      state.modelRoot = root;
      state.scene.add(root);
      let count = 0;
      root.traverse((child) => {
        if (!child.isMesh) return;
        const key = child.name || `Part${count + 1}`;
        child.userData = partMetadata(key);
        if (child.material && 'metalness' in child.material) {
          child.material.metalness = 0.05;
          child.material.roughness = 0.78;
        }
        state.meshParts[key] = child;
        count += 1;
      });
      setHidden('viewportEmpty', true);
      setGeometryStatus(`真实 GLB · ${count} 个部位`, 'ok');
      setCameraView('iso');
      const first = state.shapes[0];
      if (first) selectPart(first.key);
    },
    undefined,
    (error) => {
      clearModel();
      setHidden('viewportEmpty', false);
      byId('viewportEmpty').querySelector('span').textContent = `preview.glb 加载失败：${error?.message || '未知错误'}`;
      setGeometryStatus('预览加载失败', 'warn');
    },
  );
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
      distance = Math.max(sphere.radius * 2.6, 1000);
    }
  }
  state.controls.target.copy(target);
  if (viewType === 'front') state.camera.position.set(target.x, target.y - distance, target.z);
  else if (viewType === 'side') state.camera.position.set(target.x + distance, target.y, target.z);
  else if (viewType === 'plan') state.camera.position.set(target.x, target.y, target.z + distance);
  else state.camera.position.set(target.x - distance * 0.7, target.y - distance * 0.8, target.z + distance * 0.7);
  state.camera.near = Math.max(1, distance / 500);
  state.camera.far = distance * 80;
  state.camera.updateProjectionMatrix();
  state.controls.update();
}

function selectPart(name) {
  if (!name) return;
  const mesh = state.meshParts[name];
  const info = mesh?.userData || partMetadata(name);
  state.selectedPart = name;
  setText('hudPartName', info.name || name);
  setText('hudMaterial', info.materialName);
  setText('hudVolume', info.vol);
  setText('hudCode', info.code);
  for (const [key, item] of Object.entries(state.meshParts)) {
    if (item.material && 'emissive' in item.material) {
      item.material.emissive = new THREE.Color(key === name ? 0x388bfd : 0x000000);
      item.material.emissiveIntensity = key === name ? 0.4 : 0;
    }
  }
  const rows = document.querySelectorAll('#partsTableBody tr');
  rows.forEach((row) => row.classList.toggle('selected', row.dataset.part === name));
}

function toggleWireframe() {
  for (const mesh of Object.values(state.meshParts)) {
    const materials = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
    for (const material of materials) if (material) material.wireframe = !material.wireframe;
  }
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

async function loadPackage(path) {
  if (!path || state.loading) return;
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
    state.selectedLines = [1, 3];
    state.annotationDrafts = [];
    state.activeAnnotationId = null;
    state.screenshots = [];
    renderAnnotationSidebar();
    renderScreenshotList();
    updateChatCounts();

    renderHeader();
    renderMetadata();
    renderParams();
    renderParts();
    renderModels();
    renderIngest();
    switchView('view-metadata');
    await loadSource();
    await loadPreviewMesh();
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
    return;
  }
  setHidden('paramsEmpty', true);
  setHidden('paramsContent', false);

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
  return state.metadata?.qa?.extractionMode === 'generic_heuristic_draft';
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
    checkbox.checked = true;
    checkbox.addEventListener('change', () => {
      const mesh = state.meshParts[shape.key];
      if (mesh) mesh.visible = checkbox.checked;
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
      state.selectedPreset = preset.modelNumber;
      byId('presetSelect').value = preset.modelNumber;
      switchView('view-3d');
      setCameraView('iso');
    });
    const viewParams = document.createElement('button');
    viewParams.className = 'action-btn-small';
    viewParams.textContent = '参数';
    viewParams.addEventListener('click', () => {
      state.selectedPreset = preset.modelNumber;
      byId('paramPresetSelect').value = preset.modelNumber;
      renderPresetJson();
      switchView('view-params');
    });
    actionCell.append(view3d, viewParams);
    tr.append(numberCell, labelCell, drawingCell, paramCell, keyCell, actionCell);
    tr.addEventListener('click', (event) => {
      if (event.target.tagName === 'BUTTON') return;
      state.selectedPreset = preset.modelNumber;
      byId('presetSelect').value = preset.modelNumber;
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
  if (!button) return;
  if (!state.source?.available || !state.codeLines.length || !byId('codeEditorWrap')?.offsetParent) {
    button.hidden = true;
    return;
  }
  const line = document.querySelector(`#codeContent .code-line[data-line="${state.selectedLines[1]}"]`);
  if (!line) {
    button.hidden = true;
    return;
  }
  const rect = line.getBoundingClientRect();
  if (rect.bottom < 40 || rect.top > window.innerHeight - 40) {
    button.hidden = true;
    return;
  }
  button.style.top = `${Math.min(window.innerHeight - 46, Math.max(48, rect.bottom + 6))}px`;
  button.style.left = `${Math.min(window.innerWidth - 100, Math.max(12, rect.right - 72))}px`;
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
  state.screenshots.push(dataUrl);
  state.screenshotCollapsed = false;
  applyScreenshotCollapse();
  renderScreenshotList();
  setFeedback(`已添加第 ${state.screenshots.length} 张 3D 截图`, 'ok');
  return dataUrl;
}

async function sendFeedback() {
  if (!state.backend.ready) {
    setFeedback('尚未连接工作台后端，请使用 DSH 实例（3090）', 'error');
    return;
  }
  const text = (byId('feedbackInput')?.value || '').trim();
  const annotations = state.annotationDrafts.map((draft) => ({
    file: state.source?.path || 'generators/generator.py',
    lines: draft.lines,
    comment: draft.comment,
  }));
  const screenshots = state.screenshots.slice(0, 5);
  if (!text && !annotations.length && !screenshots.length) {
    setFeedback('当前会话内容为空：请添加代码批注、截图或输入文字', 'warn');
    return;
  }
  const kind = annotations.length ? 'annotation' : (screenshots.length ? 'screenshot' : 'message');
  setFeedback(`正在发送当前会话内容（批注 ${annotations.length} · 截图 ${screenshots.length}）…`, 'pending');
  try {
    const result = await api('/feedback', {
      method: 'POST',
      body: {
        sessionId: state.sessionId,
        kind,
        text,
        annotations,
        screenshotDataUrls: screenshots,
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
    const label = byId('annotationToggleLabel');
    if (label) label.textContent = state.annotationCollapsed ? '展开' : '收起';
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
    const label = byId('annotationToggleLabel');
    if (label) label.textContent = state.annotationCollapsed ? '展开' : '收起';
  }
  setText('annotationSidebarCount', String(state.annotationDrafts.length));
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
    const label = byId('screenshotToggleLabel');
    if (label) label.textContent = state.screenshotCollapsed ? '展开' : '收起';
  }
  writeUiFlag('rcif:screenshot-collapsed', state.screenshotCollapsed);
  requestAnimationFrame(onWindowResize);
  window.setTimeout(onWindowResize, 220);
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
  state.screenshots.forEach((dataUrl, index) => {
    const card = document.createElement('div');
    card.className = 'screenshot-card';
    const img = document.createElement('img');
    img.src = dataUrl;
    img.alt = `3D 截图 ${index + 1}`;
    img.title = '点击在新标签页查看大图';
    img.addEventListener('click', () => {
      const win = window.open();
      if (win) win.document.write(`<title>3D 截图 ${index + 1}</title><img src="${dataUrl}" style="max-width:100%;height:auto">`);
    });
    const actions = document.createElement('div');
    actions.className = 'row-actions';
    const label = document.createElement('span');
    label.className = 'label';
    label.textContent = `截图 ${index + 1}`;
    const remove = document.createElement('button');
    remove.className = 'action-btn-small';
    remove.textContent = '删除';
    remove.addEventListener('click', () => {
      state.screenshots.splice(index, 1);
      renderScreenshotList();
      updateChatCounts();
    });
    actions.append(label, remove);
    card.append(img, actions);
    list.appendChild(card);
  });
  updateChatCounts();
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
  return ['pdf', 'step', 'ifc'].includes(ext) ? ext : '';
}

function renderSessionPage() {
  const hasSession = Boolean(state.sessionId && state.workspace);
  setHidden('sessionEmpty', hasSession);
  setHidden('sessionContent', !hasSession);
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
  const isPdf = kind === 'pdf';
  if (direct) {
    direct.disabled = isPdf;
    direct.title = isPdf
      ? 'PDF 只能由智能助手生成规则推断草稿，不支持直接转换'
      : '';
  }
  const hint = byId('sourceHint');
  if (hint) {
    hint.innerHTML = isPdf
      ? 'PDF：仅支持“让智能助手转换”，由助手生成规则推断草稿（非 OCR、非图纸几何识别），必须复核型号与尺寸。'
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
        familyId: values.familyId,
        outputPath: values.outputPath,
        sourceKind: values.sourceKind,
        revision: values.revision,
        packagePath: state.packagePath || '',
      },
    });
    state.sessionInfo = data.meta;
    state.sessionDirty = false;
    setStatus('sessionStatusText', '✅ 会话信息已保存', 'ok');
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
    const response = await fetch(
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
    const data = await response.json();
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
    setStatus('sessionStatusText', '源文件后缀必须是 .step/.stp/.ifc/.pdf', 'warn');
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
    const tool = { step: 'rcif_ingest_step', ifc: 'rcif_ingest_ifc', pdf: 'rcif_ingest_pdf' }[values.sourceKind];
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
    await loadTree();
    await loadSessionMeta();
    await loadPackage(result.packagePath);
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
    setText('assistantRootLabel', state.assistantRoot);
    renderTree();
  } catch (error) {
    state.lastError = error.message;
    setFeedback(`工作区列表读取失败：${error.message}`, 'error');
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
    const box = document.createElement('div');
    box.className = 'tree-workspace';

    const head = document.createElement('div');
    head.className = 'tree-workspace-head';
    head.title = workspace.path;
    const icon = document.createElement('span');
    icon.textContent = '▸';
    const title = document.createElement('span');
    title.textContent = workspace.title || workspace.path;
    const pathHint = document.createElement('span');
    pathHint.className = 'tree-path';
    pathHint.textContent = `${workspace.sessions?.length || 0} 个会话`;
    head.append(icon, title, pathHint);
    head.addEventListener('click', () => {
      selectWorkspace(workspace);
    });

    const sessions = document.createElement('div');
    sessions.className = 'tree-sessions';
    if (!workspace.sessions?.length) {
      const empty = document.createElement('div');
      empty.className = 'tree-empty';
      empty.textContent = '暂无会话，点击左下角新建会话。';
      sessions.appendChild(empty);
    }
    for (const session of workspace.sessions || []) {
      const item = document.createElement('div');
      item.className = `tree-session${session.sessionId === state.sessionId ? ' active' : ''}`;
      item.title = session.sessionId;
      const name = document.createElement('span');
      const shortId = shortSessionId(session.sessionId);
      name.textContent = session.title && session.title !== session.sessionId
        ? session.title
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
      item.append(name, idText);
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

function selectWorkspace(workspace) {
  state.workspace = workspace || null;
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

async function selectSession(session, workspace, options = {}) {
  if (!session?.sessionId) return;
  const { notify = true } = options;
  state.autoBound = true;
  state.sessionId = session.sessionId;
  state.workspace = workspace || session.workspace || state.workspace;
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
      setFeedback(`后端未就绪：${error.message}。请使用 DSH 实例 http://127.0.0.1:3080/rcif-studio/workbench/`, 'error');
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
  byId('btnNewSession')?.addEventListener('click', () => createSession());
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

  byId('btnReloadPackage')?.addEventListener('click', () => state.packagePath && loadPackage(state.packagePath));
  byId('btnRefreshSource')?.addEventListener('click', loadSource);
  byId('btnGenericRebuild')?.addEventListener('click', () => rebuildGenericDraft({ params: collectGenericParams() }));
  byId('btnEditSource')?.addEventListener('click', () => {
    if (!state.source?.available) return;
    byId('sourceEditor').value = state.source.text || '';
    setHidden('codeEditorWrap', true);
    setHidden('btnFloatAnnotation', true);
    setHidden('annotationSidebar', true);
    setHidden('sourceEditorWrap', false);
  });
  byId('btnCancelSource')?.addEventListener('click', () => {
    setHidden('sourceEditorWrap', true);
    setHidden('codeEditorWrap', false);
    setHidden('annotationSidebar', !state.source?.available);
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

  byId('btnViewIso')?.addEventListener('click', () => setCameraView('iso'));  byId('btnViewFront')?.addEventListener('click', () => setCameraView('front'));
  byId('btnViewSide')?.addEventListener('click', () => setCameraView('side'));
  byId('btnViewPlan')?.addEventListener('click', () => setCameraView('plan'));
  byId('btnResetCamera')?.addEventListener('click', () => setCameraView('iso'));
  byId('btnWireframe')?.addEventListener('click', toggleWireframe);
  byId('btnScreenshot')?.addEventListener('click', () => captureScreenshot());
  byId('btnClearScreenshots')?.addEventListener('click', () => {
    state.screenshots = [];
    renderScreenshotList();
    updateChatCounts();
  });
  byId('btnToggleScreenshots')?.addEventListener('click', () => {
    state.screenshotCollapsed = !state.screenshotCollapsed;
    applyScreenshotCollapse();
  });

  byId('btnSendFeedback')?.addEventListener('click', sendFeedback);
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
  document.addEventListener('fullscreenchange', handleFullscreenChange);
  window.addEventListener('mouseup', () => { state.dragSelecting = false; });
  byId('btnLibraryIngest')?.addEventListener('click', ingestToLibrary);
  byId('btnOpenLibrary')?.addEventListener('click', () => {
    const url = state.backend.libraryUrl || 'http://127.0.0.1:5178';
    window.open(url, '_blank');
  });

  byId('presetSelect')?.addEventListener('change', (event) => {
    state.selectedPreset = event.target.value;
    if (byId('paramPresetSelect')) byId('paramPresetSelect').value = event.target.value;
    renderPresetJson();
  });
  byId('paramPresetSelect')?.addEventListener('change', (event) => {
    state.selectedPreset = event.target.value;
    if (byId('presetSelect')) byId('presetSelect').value = event.target.value;
    renderPresetJson();
  });
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

