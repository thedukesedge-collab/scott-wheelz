// SPDX-License-Identifier: GPL-3.0-or-later
// Refloat config read / patch tests (run bundled: see package.json "test").
import { RefloatConfig, SIGNATURE } from '../src/refcfg.js';
import { LedSync } from '../src/ledsync.js';
import layout from '../src/refloat-layout.json';

const ok = (c, m) => { if (!c) { console.error('FAIL: ' + m); process.exit(1); } };
const size = 4 + layout.fields.reduce((a, f) => a + (f.t === 1 ? 2 : f.t === 2 ? [0, 1, 1, 2, 2, 4, 4][f.x] : 1), 0);
const buf = new Uint8Array(size);
new DataView(buf.buffer).setUint32(0, SIGNATURE);
// fill with a pattern so "unchanged" is a real check
for (let i = 4; i < size; i++) buf[i] = (i * 7) & 0x01;

const c = new RefloatConfig(buf);
ok(c.ok, 'parses a config of the right size');
ok(!new RefloatConfig(buf.slice(0, size - 1)).ok, 'rejects a short config');
const bad = buf.slice(); bad[0] ^= 1;
ok(!new RefloatConfig(bad).ok, 'rejects another signature');

const d = c.copy();
ok(d.set('leds.front.brightness', 0.8), 'set brightness');
ok(Math.abs(d.get('leds.front.brightness') - 0.8) < 1e-4, 'brightness reads back');
ok(d.set('leds.rear.mode', 4) && d.get('leds.rear.mode') === 4, 'enum');
ok(!d.set('leds.rear.mode', 99), 'enum out of range refused');
ok(d.set('leds.status.idle_timeout', 60) && d.get('leds.status.idle_timeout') === 60, 'uint16');
d.set('leds.headlights_on', true);
ok(d.get('leds.headlights_on') === true, 'bool');
// every byte outside the changed fields is untouched
const changed = ['leds.front.brightness', 'leds.rear.mode', 'leds.status.idle_timeout', 'leds.headlights_on'].map(n => d.idx.get(n).o);
let diff = 0;
for (let i = 0; i < size; i++) if (d.raw[i] !== buf[i] && !changed.some(o => i >= o && i < o + 2)) diff++;
ok(diff === 0, 'only the set fields change (' + diff + ' other bytes)');
ok(c.get('leds.front.brightness') !== 0.8, 'copy() leaves the original alone');

// LEDS tab <-> config round trip
const P = { rgb: [{}, {}, {}, {}] };
let rendered = 0;
const s = new LedSync({ getP: () => P, render: () => rendered++, toast: () => {} });
const sent = [];
const board = { pkgState: 2, getConfig() {}, setConfig(raw) { sent.push(raw); } };
s.reset(board);
d.set('leds.headlights.brightness', 0.65); d.set('leds.status_idle.speed', 2.5); d.set('leds.rear.brightness', 0.3);
board.onConfig(d.raw);
ok(rendered === 1 && P.ledHeadPct === 65 && P.rfSpeed === 2.5 && P.rgb[3].bri === 0.3, 'fromConfig fills the LEDS tab');
const t0 = Date.now;
let now = 1e6; Date.now = () => now;
s.tick(true); now += 2000; s.tick(true);
ok(sent.length === 0, 'nothing written without a change');
P.ledHeadPct = 40; P.rgb[0].mode = 3;
s.tick(true); now += 2000;
board.pkgState = 3; s.tick(true);
ok(sent.length === 0, 'no write while running');
board.pkgState = 2; s.tick(true);
ok(sent.length === 1, 'written once stopped');
const w = new RefloatConfig(sent[0]);
ok(w.ok && Math.abs(w.get('leds.headlights.brightness') - 0.4) < 1e-4 && w.get('leds.headlights.mode') === 3, 'the written config has the changes');
let other = 0;
for (let i = 0; i < size; i++) if (w.raw[i] !== d.raw[i]) other++;
ok(other <= 3, 'and nothing else (' + other + ' bytes differ)');
board.onConfigAck();
now += 2000; s.tick(true);
ok(sent.length === 1, 'no repeat write after the OK');
Date.now = t0;
console.log('config tests passed (' + layout.fields.length + ' fields, ' + size + ' bytes)');
