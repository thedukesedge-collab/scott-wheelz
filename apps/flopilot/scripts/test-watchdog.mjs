// SPDX-License-Identifier: GPL-3.0-or-later
// WATCHDOG (Ghosting Protection): monitor levels, unloaded rule, both-stuck,
// emergency plan refusals, 5.0 V written past the 3.3 V limit, protected keys.
import assert from 'node:assert/strict';
import { Monitor, classify, emergencyPlan, drift, DEFAULTS, EMERG_V } from '../src/watchdog.js';
import { RefloatConfig } from '../src/refcfg.js';
import { LedSync } from '../src/ledsync.js';
import layout from '../src/refloat-layout.json';

const t = DEFAULTS;
assert.equal(classify(0.1, 2.5, t), 'ok');
assert.equal(classify(0.3, 2.5, t), 'early');
assert.equal(classify(1.4, 2.5, t), 'high');
assert.equal(classify(2.6, 2.5, t), 'crit');
assert.equal(classify(3.1, 0, t), 'high');  // pad off in Refloat: never "stuck"

// parked on the tail, no rider: ADC1 stuck at 3.2 V for 10 s
const base = { fresh: true, running: false, speed: 0, pitch: 16, a1: 3.2, a2: 0.05, sw1: 2.5, sw2: 2.5 };
let m = new Monitor(), ev = [];
for (let s = 0; s <= 10; s++) ev.push(...m.tick(s * 1000, base));
assert.deepEqual(ev.map(e => [e.pad, e.level]), [[1, 'crit']]);
// spike shorter than 10 s: nothing
m = new Monitor(); ev = [];
for (let s = 0; s < 30; s++) ev.push(...m.tick(s * 1000, Object.assign({}, base, { a1: s % 8 < 5 ? 3.2 : 0.05 })));
assert.equal(ev.length, 0);
// level board (a rider could be standing still): never judged
m = new Monitor(); ev = [];
for (let s = 0; s < 30; s++) ev.push(...m.tick(s * 1000, Object.assign({}, base, { pitch: 1 })));
assert.equal(ev.length, 0); assert.equal(m.status, 'wait');
// balancing / rolling: never judged
m = new Monitor(); ev = [];
for (let s = 0; s < 30; s++) ev.push(...m.tick(s * 1000, Object.assign({}, base, { running: true })));
assert.equal(ev.length, 0);
// no data: not "protected"
m = new Monitor();
m.tick(0, Object.assign({}, base, { fresh: false }));
assert.equal(m.status, 'nodata');
// both stuck: each pad, then the "both" alarm (pad 0)
m = new Monitor(); ev = [];
for (let s = 0; s <= 10; s++) ev.push(...m.tick(s * 1000, Object.assign({}, base, { a2: 3.0 })));
assert.ok(ev.some(e => e.pad === 0 && e.level === 'crit'));
// early warning, then back to normal
m = new Monitor(); ev = [];
for (let s = 0; s <= 10; s++) ev.push(...m.tick(s * 1000, Object.assign({}, base, { a1: 0.35 })));
for (let s = 11; s <= 22; s++) ev.push(...m.tick(s * 1000, base && Object.assign({}, base, { a1: 0.05 })));
assert.deepEqual(ev.map(e => e.level), ['early', 'ok']);

// emergency plan
const ok = emergencyPlan({ bad: 1, a1: 3.2, a2: 0.05, sw1: 2.5, sw2: 2.5, posi: false, running: false, speed: 0, early: 0.2 });
assert.deepEqual(ok.values, { fault_adc1: EMERG_V, fault_is_dual_switch: true });
assert.deepEqual(ok.orig, { fault_adc1: 2.5, fault_adc2: 2.5, fault_is_dual_switch: false });
const no = o => emergencyPlan(Object.assign({ bad: 1, a1: 3.2, a2: 0.05, sw1: 2.5, sw2: 2.5, posi: false, running: false, speed: 0, early: 0.2 }, o)).err;
assert.ok(no({ running: true }));
assert.ok(no({ speed: 3 }));
assert.ok(no({ a2: 1.5 }), 'good pad high: refuse');
assert.ok(no({ sw2: 0 }), 'good pad off: refuse');
assert.ok(no({ a1: 4.2 }), 'bad pad near 5 V: refuse');
assert.ok(no({ a1: NaN }));

// 5.0 V really goes into the config (int16 x1000), 3.3 V limit otherwise
const raw = new Uint8Array(4 + 4096);
new DataView(raw.buffer).setUint32(0, layout.signature);
let c = new RefloatConfig(raw);
if (!c.ok) {  // build a config of the exact size
  let n = 4; for (const f of layout.fields) n += f.t === 1 ? (f.x === 7 ? 2 : 4) : f.t === 2 ? ({ 1: 1, 2: 1, 3: 2, 4: 2, 5: 4, 6: 4 })[f.x] : f.t === 3 ? 1 : 1;
  const r = new Uint8Array(n); new DataView(r.buffer).setUint32(0, layout.signature); c = new RefloatConfig(r);
}
assert.ok(c.ok);
c.set('fault_adc1', 5.0); assert.equal(c.get('fault_adc1'), 3.3);
c.set('fault_adc1', 5.0, true); assert.equal(c.get('fault_adc1'), 5.0);

// protected keys: a tune can't put the footpad settings back
const sent = [];
const ls = new LedSync({ getP: () => null, render() {}, toast() {} });
ls.board = { pkgState: 2, setConfig: r => sent.push(r) };
ls.cfg = c;
ls.protect = new Map([['fault_adc1', EMERG_V], ['fault_is_dual_switch', true]]);
ls.writeValues({ fault_adc1: 2.5, kp: 21 }, 'tune');
assert.equal(new RefloatConfig(sent[0]).get('fault_adc1'), 5.0);
ls.pending = null;
ls.writeValues({ fault_adc1: 2.5 }, 'restore', null, { force: true });
assert.equal(new RefloatConfig(sent[1]).get('fault_adc1'), 2.5);

// resting drift vs last week
const days = { '2026-11-20': { s1: 1, s2: 1, n: 10 }, '2026-11-21': { s1: 1, s2: 1, n: 10 }, '2026-11-24': { s1: 3, s2: 1, n: 10 } };
const [d1, d2] = drift(days, '2026-11-24');
assert.ok(Math.abs(d1 - 0.2) < 1e-9 && Math.abs(d2) < 1e-9);
console.log('watchdog tests passed');
