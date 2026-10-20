import type { IncomingMessage, ServerResponse } from 'node:http';
import type { Plugin } from 'vite';
import { handleApi } from './api';

function middleware(req: IncomingMessage, res: ServerResponse, next: (err?: unknown) => void) {
  const url = new URL(req.url ?? '/', 'http://127.0.0.1');
  handleApi(req, res, url.pathname, url)
    .then((handled) => {
      if (!handled) next();
    })
    .catch((error) => {
      console.error('[cbfc-mock-api]', error);
      if (!res.headersSent) {
        res.statusCode = 500;
        res.setHeader('Content-Type', 'application/json; charset=utf-8');
        res.end(JSON.stringify({ code: 'INTERNAL_ERROR', message: String(error) }));
      } else {
        next(error);
      }
    });
}

/** 同一份 Mock API 同时挂载到 dev server 与 preview server */
export function cbfcMockApi(): Plugin {
  return {
    name: 'cbfc-mock-api',
    configureServer(server) {
      server.middlewares.use(middleware);
    },
    configurePreviewServer(server) {
      server.middlewares.use(middleware);
    },
  };
}
