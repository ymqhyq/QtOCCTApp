import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import { fileURLToPath, URL } from 'node:url';
import { cbfcMockApi } from './server/plugin';

export default defineConfig({
  plugins: [
    vue(),
    cbfcMockApi(),
    {
      name: 'cbfc-production-csp',
      apply: 'build',
      transformIndexHtml(html) {
        const csp =
          "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; worker-src 'self' blob:; font-src 'self' data:; frame-src 'self' cbfc:; object-src 'none'; base-uri 'self'; form-action 'self'";
        return html.replace(
          '</head>',
          `    <meta http-equiv="Content-Security-Policy" content="${csp}" />\n  </head>`,
        );
      },
    },
  ],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
      '@shared': fileURLToPath(new URL('./shared', import.meta.url)),
    },
  },
  server: {
    host: '127.0.0.1',
    port: 5178,
    strictPort: true,
    watch: {
      ignored: ['**/shots/**', '**/.chrome-*/**', '**/.npm-cache/**', '**/dist/**'],
    },
  },
  preview: {
    host: '127.0.0.1',
    port: 5178,
    strictPort: true,
    headers: {
      'Content-Security-Policy':
        "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; worker-src 'self' blob:; font-src 'self' data:; frame-src 'self' cbfc:; frame-ancestors 'none'; object-src 'none'; base-uri 'self'; form-action 'self'",
      'X-Frame-Options': 'DENY',
      'X-Content-Type-Options': 'nosniff',
      'Referrer-Policy': 'strict-origin-when-cross-origin',
    },
  },
  build: {
    target: 'es2022',
    chunkSizeWarningLimit: 1400,
    rollupOptions: {
      output: {
        manualChunks: {
          three: ['three'],
          vue: ['vue', 'vue-router', 'pinia'],
        },
      },
    },
  },
  worker: {
    format: 'es',
  },
});
