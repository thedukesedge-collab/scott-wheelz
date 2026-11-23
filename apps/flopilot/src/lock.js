// SPDX-License-Identifier: GPL-3.0-or-later
// FloPilot lock-screen ride card (iOS Live Activity, native part in native/):
// settings page (gear icon > LOCK SCREEN), start / update / end, light button.
import { registerPlugin, Capacitor } from '@capacitor/core';

const FloLive = registerPlugin('FloLive');
const KEY = 'fpLock';
const MAX = 4;
const isIOS = Capacitor.getPlatform() === 'ios';

const tempU = P => (P && P.tempF ? '°F' : '°C');
const temp = (v, P) => Math.round(P && P.tempF ? v * 1.8 + 32 : v);
export const FIELDS = {
  speed: ['SPEED', (S) => Math.round(S.speed), () => 'mph'],
  batt: ['BATTERY', (S) => Math.round(S.battPct), () => '%'],
  pkDuty: ['MAX DUTY', (S, P) => Math.round((P ? P.peakDuty : 0) * 100), () => '%'],
  pkSpeed: ['TOP SPEED', (S, P) => (P ? P.peakSpeed : 0).toFixed(1), () => 'mph'],
  trip: ['TRIP', (S) => S.trip.toFixed(1), () => 'mi'],
  volts: ['VOLTAGE', (S) => S.volts.toFixed(1), () => 'V'],
  duty: ['DUTY', (S) => Math.round(S.duty * 100), () => '%'],
  life: ['ODOMETER', (S) => Math.round(S.life), () => 'mi'],
  motorT: ['MOTOR TEMP', (S, P) => temp(S.motorT, P), tempU],
  escT: ['CONTROLLER TEMP', (S, P) => temp(S.escT, P), tempU],
};
const LIGHTS = { off: ['OFF', 0, ''], all: ['ALL LIGHTS', 1, 'LIGHTS'], head: ['HEADLIGHTS', 2, 'HEADLIGHT'] };

let cfg = { auto: true, fields: ['speed', 'batt', 'pkDuty', 'pkSpeed'], light: 'all' };
try { cfg = Object.assign(cfg, JSON.parse(localStorage.getItem(KEY) || '{}')); } catch (e) {}
cfg.fields = cfg.fields.filter(f => FIELDS[f]).slice(0, MAX);
const save = () => { try { localStorage.setItem(KEY, JSON.stringify(cfg)); } catch (e) {} };

let link = null;
let toast = () => {};
let can = { supported: false, button: false };
let active = false;
let starting = false;
let stopped = false;  // STOP pressed: no auto start until the next connect
let heldSince = 0;
let wasLive = false;

function state() {
  const S = link.S || window.__sim;
  const P = window.__simP ? window.__simP() : null;
  const items = cfg.fields.map(f => {
    const [label, val, unit] = FIELDS[f];
    let v = '-';
    try { v = String(val(S, P)); } catch (e) {}
    return { label, value: v, unit: unit(P) };
  });
  const b = link.board;
  const mask = LIGHTS[cfg.light][1];
  return {
    items,
    live: !!link.live,
    status: link.live ? 'LIVE' : 'RECONNECTING',
    lightsOn: !!(b && mask && (b.lights & mask)),
  };
}

async function start() {
  if (!isIOS || starting) return;
  starting = true;
  try {
    await FloLive.start(Object.assign({ board: link.name || 'Board', lightLabel: can.button ? LIGHTS[cfg.light][2] : '' }, state()));
    active = true;
  } catch (e) {
    console.warn('lock screen start', e);
  }
  starting = false;
}
async function end() {
  active = false;
  if (isIOS) try { await FloLive.end(); } catch (e) {}
}

function toggleLights() {
  const b = link.board;
  const mask = LIGHTS[cfg.light][1];
  if (!b || !link.live || !mask) return toast('No board connected');
  const on = !(b.lights & mask);
  b.lightsSet(mask, on ? mask : 0);
  b.lights = (b.lights & ~mask) | (on ? mask : 0);  // shown right away; the board's answer confirms it
  if (active) FloLive.update(state()).catch(() => {});
}

function tick() {
  if (!can.supported) return;
  if (link.live) {
    heldSince = 0;
    if (!wasLive) stopped = false;  // a new connection
    if (!active && cfg.auto && !stopped) start();
  } else if (active) {
    if (!heldSince) heldSince = Date.now();
    // no board for 5 minutes (powered off / out of range): close the card
    if (!link.held || Date.now() - heldSince > 5 * 60 * 1000) end();
  }
  wasLive = link.live;
  if (active) FloLive.update(state()).then(r => { if (r && r.active === false) active = false; }).catch(() => {});
}

// ---------- settings page: gear icon > LOCK SCREEN ----------
function page(pg) {
  const sw = cfg.auto ? ' on' : '';
  pg.innerHTML = '<div class="card"><b>LOCK SCREEN</b><p>A live ride card on the lock screen and Dynamic Island while a board is connected.</p>'
    + (isIOS ? (can.supported ? '' : '<p style="color:#ffd23f">Turn on Live Activities for FloPilot in iPhone Settings.</p>') : '<p>iPhone only.</p>')
    + '<div class="row"><div class="l">Start when a board connects</div><div class="sw' + sw + '" id="lkAuto"></div></div>'
    + '<div class="pills"><span class="pill on" id="lkNow">START NOW</span><span class="pill" id="lkStop">STOP</span></div></div>'
    + '<div class="card"><b>SHOW (UP TO 4)</b><p>Tap to add or remove. Number 1 is shown big.</p><div class="pills" id="lkFields"></div></div>'
    + '<div class="card"><b>LIGHT BUTTON</b><p>What the lock-screen button switches' + (can.button || !isIOS ? '' : ' (needs iOS 17)') + '.</p><div class="pills" id="lkLight"></div></div>';
  const $ = id => pg.querySelector('#' + id);
  const draw = () => {
    $('lkFields').innerHTML = Object.keys(FIELDS).map(f => {
      const n = cfg.fields.indexOf(f);
      return '<span class="pill' + (n >= 0 ? ' on' : '') + '" data-f="' + f + '">' + (n >= 0 ? (n + 1) + ' · ' : '') + FIELDS[f][0] + '</span>';
    }).join('');
    $('lkLight').innerHTML = Object.keys(LIGHTS).map(k => '<span class="pill' + (cfg.light === k ? ' on' : '') + '" data-l="' + k + '">' + LIGHTS[k][0] + '</span>').join('');
    pg.querySelectorAll('[data-f]').forEach(x => x.onclick = () => {
      const f = x.dataset.f, n = cfg.fields.indexOf(f);
      if (n >= 0) cfg.fields.splice(n, 1);
      else if (cfg.fields.length >= MAX) return toast('Up to 4: tap one to remove it first');
      else cfg.fields.push(f);
      save(); draw(); restart();
    });
    pg.querySelectorAll('[data-l]').forEach(x => x.onclick = () => { cfg.light = x.dataset.l; save(); draw(); restart(); });
  };
  draw();
  $('lkAuto').onclick = () => { cfg.auto = !cfg.auto; save(); $('lkAuto').classList.toggle('on', cfg.auto); };
  $('lkNow').onclick = () => {
    if (!isIOS) return toast('iPhone only');
    if (!link.live) return toast('Connect a board first');
    stopped = false; start().then(() => toast(active ? 'Lock screen card on' : 'Could not start: check Live Activities in iPhone Settings'));
  };
  $('lkStop').onclick = () => { stopped = true; end(); toast('Lock screen card off'); };
}
// the light button and board name are fixed when a card starts: start it again
function restart() { if (active) { end().then(start); } }

function addTab() {
  const box = document.getElementById('modalBox');
  const tabs = box && box.querySelector('.tabs');
  if (!tabs || !box.querySelector('[data-pg="2"]') || box.querySelector('[data-tab="lk"]')) return;
  const t = document.createElement('div');
  t.className = 'pill'; t.dataset.tab = 'lk'; t.textContent = 'LOCK SCREEN';
  tabs.appendChild(t);
  const pg = document.createElement('div');
  pg.dataset.pg = 'lk';
  pg.style.cssText = 'display:none;max-height:52vh;overflow:auto';
  box.querySelector('[data-pg="2"]').after(pg);
  t.onclick = () => {
    box.querySelectorAll('[data-tab]').forEach(x => x.classList.toggle('on', x === t));
    box.querySelectorAll('[data-pg]').forEach(g => (g.style.display = g === pg ? '' : 'none'));
  };
  box.querySelectorAll('[data-tab]').forEach(x => x !== t && x.addEventListener('click', () => { pg.style.display = 'none'; t.classList.remove('on'); }));
  page(pg);
}

export async function initLock(l, toastFn) {
  link = l;
  toast = toastFn;
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(addTab, 0));
  if (isIOS) {
    try { can = await FloLive.supported(); } catch (e) { console.warn('FloLive', e); }
    try { await FloLive.addListener('lightToggle', toggleLights); } catch (e) {}
    try { await FloLive.end(); } catch (e) {}  // a card left over from the last run
  }
  setInterval(tick, 1000);
  // user disconnect: close the card
  link.onUserDisconnect = () => { stopped = true; end(); };
}
