/**
 * 工业级 Three.js 视口引擎
 * - LineSegments2 / LineMaterial 抗锯齿矢量黑边，按 DPR 同步 resolution
 * - GPU 双通道 Stencil 模板缓冲实现 X/Y/Z 剖切加盖
 * - Raycast 面/边拾取 -> faceIds -> faceToShape / edgeToShape 双向反查
 * - 顶点吸附式点对点测距（含水平投影与高差）
 * - WebGL Context Lost 优雅降级与一键重建
 */
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import { LineSegments2 } from 'three/examples/jsm/lines/LineSegments2.js';
import { LineSegmentsGeometry } from 'three/examples/jsm/lines/LineSegmentsGeometry.js';
import { LineMaterial } from 'three/examples/jsm/lines/LineMaterial.js';
import type { DecodedJhb } from '@/jhb/decode';

export type ClipAxis = 'x' | 'y' | 'z';
export type ViewPreset = 'iso' | 'front' | 'side' | 'top';
export type ProjectionMode = 'perspective' | 'orthographic';
export type RenderMode = 'shaded' | 'wireframe' | 'xray';

export interface PickInfo {
  kind: 'face' | 'edge';
  shapeIndex: number;
  shapeKey: string;
  faceId?: number;
  edgeIndex?: number;
  point: [number, number, number];
}

export interface MeasureResult {
  a: [number, number, number];
  b: [number, number, number];
  distance: number;
  horizontal: number;
  deltaZ: number;
  aLabel?: string;
  bLabel?: string;
}

export interface ViewerCallbacks {
  onPick?: (info: PickInfo | null) => void;
  onMeasure?: (result: MeasureResult | null) => void;
  onContextLost?: () => void;
  onContextRestored?: () => void;
  onGeometryLoaded?: (info: { bounds: THREE.Box3; triangles: number }) => void;
  onAxesChange?: (axes: { x: [number, number]; y: [number, number]; z: [number, number] }) => void;
  onError?: (message: string) => void;
}

interface GeometryState {
  decoded: DecodedJhb;
  faceToShape: number[];
  edgeToShape: number[];
  shapeKeys: string[];
  segments: number[];
  segmentShape: number[];
  segmentPolyline: number[];
}

const BACKGROUND = 0x101d2c;
const EDGE_BLACK = 0x0b0f14;
const EDGE_WIREFRAME = 0x9cc7e8;
const EDGE_XRAY = 0xa8bfd4;
const HIGHLIGHT = 0x38bdf8;
const CAP_COLOR = 0xd1d5db;

export class ViewerEngine {
  private container: HTMLElement;
  private callbacks: ViewerCallbacks;
  private renderer!: THREE.WebGLRenderer;
  private scene!: THREE.Scene;
  private camera!: THREE.PerspectiveCamera | THREE.OrthographicCamera;
  private controls!: OrbitControls;
  private resizeObserver!: ResizeObserver;

  private mesh: THREE.Mesh | null = null;
  private edgeLines: LineSegments2 | null = null;
  private edgeMaterial: LineMaterial | null = null;
  private highlightMesh: THREE.Mesh | null = null;
  private highlightEdges: LineSegments2 | null = null;
  private hoverMesh: THREE.Mesh | null = null;
  private pickLines: THREE.LineSegments | null = null;

  private grid: THREE.GridHelper | null = null;
  private lights: THREE.Object3D[] = [];
  private compareGroup = new THREE.Group();
  private compareDecoded: DecodedJhb | null = null;
  private compareEdges: LineSegments2 | null = null;
  private measureGroup = new THREE.Group();
  private measurePoints: THREE.Vector3[] = [];
  private measureLabels: string[] = [];
  private snapMarker: THREE.Mesh | null = null;

  private clippingGroup: THREE.Group | null = null;
  private capMesh: THREE.Mesh | null = null;
  private clipPlane: THREE.Plane | null = null;
  private clipAxis: ClipAxis | null = null;
  private clipRatio = 0.76;

  private state: GeometryState | null = null;
  private bounds = new THREE.Box3(new THREE.Vector3(-1, -1, -1), new THREE.Vector3(1, 1, 1));
  private radius = 1;
  private projection: ProjectionMode = 'perspective';
  private renderMode: RenderMode = 'shaded';
  private edgesVisible = true;
  private view: ViewPreset = 'iso';
  private currentShape: number | null = null;
  private measureMode = false;
  private hiddenShapes = new Set<number>();
  /** 可见索引中的局部三角形序号 -> 原始解码网格三角形序号 */
  private visibleTriangleMap: number[] | null = null;
  private lastAxesKey = '';
  private material: THREE.MeshStandardMaterial | null = null;
  private disposed = false;
  private contextLost = false;
  private pointerDown = { x: 0, y: 0, time: 0 };
  private hoverTimer: ReturnType<typeof setTimeout> | null = null;
  private raycaster = new THREE.Raycaster();
  private pointer = new THREE.Vector2();

  constructor(container: HTMLElement, callbacks: ViewerCallbacks = {}) {
    this.container = container;
    this.callbacks = callbacks;
    this.initRenderer();
    this.initScene();
    this.resizeObserver = new ResizeObserver(() => this.resize());
    this.resizeObserver.observe(container);
    window.addEventListener('resize', this.handleWindowResize);
    this.animate();
  }

  // ------------------------------------------------------------------ setup
  private initRenderer() {
    this.renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: false,
      stencil: true,
      powerPreference: 'high-performance',
    });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    this.renderer.setSize(this.container.clientWidth || 640, this.container.clientHeight || 480, false);
    this.renderer.setClearColor(BACKGROUND, 1);
    this.renderer.outputColorSpace = THREE.SRGBColorSpace;
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
    this.renderer.toneMappingExposure = 1.12;
    this.renderer.localClippingEnabled = true;
    this.renderer.autoClear = true;
    this.renderer.domElement.style.display = 'block';
    this.renderer.domElement.style.width = '100%';
    this.renderer.domElement.style.height = '100%';
    this.renderer.domElement.style.touchAction = 'none';
    this.renderer.domElement.setAttribute('aria-label', '三维构件视口');
    this.container.appendChild(this.renderer.domElement);

    this.renderer.domElement.addEventListener('webglcontextlost', this.handleContextLost, false);
    this.renderer.domElement.addEventListener('webglcontextrestored', this.handleContextRestored, false);
    this.renderer.domElement.addEventListener('pointerdown', this.handlePointerDown);
    this.renderer.domElement.addEventListener('pointerup', this.handlePointerUp);
    this.renderer.domElement.addEventListener('pointermove', this.handlePointerMove);
    this.renderer.domElement.addEventListener('pointerleave', this.handlePointerLeave);
  }

  private initScene() {
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(BACKGROUND);
    this.scene.add(this.measureGroup);
    this.scene.add(this.compareGroup);

    const hemi = new THREE.HemisphereLight(0xf8fbff, 0x111c2e, 1.55);
    const key = new THREE.DirectionalLight(0xffffff, 2.3);
    key.position.set(1.4, 1.1, 1.8);
    const fill = new THREE.DirectionalLight(0xa9c7e8, 0.85);
    fill.position.set(-1.6, -0.8, 0.6);
    const rim = new THREE.DirectionalLight(0xdfe9f5, 0.55);
    rim.position.set(0, 1.8, -1.4);
    const top = new THREE.DirectionalLight(0xffffff, 0.95);
    top.position.set(0, 0, 3);
    this.lights = [hemi, key, fill, rim, top];
    for (const light of this.lights) this.scene.add(light);

    this.camera = this.createCamera('perspective');
    this.controls = this.createControls(this.camera);
    this.camera.position.set(2.2, 2.2, 1.8);
    this.controls.target.set(0, 0, 0);
    this.controls.update();
  }

  private createCamera(mode: ProjectionMode) {
    const aspect = this.container.clientWidth / Math.max(1, this.container.clientHeight);
    if (mode === 'orthographic') {
      const frustum = this.radius * 1.25;
      const camera = new THREE.OrthographicCamera(
        -frustum * aspect,
        frustum * aspect,
        frustum,
        -frustum,
        0.1,
        200000,
      );
      camera.up.set(0, 0, 1);
      return camera;
    }
    const camera = new THREE.PerspectiveCamera(38, aspect, 0.5, 400000);
    camera.up.set(0, 0, 1);
    return camera;
  }

  private createControls(camera: THREE.Camera) {
    const controls = new OrbitControls(camera, this.renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.085;
    controls.rotateSpeed = 0.85;
    controls.panSpeed = 0.75;
    controls.zoomSpeed = 0.95;
    controls.screenSpacePanning = true;
    controls.minDistance = 1;
    controls.maxDistance = 100000;
    controls.mouseButtons = {
      LEFT: THREE.MOUSE.ROTATE,
      MIDDLE: THREE.MOUSE.DOLLY,
      RIGHT: THREE.MOUSE.PAN,
    };
    return controls;
  }

  // -------------------------------------------------------------- geometry
  setGeometry(decoded: DecodedJhb, options: { fit?: boolean } = {}) {
    this.clearGeometry();
    this.state = {
      decoded,
      faceToShape: decoded.header.faceToShape ?? [],
      edgeToShape: decoded.header.edgeToShape ?? [],
      shapeKeys: decoded.header.shapeKeys ?? [],
      segments: [],
      segmentShape: [],
      segmentPolyline: [],
    };
    this.hiddenShapes.clear();
    this.visibleTriangleMap = null;

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(decoded.positions, 3));
    geometry.setAttribute('normal', new THREE.BufferAttribute(decoded.normals, 3));
    geometry.setIndex(new THREE.BufferAttribute(decoded.indices, 1));
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();
    this.bounds = geometry.boundingBox?.clone() ?? new THREE.Box3();
    const size = this.bounds.getSize(new THREE.Vector3());
    this.radius = Math.max(size.length() / 2, 1);

    this.material = new THREE.MeshStandardMaterial({
      color: 0xc4ced9,
      metalness: 0.12,
      roughness: 0.8,
      polygonOffset: true,
      polygonOffsetFactor: 1,
      polygonOffsetUnits: 1,
      side: THREE.DoubleSide,
      envMapIntensity: 0.6,
    });

    this.mesh = new THREE.Mesh(geometry, this.material);
    this.mesh.castShadow = false;
    this.mesh.receiveShadow = false;
    this.scene.add(this.mesh);
    this.setRenderMode(this.renderMode);

    this.buildEdgeLines(decoded);
    this.buildPickLines(decoded);
    this.buildLighting();
    this.buildGrid();
    this.applyVisibility();

    if (options.fit !== false) this.setView(this.view);
    this.applyClipping();
    this.callbacks.onGeometryLoaded?.({
      bounds: this.bounds.clone(),
      triangles: decoded.header.triangleCount,
    });
  }

  private buildMeshEdgeSegments(decoded: DecodedJhb): number[] {
    if (!decoded.positions.length || !decoded.indices.length) return [];
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(decoded.positions, 3));
    geometry.setIndex(new THREE.BufferAttribute(decoded.indices, 1));
    try {
      const edges = new THREE.EdgesGeometry(geometry, 18);
      const position = edges.getAttribute('position');
      const segments = position ? Array.from(position.array as ArrayLike<number>) : [];
      edges.dispose();
      return segments;
    } catch {
      return [];
    } finally {
      geometry.dispose();
    }
  }

  private buildEdgeLines(decoded: DecodedJhb) {
    const segments: number[] = [];
    const segmentPolyline: number[] = [];
    const segmentShape: number[] = [];
    const meshSegments = this.buildMeshEdgeSegments(decoded);
    const displaySegments = meshSegments.length ? meshSegments : segments;
    const offsets = decoded.edgeOffsets;
    for (let e = 0; e < offsets.length - 1; e += 1) {
      const start = offsets[e];
      const end = offsets[e + 1];
      const closed = Boolean(decoded.header.edges?.[e]?.closed);
      const shape = decoded.header.edgeToShape?.[e] ?? 0;
      for (let i = start; i < end - 1; i += 1) {
        segments.push(
          decoded.edges[i * 3],
          decoded.edges[i * 3 + 1],
          decoded.edges[i * 3 + 2],
          decoded.edges[(i + 1) * 3],
          decoded.edges[(i + 1) * 3 + 1],
          decoded.edges[(i + 1) * 3 + 2],
        );
        segmentPolyline.push(e);
        segmentShape.push(shape);
      }
      if (closed && end - start > 2) {
        const last = end - 1;
        segments.push(
          decoded.edges[last * 3],
          decoded.edges[last * 3 + 1],
          decoded.edges[last * 3 + 2],
          decoded.edges[start * 3],
          decoded.edges[start * 3 + 1],
          decoded.edges[start * 3 + 2],
        );
        segmentPolyline.push(e);
        segmentShape.push(shape);
      }
    }
    if (!this.state) return;
    if (meshSegments.length) {
      this.state.segments = displaySegments;
      this.state.segmentPolyline = Array.from({ length: displaySegments.length / 6 }, (_, index) => index);
      this.state.segmentShape = Array.from({ length: displaySegments.length / 6 }, () => 0);
    } else {
      this.state.segments = segments;
      this.state.segmentPolyline = segmentPolyline;
      this.state.segmentShape = segmentShape;
    }
    if (!displaySegments.length) return;

    const lineGeometry = new LineSegmentsGeometry();
    lineGeometry.setPositions(displaySegments);
    const material = new LineMaterial({
      color:
        this.renderMode === 'wireframe'
          ? EDGE_WIREFRAME
          : this.renderMode === 'xray'
            ? EDGE_XRAY
            : EDGE_BLACK,
      linewidth: 1.7,
      worldUnits: false,
      dashed: false,
      alphaToCoverage: false,
      resolution: this.resolutionVector(),
    });
    material.depthTest = false;
    material.transparent = true;
    this.edgeMaterial = material;
    this.edgeLines = new LineSegments2(lineGeometry, material);
    this.edgeLines.renderOrder = 2;
    this.scene.add(this.edgeLines);
  }

  private buildPickLines(decoded: DecodedJhb) {
    if (!this.state) return;
    const geometry = new THREE.BufferGeometry();
    const positions = new Float32Array(this.state.segments);
    geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    const material = new THREE.LineBasicMaterial({ color: 0xffffff });
    this.pickLines = new THREE.LineSegments(geometry, material);
    this.pickLines.visible = false;
    this.pickLines.frustumCulled = false;
    this.scene.add(this.pickLines);
  }

  private buildLighting() {
    const center = this.bounds.getCenter(new THREE.Vector3());
    const key = this.lights[1] as THREE.DirectionalLight;
    const fill = this.lights[2] as THREE.DirectionalLight;
    const rim = this.lights[3] as THREE.DirectionalLight;
    const top = this.lights[4] as THREE.DirectionalLight;
    key.position.copy(center).add(new THREE.Vector3(1.4, 1.1, 1.8).multiplyScalar(this.radius * 2.4));
    fill.position.copy(center).add(new THREE.Vector3(-1.6, -0.8, 0.6).multiplyScalar(this.radius * 2.4));
    rim.position.copy(center).add(new THREE.Vector3(0, 1.8, -1.4).multiplyScalar(this.radius * 1.6));
    top.position.copy(center).add(new THREE.Vector3(0, 0, 1).multiplyScalar(this.radius * 3.2));
    for (const light of this.lights) {
      if (light instanceof THREE.DirectionalLight) {
        light.target.position.copy(center);
        this.scene.add(light.target);
      }
    }
  }

  private buildGrid() {
    if (this.grid) {
      this.scene.remove(this.grid);
      this.grid.geometry.dispose();
      (this.grid.material as THREE.Material).dispose();
      this.grid = null;
    }
    const size = Math.max(this.radius * 3.2, 1000);
    const grid = new THREE.GridHelper(size, 24, 0x46647c, 0x2a4054);
    grid.rotation.x = Math.PI / 2;
    grid.position.set(0, 0, this.bounds.min.z - this.radius * 0.004);
    const material = grid.material as THREE.Material;
    material.transparent = true;
    material.opacity = 0.42;
    this.grid = grid;
    this.scene.add(grid);
  }

  // --------------------------------------------------------------- clipping
  setClippingAxis(axis: ClipAxis | null) {
    this.clipAxis = axis;
    this.applyClipping();
  }

  setClippingRatio(ratio: number) {
    this.clipRatio = Math.min(0.98, Math.max(0.02, ratio));
    this.applyClipping();
  }

  getClippingRatio() {
    return this.clipRatio;
  }

  private applyClipping() {
    this.clearClipping();
    if (!this.clipAxis || !this.mesh || !this.state) return;
    const axisIndex = this.clipAxis === 'x' ? 0 : this.clipAxis === 'y' ? 1 : 2;
    const n = new THREE.Vector3(axisIndex === 0 ? 1 : 0, axisIndex === 1 ? 1 : 0, axisIndex === 2 ? 1 : 0);
    const min = this.bounds.min[this.clipAxis];
    const max = this.bounds.max[this.clipAxis];
    const value = min + (max - min) * this.clipRatio;

    // 法向取负轴：保留 value 一侧，裁掉外侧
    const plane = new THREE.Plane(n.clone().negate(), value);
    this.clipPlane = plane;

    if (this.material) {
      this.material.clippingPlanes = [plane];
      this.material.needsUpdate = true;
    }
    if (this.edgeMaterial) {
      this.edgeMaterial.clippingPlanes = [plane];
      this.edgeMaterial.needsUpdate = true;
    }

    const capGroup = this.buildSectionCap(this.state.decoded, axisIndex, value);
    if (capGroup) {
      this.clippingGroup = capGroup;
      this.capMesh = capGroup.children.find((child) => (child as THREE.Mesh).isMesh) as THREE.Mesh ?? null;
      this.scene.add(capGroup);
    }
  }

  /**
   * 由三角网格与剖切平面求交，重建截面多边形并三角化。
   * 相比模板缓冲方案，可稳定处理任意截面并避免遮挡伪影。
   */
  private buildSectionCap(decoded: DecodedJhb, axisIndex: number, value: number): THREE.Group | null {
    const positions = decoded.positions;
    const indices = decoded.indices;
    const eps = Math.max(this.radius * 1e-6, 1e-6);
    const tol = Math.max(this.radius * 1e-5, 1e-4);
    const segmentPairs: [THREE.Vector3, THREE.Vector3][] = [];

    for (let t = 0; t < indices.length; t += 3) {
      const vertexIndices = [indices[t], indices[t + 1], indices[t + 2]];
      const distances = vertexIndices.map((v) => positions[v * 3 + axisIndex] - value);
      if (distances.every((d) => d > eps) || distances.every((d) => d < -eps)) continue;
      if (distances.every((d) => Math.abs(d) <= eps)) continue;
      const points: THREE.Vector3[] = [];
      for (let e = 0; e < 3; e += 1) {
        const a = vertexIndices[e];
        const b = vertexIndices[(e + 1) % 3];
        const da = distances[e];
        const db = distances[(e + 1) % 3];
        const pa = new THREE.Vector3(positions[a * 3], positions[a * 3 + 1], positions[a * 3 + 2]);
        const pb = new THREE.Vector3(positions[b * 3], positions[b * 3 + 1], positions[b * 3 + 2]);
        if (Math.abs(da) <= eps) points.push(pa);
        else if (Math.abs(db) <= eps) points.push(pb);
        else if ((da < 0 && db > 0) || (da > 0 && db < 0)) {
          const ratio = da / (da - db);
          points.push(pa.clone().lerp(pb, ratio));
        }
      }
      const unique: THREE.Vector3[] = [];
      for (const point of points) {
        if (!unique.some((existing) => existing.distanceTo(point) < tol)) unique.push(point);
      }
      if (unique.length >= 2) segmentPairs.push([unique[0], unique[1]]);
    }
    if (!segmentPairs.length) return null;

    const keyOf = (point: THREE.Vector3) =>
      [point.x, point.y, point.z].map((v) => Math.round(v / tol)).join(',');
    const pointMap = new Map<string, THREE.Vector3>();
    const edges: { a: string; b: string; used: boolean }[] = [];
    const adjacency = new Map<string, number[]>();
    for (const [a, b] of segmentPairs) {
      const ka = keyOf(a);
      const kb = keyOf(b);
      if (ka === kb) continue;
      pointMap.set(ka, a);
      pointMap.set(kb, b);
      const id = edges.length;
      edges.push({ a: ka, b: kb, used: false });
      if (!adjacency.has(ka)) adjacency.set(ka, []);
      if (!adjacency.has(kb)) adjacency.set(kb, []);
      adjacency.get(ka)!.push(id);
      adjacency.get(kb)!.push(id);
    }

    const loops: THREE.Vector3[][] = [];
    for (let startEdge = 0; startEdge < edges.length; startEdge += 1) {
      if (edges[startEdge].used) continue;
      const startKey = edges[startEdge].a;
      const loop: THREE.Vector3[] = [pointMap.get(startKey)!];
      let current = startKey;
      let guard = 0;
      let closed = false;
      while (guard < 200000) {
        guard += 1;
        const candidates = adjacency.get(current) ?? [];
        const nextEdgeId = candidates.find((id) => !edges[id].used);
        if (nextEdgeId === undefined) break;
        const edge = edges[nextEdgeId];
        edge.used = true;
        const next = edge.a === current ? edge.b : edge.a;
        if (next === startKey) {
          closed = true;
          break;
        }
        loop.push(pointMap.get(next)!);
        current = next;
      }
      if (closed && loop.length >= 3) loops.push(loop);
    }
    if (!loops.length) return null;

    const to2D = (point: THREE.Vector3): [number, number] =>
      axisIndex === 0 ? [point.y, point.z] : axisIndex === 1 ? [point.x, point.z] : [point.x, point.y];
    const signedArea = (loop: THREE.Vector3[]) => {
      const uv = loop.map(to2D);
      let area = 0;
      for (let i = 0; i < uv.length; i += 1) {
        const [x1, y1] = uv[i];
        const [x2, y2] = uv[(i + 1) % uv.length];
        area += x1 * y2 - x2 * y1;
      }
      return area / 2;
    };

    const sorted = [...loops].sort((a, b) => Math.abs(signedArea(b)) - Math.abs(signedArea(a)));
    const outer = signedArea(sorted[0]) < 0 ? [...sorted[0]].reverse() : sorted[0];
    const holes = sorted
      .slice(1)
      .filter((loop) => Math.abs(signedArea(loop)) > tol * tol)
      .map((loop) => (signedArea(loop) > 0 ? [...loop].reverse() : loop));

    // 使用 three 内置 Earcut（ShapeUtils.triangulateShape）统一处理任意数量内孔，
    // 避免自研耳切在双室箱梁、多减重孔等截面上的分支缺失。
    const contour2D = outer.map((point) => new THREE.Vector2(...to2D(point)));
    const holes2D = holes.map((loop) => loop.map((point) => new THREE.Vector2(...to2D(point))));
    let faces: [number, number, number][];
    try {
      faces = THREE.ShapeUtils.triangulateShape(contour2D, holes2D) as [number, number, number][];
    } catch {
      return null;
    }
    if (!faces.length) return null;

    const normal = new THREE.Vector3(axisIndex === 0 ? 1 : 0, axisIndex === 1 ? 1 : 0, axisIndex === 2 ? 1 : 0);
    const sectionVertices = [...outer, ...holes.flat()];
    const capPositions: number[] = [];
    const capNormals: number[] = [];
    for (const [a, b, c] of faces) {
      for (const index of [a, b, c]) {
        const point = sectionVertices[index];
        if (!point) continue;
        capPositions.push(point.x, point.y, point.z);
        capNormals.push(normal.x, normal.y, normal.z);
      }
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(capPositions), 3));
    geometry.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(capNormals), 3));
    const material = new THREE.MeshStandardMaterial({
      color: CAP_COLOR,
      metalness: 0.03,
      roughness: 0.92,
      side: THREE.DoubleSide,
      polygonOffset: true,
      polygonOffsetFactor: -2,
      polygonOffsetUnits: -2,
    });
    const cap = new THREE.Mesh(geometry, material);
    cap.renderOrder = 4;

    const group = new THREE.Group();
    group.add(cap);

    const contour: number[] = [];
    for (const loop of loops) {
      for (let i = 0; i < loop.length; i += 1) {
        const a = loop[i];
        const b = loop[(i + 1) % loop.length];
        contour.push(a.x, a.y, a.z, b.x, b.y, b.z);
      }
    }
    if (contour.length) {
      const contourGeometry = new LineSegmentsGeometry();
      contourGeometry.setPositions(contour);
      const contourMaterial = new LineMaterial({
        color: 0x111827,
        linewidth: 1.35,
        worldUnits: false,
        resolution: this.resolutionVector(),
      });
      const lines = new LineSegments2(contourGeometry, contourMaterial);
      lines.renderOrder = 5;
      group.add(lines);
    }
    return group;
  }

  private clearClipping() {
    if (this.material) {
      this.material.clippingPlanes = [];
      this.material.needsUpdate = true;
    }
    if (this.edgeMaterial) {
      this.edgeMaterial.clippingPlanes = [];
      this.edgeMaterial.needsUpdate = true;
    }
    if (this.clippingGroup) {
      this.scene.remove(this.clippingGroup);
      this.clippingGroup.traverse((object) => {
        const renderable = object as THREE.Mesh;
        renderable.geometry?.dispose();
        const material = renderable.material as THREE.Material | THREE.Material[] | undefined;
        if (material) {
          if (Array.isArray(material)) material.forEach((m) => m.dispose());
          else material.dispose();
        }
      });
      this.clippingGroup = null;
      this.capMesh = null;
    }
    this.clipPlane = null;
  }

  // ------------------------------------------------------------- visibility
  setShapeHidden(shapeIndex: number, hidden: boolean) {
    if (hidden) this.hiddenShapes.add(shapeIndex);
    else this.hiddenShapes.delete(shapeIndex);
    this.applyVisibility();
  }

  setAllShapesVisible() {
    this.hiddenShapes.clear();
    this.applyVisibility();
  }

  isShapeHidden(shapeIndex: number) {
    return this.hiddenShapes.has(shapeIndex);
  }

  private applyVisibility() {
    if (!this.mesh || !this.state) return;
    const { decoded, faceToShape } = this.state;
    const index = decoded.indices;
    const faceIds = decoded.faceIds;
    if (!this.hiddenShapes.size) {
      this.visibleTriangleMap = null;
      (this.mesh.geometry as THREE.BufferGeometry).setIndex(new THREE.BufferAttribute(index, 1));
    } else {
      const kept: number[] = [];
      const mapping: number[] = [];
      for (let t = 0; t < index.length / 3; t += 1) {
        const faceId = faceIds[t];
        const shapeIndex = faceToShape[faceId] ?? 0;
        if (this.hiddenShapes.has(shapeIndex)) continue;
        kept.push(index[t * 3], index[t * 3 + 1], index[t * 3 + 2]);
        mapping.push(t);
      }
      this.visibleTriangleMap = mapping;
      (this.mesh.geometry as THREE.BufferGeometry).setIndex(new THREE.BufferAttribute(new Uint32Array(kept), 1));
    }

    if (this.edgeLines && this.state.segments.length) {
      const kept: number[] = [];
      const segments = this.state.segments;
      for (let s = 0; s < this.state.segmentShape.length; s += 1) {
        if (this.hiddenShapes.has(this.state.segmentShape[s])) continue;
        kept.push(...segments.slice(s * 6, s * 6 + 6));
      }
      const geometry = new LineSegmentsGeometry();
      geometry.setPositions(kept);
      this.edgeLines.geometry.dispose();
      this.edgeLines.geometry = geometry;
    }
  }

  // ---------------------------------------------------------------- picking
  private updatePointer(event: PointerEvent) {
    const rect = this.renderer.domElement.getBoundingClientRect();
    this.pointer.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    this.pointer.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;
  }

  private raycast(event: PointerEvent) {
    this.updatePointer(event);
    this.raycaster.setFromCamera(this.pointer, this.camera);
    this.raycaster.params.Line = { threshold: this.radius * 0.008 };
    const meshHits = this.mesh ? this.raycaster.intersectObject(this.mesh, false) : [];
    const lineHits = this.pickLines ? this.raycaster.intersectObject(this.pickLines, false) : [];
    const lineHit = lineHits.find((hit) => {
      const segmentIndex = Math.floor((typeof hit.index === 'number' ? hit.index : 0) / 2);
      const polyline = this.state?.segmentPolyline[segmentIndex];
      if (polyline === undefined) return false;
      return !this.hiddenShapes.has(this.state?.edgeToShape[polyline] ?? 0);
    });
    return { meshHit: meshHits[0], lineHit };
  }

  private handlePointerDown = (event: PointerEvent) => {
    this.pointerDown = { x: event.clientX, y: event.clientY, time: performance.now() };
  };

  private handlePointerUp = (event: PointerEvent) => {
    const moved = Math.hypot(event.clientX - this.pointerDown.x, event.clientY - this.pointerDown.y);
    if (moved > 5 || event.button !== 0) return;
    if (this.measureMode) {
      this.measureAt(event);
      return;
    }
    this.pickAt(event);
  };

  private pickAt(event: PointerEvent) {
    if (!this.state) return;
    const { meshHit, lineHit } = this.raycast(event);
    const edgeThreshold = this.radius * 0.012;
    if (lineHit && (!meshHit || lineHit.distance <= meshHit.distance + edgeThreshold)) {
      const vertexIndex = typeof lineHit.index === 'number' ? lineHit.index : 0;
      const segmentIndex = Math.floor(vertexIndex / 2);
      const polyline = this.state.segmentPolyline[segmentIndex];
      const shapeIndex = this.state.edgeToShape[polyline] ?? 0;
      this.callbacks.onPick?.({
        kind: 'edge',
        shapeIndex,
        shapeKey: this.state.shapeKeys[shapeIndex] ?? `shape-${shapeIndex}`,
        edgeIndex: polyline,
        point: [lineHit.point.x, lineHit.point.y, lineHit.point.z],
      });
      return;
    }
    if (!meshHit || !meshHit.face) {
      this.callbacks.onPick?.(null);
      return;
    }
    const localTriangle = Math.floor(meshHit.faceIndex ?? 0);
    const triangleIndex = this.visibleTriangleMap
      ? (this.visibleTriangleMap[localTriangle] ?? localTriangle)
      : localTriangle;
    const faceId = this.state.decoded.faceIds[triangleIndex] ?? 0;
    const shapeIndex = this.state.faceToShape[faceId] ?? 0;
    this.callbacks.onPick?.({
      kind: 'face',
      shapeIndex,
      shapeKey: this.state.shapeKeys[shapeIndex] ?? `shape-${shapeIndex}`,
      faceId,
      point: [meshHit.point.x, meshHit.point.y, meshHit.point.z],
    });
  }

  private handlePointerMove = (event: PointerEvent) => {
    if (!this.mesh) return;
    if (this.hoverTimer) return;
    this.hoverTimer = setTimeout(() => {
      this.hoverTimer = null;
    }, 90);
    const { meshHit, lineHit } = this.raycast(event);
    this.renderer.domElement.style.cursor = this.measureMode
      ? 'crosshair'
      : meshHit || lineHit
        ? 'pointer'
        : 'grab';
    if (this.measureMode) {
      if (meshHit) {
        const snapped = this.snapPoint(meshHit);
        this.updateSnapMarker(snapped.point);
      } else {
        this.updateSnapMarker(null);
      }
    } else {
      this.updateSnapMarker(null);
      const localTriangle = meshHit?.faceIndex;
      const globalTriangle =
        typeof localTriangle === 'number'
          ? this.visibleTriangleMap
            ? (this.visibleTriangleMap[localTriangle] ?? null)
            : localTriangle
          : null;
      this.updateHover(globalTriangle);
    }
  };

  private updateSnapMarker(point: THREE.Vector3 | null) {
    if (this.snapMarker) {
      this.scene.remove(this.snapMarker);
      this.snapMarker.geometry.dispose();
      (this.snapMarker.material as THREE.Material).dispose();
      this.snapMarker = null;
    }
    if (!point) return;
    const geometry = new THREE.SphereGeometry(this.radius * 0.014, 12, 10);
    const material = new THREE.MeshBasicMaterial({
      color: HIGHLIGHT,
      transparent: true,
      opacity: 0.7,
      depthTest: false,
    });
    const marker = new THREE.Mesh(geometry, material);
    marker.position.copy(point);
    marker.renderOrder = 9;
    this.snapMarker = marker;
    this.scene.add(marker);
  }

  private handlePointerLeave = () => {
    this.updateSnapMarker(null);
    this.updateHover(null);
  };

  private updateHover(triangleIndex: number | null) {
    if (!this.state || !this.mesh) return;
    if (this.hoverMesh) {
      this.scene.remove(this.hoverMesh);
      this.hoverMesh.geometry.dispose();
      (this.hoverMesh.material as THREE.Material).dispose();
      this.hoverMesh = null;
    }
    if (triangleIndex === null) return;
    const { decoded } = this.state;
    const positions = decoded.positions;
    const offset = triangleIndex * 3;
    const array = new Float32Array(9);
    for (let i = 0; i < 3; i += 1) {
      const vertex = decoded.indices[offset + i] * 3;
      array[i * 3] = positions[vertex];
      array[i * 3 + 1] = positions[vertex + 1];
      array[i * 3 + 2] = positions[vertex + 2];
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(array, 3));
    const material = new THREE.MeshBasicMaterial({
      color: HIGHLIGHT,
      transparent: true,
      opacity: 0.22,
      depthWrite: false,
      side: THREE.DoubleSide,
    });
    this.hoverMesh = new THREE.Mesh(geometry, material);
    this.hoverMesh.renderOrder = 4;
    this.scene.add(this.hoverMesh);
  }

  // -------------------------------------------------------------- highlight
  highlightShape(shapeIndex: number | null) {
    this.currentShape = shapeIndex;
    this.clearHighlight();
    if (shapeIndex === null || !this.state) return;
    const { decoded } = this.state;
    const positions = decoded.positions;
    const normals = decoded.normals;

    const faceVertices: number[] = [];
    const faceNormals: number[] = [];
    for (let t = 0; t < decoded.indices.length / 3; t += 1) {
      const faceId = decoded.faceIds[t] ?? 0;
      if ((this.state.faceToShape[faceId] ?? 0) !== shapeIndex) continue;
      for (let k = 0; k < 3; k += 1) {
        const v = decoded.indices[t * 3 + k] * 3;
        faceVertices.push(positions[v], positions[v + 1], positions[v + 2]);
        faceNormals.push(normals[v], normals[v + 1], normals[v + 2]);
      }
    }
    if (faceVertices.length) {
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(faceVertices), 3));
      geometry.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(faceNormals), 3));
      const material = new THREE.MeshStandardMaterial({
        color: HIGHLIGHT,
        emissive: 0x0b6f9e,
        emissiveIntensity: 0.35,
        transparent: true,
        opacity: 0.4,
        roughness: 0.5,
        metalness: 0.05,
        depthWrite: false,
        polygonOffset: true,
        polygonOffsetFactor: -1,
        polygonOffsetUnits: -1,
      });
      const mesh = new THREE.Mesh(geometry, material);
      mesh.renderOrder = 5;
      this.highlightMesh = mesh;
      this.scene.add(mesh);
    }

    const edgeSegments: number[] = [];
    for (let s = 0; s < this.state.segmentShape.length; s += 1) {
      if (this.state.segmentShape[s] !== shapeIndex) continue;
      edgeSegments.push(...this.state.segments.slice(s * 6, s * 6 + 6));
    }
    if (edgeSegments.length) {
      const geometry = new LineSegmentsGeometry();
      geometry.setPositions(edgeSegments);
      const material = new LineMaterial({
        color: HIGHLIGHT,
        linewidth: 2.6,
        worldUnits: false,
        resolution: this.resolutionVector(),
      });
      const lines = new LineSegments2(geometry, material);
      lines.renderOrder = 6;
      this.highlightEdges = lines;
      this.scene.add(lines);
    }
  }

  private clearHighlight() {
    if (this.highlightMesh) {
      this.scene.remove(this.highlightMesh);
      this.highlightMesh.geometry.dispose();
      (this.highlightMesh.material as THREE.Material).dispose();
      this.highlightMesh = null;
    }
    if (this.highlightEdges) {
      this.scene.remove(this.highlightEdges);
      this.highlightEdges.geometry.dispose();
      (this.highlightEdges.material as THREE.Material).dispose();
      this.highlightEdges = null;
    }
  }

  // --------------------------------------------------------------- compare
  /** 审核双模型重叠对比：新版实体 + 旧版半透明灰度，变更部位按语义色高亮 */
  setCompareGeometry(
    primary: DecodedJhb,
    secondary: DecodedJhb,
    changes: { shapeKey: string; kind: 'added' | 'removed' | 'modified' }[] = [],
  ) {
    const view = this.view;
    this.setGeometry(primary, { fit: false });
    this.clearCompare();
    this.compareDecoded = secondary;

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.BufferAttribute(secondary.positions, 3));
    geometry.setAttribute('normal', new THREE.BufferAttribute(secondary.normals, 3));
    geometry.setIndex(new THREE.BufferAttribute(secondary.indices, 1));
    const material = new THREE.MeshStandardMaterial({
      color: 0x8fa3b5,
      transparent: true,
      opacity: 0.3,
      roughness: 0.92,
      metalness: 0.04,
      depthWrite: false,
      side: THREE.DoubleSide,
    });
    const mesh = new THREE.Mesh(geometry, material);
    mesh.renderOrder = 0;
    this.compareGroup.add(mesh);

    const segments = this.segmentsFor(secondary);
    if (segments.length) {
      const edgeGeometry = new LineSegmentsGeometry();
      edgeGeometry.setPositions(segments);
      const edgeMaterial = new LineMaterial({
        color: 0x64748b,
        linewidth: 1.1,
        worldUnits: false,
        transparent: true,
        opacity: 0.55,
        resolution: this.resolutionVector(),
      });
      this.compareEdges = new LineSegments2(edgeGeometry, edgeMaterial);
      this.compareEdges.renderOrder = 1;
      this.compareGroup.add(this.compareEdges);
    }

    const primaryKeys = this.state?.shapeKeys ?? [];
    for (const change of changes) {
      const color = change.kind === 'added' ? 0x22c55e : change.kind === 'removed' ? 0xef4444 : 0xf59e0b;
      if (change.kind === 'removed') {
        const index = (secondary.header.shapeKeys ?? []).indexOf(change.shapeKey);
        if (index >= 0) this.addShapeOverlay(secondary, index, color, 0.55);
      } else {
        const index = primaryKeys.indexOf(change.shapeKey);
        if (index >= 0) this.addShapeOverlay(primary, index, color, 0.5);
      }
    }

    const secondaryBox = new THREE.Box3(
      new THREE.Vector3(...secondary.header.bounds.min),
      new THREE.Vector3(...secondary.header.bounds.max),
    );
    this.bounds = this.bounds.clone().union(secondaryBox);
    const size = this.bounds.getSize(new THREE.Vector3());
    this.radius = Math.max(size.length() / 2, 1);
    this.buildGrid();
    this.buildLighting();
    this.setView(view);
  }

  clearCompare() {
    for (const child of [...this.compareGroup.children]) {
      this.compareGroup.remove(child);
      const mesh = child as THREE.Mesh;
      mesh.geometry?.dispose();
      const material = mesh.material as THREE.Material | THREE.Material[];
      if (Array.isArray(material)) material.forEach((m) => m.dispose());
      else material?.dispose();
    }
    this.compareEdges = null;
    this.compareDecoded = null;
  }

  getCompareDecoded() {
    return this.compareDecoded;
  }

  private segmentsFor(decoded: DecodedJhb): number[] {
    const segments: number[] = [];
    const offsets = decoded.edgeOffsets;
    for (let e = 0; e < offsets.length - 1; e += 1) {
      const start = offsets[e];
      const end = offsets[e + 1];
      const closed = Boolean(decoded.header.edges?.[e]?.closed);
      for (let i = start; i < end - 1; i += 1) {
        segments.push(
          decoded.edges[i * 3],
          decoded.edges[i * 3 + 1],
          decoded.edges[i * 3 + 2],
          decoded.edges[(i + 1) * 3],
          decoded.edges[(i + 1) * 3 + 1],
          decoded.edges[(i + 1) * 3 + 2],
        );
      }
      if (closed && end - start > 2) {
        const last = end - 1;
        segments.push(
          decoded.edges[last * 3],
          decoded.edges[last * 3 + 1],
          decoded.edges[last * 3 + 2],
          decoded.edges[start * 3],
          decoded.edges[start * 3 + 1],
          decoded.edges[start * 3 + 2],
        );
      }
    }
    return segments;
  }

  private addShapeOverlay(decoded: DecodedJhb, shapeIndex: number, color: number, opacity: number) {
    const faceToShape = decoded.header.faceToShape ?? [];
    const vertices: number[] = [];
    const normals: number[] = [];
    for (let t = 0; t < decoded.indices.length / 3; t += 1) {
      const faceId = decoded.faceIds[t] ?? 0;
      if ((faceToShape[faceId] ?? 0) !== shapeIndex) continue;
      for (let k = 0; k < 3; k += 1) {
        const v = decoded.indices[t * 3 + k] * 3;
        vertices.push(decoded.positions[v], decoded.positions[v + 1], decoded.positions[v + 2]);
        normals.push(decoded.normals[v], decoded.normals[v + 1], decoded.normals[v + 2]);
      }
    }
    if (vertices.length) {
      const geometry = new THREE.BufferGeometry();
      geometry.setAttribute('position', new THREE.BufferAttribute(new Float32Array(vertices), 3));
      geometry.setAttribute('normal', new THREE.BufferAttribute(new Float32Array(normals), 3));
      const material = new THREE.MeshStandardMaterial({
        color,
        emissive: color,
        emissiveIntensity: 0.25,
        transparent: true,
        opacity,
        roughness: 0.6,
        metalness: 0.05,
        depthWrite: false,
        polygonOffset: true,
        polygonOffsetFactor: -1,
        polygonOffsetUnits: -1,
      });
      const overlay = new THREE.Mesh(geometry, material);
      overlay.renderOrder = 7;
      this.compareGroup.add(overlay);
    }

    const segments: number[] = [];
    const edgeToShape = decoded.header.edgeToShape ?? [];
    const offsets = decoded.edgeOffsets;
    for (let e = 0; e < offsets.length - 1; e += 1) {
      if ((edgeToShape[e] ?? 0) !== shapeIndex) continue;
      const start = offsets[e];
      const end = offsets[e + 1];
      const closed = Boolean(decoded.header.edges?.[e]?.closed);
      for (let i = start; i < end - 1; i += 1) {
        segments.push(
          decoded.edges[i * 3],
          decoded.edges[i * 3 + 1],
          decoded.edges[i * 3 + 2],
          decoded.edges[(i + 1) * 3],
          decoded.edges[(i + 1) * 3 + 1],
          decoded.edges[(i + 1) * 3 + 2],
        );
      }
      if (closed && end - start > 2) {
        const last = end - 1;
        segments.push(
          decoded.edges[last * 3],
          decoded.edges[last * 3 + 1],
          decoded.edges[last * 3 + 2],
          decoded.edges[start * 3],
          decoded.edges[start * 3 + 1],
          decoded.edges[start * 3 + 2],
        );
      }
    }
    if (segments.length) {
      const edgeGeometry = new LineSegmentsGeometry();
      edgeGeometry.setPositions(segments);
      const edgeMaterial = new LineMaterial({
        color,
        linewidth: 2.3,
        worldUnits: false,
        resolution: this.resolutionVector(),
      });
      const lines = new LineSegments2(edgeGeometry, edgeMaterial);
      lines.renderOrder = 8;
      this.compareGroup.add(lines);
    }
  }

  // ------------------------------------------------------------ measurement
  setMeasureMode(enabled: boolean) {
    this.measureMode = enabled;
    this.renderer.domElement.style.cursor = enabled ? 'crosshair' : 'grab';
    if (!enabled) {
      this.measurePoints = [];
      this.updateSnapMarker(null);
    }
  }

  private measureAt(event: PointerEvent) {
    this.updatePointer(event);
    this.raycaster.setFromCamera(this.pointer, this.camera);
    if (!this.mesh) return;
    const hits = this.raycaster.intersectObject(this.mesh, false);
    const hit = hits[0];
    if (!hit) return;
    const snapped = this.snapPoint(hit);
    const point = snapped.point;
    if (this.measurePoints.length >= 2) {
      this.measurePoints = [];
      this.measureLabels = [];
      this.clearMeasurementObjects();
    }
    this.measurePoints.push(point.clone());
    this.measureLabels.push(
      this.measurePoints.length === 1 ? 'A' : 'B',
    );
    this.addMeasureMarker(point, snapped.label);

    if (this.measurePoints.length === 2) {
      const [a, b] = this.measurePoints;
      const distance = a.distanceTo(b);
      const horizontal = Math.hypot(b.x - a.x, b.y - a.y);
      const deltaZ = b.z - a.z;
      const segments = [a.x, a.y, a.z, b.x, b.y, b.z];
      const geometry = new LineSegmentsGeometry();
      geometry.setPositions(segments);
      const material = new LineMaterial({
        color: HIGHLIGHT,
        linewidth: 2.2,
        worldUnits: false,
        resolution: this.resolutionVector(),
      });
      const line = new LineSegments2(geometry, material);
      line.renderOrder = 7;
      this.measureGroup.add(line);
      this.callbacks.onMeasure?.({
        a: [a.x, a.y, a.z],
        b: [b.x, b.y, b.z],
        distance,
        horizontal,
        deltaZ,
      });
    }
  }

  private snapPoint(hit: THREE.Intersection): { point: THREE.Vector3; label?: string } {
    if (!this.state || !this.mesh) return { point: hit.point.clone() };
    const { decoded } = this.state;
    const candidates: { point: THREE.Vector3; label?: string }[] = [];
    const seen = new Set<string>();
    const addCandidate = (x: number, y: number, z: number, label?: string) => {
      const key = `${x.toFixed(2)},${y.toFixed(2)},${z.toFixed(2)}`;
      if (seen.has(key)) return;
      seen.add(key);
      candidates.push({ point: new THREE.Vector3(x, y, z), label });
    };

    // 特征边/轮廓线顶点才是工程测量真正需要的捕捉点。
    // 三角网格顶点在曲面上非常密集，不能替代特征点。
    const edges = decoded.edges;
    for (let i = 0; i + 2 < edges.length; i += 3) {
      addCandidate(edges[i], edges[i + 1], edges[i + 2]);
    }
    // 接合点/锚点优先级最高，用于跨构件定位。
    const anchors = decoded.header.anchors ?? {};
    for (const [name, anchor] of Object.entries(anchors)) {
      addCandidate(anchor.origin[0], anchor.origin[1], anchor.origin[2], name);
    }
    // 没有特征边时，退化为拾取三角形的三个顶点。
    if (!candidates.length && hit.face) {
      for (const vertexIndex of [hit.face.a, hit.face.b, hit.face.c]) {
        addCandidate(
          decoded.positions[vertexIndex * 3],
          decoded.positions[vertexIndex * 3 + 1],
          decoded.positions[vertexIndex * 3 + 2],
        );
      }
    }

    const canvas = this.renderer.domElement;
    const width = Math.max(canvas.clientWidth, 1);
    const height = Math.max(canvas.clientHeight, 1);
    const pointerX = (this.pointer.x * 0.5 + 0.5) * width;
    const pointerY = (-this.pointer.y * 0.5 + 0.5) * height;
    const hitDistance = hit.point.distanceTo(this.camera.position);
    const behindTolerance = this.radius * 0.12;
    const snapPixels = 18;
    let best = hit.point.clone();
    let bestLabel: string | undefined;
    let bestPixelDistance = snapPixels;

    for (const candidate of candidates) {
      // 不吸附被实体遮挡的背面特征点。
      if (candidate.point.distanceTo(this.camera.position) > hitDistance + behindTolerance) continue;
      const projected = candidate.point.clone().project(this.camera);
      if (projected.z < -1 || projected.z > 1) continue;
      const sx = (projected.x * 0.5 + 0.5) * width;
      const sy = (-projected.y * 0.5 + 0.5) * height;
      const pixelDistance = Math.hypot(sx - pointerX, sy - pointerY);
      if (pixelDistance < bestPixelDistance) {
        bestPixelDistance = pixelDistance;
        best = candidate.point.clone();
        bestLabel = candidate.label;
      }
    }
    return { point: best, label: bestLabel };
  }

  private addMeasureMarker(point: THREE.Vector3, label?: string) {
    const geometry = new THREE.SphereGeometry(this.radius * 0.012, 16, 12);
    const material = new THREE.MeshBasicMaterial({ color: HIGHLIGHT });
    const sphere = new THREE.Mesh(geometry, material);
    sphere.position.copy(point);
    sphere.renderOrder = 8;
    sphere.userData.measureLabel = label ?? '';
    this.measureGroup.add(sphere);
  }

  private clearMeasurementObjects() {
    for (const child of [...this.measureGroup.children]) {
      this.measureGroup.remove(child);
      const mesh = child as THREE.Mesh;
      mesh.geometry?.dispose();
      const material = mesh.material;
      if (Array.isArray(material)) material.forEach((m) => m.dispose());
      else material?.dispose();
    }
  }

  clearMeasurements() {
    this.measurePoints = [];
    this.measureLabels = [];
    this.updateSnapMarker(null);
    this.clearMeasurementObjects();
    this.callbacks.onMeasure?.(null);
  }

  // ------------------------------------------------------------------ views
  setView(view: ViewPreset) {
    this.view = view;
    const center = this.bounds.getCenter(new THREE.Vector3());
    const radius = Math.max(this.radius, 1);
    const directions: Record<ViewPreset, THREE.Vector3> = {
      iso: new THREE.Vector3(1, 1, 0.86).normalize(),
      front: new THREE.Vector3(0, -1, 0.0001).normalize(),
      side: new THREE.Vector3(1, 0, 0.0001).normalize(),
      top: new THREE.Vector3(0, 0.0001, 1).normalize(),
    };
    if (view === 'top') this.camera.up.set(0, 1, 0);
    else this.camera.up.set(0, 0, 1);

    const direction = directions[view];
    const distance = radius * (view === 'top' ? 4.3 : 3.9);
    this.camera.position.copy(center).addScaledVector(direction, distance);
    this.camera.near = Math.max(radius * 0.002, 0.05);
    this.camera.far = radius * 60;
    this.camera.updateProjectionMatrix();
    this.controls.target.copy(center);
    this.controls.minDistance = radius * 0.2;
    this.controls.maxDistance = radius * 16;
    this.controls.update();
    if (this.camera instanceof THREE.OrthographicCamera) this.updateOrthoFrustum();
  }

  setProjection(mode: ProjectionMode) {
    if (mode === this.projection) return;
    const target = this.controls.target.clone();
    const position = this.camera.position.clone();
    this.projection = mode;
    this.camera = this.createCamera(mode);
    this.camera.position.copy(position);
    this.camera.up.copy(mode === 'orthographic' && this.view === 'top' ? new THREE.Vector3(0, 1, 0) : new THREE.Vector3(0, 0, 1));
    this.controls.dispose();
    this.controls = this.createControls(this.camera);
    this.controls.target.copy(target);
    this.controls.update();
    this.resize();
    if (mode === 'orthographic') this.updateOrthoFrustum();
  }

  getProjection() {
    return this.projection;
  }

  getView() {
    return this.view;
  }

  private updateOrthoFrustum() {
    if (!(this.camera instanceof THREE.OrthographicCamera)) return;
    const aspect = this.container.clientWidth / Math.max(1, this.container.clientHeight);
    const frustum = this.radius * 1.15;
    this.camera.left = -frustum * aspect;
    this.camera.right = frustum * aspect;
    this.camera.top = frustum;
    this.camera.bottom = -frustum;
    this.camera.updateProjectionMatrix();
  }

  setEdgesVisible(visible: boolean) {
    this.edgesVisible = visible;
    if (this.edgeLines) this.edgeLines.visible = this.renderMode === 'wireframe' ? true : visible;
  }

  setRenderMode(mode: RenderMode) {
    this.renderMode = mode;
    const material = this.material;
    if (material) {
      // 工程线框只显示特征边，不显示三角剖分；避免曲面被密集三角形淹没
      material.visible = mode !== 'wireframe';
      material.wireframe = false;
      material.transparent = mode === 'xray';
      material.opacity = mode === 'xray' ? 0.18 : 1;
      material.depthWrite = mode !== 'xray';
      material.needsUpdate = true;
    }
    if (this.edgeMaterial) {
      this.edgeMaterial.color.set(
        mode === 'wireframe' ? EDGE_WIREFRAME : mode === 'xray' ? EDGE_XRAY : EDGE_BLACK,
      );
      this.edgeMaterial.needsUpdate = true;
    }
    if (this.edgeLines) this.edgeLines.visible = mode === 'wireframe' ? true : this.edgesVisible;
  }

  getRenderMode(): RenderMode {
    return this.renderMode;
  }

  setLineWidth(width: number) {
    if (this.edgeMaterial) {
      this.edgeMaterial.linewidth = width;
      this.edgeMaterial.needsUpdate = true;
    }
  }

  // ----------------------------------------------------------------- resize
  private resolutionVector() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    return new THREE.Vector2(
      Math.max(1, this.container.clientWidth) * dpr,
      Math.max(1, this.container.clientHeight) * dpr,
    );
  }

  resize() {
    const width = Math.max(1, this.container.clientWidth);
    const height = Math.max(1, this.container.clientHeight);
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    this.renderer.setSize(width, height, false);
    if (this.camera instanceof THREE.PerspectiveCamera) {
      this.camera.aspect = width / height;
    } else {
      const aspect = width / height;
      const frustum = this.radius * 1.15;
      this.camera.left = -frustum * aspect;
      this.camera.right = frustum * aspect;
      this.camera.top = frustum;
      this.camera.bottom = -frustum;
    }
    this.camera.updateProjectionMatrix();
    const resolution = this.resolutionVector();
    if (this.edgeMaterial) this.edgeMaterial.resolution.copy(resolution);
    if (this.highlightEdges) (this.highlightEdges.material as LineMaterial).resolution.copy(resolution);
  }

  private handleWindowResize = () => this.resize();

  // ------------------------------------------------------------ context lost
  private handleContextLost = (event: Event) => {
    event.preventDefault();
    this.contextLost = true;
    this.callbacks.onContextLost?.();
  };

  private handleContextRestored = () => {
    this.contextLost = false;
    this.callbacks.onContextRestored?.();
    if (this.state) {
      const decoded = this.state.decoded;
      this.setGeometry(decoded, { fit: false });
      this.setView(this.view);
    }
  };

  hardReinit() {
    const decoded = this.state?.decoded ?? null;
    const view = this.view;
    const axis = this.clipAxis;
    const ratio = this.clipRatio;
    if (this.edgeLines) {
      this.scene.remove(this.edgeLines);
      this.edgeLines.geometry.dispose();
      (this.edgeLines.material as THREE.Material).dispose();
      this.edgeLines = null;
    }
    this.clearHighlight();
    this.clearClipping();
    this.renderer.dispose();
    this.renderer.forceContextLoss?.();
    this.renderer.domElement.remove();
    this.controls.dispose();
    for (const light of this.lights) this.scene.remove(light);
    this.contextLost = false;
    this.initRenderer();
    this.initScene();
    if (decoded) {
      this.setGeometry(decoded, { fit: false });
      this.setView(view);
      if (axis) {
        this.clipAxis = axis;
        this.clipRatio = ratio;
        this.applyClipping();
      }
    }
  }

  private animate = () => {
    if (this.disposed) return;
    requestAnimationFrame(this.animate);
    if (!this.renderer || this.contextLost) return;
    this.controls.update();
    this.emitAxes();
    this.renderer.render(this.scene, this.camera);
  };

  private emitAxes() {
    if (!this.callbacks.onAxesChange) return;
    const inverse = this.camera.quaternion.clone().invert();
    const project = (x: number, y: number, z: number): [number, number] => {
      const v = new THREE.Vector3(x, y, z).applyQuaternion(inverse);
      return [Number(v.x.toFixed(3)), Number(v.y.toFixed(3))];
    };
    const axes = {
      x: project(1, 0, 0),
      y: project(0, 1, 0),
      z: project(0, 0, 1),
    };
    const key = `${axes.x.join(',')}|${axes.y.join(',')}|${axes.z.join(',')}`;
    if (key === this.lastAxesKey) return;
    this.lastAxesKey = key;
    this.callbacks.onAxesChange(axes);
  }

  // ------------------------------------------------------------------ misc
  getBounds() {
    return this.bounds.clone();
  }

  getRadius() {
    return this.radius;
  }

  getShapeKey(shapeIndex: number) {
    return this.state?.shapeKeys[shapeIndex] ?? null;
  }

  private clearGeometry() {
    this.clearCompare();
    this.clearHighlight();
    this.clearClipping();
    this.clearMeasurements();
    if (this.mesh) {
      this.scene.remove(this.mesh);
      this.mesh.geometry.dispose();
      (this.mesh.material as THREE.Material).dispose();
      this.mesh = null;
    }
    if (this.edgeLines) {
      this.scene.remove(this.edgeLines);
      this.edgeLines.geometry.dispose();
      (this.edgeLines.material as THREE.Material).dispose();
      this.edgeLines = null;
      this.edgeMaterial = null;
    }
    if (this.pickLines) {
      this.scene.remove(this.pickLines);
      this.pickLines.geometry.dispose();
      (this.pickLines.material as THREE.Material).dispose();
      this.pickLines = null;
    }
    if (this.hoverMesh) {
      this.scene.remove(this.hoverMesh);
      this.hoverMesh.geometry.dispose();
      (this.hoverMesh.material as THREE.Material).dispose();
      this.hoverMesh = null;
    }
    this.state = null;
  }

  dispose() {
    this.disposed = true;
    this.resizeObserver.disconnect();
    window.removeEventListener('resize', this.handleWindowResize);
    this.renderer.domElement.removeEventListener('webglcontextlost', this.handleContextLost);
    this.renderer.domElement.removeEventListener('webglcontextrestored', this.handleContextRestored);
    this.renderer.domElement.removeEventListener('pointerdown', this.handlePointerDown);
    this.renderer.domElement.removeEventListener('pointerup', this.handlePointerUp);
    this.renderer.domElement.removeEventListener('pointermove', this.handlePointerMove);
    this.renderer.domElement.removeEventListener('pointerleave', this.handlePointerLeave);
    this.clearGeometry();
    if (this.grid) {
      this.scene.remove(this.grid);
      this.grid.geometry.dispose();
      (this.grid.material as THREE.Material).dispose();
    }
    this.controls.dispose();
    this.renderer.dispose();
    this.renderer.domElement.remove();
  }
}
