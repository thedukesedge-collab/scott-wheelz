// SPDX-License-Identifier: GPL-3.0-or-later
// PACKAGE tab (gear icon): install the latest FloPilot package release on the
// board, the way VESC Tool installs a .vescpkg (vesc_tool codeloader.cpp):
// QML UI erase + write, LispBM code erase + write, start the code again.
// Only the newest tagged release (the rider channel, see RELEASING.md), only
// with the board stopped and nobody on it, after a confirm screen.
import { Comm, crc16 } from './vesc.js';

export const REPO = 'thedukesedge-collab/scott-wheelz';
export const ASSET = 'refloatglow.vescpkg';   // the package file name in releases
export const PKG_NAME = 'Refloat Glow';       // the package's own name inside the file
const CHUNK = 384;                            // as VESC Tool
const QML_MAX = 120 * 1024;                   // the controller's QML area
const LISP_MAX = 128 * 1024 - 6;              // the controller's Lisp area (STM32)

// ---------- .vescpkg (qCompress'd: 4-byte length + zlib) ----------
export function unpack(file, inflate) {
  const raw = inflate(file.subarray(4));
  const dv = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  let i = 0;
  const str = () => { let s = ''; while (raw[i]) s += String.fromCharCode(raw[i++]); i++; return s; };
  if (str() !== 'VESC Packet') throw new Error('not a VESC package');
  const pkg = { qmlIsFullscreen: false };
  while (i < raw.length) {
    const name = str();
    if (!name) break;
    const len = dv.getInt32(i); i += 4;
    const data = raw.subarray(i, i + len); i += len;
    if (name === 'lispData') pkg.lispData = data;
    else if (name === 'qmlIsFullscreen') pkg.qmlIsFullscreen = data[0] !== 0;
    else if (['name', 'description', 'qmlFile'].includes(name)) pkg[name] = new TextDecoder().decode(data);
  }
  return pkg;
}

// VESC Tool's qmlCompress: two imports first, then qCompress (zlib level 9)
export function qmlBlob(qml, deflate) {
  const text = new TextEncoder().encode('import Vedder.vesc.vescinterface 1.0;import "qrc:/mobile";' + qml);
  const z = deflate(text);
  const out = new Uint8Array(4 + z.length);
  new DataView(out.buffer).setUint32(0, text.length);
  out.set(z, 4);
  return out;
}

// what goes into flash: u32 length - 2, u16 crc, data (VESC Tool lispUpload / qmlUpload)
export function framed(body) {
  const out = new Uint8Array(6 + body.length);
  const dv = new DataView(out.buffer);
  dv.setUint32(0, body.length - 2);
  dv.setUint16(4, crc16(body));
  out.set(body, 6);
  return out;
}

export function check(pkg) {
  if (pkg.name !== PKG_NAME) return 'This file is not the FloPilot package (' + (pkg.name || 'no name') + ')';
  if (!pkg.lispData || !pkg.lispData.length || !pkg.qmlFile) return 'The package file is incomplete';
  return '';
}

// ---------- writing to the board ----------
const u32 = v => [(v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff];

export class Installer {
  constructor(board) {
    this.board = board;
    this.wait = null;
    board.onInstallReply = p => {
      const w = this.wait;
      if (!w || p[0] !== w.cmd) return;
      const ok = p[1] !== 0;
      if (w.offset !== undefined) {
        const off = ((p[2] << 24) | (p[3] << 16) | (p[4] << 8) | p[5]) >>> 0;
        if (off !== w.offset) return;
      }
      this.wait = null;
      w.done(ok);
    };
  }

  ask(payload, cmd, ms, offset) {
    return new Promise(done => {
      const t = setTimeout(() => { if (this.wait && this.wait.t === t) { this.wait = null; done(null); } }, ms);
      this.wait = { cmd, offset, t, done: ok => { clearTimeout(t); done(ok); } };
      this.board.send(payload);
    });
  }

  async erase(cmd, size, ms) {
    const ok = await this.ask([cmd, ...u32(size)], cmd, ms);
    if (ok !== true) throw new Error(ok === null ? 'the board did not answer the erase' : 'the board could not erase');
  }

  async write(cmd, data, progress) {
    for (let off = 0; off < data.length; off += CHUNK) {
      const part = data.subarray(off, off + CHUNK);
      let ok = null;
      for (let tries = 0; tries < 5 && ok === null; tries++) ok = await this.ask([cmd, ...u32(off), ...part], cmd, 1500, off);
      if (ok !== true) throw new Error('writing failed at ' + Math.round(100 * off / data.length) + '%');
      progress(Math.min(data.length, off + CHUNK));
    }
  }

  // pkg from unpack(); deflate(Uint8Array) -> zlib bytes; progress(0..1, text)
  async install(pkg, deflate, progress) {
    const qml = framed(Uint8Array.from([0, pkg.qmlIsFullscreen ? 2 : 1, ...qmlBlob(pkg.qmlFile, deflate)]));
    const lisp = framed(pkg.lispData);
    if (qml.length > QML_MAX) throw new Error('the package screen is too big for the controller');
    if (lisp.length > LISP_MAX) throw new Error('the package code is too big for the controller');
    const total = qml.length + lisp.length;
    progress(0, 'Clearing the old screen…');
    await this.erase(Comm.qmlErase, qml.length + 100, 8000);
    await this.write(Comm.qmlWrite, qml, n => progress(n / total, 'Writing the screen…'));
    progress(qml.length / total, 'Clearing the old package…');
    await this.erase(Comm.lispErase, pkg.lispData.length + 100, 10000);
    await this.write(Comm.lispWrite, lisp, n => progress((qml.length + n) / total, 'Writing the package…'));
    this.board.send([Comm.lispSetRunning, 1]);
    progress(1, 'Starting the package…');
  }
}
