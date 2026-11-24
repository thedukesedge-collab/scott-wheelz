// SPDX-License-Identifier: GPL-3.0-or-later
// MAX AIR detector: a real jump counts; hand spins, bails, slips, gaps don't.
import assert from 'node:assert/strict';
import { AirDetector } from '../src/airtime.js';

// samples every 100 ms (Refloat realtime rate over BLE)
function run(samples, step = 100) {
  const d = new AirDetector(), out = [];
  samples.forEach((s, i) => { const r = d.feed(i * step, s); if (r) out.push(r); });
  return out;
}
const ride = { running: true, pads: true, slip: false, amps: 10, erpm: 18000 };
const air = (n, o = {}) => Array.from({ length: n }, (_, i) => Object.assign({}, ride, { slip: true, amps: 3, erpm: 19000 + i * 800 }, o));
const land = [Object.assign({}, ride, { slip: true, amps: 45, erpm: 16000 })];
const after = Array.from({ length: 6 }, () => Object.assign({}, ride, { slip: false }));
const pre = Array.from({ length: 5 }, () => ride);

// 0.7 s jump with a landing
assert.deepEqual(run([...pre, ...air(7), ...land, ...after]), [0.7]);
// too short for STRICT (0.2 s)
assert.deepEqual(run([...pre, ...air(2), ...land, ...after]), []);
// feet off (bail / hand spin): never
assert.deepEqual(run([...pre, ...air(7, { pads: false }), ...land, ...after]), []);
// not balancing
assert.deepEqual(run([...pre, ...air(7, { running: false }), ...land, ...after]), []);
// wheelslip on loose ground with the motor pushing hard (tire loaded): never starts
assert.deepEqual(run([...pre, ...air(7, { amps: 40 }), ...after]), []);
// slip that just stops, no landing: not air
assert.deepEqual(run([...pre, ...air(7), ...after]), []);
// a data gap mid-air: doesn't count
const d = new AirDetector(); let got = 0, t = 0;
for (const s of [...pre, ...air(4)]) { got ||= d.feed(t, s); t += 100; }
t += 500;  // BLE hiccup
for (const s of [...air(3), ...land, ...after]) { got ||= d.feed(t, s); t += 100; }
assert.equal(got, 0);
// over 3 s: not a jump
assert.deepEqual(run([...pre, ...air(35), ...land, ...after]), []);
console.log('air tests passed');
