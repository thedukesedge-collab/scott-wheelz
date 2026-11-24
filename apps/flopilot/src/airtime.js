// SPDX-License-Identifier: GPL-3.0-or-later
// MAX AIR: air time off jumps, worked out from Refloat's live data (no
// accelerometer in Refloat's realtime data, so it reads the tire). Strict:
// a jump counts only when, the whole time, the board is balancing and a
// footpad is pressed (rider on), Refloat's wheelslip flag says the tire spins
// freely, motor current stays low (tire unloaded), it ends in a landing
// (a motor current spike), lasts 0.3-3 s, and no data went
// missing in between. Home screen badge (Settings › PREFERENCES › MAX AIR),
// tap for this trip's total / jumps, hold to reset; per-board trip records.
// Display only: reads data, never writes anything to the board.

export const AIR = { minS: 0.3, maxS: 3.0, lowA: 15, landA: 25, gapMs: 300, settleMs: 400 };

// the jump detector (no UI). feed(t ms, s) per realtime sample;
// s = { running, pads (bool), slip (bool), amps, erpm }; returns a jump in
// seconds when one lands, else 0
export class AirDetector {
  constructor(cfg = AIR) { this.c = cfg; this.reset(); }
  reset() { this.st = 'ground'; this.t0 = 0; this.last = 0; this.peak = 0; this.ok = true; this.land = 0; }
  feed(t, s) {
    const c = this.c, gap = this.last && t - this.last > c.gapMs;
    this.last = t;
    if (this.st === 'cool') { if (!s.slip) this.st = 'ground'; return 0; }  // wait for the slip to end
    if (this.st === 'ground') {
      if (s.running && s.pads && s.slip && Math.abs(s.amps) < c.lowA) {
        this.st = 'air'; this.t0 = t; this.peak = Math.abs(s.erpm); this.ok = true; this.land = 0;
      }
      return 0;
    }
    // in the air (or just landed, waiting for the slip flag to clear)
    if (gap || !s.running || !s.pads) this.ok = false;  // data gap / rider off: doesn't count
    if (this.st === 'air') {
      // STRICT landing: a current spike (the tire hits the ground and the
      // balance loop catches it); ERPM snapping back alone isn't enough (a
      // spin-out regaining grip does that too)
      const spike = Math.abs(s.amps) >= c.landA;
      if (spike && t - this.t0 >= 100) { this.st = 'landed'; this.land = t; }
      else if (!s.slip && Math.abs(s.amps) >= c.lowA && !spike) this.ok = false;  // loaded again without a landing
      else if (!s.slip && t - this.t0 > 200) { this.st = 'ground'; return 0; }  // slip ended, no landing: not air
      if (t - this.t0 > c.maxS * 1000) { this.st = 'cool'; return 0; }  // too long: not a jump
      return 0;
    }
    // landed: report once things settle
    if (t - this.land >= c.settleMs || !s.slip) {
      const sec = (this.land - this.t0) / 1000, ok = this.ok && sec >= c.minS && sec <= c.maxS;
      this.st = 'ground';
      return ok ? Math.round(sec * 100) / 100 : 0;
    }
    return 0;
  }
}

// ---------------------------------------------------------------- app part
let link, toast;
const LS = 'fpAir', LS_ON = 'fpAirOn';
const load = () => { try { return JSON.parse(localStorage.getItem(LS)) || {}; } catch (e) { return {}; } };
const keep = o => { try { localStorage.setItem(LS, JSON.stringify(o)); } catch (e) {} };
export const airOn = () => { try { return localStorage.getItem(LS_ON) === '1'; } catch (e) { return false; } };
const key = () => (link.live ? link.deviceId || link.name || 'board' : 'demo');
const fresh = () => ({ max: 0, total: 0, jumps: 0, start: Date.now() });
function board(o = load()) { const k = key(); if (!o[k]) o[k] = { name: link.live ? link.name || 'Board' : 'Demo', cur: fresh(), trips: [], best: 0 }; return o[k]; }
const fmt = s => s.toFixed(2) + 's';
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

// trip over (board power-cycled, trip went back to 0): keep it in the records
function closeTrip(o, b) {
  if (b.cur.jumps) { b.trips.unshift(Object.assign({ end: Date.now() }, b.cur)); b.trips = b.trips.slice(0, 50); }
  b.cur = fresh();
}

function landed(sec) {
  const o = load(), b = board(o);
  b.cur.jumps++; b.cur.total = Math.round((b.cur.total + sec) * 100) / 100;
  const rec = sec > b.cur.max;
  if (rec) b.cur.max = sec;
  const best = sec > (b.best || 0);
  if (best) b.best = sec;
  keep(o);
  if (airOn()) toast('Air ' + fmt(sec) + (best ? ' · new board record!' : rec ? ' · new trip max' : ''));
  draw();
}

function badge() {
  let el = document.getElementById('pkAir');
  if (!el) {
    const wrap = document.getElementById('speedoWrap'), ref = document.getElementById('pkSpeed');
    if (!wrap || !ref) return null;
    el = document.createElement('div');
    el.className = 'peak nf'; el.id = 'pkAir';
    el.innerHTML = '<div class="v"></div><div class="l">MAX AIR</div>';
    wrap.appendChild(el);
    let t = null, held = false;
    el.addEventListener('pointerdown', () => { held = false; t = setTimeout(() => { held = true; resetTrip(); }, 550); });
    el.addEventListener('pointerup', () => { clearTimeout(t); if (!held) details(); });
    el.addEventListener('pointerleave', () => clearTimeout(t));
    el.addEventListener('contextmenu', e => e.preventDefault());
  }
  return el;
}
// same size as TOP SPEED / MAX DUTY, centered between them
function place(el) {
  const ref = document.getElementById('pkSpeed'), duty = document.getElementById('pkDuty');
  if (!ref) return;
  const cs = getComputedStyle(ref), w = ref.offsetWidth;
  el.style.width = cs.width; el.style.height = cs.height; el.style.paddingTop = cs.paddingTop;
  el.style.left = '50%'; el.style.transform = 'translateX(-50%)'; el.style.top = cs.top;
  for (const k of ['.v', '.l']) {
    const a = ref.querySelector(k), b = el.querySelector(k), s = getComputedStyle(a);
    b.style.fontSize = s.fontSize; b.style.lineHeight = s.lineHeight;
  }
  // the max speed badges sit just under it while MAX AIR is on
  for (const id of ['msBadge', 'msOffBadge']) {
    const m = document.getElementById(id);
    if (m) m.style.top = (ref.offsetTop + ref.offsetHeight + 4) + 'px';
  }
  if (duty && w) el.dataset.w = w;
}
function draw() {
  const el = badge();
  if (!el) return;
  const on = airOn();
  el.style.display = on ? '' : 'none';
  if (!on) {
    for (const id of ['msBadge', 'msOffBadge']) { const m = document.getElementById(id); if (m) m.style.top = '-6px'; }  // their own spot
    return;
  }
  place(el);
  el.querySelector('.v').textContent = fmt(board().cur.max);
}
function resetTrip() { const o = load(), b = board(o); b.cur.max = 0; keep(o); draw(); toast('Max air reset'); }

function overlay(html) {
  let o = document.getElementById('airOv');
  if (!o) {
    o = document.createElement('div'); o.id = 'airOv';
    o.style.cssText = 'position:fixed;inset:0;z-index:85;background:rgba(4,2,10,.85);display:flex;align-items:center;justify-content:center';
    document.body.appendChild(o);
  }
  o.innerHTML = '<div style="width:min(92vw,380px);max-height:80vh;overflow:auto;background:#0b0718;border:2px solid #b14cff;border-radius:14px;box-shadow:0 0 22px rgba(177,76,255,.55);padding:14px;color:#ead6ff;font:13px/1.45 -apple-system,system-ui,sans-serif">' + html + '</div>';
  o.onclick = e => { if (e.target === o || e.target.dataset.x) o.remove(); };
  return o;
}
const H3 = t => '<h3 style="margin:0 0 10px;text-align:center;font:700 15px Orbitron,monospace;letter-spacing:2px;color:#39ff7a;text-shadow:0 0 8px rgba(57,255,122,.7)">' + t + '</h3>';
const STAT = (v, l) => '<div style="flex:1;text-align:center;border:2px solid #b14cff;border-radius:8px;padding:6px 2px"><div style="font:700 17px Orbitron,monospace;color:#39ff7a">' + v + '</div><div style="font:10px Orbitron,monospace;color:#c98bff">' + l + '</div></div>';
function details() {
  const b = board(), c = b.cur;
  overlay(H3('AIR TIME') + '<div style="display:flex;gap:6px">' + STAT(fmt(c.max), 'MAX AIR') + STAT(fmt(c.total), 'TOTAL AIR') + STAT(c.jumps, 'JUMPS') + '</div>'
    + '<p style="text-align:center;font-size:11px;color:#b98cff;margin:8px 0 0">This trip. Board record: ' + fmt(b.best || 0) + '. Hold the badge to reset max air; past trips: Settings › PREFERENCES › AIR RECORDS.</p>'
    + '<div style="text-align:center;margin-top:10px"><span data-x="1" style="display:inline-block;padding:8px 22px;border-radius:999px;border:2px solid #39ff7a;color:#39ff7a;font:700 11px Orbitron,monospace">CLOSE</span></div>');
}

// Settings › PREFERENCES: MAX AIR on/off + AIR RECORDS (per board)
function addPrefs() {
  const box = document.getElementById('modalBox');
  const pg = box && box.querySelector('[data-pg="1"]');
  if (!pg || pg.querySelector('#airCard')) return;
  const card = document.createElement('div');
  card.className = 'card'; card.id = 'airCard';
  pg.appendChild(card);
  const paint = () => {
    const o = load();
    const boards = Object.values(o).filter(b => b.trips.length || b.cur.jumps);
    card.innerHTML = '<b>MAX AIR</b><div class="row"><div class="l">Show MAX AIR on the home screen</div><div class="sw' + (airOn() ? ' on' : '') + '" id="airSw"></div></div>'
      + '<p>Air time off jumps, read from the tire: the board must be balancing with your feet on the pads, the tire spinning free, then a landing. 0.3 s and up. Just for fun: it can miss small hops.</p>'
      + '<b style="margin-top:6px">AIR RECORDS</b>'
      + (boards.length ? boards.map(b => '<div style="margin:6px 0 2px;font:700 11px Orbitron,monospace;color:#39ff7a">' + esc(b.name) + ' · BEST ' + fmt(b.best || 0) + '</div>'
        + [b.cur.jumps ? Object.assign({ now: true }, b.cur) : null, ...b.trips].filter(Boolean).slice(0, 20).map(t => '<div style="display:flex;justify-content:space-between;font-size:11px;border-top:1px solid #3a2266;padding:3px 0"><span>' + (t.now ? 'This trip' : new Date(t.start).toLocaleDateString([], { month: 'short', day: 'numeric' })) + '</span><span>max ' + fmt(t.max) + '</span><span>total ' + fmt(t.total) + '</span><span>' + t.jumps + ' jumps</span></div>').join('')).join('')
        : '<p>No jumps yet.</p>')
      + (boards.length ? '<div style="text-align:center;margin-top:6px"><span class="nb" id="airClr">CLEAR RECORDS</span></div>' : '');
    card.querySelector('#airSw').onclick = () => { try { localStorage.setItem(LS_ON, airOn() ? '0' : '1'); } catch (e) {} paint(); draw(); };
    const clr = card.querySelector('#airClr');
    if (clr) clr.onclick = () => { if (clr.dataset.sure) { keep({}); paint(); draw(); toast('Air records cleared'); } else { clr.dataset.sure = 1; clr.textContent = 'TAP AGAIN TO CLEAR'; } };
  };
  paint();
}

export function initAir(l, toastFn) {
  link = l; toast = toastFn;
  const det = new AirDetector();
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(addPrefs, 0));
  let lastRt = 0, lastTrip = null, lastKey = null, demoT = 0;
  // samples: each new realtime packet (live), or every 50 ms (demo)
  setInterval(() => {
    const S = link.S || window.__sim, b = link.board;
    if (!S) return;
    const k = link.live || link.demo ? key() : null;
    if (k !== lastKey) { lastKey = k; det.reset(); lastTrip = null; draw(); }
    if (!k) return;
    let t;
    if (link.live && b) { if (!b.lastRt || b.lastRt === lastRt) return; t = lastRt = b.lastRt; }
    else t = demoT += 50;
    // trip went back to zero (board power-cycled): new trip
    if (Number.isFinite(S.trip)) {
      if (lastTrip !== null && S.trip < lastTrip - 0.05) { const o = load(); closeTrip(o, board(o)); keep(o); draw(); }
      lastTrip = S.trip;
    }
    const sec = det.feed(t, { running: S.mode === 'running', pads: !!(S.fpL || S.fpR), slip: !!S.wheelslip, amps: S.motorA || 0, erpm: S.erpm || 0 });
    if (sec) landed(sec);
  }, 50);
  setInterval(draw, 1000);
  window.addEventListener('resize', () => setTimeout(draw, 50));
}
