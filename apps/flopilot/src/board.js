// The board link: polls Refloat over the VESC link (10 Hz, like the Flutter
// app and the package UI) and turns its realtime data into the simulator's S
// fields. Platform-free: the transport is passed in (send(bytes)).
import { Comm, Rf, Reader, encodePacket, PacketDecoder, parseValuesSetup, cString } from './vesc.js';

const MPS_TO_MPH = 2.23694;
const HW_VESC = 0, HW_CUSTOM_MODULE = 2;  // VESC Tool HW_TYPE

// COMM_FW_VERSION reply: major, minor, hw name, uuid (12), paired, test fw, hw type
export function parseFw(p) {
  let i = 3, hw = '';
  while (i < p.length && p[i]) hw += String.fromCharCode(p[i++]);
  i += 1 + 12 + 2;
  return { major: p[1], minor: p[2], hw, hwType: i < p.length ? p[i] : HW_VESC };
}

export class Board {
  constructor(transport) {
    this.t = transport;  // { send(Uint8Array) }
    this.decoder = new PacketDecoder(p => this.onPacket(p));
    this.comms = 'info';
    this.idleIds = [];
    this.runtimeIds = [];
    this.rt = {};
    this.vals = null;
    this.pkgState = -1;
    this.pkgMode = 0;
    this.fpState = 0;
    this.wheelslip = false;
    this.lastRt = 0;
    this.started = Date.now();
    this.packageMissing = false;
    this.hornWas = false;
    this.tiltSent = false;
    this.whmi = 0;
    // which device on the Bluetooth link runs Refloat: a VESC Express (Thor and
    // others) is its own device on the CAN bus, so commands are forwarded to
    // the motor controller (COMM_FORWARD_CAN), as VESC Tool does
    this.link = 'fw';      // fw -> (can -> probe) -> ok | nocan
    this.fwd = null;       // CAN id of the motor controller, null = direct
    this.linkAt = 0;
    this.canIds = [];
    this.probe = 0;
  }

  get live() { return this.comms === 'ready' && Date.now() - this.lastRt < 3000; }

  // lights (Refloat's own LIGHTS_CONTROL): mask 1 = all lights, 2 = headlights; mask 0 = just ask
  lightsSet(mask, value) { this.app([Rf.lights, 0, 0, 0, mask & 0xff, value & 0xff]); }
  // Refloat's own config (VESC custom config 0): read / write, see ledsync.js
  getConfig() { this.send([Comm.getCustomConfig, 0]); }
  setConfig(raw) { this.send([Comm.setCustomConfig, 0, ...raw]); }
  app(bytes) { this.send([Comm.customAppData, Rf.pkgId, ...bytes]); }
  send(payload) { this.raw(this.fwd === null ? payload : [Comm.forwardCan, this.fwd, ...payload]); }
  raw(payload) { this.t.send(encodePacket(payload)); }

  // find the motor controller: firmware version first; a VESC Express (hw type
  // "custom module") -> ping the CAN bus -> ask each id for its firmware -> the
  // first motor controller (hw type VESC) gets everything from now on
  discover() {
    const now = Date.now();
    if (this.link === 'fw' && now - this.linkAt > 500) { this.linkAt = now; this.raw([Comm.fwVersion]); }
    else if ((this.link === 'can' || this.link === 'nocan') && now - this.linkAt > 2500) { this.linkAt = now; this.raw([Comm.pingCan]); }
    else if (this.link === 'probe' && now - this.linkAt > 500) {
      if (this.probe >= this.canIds.length) { this.link = 'nocan'; this.linkAt = now; return; }
      this.linkAt = now;
      this.raw([Comm.forwardCan, this.canIds[this.probe++], Comm.fwVersion]);
    }
  }

  onFw(fw) {
    if (this.link === 'fw') {
      if (fw.hwType === HW_CUSTOM_MODULE) { this.link = 'can'; this.linkAt = 0; this.express = fw; return; }
      this.useController(null, fw);
    } else if (this.link === 'probe' && fw.hwType === HW_VESC) {
      this.useController(this.canIds[this.probe - 1], fw);
    }
  }

  useController(id, fw) {
    this.fwd = id;
    this.fw = { major: fw.major, minor: fw.minor, hw: fw.hw };
    this.link = 'ok';
    this.started = Date.now();  // the "Refloat not found" wait starts now
  }

  receive(bytes) { this.decoder.add(bytes); }

  // every 100 ms; S = the simulator's state (tilt slider, horn)
  tick(S) {
    if (this.link !== 'ok') { this.discover(); return; }
    this.send([Comm.getValuesSetup]);
    if (this.comms === 'info') this.app([Rf.info, 2]);
    else if (this.comms === 'ids') this.app([Rf.realtimeDataIds]);
    else this.app([Rf.realtimeData]);
    if (this.comms === 'ready' && (this.n = (this.n || 0) + 1) % 20 === 1) this.lightsSet(0, 0);  // light state every 2 s
    if (this.comms === 'ready' && this.n % 50 === 2) this.app([Rf.lcmDeviceInfo]);  // light module name every 5 s
    if (this.comms !== 'ready' && Date.now() - this.started > 5000) this.packageMissing = true;

    // MOVE slider (CONTROL tab): Refloat's own RC move (stock 1.2.3 too, only
    // with the board idle), resent every 100 ms while held, as the package
    // does (ui.qml.in sendRcMove): direction, current 10-80 A, time, checksum
    const mv = S ? S.move || 0 : 0;
    if (mv !== 0 && this.pkgState !== Rf.sRunning) {
      const cur = Math.abs(Math.round(mv * 70)) + 10;
      this.app([Rf.rcMove, mv > 0 ? 1 : 0, cur, 1, cur + 1]);
    }
    // horn, melodies and the tilt slider: sent over the Lisp REPL by sound.js
  }

  onPacket(p) {
    if (!p.length) return;
    const r = new Reader(p, 1);
    try {
      if (p[0] === Comm.getValuesSetup) {
        const v = parseValuesSetup(r);
        if (v) {
          this.vals = v;
          const mph = Math.abs(v.speedMs * MPS_TO_MPH);
          const wh = Math.max(-60, Math.min(60, v.currentIn * v.vIn / Math.max(mph, 1e-6)));
          this.whmi += (wh - this.whmi) * 0.1;
        }
      } else if (p[0] === Comm.fwVersion && p.length >= 3) {
        this.onFw(parseFw(p));
      } else if (p[0] === Comm.pingCan) {
        if (this.link === 'can' || this.link === 'nocan') {
          this.canIds = [...p.slice(1)];
          this.probe = 0;
          this.link = this.canIds.length ? 'probe' : 'nocan';
          this.linkAt = 0;
        }
      } else if (p[0] === Comm.qmlErase || p[0] === Comm.qmlWrite || p[0] === Comm.lispErase || p[0] === Comm.lispWrite) {
        if (this.onInstallReply) this.onInstallReply(p);
      } else if (p[0] === Comm.lispPrint) {
        if (this.onLispPrint) this.onLispPrint(String.fromCharCode(...p.slice(1)));
      } else if (p[0] === Comm.lispStats && p.length >= 7) {
        const f16 = i => ((p[i] << 8 | p[i + 1]) << 16 >> 16) / 100;
        if (this.onLispStats) this.onLispStats({ cpu: f16(1), heap: f16(3), mem: f16(5) });
      } else if (p[0] === Comm.getMcconf) {
        if (this.onMcconf) this.onMcconf(p.slice(1));  // motor config: signature + fields (mcconf.js)
      } else if (p[0] === Comm.setMcconf) {
        if (this.onMcconfAck) this.onMcconfAck();
      } else if (p[0] === Comm.getCustomConfig) {
        if (p[1] === 0 && this.onConfig) this.onConfig(p.slice(2));
      } else if (p[0] === Comm.setCustomConfig) {
        if (this.onConfigAck) this.onConfigAck();
      } else if (p[0] === Comm.customAppData && p[1] === Rf.pkgId && p[2] === 128) {
        if (this.onLcmFw) this.onLcmFw(p.slice(2));  // RF Glow COMMAND_LCM_FW status (lcmfw.js)
      } else if (p[0] === Comm.customAppData) {
        this.onAppData(r);
      }
    } catch (e) {
      console.warn('packet parse error', e);
    }
  }

  onAppData(r) {
    if (r.u8() !== Rf.pkgId) return;
    const msg = r.u8();
    if (msg === Rf.info) {
      this.comms = 'ids';
      this.packageMissing = false;
      // COMMAND_INFO v2: name (20), major, minor, patch, suffix (20)
      try {
        if (r.u8() === 2) {
          r.u8();  // flags
          const str = n => { let t = ''; for (let i = 0; i < n; i++) { const c = r.u8(); if (c) t += String.fromCharCode(c); } return t.replace(/\0.*$/, ''); };
          const name = str(20);
          const ver = r.u8() + '.' + r.u8() + '.' + r.u8() + str(20);
          this.pkg = { name, version: ver };
        }
      } catch (e) {}
    } else if (msg === Rf.realtimeDataIds) {
      if (this.comms !== 'ids') return;
      const n1 = r.u8(); this.idleIds = Array.from({ length: n1 }, () => r.lenString());
      const n2 = r.u8(); this.runtimeIds = Array.from({ length: n2 }, () => r.lenString());
      this.comms = 'ready';
    } else if (msg === Rf.lcmDeviceInfo) {
      // the light module's own name, as it gives it in its polls ('' = none / not polling)
      let n = '';
      try { for (;;) { const c = r.u8(); if (!c) break; n += String.fromCharCode(c); } } catch (e) {}
      this.lcmName = n;
    } else if (msg === 129) {
      // RF Glow COMMAND_LCM_BATTERY reply: 0, enabled, resistance, 11 points, cells, volts (u16 0.1 V), %
      try {
        if (r.u8() === 0) {
          const on = r.u8() !== 0, res = r.u8();
          for (let i = 0; i < 11; i++) r.u8();
          this.battery = { on, res, cells: r.u8() };
        }
      } catch (e) {}
    } else if (msg === Rf.lights) {
      this.lights = r.u8();  // bit 0 = lights on, bit 1 = headlights on
    } else if (msg === Rf.realtimeData) {
      if (this.comms !== 'ready') return;
      this.lastRt = Date.now();
      const mask = r.u8();
      r.u8();  // extra flags
      r.u32();  // time
      const ms = r.u8();
      this.pkgMode = ms >> 4;
      this.pkgState = ms & 0xf;
      const sf = r.u8();
      this.fpState = sf >> 6;
      this.wheelslip = !!(sf & 1);
      r.u8();  // sat / stop condition
      r.u8();  // beep reason
      const rt = {};
      for (const id of this.idleIds) rt[id] = r.f16();
      if (mask & 1) for (const id of this.runtimeIds) rt[id] = r.f16();
      this.rt = rt;
    }
  }

  // realtime values -> the simulator's state
  apply(S) {
    const rt = this.rt, v = this.vals || {};
    const num = (x, d = 0) => (Number.isFinite(x) ? x : d);
    S.mode = this.pkgMode === Rf.mFlywheel ? 'flywheel' : this.pkgState === Rf.sRunning ? 'running' : 'ready';
    S.speed = Math.abs(num(rt['motor.speed'], num(v.speedMs))) * MPS_TO_MPH;
    S.erpm = num(rt['motor.erpm'], num(v.rpm));
    S.duty = Math.abs(num(rt['motor.duty_cycle'], num(v.duty)));
    S.motorA = num(rt['motor.current'], num(v.currentMotor));
    S.battA = num(rt['motor.batt_current'], num(v.currentIn));
    S.volts = num(rt['motor.batt_voltage'], num(v.vIn, S.volts));
    S.pitch = num(rt['imu.pitch']);
    S.balance = num(rt['imu.balance_pitch'], S.pitch);
    S.roll = num(rt['imu.roll']);
    S.setpoint = num(rt['setpoint']);
    S.fpL = !!(this.fpState & 1);
    S.fpR = !!(this.fpState & 2);
    S.wheelslip = this.wheelslip;
    S.motorT = num(rt['motor.motor_temp'], num(v.tempMotor, S.motorT));
    S.escT = num(rt['motor.mosfet_temp'], num(v.tempMos, S.escT));
    if (Number.isFinite(v.batteryLevel)) S.battPct = Math.max(0, Math.min(100, v.batteryLevel * 100));
    if (Number.isFinite(v.tachAbsM)) S.trip = v.tachAbsM / 1609.34;  // since power-on, like the package
    if (Number.isFinite(v.odometerM)) S.life = v.odometerM / 1609.34;
    S.whmi = this.whmi;
    S.auto = false;
    S.scen = null;
  }
}
