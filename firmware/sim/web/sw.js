// Service worker of the simulator: plays the network between the browser and
// the reader. Requests for /device/... (the address of the upload page in the
// simulator) and every request made by that page go to the firmware's web
// server through the simulator tab.
'use strict';

const TIMEOUT_MS = 15000;

self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', (e) => e.waitUntil(self.clients.claim()));

const devicePrefix = () => new URL('device/', self.registration.scope).pathname;

self.addEventListener('fetch', (event) => {
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return;
  event.respondWith((async () => {
    let path = null;
    if (url.pathname.startsWith(devicePrefix())) {
      path = '/' + url.pathname.slice(devicePrefix().length);
    } else if (event.clientId) {
      const client = await self.clients.get(event.clientId);
      if (client && new URL(client.url).pathname.startsWith(devicePrefix())) path = url.pathname;
    }
    if (path === null) return fetch(event.request);
    return forward(event.request, path, url.search.slice(1));
  })());
});

async function simulatorTab() {
  const all = await self.clients.matchAll({ type: 'window', includeUncontrolled: true });
  return all.find((c) => !new URL(c.url).pathname.startsWith(devicePrefix()));
}

async function forward(request, path, query) {
  const tab = await simulatorTab();
  if (!tab) return new Response('The simulator tab is closed.', { status: 504 });
  const msg = { kind: 'http', method: request.method, path, query, fileName: '', body: null };
  if (request.method === 'POST') {
    const type = request.headers.get('content-type') || '';
    if (type.startsWith('multipart/form-data')) {
      const form = await request.formData();
      for (const [, v] of form) {
        if (typeof v !== 'string') { msg.fileName = v.name; msg.body = await v.arrayBuffer(); break; }
      }
    } else {
      msg.body = await request.arrayBuffer();
    }
  }
  const channel = new MessageChannel();
  const reply = new Promise((resolve) => {
    const timer = setTimeout(() => resolve({ status: 504, type: 'text/plain', body: new TextEncoder().encode('timeout') }), TIMEOUT_MS);
    channel.port1.onmessage = (e) => { clearTimeout(timer); resolve(e.data); };
  });
  tab.postMessage(msg, [channel.port2]);
  const r = await reply;
  return new Response(r.body, { status: r.status, headers: { 'content-type': r.type || 'text/plain' } });
}
