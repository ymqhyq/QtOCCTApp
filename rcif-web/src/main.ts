import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import { router } from './router';
import { useSessionStore } from './stores/session';
import './style.css';

async function boot() {
  const app = createApp(App);
  const pinia = createPinia();
  app.use(pinia);

  const session = useSessionStore(pinia);
  await session.bootstrap();

  // 演示便捷入口：?role=reviewer 直接切换身份（仅用于演示与走查）
  const requestedRole = new URLSearchParams(window.location.search).get('role');
  if (requestedRole && session.user?.role !== requestedRole) {
    try {
      await session.login(requestedRole);
    } catch {
      /* 忽略非法角色 */
    }
  }

  app.use(router);
  await router.isReady();
  app.mount('#app');
}

void boot();
