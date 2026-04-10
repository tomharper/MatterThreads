/**
 * MatterThreads Simulation Tunnel Client
 *
 * Run on your Mac alongside the C++ simulation. Connects outbound
 * to the Railway relay via WebSocket. Receives forwarded HTTP requests
 * from iOS and proxies them to the local dashboard (8080) or gateway (8090).
 *
 * Usage:
 *   RELAY_URL=wss://your-relay.up.railway.app/tunnel?token=your-token node tunnel.js
 *
 * Env vars:
 *   RELAY_URL        – full WebSocket URL including token
 *   DASHBOARD_PORT   – local dashboard port (default 8080)
 *   GATEWAY_PORT     – local gateway port (default 8090)
 */

import WebSocket from 'ws';
import http from 'http';

const RELAY_URL = process.env.RELAY_URL;
const DASHBOARD_PORT = parseInt(process.env.DASHBOARD_PORT || '8080', 10);
const GATEWAY_PORT = parseInt(process.env.GATEWAY_PORT || '8090', 10);

if (!RELAY_URL) {
  console.error('Error: RELAY_URL env var is required');
  console.error('  Example: RELAY_URL=wss://matterthreads.up.railway.app/tunnel?token=dev-token node tunnel.js');
  process.exit(1);
}

const RECONNECT_DELAY_MS = 3000;
let ws = null;

function connect() {
  console.log(`Connecting to relay: ${RELAY_URL.replace(/token=.*/, 'token=***')}`);
  ws = new WebSocket(RELAY_URL);

  ws.on('open', () => {
    console.log('Connected to relay');
  });

  ws.on('message', async (data) => {
    try {
      const req = JSON.parse(data);
      const response = await proxyToLocal(req);
      ws.send(JSON.stringify(response));
    } catch (err) {
      console.error('Error handling request:', err.message);
    }
  });

  ws.on('close', (code, reason) => {
    console.log(`Disconnected from relay (code=${code}). Reconnecting in ${RECONNECT_DELAY_MS}ms...`);
    setTimeout(connect, RECONNECT_DELAY_MS);
  });

  ws.on('error', (err) => {
    console.error('WebSocket error:', err.message);
    // 'close' event will fire after this, triggering reconnect
  });
}

/**
 * Proxy an incoming request envelope to the local simulation.
 * Routes /api/* to dashboard (8080) or gateway (8090) based on path.
 */
function proxyToLocal(req) {
  return new Promise((resolve) => {
    // Determine target port:
    // Gateway paths: /api/vans, /api/fleet, /api/health
    // Everything else: dashboard
    const path = req.path || '/';
    const isGateway = path.startsWith('/api/vans') ||
                      path.startsWith('/api/fleet') ||
                      path.startsWith('/api/health');
    const port = isGateway ? GATEWAY_PORT : DASHBOARD_PORT;

    const options = {
      hostname: 'localhost',
      port,
      path,
      method: req.method || 'GET',
      headers: { 'Content-Type': 'application/json' },
      timeout: 8000,
    };

    const httpReq = http.request(options, (httpRes) => {
      let body = '';
      httpRes.on('data', (chunk) => { body += chunk; });
      httpRes.on('end', () => {
        let parsed;
        try {
          parsed = JSON.parse(body);
        } catch {
          parsed = body;
        }
        resolve({ id: req.id, status: httpRes.statusCode, body: parsed });
      });
    });

    httpReq.on('error', (err) => {
      resolve({ id: req.id, status: 502, body: { error: `local: ${err.message}` } });
    });

    httpReq.on('timeout', () => {
      httpReq.destroy();
      resolve({ id: req.id, status: 504, body: { error: 'local timeout' } });
    });

    if (req.body && req.method !== 'GET') {
      httpReq.write(typeof req.body === 'string' ? req.body : JSON.stringify(req.body));
    }
    httpReq.end();
  });
}

// Start
connect();

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\nShutting down tunnel...');
  if (ws) ws.close();
  process.exit(0);
});
