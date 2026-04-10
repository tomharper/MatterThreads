/**
 * MatterThreads Simulation Relay
 *
 * Sits on Railway (or any public host). Two roles:
 *   1. Accepts a persistent WebSocket from the Mac-side tunnel client
 *      on /tunnel?token=<RELAY_TOKEN>
 *   2. Exposes HTTPS endpoints matching the dashboard/gateway API surface.
 *      Every incoming HTTP request is forwarded over the WebSocket tunnel
 *      to the Mac, which proxies it to localhost:8080/8090 and returns
 *      the response.
 *
 * Env vars:
 *   PORT         – HTTP listen port (Railway sets this automatically)
 *   RELAY_TOKEN  – shared secret for both tunnel auth and iOS bearer token
 */

import Fastify from 'fastify';
import { WebSocketServer, WebSocket } from 'ws';
import { randomUUID } from 'crypto';

const PORT = parseInt(process.env.PORT || '3000', 10);
const RELAY_TOKEN = process.env.RELAY_TOKEN || 'dev-token';
const REQUEST_TIMEOUT_MS = 10_000;

const app = Fastify({ logger: true });

// ─── Tunnel state ─────────────────────────────────────────────────────

let tunnelSocket = null;
const pendingRequests = new Map(); // id → { resolve, timer }

// ─── WebSocket upgrade for /tunnel ────────────────────────────────────

const wss = new WebSocketServer({ noServer: true });

app.server.on('upgrade', (req, socket, head) => {
  const url = new URL(req.url, `http://${req.headers.host}`);
  if (url.pathname !== '/tunnel') {
    socket.destroy();
    return;
  }
  const token = url.searchParams.get('token');
  if (token !== RELAY_TOKEN) {
    socket.write('HTTP/1.1 401 Unauthorized\r\n\r\n');
    socket.destroy();
    return;
  }
  wss.handleUpgrade(req, socket, head, (ws) => {
    wss.emit('connection', ws, req);
  });
});

wss.on('connection', (ws) => {
  if (tunnelSocket && tunnelSocket.readyState === WebSocket.OPEN) {
    tunnelSocket.close(1000, 'replaced');
  }
  tunnelSocket = ws;
  app.log.info('Tunnel client connected');

  ws.on('message', (data) => {
    try {
      const msg = JSON.parse(data);
      const pending = pendingRequests.get(msg.id);
      if (pending) {
        clearTimeout(pending.timer);
        pendingRequests.delete(msg.id);
        pending.resolve(msg);
      }
    } catch (e) {
      app.log.error({ err: e }, 'Bad tunnel message');
    }
  });

  ws.on('close', () => {
    if (tunnelSocket === ws) {
      tunnelSocket = null;
      app.log.info('Tunnel client disconnected');
    }
  });

  ws.on('error', (err) => {
    app.log.error({ err }, 'Tunnel socket error');
  });
});

// ─── Auth middleware ──────────────────────────────────────────────────

function checkAuth(request, reply) {
  const auth = request.headers.authorization;
  if (auth !== `Bearer ${RELAY_TOKEN}`) {
    reply.code(401).send({ error: 'unauthorized' });
    return false;
  }
  return true;
}

// ─── Forward helper ──────────────────────────────────────────────────

function forwardToTunnel(method, path, body) {
  return new Promise((resolve, reject) => {
    if (!tunnelSocket || tunnelSocket.readyState !== WebSocket.OPEN) {
      reject(new Error('no tunnel'));
      return;
    }
    const id = randomUUID();
    const timer = setTimeout(() => {
      pendingRequests.delete(id);
      reject(new Error('timeout'));
    }, REQUEST_TIMEOUT_MS);

    pendingRequests.set(id, { resolve, timer });
    tunnelSocket.send(JSON.stringify({ id, method, path, body }));
  });
}

// ─── Health check (no auth, no tunnel needed) ────────────────────────

app.get('/health', async () => {
  return {
    relay: 'ok',
    tunnel: tunnelSocket?.readyState === WebSocket.OPEN ? 'connected' : 'disconnected',
  };
});

// ─── Catch-all: forward /api/* to tunnel ─────────────────────────────

app.all('/api/*', async (request, reply) => {
  if (!checkAuth(request, reply)) return;

  try {
    const result = await forwardToTunnel(
      request.method,
      request.url,           // includes /api/... and query string
      request.body ?? null
    );
    reply.code(result.status || 200).send(result.body);
  } catch (err) {
    if (err.message === 'no tunnel') {
      reply.code(502).send({ error: 'simulation not connected' });
    } else if (err.message === 'timeout') {
      reply.code(504).send({ error: 'simulation did not respond' });
    } else {
      reply.code(500).send({ error: err.message });
    }
  }
});

// ─── Start ────────────────────────────────────────────────────────────

app.listen({ port: PORT, host: '0.0.0.0' }).then(() => {
  app.log.info(`Relay listening on :${PORT}, token: ${RELAY_TOKEN.slice(0, 4)}...`);
});
