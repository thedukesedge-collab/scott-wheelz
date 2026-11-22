// Live board state + the Refloat package protocol (same messages the package
// AppUI uses: package id 101 inside COMM_CUSTOM_APP_DATA).
import 'dart:async';
import 'dart:math' as math;
import 'dart:typed_data';

import 'package:flutter/foundation.dart';

import '../prefs.dart';
import '../vesc/buffers.dart';
import '../vesc/link.dart';

class Rf {
  static const pkgId = 101;
  static const info = 0;
  static const rcMove = 7;
  static const flywheel = 22;
  static const realtimeData = 31;
  static const realtimeDataIds = 32;
  static const horn = 120;  // Refloat Glow haptic horn

  // package states / modes / footpad
  static const sDisabled = 0, sStartup = 1, sReady = 2, sRunning = 3, sConnecting = 254, sDisconnected = 255;
  static const mNormal = 0, mHandtest = 1, mFlywheel = 2;
  static const fsNone = 0, fsLeft = 1, fsRight = 2, fsBoth = 3;
}

enum Comms { info, ids, ready }

/// One series sample ring for the DATA graph.
class SeriesBuffer {
  SeriesBuffer(this.capacity);
  final int capacity;
  final Map<String, Float32List> data = {};
  int head = 0, count = 0;

  void add(Map<String, double> sample) {
    for (final e in sample.entries) {
      (data[e.key] ??= Float32List(capacity))[head] = e.value.isFinite ? e.value : 0;
    }
    head = (head + 1) % capacity;
    count = math.min(capacity, count + 1);
  }

  double at(String id, int k) {
    final d = data[id];
    if (d == null) return 0;
    return d[(head - count + k + capacity) % capacity];
  }
}

class Board extends ChangeNotifier {
  Board(this.prefs) {
    prefs.addListener(notifyListeners);
  }

  final Prefs prefs;
  VescLink? link;
  StreamSubscription<Uint8List>? _pktSub;
  StreamSubscription<bool>? _connSub;
  Timer? _poll;
  Timer? _hornMax;

  // connection / package
  bool linkUp = false;
  Comms comms = Comms.info;
  DateTime _lastRt = DateTime.fromMillisecondsSinceEpoch(0);
  bool packageMissing = false;
  int ticksPerSecond = 10000;

  // realtime
  int pkgState = Rf.sDisconnected;
  int pkgMode = Rf.mNormal;
  int fpState = Rf.fsNone;
  bool wheelslip = false, darkride = false, charging = false;
  int sat = 0, stopCondition = 0, beepReason = 0;
  List<String> idleIds = [], runtimeIds = [];
  final Map<String, double> rt = {};

  // values setup
  double batteryLevel = double.nan, vIn = double.nan, odometerM = double.nan, tachAbsM = double.nan;
  double whPerDist = 0;

  // controls
  double tilt = 0;          // -1..1 from the TILT slider
  bool tiltHeld = false;
  bool _tiltSentNonZero = false;
  double tiltAngleLimit = 10;
  double move = 0;          // -1..1 vertical move slider
  bool hornPressed = false, hornExpired = false, hornBlocked = false;
  double hornBudget = 3.0;
  static const hornBudgetMax = 3.0;

  // peaks filter
  int _peakOkSince = 0;

  final SeriesBuffer plot = SeriesBuffer(600);  // 60 s at 10 Hz

  bool get connected => link != null && linkUp;
  bool get running => pkgState == Rf.sRunning;
  bool get hornAvailable => pkgState == Rf.sRunning || pkgState == Rf.sReady;
  bool get honking => hornPressed && !hornExpired && !hornBlocked && hornBudget > 0;
  double get imperial => prefs.metric ? 1.0 : 0.621371;
  String get speedUnit => prefs.metric ? 'km/h' : 'mph';
  String get distUnit => prefs.metric ? 'km' : 'mi';

  double v(String id) => rt[id] ?? double.nan;

  Future<void> attach(VescLink l) async {
    await detach();
    link = l;
    linkUp = true;
    comms = Comms.info;
    packageMissing = false;
    pkgState = Rf.sConnecting;
    _pktSub = l.packets.listen(_onPacket);
    _connSub = l.connected.listen((up) {
      linkUp = up;
      if (!up) pkgState = Rf.sDisconnected;
      notifyListeners();
    });
    final started = DateTime.now();
    _poll = Timer.periodic(const Duration(milliseconds: 100), (_) {
      _tick();
      if (comms != Comms.ready && DateTime.now().difference(started).inSeconds > 5) {
        packageMissing = true;
      }
      if (comms == Comms.ready && DateTime.now().difference(_lastRt).inSeconds > 3) {
        pkgState = Rf.sDisconnected;
      }
    });
    notifyListeners();
  }

  Future<void> detach() async {
    _poll?.cancel();
    _hornMax?.cancel();
    await _pktSub?.cancel();
    await _connSub?.cancel();
    final l = link;
    link = null;
    linkUp = false;
    pkgState = Rf.sDisconnected;
    rt.clear();
    if (l != null) await l.close();
    notifyListeners();
  }

  // ---------------- outgoing ----------------

  void _app(List<int> bytes) => link?.send([Comm.customAppData, Rf.pkgId, ...bytes]);

  void _tick() {
    final l = link;
    if (l == null) return;
    l.send([Comm.getValuesSetup]);
    switch (comms) {
      case Comms.info:
        _app([Rf.info, 2]);  // INFO v2
      case Comms.ids:
        _app([Rf.realtimeDataIds]);
      case Comms.ready:
        _app([Rf.realtimeData]);
    }

    if (move != 0 && !running) {
      final current = (move.abs() * 70).round() + 10;
      _app([Rf.rcMove, move > 0 ? 1 : 0, current, 1, current + 1]);
    }

    // horn heat budget (same rules as the package UI)
    if (honking) {
      hornBudget = math.max(0, hornBudget - (running ? 0.1 : 0.15));
      if (hornBudget <= 0) {
        hornExpired = true;
        _sendHorn(0);
      } else {
        _sendHorn(1);
      }
    } else {
      hornBudget = math.min(hornBudgetMax, hornBudget + 0.1);
    }

    // TILT: remote tilt via LispBM, one final 0 after it springs back
    final tiltActive = tiltHeld || tilt != 0;
    if (tiltActive || _tiltSentNonZero) {
      final deg = tilt * math.min(4.0, tiltAngleLimit);
      final input = tiltAngleLimit > 0 ? (deg / tiltAngleLimit).clamp(-1.0, 1.0) : 0.0;
      final w = ByteWriter()..u8(Comm.lispReplCmd)..cString('(set-remote-state ${input.toStringAsFixed(3)} 0 0 0 0)');
      l.send(w.toBytes());
      _tiltSentNonZero = tiltActive;
    }
  }

  void _sendHorn(int req) => _app([Rf.horn, req, 3]);

  void hornDown() {
    if (!hornAvailable) return;
    hornPressed = true;
    hornExpired = false;
    hornBlocked = hornBudget < 0.2;
    if (!hornBlocked) {
      _sendHorn(2);
      _hornMax?.cancel();
      _hornMax = Timer(const Duration(seconds: 3), () {
        hornExpired = true;
        _sendHorn(0);
        notifyListeners();
      });
    }
    notifyListeners();
  }

  void hornUp() {
    hornPressed = false;
    hornExpired = false;
    hornBlocked = false;
    _hornMax?.cancel();
    _sendHorn(0);
    notifyListeners();
  }

  void setTilt(double v) {
    tilt = v;
    notifyListeners();
  }

  void setTiltHeld(bool held) {
    tiltHeld = held;
    notifyListeners();
  }

  void setMove(double v) {
    move = v;
    notifyListeners();
  }

  void setFlywheel(bool on) {
    if (on) {
      _app([Rf.flywheel, 130, 0, 0, 0, 0, 1, 0]);
    } else {
      _app([Rf.flywheel, 128]);
    }
  }

  void resetPeaks() {
    prefs.peakSpeed = 0;
    prefs.peakDuty = 0;
    notifyListeners();
  }

  // ---------------- incoming ----------------

  void _onPacket(Uint8List p) {
    if (p.isEmpty) return;
    final r = ByteReader(p, 1);
    try {
      switch (p[0]) {
        case Comm.getValuesSetup:
          _onValuesSetup(r);
        case Comm.customAppData:
          _onAppData(r);
      }
    } catch (e) {
      debugPrint('packet parse error: $e');
    }
  }

  void _onValuesSetup(ByteReader r) {
    if (!r.has(65)) return;
    r.d16(10); r.d16(10);                 // temp mos, temp motor
    r.d32(100);                           // current motor
    final currentIn = r.d32(100);
    r.d16(1000);                          // duty
    r.d32(1);                             // rpm
    final speedMs = r.d32(1000);
    vIn = r.d16(10);
    batteryLevel = r.d16(1000);
    r.d32(1e4); r.d32(1e4); r.d32(1e4); r.d32(1e4);  // Ah, Ah chg, Wh, Wh chg
    r.d32(1000);                          // tachometer
    tachAbsM = r.d32(1000);
    r.d32(1e6);                           // position
    r.i8(); r.u8(); r.u8();               // fault, id, num vescs
    r.d32(1000);                          // battery wh
    odometerM = r.u32().toDouble();

    // peaks reset when the board's trip counter goes backwards (power cycle)
    if (tachAbsM < prefs.peakTach) {
      prefs.peakSpeed = 0;
      prefs.peakDuty = 0;
    }
    if (tachAbsM != prefs.peakTach) prefs.peakTach = tachAbsM;

    final speed = (speedMs * 3.6 * imperial).abs();
    final wh = (currentIn * vIn / math.max(speed, 1e-6)).clamp(-60.0, 60.0);
    whPerDist += (wh - whPerDist) * 0.1;
    notifyListeners();
  }

  void _onAppData(ByteReader r) {
    if (r.u8() != Rf.pkgId) return;
    final msg = r.u8();
    if (msg == Rf.info) {
      r.u8();               // version
      r.u8();               // flags
      r.pos += 47;          // name, version, suffix, git hash
      if (r.has(4)) ticksPerSecond = r.u32();
      comms = Comms.ids;
      packageMissing = false;
    } else if (msg == Rf.realtimeDataIds) {
      if (comms != Comms.ids) return;
      idleIds = List.generate(r.u8(), (_) => r.lenString());
      runtimeIds = List.generate(r.u8(), (_) => r.lenString());
      comms = Comms.ready;
    } else if (msg == Rf.realtimeData) {
      if (comms != Comms.ready) return;
      _lastRt = DateTime.now();
      final mask = r.u8();
      final hasRuntime = mask & 1 != 0;
      r.u8();                // extra flags
      r.u32();               // time
      final ms = r.u8();
      pkgMode = ms >> 4;
      pkgState = ms & 0xF;
      final sf = r.u8();
      fpState = sf >> 6;
      wheelslip = sf & 1 != 0;
      darkride = sf & 2 != 0;
      charging = sf & 0x20 != 0;
      final ss = r.u8();
      sat = ss >> 4;
      stopCondition = ss & 0xF;
      beepReason = r.u8();
      rt.clear();
      for (final id in idleIds) {
        rt[id] = r.f16();
      }
      if (hasRuntime) {
        for (final id in runtimeIds) {
          rt[id] = r.f16();
        }
      }
      _updatePeaks();
      plot.add({
        ...rt,
        'state.running': running ? 1 : 0,
        'state.wheelslip': wheelslip ? 1 : 0,
        'footpad.state': fpState.toDouble(),
      });
      notifyListeners();
    }
  }

  // Ride peaks with free spin filtered out: only when, for the last 0.5 s, the
  // board is running, no wheelslip / darkride, a footpad is pressed and the
  // motor is under real load (>= 4 A).
  void _updatePeaks() {
    final speed = v('motor.speed').abs();
    final duty = v('motor.duty_cycle').abs();
    final amps = v('motor.current').abs();
    final ok = running && !wheelslip && !darkride && fpState != Rf.fsNone && (amps.isNaN || amps >= 4);
    final now = DateTime.now().millisecondsSinceEpoch;
    if (!ok) {
      _peakOkSince = 0;
    } else if (_peakOkSince == 0) {
      _peakOkSince = now;
    } else if (now - _peakOkSince >= 500) {
      if (speed.isFinite && speed > prefs.peakSpeed) prefs.peakSpeed = speed;
      if (duty.isFinite && duty > prefs.peakDuty) prefs.peakDuty = math.min(1, duty);
    }
  }

  @override
  void dispose() {
    prefs.removeListener(notifyListeners);
    detach();
    super.dispose();
  }
}
