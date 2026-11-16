// Find and connect to a board (or start the demo board).
import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../prefs.dart';
import '../refloat/demo_link.dart';
import '../theme.dart';
import '../vesc/link.dart';

class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key, required this.prefs, required this.onLink});
  final Prefs prefs;
  final ValueChanged<VescLink> onLink;

  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  List<ScanResult> results = [];
  StreamSubscription<List<ScanResult>>? _sub;
  StreamSubscription<BluetoothAdapterState>? _adapterSub;
  BluetoothAdapterState adapter = BluetoothAdapterState.unknown;
  bool scanning = false;
  String? connectingId;
  String? error;

  @override
  void initState() {
    super.initState();
    _sub = FlutterBluePlus.scanResults.listen((r) {
      final named = r.where((x) => x.advertisementData.advName.isNotEmpty || x.device.platformName.isNotEmpty).toList();
      // boards advertising the VESC UART service first, then by signal
      named.sort((a, b) {
        final av = a.advertisementData.serviceUuids.contains(BleLink.serviceUuid) ? 0 : 1;
        final bv = b.advertisementData.serviceUuids.contains(BleLink.serviceUuid) ? 0 : 1;
        return av != bv ? av - bv : b.rssi - a.rssi;
      });
      if (mounted) setState(() => results = named);
      final last = widget.prefs.lastDevice;
      if (last.isNotEmpty && connectingId == null) {
        for (final x in named) {
          if (x.device.remoteId.str == last) {
            _connect(x.device);
            break;
          }
        }
      }
    });
    FlutterBluePlus.isScanning.listen((s) {
      if (mounted) setState(() => scanning = s);
    });
    _adapterSub = FlutterBluePlus.adapterState.listen((s) {
      if (mounted) setState(() => adapter = s);
      if (s == BluetoothAdapterState.on) _scan();
    });
  }

  Future<void> _scan() async {
    setState(() => error = null);
    try {
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 10));
    } catch (e) {
      setState(() => error = 'Scan failed: $e');
    }
  }

  Future<void> _connect(BluetoothDevice d) async {
    if (connectingId != null) return;
    setState(() {
      connectingId = d.remoteId.str;
      error = null;
    });
    try {
      await FlutterBluePlus.stopScan();
      final link = await BleLink.connect(d);
      widget.prefs.lastDevice = d.remoteId.str;
      widget.onLink(link);
    } catch (e) {
      if (mounted) {
        setState(() {
          error = 'Could not connect: $e';
          connectingId = null;
        });
      }
    }
  }

  @override
  void dispose() {
    _sub?.cancel();
    _adapterSub?.cancel();
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Neon.bg,
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(18),
          child: Column(crossAxisAlignment: CrossAxisAlignment.stretch, children: [
            const SizedBox(height: 20),
            Center(child: Text.rich(TextSpan(style: tron(34, Neon.lavLight), children: const [
              TextSpan(text: 'RE:', style: TextStyle(color: Neon.purple)),
              TextSpan(text: 'FLOAT'),
            ]))),
            const SizedBox(height: 10),
            Row(children: [
              Expanded(child: Container(height: 2, color: Neon.purple)),
              Padding(padding: const EdgeInsets.symmetric(horizontal: 12), child: Text('RINK-MIXED', style: tron(18, Neon.green))),
              Expanded(child: Container(height: 2, color: Neon.purple)),
            ]),
            const SizedBox(height: 8),
            Center(child: Text('A CUSTOM REFLOAT REMIX', style: tron(11, Neon.dim, weight: 500, spacing: 1))),
            const SizedBox(height: 28),
            Row(children: [
              Text('BOARDS', style: tron(13, Neon.lav)),
              const Spacer(),
              if (scanning) const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2, color: Neon.green)),
              TextButton(
                onPressed: adapter == BluetoothAdapterState.on && !scanning ? _scan : null,
                child: Text('SCAN', style: tron(12, Neon.green)),
              ),
            ]),
            if (adapter != BluetoothAdapterState.on && adapter != BluetoothAdapterState.unknown)
              const Padding(
                padding: EdgeInsets.symmetric(vertical: 8),
                child: Text('Turn on Bluetooth (and allow this app to use it) to find your board.', style: TextStyle(color: Neon.orange)),
              ),
            if (error != null) Padding(padding: const EdgeInsets.symmetric(vertical: 8), child: Text(error!, style: const TextStyle(color: Neon.red))),
            Expanded(
              child: results.isEmpty
                  ? Center(child: Text(scanning ? 'Looking for boards…' : 'No boards found yet. Power the board on and tap SCAN.',
                      textAlign: TextAlign.center, style: const TextStyle(color: Neon.dim)))
                  : ListView(children: [
                      for (final r in results)
                        Container(
                          margin: const EdgeInsets.only(bottom: 8),
                          decoration: BoxDecoration(
                            color: Neon.panel,
                            border: Border.all(color: r.advertisementData.serviceUuids.contains(BleLink.serviceUuid) ? Neon.purple : const Color(0xFF2A1A4A), width: 2),
                            borderRadius: BorderRadius.circular(10),
                          ),
                          child: ListTile(
                            title: Text(r.advertisementData.advName.isNotEmpty ? r.advertisementData.advName : r.device.platformName,
                                style: tron(14, Neon.mint)),
                            subtitle: Text('${r.device.remoteId.str}   ${r.rssi} dBm', style: const TextStyle(color: Neon.dim, fontSize: 11)),
                            trailing: connectingId == r.device.remoteId.str
                                ? const SizedBox(width: 18, height: 18, child: CircularProgressIndicator(strokeWidth: 2, color: Neon.green))
                                : Text('CONNECT', style: tron(11, Neon.green)),
                            onTap: () => _connect(r.device),
                          ),
                        ),
                    ]),
            ),
            OutlinedButton(
              style: OutlinedButton.styleFrom(side: const BorderSide(color: Neon.purple, width: 2), padding: const EdgeInsets.all(14)),
              onPressed: () => widget.onLink(DemoLink()),
              child: Text('TRY THE DEMO BOARD', style: tron(13, Neon.purple)),
            ),
          ]),
        ),
      ),
    );
  }
}
