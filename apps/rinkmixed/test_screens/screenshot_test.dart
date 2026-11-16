// Renders the home screen against the demo board and saves PNGs (run with
// --update-goldens in CI; the images are published with each app build).
import 'dart:io';

import 'package:flutter/material.dart';
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
  await tester.runAsync(loadFonts);
  tester.view.physicalSize = const Size(1179, 2556);
  tester.view.devicePixelRatio = 3;
  addTearDown(tester.view.reset);
  SharedPreferences.setMockInitialValues({'sheet': sheet, 'tab': 2, 'rail': 0, 'peakSpeed': 33.0, 'peakDuty': 0.62});
  final prefs = await Prefs.load();
  final board = Board(prefs);
  await board.attach(DemoLink());
  await tester.pumpWidget(MaterialApp(
    theme: appTheme(),
    debugShowCheckedModeBanner: false,
    home: Dashboard(board: board, onDisconnect: () {}),
  ));
  for (var i = 0; i < 120; i++) {
    await tester.pump(const Duration(milliseconds: 100));  // 12 s of demo riding
  }
  await expectLater(find.byType(Dashboard), matchesGoldenFile('goldens/$name.png'));
  await board.detach();
  await tester.pump(const Duration(seconds: 1));
}

void main() {
  testWidgets('home collapsed', (t) => shot(t, 'home_collapsed'));
  testWidgets('home open', (t) => shot(t, 'home_open', sheet: 1));
}
