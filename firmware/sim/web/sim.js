// E-reader simulator page: runs firmware.js/.wasm (pio run -e sim) and plays
// the hardware around it. The C side calls the hooks in `hooks()` (see
// sim/src/Bridge.cpp); the page calls the exported sim_* functions.
'use strict';

const $ = (s) => document.querySelector(s);
const PANEL_W = 400, PANEL_H = 300, STRIDE = PANEL_W / 8;
const EXT1 = 3;                       // ESP_SLEEP_WAKEUP_EXT1
const SAMPLES = ['sample.epub', 'sample2.epub', 'drm.epub'];
const WIFI_SAMPLE = '# One network per line: SSID = key (empty for an open network)\nSimulator WLAN =\n';
const SAMPLE_TXT = `Welcome to the e-reader simulator

This page runs the real firmware, compiled to WebAssembly. The display, the five buttons, the microSD card, the USB-C port and the Wi-Fi upload page are simulated around it.

Turn pages with NEXT and PREV (the arrow keys work too). Hold a page-turn button for a long press to jump ten pages. MENU opens the menu: font size, contents, Wi-Fi upload, USB drive, sleep.

After three minutes without a button the reader goes to deep sleep. The page then keeps the picture, like e-paper does; any button wakes it up again, and NEXT or PREV turns the page right away.

Use "Skip 3 min" to get there without waiting.
`;

// ---------------------------------------------------------------- state

let wasmModule = null;      // compiled once, instantiated on every boot
let M = null;               // running firmware instance
let fsOwner = null;         // instance whose FS the file panel uses
let state = 'off';          // off | awake | sleep
let rtcSaved = null, wakeMask = 0;
let buttonPins = [5, 6, 9, 10, 11];
const held = new Set();     // button indices currently pressed
let cable = 0, usbMedia = false, webOn = false;
let nextReq = 1;
const pending = new Map();  // http request id -> MessagePort

// ---------------------------------------------------------------- panel

const canvas = $('#panel');
const ctx = canvas.getContext('2d');
const glass = ctx.createImageData(300, 400);   // what the e-paper shows
const PAPER = [233, 231, 224], INK = [35, 35, 35];
let animation = Promise.resolve();

function paint(fill) {
  const d = glass.data;
  for (let i = 0; i < d.length; i += 4) {
    const c = fill(i >> 2);
    d[i] = c[0]; d[i + 1] = c[1]; d[i + 2] = c[2]; d[i + 3] = 255;
  }
  ctx.putImageData(glass, 0, 0);
}

// Panel RAM is 400 × 300 (landscape). The module sits with its flex cable at
// the top of the case, so GxEPD2 rotation 1 is upright portrait here:
// screen (x, y) = panel (399 - y, x).
function ramToScreen(ram) {
  const px = new Uint8Array(300 * 400);
  for (let y = 0; y < 400; y++) {
    const panelX = PANEL_W - 1 - y;
    const byte = panelX >> 3, bit = 0x80 >> (panelX & 7);
    for (let x = 0; x < 300; x++) px[y * 300 + x] = (ram[x * STRIDE + byte] & bit) ? 1 : 0;
  }
  return px;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

function refresh(ram, kind, ms) {
  const px = ramToScreen(ram);
  const show = () => paint((i) => (px[i] ? PAPER : INK));
  animation = animation.then(async () => {
    if (ms <= 0) return show();
    setState('busy');
    if (kind === 0) {
      // full refresh: the panel flashes inverted, black, white, then the picture
      const old = new Uint8Array(300 * 400);
      for (let i = 0; i < old.length; i++) old[i] = glass.data[i * 4] > 128 ? 1 : 0;
      const step = ms / 4;
      paint((i) => (old[i] ? INK : PAPER)); await sleep(step);
      paint(() => INK); await sleep(step);
      paint(() => PAPER); await sleep(step);
      show(); await sleep(step * 0.5);
    } else {
      await sleep(ms * 0.4);
      show();
    }
    if (state === 'awake') setState('awake');
  });
}

// ---------------------------------------------------------------- status

function setState(s, text) {
  if (s !== 'busy') state = s;
  const dot = $('#stateDot');
  dot.className = 'dot ' + (s === 'busy' ? 'busy' : s);
  $('#stateText').textContent = text || {
    awake: 'Awake', busy: 'Refreshing the display…', sleep: 'Deep sleep: press any button to wake it', off: 'Off',
  }[s];
  const ov = $('#overlay');
  if (s === 'sleep') { ov.hidden = false; ov.textContent = 'Deep sleep\nany button wakes the reader'; }
  else if (s === 'off') { ov.hidden = false; ov.textContent = text || 'Off'; }
  else ov.hidden = true;
}

function log(text, cls) {
  const el = $('#log');
  const atEnd = el.scrollTop + el.clientHeight >= el.scrollHeight - 4;
  const line = document.createElement('div');
  line.textContent = text;
  if (cls) line.className = cls;
  el.append(line);
  while (el.childNodes.length > 2000) el.firstChild.remove();
  if (atEnd) el.scrollTop = el.scrollHeight;
}

// ---------------------------------------------------------------- boot

async function loadWasm() {
  const resp = fetch('firmware.wasm');
  try { wasmModule = await WebAssembly.compileStreaming(resp); }
  catch { wasmModule = await WebAssembly.compile(await (await fetch('firmware.wasm')).arrayBuffer()); }
}

function hooks(boot) {
  return {
    boot,
    powerCut: false,
    refresh,
    serial: (t) => log(t),
    led: (on) => $('#led').classList.toggle('on', on),
    deepSleep: (rtc, lo) => { rtcSaved = rtc; wakeMask = lo; onDeepSleep(); },
    httpResponse: (id, code, type, body) => {
      const port = pending.get(id);
      if (!port) return;
      pending.delete(id);
      port.postMessage({ status: code, type, body }, [body.buffer]);
    },
    webListening: (on) => { webOn = on; showWeb(); },
    usbMedia: (on) => { usbMedia = on; $('#drive').hidden = !on; },
    fsChanged: () => { scheduleSync(); scheduleList(); },
  };
}

async function boot(info) {
  const inst = {
    sim: hooks(info),
    instantiateWasm(imports, receive) {
      WebAssembly.instantiate(wasmModule, imports).then((i) => receive(i, wasmModule));
      return {};
    },
    print: (t) => log(t),
    printErr: (t) => { if (!/keepRuntimeAlive|program exited/.test(t)) log(t, 'err'); },
    preRun: [() => {
      for (const dir of ['/sdcard', '/nvs']) {
        inst.FS.mkdir(dir);
        inst.FS.mount(inst.IDBFS || inst.FS.filesystems.IDBFS, {}, dir);
      }
      inst.addRunDependency('idbfs');
      inst.FS.syncfs(true, async (err) => {
        if (err) log('Could not load the card from IndexedDB: ' + err, 'err');
        await seedCard(inst);
        inst.removeRunDependency('idbfs');
      });
    }],
    onRuntimeInitialized() {
      buttonPins = [0, 1, 2, 3, 4].map((i) => inst._sim_button_pin(i));
      pushInputs(inst);
    },
  };
  M = inst;
  fsOwner = inst;
  setState('awake', info.wakeCause === EXT1 ? 'Waking up…' : 'Booting…');
  await createFirmware(inst);
  if (M === inst && state === 'awake') setState('awake');
  scheduleList();
}

// Everything the hardware around the chip looks like right now.
function pushInputs(inst) {
  for (let i = 0; i < 5; i++) inst._sim_set_input(buttonPins[i], held.has(i) ? 0 : -1);
  inst._sim_set_time_scale($('#instant').checked ? 0 : 1);
  pushBattery(inst);
  inst._sim_set_card($('#card').checked ? 1 : 0);
  inst._sim_set_wifi_range($('#wifiRange').checked ? 1 : 0);
  inst._sim_usb_cable(cable);
}

function pushBattery(inst = M) {
  if (!inst) return;
  const pct = +$('#battery').value;
  inst._sim_set_battery($('#gauge').checked ? 1 : 0, pct, Math.round(3300 + pct * 8.5));
}

function endInstance() {
  for (const [id, port] of pending) port.postMessage({ status: 504, type: 'text/plain', body: new Uint8Array() });
  pending.clear();
  webOn = false; showWeb();
  usbMedia = false; $('#drive').hidden = true;
  M = null;
}

async function onDeepSleep() {
  endInstance();
  setState('sleep');
  await syncCard();
}

// A wake-up button went down: the chip resets and keeps its RTC memory.
function wake(btn) {
  const pin = buttonPins[btn];
  if (!(wakeMask & (1 << pin))) return;
  boot({ rtc: rtcSaved, wakeCause: EXT1, ext1: 1 << pin });
}

async function reset() {
  if (M) {
    M.sim.powerCut = true;     // the firmware stops at its next delay()
    endInstance();
  }
  setState('off', 'Restarting…');
  await syncCard();
  rtcSaved = null;
  await sleep(150);
  boot({ rtc: null, wakeCause: 0, ext1: 0 });
}

// ---------------------------------------------------------------- buttons

// A finger holds a button for at least ~80 ms; key events and quick taps can
// be shorter than the firmware's poll + debounce, so short releases wait.
const MIN_PRESS_MS = 80;
const downAt = new Map();

function press(btn, down) {
  if (down === held.has(btn)) return;
  if (down) downAt.set(btn, performance.now());
  else {
    const left = MIN_PRESS_MS - (performance.now() - (downAt.get(btn) || 0));
    if (left > 0) { setTimeout(() => press(btn, false), left); return; }
  }
  if (down) held.add(btn); else held.delete(btn);
  document.querySelectorAll(`[data-btn="${btn}"]`).forEach((b) => b.classList.toggle('down', down));
  if (M) M._sim_set_input(buttonPins[btn], down ? 0 : -1);
  else if (down && state === 'sleep') wake(btn);
}

document.querySelectorAll('[data-btn]').forEach((el) => {
  const btn = +el.dataset.btn;
  el.addEventListener('pointerdown', (e) => { e.preventDefault(); el.setPointerCapture(e.pointerId); press(btn, true); });
  el.addEventListener('pointerup', () => press(btn, false));
  el.addEventListener('pointercancel', () => press(btn, false));
  el.addEventListener('keydown', (e) => { if ((e.key === ' ' || e.key === 'Enter') && !e.repeat) { e.preventDefault(); e.stopPropagation(); press(btn, true); } });
  el.addEventListener('keyup', (e) => { if (e.key === ' ' || e.key === 'Enter') { e.stopPropagation(); press(btn, false); } });
  el.addEventListener('contextmenu', (e) => e.preventDefault());
});

const KEYS = {
  ArrowLeft: 0, ArrowUp: 0, PageUp: 0,
  ArrowRight: 1, ArrowDown: 1, PageDown: 1,
  m: 2, M: 2, Enter: 3, Escape: 4, Backspace: 4,
};
function typing(e) { return /^(INPUT|TEXTAREA|SELECT)$/.test(e.target.tagName) || e.target.isContentEditable; }
window.addEventListener('keydown', (e) => {
  if (typing(e) || e.metaKey || e.ctrlKey || e.altKey || !(e.key in KEYS)) return;
  e.preventDefault();
  if (!e.repeat) press(KEYS[e.key], true);
});
window.addEventListener('keyup', (e) => { if (e.key in KEYS && !typing(e)) press(KEYS[e.key], false); });
window.addEventListener('blur', () => { for (const b of [...held]) press(b, false); });

// ---------------------------------------------------------------- controls

$('#resetBtn').onclick = reset;
$('#skipBtn').onclick = () => M && M._sim_time_skip(3 * 60 * 1000);
$('#instant').onchange = () => M && M._sim_set_time_scale($('#instant').checked ? 0 : 1);
$('#battery').oninput = () => { $('#batteryOut').textContent = $('#battery').value + ' %'; pushBattery(); };
$('#gauge').onchange = () => pushBattery();
$('#card').onchange = () => M && M._sim_set_card($('#card').checked ? 1 : 0);
$('#wifiRange').onchange = () => M && M._sim_set_wifi_range($('#wifiRange').checked ? 1 : 0);
document.querySelectorAll('input[name=cable]').forEach((r) => r.onchange = () => {
  cable = +r.value;
  if (M) M._sim_usb_cable(cable);
});
$('#ejectBtn').onclick = () => M && M._sim_usb_eject();
$('#clearLog').onclick = () => { $('#log').textContent = ''; };

// ---------------------------------------------------------------- card

const CARD = '/sdcard';

async function seedCard(inst) {
  const FS = inst.FS;
  if (FS.readdir(CARD).length > 2) return;   // only . and ..
  log('First start: putting sample books and a wifi.txt on the card.');
  FS.mkdir(CARD + '/books');
  for (const name of SAMPLES) {
    try {
      const r = await fetch('samples/' + name);
      if (r.ok) FS.writeFile(`${CARD}/books/${name}`, new Uint8Array(await r.arrayBuffer()));
    } catch { /* offline: no samples */ }
  }
  FS.writeFile(`${CARD}/books/Simulator.txt`, SAMPLE_TXT);
  FS.writeFile(`${CARD}/wifi.txt`, WIFI_SAMPLE);
}

let syncing = null, syncAgain = false, syncTimer = 0;
function syncCard() {
  const inst = fsOwner;
  if (!inst) return Promise.resolve();
  if (syncing) { syncAgain = true; return syncing; }
  syncing = new Promise((resolve) => inst.FS.syncfs(false, (err) => {
    if (err) log('Could not save the card to IndexedDB: ' + err, 'err');
    resolve();
  })).then(() => {
    syncing = null;
    if (syncAgain) { syncAgain = false; return syncCard(); }
  });
  return syncing;
}
function scheduleSync() { clearTimeout(syncTimer); syncTimer = setTimeout(syncCard, 800); }
setInterval(() => { if (M) syncCard(); }, 5000);   // cache files the firmware writes
document.addEventListener('visibilitychange', () => { if (document.hidden) syncCard(); });
window.addEventListener('pagehide', () => syncCard());

function fmtSize(n) {
  return n >= 1048576 ? (n / 1048576).toFixed(1) + ' MB' : n >= 1024 ? Math.ceil(n / 1024) + ' KB' : n + ' B';
}

let listTimer = 0;
function scheduleList() { clearTimeout(listTimer); listTimer = setTimeout(listFiles, 300); }

function listFiles() {
  const ul = $('#files');
  ul.textContent = '';
  if (!fsOwner) return;
  const FS = fsOwner.FS;
  const walk = (dir, depth) => {
    let names;
    try { names = FS.readdir(dir).filter((n) => n !== '.' && n !== '..').sort(); } catch { return; }
    const entries = names.map((n) => ({ n, st: FS.stat(`${dir}/${n}`) }));
    entries.sort((a, b) => FS.isDir(b.st.mode) - FS.isDir(a.st.mode) || a.n.localeCompare(b.n));
    for (const { n, st } of entries) {
      const path = `${dir}/${n}`, isDir = FS.isDir(st.mode);
      const li = document.createElement('li');
      li.className = (isDir ? 'dir' : '') + (n.startsWith('.') ? ' hidden' : '');
      const name = document.createElement('span');
      name.className = 'name';
      name.style.paddingLeft = depth * 14 + 'px';
      name.textContent = isDir ? n + '/' : n;
      name.title = path.slice(CARD.length);
      li.append(name);
      if (!isDir) {
        const size = document.createElement('span');
        size.className = 'size';
        size.textContent = fmtSize(st.size);
        const dl = document.createElement('button');
        dl.textContent = 'Save';
        dl.title = 'Download ' + n;
        dl.onclick = () => download(path, n);
        const del = document.createElement('button');
        del.textContent = 'Delete';
        del.onclick = () => { if (confirm(`Delete ${path.slice(CARD.length)}?`)) { FS.unlink(path); afterEdit(); } };
        li.append(size, dl, del);
      }
      ul.append(li);
      if (isDir && n !== '.reader') walk(path, depth + 1);
    }
  };
  walk(CARD, 0);
  if (!ul.childNodes.length) ul.innerHTML = '<li class="empty">The card is empty.</li>';
}

function download(path, name) {
  const data = fsOwner.FS.readFile(path);
  const a = document.createElement('a');
  a.href = URL.createObjectURL(new Blob([data]));
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

function afterEdit() { listFiles(); syncCard(); }

async function addFiles(files) {
  if (!fsOwner) return;
  const FS = fsOwner.FS;
  try { FS.mkdir(CARD + '/books'); } catch { /* exists */ }
  for (const f of files) {
    const data = new Uint8Array(await f.arrayBuffer());
    const target = f.name.toLowerCase() === 'wifi.txt' ? `${CARD}/wifi.txt` : `${CARD}/books/${f.name.replace(/[\\/]/g, '_')}`;
    FS.writeFile(target, data);
    log(`Card: copied ${f.name} (${fmtSize(data.length)})`);
  }
  afterEdit();
}

const drop = $('#drop');
drop.onclick = () => $('#pick').click();
drop.onkeydown = (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); $('#pick').click(); } };
$('#pick').onchange = (e) => { addFiles([...e.target.files]); e.target.value = ''; };
drop.ondragover = (e) => { e.preventDefault(); drop.classList.add('over'); };
drop.ondragleave = () => drop.classList.remove('over');
drop.ondrop = (e) => { e.preventDefault(); drop.classList.remove('over'); addFiles([...e.dataTransfer.files]); };
$('#refreshFiles').onclick = listFiles;

$('#editWifi').onclick = () => {
  let text = '';
  try { text = new TextDecoder().decode(fsOwner.FS.readFile(CARD + '/wifi.txt')); } catch { text = WIFI_SAMPLE; }
  $('#wifiText').value = text;
  $('#wifiEditor').hidden = false;
  $('#wifiText').focus();
};
$('#cancelWifi').onclick = () => { $('#wifiEditor').hidden = true; };
$('#saveWifi').onclick = () => {
  fsOwner.FS.writeFile(CARD + '/wifi.txt', new TextEncoder().encode($('#wifiText').value));
  $('#wifiEditor').hidden = true;
  afterEdit();
};

$('#wipeCard').onclick = async () => {
  if (!fsOwner || !confirm('Erase the card and the saved settings, and put the sample books back?')) return;
  const FS = fsOwner.FS;
  const rm = (p) => {
    for (const n of FS.readdir(p)) {
      if (n === '.' || n === '..') continue;
      const q = `${p}/${n}`;
      if (FS.isDir(FS.stat(q).mode)) { rm(q); FS.rmdir(q); } else FS.unlink(q);
    }
  };
  rm(CARD);
  rm('/nvs');
  await seedCard(fsOwner);
  await syncCard();
  reset();
};

// ---------------------------------------------------------------- Wi-Fi page

function showWeb() {
  $('#web').hidden = !webOn;
  $('#webState').textContent = webOn
    ? 'Upload page: on, at http://ereader.local/ (here below, through the simulator).'
    : 'Upload page: off. Start it on the reader with MENU → Wi‑Fi upload.';
  const frame = $('#webFrame');
  if (webOn && swReady) { if (!frame.src) frame.src = 'device/'; }
  else frame.removeAttribute('src');
}

let swReady = false;
async function startServiceWorker() {
  if (!('serviceWorker' in navigator)) { log('No service worker: the upload page cannot be shown.', 'err'); return; }
  navigator.serviceWorker.addEventListener('message', (e) => {
    if (e.data && e.data.kind === 'http') handleHttp(e.data, e.ports[0]);
  });
  navigator.serviceWorker.startMessages();
  try {
    await navigator.serviceWorker.register('sw.js');
    await Promise.race([navigator.serviceWorker.ready, sleep(4000).then(() => { throw new Error('timeout'); })]);
    swReady = true;
    showWeb();
  } catch (e) {
    log('Service worker failed (' + e.message + '): the upload page cannot be shown.', 'err');
  }
}

function handleHttp(req, port) {
  const fail = () => port.postMessage({ status: 504, type: 'text/html', body: new TextEncoder().encode(
    '<!doctype html><meta charset="utf-8"><body style="font:16px system-ui;padding:16px">' +
    '<p><b>ereader.local is not reachable.</b></p><p>Start the upload page on the reader: MENU → Wi‑Fi upload.</p>') });
  if (!M) return fail();
  const id = nextReq++;
  const s = (t) => M.stringToNewUTF8(t || '');
  const pUri = s(req.path), pQuery = s(req.query), pName = s(req.fileName);
  let pData = 0, len = 0;
  if (req.body && req.body.byteLength) {
    len = req.body.byteLength;
    pData = M._malloc(len);
    M.HEAPU8.set(new Uint8Array(req.body), pData);
  }
  pending.set(id, port);
  const ok = M._sim_http_request(id, req.method === 'POST' ? 1 : 0, pUri, pQuery, pName, pData, len);
  for (const p of [pUri, pQuery, pName, pData]) if (p) M._free(p);
  if (!ok) { pending.delete(id); fail(); }
}

// ---------------------------------------------------------------- layout

function fit() {
  const narrow = innerWidth <= 860;
  const availW = (narrow ? innerWidth - 32 : innerWidth - 48 - 420 - 24) - 0;
  const availH = narrow ? innerHeight * 0.8 : innerHeight - 48 - 70;
  // device 95.3 mm + 12 mm for the side labels, 136.8 mm high
  const mm = Math.max(2.4, Math.min(availW / 108, availH / 137, 7));
  document.documentElement.style.setProperty('--mm', mm.toFixed(3) + 'px');
}
window.addEventListener('resize', fit);
fit();

// ---------------------------------------------------------------- start

paint(() => PAPER);
setState('off', 'Loading firmware…');
(async () => {
  await startServiceWorker();
  try {
    await loadWasm();
  } catch (e) {
    setState('off', 'firmware.wasm could not be loaded. Serve this folder over HTTP (pio run -e sim -t serve).');
    log(String(e), 'err');
    return;
  }
  boot({ rtc: null, wakeCause: 0, ext1: 0 });
})();
