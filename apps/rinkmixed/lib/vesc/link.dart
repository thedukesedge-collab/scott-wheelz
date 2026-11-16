// A connection to a VESC: sends command payloads, delivers response payloads.
import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'packet.dart';

/// VESC command ids (bldc datatypes.h, COMM_PACKET_ID).
class Comm {
  static const fwVersion = 0;
  static const customAppData = 36;
  static const getValuesSetup = 47;
  static const getCustomConfigXml = 92;
  static const getCustomConfig = 93;
  static const getCustomConfigDefault = 94;
  static const setCustomConfig = 95;
  static const lispReplCmd = 138;
}

abstract class VescLink {
  String get name;
  bool get isDemo;
  Stream<Uint8List> get packets;
  Stream<bool> get connected;
  Future<void> send(List<int> payload);
  Future<void> close();
}

/// VESC Express / NRF BLE: Nordic UART service.
class BleLink implements VescLink {
  BleLink._(this.device, this._rx, this._tx, this._chunk);

  static final serviceUuid = Guid('6e400001-b5a3-f393-e0a9-e50e24dcca9e');
  static final rxUuid = Guid('6e400002-b5a3-f393-e0a9-e50e24dcca9e');  // phone -> VESC
  static final txUuid = Guid('6e400003-b5a3-f393-e0a9-e50e24dcca9e');  // VESC -> phone

  final BluetoothDevice device;
  final BluetoothCharacteristic _rx;
  final BluetoothCharacteristic _tx;
  final int _chunk;
  final _packets = StreamController<Uint8List>.broadcast();
  final _connected = StreamController<bool>.broadcast();
  late final PacketDecoder _decoder = PacketDecoder(_packets.add);
  StreamSubscription<List<int>>? _notifySub;
  StreamSubscription<BluetoothConnectionState>? _stateSub;
  Future<void> _writeChain = Future.value();
  bool _closed = false;

  static Future<BleLink> connect(BluetoothDevice device) async {
    await device.connect(timeout: const Duration(seconds: 12), autoConnect: false);
    try {
      await device.requestMtu(512);
    } catch (_) {
      // iOS negotiates by itself
    }
    final services = await device.discoverServices();
    BluetoothCharacteristic? rx, tx;
    for (final s in services) {
      if (s.uuid != serviceUuid) continue;
      for (final c in s.characteristics) {
        if (c.uuid == rxUuid) rx = c;
        if (c.uuid == txUuid) tx = c;
      }
    }
    if (rx == null || tx == null) {
      await device.disconnect();
      throw StateError('This device has no VESC BLE (UART) service');
    }
    final mtu = device.mtuNow;
    final chunk = (mtu - 3).clamp(20, 244);
    final link = BleLink._(device, rx, tx, chunk);
    await link._start();
    return link;
  }

  Future<void> _start() async {
    _notifySub = _tx.onValueReceived.listen(_decoder.add);
    device.cancelWhenDisconnected(_notifySub!);
    await _tx.setNotifyValue(true);
    _stateSub = device.connectionState.listen((s) {
      final up = s == BluetoothConnectionState.connected;
      _connected.add(up);
      if (!up) _decoder.reset();
    });
  }

  @override
  String get name => device.platformName.isNotEmpty ? device.platformName : device.remoteId.str;

  @override
  bool get isDemo => false;

  @override
  Stream<Uint8List> get packets => _packets.stream;

  @override
  Stream<bool> get connected => _connected.stream;

  @override
  Future<void> send(List<int> payload) {
    if (_closed) return Future.value();
    final frame = encodePacket(payload);
    final noResp = _rx.properties.writeWithoutResponse;
    _writeChain = _writeChain.then((_) async {
      for (var i = 0; i < frame.length; i += _chunk) {
        final end = (i + _chunk < frame.length) ? i + _chunk : frame.length;
        try {
          await _rx.write(frame.sublist(i, end), withoutResponse: noResp);
        } catch (_) {
          return;  // dropped link: the connection stream reports it
        }
      }
    });
    return _writeChain;
  }

  @override
  Future<void> close() async {
    _closed = true;
    await _notifySub?.cancel();
    await _stateSub?.cancel();
    try {
      await device.disconnect();
    } catch (_) {}
    await _packets.close();
    await _connected.close();
  }
}
