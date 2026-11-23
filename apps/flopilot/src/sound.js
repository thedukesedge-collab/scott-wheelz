// SPDX-License-Identifier: GPL-3.0-or-later
// Horn, melodies and MIXER tiles through the motor, without any package
// support: VESC firmware 6.05+ plays tones itself (LispBM foc-play-tone,
// "also works while the motor is running"), sent over the Lisp REPL like the
// tilt slider. Works the same on stock Refloat 1.2.3 and Refloat Glow.
//
// Safety, on the board (a small watchdog defined once per connection):
// - every tone needs a keepalive from the app at least every 0.35 s, or it
//   stops (phone in a pocket, Bluetooth drop, app paused...)
// - each sound has a hard time cap counted on the board (horn 3 s riding /
//   2 s idle, like Refloat Glow; tiles 2 s; melodies 4.5 s); holding on past
//   the cap stays silent until the keepalives stop
// - volume and pitch are clamped here to Refloat Glow's horn limits
//   (0.4-4 V, 300-1000 Hz; melodies at most 2.5 V)
// Melodies and tiles play only with the board stopped (READY), as in Glow.
import { Comm, cString } from './vesc.js';

const DEFS = [
  '(define fp-k 0)',
  '(define fp-s 0)',
  '(define fp-cap 0)',
  '(define fp-on 0)',
  '(defun fp-dog () (progn (loopwhile (and (< (secs-since fp-k) 0.35) (< (secs-since fp-s) fp-cap)) (sleep 0.05)) (foc-play-stop) (loopwhile (< (secs-since fp-k) 0.35) (sleep 0.05)) (define fp-on 0)))',
  '(defun fp-tone (hz v cap) (progn (if (= fp-on 0) (progn (define fp-s (systime)) (define fp-cap cap) (define fp-on 1) (spawn fp-dog))) (define fp-k (systime)) (if (< (secs-since fp-s) fp-cap) (foc-play-tone 0 hz v) (foc-play-stop))))',
];
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));

export class Sound {
  constructor(link) {
    this.link = link;
    this.board = null;
    this.ready = false;
    this.horn = false;
    this.mel = null;   // { notes: [[hz, ms]], t0, v }
    this.tile = null;  // { hz, v, t0 }
    this.timer = setInterval(() => this.tick(), 90);
  }

  repl(expr) {
    const b = this.link.board;
    if (b) b.send([Comm.lispReplCmd, ...cString(expr)]);
  }

  // the watchdog + tone function, once per connection
  install() {
    const b = this.link.board;
    if (!b || !this.link.live) { this.ready = false; this.board = null; return false; }
    if (this.board !== b) { this.board = b; this.ready = false; }
    if (!this.ready) { DEFS.forEach(d => this.repl(d)); this.ready = true; }
    return true;
  }

  tone(hz, v, cap) { this.repl('(fp-tone ' + Math.round(hz) + ' ' + v.toFixed(2) + ' ' + cap.toFixed(1) + ')'); }
  stop() { this.repl('(foc-play-stop)'); }

  // simulator hooks (window.RF_SOUND): true = the board plays it, skip the phone's audio
  melody(notes, volume, noteHz) {
    if (!this.install() || this.running()) return false;
    this.mel = {
      notes: notes.map(([l, ms]) => [l > 0 && l <= 10 ? noteHz(l) : 0, ms]),
      t0: Date.now(), v: clamp(volume, 0.25, 2.5),
    };
    this.tile = null;
    return true;
  }
  live(hz, volume) {
    if (!this.install() || this.running()) return false;
    this.tile = { hz: clamp(hz, 300, 1000), v: clamp(volume, 0.25, 2.5), t0: Date.now() };
    this.mel = null;
    return true;
  }
  liveStop() { if (this.tile) { this.tile = null; this.stop(); } }
  stopAll() { const any = this.mel || this.tile; this.mel = null; this.tile = null; if (any) this.stop(); }

  running() { const S = this.link.S || window.__sim; return !!S && S.mode === 'running'; }

  tick() {
    const link = this.link;
    if (!link.live || !link.board) { this.mel = null; this.tile = null; this.horn = false; return; }
    const S = link.S || window.__sim, P = window.__simP ? window.__simP() : {};
    // horn (the app's own press / budget rules in the simulator UI, S.horn)
    if (S && S.horn) {
      if (!this.install()) return;
      this.mel = null; this.tile = null;
      this.tone(clamp(P.hBeepHz || 495, 300, 1000), clamp(P.hBeep || 2.5, 0.4, 4), this.running() ? 3.0 : 2.0);
      this.horn = true;
      return;
    }
    if (this.horn) { this.horn = false; this.stop(); }
    // melodies and tiles only with the board stopped
    if ((this.mel || this.tile) && this.running()) { this.stopAll(); return; }
    if (this.tile) {
      if (Date.now() - this.tile.t0 > 2000) { this.liveStop(); return; }
      this.tone(this.tile.hz, this.tile.v, 2.0);
      return;
    }
    if (this.mel) {
      let t = Date.now() - this.mel.t0;
      for (const [hz, ms] of this.mel.notes) {
        if (t < ms) { this.tone(hz || 300, hz ? this.mel.v : 0, 4.5); return; }
        t -= ms;
      }
      this.mel = null;
      this.stop();
    }
  }
}
