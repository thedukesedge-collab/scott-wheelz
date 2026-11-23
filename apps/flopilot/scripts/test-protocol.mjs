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

// horn / melody through VESC firmware tones (sound.js): watchdog installed once,
// clamped values, a stop on release, melodies refused while running
const { Sound } = await import('../src/sound.js');
const repl = () => sent.map(b => [...b]).filter(b => b[2] === Comm.lispReplCmd)
  .map(b => String.fromCharCode(...b.slice(3, b.indexOf(0, 3))));
globalThis.window = { __simP: () => ({ hBeep: 9, hBeepHz: 50 }) };
const link = { live: true, board, S };
const snd = new Sound(link);
clearInterval(snd.timer);
sent.length = 0;
S.mode = 'running'; S.horn = true; snd.tick(); snd.tick(); S.horn = false; snd.tick();
let ex = repl();
assert.ok(ex.some(e => e.startsWith('(defun fp-dog')) && ex.some(e => e.startsWith('(defun fp-tone')));
assert.equal(ex.filter(e => e.startsWith('(defun')).length, 2);
assert.deepEqual(ex.filter(e => e.startsWith('(fp-tone')), ['(fp-tone 300 4.00 3.0)', '(fp-tone 300 4.00 3.0)']);
assert.equal(ex[ex.length - 1], '(foc-play-stop)');
assert.equal(snd.melody([[3, 200]], 1, l => 392), false);  // riding: no melody
S.mode = 'ready'; sent.length = 0;
assert.equal(snd.melody([[3, 200], [0, 100]], 9, l => 392), true);
snd.tick();
ex = repl();
assert.deepEqual(ex, ['(fp-tone 392 2.50 4.5)']);  // volume capped at 2.5 V, idle limits
S.mode = 'running'; sent.length = 0; snd.tick();
assert.deepEqual(repl(), ['(foc-play-stop)']);  // started riding: stopped
delete globalThis.window;

console.log('protocol tests passed');
