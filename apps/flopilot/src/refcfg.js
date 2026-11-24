// SPDX-License-Identifier: GPL-3.0-or-later
// Refloat's own config (VESC custom config 0), read and patched in place.
// The layout comes from Refloat's settings.xml (scripts/gen-layout.mjs; Refloat
// Glow's is the same as stock Refloat 1.2.3). The board sends a signature
// first: only a matching signature and exact length count as readable, and a
// write only changes the bytes of the fields that were set - every other
// setting (tunes, balance...) goes back byte for byte as it came.
import layout from './refloat-layout.json';

export const SIGNATURE = layout.signature;
const INT_SIZE = { 1: 1, 2: 1, 3: 2, 4: 2, 5: 4, 6: 4 };

// any VESC config with a known layout (Refloat's by default; motor config:
// mcconf.js); the layout's signature must match the board's
export class RefloatConfig {
  constructor(bytes, lay = layout) {
    this.layout = lay;
    this.raw = Uint8Array.from(bytes);
    this.dv = new DataView(this.raw.buffer);
    this.idx = new Map();
    this.ok = false;
    if (this.raw.length < 4) return;
    this.signature = this.dv.getUint32(0);
    if (this.signature !== lay.signature) return;
    let o = 4;
    for (const f of lay.fields) {
      let size;
      if (f.t === 1) size = f.x === 7 ? 2 : f.x === 8 || f.x === 9 ? 4 : 0;
      else if (f.t === 2) size = INT_SIZE[f.x] || 0;
      else if (f.t === 3) { let e = o; while (e < this.raw.length && this.raw[e]) e++; size = e - o + 1; }
      else if (f.t >= 4 && f.t <= 6) size = 1;
      if (!size) return;
      this.idx.set(f.n, { f, o });
      o += size;
    }
    this.ok = o === this.raw.length;
  }

  has(name) { return this.idx.has(name); }

  get(name) {
    const e = this.idx.get(name);
    if (!e) throw new Error('no setting ' + name);
    const { f, o } = e, dv = this.dv;
    if (f.t === 1) {
      if (f.x === 7) return dv.getInt16(o) / f.s;
      if (f.x === 8) return dv.getInt32(o) / f.s;
      return dv.getFloat32(o);
    }
    if (f.t === 2) {
      switch (f.x) {
        case 1: return dv.getUint8(o);
        case 2: return dv.getInt8(o);
        case 3: return dv.getUint16(o);
        case 4: return dv.getInt16(o);
        case 5: return dv.getUint32(o);
        case 6: return dv.getInt32(o);
        default: return NaN;
      }
    }
    if (f.t === 5) return dv.getUint8(o) !== 0;
    if (f.t === 3) { let s = ''; for (let i = o; this.raw[i]; i++) s += String.fromCharCode(this.raw[i]); return s; }
    return dv.getUint8(o);
  }

  // returns true when the stored bytes changed. noLimit: skip the settings
  // file's min / max (only the watchdog's emergency 5.0 V pad switch-off,
  // past Refloat's 3.3 V editor limit; the field itself holds up to 32.7 V)
  set(name, v, noLimit) {
    const e = this.idx.get(name);
    if (!e) throw new Error('no setting ' + name);
    const { f, o } = e, dv = this.dv;
    const before = this.raw.slice(o, o + 4).join();
    if (f.t === 1) {
      if (!noLimit && typeof f.min === 'number') v = Math.max(f.min, v);
      if (!noLimit && typeof f.max === 'number') v = Math.min(f.max, v);
      if (f.x === 7) dv.setInt16(o, Math.max(-32768, Math.min(32767, Math.round(v * f.s))));
      else if (f.x === 8) dv.setInt32(o, Math.round(v * f.s));
      else dv.setFloat32(o, v);
    } else if (f.t === 2) {
      v = Math.round(v);
      if (typeof f.min === 'number') v = Math.max(f.min, v);
      if (typeof f.max === 'number') v = Math.min(f.max, v);
      const m = [null, 'setUint8', 'setInt8', 'setUint16', 'setInt16', 'setUint32', 'setInt32'][f.x];
      if (!m) return false;
      dv[m](o, v);
    } else if (f.t === 5) dv.setUint8(o, v ? 1 : 0);
    else if (f.t === 4 || f.t === 6) {
      v = Math.round(v);
      if (f.e && (v < 0 || v >= f.e.length)) return false;  // not one of the setting's choices
      dv.setUint8(o, v & 0xff);
    } else return false;  // text settings: not written from the app
    return this.raw.slice(o, o + 4).join() !== before;
  }

  copy() { return new RefloatConfig(this.raw, this.layout); }
}
