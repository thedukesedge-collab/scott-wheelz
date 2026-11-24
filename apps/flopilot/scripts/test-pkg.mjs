// SPDX-License-Identifier: GPL-3.0-or-later
// Package installer tests: unpack a .vescpkg, write it to a pretend board
// that checks every chunk the way the VESC firmware does.
import assert from 'node:assert/strict';
import zlib from 'node:zlib';
import { readFileSync, existsSync } from 'node:fs';
import { unpack, check, framed, qmlBlob, Installer, PKG_NAME } from '../src/pkg.js';
import { Comm, crc16 } from '../src/vesc.js';

const inflate = b => new Uint8Array(zlib.inflateSync(b));
const deflate = b => new Uint8Array(zlib.deflateSync(b, { level: 9 }));

// a small package, packed like VESC Tool's packVescPackage + qCompress
function pack(fields) {
  const parts = [Buffer.from('VESC Packet\0')];
  for (const [k, v] of fields) {
    const d = Buffer.from(v);
    const len = Buffer.alloc(4); len.writeInt32BE(d.length);
    parts.push(Buffer.from(k + '\0'), len, d);
  }
  const raw = Buffer.concat(parts);
  const head = Buffer.alloc(4); head.writeUInt32BE(raw.length);
  return new Uint8Array(Buffer.concat([head, zlib.deflateSync(raw)]));
}
const lisp = Uint8Array.from({ length: 5000 }, (_, i) => (i * 31) & 0xff);
const file = pack([['name', PKG_NAME], ['lispData', lisp], ['qmlFile', 'Item { }'], ['qmlIsFullscreen', [1]]]);
const pkg = unpack(file, inflate);
assert.equal(pkg.name, PKG_NAME);
assert.equal(check(pkg), '');
assert.equal(pkg.qmlIsFullscreen, true);
assert.deepEqual([...pkg.lispData], [...lisp]);
assert.ok(check({ name: 'Refloat', lispData: lisp, qmlFile: 'x' }).includes('not RF Glow'));

// qml blob = qCompress of the two imports + the file
const blob = qmlBlob('Item { }', deflate);
assert.equal(Buffer.from(zlib.inflateSync(blob.subarray(4))).toString(), 'import Vedder.vesc.vescinterface 1.0;import "qrc:/mobile";Item { }');

// pretend board: erase, then writes at the right offsets, rebuilt and crc-checked
const flash = { [Comm.qmlWrite]: [], [Comm.lispWrite]: [] };
let erased = [], running = false, dropOnce = true;
const board = {
  send(p) {
    const reply = r => setTimeout(() => board.onInstallReply(Uint8Array.from(r)), 1);
    const off = ((p[1] << 24) | (p[2] << 16) | (p[3] << 8) | p[4]) >>> 0;
    if (p[0] === Comm.qmlErase || p[0] === Comm.lispErase) { erased.push(p[0]); reply([p[0], 1]); }
    else if (p[0] === Comm.qmlWrite || p[0] === Comm.lispWrite) {
      if (dropOnce && off === 384) { dropOnce = false; return; }  // a lost packet: must be resent
      flash[p[0]][off] = p.slice(5);
      reply([p[0], 1, p[1], p[2], p[3], p[4]]);
    } else if (p[0] === Comm.lispSetRunning) running = p[1] === 1;
  },
};
const joined = cmd => Uint8Array.from(Object.keys(flash[cmd]).map(Number).sort((a, b) => a - b).flatMap(k => [...flash[cmd][k]]));
let last = 0;
await new Installer(board).install(pkg, deflate, f => { assert.ok(f >= last); last = f; });
assert.deepEqual(erased, [Comm.qmlErase, Comm.lispErase]);
assert.ok(running && last === 1);
const lw = joined(Comm.lispWrite);
assert.deepEqual([...lw], [...framed(lisp)]);
const dv = new DataView(lw.buffer);
assert.equal(dv.getUint32(0), lisp.length - 2);
assert.equal(dv.getUint16(4), crc16(lisp));
const qw = joined(Comm.qmlWrite);
assert.deepEqual([...qw.subarray(6, 8)], [0, 2]);  // fullscreen flag

// a board that never answers: a clear error, not a hang
const dead = { send() {} };
await assert.rejects(new Installer(dead).install(pkg, deflate, () => {}), /did not answer/);

// the real package, when one is around (local runs)
const real = '/tmp/pk/p.vescpkg';
if (existsSync(real)) {
  const rp = unpack(new Uint8Array(readFileSync(real)), inflate);
  assert.equal(check(rp), '');
  const q = qmlBlob(rp.qmlFile, deflate);
  console.log('real package: lisp ' + rp.lispData.length + ' B, qml ' + q.length + ' B compressed');
  assert.ok(q.length + 8 < 120 * 1024);
}
console.log('package tests passed');
