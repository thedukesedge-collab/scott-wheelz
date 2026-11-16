// A pretend board that answers the same messages a real VESC + Refloat does,
// so the whole app (protocol included) can be tried without hardware.
import 'dart:async';
import 'dart:math' as math;
import 'dart:typed_data';

import '../vesc/buffers.dart';
import '../vesc/link.dart';
import 'board.dart';

class DemoLink implements VescLink {
  DemoLink() {
    _timer = Timer.periodic(const Duration(milliseconds: 50), (_) => _step(0.05));
    Future.microtask(() => _connected.add(true));
  }

  final _packets = StreamController<Uint8List>.broadcast();
  final _connected = StreamController<bool>.broadcast();
  late final Timer _timer;
  final _rnd = math.Random();

  static const idle = ['motor.speed', 'motor.erpm', 'motor.current', 'motor.dir_current', 'motor.filt_current',
    'motor.duty_cycle', 'motor.batt_voltage', 'motor.batt_current', 'motor.mosfet_temp', 'motor.motor_temp',
    'imu.pitch', 'imu.balance_pitch', 'imu.roll', 'footpad.adc1', 'footpad.adc2', 'remote.input', 'imu.yaw'];
  static const runtime = ['setpoint', 'atr.setpoint', 'brake_tilt.setpoint', 'torque_tilt.setpoint',
    'turn_tilt.setpoint', 'remote.setpoint', 'balance_current', 'atr.accel_diff', 'atr.speed_boost', 'booster.current'];

  // sim state
  double t = 0, speed = 0, accel = 0, target = 0, nextTarget = 0, pitch = 14, roll = 0, amps = 0;
  double battPct = 0.9, motorT = 22, escT = 29, tach = 0, odo = 182650, remoteTilt = 0;
  bool riding = false, flywheel = false;
  double phase = 0;  // ride / rest cycle
  double moveCmd = 0, moveTimer = 0;

  @override
  String get name => 'Demo board';
  @override
  bool get isDemo => true;
  @override
  Stream<Uint8List> get packets => _packets.stream;
  @override
  Stream<bool> get connected => _connected.stream;

  void _step(double dt) {
    t += dt;
    phase += dt;
    // 40 s riding, 8 s resting tail-down
    final cycle = phase % 48;
    final wantRide = !flywheel && cycle < 40;
    if (wantRide != riding) riding = wantRide;
    if (riding) {
      nextTarget -= dt;
      if (nextTarget <= 0) {
        target = _rnd.nextDouble() < 0.2 ? _rnd.nextDouble() * 6 : 8 + _rnd.nextDouble() * 34;  // km/h
        nextTarget = 3 + _rnd.nextDouble() * 5;
      }
      if (cycle > 36) target = 0;
    } else {
      target = moveTimer > 0 ? moveCmd.abs() * 4 : 0;
    }
    moveTimer = math.max(0, moveTimer - dt);
    final prev = speed;
    speed += (target - speed).clamp(-12 * dt, 8 * dt);
    accel += ((speed - prev) / dt - accel) * 0.2;
    amps = riding ? 2 + speed * 0.3 + accel * 3 + (_rnd.nextDouble() - 0.5) : 0;
    final pTarget = riding ? accel * 1.0 + remoteTilt * 4 + (_rnd.nextDouble() - 0.5) * 0.4 : (flywheel ? 0.0 : 14.0);
    pitch += (pTarget - pitch) * 0.2;
    roll += ((riding ? math.sin(t / 1.1) * 7 : 0) - roll) * 0.2;
    tach += speed / 3.6 * dt;
    odo += speed / 3.6 * dt;
    battPct = math.max(0, battPct - amps * dt * 2e-6);
    if (riding) {
      motorT += (22 + amps * 1.1 - motorT) * dt * 0.02;
      escT += (29 + amps * 0.7 - escT) * dt * 0.02;
    }
  }

  double get volts => 66 + 18 * battPct;
  double get duty => (speed / 66 + amps * 0.002).clamp(0.0, 1.0);

  @override
  Future<void> send(List<int> payload) async {
    if (payload.isEmpty) return;
    final r = ByteReader(Uint8List.fromList(payload), 1);
    switch (payload[0]) {
      case Comm.getValuesSetup:
        _reply(_valuesSetup());
      case Comm.lispReplCmd:
        final cmd = r.cString();
        final m = RegExp(r'set-remote-state ([-0-9.]+)').firstMatch(cmd);
        if (m != null) remoteTilt = double.tryParse(m.group(1)!) ?? 0;
      case Comm.customAppData:
        if (r.u8() != Rf.pkgId) return;
        final cmd = r.u8();
        if (cmd == Rf.info) {
          final w = ByteWriter()..u8(Comm.customAppData)..u8(Rf.pkgId)..u8(Rf.info)..u8(2)..u8(0);
          w.bytes(List.filled(47, 0));
          w.u32(10000);
          w.u32(0);
          w.u8(0);
          _reply(w.toBytes());
        } else if (cmd == Rf.realtimeDataIds) {
          final w = ByteWriter()..u8(Comm.customAppData)..u8(Rf.pkgId)..u8(Rf.realtimeDataIds);
          w.u8(idle.length);
          for (final s in idle) {
            w.lenString(s);
          }
          w.u8(runtime.length);
          for (final s in runtime) {
            w.lenString(s);
          }
          _reply(w.toBytes());
        } else if (cmd == Rf.realtimeData) {
          _reply(_rtData());
        } else if (cmd == Rf.flywheel) {
          flywheel = r.u8() == 130;
        } else if (cmd == Rf.rcMove) {
          final dir = r.u8();
          moveCmd = dir == 1 ? 1 : -1;
          moveTimer = 0.2;
        }
    }
  }

  Uint8List _valuesSetup() {
    final w = ByteWriter()..u8(Comm.getValuesSetup);
    w.d16(escT, 10); w.d16(motorT, 10);
    w.d32(amps, 100); w.d32(amps * duty, 100);
    w.d16(duty, 1000); w.d32(speed * 1200, 1);
    w.d32(speed / 3.6, 1000);
    w.d16(volts, 10); w.d16(battPct, 1000);
    for (var i = 0; i < 4; i++) {
      w.d32(0, 1e4);
    }
    w.d32(tach, 1000); w.d32(tach, 1000); w.d32(0, 1e6);
    w.u8(0); w.u8(0); w.u8(1);
    w.d32(1400, 1000);
    w.u32(odo.round());
    w.u32((t * 1000).round());
    return w.toBytes();
  }

  Uint8List _rtData() {
    final w = ByteWriter()..u8(Comm.customAppData)..u8(Rf.pkgId)..u8(Rf.realtimeData);
    w.u8(riding ? 1 : 0);
    w.u8(0);
    w.u32((t * 10000).round());
    final state = riding ? Rf.sRunning : Rf.sReady;
    w.u8(((flywheel ? Rf.mFlywheel : Rf.mNormal) << 4) | state);
    w.u8(((riding ? Rf.fsBoth : Rf.fsNone) << 6));
    w.u8(0);
    w.u8(0);
    final vals = <String, double>{
      'motor.speed': speed, 'motor.erpm': speed * 1200, 'motor.current': amps, 'motor.dir_current': amps,
      'motor.filt_current': amps, 'motor.duty_cycle': duty, 'motor.batt_voltage': volts,
      'motor.batt_current': amps * duty, 'motor.mosfet_temp': escT, 'motor.motor_temp': motorT,
      'imu.pitch': pitch, 'imu.balance_pitch': pitch * (riding ? 0.85 : 1), 'imu.roll': roll,
      'footpad.adc1': riding ? 2.9 : 0, 'footpad.adc2': riding ? 3.0 : 0, 'remote.input': remoteTilt, 'imu.yaw': 0,
      'setpoint': remoteTilt * 4, 'atr.setpoint': (accel * 0.3).clamp(-2.5, 2.5), 'brake_tilt.setpoint': 0,
      'torque_tilt.setpoint': (amps * 0.06).clamp(-3, 3), 'turn_tilt.setpoint': 0, 'remote.setpoint': remoteTilt * 4,
      'balance_current': amps, 'atr.accel_diff': 0, 'atr.speed_boost': 0, 'booster.current': 0,
    };
    for (final id in idle) {
      w.f16(vals[id] ?? 0);
    }
    if (riding) {
      for (final id in runtime) {
        w.f16(vals[id] ?? 0);
      }
    }
    return w.toBytes();
  }

  void _reply(Uint8List p) {
    Future.delayed(const Duration(milliseconds: 15), () {
      if (!_packets.isClosed) _packets.add(p);
    });
  }

  @override
  Future<void> close() async {
    _timer.cancel();
    await _packets.close();
    await _connected.close();
  }
}
