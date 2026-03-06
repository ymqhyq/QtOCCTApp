import { BitByBitBase, Inputs, initBitByBit, initBabylonJS, type InitBitByBitOptions } from "@bitbybit-dev/babylonjs";
// Must import loaders to support GLB/GLTF loading
import "@babylonjs/loaders";
import * as BABYLON from "@babylonjs/core";

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
        console.log("Starting BitbybitService.init matching official examples...");

        // Ensure canvas has the ID expected by initBabylonJS
        if (!canvas.id) {
            canvas.id = "babylon-canvas";
        }

        // Match Official Example's InitBabylonJSDto settings
        const babylonOptions = new Inputs.BabylonJSScene.InitBabylonJSDto();
        babylonOptions.canvasId = canvas.id;
        babylonOptions.sceneSize = 200;
        babylonOptions.enableShadows = true;
        babylonOptions.enableGround = false; // We might not want the ground for a CAD viewer initially
        babylonOptions.groundColor = "#333333";
        babylonOptions.groundCenter = [0, -75, 0];
        // 提升官方初始化默认打光强度解决无光照死黑问题
        babylonOptions.hemisphereLightIntensity = 2.5;
        babylonOptions.directionalLightIntensity = 3.0;

        // This will create Engine, Scene, and Default Lights just like the official example
        const { scene, engine, hemisphericLight, directionalLight } = initBabylonJS(babylonOptions);
        const bitbybit = new BitByBitBase();

        // Use standard CDN workers by default, matching starter-template
        const options: InitBitByBitOptions = {
            enableOCCT: true,
            enableJSCAD: false,
            enableManifold: false,
        };

        try {
            console.log("Calling initBitByBit...");
            await initBitByBit(scene, bitbybit, options);
            console.log("BitByBit kernel initialized successfully!");
        } catch (initErr) {
            console.error("Critical error during BitByBit core initialization:", initErr);
            throw initErr;
        }

        this.bitbybit = bitbybit;

        // 对官方生成好的标准光源进行二次调优 (模拟 CAD 经典三点打光或全景打光环境)
        if (hemisphericLight) {
            hemisphericLight.direction = new BABYLON.Vector3(0, 1, 0);
        }

        if (directionalLight) {
            // 将默认的方向光作为强的主侧光
            directionalLight.direction = new BABYLON.Vector3(-1, -2, -1);
        }

        // 添加一个补光灯保证暗面也能被照亮
        const fillLight = new BABYLON.DirectionalLight(
            "fillLight",
            new BABYLON.Vector3(1, 0.5, 1),
            scene as any
        );
        fillLight.intensity = 2.0;

        // Start render loop
        engine.runRenderLoop(() => {
            if (scene.activeCamera) {
                scene.render();
            }
        });

        // Optional: Configure scene settings
        this.bitbybit.babylon.scene.backgroundColour({ colour: "#1a1a1b" });

        return bitbybit;
    }

    public async clearScene() {
        if (this.bitbybit && this.bitbybit.context && this.bitbybit.context.scene) {
            // Scene clear logic if needed
        }
    }
}

export const bbbService = BitbybitService.getInstance();
