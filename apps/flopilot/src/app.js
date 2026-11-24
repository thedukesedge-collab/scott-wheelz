// FloPilot app layer: Bluetooth (VESC Express, Nordic UART) +
// a CONNECT button on top of the simulator UI. While a board is connected the
// simulator shows the board's real data (window.RF_LINK.live); otherwise it
// keeps running as the demo board.
import { BleClient } from '@capacitor-community/bluetooth-le';
import { Board } from './board.js';
import { initLock } from './lock.js';
import { searchBoard, boardName } from './scan.js';
import { LedSync } from './ledsync.js';
import { Sound } from './sound.js';
import { initPkg } from './pkgui.js';
import { initMixer } from './mixer.js';
import { initLights } from './lights.js';
import { initLcmUi } from './lcmui.js';

const NUS = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const RX = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';  // phone -> VESC
const TX = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';  // VESC -> phone
const SAVED = 'fpBoard';

const link = {
  live: false,
  board: null,
  deviceId: null,
  name: '',
  chunk: 20,
  writing: Promise.resolve(),
  poll: null,
  S: null,
  demo: false,  // no board: run the demo board (DEMO pill), otherwise all zeros
  snap: null,   // last readings from the board, kept while the link is down
  get blank() { return !this.live && !this.demo; },
  get held() { return this.blank && !!this.snap; },
  get status() { return this.held ? (this.deviceId ? 'CONNECTING' : 'RECONNECTING') : 'NO BOARD'; },
  apply(S, now) {
    this.S = S;
    if (this.live && this.board) { this.board.apply(S, now); takeSnap(S); }
    else if (this.held) Object.assign(S, this.snap, MOTION_ZERO);
    else if (this.blank) blankBoard(S);
  },
};
window.RF_LINK = link;
// name at the top left: the one you gave it (pencil); until then, with a
// board connected, its Bluetooth name instead of the demo's "Growler"
link.displayName = name => (link.live && (!name || name === 'Growler') ? link.name || name : name);
// LEDS tab <-> Refloat's own LED settings on the board (stock Refloat 1.2.3 too)
const ledSync = new LedSync({
  getP: () => (window.__simP ? window.__simP() : null),
  render: () => { try { window.__simRender && window.__simRender(); } catch (e) { console.warn(e); } },
  toast: m => toastMsg(m),
});
setInterval(() => ledSync.tick(link.live && !link.installing), 1000);
// horn / melodies / MIXER tiles through the motor (VESC firmware tones)
const sound = new Sound(link, m => toastMsg(m));
window.RF_SOUND = sound;
// after a package install: read the config again, set the horn helper up again
link.onPackageChanged = () => { ledSync.reset(link.board); sound.reset(link.board); };
const DEMO = 'fpDemo';
// DEMO always starts off: the app opens on the CONNECT BOARD screen
try { localStorage.removeItem(DEMO); } catch (e) {}

// link lost: keep the board's last readings (trip, battery, temps...) on screen
// and saved on the phone, but show it standing still. Top speed / max duty are
// the app's own ride peaks (saved by the UI), so they carry on too.
const LAST = 'fpLast';
const KEEP = ['volts', 'battPct', 'motorT', 'escT', 'trip', 'life', 'whmi'];
const MOTION_ZERO = {
  mode: 'ready', speed: 0, erpm: 0, duty: 0, motorA: 0, battA: 0, pitch: 0, balance: 0, roll: 0,
  setpoint: 0, fpL: false, fpR: false, wheelslip: false, auto: false, scen: null, nose: false,
};
let snapSaved = 0;
function takeSnap(S) {
  const o = {};
  for (const k of KEEP) o[k] = S[k];
  link.snap = o;
  if (Date.now() - snapSaved > 2000) {
    snapSaved = Date.now();
    try { localStorage.setItem(LAST, JSON.stringify({ t: snapSaved, id: link.deviceId, snap: o })); } catch (e) {}
  }
}
try {
  const l = JSON.parse(localStorage.getItem(LAST) || 'null');
  if (l && l.snap && Date.now() - l.t < 12 * 3600 * 1000) link.snap = l.snap;  // from the last 12 h
} catch (e) {}

// no board connected: every reading at zero, nothing moving
function blankBoard(S) {
  Object.assign(S, {
    mode: 'ready', speed: 0, erpm: 0, duty: 0, motorA: 0, battA: 0, volts: 0, battPct: 0,
    pitch: 0, balance: 0, roll: 0, setpoint: 0, tilt: 0, fpL: false, fpR: false, wheelslip: false,
    motorT: 0, escT: 0, trip: 0, life: 0, whmi: 0, auto: false, scen: null, nose: false,
  });
}

// ---------- transport ----------
function send(bytes) {
  if (!link.deviceId) return;
  const id = link.deviceId;
  for (let i = 0; i < bytes.length; i += link.chunk) {
    const part = bytes.slice(i, i + link.chunk);
    link.writing = link.writing
      .then(() => BleClient.writeWithoutResponse(id, NUS, RX, new DataView(part.buffer, part.byteOffset, part.byteLength)))
      .catch(e => console.warn('write failed', e));
  }
}

let connecting = false;
async function connect(deviceId, name) {
  if (connecting) throw new Error('busy');
  connecting = true;
  try {
    await connectOnce(deviceId, name);
  } finally {
    connecting = false;
  }
}
async function connectOnce(deviceId, name) {
  setPill('CONNECTING…', 'busy');
  await BleClient.connect(deviceId, () => onDisconnected(true), { timeout: 8000 });
  link.deviceId = deviceId;
  link.name = boardName(deviceId, name);
  try {
    const mtu = await BleClient.getMtu(deviceId);
    link.chunk = Math.max(20, Math.min(244, mtu - 3));
  } catch (e) {
    link.chunk = 20;
  }
  link.board = new Board({ send });
  ledSync.reset(link.board);
  await BleClient.startNotifications(deviceId, NUS, TX, dv => {
    link.board && link.board.receive(new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength));
  });
  try { localStorage.setItem(SAVED, JSON.stringify({ deviceId, name: link.name })); } catch (e) {}
  link.poll = setInterval(() => {
    const b = link.board;
    if (!b || link.installing) return;  // package install: only its own packets
    b.tick(link.S || window.__sim);
    const was = link.live;
    link.live = b.live;
    if (link.live !== was || !link.live) updatePill();
  }, 100);
  updatePill();
}

// lost = the board dropped (pocket, range...): keep trying the same board
let retry = null;
let userOff = false;
function onDisconnected(lost) {
  clearInterval(link.poll);
  link.poll = null;
  link.live = false;
  link.board = null;
  ledSync.reset(null);
  link.deviceId = null;
  if (lost && !userOff) {
    setPill('RECONNECTING…', 'busy');
    scheduleRetry();
  } else setPill('CONNECT BOARD', 'off');
}
function scheduleRetry() {
  clearTimeout(retry);
  retry = setTimeout(async () => {
    if (link.deviceId || userOff) return;
    if (!(await reconnectSaved())) scheduleRetry();
  }, 4000);
}
// back from the pocket / lock screen: try right away
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && !link.deviceId && !userOff) reconnectSaved().then(ok => ok || scheduleRetry());
});

async function disconnect() {
  const id = link.deviceId;
  userOff = true;
  clearTimeout(retry);
  if (link.onUserDisconnect) link.onUserDisconnect();
  onDisconnected(false);
  if (id) try { await BleClient.disconnect(id); } catch (e) {}
  setPill('CONNECT BOARD', 'off');
}

async function pickAndConnect() {
  try {
    let last = null;
    try { last = JSON.parse(localStorage.getItem(SAVED) || 'null'); } catch (e) {}
    const dev = await searchBoard({ lastId: last && last.deviceId, toast: toastMsg });
    if (!dev) return;
    userOff = false;
    clearTimeout(retry);
    await connect(dev.deviceId, dev.name);
  } catch (e) {
    console.warn(e);
    if (!link.deviceId) setPill('CONNECT BOARD', 'off');
    toastMsg('Could not connect: ' + (e && e.message || e));
  }
}

async function reconnectSaved() {
  let saved = null;
  try { saved = JSON.parse(localStorage.getItem(SAVED) || 'null'); } catch (e) {}
  if (!saved) return false;
  try {
    await BleClient.initialize();  // also asks for location: the manifest has no neverForLocation flag
    await connect(saved.deviceId, saved.name);
    return true;
  } catch (e) {
    if (!link.deviceId) setPill(link.held && !userOff ? 'RECONNECTING… · TAP TO PICK' : 'CONNECT BOARD', link.held ? 'busy' : 'off');
    return false;
  }
}

// ---------- CONNECT pill (top of the screen) ----------
let pill;
function setPill(text, look) {
  if (!pill) return;
  pill.textContent = text;
  pill.dataset.look = look;
}
function updatePill() {
  const b = link.board;
  if (!link.deviceId) return setPill('CONNECT BOARD', 'off');
  if (link.live) return setPill('● ' + link.name.toUpperCase(), 'on');
  if (b && (b.link === 'can' || b.link === 'probe')) return setPill(link.name.toUpperCase() + ' · FINDING CONTROLLER…', 'busy');
  if (b && b.link === 'nocan') return setPill(link.name.toUpperCase() + ' · NO CONTROLLER ON CAN', 'warn');
  if (b && b.packageMissing) return setPill(link.name.toUpperCase() + ' · REFLOAT NOT FOUND', 'warn');
  setPill(link.name.toUpperCase() + ' · STARTING…', 'busy');
}
function toastMsg(m) {
  const t = document.getElementById('toast');
  if (!t) return;
  t.textContent = m; t.style.opacity = 1;
  clearTimeout(toastMsg.h); toastMsg.h = setTimeout(() => (t.style.opacity = 0), 2200);
}

let demoPill = null;
function addUi() {
  const style = document.createElement('style');
  style.textContent = `
    #rmConnect { position: fixed; top: calc(env(safe-area-inset-top, 0px) + 80px); left: 50%; transform: translateX(-50%);
      z-index: 60; font: 700 11px Orbitron, monospace; letter-spacing: 1px; padding: 6px 14px; border-radius: 999px;
      border: 2px solid #b14cff; color: #ead6ff; background: rgba(10,6,20,.85); white-space: nowrap; }
    #rmConnect[data-look=on] { border-color: #39ff7a; color: #39ff7a; box-shadow: 0 0 10px rgba(57,255,122,.45); }
    #rmConnect[data-look=busy] { border-color: #ffd23f; color: #ffd23f; }
    #rmConnect[data-look=warn] { border-color: #ff8a1f; color: #ff8a1f; }
    /* no board: a bigger CONNECT prompt */
    /* no board (or reconnecting) and no demo: Home Screen and swipe-up window
       dimmed, no board name, taps go to #rmShield */
    body.rmBlank #hud > * { opacity: .28; filter: saturate(.4); transition: opacity .3s; }
    body.rmBlank #mainBg { opacity: .45; }
    body.rmBlank #sheet { opacity: .35; filter: saturate(.4); }
    body.rmBlank #boardName, body.rmBlank #lightsOverride { visibility: hidden; }
    #rmShield { display: none; position: absolute; inset: 0; z-index: 15; }
    body.rmBlank #rmShield { display: block; }
    body.rmModal #rmConnect, body.rmModal #rmDemo { display: none; }
    #rmDemo { font-size: 12px !important; padding: 7px 16px !important; border-color: #39ff7a !important; color: #39ff7a !important; }
    /* settings tabs: wrap onto two rows instead of running off the window */
    .modal .tabs { flex-wrap: wrap; gap: 6px !important; }
    .modal .tabs .pill { padding: 6px 10px; font-size: 11px; }
    body.rmBlank #rmConnect { font-size: 15px; padding: 12px 26px; border-color: #39ff7a; color: #39ff7a;
      box-shadow: 0 0 14px rgba(57,255,122,.5); }
    #rmDemo { position: fixed; left: 50%; transform: translateX(-50%); z-index: 60; font: 700 10px Orbitron, monospace;
      letter-spacing: 1px; padding: 4px 12px; border-radius: 999px; border: 1px solid #7a3fc0; color: #b98cff;
      background: rgba(10,6,20,.85); white-space: nowrap; }
    body.rmLive #rmDemo { display: none; }
    /* the simulator's drive panel is for the demo board only */
    body.rmLive #simBtn, body.rmLive #sim, body.rmBlank #simBtn, body.rmBlank #sim { display: none !important; }`;
  document.head.appendChild(style);
  pill = document.createElement('div');
  pill.id = 'rmConnect';
  pill.onclick = () => (link.deviceId ? (confirm('Disconnect from ' + link.name + '?') && disconnect()) : pickAndConnect());
  document.body.appendChild(pill);
  setPill('CONNECT BOARD', 'off');
  // sit just under the board name row (clear of the battery bar and the buttons)
  demoPill = document.createElement('div');
  demoPill.id = 'rmDemo';
  demoPill.onclick = () => {
    link.demo = !link.demo;
    if (link.demo && link.S) Object.assign(link.S, { battPct: 90, motorT: 19.5, escT: 27.7 });  // demo start values
    refresh();
    // DEMO brings up the simulator's drive panel; stopping it closes the panel
    const sim = document.getElementById('sim');
    if (sim) sim.classList.toggle('on', link.demo);
    toastMsg(link.demo ? 'Demo board: drive it with the SIMULATOR panel' : 'Demo off');
  };
  document.body.appendChild(demoPill);
  // no board and no demo: only CONNECT BOARD and DEMO work; a tap anywhere
  // else on the Home Screen / swipe-up window says so (dialogs, the tour and
  // the board search stay on top and usable)
  const shield = document.createElement('div');
  shield.id = 'rmShield';
  shield.onclick = () => toastMsg('Connect your board, or tap DEMO to try the app');
  (document.getElementById('phone') || document.body).appendChild(shield);
  // the pills stay out of the way while a dialog is open
  const modalEl = document.getElementById('modal');
  const modalOpen = () => !!(modalEl && modalEl.classList.contains('on'));
  if (modalEl) new MutationObserver(() => document.body.classList.toggle('rmModal', modalOpen())).observe(modalEl, { attributes: true, attributeFilter: ['class'] });
  const placePill = () => {
    const row = document.getElementById('nameRow');
    if (!row) return;
    const top = Math.round(row.getBoundingClientRect().bottom + 6);
    pill.style.top = top + 'px';
    demoPill.style.top = (top + pill.offsetHeight + 6) + 'px';
  };
  const refresh = () => {
    document.body.classList.toggle('rmLive', !!link.live);
    document.body.classList.toggle('rmBlank', link.blank);
    demoPill.textContent = link.demo ? 'DEMO ON · TAP TO STOP' : '▶ DEMO';
    placePill();
  };
  refresh();
  setInterval(refresh, 500);
  window.addEventListener('resize', placePill);
  initLock(link, toastMsg);
  initPkg(link, toastMsg);
  initMixer(link, toastMsg);
  initLights(link, ledSync, toastMsg);
  initLcmUi(link, toastMsg);
  reconnectSaved().then(ok => ok || (link.snap && scheduleRetry()));
}

if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', addUi);
else addUi();
