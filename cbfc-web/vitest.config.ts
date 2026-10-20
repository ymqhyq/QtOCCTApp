import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'node',
    include: ['shared/**/*.spec.ts', 'src/**/*.spec.ts', 'server/**/*.spec.ts'],
    // 沙箱环境无法 fork 子进程（EPERM），统一使用 worker_threads 线程池
    pool: 'threads',
    poolOptions: { threads: { singleThread: true } },
  },
});
