// VESC packet framing + byte helpers (ported from the Flutter app's
// lib/vesc/packet.dart and buffers.dart). No platform code here, so it can be
// tested with plain node (scripts/test-protocol.mjs).

export const Comm = {
  fwVersion: 0,
  getValuesSetup: 47,
  customAppData: 36,
  lispReplCmd: 138,
  getCustomConfig: 93,
  setCustomConfig: 95,
};

// Refloat custom app data (package id 101), as in the package UI
export const Rf = {
  pkgId: 101,
  info: 0,
  realtimeData: 31,
  realtimeDataIds: 32,
  lights: 20,  // Refloat COMMAND_LIGHTS_CONTROL (stock 1.2.3): u32 mask, u8 value -> u8 state
  horn: 120,
  sDisabled: 0, sStartup: 1, sReady: 2, sRunning: 3,
  mNormal: 0, mHandtest: 1, mFlywheel: 2,
};

export function crc16(data) {
  let crc = 0;
  for (const b of data) {
    crc ^= (b & 0xff) << 8;
    for (let i = 0; i < 8; i++) {
      crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff;
    }
  }
  return crc;
}

export function encodePacket(payload) {
  const len = payload.length;
  const head = len < 256 ? [2, len] : len < 65536 ? [3, (len >> 8) & 0xff, len & 0xff] : [4, (len >> 16) & 0xff, (len >> 8) & 0xff, len & 0xff];
  const crc = crc16(payload);
  return Uint8Array.from([...head, ...payload, (crc >> 8) & 0xff, crc & 0xff, 3]);
}

export class PacketDecoder {
  constructor(onPacket, maxLen = 8192) {
    this.onPacket = onPacket;
    this.maxLen = maxLen;
    this.buf = [];
  }

  reset() { this.buf = []; }

  add(bytes) {
    for (const b of bytes) this.buf.push(b);
    const buf = this.buf;
    while (buf.length) {
      const start = buf[0];
      if (start !== 2 && start !== 3 && start !== 4) { buf.shift(); continue; }
      const hdr = start;  // 2, 3 or 4 bytes of header
      if (buf.length < hdr) return;
      const len = start === 2 ? buf[1] : start === 3 ? (buf[1] << 8) | buf[2] : (buf[1] << 16) | (buf[2] << 8) | buf[3];
      if (len === 0 || len > this.maxLen) { buf.shift(); continue; }
      const total = hdr + len + 3;
      if (buf.length < total) return;
      const payload = buf.slice(hdr, hdr + len);
      const crc = (buf[hdr + len] << 8) | buf[hdr + len + 1];
      if (buf[hdr + len + 2] !== 3 || crc !== crc16(payload)) { buf.shift(); continue; }  // resync
      buf.splice(0, total);
      this.onPacket(Uint8Array.from(payload));
    }
  }
}

export function halfToFloat(h) {
  const s = (h >> 15) & 1, e = (h >> 10) & 0x1f, m = h & 0x3ff;
  let v;
  if (e === 0) v = m * Math.pow(2, -24);
  else if (e === 31) v = m === 0 ? Infinity : NaN;
  else v = (1 + m / 1024) * Math.pow(2, e - 15);
  return s ? -v : v;
}

export class Reader {
  constructor(bytes, pos = 0) {
    this.b = bytes;
    this.dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
    this.pos = pos;
  }
  get remaining() { return this.b.length - this.pos; }
  has(n) { return this.remaining >= n; }
  u8() { return this.dv.getUint8(this.pos++); }
  i8() { return this.dv.getInt8(this.pos++); }
  u16() { const v = this.dv.getUint16(this.pos); this.pos += 2; return v; }
  i16() { const v = this.dv.getInt16(this.pos); this.pos += 2; return v; }
  u32() { const v = this.dv.getUint32(this.pos); this.pos += 4; return v; }
  i32() { const v = this.dv.getInt32(this.pos); this.pos += 4; return v; }
  f16() { return halfToFloat(this.u16()); }
  d16(scale) { return this.i16() / scale; }
  d32(scale) { return this.i32() / scale; }
  lenString() {
    const n = this.u8();
    let s = '';
    for (let i = 0; i < n; i++) s += String.fromCharCode(this.b[this.pos + i]);
    this.pos += n;
    return s;
  }
}

export function cString(s) {
  return [...new TextEncoder().encode(s), 0];
}

// COMM_GET_VALUES_SETUP reply (after the command byte)
export function parseValuesSetup(r) {
  if (!r.has(65)) return null;
  const out = {};
  out.tempMos = r.d16(10);
  out.tempMotor = r.d16(10);
  out.currentMotor = r.d32(100);
  out.currentIn = r.d32(100);
  out.duty = r.d16(1000);
  out.rpm = r.d32(1);
  out.speedMs = r.d32(1000);
  out.vIn = r.d16(10);
  out.batteryLevel = r.d16(1000);
  r.d32(1e4); r.d32(1e4); r.d32(1e4); r.d32(1e4);  // Ah, Ah charged, Wh, Wh charged
  out.tach = r.d32(1000);
  out.tachAbsM = r.d32(1000);
  r.d32(1e6);  // position
  out.fault = r.i8(); r.u8(); r.u8();  // fault, id, number of VESCs
  r.d32(1000);  // battery Wh
  out.odometerM = r.has(4) ? r.u32() : NaN;
  return out;
}
