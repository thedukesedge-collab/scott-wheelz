// Protocol self-test (no phone needed): framing round trip, CRC, and a fake
// Refloat board answering INFO / IDS / REALTIME, checked against S.
import assert from 'node:assert/strict';
import { crc16, encodePacket, PacketDecoder, Comm, Rf } from '../src/vesc.js';
import { Board } from '../src/board.js';

// CRC-16/XMODEM check value
assert.equal(crc16([...'123456789'].map(c => c.charCodeAt(0))), 0x31c3);

// framing round trip, split into BLE-sized chunks with noise in front
const got = [];
const dec = new PacketDecoder(p => got.push([...p]));
const pkt = encodePacket([1, 2, 3, 250]);
dec.add([0, 7, 9]);
dec.add(pkt.slice(0, 3));
dec.add(pkt.slice(3));
assert.deepEqual(got, [[1, 2, 3, 250]]);

// half floats as Refloat sends them
function half(v) {
  const f = new Float32Array([v]), x = new Uint32Array(f.buffer)[0];
  const sign = (x >> 16) & 0x8000; let e = ((x >> 23) & 0xff) - 127 + 15; const m = x & 0x7fffff;
  if (e <= 0) return sign;
  return sign | (e << 10) | ((m + 0x1000) >> 13);
}
const lenStr = s => [s.length, ...[...s].map(c => c.charCodeAt(0))];

// fake board: answers the app's polls
const sent = [];
let board;
const answer = payload => board.receive(encodePacket(payload));
board = new Board({ send: b => sent.push(b) });
const S = { tilt: 0, horn: false, volts: 80, motorT: 20, escT: 25 };

board.tick(S);
answer([Comm.customAppData, Rf.pkgId, Rf.info, 2, 0, ...new Array(47).fill(0), 0, 0, 0x27, 0x10]);
assert.equal(board.comms, 'ids');
board.tick(S);
const idle = ['motor.speed', 'motor.duty_cycle', 'imu.pitch', 'imu.roll'];
answer([Comm.customAppData, Rf.pkgId, Rf.realtimeDataIds, idle.length, ...idle.flatMap(lenStr), 1, ...lenStr('setpoint')]);
assert.equal(board.comms, 'ready');
const vals = [5, 0.5, 3, -2, 1.5].map(half);
const u16 = v => [(v >> 8) & 0xff, v & 0xff];
answer([Comm.customAppData, Rf.pkgId, Rf.realtimeData, 1, 0, 0, 0, 0, 1, (Rf.mNormal << 4) | Rf.sRunning, 3 << 6, 0, 0, ...vals.flatMap(u16)]);
assert.ok(board.live);
board.apply(S);
assert.equal(S.mode, 'running');
assert.ok(Math.abs(S.speed - 5 * 2.23694) < 0.01);
assert.equal(S.duty, 0.5);
assert.equal(S.pitch, 3);
assert.equal(S.roll, -2);
assert.equal(S.setpoint, 1.5);
assert.ok(S.fpL && S.fpR);

// horn / melody / tilt through the Lisp REPL (sound.js), against the firmware's
// rules: one command at a time, >= 0.5 s apart, memory checked first
const { Sound, REPL_GAP, INSTALL_CMDS } = await import('../src/sound.js');
const body = b => b.slice(b[0] === 2 ? 2 : 3);  // payload (2- or 3-byte header)
const repl = () => sent.map(b => body([...b])).filter(p => p[0] === Comm.lispReplCmd)
  .map(p => String.fromCharCode(...p.slice(1, p.indexOf(0, 1))));
const statsAsks = () => sent.map(b => [...b]).filter(b => b[2] === Comm.lispStats).length;
let HP = { hAdv: true, hBeep: 9, hBeepHz: 50 };
globalThis.window = { __simP: () => HP };
let now = 1e7;
const realNow = Date.now;
Date.now = () => now;
const link = { live: true, board, S };
const run = (snd, ms) => { for (let t = 0; t < ms; t += 50) { now += 50; snd.tick(); } };
const f16 = v => [(Math.round(v * 100) >> 8) & 0xff, Math.round(v * 100) & 0xff];
const stats = (mem, heap) => board.onPacket(Uint8Array.from([Comm.lispStats, ...f16(3), ...f16(heap), ...f16(mem), 0, 0, 0]));
const print = t => board.onPacket(Uint8Array.from([Comm.lispPrint, ...[...t].map(c => c.charCodeAt(0))]));
S.mode = 'ready'; S.horn = false; S.tilt = 0;

// old firmware: nothing at all
board.fw = { major: 6, minor: 2 };
let warn = '';
let snd = new Sound(link, m => (warn = m));
clearInterval(snd.timer);
sent.length = 0; S.horn = true; run(snd, 500); S.horn = false;
assert.equal(repl().length + statsAsks(), 0);
assert.ok(warn.includes('6.05') && warn.includes('6.02'));

// memory too full (like the Thor 301 in Floaty: 69% would be fine, 90% is not)
board.fw = { major: 6, minor: 5 };
warn = '';
snd = new Sound(link, m => (warn = m)); clearInterval(snd.timer);
sent.length = 0; run(snd, 100);
assert.equal(statsAsks(), 1);
stats(90, 40); S.horn = true; run(snd, 2000); S.horn = false;
assert.equal(repl().length, 0);
assert.ok(warn.includes('memory') && warn.includes('90%'));

// normal: stats ok -> probe -> 3 install steps (each waits for its answer) -> ready
snd = new Sound(link, m => (warn = m)); clearInterval(snd.timer);
sent.length = 0; run(snd, 100); stats(69, 50);
run(snd, 100);
assert.deepEqual(repl(), ['fp-d']);
print('***\tError: variable_not_bound fp-d');
for (const [k, mark] of [[0, 'fpi1'], [1, 'fpi2'], [2, 'fpok']]) {
  run(snd, REPL_GAP + 50);
  assert.equal(repl()[repl().length - 1], INSTALL_CMDS[k]);
  print('> ' + mark);
}
assert.equal(snd.state, 'ready');
assert.ok(INSTALL_CMDS.every(c => c.length < 500), 'each install command fits a VESC packet');

// horn: keepalives never closer than the firmware's 0.5 s, then a stop
sent.length = 0; S.mode = 'running'; S.horn = true; run(snd, 2000); S.horn = false; run(snd, 1000);
const ex = repl();
const horns = ex.filter(e => e.startsWith('(fp-t'));
assert.ok(horns.length >= 3 && horns.length <= 4, 'keepalive count ' + horns.length);
assert.equal(horns[0], '(fp-t 300 4.00 3.0)');  // clamped to 300 Hz / 4 V, riding cap 3 s
assert.ok(ex.includes('(foc-play-stop)'));
const times = sent.filter(b => body([...b])[0] === Comm.lispReplCmd).length;
assert.ok(times === ex.length);
// SIMPLE horn settings: VOLUME 5 = 2 V, PITCH 10 = 1000 Hz, idle cap 2 s
HP = { hAdv: false, hsVol: 5, hsPitch: 10 };
S.mode = 'ready'; sent.length = 0; S.horn = true; run(snd, 700); S.horn = false; run(snd, 700);
assert.equal(repl()[0], '(fp-t 1000 2.00 2.0)');
// melody: one command, sent whole, nothing else while it plays
sent.length = 0;
assert.equal(snd.melody([[3, 200], [0, 100], [11, 200], [10, 2000]], 9), true);
run(snd, 700);
assert.deepEqual(repl(), ["(fp-m 2.50 '(3 10 0 5 0 10 10 100))"]);
S.tilt = 0.5; run(snd, 900);
assert.equal(repl().length, 1, 'REPL left alone while the melody plays');
run(snd, 2500);
assert.ok(repl().some(e => e.startsWith('(set-remote-state 0.200')));
// tilt back to 0 is sent (and repeated)
sent.length = 0; S.tilt = 0; run(snd, 3000);
assert.ok(repl().filter(e => e === '(set-remote-state 0.000 0 0 0 0)').length >= 2);
// a 64-entry MIXER take still fits one command
sent.length = 0;
snd.melody(Array.from({ length: 64 }, (_, k) => [k % 2 ? 0 : 10, 2000]), 1);
run(snd, 700);
assert.ok(repl()[0].length < 500);
// MIXER live tiles: RF Glow board = its live tone every 100 ms, then level 0;
// stock board = the phone plays it (default)
const glowTones = () => sent.map(b => body([...b])).filter(p => p[0] === Comm.customAppData && p[2] === 125).map(p => p[3]);
S.mode = 'ready'; S.horn = false;
board.pkg = { name: 'Refloat', version: '1.2.3' };
assert.equal(snd.live(392, 1, 3), false);
board.pkg = { name: 'Refloat Glow', version: '1.2.3-rf181' };
sent.length = 0;
assert.equal(snd.live(392, 1, 3), true);
run(snd, 500); snd.liveStop(); run(snd, 300);
const g = glowTones();
assert.ok(g.filter(x => x === 3).length >= 4 && g.filter(x => x === 3).length <= 6, 'tone every 100 ms: ' + g.join());
assert.equal(g[g.length - 1], 0);
board.pkg = null;
Date.now = realNow;
delete globalThis.window;

console.log('protocol tests passed');
