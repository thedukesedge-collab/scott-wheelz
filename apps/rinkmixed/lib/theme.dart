import 'dart:ui';

import 'package:flutter/material.dart';

class Neon {
  static const green = Color(0xFF39FF7A);
  static const purple = Color(0xFFB14CFF);
  static const red = Color(0xFFFF2B4F);
  static const orange = Color(0xFFFF8A1F);
  static const bg = Color(0xFF05060D);
  static const grid = Color(0xFF160D2B);
  static const lav = Color(0xFFC98BFF);
  static const lavLight = Color(0xFFEAD6FF);
  static const text = Color(0xFFC3A6FF);
  static const dim = Color(0xFF7D68AB);
  static const lightBg = Color(0xFF24123F);
  static const panel = Color(0xFF0B0718);
  static const mint = Color(0xFFD6FFE4);

  static const names = ['Green', 'Purple', 'Cyan', 'Magenta', 'Orange', 'Yellow', 'Red', 'Blue'];
  static const palette = [
    Color(0xFF39FF7A), Color(0xFFB14CFF), Color(0xFF00F0FF), Color(0xFFFF2BD6),
    Color(0xFFFF8A1F), Color(0xFFF5FF3B), Color(0xFFFF2B4F), Color(0xFF3B7BFF),
  ];
}

/// The digital font used everywhere (Orbitron, bundled).
TextStyle neonText(double size, Color color, {double weight = 700, double spacing = 0, List<Shadow>? glow}) {
  return TextStyle(
    fontFamily: 'Orbitron',
    fontSize: size,
    color: color,
    letterSpacing: spacing,
    fontVariations: [FontVariation('wght', weight)],
    shadows: glow,
    height: 1.1,
  );
}

TextStyle mono(double size, Color color) =>
    TextStyle(fontFamily: 'monospace', fontSize: size, color: color, height: 1.1);

List<Shadow> glowOf(Color c, [double blur = 8]) => [Shadow(color: c, blurRadius: blur)];

String fx1(double v) => v.isFinite ? v.toStringAsFixed(1) : '-.-';

ThemeData appTheme() => ThemeData(
      brightness: Brightness.dark,
      scaffoldBackgroundColor: Colors.black,
      colorScheme: const ColorScheme.dark(primary: Neon.green, secondary: Neon.purple, surface: Neon.panel),
      fontFamily: null,
      useMaterial3: true,
    );
