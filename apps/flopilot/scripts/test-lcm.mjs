// SPDX-License-Identifier: GPL-3.0-or-later
// SLCM update tests (lcmfw.js): .hex parsing, version fit, and a full update
// against a pretend RF Glow + SLCM bootloader that follows src/lcm_fw.c.
import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import { parseHex, crc16, parseVer, fitOf, LcmUpdater } from '../src/lcmfw.js';

// a small Intel hex: app image at 0x08000600 with a valid vector table and a name
function makeHex(bytes) {
  const rec = (addr, type, data) => {
    const r = [data.length, (addr >> 8) & 255, addr & 255, type, ...data];
    const sum = (256 - (r.reduce((a, b) => a + b, 0) & 255)) & 255;
    return ':' + [...r, sum].map(x => x.toString(16).padStart(2, '0').toUpperCase()).join('');
  };
  const out = [rec(0, 4, [0x08, 0x00])];
  for (let i = 0; i < bytes.length; i += 16) out.push(rec(0x0600 + i, 0, [...bytes.slice(i, i + 16)]));
  out.push(rec(0, 1, []));
  return out.join('\n');
}
const app = new Uint8Array(300).fill(0x11);
new DataView(app.buffer).setUint32(0, 0x20000800, true);   // stack pointer
new DataView(app.buffer).setUint32(4, 0x08000641, true);   // reset vector (thumb)
app.set([...'VPBL-r37-fc70747'].map(c => c.charCodeAt(0)), 100);
const h = parseHex(makeHex(app), 'test');
assert.equal(h.error, undefined);
assert.equal(h.size, 384);  // whole 128-byte pages, padded with 0xFF
assert.equal(h.bytes[300], 0xff);
assert.equal(h.slcm, 'r37-fc70747');
assert.equal(h.crc, crc16(h.bytes, h.size));
assert.ok(parseHex(makeHex(app).replace(':10', ':11'), 'x').error);  // broken checksum
const bad = app.slice(); bad[4] = 0;
assert.ok(parseHex(makeHex(bad), 'x').error.includes("doesn't look like"));

// version fit
assert.equal(fitOf(parseVer('r37-fc70747'), parseVer('r37-fc70747')), 'same');
assert.equal(fitOf(parseVer('r35-aaaaaaa'), parseVer('r37-fc70747')), 'older');
assert.equal(fitOf(parseVer('r40-bbbbbbb'), parseVer('r37-fc70747')), 'newer');

// full update: RF Glow relay + bootloader, as src/lcm_fw.c
const fw = { state: 0, error: 0, want: 0, size: 0, crc: 0, pages: {}, got: new Uint8Array(h.size) };
let lcmName = 'VPBL-r35-aaaaaaa', ticks = 0;
const link = { live: true };
const board = {
  lcmName, pkg: { name: 'Refloat Glow' }, pkgState: 2,
  app(bytes) {
    const [cmd, sub, ...d] = bytes;
    if (cmd !== 128) return;
    const u32 = i => ((d[i] << 24) | (d[i + 1] << 16) | (d[i + 2] << 8) | d[i + 3]) >>> 0;
    if (sub === 1) { Object.assign(fw, { state: 1, error: 0, size: u32(0), crc: (d[4] << 8) | d[5], want: 128 }); }
    if (sub === 2) { const off = u32(0); if (fw.state === 1 || fw.state === 2) fw.pages[(off / 128) & 1] = { off, data: d.slice(4, 132) }; }
    if (sub === 4) fw.state = 0;
    if (sub === 3) {
      // the bootloader: starts a few polls after staging, then takes the page it wants when there
      if (fw.state === 1 && ++ticks > 3) fw.state = 2;
      if (fw.state === 2) {
        const p = fw.pages[(fw.want / 128) & 1];
        if (p && p.off === fw.want) {
          fw.got.set(p.data, fw.want);
          // pages 128, 256, ... last, then page 0 (the vector table) last of all
          if (fw.want === 0) { fw.state = crc16(fw.got, fw.size) === fw.crc ? 3 : 4; fw.error = fw.state === 4 ? 5 : 0; }
          else fw.want = fw.want + 128 < fw.size ? fw.want + 128 : 0;
        }
      }
      const w = fw.want;
      setTimeout(() => board.onLcmFw([128, 3, fw.state, fw.error, (w >>> 24) & 255, (w >>> 16) & 255, (w >>> 8) & 255, w & 255, 0, 0, 0, 0, 2]), 1);
    }
  },
};
const up = new LcmUpdater(link);
up.attach(board);
assert.equal(up.fitted, 'vpbl');
assert.ok(up.capable && up.rfGlow);
await new Promise((done, fail) => { setTimeout(() => fail(new Error('update did not finish: ' + up.message)), 20000); up.onChange = () => { if (!up.active && up.done !== undefined) done(); }; up.start(h); });
assert.equal(up.done, true, up.message);
assert.deepEqual([...fw.got], [...h.bytes]);  // the LCM got the exact image
assert.ok(!link.lcmFlashing);

// stock / no bootloader: not capable
board.lcmName = 'VP-r20-ccccccc';
assert.equal(up.fitted, 'vp');
assert.ok(!up.capable);

// the real SLCM.hex, when one is around (local runs)
const real = '/tmp/slcm/SLCM.hex';
if (existsSync(real)) {
  const r = parseHex(readFileSync(real, 'utf8'), 'SLCM.hex');
  assert.equal(r.error, undefined, r.error);
  console.log('real SLCM.hex: ' + r.size + ' B, ' + r.slcm);
}
console.log('lcm tests passed');
