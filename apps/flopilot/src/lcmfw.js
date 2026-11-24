// SPDX-License-Identifier: GPL-3.0-or-later
// LIGHT MODULE (SLCM) firmware: check what's fitted and update it over
// Bluetooth, the way RF Glow's own screen does (ui.qml.in lcmFw / updater).
// The phone stages SLCM.hex's app image with RF Glow's COMMAND_LCM_FW relay;
// RF Glow raises the update flag (board not running only), the SLCM hands
// over to its bootloader, which pulls the image page by page; the phone keeps
// the two pages RF Glow holds filled (see src/lcm_fw.h, lcm/bootloader/README.md).
// Image: SLCM.hex from the latest tagged release (the rider channel).
import { CapacitorHttp } from '@capacitor/core';
import { Rf } from './vesc.js';
import { REPO } from './pkg.js';

const LCM_FW = 128;  // RF Glow COMMAND_LCM_FW
const API = 'https://api.github.com/repos/' + REPO + '/releases/latest';
const RF_GLOW = 'Refloat Glow';

// ---------- SLCM.hex (Intel hex) -> app image, as lcmFw.parseHex ----------
export function crc16(b, n) {
  let crc = 0;
  for (let i = 0; i < n; i++) {
    crc ^= b[i] << 8;
    for (let k = 0; k < 8; k++) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
  }
  return crc;
}

export function parseHex(text, name) {
  const APP = 0x08000600, END = 0x08003F80;
  const mem = new Map();
  let base = 0, maxA = -1, n = 0;
  const lines = text.split(/\r?\n/);
  for (let li = 0; li < lines.length; li++) {
    const l = lines[li].trim();
    if (l === '') continue;
    if (l[0] !== ':' || l.length < 11 || (l.length - 1) % 2) return { error: 'Not an Intel .hex file (line ' + (li + 1) + ').' };
    const r = [];
    for (let i = 1; i < l.length; i += 2) {
      const v = parseInt(l.substr(i, 2), 16);
      if (isNaN(v)) return { error: 'Not an Intel .hex file (line ' + (li + 1) + ').' };
      r.push(v);
    }
    let sum = 0;
    for (const v of r) sum = (sum + v) & 255;
    if (sum !== 0 || r.length !== r[0] + 5) return { error: 'Checksum error in the .hex (line ' + (li + 1) + ').' };
    const type = r[3], addr = (r[1] << 8) | r[2];
    if (type === 0) {
      for (let i = 0; i < r[0]; i++) {
        const a = base + addr + i;
        if (a >= APP && a < END) { mem.set(a, r[4 + i]); if (a > maxA) maxA = a; n++; }
        else if (a >= END && a < 0x08004000) return { error: 'This .hex writes the battery LUT page: not an update image.' };
      }
    } else if (type === 4) base = ((r[4] << 8) | r[5]) * 65536;
    else if (type === 2) base = ((r[4] << 8) | r[5]) * 16;
    else if (type === 1) break;
  }
  if (n === 0) return { error: 'No firmware at 0x08000600: not an SLCM (bootloader) build.' };
  const size = Math.ceil((maxA + 1 - APP) / 128) * 128;
  const b = new Uint8Array(size);
  for (let i = 0; i < size; i++) b[i] = mem.has(APP + i) ? mem.get(APP + i) : 0xff;
  const sp = (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) >>> 0;
  const pc = (b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24)) >>> 0;
  if (sp < 0x20000001 || sp > 0x20000800 || !(pc & 1) || pc < APP || pc >= END) {
    return { error: "The image doesn't look like an SLCM app built for the bootloader." };
  }
  let slcm = '';
  for (let i = 0; i + 6 < size && !slcm; i++) {
    if (b[i] === 86 && b[i + 1] === 80 && b[i + 2] === 66 && b[i + 3] === 76 && b[i + 4] === 45) {  // "VPBL-"
      let j = i + 5, v = '';
      while (j < size && j < i + 25 && b[j] >= 45 && b[j] <= 122) v += String.fromCharCode(b[j++]);
      slcm = v;
    }
  }
  return { size, crc: crc16(b, size), file: name, slcm, bytes: b };
}

// "r37-fc70747" -> { rev: 37, hash }; anything else: rev -1
export function parseVer(v) {
  const m = /^r(\d+)-([0-9a-f]+)$/.exec(v || '');
  return m ? { rev: parseInt(m[1], 10), hash: m[2] } : { rev: -1, hash: v || '' };
}
export const verText = v => (v.rev > 0 ? 'r' + v.rev + ' (' + v.hash + ')' : v.hash || 'unknown');
export function fitOf(lcm, ref) {
  if (!lcm || !ref) return '';
  if (lcm.hash && lcm.hash === ref.hash) return 'same';
  if (lcm.rev > 0 && ref.rev > 0) return lcm.rev < ref.rev ? 'older' : lcm.rev > ref.rev ? 'newer' : 'other';
  return lcm.rev < 0 && ref.rev !== -1 ? 'older' : 'other';
}

const ERR = ['', 'The board is running. Stop and try again.', 'The firmware image is broken (bad size).',
  "The LCM didn't start updating: it has no bootloader.", 'The LCM got nothing and went back to its old firmware.',
  'Checksum mismatch: the LCM is retrying.'];

// ---------- the updater ----------
export class LcmUpdater {
  constructor(link) {
    this.link = link;
    this.board = null;
    this.status = null;     // last STATUS: { state, error, want, flags }
    this.active = false;
    this.progress = 0;
    this.message = '';
    this.timer = null;
    this.onChange = () => {};
  }

  attach(b) {
    if (this.board === b) return;
    this.board = b;
    this.status = null;
    if (b) b.onLcmFw = p => this.onReply(p);
  }

  send(sub, extra = []) { if (this.board) this.board.app([LCM_FW, sub, ...extra]); }
  ask() { if (this.board) { this.board.app([Rf.lcmDeviceInfo]); this.send(3); } }

  get name() { return (this.board && this.board.lcmName) || ''; }
  get rfGlow() { const b = this.board; return !!(b && b.pkg && b.pkg.name === RF_GLOW); }
  get blActive() { return !!(this.status && this.status.flags & 1); }
  get polling() { return !!(this.status && this.status.flags & 4); }
  get hasBootloader() { return this.name.startsWith('VPBL-'); }
  get fitted() {
    const n = this.name;
    if (this.blActive && !this.hasBootloader) return 'recovery';
    if (this.hasBootloader) return 'vpbl';
    if (n.startsWith('VP-')) return 'vp';
    if (n) return 'other';
    return this.polling && !this.blActive ? 'legacy' : 'none';
  }
  get capable() { return ['vpbl', 'recovery', 'legacy'].includes(this.fitted); }
  get lcmVer() { const n = this.name; return parseVer(n.startsWith('VPBL-') ? n.slice(5) : n.startsWith('VP-') ? n.slice(3) : ''); }

  onReply(p) {
    // [36?] stripped by the board: p = payload after package id: [128, 3, state, error, want u32, size u32, flags]
    if (p[1] !== 3 || p.length < 12) return;
    const u32 = i => ((p[i] << 24) | (p[i + 1] << 16) | (p[i + 2] << 8) | p[i + 3]) >>> 0;
    this.status = { state: p[2], error: p[3], want: u32(4), flags: p.length > 12 ? p[12] : 0 };
    this.noReply = 0;
    if (this.active) this.step();
    this.onChange();
  }

  sendPage(off) {
    const img = this.image;
    if (off < 0 || off >= img.size) return;
    const d = [(off >>> 24) & 255, (off >>> 16) & 255, (off >>> 8) & 255, off & 255];
    for (let i = 0; i < 128; i++) d.push(img.bytes[off + i]);
    this.send(2, d);
  }

  step() {
    const st = this.status, img = this.image, pages = img.size / 128;
    if (st.state === 1) {
      this.message = 'Waiting for the LCM to start its bootloader…';
      this.sendPage(128); this.sendPage(256);
    } else if (st.state === 2) {
      const done = st.want === 0 ? pages - 1 : Math.max(0, st.want / 128 - 1);
      this.progress = done / pages;
      this.message = (st.error === 5 ? ERR[5] + ' ' : '') + 'Updating the LCM: ' + Math.round(this.progress * 100) + '%';
      this.sendPage(st.want);
      // and the next one - unless it shares RF Glow's slot with the wanted page
      // (two slots, by page number: with an odd page count the last page and
      // page 0 share one, and sending page 0 would overwrite the last page)
      const next = st.want === 0 ? -1 : st.want + 128 < img.size ? st.want + 128 : 0;
      if (next >= 0 && ((next / 128) & 1) !== ((st.want / 128) & 1)) this.sendPage(next);
    } else if (st.state === 3) {
      this.finish(true, 'Done: the LCM firmware is updated and running.');
    } else if (st.state === 4) {
      this.finish(false, ERR[st.error] || 'The update failed.');
    }
  }

  start(image) {
    if (this.active) return;
    this.image = image;
    this.active = true;
    this.link.lcmFlashing = true;
    this.progress = 0;
    this.noReply = 0;
    this.message = 'Starting…';
    this.send(1, [(image.size >>> 24) & 255, (image.size >>> 16) & 255, (image.size >>> 8) & 255, image.size & 255, (image.crc >> 8) & 255, image.crc & 255]);
    this.timer = setInterval(() => {
      if (++this.noReply > 150) {  // ~12 s without a reply
        this.finish(false, 'Connection lost. The LCM waits: reconnect and update again (or power-cycle the board).');
        return;
      }
      this.send(3);
    }, 80);
    this.onChange();
  }

  cancel() {
    this.send(4);
    this.finish(false, 'Cancelled. If the LCM had started, power-cycle the board and update again.');
  }

  finish(ok, msg) {
    clearInterval(this.timer);
    this.timer = null;
    this.active = false;
    this.link.lcmFlashing = false;
    this.done = ok;
    if (ok) this.progress = 1;
    this.message = msg;
    this.onChange();
  }
}

// ---------- latest release: SLCM.hex + its version (slcm-r<N>-<hash>.txt) ----------
export async function fetchRelease() {
  try {
    const r = await CapacitorHttp.get({ url: API, headers: { Accept: 'application/vnd.github+json' } });
    if (r.status === 404) return { none: true };
    if (r.status !== 200) return { error: 'GitHub answered ' + r.status };
    const d = typeof r.data === 'string' ? JSON.parse(r.data) : r.data;
    const out = { tag: d.tag_name, hexUrl: '', slcm: '' };
    for (const a of d.assets || []) {
      if (a.name === 'SLCM.hex') out.hexUrl = a.browser_download_url;
      const m = /^slcm-(r\d+-[0-9a-f]+)\.txt$/.exec(a.name || '');
      if (m) out.slcm = m[1];
    }
    return out;
  } catch (e) {
    return { error: 'No internet? ' + (e && e.message || e) };
  }
}
export async function downloadHex(rel) {
  const r = await CapacitorHttp.get({ url: rel.hexUrl, responseType: 'text' });
  if (r.status !== 200) throw new Error('download failed (' + r.status + ')');
  const h = parseHex(String(r.data), 'SLCM.hex (' + rel.tag + ')');
  if (h.error) throw new Error(h.error);
  return h;
}
