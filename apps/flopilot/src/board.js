// The board link: polls Refloat over the VESC link (10 Hz, like the Flutter
// app and the package UI) and turns its realtime data into the simulator's S
// fields. Platform-free: the transport is passed in (send(bytes)).
import { Comm, Rf, Reader, encodePacket, PacketDecoder, parseValuesSetup, cString } from './vesc.js';

const MPS_TO_MPH = 2.23694;

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
  }

  get live() { return this.comms === 'ready' && Date.now() - this.lastRt < 3000; }

  // lights (Refloat's own LIGHTS_CONTROL): mask 1 = all lights, 2 = headlights; mask 0 = just ask
  lightsSet(mask, value) { this.app([Rf.lights, 0, 0, 0, mask & 0xff, value & 0xff]); }
  // Refloat's own config (VESC custom config 0): read / write, see ledsync.js
  getConfig() { this.send([Comm.getCustomConfig, 0]); }
  setConfig(raw) { this.send([Comm.setCustomConfig, 0, ...raw]); }
  app(bytes) { this.send([Comm.customAppData, Rf.pkgId, ...bytes]); }
  send(payload) { this.t.send(encodePacket(payload)); }
  receive(bytes) { this.decoder.add(bytes); }

  // every 100 ms; S = the simulator's state (tilt slider, horn)
  tick(S) {
    this.send([Comm.getValuesSetup]);
    if (this.comms === 'info') this.app([Rf.info, 2]);
    else if (this.comms === 'ids') this.app([Rf.realtimeDataIds]);
    else this.app([Rf.realtimeData]);
    if (this.comms === 'ready' && (this.n = (this.n || 0) + 1) % 20 === 1) this.lightsSet(0, 0);  // light state every 2 s
    if (this.comms !== 'ready' && Date.now() - this.started > 5000) this.packageMissing = true;

    if (!S) return;
    // horn: played through the VESC firmware's motor tones, see sound.js

    // tilt slider: Refloat's remote input via Lisp (the package only uses it
    // while engaged and moving); 4 deg at full slider, with a 10 deg limit
    const tilt = S.tilt || 0;
    if (tilt !== 0 || this.tiltSent) {
      const input = Math.max(-1, Math.min(1, tilt * 0.4));
      this.send([Comm.lispReplCmd, ...cString('(set-remote-state ' + input.toFixed(3) + ' 0 0 0 0)')]);
      this.tiltSent = tilt !== 0;
    }
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
      } else if (p[0] === Comm.getCustomConfig) {
        if (p[1] === 0 && this.onConfig) this.onConfig(p.slice(2));
      } else if (p[0] === Comm.setCustomConfig) {
        if (this.onConfigAck) this.onConfigAck();
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
    } else if (msg === Rf.realtimeDataIds) {
      if (this.comms !== 'ids') return;
      const n1 = r.u8(); this.idleIds = Array.from({ length: n1 }, () => r.lenString());
      const n2 = r.u8(); this.runtimeIds = Array.from({ length: n2 }, () => r.lenString());
      this.comms = 'ready';
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
