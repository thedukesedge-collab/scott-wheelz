// Canvas drawings ported from the package UI / simulator.
import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:path_drawing/path_drawing.dart';

import '../prefs.dart';
import '../refloat/board.dart';
import '../theme.dart';

const _deg = math.pi / 180;

Paint _stroke(Color c, double w) => Paint()
  ..style = PaintingStyle.stroke
  ..strokeWidth = w
  ..color = c
  ..strokeCap = StrokeCap.round
  ..strokeJoin = StrokeJoin.round
  ..isAntiAlias = true;

/// Soft wide halo under a crisp line.
void glowStroke(Canvas c, Path p, Color col, double w) {
  c.drawPath(p, _stroke(col.withOpacity(0.22), w * 3.5));
  c.drawPath(p, _stroke(col, w));
}

void drawText(Canvas c, String s, Offset at, TextStyle st, {TextAlign align = TextAlign.center, double opacity = 1}) {
  final tp = TextPainter(
    text: TextSpan(text: s, style: opacity < 1 ? st.copyWith(color: st.color!.withOpacity(opacity)) : st),
    textDirection: TextDirection.ltr,
  )..layout();
  final dx = align == TextAlign.center ? tp.width / 2 : align == TextAlign.right ? tp.width : 0.0;
  tp.paint(c, at - Offset(dx, tp.height / 2));
}

// ---------------- speedometer + current / duty dials ----------------

class SpeedoPainter extends CustomPainter {
  SpeedoPainter(this.b, this.u) : super(repaint: b);
  final Board b;
  final double u;

  @override
  void paint(Canvas c, Size size) {
    final w = size.width, h = size.height;
    final r = math.max(u, math.min(w * 0.46, (h - 0.4 * u) / 1.58));
    final cx = w / 2, cy = r * 1.08 + 0.05 * u;
    final center = Offset(cx, cy);
    const start = 150 * _deg, sweep = 240 * _deg, n = 40;
    final t = r * 0.17, rSeg = r - t / 2, rDuty = r - t - r * 0.07, rLabel = rDuty - r * 0.14;
    final speed = (b.v('motor.speed') * b.imperial).abs();
    final dialMax = b.prefs.dialMax.toDouble();
    final frac = speed.isFinite ? (speed / dialMax).clamp(0.0, 1.0) : 0.0;
    const dim = Color(0xFF1E1433);

    // outer ring + ticks
    final outer = Path()..addArc(Rect.fromCircle(center: center, radius: r * 1.04), start, sweep);
    glowStroke(c, outer, Neon.purple, 1.5);
    final ticks = Path();
    for (var j = 0; j <= 8; j++) {
      final a = start + sweep * j / 8;
      ticks.moveTo(cx + math.cos(a) * r * 1.04, cy + math.sin(a) * r * 1.04);
      ticks.lineTo(cx + math.cos(a) * r * 1.09, cy + math.sin(a) * r * 1.09);
    }
    c.drawPath(ticks, _stroke(Neon.purple, 1.5)..strokeCap = StrokeCap.butt);

    // segments: unlit, halo under lit, lit
    const gap = 0.012;
    final segRect = Rect.fromCircle(center: center, radius: rSeg);
    Color segCol(double pos) => pos < 0.7 ? Neon.green : pos < 0.9 ? Neon.purple : Neon.red;
    for (var pass = 0; pass < 3; pass++) {
      for (var i = 0; i < n; i++) {
        final pos = (i + 0.5) / n;
        final lit = pos <= frac;
        if ((pass == 0) == lit) continue;
        final a0 = start + sweep * i / n + gap / 2;
        final a1 = start + sweep * (i + 1) / n - gap / 2;
        final p = Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = pass == 1 ? t + r * 0.07 : t
          ..color = !lit ? dim : pass == 1 ? segCol(pos).withOpacity(0.22) : segCol(pos);
        c.drawArc(segRect, a0, a1 - a0, false, p);
      }
    }

    // duty ring
    final dutyRect = Rect.fromCircle(center: center, radius: rDuty);
    final dw = math.max(2.0, r * 0.045);
    c.drawArc(dutyRect, start, sweep, false, Paint()..style = PaintingStyle.stroke..strokeWidth = dw..color = dim);
    final duty = b.v('motor.duty_cycle').abs();
    if (duty.isFinite && duty > 0.002) {
      final d = duty.clamp(0.0, 1.0);
      final col = d >= 0.9 ? Neon.red : d >= 0.8 ? Neon.orange : Neon.purple;
      c.drawArc(dutyRect, start, sweep * d, false, Paint()..style = PaintingStyle.stroke..strokeWidth = dw * 2.6..color = col.withOpacity(0.25));
      c.drawArc(dutyRect, start, sweep * d, false, Paint()..style = PaintingStyle.stroke..strokeWidth = dw..color = col);
    }

    // scale labels
    for (var j = 0; j <= 4; j++) {
      final a = start + sweep * j / 4;
      drawText(c, '${(dialMax * j / 4).round()}', Offset(cx + math.cos(a) * rLabel, cy + math.sin(a) * rLabel),
          mono(math.max(9, r * 0.1), Neon.dim));
    }

    // trip / life minis or WHEELSLIP
    final digitsY = cy - r * 0.08;
    if (b.wheelslip) {
      drawText(c, 'WHEELSLIP', Offset(cx, digitsY - r * 0.33), tron(math.max(9, r * 0.11), Neon.red));
    } else {
      final mw = r * 0.36, my = cy - r * 0.55;
      final vs = math.max(0.22 * u, r * 0.1), ls = math.max(0.17 * u, r * 0.07);
      final trip = b.tachAbsM * 0.001 * b.imperial, life = b.odometerM * 0.001 * b.imperial;
      for (final e in [(cx - r * 0.44 + mw / 2, trip, 'TRIP'), (cx + r * 0.08 + mw / 2, life, 'LIFE')]) {
        drawText(c, '${fx1(e.$2)}${b.distUnit}', Offset(e.$1, my + vs * 0.6), tron(vs, Neon.lavLight), opacity: 0.75);
        drawText(c, e.$3, Offset(e.$1, my + vs * 1.2 + ls * 0.6), tron(ls, Neon.lav), opacity: 0.75);
      }
    }

    // digits, unit, erpm, duty
    final digits = speed.isFinite ? '${speed.round()}' : '--';
    drawText(c, digits, Offset(cx, digitsY), tron(r * 0.5, Neon.mint, glow: glowOf(Neon.green, r * 0.12)));
    var ly = digitsY + r * 0.3;
    drawText(c, b.speedUnit.toUpperCase(), Offset(cx, ly), tron(math.max(9, r * 0.12), Neon.green, weight: 500));
    ly += r * 0.13;
    final erpm = b.v('motor.erpm');
    drawText(c, '${erpm.isFinite ? erpm.round() : '--'} ERPM', Offset(cx, ly), mono(math.max(8, r * 0.095), Neon.dim));
    ly += r * 0.11;
    final dcol = duty >= 0.9 ? Neon.red : duty >= 0.8 ? Neon.orange : Neon.purple;
    drawText(c, 'DUTY ${duty.isFinite ? (duty * 100).round() : '--'}%', Offset(cx, ly), mono(math.max(8, r * 0.095), dcol));

    // current / duty dials in the bottom corners
    if (h >= 2.6 * u) {
      final ds = 1.9 * u;
      final dy = h - 0.35 * u - ds / 2;
      _dial(c, Offset(ds / 2, dy), ds, b.v('motor.current'), -60, 80, 'A', 'CURRENT', false);
      _dial(c, Offset(w - ds / 2, dy), ds, duty * 100, 0, 100, '%', 'DUTY', true);
    }
  }

  void _dial(Canvas c, Offset o, double size, double v, double mn, double mx, String unit, String label, bool warnHigh) {
    final r = size * 0.36, lw = size * 0.075;
    final rect = Rect.fromCircle(center: o, radius: r);
    c.drawCircle(o, r, Paint()..style = PaintingStyle.stroke..strokeWidth = lw..color = Neon.lightBg);
    if (v.isFinite) {
      final f = v >= 0 ? (v / mx).clamp(0.0, 1.0) : (v / mn).clamp(0.0, 1.0);
      if (f > 0.002) {
        final col = warnHigh && v >= 80 ? Neon.orange : Neon.green;
        const base = math.pi / 2;
        final sw = f * math.pi * 2 * 0.999;
        final p = Paint()..style = PaintingStyle.stroke..strokeWidth = lw..color = col;
        if (v >= 0) {
          c.drawArc(rect, base, sw, false, p);
        } else {
          c.drawArc(rect, base - sw, sw, false, p);
        }
      }
    }
    c.drawRect(Rect.fromCenter(center: o + Offset(0, r), width: 1, height: lw * 1.3), Paint()..color = Neon.mint);
    drawText(c, v.isFinite ? '${v.round()}' : '--', o - Offset(size * 0.06, 0), tron(0.4 * u, Neon.lavLight));
    drawText(c, unit, o + Offset(size * 0.17, 0.04 * u), tron(0.27 * u, Neon.lav), align: TextAlign.left);
    drawText(c, label, o + Offset(0, r + lw + 0.22 * u), tron(0.27 * u, Neon.lav));
  }

  @override
  bool shouldRepaint(covariant SpeedoPainter old) => true;
}

// ---------------- footpad sensors ----------------

final _fpOuter = parseSvgPathData('M 62 2 L 38 2 A 36 15 180 0 0 2 17 L 2 78 L 98 78 L 98 17 A 36 15 0 0 0 62 2 M 50 11 L 50 69');
final _fpLeft = parseSvgPathData('M 44 8 L 44 72 L 8 72 L 8 18.4 A 30 10.4 180 0 1 38 8 L 44 8');
final _fpRight = parseSvgPathData('M 56 72 L 92 72 L 92 18.4 A 30 10.4 0 0 0 62 8 L 56 8 L 56 72');

class FootpadPainter extends CustomPainter {
  FootpadPainter(this.b) : super(repaint: b);
  final Board b;

  @override
  void paint(Canvas c, Size size) {
    final s = size.width;
    final k = s / 100;
    final left = b.fpState == Rf.fsLeft || b.fpState == Rf.fsBoth;
    final right = b.fpState == Rf.fsRight || b.fpState == Rf.fsBoth;
    c.save();
    c.translate(0, s * 0.02);
    c.scale(k, k);
    glowStroke(c, _fpOuter, Neon.purple, 2.5 / k * 1.2);
    for (final e in [(_fpLeft, left), (_fpRight, right)]) {
      if (e.$2) {
        c.drawPath(e.$1, Paint()..color = Neon.green.withOpacity(0.25));
        glowStroke(c, e.$1, Neon.green, 2 / k);
      }
    }
    c.restore();
    drawText(c, 'ADC1', Offset(s * 0.26, s * 0.47), tron(s * 0.12, left ? Neon.green : const Color(0xFF9B6BD6), weight: 500));
    drawText(c, 'ADC2', Offset(s * 0.74, s * 0.47), tron(s * 0.12, right ? Neon.green : const Color(0xFF9B6BD6), weight: 500));
    drawText(c, '${fx1(b.v('footpad.adc1'))}V', Offset(s * 0.26, s * 0.92), mono(s * 0.14, Neon.text));
    drawText(c, '${fx1(b.v('footpad.adc2'))}V', Offset(s * 0.74, s * 0.92), mono(s * 0.14, Neon.text));
  }

  @override
  bool shouldRepaint(covariant CustomPainter old) => true;
}

// ---------------- front / roll view ----------------

class RollPainter extends CustomPainter {
  RollPainter(this.b) : super(repaint: b);
  final Board b;

  @override
  void paint(Canvas c, Size size) {
    final s = size.width;
    final labelSize = s * 0.13;
    final ch = s - labelSize * 1.1;
    final sc = ch * 0.95 / 100, uu = 1 / sc;
    final pitch = b.v('imu.pitch');
    final roll = b.v('imu.roll');
    final lift = -29 * ((pitch.isFinite ? pitch : 0) / 20).clamp(-1.0, 1.0);
    c.save();
    c.translate(s / 2, ch / 2);
    c.scale(sc, sc);
    // fixed scale
    final scale = Path()..addArc(Rect.fromCircle(center: Offset.zero, radius: 47), math.pi * 1.17, math.pi * 0.66);
    for (var t = -45; t <= 45; t += 15) {
      final ta = (t - 90) * _deg, r1 = t == 0 ? 42.0 : 44.0;
      scale.moveTo(math.cos(ta) * r1, math.sin(ta) * r1);
      scale.lineTo(math.cos(ta) * 47, math.sin(ta) * 47);
    }
    c.drawPath(scale, _stroke(Neon.purple.withOpacity(0.5), 1.2 * uu));
    c.rotate((roll.isFinite ? roll : 0) * _deg);
    c.drawPath(Path()..moveTo(0, -48)..lineTo(-3, -53)..lineTo(3, -53)..close(), Paint()..color = Neon.green);
    final slick = b.prefs.tire == 1;
    glowStroke(c, Path()..addRRect(RRect.fromLTRBR(-25, -40, 25, 40, Radius.circular(slick ? 24 : 17))), Neon.green, 2 * uu);
    if (!slick) {
      final tread = Path();
      for (var y = -30.0; y <= 30; y += 7.5) {
        tread.moveTo(-18, y);
        tread.lineTo(18, y);
      }
      c.drawPath(tread, _stroke(Neon.green.withOpacity(0.35), 1 * uu));
    }
    c.drawCircle(Offset.zero, 3.5, Paint()..color = Neon.purple);
    final block = RRect.fromLTRBR(-47, lift - 10, 47, lift + 10, const Radius.circular(4));
    c.drawRRect(block, Paint()..color = const Color(0xFF0B0716));
    glowStroke(c, Path()..addRRect(block), Neon.purple, 2 * uu);
    final bar = Path()..moveTo(-34, lift - 2)..lineTo(34, lift - 2);
    glowStroke(c, bar, Neon.mint, 2.5 * uu);
    c.drawPath(bar, _stroke(Neon.green.withOpacity(0.6), 6 * uu));
    c.restore();
    drawText(c, 'ROLL  ${fx1(roll)}°', Offset(s / 2, s - labelSize * 0.55), mono(labelSize, Neon.text));
  }

  @override
  bool shouldRepaint(covariant CustomPainter old) => true;
}

// ---------------- side / pitch view ----------------

double _lerp(double x, List<double> xs, List<double> ys) {
  for (var i = 1; i < xs.length; i++) {
    if (x <= xs[i]) return ys[i - 1] + (x - xs[i - 1]) / (xs[i] - xs[i - 1]) * (ys[i] - ys[i - 1]);
  }
  return ys.last;
}

class SidePainter extends CustomPainter {
  SidePainter(this.b, this.u) : super(repaint: b);
  final Board b;
  final double u;

  Prefs get p => b.prefs;

  double railY(double x) {
    if (p.rail == 1) return _lerp(x, [-97, -33, -12, 12, 33, 97], [0, 3.2, -1.2, -1.2, 3.2, 0]);
    if (p.rail == 2) return _lerp(x, [-97, -80, -40, 40, 80, 97], [-5, -2, 0, 0, -2, -5]);
    return 0;
  }

  Path railPath() {
    final c = Path();
    if (p.rail == 2) {
      c.moveTo(-97, railY(-97) - 8);
      for (var x = -92.0; x <= 92; x += 4) {
        c.lineTo(x, railY(x) - 7);
      }
      c.lineTo(97, railY(97) - 8);
      c.lineTo(96, railY(96) - 2);
      c.lineTo(84, railY(84) + 7);
      for (var x = 80.0; x >= -80; x -= 4) {
        c.lineTo(x, railY(x) + 7);
      }
      c.lineTo(-84, railY(-84) + 7);
      c.lineTo(-96, railY(-96) - 2);
      return c..close();
    }
    if (p.rail == 1) {
      const xs = [-97.0, -33.0, -12.0, 12.0, 33.0, 97.0];
      c.moveTo(xs[0], railY(xs[0]) - 6);
      for (var i = 1; i < xs.length; i++) {
        c.lineTo(xs[i], railY(xs[i]) - 6);
      }
      c.lineTo(86, railY(86) + 6);
      for (var j = xs.length - 2; j >= 1; j--) {
        c.lineTo(xs[j], railY(xs[j]) + 6);
      }
      c.lineTo(-86, railY(-86) + 6);
      return c..close();
    }
    c.moveTo(-97, -3);
    for (var x = -95.0; x <= 95; x += 5) {
      c.lineTo(x, -8);
    }
    c.lineTo(97, -3);
    c.lineTo(92, 4);
    for (var x = 85.0; x >= -85; x -= 5) {
      c.lineTo(x, 8);
    }
    c.lineTo(-92, 4);
    return c..close();
  }

  Path padPath(double x0, double x1) {
    final off = p.rail == 0 ? 11.0 : 9.0;
    final c = Path()..moveTo(x0, railY(x0) - off);
    for (var x = x0 + 2; x < x1; x += 2) {
      c.lineTo(x, railY(x) - off);
    }
    return c..lineTo(x1, railY(x1) - off);
  }

  Path _arcs(double r, double halfGap) {
    final a0 = math.asin(math.min(1, halfGap / r));
    final rect = Rect.fromCircle(center: Offset.zero, radius: r);
    return Path()
      ..addArc(rect, a0, math.pi - 2 * a0)
      ..addArc(rect, math.pi + a0, math.pi - 2 * a0);
  }

  @override
  void paint(Canvas c, Size size) {
    final s = size.width;
    final sc = s * 0.88 / 200, uu = 1 / sc, flip = p.flip ? -1.0 : 1.0;
    final pitch = b.v('imu.pitch'), bal = b.v('imu.balance_pitch');
    void frame(double rot) {
      c.translate(s / 2, s / 2);
      c.scale(flip, 1);
      c.rotate((rot.isFinite ? rot : 0) * _deg);
      c.scale(sc, sc);
    }

    // balance outline
    c.save();
    frame(bal);
    c.drawPath(railPath(), _stroke(const Color(0xFF6A4A9E), 2 * uu));
    c.restore();

    c.save();
    frame(pitch);
    glowStroke(c, railPath(), Neon.purple, 2 * uu);
    final atom = p.rail == 2;
    if (atom) {
      final seam = Path()..moveTo(-95, railY(-95) - 3);
      for (var x = -91.0; x <= 95; x += 4) {
        seam.lineTo(x, railY(x) - 3);
      }
      glowStroke(c, seam, Neon.purple, 1 * uu);
      final lattice = Path();
      for (final side in [-1.0, 1.0]) {
        for (var c0 = 22.0; c0 < 77; c0 += 11) {
          final xa = side * c0, xb = side * (c0 + 11);
          lattice.moveTo(xa, railY(xa) - 1);
          lattice.lineTo(xb, railY(xb) + 5);
          lattice.moveTo(xa, railY(xa) + 5);
          lattice.lineTo(xb, railY(xb) - 1);
        }
      }
      glowStroke(c, lattice, Neon.purple, 0.8 * uu);
      for (final e in [-1.0, 1.0]) {
        final ins = Path()
          ..moveTo(e * 90, railY(e * 90) - 1)
          ..lineTo(e * 84, railY(e * 84) - 1)
          ..lineTo(e * 72, railY(e * 72) + 5)
          ..lineTo(e * 78, railY(e * 78) + 5)
          ..close();
        glowStroke(c, ins, Neon.green, 1.5 * uu);
      }
    }
    // tire
    glowStroke(c, _arcs(36, 8), Neon.green, (p.tire == 1 ? 3 : 2) * uu);
    if (p.tire == 0) {
      final tread = Path();
      for (var i = 0; i < 36; i++) {
        final a = i * math.pi / 18, y1 = 31 * math.sin(a);
        if (y1.abs() < 10) continue;
        tread.moveTo(31 * math.cos(a), y1);
        tread.lineTo(36 * math.cos(a), 36 * math.sin(a));
      }
      c.drawPath(tread, _stroke(Neon.green, 1.2 * uu));
    }
    if (atom) {
      glowStroke(c, _arcs(18, 8), Neon.green, 1.8 * uu);
      glowStroke(c, _arcs(13, 8), Neon.green, 1.2 * uu);
      final bolts = Path();
      for (var bi = 0; bi < 8; bi++) {
        final ba = bi * math.pi / 4 + math.pi / 8, bx = 15.5 * math.cos(ba), by = 15.5 * math.sin(ba);
        if (by.abs() < 9) continue;
        bolts.addOval(Rect.fromCircle(center: Offset(bx, by), radius: 1.3));
      }
      glowStroke(c, bolts, Neon.green, 1 * uu);
      final axle = Path();
      for (final ax in [-11.0, -6.0, 6.0, 11.0]) {
        axle.addOval(Rect.fromCircle(center: Offset(ax, railY(0) + 2), radius: 1.8));
      }
      glowStroke(c, axle, Neon.green, 1 * uu);
    } else {
      glowStroke(c, _arcs(18, 8), Neon.purple, 1.5 * uu);
      c.drawCircle(Offset.zero, 3.5, Paint()..color = Neon.purple);
      glowStroke(c, Path()..addArc(Rect.fromCircle(center: Offset.zero, radius: 43), math.pi * 1.13, math.pi * 0.74), Neon.purple, 2 * uu);
    }
    final fpOn = b.fpState != Rf.fsNone;
    glowStroke(c, padPath(-88, -46), fpOn ? Neon.green : const Color(0xFF1D6B3C), (fpOn ? 4 : 2.5) * uu);
    c.scale(flip, 1);  // keep the labels readable
    final ls = math.max(8.0, 0.24 * u) * uu;
    drawText(c, 'NOSE', Offset(-70 * flip, 26), mono(ls, Neon.green).copyWith(fontWeight: FontWeight.bold));
    drawText(c, 'TAIL', Offset(70 * flip, 26), mono(ls, Neon.purple).copyWith(fontWeight: FontWeight.bold));
    c.restore();

    if (b.running) {
      c.save();
      frame(b.v('setpoint'));
      final sp = Path()..moveTo(-86, 0)..lineTo(-43, 0)..moveTo(43, 0)..lineTo(86, 0);
      c.drawPath(sp, _stroke(Neon.orange, 2 * uu));
      c.restore();
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter old) => true;
}

// ---------------- icons ----------------

final pencilPath = parseSvgPathData('M 10.59 24 L 17.63 32.42 L 6 35 L 10.59 24 M 10.59 24 L 34.39 4 L 41.49 12.42 L 17.69 32.42 M 4 42 L 46 42');
final gearPath = parseSvgPathData(
    'M 14.25 37.81 L 10.59 39.42 A 3 3 0 0 1 6.98 38.47 A 22.5 22.5 0 0 1 4.32 33.87 A 3 3 0 0 1 5.30 30.27 L 8.53 27.9 A 3.6 3.6 0 0 0 8.53 22.1 L 5.30 19.73 A 3 3 0 0 1 4.32 16.13 A 22.5 22.5 0 0 1 6.98 11.53 A 3 3 0 0 1 10.59 10.58 L 14.25 12.19 A 3.6 3.6 0 0 0 19.28 9.29 L 19.79 5.31 A 3 3 0 0 1 22.35 2.66 A 22.5 22.5 0 0 1 27.65 2.66 A 3 3 0 0 1 30.28 5.31 L 30.72 9.29 A 3.6 3.6 0 0 0 35.75 12.19 L 39.41 10.58 A 3 3 0 0 1 43.02 11.53 A 22.5 22.5 0 0 1 45.67 16.13 A 3 3 0 0 1 44.7 19.73 L 41.47 22.1 A 3.6 3.6 0 0 0 41.47 27.9 L 44.7 30.27 A 3 3 0 0 1 45.68 33.87 A 22.5 22.5 0 0 1 43.02 38.47 A 3 3 0 0 1 39.41 39.42 L 35.75 37.81 A 3.6 3.6 0 0 0 30.72 40.71 L 30.28 44.69 A 3 3 0 0 1 27.65 47.34 A 22.5 22.5 0 0 1 22.35 47.34 A 3 3 0 0 1 19.72 44.69 L 19.28 40.71 A 3.6 3.6 0 0 0 14.25 37.81 M 31.5 25 A 6.5 6.5 0 1 1 18.5 25 A 6.5 6.5 0 1 1 31.5 25');
final enginePath = parseSvgPathData(
    'M14 8 L14 12 L20 12 L20 16 L14 16 L10 20 L10 26 L6 26 L6 20 L2 20 L2 36 L6 36 L6 30 L10 30 L10 36 L16 36 L20 40 L36 40 L36 32 L40 32 L40 38 L46 38 L46 18 L40 18 L40 24 L36 24 L36 16 L24 16 L24 12 L30 12 L30 8 Z');

class IconPainter extends CustomPainter {
  IconPainter(this.path, this.color, {this.box = 50, this.lineWidth = 3.2});
  final Path path;
  final Color color;
  final double box, lineWidth;

  @override
  void paint(Canvas c, Size size) {
    final k = math.min(size.width, size.height) / box;
    c.translate((size.width - box * k) / 2, (size.height - box * k) / 2);
    c.scale(k, k);
    glowStroke(c, path, color, lineWidth);
  }

  @override
  bool shouldRepaint(covariant IconPainter old) => old.color != color || old.path != path;
}

// ---------------- DATA graph ----------------

class PlotSeries {
  const PlotSeries(this.id, this.name, this.color, this.defaultOn, {this.scale = 1});
  final String id, name;
  final Color color;
  final bool defaultOn;
  final double scale;
}

const plotSeries = [
  PlotSeries('motor.erpm', 'ERPM', Color(0xFF469138), true),
  PlotSeries('motor.duty_cycle', 'Duty Cycle', Color(0xFFE3AC34), true, scale: 100),
  PlotSeries('motor.current', 'Motor Current', Color(0xFFE33421), true),
  PlotSeries('imu.pitch', 'Pitch', Color(0xFFB1B277), true),
  PlotSeries('imu.balance_pitch', 'Balance Pitch', Color(0xFF53B3BF), true),
  PlotSeries('torque_tilt.setpoint', 'Torque Tilt Setpoint', Color(0xFFDF4A98), true),
  PlotSeries('atr.setpoint', 'ATR Setpoint', Color(0xFF7640D6), true),
  PlotSeries('setpoint', 'Setpoint', Color(0xFFE57E20), true),
  PlotSeries('motor.speed', 'Speed', Color(0xFF3F9469), false),
  PlotSeries('motor.batt_voltage', 'Battery Voltage', Color(0xFF6E6EE0), false),
  PlotSeries('motor.batt_current', 'Battery Current', Color(0xFFCF44CD), false),
  PlotSeries('motor.mosfet_temp', 'Controller Temp', Color(0xFFDDAE68), false),
  PlotSeries('motor.motor_temp', 'Motor Temp', Color(0xFF88BB93), false),
  PlotSeries('imu.roll', 'Roll', Color(0xFFC68F7E), false),
  PlotSeries('footpad.adc1', 'ADC1 Voltage', Color(0xFF69AAD7), false),
  PlotSeries('footpad.adc2', 'ADC2 Voltage', Color(0xFF4A8772), false),
  PlotSeries('state.running', 'Running', Color(0xFF3F6620), false),
  PlotSeries('state.wheelslip', 'Wheelslip', Color(0xFF893075), false),
];

class PlotPainter extends CustomPainter {
  PlotPainter(this.b, this.visible, this.cursor) : super(repaint: b);
  final Board b;
  final Set<String> visible;
  final double? cursor;  // 0..1 across the width

  @override
  void paint(Canvas c, Size size) {
    final w = size.width, h = size.height;
    final buf = b.plot;
    c.drawRect(Offset.zero & size, Paint()..color = Neon.bg.withOpacity(0.85));
    final grid = Paint()..color = const Color(0xFF1D1235)..strokeWidth = 1;
    const secs = 60;
    for (var sx = 0; sx <= secs; sx += 10) {
      final x = w - sx / secs * w;
      c.drawLine(Offset(x, 0), Offset(x, h), grid);
      if (sx > 0 && sx < secs) drawText(c, '-${sx}s', Offset(x - 2, h - 7), mono(9, const Color(0xFF4B3A75)), align: TextAlign.right);
    }
    for (var r = 1; r < 4; r++) {
      c.drawLine(Offset(0, h * r / 4), Offset(w, h * r / 4), grid);
    }
    final n = buf.count;
    if (n < 2) return;
    final dx = w / (buf.capacity - 1), x0 = w - n * dx;
    for (final sr in plotSeries) {
      if (!visible.contains(sr.id)) continue;
      var mn = double.infinity, mx = -double.infinity;
      for (var k = 0; k < n; k++) {
        final v = buf.at(sr.id, k);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
      }
      if (mn < 0 && mx > 0) {
        final m = math.max(-mn, mx);
        mn = -m;
        mx = m;
      }
      if (mx - mn < 1e-6) {
        mx += 1;
        mn -= 1;
      }
      final pad = (mx - mn) * 0.08;
      mn -= pad;
      mx += pad;
      final path = Path();
      for (var k = 0; k < n; k++) {
        final x = x0 + k * dx, y = h - (buf.at(sr.id, k) - mn) / (mx - mn) * h;
        if (k == 0) {
          path.moveTo(x, y);
        } else {
          path.lineTo(x, y);
        }
      }
      c.drawPath(path, _stroke(sr.color, 1.6));
    }
    if (cursor != null) {
      final x = cursor! * w;
      c.drawLine(Offset(x, 0), Offset(x, h), Paint()..color = Neon.mint.withOpacity(0.7));
    }
  }

  @override
  bool shouldRepaint(covariant CustomPainter old) => true;
}
