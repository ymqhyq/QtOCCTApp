import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";
import path from "path";
import topLevelAwait from "vite-plugin-top-level-await";
import wasm from "vite-plugin-wasm";
import { nodePolyfills } from "vite-plugin-node-polyfills";

// https://vitejs.dev/config/
export default defineConfig({
    plugins: [
        vue(),
        wasm(),
        topLevelAwait(),
        nodePolyfills({
            include: ['module', 'path', 'fs']
        })
    ],
    resolve: {
        alias: {
            "@": path.resolve(__dirname, "./src"),
        },
    },
    server: {
        proxy: {
            '/api': {
                target: 'http://127.0.0.1:8000',
                changeOrigin: true
            }
        }
    },
    optimizeDeps: {
        exclude: ["@bitbybit-dev/occt-worker"]
    }
});
