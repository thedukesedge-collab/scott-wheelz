// FloPilot app layer: Bluetooth (VESC Express, Nordic UART) +
// a CONNECT button on top of the simulator UI. While a board is connected the
// simulator shows the board's real data (window.RF_LINK.live); otherwise it
// keeps running as the demo board.
import { BleClient } from '@capacitor-community/bluetooth-le';
import { Board } from './board.js';

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
  apply(S, now) {
    this.S = S;
    if (this.board) this.board.apply(S, now);
  },
};
window.RF_LINK = link;

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

async function connect(deviceId, name) {
  setPill('CONNECTING…', 'busy');
  await BleClient.connect(deviceId, () => onDisconnected());
  link.deviceId = deviceId;
  link.name = name || 'BOARD';
  try {
    const mtu = await BleClient.getMtu(deviceId);
    link.chunk = Math.max(20, Math.min(244, mtu - 3));
  } catch (e) {
    link.chunk = 20;
  }
  link.board = new Board({ send });
  await BleClient.startNotifications(deviceId, NUS, TX, dv => {
    link.board && link.board.receive(new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength));
  });
  try { localStorage.setItem(SAVED, JSON.stringify({ deviceId, name: link.name })); } catch (e) {}
  link.poll = setInterval(() => {
    const b = link.board;
    if (!b) return;
    b.tick(link.S || window.__sim);
    const was = link.live;
    link.live = b.live;
    if (link.live !== was || !link.live) updatePill();
  }, 100);
  updatePill();
}

function onDisconnected() {
  clearInterval(link.poll);
  link.poll = null;
  link.live = false;
  link.board = null;
  link.deviceId = null;
  setPill('DISCONNECTED · TAP TO RECONNECT', 'off');
}

async function disconnect() {
  const id = link.deviceId;
  onDisconnected();
  if (id) try { await BleClient.disconnect(id); } catch (e) {}
  setPill('CONNECT BOARD', 'off');
}

async function pickAndConnect() {
  try {
    await BleClient.initialize();  // also asks for location: the manifest has no neverForLocation flag
    const dev = await BleClient.requestDevice({ services: [NUS] });
    await connect(dev.deviceId, dev.name);
  } catch (e) {
    console.warn(e);
    setPill('CONNECT BOARD', 'off');
    toastMsg(String(e && e.message || e).includes('cancel') ? 'No board picked' : 'Could not connect: ' + (e && e.message || e));
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
    setPill('CONNECT BOARD', 'off');
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
  if (b && b.packageMissing) return setPill(link.name.toUpperCase() + ' · REFLOAT NOT FOUND', 'warn');
  setPill(link.name.toUpperCase() + ' · STARTING…', 'busy');
}
function toastMsg(m) {
  const t = document.getElementById('toast');
  if (!t) return;
  t.textContent = m; t.style.opacity = 1;
  clearTimeout(toastMsg.h); toastMsg.h = setTimeout(() => (t.style.opacity = 0), 2200);
}

function addUi() {
  const style = document.createElement('style');
  style.textContent = `
    #rmConnect { position: fixed; top: calc(env(safe-area-inset-top, 0px) + 6px); left: 50%; transform: translateX(-50%);
      z-index: 60; font: 700 11px Orbitron, monospace; letter-spacing: 1px; padding: 6px 14px; border-radius: 999px;
      border: 2px solid #b14cff; color: #ead6ff; background: rgba(10,6,20,.85); white-space: nowrap; }
    #rmConnect[data-look=on] { border-color: #39ff7a; color: #39ff7a; box-shadow: 0 0 10px rgba(57,255,122,.45); }
    #rmConnect[data-look=busy] { border-color: #ffd23f; color: #ffd23f; }
    #rmConnect[data-look=warn] { border-color: #ff8a1f; color: #ff8a1f; }
    /* the simulator's drive panel is for the demo board only */
    body.rmLive #simBtn, body.rmLive #sim { display: none !important; }`;
  document.head.appendChild(style);
  pill = document.createElement('div');
  pill.id = 'rmConnect';
  pill.onclick = () => (link.deviceId ? (confirm('Disconnect from ' + link.name + '?') && disconnect()) : pickAndConnect());
  document.body.appendChild(pill);
  setPill('CONNECT BOARD', 'off');
  setInterval(() => document.body.classList.toggle('rmLive', !!link.live), 500);
  reconnectSaved();
}

if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', addUi);
else addUi();
