// SPDX-License-Identifier: GPL-3.0-or-later
// LEDS tab <-> Refloat's own LED settings on the board, the way the Refloat
// Glow package does it (ui.qml.in ledSender.toConfig / fromConfig): works with
// stock Refloat 1.2.3 too. Only settings changed in the app since they were
// read are written, and only while the board is stopped.
import { RefloatConfig } from './refcfg.js';
import { Rf } from './vesc.js';

const BARS = ['headlights', 'taillights', 'front', 'rear'];
const NOT_TUNE = ['tiltback_return_speed', 'tiltback_duty', 'tiltback_duty_angle', 'tiltback_duty_speed', 'is_dutybeep_enabled',
  'tiltback_hv_angle', 'tiltback_hv_speed', 'tiltback_lv_angle', 'tiltback_lv_speed', 'persistent_fatal_error', 'disabled',
  'fault_adc1', 'fault_adc2', 'hertz', 'inputtilt_remote_type', 'inputtilt_deadband', 'is_footbeep_enabled', 'is_beeper_enabled',
  'tiltback_hv', 'tiltback_lv', 'tiltback_speed'];
export const isTune = n => !n.startsWith('haptic.') && !n.startsWith('hardware.') && !n.startsWith('leds.') && !n.startsWith('meta.') && !NOT_TUNE.includes(n);
const pct = v => Math.max(0, Math.min(100, Math.round(v * 100)));

// what the app's LED settings mean in Refloat's config
function desired(P) {
  const off = P.rgbOff || {};
  const d = {
    'leds.headlights_on': !!P.ledHead,
    'leds.headlights.brightness': P.ledHeadPct / 100,
    'leds.front.brightness': off[2] ? 0 : P.ledHeadIdlePct / 100,
    'leds.status_idle.brightness': P.ledIdlePct / 100,
    'leds.status.brightness_headlights_on': P.ledRidePct / 100,
    'leds.status.brightness_headlights_off': P.ledRidePct / 100,
    'leds.lights_off_when_lifted': !!P.rgbLifted,
    'leds.status_idle.mode': P.rfMode,
    'leds.status_idle.color1': P.rfColor1,
    'leds.status_idle.color2': P.rfColor2,
    'leds.status_idle.speed': P.rfSpeed,
    'leds.status.idle_timeout': P.rfTimeout,
    'leds.status.show_sensors_while_running': P.rfSens !== false,
    'is_beeper_enabled': P.ctrlBeep !== false,
    'persistent_fatal_error': P.fatalPersist !== false,
  };
  if (typeof P.rfDutyTh === 'number') d['leds.status.duty_threshold'] = P.rfDutyTh / 100;
  // Settings › RIDE › HAPTIC FEEDBACK / PUSHBACK (RF Glow's hapticCfg / pushbackCfg)
  const h = P.hap, b = P.pb;
  if (h) Object.assign(d, { 'haptic.duty.strength': h.str, 'tiltback_duty': h.duty / 100, 'tiltback_speed': h.spd });
  if (b) Object.assign(d, {
    'tiltback_duty_angle': b.dutyAng, 'tiltback_duty_speed': b.dutyRate,
    'tiltback_hv': b.hv, 'tiltback_hv_angle': b.hvAng, 'tiltback_hv_speed': b.hvRate,
    'tiltback_lv': b.lv, 'tiltback_lv_angle': b.lvAng, 'tiltback_lv_speed': b.lvRate,
    'tiltback_return_speed': b.ret, 'tiltback_constant': b.cAng, 'tiltback_constant_erpm': b.cErpm,
    'tiltback_variable': b.vRate, 'tiltback_variable_max': b.vMax, 'tiltback_variable_erpm': b.vErpm,
  });
  (P.rgb || []).forEach((b, k) => {
    const n = 'leds.' + BARS[k] + '.';
    d[n + 'mode'] = b.mode; d[n + 'color1'] = b.c1; d[n + 'color2'] = b.c2; d[n + 'speed'] = b.speed;
    if (k === 1) d[n + 'brightness'] = b.bri;
    if (k === 3) d[n + 'brightness'] = off[3] ? 0 : b.bri;
  });
  return d;
}
const same = (a, b) => typeof a === 'number' && typeof b === 'number' ? Math.abs(a - b) < 1e-4 : a === b;

function fromConfig(P, c) {
  const g = n => c.get(n);
  P.ledHead = g('leds.headlights_on');
  P.ledHeadPct = pct(g('leds.headlights.brightness'));
  P.rgbOff = P.rgbOff || {};
  const fb = g('leds.front.brightness');
  P.rgbOff[2] = fb === 0;
  if (fb > 0) P.ledHeadIdlePct = pct(fb);
  P.ledIdlePct = pct(g('leds.status_idle.brightness'));
  P.ledRidePct = pct(g('leds.status.brightness_headlights_on'));
  P.rgbLifted = g('leds.lights_off_when_lifted');
  P.rfMode = g('leds.status_idle.mode');
  P.rfColor1 = g('leds.status_idle.color1');
  P.rfColor2 = g('leds.status_idle.color2');
  P.rfSpeed = g('leds.status_idle.speed');
  P.rfTimeout = g('leds.status.idle_timeout');
  P.rfDutyTh = pct(g('leds.status.duty_threshold'));
  P.rfSens = g('leds.status.show_sensors_while_running');
  P.ctrlBeep = g('is_beeper_enabled');
  P.fatalPersist = g('persistent_fatal_error');
  const r2 = v => Math.round(v * 100) / 100;
  P.hap = Object.assign(P.hap || {}, { str: r2(g('haptic.duty.strength')), duty: Math.round(g('tiltback_duty') * 100), spd: g('tiltback_speed') });
  if (P.hap.str > 0) P.hap.last = P.hap.str;
  P.pb = Object.assign(P.pb || {}, {
    dutyAng: r2(g('tiltback_duty_angle')), dutyRate: r2(g('tiltback_duty_speed')),
    hv: r2(g('tiltback_hv')), hvAng: r2(g('tiltback_hv_angle')), hvRate: r2(g('tiltback_hv_speed')),
    lv: r2(g('tiltback_lv')), lvAng: r2(g('tiltback_lv_angle')), lvRate: r2(g('tiltback_lv_speed')),
    ret: r2(g('tiltback_return_speed')), cAng: r2(g('tiltback_constant')), cErpm: g('tiltback_constant_erpm'),
    vRate: r2(g('tiltback_variable')), vMax: r2(g('tiltback_variable_max')), vErpm: g('tiltback_variable_erpm'),
  });
  if (P.pb.dutyAng > 0) P.pb.lastAng = P.pb.dutyAng;
  P.rgb = BARS.map((bar, k) => {
    const n = 'leds.' + bar + '.', old = (P.rgb && P.rgb[k]) || {};
    const bri = g(n + 'brightness');
    const b = { mode: g(n + 'mode'), c1: g(n + 'color1'), c2: g(n + 'color2'), speed: g(n + 'speed'), bri: old.bri || 0.5 };
    if (k === 1) b.bri = bri;
    if (k === 3) { P.rgbOff[3] = bri === 0; if (bri > 0) b.bri = bri; }
    return b;
  });
  // which LED view: decided in lights.js from this and the light module's name
  return { hw: g('hardware.leds.mode') };
}

export class LedSync {
  constructor({ getP, render, toast }) {
    this.getP = getP; this.render = render; this.toast = toast;
    this.reset(null);
  }

  reset(board) {
    this.board = board;
    this.cfg = null;
    this.hw = null;        // hardware.leds.mode: 0 off, 1 internal, 2 external, 3 both
    this.last = null;      // desired() right after the last read / write
    this.pending = null;   // config bytes sent, waiting for the board's OK
    this.asked = 0;
    this.changedAt = 0;
    this.lastSeen = null;
    this.warned = false;
    if (board) {
      board.onConfig = bytes => this.onConfig(bytes);
      board.onConfigAck = () => this.onAck();
    }
  }

  onConfig(bytes) {
    const c = new RefloatConfig(bytes);
    if (!c.ok) {
      this.cfg = null;
      if (!this.warned) { this.warned = true; this.toast("LED settings: this board's Refloat version isn't supported yet"); }
      return;
    }
    const P = this.getP();
    if (!P) return;
    // an app change waiting to be written is newer: keep it
    if (this.last && Object.entries(desired(P)).some(([k, v]) => !same(v, this.last[k]))) { this.cfg = c; return; }
    this.cfg = c;
    this.hw = fromConfig(P, c).hw;
    this.last = desired(P);
    this.lastSeen = JSON.stringify(this.last);
    this.render();
  }

  onAck() {
    if (!this.pending) return;
    const p = this.pending;
    this.cfg = p.cfg;
    this.last = p.want;
    this.pending = null;
    this.toast(p.msg || 'Saved to the board');
    if (p.reread && this.board) { this.cfg = null; this.asked = 0; }  // read it back (the app's view follows the board)
    if (p.done) p.done(true);
  }

  // any other Refloat settings (a tune, package on/off, remote tilt): patch
  // only these, board stopped only. Returns '' or why not.
  writeValues(values, msg, done) {
    const b = this.board;
    if (!b || !this.cfg) return "Refloat's settings aren't loaded yet";
    if (b.pkgState === Rf.sRunning) return 'Stop the board first';
    if (this.pending) return 'Busy saving, try again in a moment';
    const c = this.cfg.copy();
    let n = 0;
    for (const [k, v] of Object.entries(values)) if (c.has(k) && c.set(k, v)) n++;
    if (!n) { if (done) done(true); this.toast(msg || 'Already set'); return ''; }
    this.pending = { cfg: c, want: this.last, t: Date.now(), msg, reread: true, done };
    b.setConfig(c.raw);
    return '';
  }
  get(name) { try { return this.cfg ? this.cfg.get(name) : undefined; } catch (e) { return undefined; } }

  // a tune: every Refloat setting except LEDs, haptic, hardware and RF Glow's
  // not-a-tune list (as RF Glow's tuneManager.paramIsTune)
  tuneValues() {
    if (!this.cfg) return null;
    const out = {};
    for (const [n, e] of this.cfg.idx) if (isTune(n) && e.f.t !== 3) out[n] = this.cfg.get(n);
    return out;
  }

  // every second while connected
  tick(live) {
    const b = this.board;
    if (!b || !live) return;
    const now = Date.now();
    if (!this.cfg) {
      if (!this.warned && now - this.asked > 3000) { this.asked = now; b.getConfig(); }
      return;
    }
    if (this.pending) {
      if (now - this.pending.t > 4000) { const p = this.pending; this.pending = null; this.asked = 0; this.cfg = null; if (p.done) p.done(false); }  // no OK: read it again
      return;
    }
    const P = this.getP();
    if (!P || !this.last) return;
    const want = desired(P), seen = JSON.stringify(want);
    if (seen !== this.lastSeen) { this.lastSeen = seen; this.changedAt = now; return; }
    const keys = Object.keys(want).filter(k => this.cfg.has(k) && !same(want[k], this.last[k]));
    if (!keys.length || now - this.changedAt < 1500) return;
    // safety: config writes only with the board stopped (as the package does)
    if (b.pkgState === Rf.sRunning) return;
    const c = this.cfg.copy();
    let changed = false;
    for (const k of keys) changed = c.set(k, want[k]) || changed;
    if (!changed) { this.last = want; return; }
    this.pending = { cfg: c, want, t: now };
    b.setConfig(c.raw);
  }
}
