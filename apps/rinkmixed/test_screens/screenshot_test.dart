// Renders the home screen against the demo board and saves PNGs (run with
// --update-goldens in CI; the images are published with each app build).
import 'dart:io';

import 'dart:ui' show ImageByteFormat;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:rinkmixed/prefs.dart';
import 'package:rinkmixed/refloat/board.dart';
import 'package:rinkmixed/refloat/demo_link.dart';
import 'package:rinkmixed/theme.dart';
import 'package:rinkmixed/ui/dashboard.dart';
import 'package:shared_preferences/shared_preferences.dart';

Future<void> _font(String family, String path) async {
  final f = File(path);
  if (!f.existsSync()) return;
  final loader = FontLoader(family)..addFont(Future.value(ByteData.sublistView(f.readAsBytesSync())));
  await loader.load();
}

Future<void> loadFonts() async {
  await _font('Orbitron', 'assets/fonts/Orbitron.ttf');
  final root = Platform.environment['FLUTTER_ROOT'] ?? '';
  final roboto = '$root/bin/cache/artifacts/material_fonts/Roboto-Regular.ttf';
  await _font('Roboto', roboto);
  await _font('monospace', roboto);
}

Future<void> shot(WidgetTester tester, String name, {double sheet = 0}) async {
  tester.view.physicalSize = const Size(1179, 2556);
  tester.view.devicePixelRatio = 3;
  addTearDown(tester.view.reset);
  SharedPreferences.setMockInitialValues({'sheet': sheet, 'tab': 2, 'rail': 0, 'peakSpeed': 33.0, 'peakDuty': 0.62});
  // Everything that runs on real timers / IO goes through runAsync; the demo
  // board then rides for a few real seconds before the frame is captured.
  final board = (await tester.runAsync(() async {
    await loadFonts();
    final b = Board(await Prefs.load());
    await b.attach(DemoLink());
    return b;
  }))!;
  final key = GlobalKey();
  await tester.pumpWidget(RepaintBoundary(
    key: key,
    child: MaterialApp(
      theme: appTheme(),
      debugShowCheckedModeBanner: false,
      home: Dashboard(board: board, onDisconnect: () {}),
    ),
  ));
  for (var i = 0; i < 8; i++) {
    await tester.runAsync(() => Future<void>.delayed(const Duration(milliseconds: 800)));
    await tester.pump(const Duration(milliseconds: 300));
  }
  await tester.runAsync(() async {
    final boundary = key.currentContext!.findRenderObject()! as RenderRepaintBoundary;
    final img = await boundary.toImage(pixelRatio: 3);
    final png = await img.toByteData(format: ImageByteFormat.png);
    final out = File('screens/$name.png')..createSync(recursive: true);
    out.writeAsBytesSync(png!.buffer.asUint8List());
    await board.detach();
  });
  await tester.pumpWidget(const SizedBox());
  await tester.pump(const Duration(seconds: 2));
}

void main() {
  const to = Timeout(Duration(minutes: 2));
  testWidgets('home collapsed', (t) => shot(t, 'home_collapsed'), timeout: to);
  testWidgets('home open', (t) => shot(t, 'home_open', sheet: 1), timeout: to);
}
