// SPDX-License-Identifier: GPL-3.0-or-later
// VESC motor config (mcconf): read with VESC Tool's own layout for the
// board's firmware (6.05 / 6.06, signature checked), patch single fields.
// Used for CONTROL › MAX SPEED (Max ERPM, as RF Glow's maxSpeed in ui.qml.in)
// and for backups. Any app can do this with the firmware's own commands
// (COMM_GET_MCCONF / COMM_SET_MCCONF), as VESC Tool does: no package needed.
// Writes: board stopped only (writing the motor config resets the motor),
// then read back to confirm.
import { RefloatConfig } from './refcfg.js';
import { Comm, Rf } from './vesc.js';
import l605 from './mcconf-6.05.json';
import l606 from './mcconf-6.06.json';

const LAYOUTS = [l605, l606];
const ERPM_NONE = 100000;   // VESC's default Max ERPM (no limit)
const MIN_KMH = 64.4, MAX_KMH = 120.7;  // 40-75 mph, as RF Glow

export function parseMcconf(bytes) {
  for (const l of LAYOUTS) {
    const c = new RefloatConfig(bytes, l);
    if (c.ok) return c;
  }
  return null;
}

export class McConf {
  constructor(link, toast) {
    this.link = link; this.toast = toast;
    this.board = null; this.cfg = null; this.unknown = false;
    this.wait = null;
  }
  attach(b) {
    if (this.board === b) return;
    this.board = b; this.cfg = null; this.unknown = false;
    if (b) {
      b.onMcconf = bytes => this.onRead(bytes);
      b.onMcconfAck = () => { const w = this.wait; if (w && w.kind === 'write') { this.wait = null; w.done(true); } };
    }
  }
  onRead(bytes) {
    const c = parseMcconf(bytes);
    if (!c) this.unknown = true;
    else this.cfg = c;
    const w = this.wait;
    if (w && w.kind === 'read') { this.wait = null; w.done(!!c); }
  }
  ask(kind, payload, ms) {
    return new Promise(done => {
      const t = setTimeout(() => { if (this.wait && this.wait.t === t) { this.wait = null; done(false); } }, ms);
      this.wait = { kind, t, done: ok => { clearTimeout(t); done(ok); } };
      this.board.send(payload);
    });
  }
  read() { return this.board ? this.ask('read', [Comm.getMcconf], 3000) : Promise.resolve(false); }
  get(n) { try { return this.cfg ? this.cfg.get(n) : undefined; } catch (e) { return undefined; } }

  // wheel / poles / gear from Motor Settings › Additional Info
  get erpmPerKmh() {
    const d = this.get('si_wheel_diameter'), p = this.get('si_motor_poles'), g = Math.max(0.01, this.get('si_gear_ratio') || 1);
    return d > 0.05 && p >= 2 ? (1000 / 60) / (Math.PI * d) * (p / 2) * g : 0;
  }
  get capMph() {
    const e = this.get('l_max_erpm'), k = this.erpmPerKmh;
    return k && e > 0 && e < ERPM_NONE * 0.99 ? e / k * 0.621371 : 0;  // not rounded: shown in mph or km/h
  }

  // write a patched copy (board stopped), then read back and check
  async write(values, check) {
    const b = this.board;
    if (!b || !this.link.live) return 'Connect your board first';
    if (b.pkgState === Rf.sRunning) return 'Stop riding first: motor settings can\'t change while the board is engaged';
    if (!(await this.read())) return this.unknown ? 'This firmware\'s motor settings aren\'t supported yet (VESC 6.05 / 6.06 are)' : 'The board didn\'t send its motor settings';
    if (b.pkgState === Rf.sRunning) return 'Stop riding first';
    const c = this.cfg.copy();
    for (const [k, v] of Object.entries(values)) if (c.has(k)) c.set(k, v);
    if (!(await this.ask('write', [Comm.setMcconf, ...c.raw], 4000))) return 'The board didn\'t confirm the motor settings';
    await new Promise(r => setTimeout(r, 800));
    if (!(await this.read()) || !check(this)) return 'Saved, but the read-back didn\'t match: check Motor Settings in VESC Tool';
    return '';
  }

  // RF Glow's maxSpeed.apply: Max ERPM (and Min ERPM) from a speed, push fading in the last 5%
  async setLimit(mph) {
    if (!(await this.read())) return this.unknown ? 'This firmware\'s motor settings aren\'t supported yet (VESC 6.05 / 6.06 are)' : 'The board didn\'t send its motor settings';
    const k = this.erpmPerKmh;
    if (!k) return 'Set the wheel diameter and motor poles in VESC Tool first (Motor Settings › Additional Info)';
    const kmh = Math.max(MIN_KMH, Math.min(MAX_KMH, mph / 0.621371));
    const e = Math.round(kmh * k);
    const vals = { l_max_erpm: e, l_min_erpm: -e };
    if ((this.get('l_erpm_start') || 0) < 0.95) vals.l_erpm_start = 0.95;
    return this.write(vals, m => Math.abs(m.get('l_max_erpm') - e) < 2);
  }
  async removeLimit() {
    return this.write({ l_max_erpm: ERPM_NONE, l_min_erpm: -ERPM_NONE }, m => Math.abs(m.get('l_max_erpm') - ERPM_NONE) < 2);
  }
}
