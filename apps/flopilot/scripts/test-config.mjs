// SPDX-License-Identifier: GPL-3.0-or-later
// Refloat config read / patch tests (run bundled: see package.json "test").
import { RefloatConfig, SIGNATURE } from '../src/refcfg.js';
import { LedSync } from '../src/ledsync.js';
import layout from '../src/refloat-layout.json';
import l605 from '../src/mcconf-6.05.json';
import { McConf, parseMcconf } from '../src/mcconf.js';

const assert0 = (c, m) => ok(c, m);
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
// doubles outside the LEDs read with their scale (Angle P etc.)
d.set('kp', 21.5); d.set('tiltback_duty', 0.82); d.set('tiltback_speed', 40); d.set('haptic.duty.strength', 3.5);
assert0(Math.abs(d.get('kp') - 21.5) < 1e-6, 'kp reads back');
// Settings › RIDE: HAPTIC / PUSHBACK fill from the board, and changes are written
const P2 = { rgb: [{}, {}, {}, {}] };
const sent2 = [];
const s2 = new LedSync({ getP: () => P2, render: () => {}, toast: () => {} });
const board2 = { pkgState: 2, getConfig() {}, setConfig(raw) { sent2.push(raw); } };
s2.reset(board2);
d.set('tiltback_duty_angle', 5); d.set('tiltback_hv', 4.3);
board2.onConfig(d.raw);
assert0(P2.hap.duty === 82 && P2.hap.spd === 40 && P2.hap.str === 3.5 && P2.pb.dutyAng === 5 && P2.pb.hv === 4.3, 'haptic / pushback read');
s2.tick(true); now += 2000; s2.tick(true);
assert0(sent2.length === 0, 'no write without a change');
P2.hap.duty = 85; P2.pb.dutyAng = 0;
s2.tick(true); now += 2000; s2.tick(true);
const w2 = new RefloatConfig(sent2[0]);
assert0(Math.abs(w2.get('tiltback_duty') - 0.85) < 1e-4 && w2.get('tiltback_duty_angle') === 0 && Math.abs(w2.get('kp') - 21.5) < 1e-6, 'haptic / pushback written, kp untouched');
board2.onConfigAck();
// writeValues (a tune / package on-off): only those, board stopped
const tv = s2.tuneValues();
assert0(tv && 'kp' in tv && !('tiltback_duty' in tv) && !('leds.on' in tv) && !('disabled' in tv), 'tune = Refloat tune settings only');
board2.pkgState = 3;
assert0(s2.writeValues({ disabled: true }) === 'Stop the board first', 'no writes while running');
board2.pkgState = 2;
assert0(s2.writeValues({ kp: 22 }, 'ok') === '', 'write accepted');
const w3 = new RefloatConfig(sent2[sent2.length - 1]);
assert0(w3.get('kp') === 22 && Math.abs(w3.get('tiltback_duty') - 0.85) < 1e-4, 'only kp changed');
Date.now = t0;
// motor config (6.05 layout, signature = the firmware's MCCONF_SIGNATURE)
assert0(l605.signature === 1065524471, '6.05 signature matches the firmware');
const msz = 4 + l605.fields.reduce((a, f) => a + (f.t === 1 ? (f.x === 7 ? 2 : 4) : f.t === 2 ? [0, 1, 1, 2, 2, 4, 4][f.x] : 1), 0);
const mraw = new Uint8Array(msz); new DataView(mraw.buffer).setUint32(0, l605.signature);
const m0 = parseMcconf(mraw);
assert0(m0 && m0.ok, 'mcconf parses');
m0.set('si_wheel_diameter', 0.28); m0.set('si_motor_poles', 30); m0.set('si_gear_ratio', 1); m0.set('l_max_erpm', 100000); m0.set('l_min_erpm', -100000); m0.set('l_erpm_start', 0.8);
m0.set('l_current_max', 60);
let mflash = m0.raw.slice();
const mboard = { pkgState: 2, send(p) {
  if (p[0] === 14) setTimeout(() => mboard.onMcconf(mflash.slice()), 1);
  if (p[0] === 13) { mflash = Uint8Array.from(p.slice(1)); setTimeout(() => mboard.onMcconfAck(), 1); }
} };
const realTimeout = globalThis.setTimeout;
Date.now = t0;
const mc = new McConf({ live: true }, () => {});
mc.attach(mboard);
const e1 = await mc.setLimit(45);
assert0(e1 === '', 'limit set: ' + e1);
const after = parseMcconf(mflash);
const want = Math.round(45 / 0.621371 * (1000 / 60) / (Math.PI * 0.28) * 15);
assert0(Math.abs(after.get('l_max_erpm') - want) < 2 && Math.abs(after.get('l_min_erpm') + want) < 2 && Math.abs(after.get('l_erpm_start') - 0.95) < 1e-4, 'Max / Min ERPM and ERPM start');
assert0(Math.abs(after.get('l_current_max') - 60) < 1e-4, 'other motor settings untouched');
assert0(Math.round(mc.capMph) === 45, 'cap reads back as 45 mph');
mboard.pkgState = 3;
assert0((await mc.removeLimit()).startsWith('Stop riding'), 'no motor writes while riding');
mboard.pkgState = 2;
assert0((await mc.removeLimit()) === '' && parseMcconf(mflash).get('l_max_erpm') === 100000, 'limit removed');
console.log('config tests passed (' + layout.fields.length + ' fields, ' + size + ' bytes)');
