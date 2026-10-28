/**
 * RCIF Studio client bridge (browser half).
 *
 * The workbench lives in a sibling tab/iframe. When the user selects a
 * workspace/session there, it posts a `rcif:open-session` message over
 * BroadcastChannel (and persists it in localStorage as a cold-start fallback).
 * This plugin calls the real DSH navigation service `uiWorkspace.openSession`,
 * so the assistant switches to the same session.
 */
window.__ModuleLoader__.load({
  id: '@rcif/dsh-studio-plugin',
  factory: () => {
    var module = { exports: {} };
    var exports = module.exports;

    const name = 'rcif-session-bridge';
    const inject = ['uiWorkspace'];
    const CHANNEL = 'rcif-session-bridge';
    const STORAGE_KEY = 'rcif:open-session';

    function apply(ctx) {
      window.__RCIF_BRIDGE_READY__ = true;
      const open = (sessionId) => {
        if (typeof sessionId !== 'string' || sessionId.length === 0) return;
        window.__RCIF_LAST_OPEN__ = sessionId;
        try {
          ctx.uiWorkspace.openSession(sessionId);
          // 新建会话刚 attach 时，助手会话列表可能还没收到增量；
          // 700ms 后再对齐一次，避免 openSession 静默 no-op。
          window.setTimeout(() => {
            try {
              ctx.uiWorkspace.openSession(sessionId);
            } catch {
              /* session may have been archived */
            }
          }, 700);
        } catch (error) {
          // 会话可能已被归档/删除；导航失败不应影响工作台
          window.__RCIF_BRIDGE_ERROR__ = String(error);
          console.warn('[rcif] openSession failed', error);
        }
      };

      let channel;
      try {
        channel = new BroadcastChannel(CHANNEL);
        channel.addEventListener('message', (event) => {
          const data = event.data;
          if (!data || data.type !== 'rcif:open-session') return;
          open(data.sessionId);
        });
      } catch {
        channel = undefined;
      }

      const onStorage = (event) => {
        if (event.key !== STORAGE_KEY || !event.newValue) return;
        open(event.newValue);
      };
      window.addEventListener('storage', onStorage);

      // 助手后打开时，消费工作台留下的最后一次会话选择
      try {
        const pending = window.localStorage.getItem(STORAGE_KEY);
        if (pending) {
          window.setTimeout(() => open(pending), 800);
        }
      } catch {
        /* localStorage 不可用时只依赖 BroadcastChannel */
      }

      ctx.effect(() => () => {
        try {
          channel?.close();
        } catch {
          /* already closed */
        }
        window.removeEventListener('storage', onStorage);
      }, 'rcif-session-bridge');
    }

    exports.name = name;
    exports.inject = inject;
    exports.apply = apply;
    return module.exports;
  },
});
