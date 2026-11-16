import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:rinkmixed/prefs.dart';
import 'package:rinkmixed/refloat/board.dart';
import 'package:rinkmixed/refloat/demo_link.dart';
import 'package:rinkmixed/vesc/buffers.dart';
import 'package:rinkmixed/vesc/packet.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  test('crc16 matches CRC-16/XMODEM', () {
    expect(crc16(ascii.encode('123456789')), 0x31C3);
  });

  test('packets round-trip, also when split into BLE-sized pieces', () {
    final got = <Uint8List>[];
    final dec = PacketDecoder(got.add);
    final small = [36, 101, 31, 1, 2, 3];
    final big = List<int>.generate(700, (i) => i & 0xFF);
    final stream = [0x55, ...encodePacket(small), ...encodePacket(big)];  // junk byte first
    for (var i = 0; i < stream.length; i += 20) {
      dec.add(stream.sublist(i, i + 20 > stream.length ? stream.length : i + 20));
    }
    expect(got.length, 2);
    expect(got[0], small);
    expect(got[1], big);
  });

  test('corrupted packet is dropped', () {
    final got = <Uint8List>[];
    final f = encodePacket([1, 2, 3]);
    f[3] ^= 0xFF;
    PacketDecoder(got.add).add(f);
    expect(got, isEmpty);
  });

  test('float16 round-trip', () {
    for (final v in [0.0, 1.0, -2.5, 14.0, 81.7, 0.333, -60.0, 1200.0]) {
      expect(halfToDouble(doubleToHalf(v)), closeTo(v, v.abs() * 0.002 + 1e-3));
    }
  });

  test('full Refloat handshake + realtime data against the demo board', () async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await Prefs.load();
    final board = Board(prefs);
    await board.attach(DemoLink());
    await Future<void>.delayed(const Duration(milliseconds: 1500));
    expect(board.comms, Comms.ready);
    expect(board.rt.containsKey('motor.speed'), isTrue);
    expect(board.rt['imu.pitch']!.isFinite, isTrue);
    expect(board.batteryLevel, closeTo(0.9, 0.05));
    expect(board.pkgState == Rf.sRunning || board.pkgState == Rf.sReady, isTrue);
    await board.detach();
  });
}
