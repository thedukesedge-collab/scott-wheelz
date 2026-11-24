// SPDX-License-Identifier: GPL-3.0-or-later
// Horn, melodies, MIXER tiles and the tilt slider through the VESC firmware's
// Lisp REPL - no package support needed, same on stock Refloat 1.2.3 and
// Refloat Glow. Tones use foc-play-tone (VESC firmware 6.05+; it "also works
// while the motor is running").
//
// The REPL's own limits (VESC firmware lispif.c), which everything here obeys:
// - a command arriving less than 0.5 s after the previous one is dropped, and
//   so is one sent while an earlier command is still running
//   -> one command at a time, at least REPL_GAP apart (Repl below)
// - commands run in the Lisp memory (28 KB on the controller) that Refloat
//   itself, its LED buffers and any other scripts also live in
//   -> before installing anything the app reads the board's Lisp memory use
//      (COMM_LISP_GET_STATS) and stays off above MEM_MAX / HEAP_MAX. FloPilot's
//      helper is small: a few definitions and one watchdog thread (~1 KB).
//
// Safety, on the board (installed once per controller boot, checked first):
// - a tone keeps playing only while keepalives arrive (every REPL_GAP); the
//   watchdog thread stops it STALE s after the last one (Bluetooth drop, phone
//   in a pocket, app paused...)
// - hard time caps counted on the board: horn 3 s riding / 2 s idle (Refloat
//   Glow's), tiles 2 s; holding on past the cap stays silent until released
// - melodies play on the board as a whole (max 4 s, the MIXER's limit) and stop
//   if the wheel starts turning (melodies are for a stopped board)
// - volume and pitch clamped here to Refloat Glow's horn limits
//   (0.4-4 V, 300-1000 Hz; melodies / tiles at most 2.5 V)
import { Comm, cString } from './vesc.js';

const RF_GLOW = 'Refloat Glow';  // RF Glow's package name on the board
const LIVE_TONE = 125;           // RF Glow COMMAND_LIVE_TONE
export const mixLive = () => { try { return localStorage.getItem('fpMixLive') || 'phone'; } catch (e) { return 'phone'; } };
export const REPL_GAP = 600;  // ms between REPL commands (firmware drops < 500 ms)
const STALE = 1.4;            // s without keepalive before the board stops a tone
export const MEM_MAX = 85;    // % Lisp memory in use: above this, stay off
export const HEAP_MAX = 85;   // % Lisp heap in use: above this, stay off
const MIN_V = 0.4, MAX_V = 4, MEL_MAX_V = 2.5;

// installed in three REPL commands (a VESC packet holds at most 512 bytes);
// each ends with a marker the app waits for. fp-run 0 ends the watchdog.
const INSTALL = [
  '(progn (define fp-k 0) (define fp-s 0) (define fp-c 0) (define fp-on 0) (define fp-run 1) (define fp-d 0)'
  + " (define fp-hz '(330 349 392 440 494 523 587 659 698 784))"
  + ' (defun fp-t (hz v c) (progn (if (= fp-on 0) (progn (define fp-s (systime)) (define fp-c c) (define fp-on 1)))'
  + " (define fp-k (systime)) (if (< (secs-since fp-s) fp-c) (foc-play-tone 0 hz v) (foc-play-stop)))) 'fpi1)",
  '(progn (defun fp-m (v n) (if (and n (< (abs (get-speed)) 0.5)) (progn (if (and (> (car n) 0) (< (car n) 11))'
  + ' (foc-play-tone 0 (ix fp-hz (- (car n) 1)) v) (foc-play-tone 0 300 0)) (sleep (* 0.02 (car (cdr n))))'
  + " (fp-m v (cdr (cdr n)))) (foc-play-stop))) 'fpi2)",
  // fp-d: the watchdog runs (spawned once, even if this command is sent twice)
  '(progn (if (= fp-d 0) (progn (define fp-d 1) (spawn (lambda () (loopwhile (= fp-run 1) (progn (if (and (= fp-on 1) (or (> (secs-since fp-k) ' + STALE + ')'
  + ' (> (secs-since fp-s) fp-c))) (progn (foc-play-stop) (if (> (secs-since fp-k) ' + STALE + ') (define fp-on 0)))) (sleep 0.05)))))))'
  + " 'fpok)",
];
export const INSTALL_CMDS = INSTALL;
const MAX_EXPR = 480;  // characters per REPL command

const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
export const toneFw = fw => !!fw && (fw.major > 6 || (fw.major === 6 && fw.minor >= 5));
const fwText = fw => fw.major + '.' + String(fw.minor).padStart(2, '0');

export class Sound {
  constructor(link, toast = () => {}) {
    this.link = link;
    this.toast = toast;
    this.board = null;
    this.timer = setInterval(() => this.tick(), 50);
  }

  // new connection: start over
  reset(b) {
    this.board = b;
    this.state = 'idle';    // idle -> stats -> probe -> install -> ready | off
    this.why = '';
    this.busyUntil = 0;     // no REPL command before this time
    this.waitReply = null;  // { tag, until }
    this.horn = false;
    this.hornSent = 0;
    this.tile = null;
    this.stopNeeded = false;
    this.mel = null;        // melody waiting to be sent
    this.glowTile = null;
    this.glowStop = 0;
    this.melUntil = 0;
    this.tiltSent = '0.000';  // nothing to send until the slider moves
    this.tiltZeros = 3;
    this.tiltAt = 0;
    if (b) {
      b.onLispPrint = t => this.onPrint(t);
      b.onLispStats = st => this.onStats(st);
    }
  }

  send(expr) {
    const b = this.board;
    if (!b) return;
    b.send([Comm.lispReplCmd, ...cString(expr)]);
    this.busyUntil = Date.now() + REPL_GAP;
  }

  onStats(st) {
    if (this.state !== 'stats') return;
    this.stats = st;
    if (st.mem > MEM_MAX || st.heap > HEAP_MAX) {
      this.off('memory', "The board's Lisp memory is too full for the horn and melodies ("
        + Math.round(Math.max(st.mem, st.heap)) + '% used, FloPilot needs it under ' + MEM_MAX + '%). Other Lisp scripts or LED settings use it.');
      return;
    }
    this.state = 'probe';
  }

  onPrint(text) {
    const w = this.waitReply;
    if (!w) return;
    if (w.tag === 'probe') {
      // "> 1": installed this boot with its watchdog running; else (not bound, 0): install
      this.waitReply = null;
      this.state = /^>\s*1\b/.test(text) ? 'ready' : 'install';
      this.step = 0;
    } else if (w.tag === 'install') {
      this.waitReply = null;
      if (text.includes(['fpi1', 'fpi2', 'fpok'][this.step])) {
        if (++this.step >= INSTALL.length) this.state = 'ready';
      } else this.off('install', 'Horn and melodies could not start on the board (' + text.slice(0, 60) + ')');
    }
  }

  off(why, msg) {
    this.state = 'off';
    this.msg = msg;
    if (this.why !== why) { this.why = why; this.msgAt = Date.now(); this.toast(msg); }
  }

  // why the horn / melodies are unavailable ('' = available or still checking)
  get unavailable() { return this.state === 'off' ? this.why : ''; }

  running() { const S = this.link.S || window.__sim; return !!S && S.mode === 'running'; }

  // simulator hooks (window.RF_SOUND): true = handled here (the phone stays quiet)
  melody(notes, volume) {
    if (this.state !== 'ready') { this.explain(); return true; }
    if (this.running()) return true;
    // levels 1-10 and 20 ms units, as the MIXER records them (max 4 s); 11 = controller beep: a rest
    let total = 0;
    const flat = [];
    for (const [l, ms] of notes) {
      if (total >= 4000 || flat.join(' ').length > MAX_EXPR - 40) break;
      const u = Math.max(1, Math.round(Math.min(ms, 4000 - total) / 20));
      flat.push(l >= 1 && l <= 10 ? l : 0, u);
      total += u * 20;
    }
    this.mel = { v: clamp(volume, 0.25, MEL_MAX_V), list: flat, ms: total };
    this.tile = null;
    return true;
  }
  // MIXER tiles (live). RF Glow on the board: its own live tone command, sent
  // every 100 ms - real time. Stock Refloat: the rider's choice (mixLive):
  // 'phone' (default) = the phone plays it, 'board' = the motor plays it
  // through the REPL, about half a second late.
  live(hz, volume, level) {
    if (this.rfGlow()) {
      if (this.running()) return true;
      this.glowTile = { level: Math.max(1, Math.min(10, level || 1)), t0: Date.now(), at: 0 };
      return true;
    }
    if (mixLive() !== 'board') return false;  // the phone plays it
    if (this.state !== 'ready') { this.explain(); return true; }
    if (this.running()) return true;
    this.tile = { hz: clamp(hz, 300, 1000), v: clamp(volume, 0.25, MEL_MAX_V), t0: Date.now(), sent: 0 };
    return true;
  }
  liveStop() {
    if (this.glowTile) { this.glowTile = null; this.glowStop = 3; }  // level 0, sent a few times
    if (this.tile) { this.tile = null; this.stopNeeded = true; }
  }
  rfGlow() { const b = this.link.board; return !!(b && this.link.live && b.pkg && b.pkg.name === RF_GLOW); }

  // the package's live tone (COMMAND_LIVE_TONE): level 1-10, 0 = stop; the
  // package stops by itself 0.3 s after the last one, caps a note at 2 s and
  // plays only with the board stopped
  glowTick(now) {
    const b = this.link.board;
    if (!b) return;
    if (this.glowTile) {
      if (now - this.glowTile.t0 > 2000 || this.running()) { this.liveStop(); return; }
      if (now - this.glowTile.at >= 100) { this.glowTile.at = now; b.app([LIVE_TONE, this.glowTile.level]); }
    } else if (this.glowStop > 0) {
      this.glowStop--;
      b.app([LIVE_TONE, 0]);
    }
  }

  // a press while unavailable: say why (at most every 5 s)
  explain() {
    const b = this.board;
    if (this.state !== 'off' && b && b.fw && !toneFw(b.fw)) {
      this.off('fw', 'Horn and melodies need VESC firmware 6.05 or newer (this board: ' + fwText(b.fw) + ')');
    } else if (this.state === 'off' && Date.now() - (this.msgAt || 0) > 5000) {
      this.msgAt = Date.now();
      this.toast(this.msg);
    }
  }

  // tilt slider: Refloat's remote input (only used while engaged and moving);
  // 4 deg at full slider. Sent when it changes, refreshed every 2 s, and the
  // return to 0 is repeated so it can't be lost. Needs no install / memory.
  sendTilt(S, now) {
    if (now < this.busyUntil || now < this.melUntil || this.waitReply) return;
    const tilt = Math.max(-1, Math.min(1, ((S && S.tilt) || 0) * 0.4));
    const key = tilt.toFixed(3);
    if (key !== this.tiltSent || (tilt !== 0 && now - this.tiltAt > 2000) || (tilt === 0 && this.tiltZeros < 3)) {
      if (key !== this.tiltSent) this.tiltZeros = 0;
      if (tilt === 0) this.tiltZeros = (this.tiltZeros || 0) + 1;
      this.send('(set-remote-state ' + key + ' 0 0 0 0)');
      this.tiltSent = key;
      this.tiltAt = now;
    }
  }

  tick() {
    const link = this.link, b = link.board, now = Date.now();
    if (b !== this.board) this.reset(b);
    if (!b || !link.live || link.installing || link.lcmFlashing) return;
    this.glowTick(now);
    const S = link.S || window.__sim, P = window.__simP ? window.__simP() : {};

    // what the app wants right now
    const hornOn = !!(S && S.horn);
    if (this.horn && !hornOn) this.stopNeeded = true;
    this.horn = hornOn;
    if (this.tile && (now - this.tile.t0 > 2000 || this.running())) this.liveStop();
    if (this.mel && this.running()) this.mel = null;

    // checks before anything is installed: firmware, then Lisp memory, then
    // "already installed this boot?", then install
    if (this.state === 'idle') {
      if (!b.fw) return;
      if (!toneFw(b.fw)) { if (hornOn) this.explain(); this.sendTilt(S, now); return; }
      this.state = 'stats';
      this.statsAt = now;
      b.send([Comm.lispStats, 0]);
      return;
    }
    if (this.state === 'stats') {
      if (now - this.statsAt > 1500) { this.statsAt = now; b.send([Comm.lispStats, 0]); }
      return;
    }
    if (this.state === 'off') { if (hornOn) this.explain(); this.sendTilt(S, now); return; }

    if (this.waitReply && now > this.waitReply.until) {
      // no answer: the command was probably dropped (REPL busy) - ask again
      this.state = this.waitReply.tag;
      this.waitReply = null;
    }
    if (now < this.busyUntil || now < this.melUntil || this.waitReply) return;

    if (this.state === 'probe') { this.send('fp-d'); this.waitReply = { tag: 'probe', until: now + 1500 }; return; }
    if (this.state === 'install') { this.send(INSTALL[this.step]); this.waitReply = { tag: 'install', until: now + 2500 }; return; }
    if (this.state !== 'ready') return;

    // one REPL command per slot, most important first
    if (hornOn) {
      const v = P.hAdv ? P.hBeep : (P.hsVol || 5) * 0.4;
      const hz = P.hAdv ? P.hBeepHz : Math.round((300 + ((P.hsPitch || 3) - 1) * 700 / 9) / 25) * 25;
      this.send('(fp-t ' + Math.round(clamp(hz || 495, 300, 1000)) + ' ' + clamp(v || 2, MIN_V, MAX_V).toFixed(2) + ' ' + (this.running() ? '3.0' : '2.0') + ')');
      this.stopNeeded = false;
      return;
    }
    if (this.stopNeeded) { this.send('(foc-play-stop)'); this.stopNeeded = false; return; }
    if (this.tile) {
      this.send('(fp-t ' + Math.round(this.tile.hz) + ' ' + this.tile.v.toFixed(2) + ' 2.0)');
      return;
    }
    if (this.mel) {
      const m = this.mel;
      this.mel = null;
      this.send("(fp-m " + m.v.toFixed(2) + " '(" + m.list.join(' ') + '))');
      this.melUntil = now + m.ms + 400;  // the REPL is busy while it plays
      return;
    }
    this.sendTilt(S, now);
  }
}
