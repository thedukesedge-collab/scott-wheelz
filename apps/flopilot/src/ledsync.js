// SPDX-License-Identifier: GPL-3.0-or-later
// LEDS tab <-> Refloat's own LED settings on the board, the way the Refloat
// Glow package does it (ui.qml.in ledSender.toConfig / fromConfig): works with
// stock Refloat 1.2.3 too. Only settings changed in the app since they were
// read are written, and only while the board is stopped.
import { RefloatConfig } from './refcfg.js';
import { Rf } from './vesc.js';

const BARS = ['headlights', 'taillights', 'front', 'rear'];
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
  P.rgb = BARS.map((bar, k) => {
    const n = 'leds.' + bar + '.', old = (P.rgb && P.rgb[k]) || {};
    const bri = g(n + 'brightness');
    const b = { mode: g(n + 'mode'), c1: g(n + 'color1'), c2: g(n + 'color2'), speed: g(n + 'speed'), bri: old.bri || 0.5 };
    if (k === 1) b.bri = bri;
    if (k === 3) { P.rgbOff[3] = bri === 0; if (bri > 0) b.bri = bri; }
    return b;
  });
  // which LED view: Refloat-driven strips (Internal) or a light module (External)
  const hw = g('hardware.leds.mode');
  if (hw & 2) P.simSetup = 'slcm';
  else if (hw) P.simSetup = 'refloat';
}

export class LedSync {
  constructor({ getP, render, toast }) {
    this.getP = getP; this.render = render; this.toast = toast;
    this.reset(null);
  }

  reset(board) {
    this.board = board;
    this.cfg = null;
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
    fromConfig(P, c);
    this.last = desired(P);
    this.lastSeen = JSON.stringify(this.last);
    this.render();
  }

  onAck() {
    if (!this.pending) return;
    this.cfg = this.pending.cfg;
    this.last = this.pending.want;
    this.pending = null;
    this.toast('Saved to the board');
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
      if (now - this.pending.t > 4000) { this.pending = null; this.asked = 0; this.cfg = null; }  // no OK: read it again
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
