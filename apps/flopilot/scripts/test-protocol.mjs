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
assert.ok(Math.abs(S.speed - 5 * 0.621371) < 0.01);  // Refloat's motor.speed is km/h (no VESC values yet)
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
assert.ok(repl().some(e => e.startsWith('(set-remote-state 0.280')));
// tilt back to 0 is sent (and repeated)
sent.length = 0; S.tilt = 0; run(snd, 3000);
assert.ok(repl().filter(e => e === '(set-remote-state 0.000 0 0 0 0)').length >= 2);
// a 64-entry MIXER take still fits one command
sent.length = 0;
snd.melody(Array.from({ length: 64 }, (_, k) => [k % 2 ? 0 : 10, 2000]), 1);
run(snd, 700);
assert.ok(repl()[0].length < 500);
// MIXER live tiles: RF Glow board = its live tone every 100 ms, then level 0;
// LIVE ON PHONE = the phone plays it; LIVE ON BOARD is the default
const glowTones = () => sent.map(b => body([...b])).filter(p => p[0] === Comm.customAppData && p[2] === 125).map(p => p[3]);
S.mode = 'ready'; S.horn = false;
globalThis.localStorage = { getItem: () => 'phone', setItem() {} };  // LIVE ON PHONE
board.pkg = { name: 'Refloat', version: '1.2.3' };
assert.equal(snd.live(392, 1, 3), false);
board.pkg = { name: 'Refloat Glow', version: '1.2.3-rf181' };
assert.equal(snd.live(392, 1, 3), false);
delete globalThis.localStorage;  // default: LIVE ON BOARD
sent.length = 0;
assert.equal(snd.live(392, 1, 3), true);
run(snd, 500); snd.liveStop(); run(snd, 300);
const g = glowTones();
assert.ok(g.filter(x => x === 3).length >= 4 && g.filter(x => x === 3).length <= 6, 'tone every 100 ms: ' + g.join());
assert.equal(g[g.length - 1], 0);
board.pkg = null;
delete globalThis.localStorage;
Date.now = realNow;
delete globalThis.window;

// LEDS view + SLCM settings (lights.js), checked against RF Glow's ledSender.send()
const { vpSettings, pickSetup, isSlcm } = await import('../src/lights.js');
assert.ok(isSlcm('VPBL-r37fc707') && isSlcm('VP-abc') && !isSlcm('Float LCM') && !isSlcm(''));
assert.equal(pickSetup({ hw: 2, lcmName: 'VPBL-r37', rfGlow: true }), 'slcm');
assert.equal(pickSetup({ hw: 2, lcmName: 'VPBL-r37', rfGlow: false }), 'lcm');  // stock: SLCM features need RF Glow; brightness only
assert.equal(pickSetup({ hw: 2, lcmName: 'GTV_1_2_0', rfGlow: true }), 'lcm');  // Dado's Floatwheel firmware
assert.equal(pickSetup({ hw: 3, lcmName: '', rfGlow: false }), 'lcm');
assert.equal(pickSetup({ hw: 1, lcmName: '', rfGlow: false }), 'refloat');
assert.equal(pickSetup({ hw: 1, lcmName: '', rfGlow: false, rgb: true }), 'refloat');
assert.equal(pickSetup({ hw: 3, lcmName: 'VPBL-r37', rfGlow: true, rgb: true }), 'refloat');  // SLCM + strips: Refloat's strips view
assert.equal(pickSetup({ hw: 2, lcmName: 'VPBL-r37', rfGlow: true, rgb: false, status: false }), 'slcm');
assert.equal(pickSetup({ hw: 0, lcmName: '', rfGlow: false }), 'none');
assert.equal(pickSetup({ hw: null }), null);
const vp = vpSettings({ ledHead: true, ledBar: true, ledBeep: true, snzStyle: 2, ledHeadPct: 80, ledIdlePct: 70, ledRidePct: 60,
  ledAnimIdle: 3, ledAnimRide: 1, ledAnimDoze: 0, ledAnimBoot: 11, ledAnimShut: 0, ledColor: 300, ledHeadIdlePct: 50,
  ledSnoozeMin: 5, rideMode: 2, stSnz: 4, ride: { c1: 4, c2: 0, speed: 1.5 }, snz: { c1: 6, c2: 1, speed: 2 }, snzAdv: true });
assert.deepEqual([...vp], [39, 80, 70, 60, 3, 1, 0, 11, 0, 1, 44, 50, 0x24, 1, 0, 150, 39, 29, 60, 0, 5, 2, 2, 4, 4, 0, 15, 6, 1, 20]);
// the light module's name from COMMAND_LCM_DEVICE_INFO
board.onPacket(Uint8Array.from([Comm.customAppData, Rf.pkgId, 27, ...[...'VPBL-r37'].map(c => c.charCodeAt(0)), 0]));
assert.equal(board.lcmName, 'VPBL-r37');

// finding the motor controller behind a VESC Express (Thor etc.)
{
  const { Board, parseFw } = await import('../src/board.js');
  const fwReply = (hw, type) => Uint8Array.from([Comm.fwVersion, 6, 5, ...[...hw].map(c => c.charCodeAt(0)), 0, ...Array(12).fill(7), 0, 0, type]);
  assert.deepEqual(parseFw(fwReply('VESC Express T', 2)), { major: 6, minor: 5, hw: 'VESC Express T', hwType: 2 });
  const pl = b => (b[0] === 2 ? b.slice(2, 2 + b[1]) : b.slice(3, 3 + ((b[1] << 8) | b[2])));
  const out = [];
  const tb = new Board({ send: b => out.push(pl([...b])) });
  const realNow2 = Date.now; let t2 = 5e6; Date.now = () => t2;
  const step = () => { t2 += 600; tb.tick(null); };
  step();
  assert.deepEqual(out.pop(), [Comm.fwVersion]);            // who's there?
  tb.onPacket(fwReply('VESC Express T', 2));                 // an Express
  step();
  assert.deepEqual(out.pop(), [Comm.pingCan]);               // ping the CAN bus
  tb.onPacket(Uint8Array.from([Comm.pingCan, 3, 10]));       // ids 3 and 10
  step();
  assert.deepEqual(out.pop(), [Comm.forwardCan, 3, Comm.fwVersion]);
  tb.onPacket(fwReply('BMS', 1));                            // 3 is a BMS: next
  step();
  assert.deepEqual(out.pop(), [Comm.forwardCan, 10, Comm.fwVersion]);
  tb.onPacket(fwReply('THOR400', 0));                        // 10 is the controller
  assert.equal(tb.link, 'ok'); assert.equal(tb.fwd, 10); assert.deepEqual([tb.fw.major, tb.fw.minor], [6, 5]);
  out.length = 0; step();
  assert.ok(out.length > 0 && out.every(p => p[0] === Comm.forwardCan && p[1] === 10), 'everything forwarded to id 10');
  // a plain controller: direct, nothing forwarded
  const out2 = [];
  const db = new Board({ send: b => out2.push(pl([...b])) });
  t2 += 600; db.tick(null); db.onPacket(fwReply('60_MK6', 0));
  assert.equal(db.fwd, null); assert.equal(db.link, 'ok');
  out2.length = 0; t2 += 600; db.tick(null);
  assert.ok(out2.every(p => p[0] !== Comm.forwardCan));
  Date.now = realNow2;
}

console.log('protocol tests passed');

// tilt slider: the same angle on every board (Refloat: angle = invert(deadband(input)) x limit)
{
  const { Sound } = await import('../src/sound.js');
  const s = new Sound({}, () => {});
  clearInterval(s.timer);
  const angle = (cfg, t) => {
    s.ledSync = { get: n => cfg[n] };
    const raw = s.tiltValue({ tilt: t });
    let v = Math.abs(raw) < cfg.inputtilt_deadband ? 0 : Math.sign(raw) * (Math.abs(raw) - cfg.inputtilt_deadband) / (1 - cfg.inputtilt_deadband);
    if (cfg.inputtilt_invert_throttle) v = -v;
    return v * cfg.inputtilt_angle_limit;
  };
  const stock = { inputtilt_angle_limit: 10, inputtilt_deadband: 0.1, inputtilt_invert_throttle: true };
  for (const cfg of [stock, { inputtilt_angle_limit: 5, inputtilt_deadband: 0.5, inputtilt_invert_throttle: false },
    { inputtilt_angle_limit: 20, inputtilt_deadband: 0, inputtilt_invert_throttle: 1 }]) {
    assert.ok(Math.abs(angle(cfg, 1) - angle(stock, 1)) < 1e-6, 'same angle at full slider');
    assert.ok(Math.abs(Math.abs(angle(cfg, 1)) - 4) < 1e-6, '4 deg at full slider');
    assert.ok(Math.abs(angle(cfg, -0.5) + angle(cfg, 0.5)) < 1e-6);
  }
  assert.ok(angle(stock, 1) < 0, 'direction as before on stock settings');
  s.ledSync = null;
  assert.equal(s.tiltValue({ tilt: 0 }), 0);
}
console.log('tilt slider tests passed');
