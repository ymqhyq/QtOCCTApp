import { Engine, Scene, Vector3, HemisphericLight, ArcRotateCamera, Color4 } from "@babylonjs/core";
import { BitByBitBase } from "@bitbybit-dev/babylonjs";
// Import worker URLs or create workers (vite can bundle workers)
import OCCTWorker from "@bitbybit-dev/occt-worker/lib/occ-worker/occ-worker.js?worker";

export class BitbybitService {
    private static instance: BitbybitService;
    public bitbybit!: BitByBitBase;

    private constructor() { }

    public static getInstance(): BitbybitService {
        if (!BitbybitService.instance) {
            BitbybitService.instance = new BitbybitService();
        }
        return BitbybitService.instance;
    }

    public async init(canvas: HTMLCanvasElement) {
        // 1. Initialize BabylonJS Engine and Scene
        const engine = new Engine(canvas, true, { preserveDrawingBuffer: true, stencil: true });
        const scene = new Scene(engine);
        scene.clearColor = new Color4(0.1, 0.1, 0.1, 1);

        // 2. Setup Camera and Light
        const camera = new ArcRotateCamera("Camera", -Math.PI / 2, Math.PI / 2.5, 50, Vector3.Zero(), scene);
        camera.attachControl(canvas, true);
        const light = new HemisphericLight("light", new Vector3(0, 1, 0), scene);
        light.intensity = 0.7;

        // 3. Start render loop
        engine.runRenderLoop(() => {
            scene.render();
        });

        // 4. Initialize BitByBit
        const bitbybit = new BitByBitBase();

        // Spawn OCCT worker using Vite's ?worker syntax
        const occtWorker = new OCCTWorker();

        bitbybit.init(scene as any, occtWorker);

        this.bitbybit = bitbybit;

        // Optional: Configure scene settings
        this.bitbybit.babylon.scene.backgroundColour({ colour: "#1a1a1b" });

        // Use native BabylonJS to test rendering
        import("@babylonjs/core").then(({ MeshBuilder }) => {
            MeshBuilder.CreateBox("testBox", { size: 10 }, scene);
        });

        return bitbybit;
    }

    public async clearScene() {
        if (this.bitbybit && this.bitbybit.context && this.bitbybit.context.scene) {
            // Scene clear logic if needed, or loop meshes
        }
    }
}

export const bbbService = BitbybitService.getInstance();
