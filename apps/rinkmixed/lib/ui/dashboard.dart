// The home screen, laid out like the package UI.
import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../refloat/board.dart';
import '../theme.dart';
import 'painters.dart';
import 'sheet.dart';
import 'widgets.dart';

class Dashboard extends StatefulWidget {
  const Dashboard({super.key, required this.board, required this.onDisconnect});
  final Board board;
  final VoidCallback onDisconnect;

  @override
  State<Dashboard> createState() => _DashboardState();
}

class _DashboardState extends State<Dashboard> {
  late double sheetF = widget.board.prefs.sheet >= 0.5 ? 1 : 0;
  bool dragging = false;

  Board get b => widget.board;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      body: SafeArea(
        child: LayoutBuilder(builder: (context, c) {
          final rootW = c.maxWidth - 10, rootH = c.maxHeight - 10;
          final u = c.maxWidth * 0.1;
          final handleH = 0.75 * u;
          final sheetMin = handleH + 1;
          final sheetMax = math.max(handleH + 0.85 * u + 0.4 * u + 2, rootH * 0.4);
          final sheetH = sheetMin + (sheetMax - sheetMin) * sheetF;
          final hudH = rootH - sheetMin - 0.1 * u;
          final avail = rootH - sheetH - 0.1 * u;
          final fit = math.min(1.0, avail / hudH);
          return Stack(children: [
            Positioned(
              left: 5, top: 5, width: rootW, height: avail,
              child: Container(
                decoration: BoxDecoration(border: Border.all(color: Neon.purple), borderRadius: BorderRadius.circular(4), color: Neon.bg),
                child: CustomPaint(painter: GridPainter(math.max(12, 0.5 * u))),
              ),
            ),
            Positioned(
              left: 5, top: 5, width: rootW, height: hudH,
              child: Transform.scale(
                scale: fit,
                alignment: Alignment.topCenter,
                child: AnimatedBuilder(animation: b, builder: (context, _) => _hud(context, rootW, hudH, u)),
              ),
            ),
            AnimatedPositioned(
              duration: dragging ? Duration.zero : const Duration(milliseconds: 220),
              curve: Curves.easeOutCubic,
              left: 5, width: rootW, bottom: 5, height: sheetH,
              child: BottomPanel(
                board: b,
                u: u,
                fullHeight: sheetMax,
                handleHeight: handleH,
                onDrag: (dy) => setState(() {
                  dragging = true;
                  sheetF = (sheetF - dy / (sheetMax - sheetMin)).clamp(0.0, 1.0);
                }),
                onRelease: (moved) => setState(() {
                  dragging = false;
                  sheetF = moved ? (sheetF < 0.5 ? 0 : 1) : (sheetF < 0.5 ? 1 : 0);
                  b.prefs.sheet = sheetF;
                }),
              ),
            ),
          ]);
        }),
      ),
    );
  }

  Widget _hud(BuildContext context, double w, double h, double u) {
    final halfW = w / 2;
    final footS = halfW * 0.46;
    final pitchS = halfW * 0.85;
    final hudRowH = math.max(footS + 1.5 * u, pitchS);
    final fixed = 7.32 * u + hudRowH;
    final speedoH = (h - fixed).clamp(1.8 * u, 7 * u);
    return Column(children: [
      _battery(u, w),
      _nameRow(context, u),
      const Spacer(),
      SizedBox(height: speedoH, width: w, child: _speedo(u)),
      const Spacer(),
      SizedBox(height: hudRowH, width: w, child: _hudRow(u, halfW, footS, pitchS, hudRowH)),
      SizedBox(height: 0.08 * u),
      SizedBox(height: 1.1 * u, child: _midRow(u)),
      SizedBox(height: 0.1 * u),
      SizedBox(height: 0.85 * u, child: _tiltRow(u)),
      Padding(
        padding: EdgeInsets.fromLTRB(0.15 * u, 0.15 * u, 0.15 * u, 0.35 * u),
        child: SizedBox(height: 1.75 * u, child: HornButton(board: b, u: u)),
      ),
    ]);
  }

  Widget _battery(double u, double w) {
    final lvl = b.batteryLevel.isFinite ? b.batteryLevel.clamp(0.0, 1.0) : 0.0;
    final col = HSVColor.fromAHSV(1, lvl * 140, 0.85, 1).toColor();
    return SizedBox(
      height: 0.8 * u,
      child: Stack(children: [
        Positioned.fill(child: Container(color: Neon.lightBg)),
        Positioned(left: 0, top: 0, bottom: 0, width: w * lvl,
          child: Container(decoration: BoxDecoration(color: col, boxShadow: [BoxShadow(color: col, blurRadius: 8 + (1 - lvl) * 14)]))),
        Positioned.fill(
          child: Row(children: [
            Expanded(child: Center(child: Text('${fx1(b.vIn)}V', style: tron(0.34 * u, const Color(0xFFD9A6FF), glow: glowOf(Neon.purple, 6))))),
            Expanded(child: Center(child: Text(b.batteryLevel.isFinite ? '${(lvl * 100).round()}%' : '--%',
                style: tron(0.5 * u, const Color(0xFFD9A6FF), glow: glowOf(Neon.purple, 8))))),
            const Expanded(child: SizedBox()),
          ]),
        ),
      ]),
    );
  }

  Widget _nameRow(BuildContext context, double u) {
    return Padding(
      padding: EdgeInsets.fromLTRB(0.2 * u, 0.06 * u, 0, 0.06 * u),
      child: SizedBox(
        height: 1.1 * u,
        child: Row(children: [
          Expanded(
            child: Text(b.prefs.boardName, maxLines: 1, overflow: TextOverflow.ellipsis,
                style: tron(0.5 * u, Neon.green, spacing: 0.03 * u, glow: glowOf(Neon.green.withOpacity(0.55), 8))),
          ),
          TronIconButton(path: pencilPath, size: 0.9 * u, onTap: () async {
            final name = await askText(context, 'BOARD NAME', b.prefs.boardName);
            if (name != null && name.isNotEmpty) b.prefs.boardName = name;
          }),
          TronIconButton(path: gearPath, size: 0.9 * u, onTap: () => _settings(context)),
        ]),
      ),
    );
  }

  void _settings(BuildContext context) {
    showModalBottomSheet(
      context: context,
      backgroundColor: Neon.panel,
      builder: (ctx) => StatefulBuilder(builder: (ctx, setS) {
        final p = b.prefs;
        return Padding(
          padding: const EdgeInsets.all(16),
          child: Column(mainAxisSize: MainAxisSize.min, crossAxisAlignment: CrossAxisAlignment.start, children: [
            Text('SETTINGS', style: tron(16, Neon.mint)),
            const SizedBox(height: 12),
            Text('Speed dial max: ${p.dialMax} ${b.speedUnit}', style: const TextStyle(color: Neon.text)),
            Slider(value: p.dialMax.toDouble(), min: 20, max: 80, divisions: 12, activeColor: Neon.green,
                onChanged: (v) => setS(() => p.dialMax = v.round())),
            SwitchListTile(
              value: p.metric, activeColor: Neon.green, contentPadding: EdgeInsets.zero,
              title: const Text('Metric units (km/h, km)', style: TextStyle(color: Neon.text)),
              onChanged: (v) => setS(() => p.metric = v),
            ),
            const SizedBox(height: 8),
            Row(children: [
              Pill('RESET PEAKS', on: false, onTap: () { b.resetPeaks(); Navigator.pop(ctx); }),
              const SizedBox(width: 8),
              Pill('DISCONNECT', on: false, onTap: () { Navigator.pop(ctx); widget.onDisconnect(); }),
            ]),
            const SizedBox(height: 8),
          ]),
        );
      }),
    );
  }

  Widget _speedo(double u) {
    Widget peak(String label, String value, bool warn) => GestureDetector(
          onLongPress: () {
            b.resetPeaks();
            ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Top speed / max duty reset')));
          },
          child: Container(
            width: 2.0 * u,
            height: 0.95 * u,
            decoration: BoxDecoration(
              color: Neon.panel,
              borderRadius: BorderRadius.circular(0.12 * u),
              border: Border.all(color: warn ? Neon.red : Neon.purple, width: 2),
              boxShadow: [BoxShadow(color: (warn ? Neon.red : Neon.purple).withOpacity(0.45), blurRadius: 8)],
            ),
            child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
              FittedBox(child: Text(value, style: tron(0.4 * u, warn ? Neon.red : Neon.green))),
              Text(label, style: tron(0.18 * u, Neon.lav)),
            ]),
          ),
        );
    final pk = b.prefs;
    return Stack(children: [
      Positioned.fill(child: CustomPaint(painter: SpeedoPainter(b, u))),
      Positioned(left: 0.1 * u, top: 0, child: peak('TOP SPEED', '${fx1(pk.peakSpeed * b.imperial)}${b.speedUnit}', false)),
      Positioned(right: 0.1 * u, top: 0, child: peak('MAX DUTY', '${fx1(pk.peakDuty * 100)}%', pk.peakDuty >= 0.9)),
    ]);
  }

  Widget _hudRow(double u, double halfW, double footS, double pitchS, double rowH) {
    final sidePad = (halfW - 2 * footS) / 4;
    final pTop = (rowH - pitchS) / 2;
    final tw = 1.4 * u;
    final battA = b.v('motor.batt_current');
    Widget stat(String value, String label, Color vc) => Column(mainAxisSize: MainAxisSize.min, children: [
          FittedBox(child: Text(value, style: tron(0.4 * u, vc))),
          NeonLabel(label, size: 0.27 * u),
        ]);
    return Stack(clipBehavior: Clip.none, children: [
      Positioned(left: sidePad, top: 0, width: footS, height: footS, child: CustomPaint(painter: FootpadPainter(b))),
      Positioned(left: halfW - sidePad - footS, top: 0, width: footS, height: footS, child: CustomPaint(painter: RollPainter(b))),
      Positioned(left: 0, bottom: 0.1 * u, width: 1.3 * u, child: stat(fx1(b.whPerDist), 'WH/${b.distUnit.toUpperCase()}', Neon.lavLight)),
      Positioned(left: 1.3 * u, bottom: 0, width: halfW - 1.3 * u, child: Column(mainAxisSize: MainAxisSize.min, children: [
        Text.rich(TextSpan(children: [
          TextSpan(text: fx1(battA), style: tron(0.4 * u, Neon.lavLight)),
          TextSpan(text: ' A', style: tron(0.27 * u, Neon.lav)),
        ])),
        SizedBox(height: 0.06 * u),
        SizedBox(width: 2.8 * u, height: 0.14 * u, child: CustomPaint(painter: _BarPainter(battA.isFinite ? battA / 60 : 0))),
        SizedBox(height: 0.06 * u),
        NeonLabel('BATTERY AMPS', size: 0.27 * u),
      ])),
      Positioned(left: halfW + (halfW - pitchS) / 2, top: pTop - 0.6 * u, width: pitchS, height: pitchS,
          child: CustomPaint(painter: SidePainter(b, u))),
      Positioned(left: halfW, top: pTop + pitchS - 0.95 * u, width: tw, child: stat('${fx1(b.v('imu.pitch'))}°', 'PITCH', Neon.lavLight)),
      Positioned(left: halfW * 1.5 + 0.05 * u, top: pTop + pitchS - 0.95 * u, width: tw, child: stat('${fx1(b.v('imu.balance_pitch'))}°', 'BALANCE', Neon.purple)),
      if (b.running)
        Positioned(left: halfW * 2 - tw, top: pTop, width: tw, child: Column(children: [
          Text('setpoint', style: TextStyle(color: Neon.text, fontSize: 0.3 * u)),
          Text('${fx1(b.v('setpoint'))}°', style: TextStyle(color: Neon.orange, fontSize: 0.35 * u)),
        ])),
    ]);
  }

  Widget _midRow(double u) {
    final mt = b.v('motor.motor_temp'), et = b.v('motor.mosfet_temp');
    final mWarn = mt.isFinite && mt > 85, eWarn = et.isFinite && et > 75;
    return Stack(children: [
      Positioned(left: 0, top: 0, bottom: 0, width: 1.7 * u, child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
        FittedBox(child: Text('${fx1(mt)}°C', style: tron(0.4 * u, mWarn ? Neon.red : Neon.lavLight))),
        SizedBox(width: 0.75 * u, height: 0.55 * u, child: CustomPaint(painter: IconPainter(enginePath, mWarn ? Neon.red : Neon.lav, box: 48, lineWidth: 3))),
      ])),
      Positioned(right: 0, top: 0, bottom: 0, width: 1.7 * u, child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
        FittedBox(child: Text('${fx1(et)}°C', style: tron(0.4 * u, eWarn ? Neon.red : Neon.lavLight))),
        NeonLabel('ESC', size: 0.3 * u),
      ])),
      Positioned(left: 1.9 * u, right: 1.9 * u, top: 0, bottom: 0, child: Center(child: FittedBox(
        child: Text(_status(), style: tron(0.42 * u, Neon.mint, glow: glowOf(Neon.green, 8))),
      ))),
    ]);
  }

  String _status() {
    if (b.link == null || !b.linkUp) return 'DISCONNECTED';
    if (b.packageMissing) return 'NO PACKAGE';
    if (b.comms != Comms.ready || b.pkgState == Rf.sConnecting) return 'CONNECTING...';
    if (b.pkgState == Rf.sDisconnected) return 'NO DATA';
    if (b.pkgMode == Rf.mFlywheel) return 'FLYWHEEL';
    if (b.pkgMode == Rf.mHandtest) return 'HANDTEST';
    if (b.running) {
      switch (b.sat) {
        case 5: return 'PUSHBACK: SPEED';
        case 6: return 'PUSHBACK: DUTY';
        case 7: return 'PUSHBACK: ERROR';
        case 10: return 'PUSHBACK: HV';
        case 11: return 'PUSHBACK: LV';
        case 12: return 'PUSHBACK: TEMP';
      }
      return 'RUNNING';
    }
    if (b.pkgState == Rf.sDisabled) return 'DISABLED';
    if (b.pkgState == Rf.sStartup) return 'STARTUP';
    return 'READY';
  }

  Widget _tiltRow(double u) {
    final on = b.tilt != 0;
    final deg = b.tilt * math.min(4.0, b.tiltAngleLimit);
    return Padding(
      padding: EdgeInsets.symmetric(horizontal: 0.2 * u),
      child: Row(children: [
        SizedBox(width: 1.3 * u, child: NeonLabel('TILT', size: 0.36 * u, color: on ? Neon.green : Neon.lav, align: TextAlign.left)),
        Expanded(child: SpringSlider(
          value: b.tilt,
          knob: 0.6 * u,
          enabled: b.hornAvailable,
          onChanged: b.setTilt,
          onHeld: b.setTiltHeld,
        )),
        SizedBox(width: 1.5 * u, child: Text('${deg > 0 ? '+' : ''}${deg.toStringAsFixed(1)}°', textAlign: TextAlign.right,
            style: tron(0.36 * u, on ? Neon.green : Neon.lavLight))),
      ]),
    );
  }
}

class _BarPainter extends CustomPainter {
  _BarPainter(this.f);
  final double f;

  @override
  void paint(Canvas c, Size s) {
    c.drawRect(Offset.zero & s, Paint()..color = Neon.lightBg);
    final w = f.clamp(-1.0, 1.0) * s.width / 2;
    c.drawRect(Rect.fromLTRB(math.min(s.width / 2, s.width / 2 + w), 0, math.max(s.width / 2, s.width / 2 + w), s.height), Paint()..color = Neon.green);
    c.drawRect(Rect.fromLTWH(s.width / 2 - 0.5, 0, 1, s.height), Paint()..color = Neon.mint);
  }

  @override
  bool shouldRepaint(covariant _BarPainter old) => old.f != f;
}

class GridPainter extends CustomPainter {
  GridPainter(this.step);
  final double step;

  @override
  void paint(Canvas c, Size s) {
    final p = Paint()..color = Neon.grid..strokeWidth = 1;
    for (var x = 0.0; x < s.width; x += step) {
      c.drawLine(Offset(x, 0), Offset(x, s.height), p);
    }
    for (var y = 0.0; y < s.height; y += step) {
      c.drawLine(Offset(0, y), Offset(s.width, y), p);
    }
  }

  @override
  bool shouldRepaint(covariant GridPainter old) => old.step != step;
}

class HornButton extends StatelessWidget {
  const HornButton({super.key, required this.board, required this.u});
  final Board board;
  final double u;

  @override
  Widget build(BuildContext context) {
    final b = board;
    final honk = b.honking;
    final label = honk
        ? 'HONK'
        : b.hornBlocked || (b.hornPressed && b.hornBudget <= 0)
            ? 'WAIT'
            : b.hornExpired
                ? 'MAX'
                : 'HORN';
    return Listener(
      onPointerDown: (_) => b.hornDown(),
      onPointerUp: (_) => b.hornUp(),
      onPointerCancel: (_) => b.hornUp(),
      child: Opacity(
        opacity: b.hornAvailable && b.hornBudget >= 0.2 ? 1 : 0.4,
        child: Container(
          decoration: BoxDecoration(
            color: honk ? Neon.purple : const Color(0xFF140A26),
            borderRadius: BorderRadius.circular(0.3 * u),
            border: Border.all(color: Neon.purple, width: 3),
            boxShadow: [BoxShadow(color: Neon.purple.withOpacity(honk ? 0.9 : 0.3), blurRadius: 0.25 * u * 2)],
          ),
          child: Stack(children: [
            Center(child: Text(label, style: tron(0.75 * u, honk ? Neon.bg : Neon.purple, spacing: 0.2 * u))),
            Positioned(
              left: 0.3 * u, bottom: 0.12 * u,
              child: Opacity(
                opacity: b.hornBudget < Board.hornBudgetMax ? 0.9 : 0.35,
                child: Container(
                  width: (10 * u - 1.2 * u) * b.hornBudget / Board.hornBudgetMax,
                  height: 0.07 * u,
                  decoration: BoxDecoration(color: b.hornBudget >= 1 ? Neon.green : Neon.orange, borderRadius: BorderRadius.circular(2)),
                ),
              ),
            ),
          ]),
        ),
      ),
    );
  }
}
