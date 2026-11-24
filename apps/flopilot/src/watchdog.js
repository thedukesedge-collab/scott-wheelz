// SPDX-License-Identifier: GPL-3.0-or-later
// FloPilot WATCHDOG (gear › WATCHDOG): Ghosting Protection.
//
// What it does
// - Watches ADC1 / ADC2 (Refloat's realtime footpad.adc1 / footpad.adc2)
//   while the app is connected. A stuck-sensor check only counts when the
//   board is stopped, not balancing and parked on its tail / nose (tilted
//   past TAIL degrees): nobody can be riding it then. Warnings need the
//   reading to hold for DUR seconds (default 10), not a spike.
// - Levels per pad: OK, EARLY (> early V), HIGH (> high V), STUCK (at / over
//   the pad's Refloat switch voltage). Both pads STUCK = possible ghosting.
// - Ghosting Pad Emergency (one bad pad, user's own tested workaround): the
//   bad pad's switch voltage to 5.0 V (past Refloat's 3.3 V editor limit, so
//   it can't trip) + Posi (Treat Both Sensors as One) on, so the good pad
//   engages alone. Never automatic: confirm, board stopped, good pad must
//   read normal, bad pad must read under 4 V, both values read back. The
//   original ADC1 / ADC2 / Posi values are kept for RESTORE.
// - Never: both pads off, anything while moving / balancing, claims of being
//   protected without fresh data, clearing history on ACKNOWLEDGE.
// Refloat 1.2.3 footpad logic (src/footpad_sensor.c): pad pressed when
// volts > its switch voltage; one pad at 0 V = that pad ignored (single
// sensor); BOTH at 0 V = "always pressed". Posi: one pad counts as both.
import { registerPlugin, Capacitor } from '@capacitor/core';
import { Rf } from './vesc.js';

const FloLive = registerPlugin('FloLive');
const isIOS = Capacitor.getPlatform() === 'ios';

export const EMERG_V = 5.0;     // emergency switch voltage for the bad pad
const BAD_MAX = 4.0;            // bad pad reading must stay under this (5.0 V would still trip near it)
const RANK = { ok: 0, early: 1, high: 2, crit: 3 };
const WORD = { ok: 'OK', early: 'EARLY', high: 'HIGH', crit: 'STUCK' };
const COLOR = { ok: '#39ff7a', early: '#f5ff3b', high: '#ff8a1f', crit: '#ff2b4f' };
const NAME = { early: 'EARLY WARNING', high: 'HIGH WARNING', crit: 'STUCK' };
export const DEFAULTS = { early: 0.2, high: 1.0, dur: 10, tail: 10 };

// one pad's level from its volts and Refloat switch voltage (0 = pad off)
export function classify(v, sw, t) {
  if (!Number.isFinite(v)) return 'ok';
  if (sw > 0 && v >= sw) return 'crit';
  if (v > t.high) return 'high';
  if (v > t.early) return 'early';
  return 'ok';
}

// the stuck-sensor monitor (no UI): tick() about once a second
export class Monitor {
  constructor(t = DEFAULTS) {
    this.t = Object.assign({}, DEFAULTS, t);
    this.reset();
  }
  reset() {
    this.status = 'nodata';  // nodata | wait | check
    this.pad = [1, 2].map(() => ({ level: 'ok', cand: 'ok', since: 0, v: NaN }));
    this.both = false;
  }
  // inp: { fresh, running, speed (mph), pitch (deg), a1, a2, sw1, sw2 }
  // returns events: [{ pad: 1|2|0 (0 = both), level, v }]
  tick(now, inp) {
    const ev = [];
    if (!inp.fresh) { this.status = 'nodata'; this.pad.forEach(p => { p.cand = p.level; p.since = now; }); return ev; }
    this.pad[0].v = inp.a1; this.pad[1].v = inp.a2;
    const unloaded = !inp.running && Math.abs(inp.speed) < 0.5 && Math.abs(inp.pitch) >= this.t.tail;
    if (!unloaded) { this.status = 'wait'; this.pad.forEach(p => { p.cand = p.level; p.since = now; }); return ev; }
    this.status = 'check';
    [[inp.a1, inp.sw1], [inp.a2, inp.sw2]].forEach(([v, sw], i) => {
      const p = this.pad[i], lvl = classify(v, sw, this.t);
      if (lvl !== p.cand) { p.cand = lvl; p.since = now; }
      if (lvl !== p.level && now - p.since >= this.t.dur * 1000) {
        const up = RANK[lvl] > RANK[p.level];
        p.level = lvl;
        if (up) ev.push({ pad: i + 1, level: lvl, v });
        else if (lvl === 'ok') ev.push({ pad: i + 1, level: 'ok', v });
      }
    });
    const both = this.pad[0].level === 'crit' && this.pad[1].level === 'crit';
    if (both && !this.both) ev.push({ pad: 0, level: 'crit', v: Math.max(inp.a1, inp.a2) });
    this.both = both;
    return ev;
  }
  // seconds the pad has read above normal while checked
  stuckFor(i, now) { const p = this.pad[i]; return this.status === 'check' && p.cand !== 'ok' ? Math.round((now - p.since) / 1000) : 0; }
}

// Ghosting Pad Emergency: the values to write, or why not (pure, tested)
export function emergencyPlan({ bad, a1, a2, sw1, sw2, posi, running, speed, early }) {
  if (bad !== 1 && bad !== 2) return { err: 'Pick the faulty pad' };
  if (running || Math.abs(speed) >= 0.5) return { err: 'Stop the board first (not balancing, not rolling)' };
  const good = bad === 1 ? 2 : 1, gv = good === 1 ? a1 : a2, bv = bad === 1 ? a1 : a2, gsw = good === 1 ? sw1 : sw2;
  if (!Number.isFinite(gv) || !Number.isFinite(bv)) return { err: 'No live footpad readings' };
  if (!(gsw > 0)) return { err: 'ADC' + good + ' is switched off in Refloat (0 V): the board would have no working pad' };
  if (gv > early) return { err: 'ADC' + good + ' (the good pad) also reads high (' + gv.toFixed(2) + ' V). Both pads may be faulty: no single-pad emergency' };
  if (bv >= BAD_MAX) return { err: 'ADC' + bad + ' reads ' + bv.toFixed(2) + ' V: too close to 5.0 V, the switch-off may not hold on this controller' };
  const values = { ['fault_adc' + bad]: EMERG_V, fault_is_dual_switch: true };
  return { good, values, orig: { fault_adc1: sw1, fault_adc2: sw2, fault_is_dual_switch: !!posi } };
}

// ---------------------------------------------------------------- app part
let link, ledSync, toast, mon;
const LS_T = 'fpWd', LS_B = 'fpWdB';
const load = (k, d) => { try { return JSON.parse(localStorage.getItem(k)) || d; } catch (e) { return d; } };
const keep = (k, v) => { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) {} };
const thr = () => Object.assign({}, DEFAULTS, load(LS_T, {}));
const boardKey = () => link.deviceId || link.name || 'board';
function bstore() { const all = load(LS_B, {}); return all[boardKey()] || { hist: [], days: {} }; }
function bsave(o) { const all = load(LS_B, {}); all[boardKey()] = o; keep(LS_B, all); }
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const fmtT = t => new Date(t).toLocaleString([], { month: 'short', day: 'numeric', hour: 'numeric', minute: '2-digit' });
const day = t => new Date(t).toISOString().slice(0, 10);

function live() {
  const b = link.board, S = link.S || window.__sim;
  if (!b || !link.live || !S) return null;
  const fresh = !!b.lastRt && Date.now() - b.lastRt < 2500;
  const g = n => ledSync.get(n);
  const emerg = bstore().emerg;
  // with the emergency on, judge the bad pad against its original switch voltage
  const sw = i => (emerg && emerg.pad === i ? emerg.orig['fault_adc' + i] : g('fault_adc' + i));
  return {
    fresh: fresh && Number.isFinite(S.adc1) && Number.isFinite(S.adc2),
    running: b.pkgState === Rf.sRunning, speed: S.speed || 0, pitch: S.pitch || 0,
    a1: S.adc1, a2: S.adc2, sw1: sw(1), sw2: sw(2),
    cfg1: g('fault_adc1'), cfg2: g('fault_adc2'), posi: g('fault_is_dual_switch'),
  };
}

// ---------- alerts: banner (EARLY / HIGH), window (STUCK), iPhone notification
let lastNote = {};
function notify(title, body, urgent) {
  if (!isIOS || !document.hidden) return;
  FloLive.notify({ title, body, urgent: !!urgent, id: 'wd' + title }).catch(() => {});
}
function banner(text, color) {
  let b = document.getElementById('wdBanner');
  if (!b) {
    b = document.createElement('div'); b.id = 'wdBanner';
    b.style.cssText = 'position:fixed;left:10px;right:10px;top:calc(env(safe-area-inset-top,0px) + 8px);z-index:90;padding:10px 34px 10px 12px;border-radius:12px;background:#0b0718;border:2px solid;font:600 12px/1.35 -apple-system,system-ui,sans-serif;box-shadow:0 0 16px rgba(0,0,0,.6)';
    document.body.appendChild(b);
  }
  b.style.borderColor = color; b.style.color = color;
  b.innerHTML = esc(text) + '<span style="position:absolute;right:10px;top:8px;font-size:16px;color:#c98bff" id="wdBx">✕</span>';
  b.onclick = e => { b.remove(); if (e.target.id !== 'wdBx') openTab(); };
  clearTimeout(banner.h); banner.h = setTimeout(() => b.remove(), 20000);
}
function overlay(html, border = '#ff2b4f') {
  let o = document.getElementById('wdOv');
  if (!o) {
    o = document.createElement('div'); o.id = 'wdOv';
    o.style.cssText = 'position:fixed;inset:0;z-index:95;background:rgba(4,2,10,.88);display:flex;align-items:center;justify-content:center';
    document.body.appendChild(o);
  }
  o.innerHTML = '<div style="width:min(92vw,400px);max-height:84vh;overflow:auto;background:#0b0718;border:2px solid ' + border + ';border-radius:14px;box-shadow:0 0 22px ' + border + '88;padding:14px;color:#ead6ff;font:13px/1.45 -apple-system,system-ui,sans-serif">' + html + '</div>';
  return o;
}
const closeOv = () => { const o = document.getElementById('wdOv'); if (o) o.remove(); };
const H3 = (t, c = '#ff2b4f') => '<h3 style="margin:0 0 10px;text-align:center;font:700 15px Orbitron,monospace;letter-spacing:1px;color:' + c + ';text-shadow:0 0 8px ' + c + 'aa">' + t + '</h3>';
const BTN = (id, t, c = '#b14cff') => '<div id="' + id + '" style="flex:1;min-width:30%;text-align:center;padding:10px 6px;border-radius:999px;font:700 11px Orbitron,monospace;letter-spacing:1px;border:2px solid ' + c + ';color:' + c + '">' + t + '</div>';
const ROW = (...b) => '<div style="display:flex;flex-wrap:wrap;gap:8px;margin-top:10px">' + b.join('') + '</div>';

function record(pad, level, v, extra) {
  const o = bstore();
  o.hist.unshift({ t: Date.now(), pad, level, v: Math.round(v * 100) / 100, ...(extra || {}) });
  o.hist = o.hist.slice(0, 50);
  bsave(o);
}

function onEvent(e) {
  const o = bstore(), emerg = o.emerg;
  if (e.level === 'ok') { if (e.pad) toast('ADC' + e.pad + ' back to normal'); return; }
  // the pad the emergency already switched off: no new alarms (it's known)
  if (emerg && e.pad === emerg.pad) return;
  const goodStuck = emerg && e.pad && e.pad !== emerg.pad && e.level === 'crit';
  record(e.pad, goodStuck ? 'crit-both' : e.pad ? e.level : 'crit-both', e.v);
  const key = e.pad + e.level;
  if (Date.now() - (lastNote[key] || 0) < 10 * 60 * 1000) return;  // no repeats within 10 min
  lastNote[key] = Date.now();
  if (e.pad === 0 || goodStuck) return critical(goodStuck);
  if (e.level === 'crit') {
    notify('⚠️ ADC' + e.pad + ' looks stuck', 'Reads ' + e.v.toFixed(2) + ' V with nobody on the board. Open FloPilot › WATCHDOG.', true);
    const ov = overlay(H3('⚠️ ADC' + e.pad + ' LOOKS STUCK')
      + '<p>ADC' + e.pad + ' reads <b>' + e.v.toFixed(2) + ' V</b> (at or over its switch voltage) for ' + mon.t.dur + ' s while the board is parked on its tail, so nobody should be on it.</p>'
      + '<p>Check the sensor and its plug before riding. If the sensor has failed, WATCHDOG has a Ghosting Pad Emergency setting to get you home on the other pad.</p>'
      + ROW(BTN('wdD', 'VIEW SENSOR DIAGNOSTICS', '#39ff7a'), BTN('wdA', 'ACKNOWLEDGE')));
    ov.querySelector('#wdD').onclick = () => { closeOv(); openTab(); };
    ov.querySelector('#wdA').onclick = () => { ack(); closeOv(); };
    return;
  }
  const txt = 'ADC' + e.pad + ' ' + NAME[e.level] + ': ' + e.v.toFixed(2) + ' V with nobody on the board. Tap for WATCHDOG.';
  banner(txt, COLOR[e.level]);
  notify('ADC' + e.pad + ' ' + NAME[e.level], e.v.toFixed(2) + ' V with nobody on the board (normal 0–' + mon.t.early + ' V).');
}
function ack() { const o = bstore(); if (o.hist[0]) { o.hist[0].ack = Date.now(); bsave(o); } }

function critical(goodStuck) {
  notify('‼️ CRITICAL — POSSIBLE GHOSTING HAZARD', 'Both footpad sensors appear stuck active. Your Onewheel may engage unexpectedly without a rider.', true);
  const ov = overlay(H3('‼️ CRITICAL — POSSIBLE GHOSTING HAZARD')
    + '<p style="text-align:center"><b>' + (goodStuck ? 'The remaining footpad sensor appears stuck active (the other one is already switched off by the emergency setting). Your Onewheel may engage unexpectedly without a rider.' : 'Both footpad sensors appear stuck active. Your Onewheel may engage unexpectedly without a rider.') + '</b></p>'
    + '<p>Stop using the board and disable the Float Package when safe. Keep clear of the wheel; power the board off if you can.</p>'
    + ROW(BTN('wdD', 'VIEW SENSOR DIAGNOSTICS', '#39ff7a'), BTN('wdX', 'DISABLE PACKAGE', '#ff2b4f'), BTN('wdA', 'ACKNOWLEDGE / POSSIBLE FALSE POSITIVE')));
  ov.querySelector('#wdD').onclick = () => { closeOv(); openTab(); };
  ov.querySelector('#wdA').onclick = () => { ack(); closeOv(); toast('Noted. The warning stays in WATCHDOG history'); };
  ov.querySelector('#wdX').onclick = () => {
    const L = live();
    if (!L) return toast('Not connected');
    if (L.running || Math.abs(L.speed) >= 0.5) return toast('The board is balancing or rolling: only when stopped');
    const o2 = overlay(H3('DISABLE PACKAGE?')
      + '<p>Turns Refloat off (its own "Disable" setting): the board will not balance or engage until you enable it again in Settings › SETUP.</p>'
      + ROW(BTN('wdN', 'CANCEL'), BTN('wdY', 'DISABLE', '#ff2b4f')));
    o2.querySelector('#wdN').onclick = () => critical(goodStuck);
    o2.querySelector('#wdY').onclick = () => {
      closeOv();
      const err = ledSync.writeValues({ disabled: true }, 'Package disabled: the board won\'t balance', ok => { if (!ok) toast('The board didn\'t confirm: power it off'); });
      if (err) toast(err);
    };
  };
}

// ---------- daily resting baseline + drift
function sample(L) {
  if (mon.status !== 'check' || !(L.a1 <= mon.t.high && L.a2 <= mon.t.high)) return;
  const o = bstore(), d = day(Date.now());
  const x = o.days[d] || { s1: 0, s2: 0, n: 0 };
  if (x.n >= 600) return;  // enough for today
  x.s1 += L.a1; x.s2 += L.a2; x.n++;
  o.days[d] = x;
  const keys = Object.keys(o.days).sort();
  while (keys.length > 60) delete o.days[keys.shift()];
  if (x.n % 10 === 0 || x.n === 1) bsave(o);  // not every second
}
export function drift(days, today) {
  const ks = Object.keys(days).sort(), t = days[today];
  if (!t || !t.n) return [null, null];
  const prev = ks.filter(k => k < today).slice(-7).map(k => days[k]).filter(x => x.n);
  if (prev.length < 2) return [null, null];
  return [1, 2].map(i => {
    const base = prev.reduce((a, x) => a + x['s' + i] / x.n, 0) / prev.length, now = t['s' + i] / t.n;
    return now - base;
  });
}

// ---------- gear › WATCHDOG
let page = null;
function openTab() {
  const btn = document.getElementById('btnSettings');
  if (btn) btn.click();
  setTimeout(() => { const t = document.querySelector('#modalBox [data-tab="wd"]'); if (t) t.click(); }, 60);
}
function gauge(i, L, now) {
  const v = L ? L['a' + i] : NaN, sw = L ? L['sw' + i] : NaN, emerg = bstore().emerg;
  const p = mon.pad[i - 1], lvl = L && L.fresh ? classify(v, sw, mon.t) : 'ok', c = L && L.fresh ? COLOR[lvl] : '#5c5470';
  const pct = x => Math.max(0, Math.min(100, (x / 3.3) * 100));
  const mark = (x, col) => (x > 0 && x <= 3.3 ? '<div style="position:absolute;top:-3px;bottom:-3px;width:2px;left:' + pct(x) + '%;background:' + col + '"></div>' : '');
  const word = !L || !L.fresh ? '—' : emerg && emerg.pad === i ? 'SWITCHED OFF' : WORD[p.level !== 'ok' ? p.level : lvl];
  const st = mon.stuckFor(i - 1, now);
  return '<div style="flex:1;border:2px solid ' + c + ';border-radius:12px;padding:8px;text-align:center;box-shadow:0 0 10px ' + c + '55">'
    + '<div style="font:700 12px Orbitron,monospace;color:#c98bff">ADC' + i + '</div>'
    + '<div style="font:700 24px Orbitron,monospace;color:' + c + '">' + (L && L.fresh ? v.toFixed(2) : '--') + '<span style="font-size:12px"> V</span></div>'
    + '<div style="position:relative;height:8px;border-radius:4px;background:#1c1235;margin:6px 2px"><div style="height:100%;border-radius:4px;width:' + (L && L.fresh ? pct(v) : 0) + '%;background:' + c + '"></div>'
    + mark(mon.t.early, '#f5ff3b') + mark(mon.t.high, '#ff8a1f') + mark(sw, '#ff2b4f') + '</div>'
    + '<div style="font:700 11px Orbitron,monospace;color:' + c + '">' + word + '</div>'
    + '<div style="font-size:10px;color:#9d8cc0">' + (st ? 'above normal ' + st + ' s' : '&nbsp;') + '</div></div>';
}
function draw() {
  if (!page || !page.isConnected) { page = null; return; }
  const L = live(), now = Date.now(), o = bstore(), emerg = o.emerg, t = mon.t;
  const stat = !L ? ['NO BOARD', 'Connect a board to watch its footpad sensors.', '#5c5470']
    : !L.fresh ? ['NOT MONITORING', 'No footpad data right now (connection lost?). The board is not being watched.', '#ff8a1f']
    : mon.status === 'wait' ? ['WAITING', 'Checks run when the board is stopped and parked on its tail (tilted ' + t.tail + '°+), so nobody is on it.', '#f5ff3b']
    : ['MONITORING', 'Board parked: checking both pads.', '#39ff7a'];
  const [d1, d2] = drift(o.days, day(now));
  const dtxt = [d1, d2].map((d, k) => d === null ? '' : 'ADC' + (k + 1) + ' resting ' + (d >= 0 ? '+' : '') + d.toFixed(2) + ' V vs last week' + (d >= 0.1 ? ' ⚠ rising' : '')).filter(Boolean).join(' · ');
  const days = Object.keys(o.days).sort().slice(-7).reverse().map(k => { const x = o.days[k]; return '<div style="display:flex;justify-content:space-between;font-size:11px"><span>' + k.slice(5) + '</span><span>ADC1 ' + (x.s1 / x.n).toFixed(2) + ' V</span><span>ADC2 ' + (x.s2 / x.n).toFixed(2) + ' V</span></div>'; }).join('');
  const LV = { early: 'EARLY', high: 'HIGH', crit: 'STUCK', 'crit-both': 'BOTH / GHOSTING', emerg: 'EMERGENCY ON', restore: 'RESTORED' };
  const hist = o.hist.length ? o.hist.map(h => '<div style="display:flex;justify-content:space-between;gap:6px;font-size:11px;border-top:1px solid #3a2266;padding:3px 0"><span>' + fmtT(h.t) + '</span><span>' + (h.pad ? 'ADC' + h.pad : 'BOTH') + '</span><span style="color:' + (COLOR[h.level] || '#ff2b4f') + '">' + (LV[h.level] || h.level) + '</span><span>' + (h.v != null ? h.v.toFixed(2) + ' V' : '') + (h.ack ? ' ✓' : '') + '</span></div>').join('') : '<p>No warnings yet.</p>';
  const single = [1, 2].filter(i => RANK[mon.pad[i - 1].level] >= 2), canEm = !emerg && single.length === 1 && !mon.both;
  page.innerHTML = '<div class="card"><b>GHOSTING PROTECTION</b>'
    + '<div style="text-align:center;font:700 12px Orbitron,monospace;color:' + stat[2] + '">' + stat[0] + '</div><p>' + stat[1] + '</p>'
    + '<div style="display:flex;gap:8px">' + gauge(1, L, now) + gauge(2, L, now) + '</div>'
    + '<p style="margin-top:8px">Switch voltage: ADC1 ' + (L && Number.isFinite(L.cfg1) ? L.cfg1.toFixed(2) : '--') + ' V · ADC2 ' + (L && Number.isFinite(L.cfg2) ? L.cfg2.toFixed(2) : '--') + ' V · Posi ' + (L && L.posi !== undefined ? (L.posi ? 'ON' : 'OFF') : '--') + '</p>'
    + '<p>Normal 0–' + t.early + ' V · Early > ' + t.early + ' V · High > ' + t.high + ' V · Stuck ≥ switch voltage · ' + t.dur + ' s</p></div>'
    + (emerg ? emergCard(emerg, L) : '<div class="card"><b>GHOSTING PAD EMERGENCY</b><p>' + (mon.both ? 'Both pads look stuck: no single-pad emergency. Stop using the board.' : canEm ? 'ADC' + single[0] + ' reads high. If that sensor has failed, this switches it off so ADC' + (single[0] === 1 ? 2 : 1) + ' engages alone.' : 'Shows up when one pad reads HIGH or STUCK while the board is parked.') + '</p>'
      + '<div style="text-align:center"><span class="nb" id="wdEm" style="' + (canEm ? 'border-color:#ff2b4f;color:#ff2b4f' : 'opacity:.45') + '">EMERGENCY SETTINGS</span></div></div>')
    + '<div class="card"><b>HISTORY</b>' + hist + (o.hist.length ? '<div style="text-align:center;margin-top:6px"><span class="nb" id="wdClr">CLEAR HISTORY</span></div>' : '') + '</div>'
    + '<div class="card"><b>RESTING VOLTAGE (DAILY)</b>' + (dtxt ? '<p>' + dtxt + '</p>' : '<p>Builds up over a few days of parking the board: a slow rise means a sensor is wearing out.</p>') + days + '</div>'
    + '<div class="card"><b>ADVANCED</b><div id="wdAdv"></div><div style="text-align:center"><span class="nb" id="wdDef">DEFAULTS</span></div></div>';
  const $ = id => page.querySelector('#' + id);
  if ($('wdEm')) $('wdEm').onclick = () => (canEm ? emergency(single[0]) : toast(mon.both ? 'Both pads look stuck: no single-pad emergency' : 'Only when one pad reads HIGH or STUCK while parked'));
  if ($('wdClr')) $('wdClr').onclick = () => { const x = bstore(); x.hist = []; bsave(x); draw(); };
  if ($('wdRs')) $('wdRs').onclick = restore;
  const adv = $('wdAdv');
  [['early', 'EARLY above', 0.05, 0.1, 1.0, ' V'], ['high', 'HIGH above', 0.1, 0.3, 3.0, ' V'], ['dur', 'Hold for', 1, 3, 60, ' s'], ['tail', 'Parked tilt', 1, 5, 30, '°']].forEach(([k, l, st, mn, mx, u]) => {
    const r = document.createElement('div'); r.className = 'row';
    r.innerHTML = '<div class="l">' + l + '</div><span class="nb" data-d="-1">−</span><span style="min-width:62px;text-align:center;color:#39ff7a;font:700 13px Orbitron,monospace">' + t[k] + u + '</span><span class="nb" data-d="1">+</span>';
    r.querySelectorAll('[data-d]').forEach(b => (b.onclick = () => {
      const all = thr(), v = Math.round(Math.max(mn, Math.min(mx, all[k] + st * +b.dataset.d)) * 100) / 100;
      if (k === 'early' && v >= all.high) return toast('EARLY must stay under HIGH');
      if (k === 'high' && v <= all.early) return toast('HIGH must stay over EARLY');
      all[k] = v; keep(LS_T, all); mon.t = all; draw();
    }));
    adv.appendChild(r);
  });
  $('wdDef').onclick = () => { keep(LS_T, {}); mon.t = thr(); draw(); };
  if ($('wdT')) testRows();
}

// ---------- Ghosting Pad Emergency
function emergency(bad) {
  const L = live();
  if (!L || !L.fresh) return toast('No live footpad data');
  const good = bad === 1 ? 2 : 1;
  const ov = overlay(H3('⚠️ EMERGENCY SENSOR CONFIGURATION')
    + '<p><b>This configuration is intended for emergency recovery when one footpad sensor has failed. It reduces rider-presence detection redundancy and may change engagement or stopping behavior. Replace the faulty sensor before your next ride.</b></p>'
    + '<div style="display:flex;gap:6px;margin:6px 0"><span class="wdP" data-p="1"></span><span class="wdP" data-p="2"></span></div>'
    + '<div id="wdPlan"></div>'
    + '<p style="font-size:11px;color:#c98bff">Not a repair and not guaranteed safe for riding. Heel-lift (half-switch) stop is gone; only the good pad tells the board you\'re on it. It works only while the bad pad reads under 5.0 V, so the app checks that first; check it on the bench on your controller.</p>'
    + '<label style="display:flex;gap:8px;align-items:center;margin-top:8px;font-size:12px"><input type="checkbox" id="wdOk"> I understand. The board is stopped and nobody is on it.</label>'
    + ROW(BTN('wdN', 'CANCEL'), BTN('wdY', 'APPLY', '#ff2b4f')), '#ff2b4f');
  let pick = bad;
  const plan = () => {
    const x = live() || {};
    return emergencyPlan({ bad: pick, a1: x.a1, a2: x.a2, sw1: x.cfg1, sw2: x.cfg2, posi: x.posi, running: x.running, speed: x.speed, early: mon.t.early });
  };
  const paint = () => {
    const x = live() || {}, p = plan();
    ov.querySelectorAll('.wdP').forEach(e => {
      const i = +e.dataset.p, on = i === pick;
      e.style.cssText = 'flex:1;text-align:center;padding:8px 4px;border-radius:10px;font:700 11px Orbitron,monospace;border:2px solid ' + (on ? '#ff2b4f' : '#3a2266') + ';color:' + (on ? '#ff2b4f' : '#c98bff');
      e.textContent = 'ADC' + i + (on ? ' FAULTY' : ' OK') + ' · ' + (Number.isFinite(x['a' + i]) ? x['a' + i].toFixed(2) + ' V' : '--');
    });
    const g = pick === 1 ? 2 : 1;
    ov.querySelector('#wdPlan').innerHTML = '<div style="font-size:12px;border:1px solid #3a2266;border-radius:10px;padding:8px">'
      + '<div>Now: ADC1 ' + fmtV(x.cfg1) + ' · ADC2 ' + fmtV(x.cfg2) + ' · Posi ' + (x.posi ? 'ON' : 'OFF') + '</div>'
      + '<div style="color:#39ff7a">New: ADC' + pick + ' ' + EMERG_V.toFixed(2) + ' V (never reached: switched off) · ADC' + g + ' ' + fmtV(x['cfg' + g]) + ' (unchanged) · Posi ON</div>'
      + (p.err ? '<div style="color:#ff2b4f;margin-top:4px">' + esc(p.err) + '</div>' : '<div style="margin-top:4px">Only ADC' + g + ' will engage the board. Your original values are kept for RESTORE.</div>') + '</div>';
  };
  ov.querySelectorAll('.wdP').forEach(e => (e.onclick = () => { pick = +e.dataset.p; paint(); }));
  paint();
  const h = setInterval(() => (document.getElementById('wdOv') ? paint() : clearInterval(h)), 1000);
  ov.querySelector('#wdN').onclick = () => { clearInterval(h); closeOv(); };
  ov.querySelector('#wdY').onclick = () => {
    if (!ov.querySelector('#wdOk').checked) return toast('Tick the box first');
    const p = plan();
    if (p.err) return toast(p.err);
    clearInterval(h); closeOv();
    // originals first (never overwritten by emergency values)
    const o = bstore();
    o.emerg = { pad: pick, orig: p.orig, t: Date.now(), applied: false };
    bsave(o);
    write(p.values, Object.keys(p.values), 'Emergency settings sent, checking…', () => {
      const x = bstore();
      x.emerg.applied = true;
      bsave(x);
      record(pick, 'emerg', (live() || {})['a' + pick] || 0);
      protect();
      toast('Emergency settings saved and read back. Do the PAD TEST before riding');
      openTab();
    }, err => {
      // the board may still have taken it: keep the originals if it did
      const x = bstore(), on = ledSync.cfg && Math.abs(ledSync.get('fault_adc' + pick) - EMERG_V) < 0.01;
      if (on) { x.emerg.applied = true; bsave(x); record(pick, 'emerg', 0); }
      else { delete x.emerg; bsave(x); }
      protect();
      toast(err + (on ? ' (the board shows the emergency value: RESTORE is in WATCHDOG)' : ''));
    });
  };
}
const fmtV = v => (Number.isFinite(v) ? v.toFixed(2) + ' V' : '--');

// write, then wait for the board to send its settings back and compare
function write(values, noLimit, msg, ok, fail) {
  const L = live();
  if (!L || L.running || Math.abs(L.speed) >= 0.5) return fail('Stop the board first');
  const err = ledSync.writeValues(values, msg, done => {
    if (!done) return fail('The board didn\'t confirm. Nothing changed: check in VESC Tool');
    const t0 = Date.now();
    const h = setInterval(() => {
      if (!ledSync.cfg) { if (Date.now() - t0 > 8000) { clearInterval(h); fail('Couldn\'t read the settings back: check them in VESC Tool'); } return; }
      clearInterval(h);
      const bad = Object.entries(values).filter(([k, v]) => { const g = ledSync.get(k); return typeof v === 'boolean' ? !!g !== v : Math.abs(g - v) > 0.01; });
      if (bad.length) fail('Read back different values (' + bad.map(([k]) => k).join(', ') + '): check in VESC Tool');
      else ok();
    }, 300);
  }, { force: true, noLimit });
  if (err) fail(err);
}

function protect() {
  const e = bstore().emerg;
  ledSync.protect = e && e.applied ? new Map([['fault_adc' + e.pad, EMERG_V], ['fault_is_dual_switch', true]]) : null;
}

function emergCard(e, L) {
  const g = e.pad === 1 ? 2 : 1;
  const changed = L && ledSync.cfg && (Math.abs(L['cfg' + e.pad] - EMERG_V) > 0.01 || !L.posi);
  return '<div class="card" style="border-color:#ff2b4f"><b style="color:#ff2b4f">⚠️ EMERGENCY SENSOR CONFIGURATION ON</b>'
    + '<p>ADC' + e.pad + ' switched off (' + EMERG_V.toFixed(1) + ' V), Posi ON since ' + fmtT(e.t) + '. Only ADC' + g + ' engages the board. Replace the faulty sensor before your next ride.</p>'
    + (changed ? '<p style="color:#ff2b4f"><b>The board\'s footpad settings no longer match the emergency values (changed in VESC Tool?). ADC' + e.pad + ' may be active again.</b></p>' : '')
    + '<div style="font:700 11px Orbitron,monospace;color:#c98bff;text-align:center">PAD TEST (board stopped, on its tail)</div><div id="wdT"></div>'
    + '<div style="text-align:center;margin-top:8px"><span class="nb" id="wdRs" style="border-color:#39ff7a;color:#39ff7a">RESTORE ORIGINAL SETTINGS</span></div></div>';
}
// the pad test: press the good pad (on), release (off), touch the bad pad (stays off)
let test = { pressed: false, released: false, badQuiet: true };
function testRows() {
  const e = bstore().emerg, b = link.board, box = page && page.querySelector('#wdT');
  if (!e || !box) return;
  const g = e.pad === 1 ? 2 : 1, st = b ? b.fpState : 0;
  const goodOn = !!(st & (g === 1 ? 1 : 2)), badOn = !!(st & (e.pad === 1 ? 1 : 2));
  if (goodOn) test.pressed = true;
  if (test.pressed && !goodOn) test.released = true;
  if (badOn) test.badQuiet = false;
  const row = (ok, t) => '<div style="font-size:12px;color:' + (ok ? '#39ff7a' : '#c98bff') + '">' + (ok ? '✓ ' : '○ ') + t + '</div>';
  box.innerHTML = row(test.pressed, 'Press ADC' + g + ': the board sees it' + (goodOn ? ' (pressed now)' : ''))
    + row(test.released, 'Release ADC' + g + ': it goes off')
    + row(test.badQuiet && test.released, 'Touch ADC' + e.pad + ': it must stay off' + (test.badQuiet ? '' : ' ✕ ADC' + e.pad + ' still registers: don\'t ride, RESTORE'));
}

function restore() {
  const e = bstore().emerg;
  if (!e) return;
  const ov = overlay(H3('RESTORE ORIGINAL SETTINGS', '#39ff7a')
    + '<p>Puts back ADC1 ' + fmtV(e.orig.fault_adc1) + ', ADC2 ' + fmtV(e.orig.fault_adc2) + ', Posi ' + (e.orig.fault_is_dual_switch ? 'ON' : 'OFF') + '.</p>'
    + '<p><b>Only after the sensor has been inspected, repaired or replaced and tested.</b> A still-faulty pad becomes active again.</p>'
    + ROW(BTN('wdN', 'CANCEL'), BTN('wdY', 'RESTORE', '#39ff7a')), '#39ff7a');
  ov.querySelector('#wdN').onclick = closeOv;
  ov.querySelector('#wdY').onclick = () => {
    closeOv();
    write(e.orig, [], 'Restoring…', () => {
      const x = bstore(); delete x.emerg; bsave(x); protect();
      record(e.pad, 'restore', (live() || {})['a' + e.pad] || 0);
      test = { pressed: false, released: false, badQuiet: true };
      toast('Original settings restored. Test both pads before riding');
      draw();
    }, err => toast(err));
  };
}

function addTab() {
  const box = document.getElementById('modalBox');
  const tabs = box && box.querySelector('.tabs');
  if (!tabs || !box.querySelector('[data-pg="2"]') || box.querySelector('[data-tab="wd"]')) return;
  const t = document.createElement('div');
  t.className = 'pill'; t.dataset.tab = 'wd'; t.textContent = 'WATCHDOG';
  tabs.appendChild(t);
  const pg = document.createElement('div');
  pg.dataset.pg = 'wd';
  pg.style.cssText = 'display:none;max-height:52vh;overflow:auto';
  box.querySelector('[data-pg="2"]').after(pg);
  t.onclick = () => {
    box.querySelectorAll('[data-tab]').forEach(x => x.classList.toggle('on', x === t));
    box.querySelectorAll('[data-pg]').forEach(g => (g.style.display = g === pg ? '' : 'none'));
    page = pg; draw();
  };
  box.querySelectorAll('[data-tab]').forEach(x => x !== t && x.addEventListener('click', () => { pg.style.display = 'none'; t.classList.remove('on'); page = null; }));
}

export function initWatchdog(l, ls, toastFn) {
  link = l; ledSync = ls; toast = toastFn;
  mon = new Monitor(thr());
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(addTab, 0));
  let asked = false, was = null;
  setInterval(() => {
    const L = live();
    const key = link.live ? boardKey() : null;
    if (key !== was) { was = key; mon.reset(); lastNote = {}; test = { pressed: false, released: false, badQuiet: true }; protect(); }
    if (L && !asked && isIOS) { asked = true; FloLive.notifyAsk().catch(() => {}); }
    if (L) {
      for (const e of mon.tick(Date.now(), L)) onEvent(e);
      if (L.fresh) sample(L);
    } else mon.reset();
    if (page) { if (page.style.display === 'none') page = null; else draw(); }
  }, 1000);
}
