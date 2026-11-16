// Pull-up panel: TUNES / CONTROL / DATA / BOARD STYLE.
import 'package:flutter/material.dart';

import '../refloat/board.dart';
import '../theme.dart';
import 'dashboard.dart';
import 'painters.dart';
import 'widgets.dart';

const appVersion = '0.1.0';

class BottomPanel extends StatefulWidget {
  const BottomPanel({
    super.key,
    required this.board,
    required this.u,
    required this.fullHeight,
    required this.handleHeight,
    required this.onDrag,
    required this.onRelease,
  });

  final Board board;
  final double u, fullHeight, handleHeight;
  final ValueChanged<double> onDrag;
  final ValueChanged<bool> onRelease;

  @override
  State<BottomPanel> createState() => _BottomPanelState();
}

class _BottomPanelState extends State<BottomPanel> {
  double _moved = 0;
  bool _dragging = false;

  Board get b => widget.board;
  double get u => widget.u;

  static const tabs = ['TUNES', 'CONTROL', 'DATA', 'BOARD STYLE'];

  @override
  Widget build(BuildContext context) {
    final tab = b.prefs.tab;
    return Container(
      decoration: BoxDecoration(
        color: Neon.bg,
        border: Border.all(color: Neon.purple),
        borderRadius: BorderRadius.circular(8),
      ),
      clipBehavior: Clip.antiAlias,
      child: OverflowBox(
        alignment: Alignment.topCenter,
        minHeight: widget.fullHeight - 2,
        maxHeight: widget.fullHeight - 2,
        child: Stack(children: [
          Positioned.fill(child: CustomPaint(painter: GridPainter(12.0 > 0.5 * u ? 12.0 : 0.5 * u))),
          Column(children: [
            GestureDetector(
              behavior: HitTestBehavior.opaque,
              onVerticalDragStart: (_) => setState(() { _dragging = true; _moved = 0; }),
              onVerticalDragUpdate: (d) { _moved += d.delta.dy.abs(); widget.onDrag(d.delta.dy); },
              onVerticalDragEnd: (_) { setState(() => _dragging = false); widget.onRelease(_moved > 0.15 * u); },
              onTap: () => widget.onRelease(false),
              child: SizedBox(
                height: widget.handleHeight,
                child: Center(child: Container(
                  width: 1.4 * u, height: 0.12 * u,
                  decoration: BoxDecoration(color: _dragging ? Neon.green : Neon.purple, borderRadius: BorderRadius.circular(4)),
                )),
              ),
            ),
            Container(
              height: 0.85 * u,
              decoration: const BoxDecoration(
                color: Neon.panel,
                border: Border.symmetric(horizontal: BorderSide(color: Color(0x99B14CFF))),
              ),
              child: Row(children: [
                for (var i = 0; i < tabs.length; i++)
                  Expanded(child: GestureDetector(
                    onTap: () => setState(() => b.prefs.tab = i),
                    child: Container(
                      decoration: BoxDecoration(
                        color: tab == i ? const Color(0xFF0E2A1A) : Colors.transparent,
                        border: Border(
                          right: i < tabs.length - 1 ? const BorderSide(color: Color(0x80B14CFF)) : BorderSide.none,
                          bottom: tab == i ? const BorderSide(color: Neon.green, width: 3) : BorderSide.none,
                        ),
                      ),
                      alignment: Alignment.center,
                      child: FittedBox(child: Padding(
                        padding: const EdgeInsets.symmetric(horizontal: 4),
                        child: Text(tabs[i], style: tron(0.27 * u, tab == i ? Neon.green : Neon.lav)),
                      )),
                    ),
                  )),
              ]),
            ),
            Expanded(child: AnimatedBuilder(animation: b, builder: (context, _) => _page(context, tab))),
            Padding(
              padding: EdgeInsets.symmetric(vertical: 0.05 * u),
              child: Text.rich(TextSpan(style: tron(0.24 * u, Neon.dim, spacing: 0.03 * u), children: const [
                TextSpan(text: 'RE:', style: TextStyle(color: Neon.purple)),
                TextSpan(text: 'FLOAT', style: TextStyle(color: Neon.lavLight)),
                TextSpan(text: ' — ', style: TextStyle(color: Color(0xFF7A3FC0))),
                TextSpan(text: 'RINK-MIXED', style: TextStyle(color: Neon.green)),
                TextSpan(text: '  app $appVersion'),
              ])),
            ),
          ]),
        ]),
      ),
    );
  }

  Widget _page(BuildContext context, int tab) {
    switch (tab) {
      case 0:
        return TunesPage(board: b, u: u);
      case 1:
        return _control();
      case 2:
        return DataPage(board: b, u: u);
      default:
        return _style();
    }
  }

  Widget _control() {
    final fly = b.pkgMode == Rf.mFlywheel;
    final canFly = fly || (b.pkgState == Rf.sReady && b.fpState == Rf.fsNone);
    return ListView(padding: const EdgeInsets.all(10), children: [
      Center(child: Text('Move', style: const TextStyle(color: Neon.mint, fontSize: 18))),
      const SizedBox(height: 6),
      Center(child: NeonLabel('FWD', size: 0.3 * u, color: b.move > 0 ? Neon.green : Neon.lav)),
      Center(child: SizedBox(
        width: 1.2 * u, height: 3.2 * u,
        child: SpringSlider(value: b.move, vertical: true, knob: 0.65 * u, enabled: !b.running && b.connected, onChanged: b.setMove),
      )),
      Center(child: NeonLabel('REV', size: 0.3 * u, color: b.move < 0 ? Neon.green : Neon.lav)),
      const SizedBox(height: 14),
      Center(child: NeonLabel('MAGIC FLYWHEEL', size: 0.4 * u)),
      const SizedBox(height: 6),
      const Text('Before enabling flywheel mode make sure that your board is nose up and perfectly balanced. '
          'To turn it off, you must disengage the board first.', style: TextStyle(color: Neon.mint, fontSize: 12)),
      const SizedBox(height: 10),
      Center(child: Opacity(
        opacity: canFly ? 1 : 0.4,
        child: GestureDetector(
          onTap: canFly ? () => b.setFlywheel(!fly) : null,
          child: Container(
            width: 4.2 * u, height: 1.1 * u,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              color: fly ? const Color(0xFF0E2A1A) : const Color(0xFF140A26),
              borderRadius: BorderRadius.circular(0.3 * u),
              border: Border.all(color: fly ? Neon.green : Neon.purple, width: 3),
              boxShadow: [BoxShadow(color: (fly ? Neon.green : Neon.purple).withOpacity(fly ? 0.8 : 0.3), blurRadius: 14)],
            ),
            child: FittedBox(child: Text(fly ? 'FLYWHEEL OFF' : 'FLYWHEEL ON',
                style: tron(0.36 * u, fly ? Neon.green : Neon.purple, spacing: 0.08 * u))),
          ),
        ),
      )),
    ]);
  }

  Widget _style() {
    final p = b.prefs;
    Widget title(String t) => Padding(
          padding: const EdgeInsets.only(top: 10, bottom: 6),
          child: Center(child: Text(t, style: const TextStyle(color: Neon.mint, fontSize: 18))),
        );
    Widget pills(List<String> items, int sel, ValueChanged<int> set) => Wrap(
          alignment: WrapAlignment.center, spacing: 6, runSpacing: 6,
          children: [for (var i = 0; i < items.length; i++) Pill(items[i], on: sel == i, onTap: () => setState(() => set(i)))],
        );
    return ListView(padding: const EdgeInsets.all(10), children: [
      title('Rails'),
      pills(['STRAIGHT', 'WTF', 'ATOM'], p.rail, (i) => p.rail = i),
      title('Tire'),
      pills(['TREADED', 'SLICK'], p.tire, (i) => p.tire = i),
      title('View'),
      Center(child: Pill('FLIP', on: p.flip, onTap: () => setState(() => p.flip = !p.flip))),
    ]);
  }
}

// ---------------- TUNES ----------------

class TunesPage extends StatelessWidget {
  const TunesPage({super.key, required this.board, required this.u});
  final Board board;
  final double u;

  int _colorIdx(int slot) {
    final v = board.prefs.tuneColors['$slot'];
    return v is int && v >= 0 && v < Neon.palette.length ? v : (slot - 1) % Neon.palette.length;
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.all(10),
      child: Column(children: [
        Expanded(child: GridView.count(
          crossAxisCount: 2, mainAxisSpacing: 12, crossAxisSpacing: 12, childAspectRatio: 2.2,
          children: [
            for (var slot = 1; slot <= 4; slot++)
              GestureDetector(
                onLongPress: () => _colors(context, slot),
                child: Container(
                  alignment: Alignment.center,
                  decoration: BoxDecoration(
                    color: Neon.panel,
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: Neon.palette[_colorIdx(slot)], width: 3),
                    boxShadow: [BoxShadow(color: Neon.palette[_colorIdx(slot)].withOpacity(0.3), blurRadius: 8)],
                  ),
                  child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
                    Text('SLOT $slot', style: tron(0.36 * u, Neon.palette[_colorIdx(slot)], spacing: 0.06 * u)),
                    const Text('hold for color', style: TextStyle(color: Neon.dim, fontSize: 11)),
                  ]),
                ),
              ),
          ],
        )),
        const Text('Saving and applying tunes to the board arrives in the next app update.',
            textAlign: TextAlign.center, style: TextStyle(color: Neon.dim, fontSize: 12)),
      ]),
    );
  }

  void _colors(BuildContext context, int slot) {
    showDialog(
      context: context,
      builder: (ctx) => StatefulBuilder(builder: (ctx, setS) {
        final sel = _colorIdx(slot);
        return AlertDialog(
          backgroundColor: Neon.panel,
          shape: RoundedRectangleBorder(side: const BorderSide(color: Neon.purple, width: 2), borderRadius: BorderRadius.circular(10)),
          title: Text('SLOT $slot COLOR: ${Neon.names[sel].toUpperCase()}', style: tron(14, Neon.mint)),
          content: Wrap(spacing: 10, runSpacing: 10, children: [
            for (var i = 0; i < Neon.palette.length; i++)
              GestureDetector(
                onTap: () => setS(() => board.prefs.setTuneColor(slot, i)),
                child: Container(
                  width: 34, height: 34,
                  decoration: BoxDecoration(
                    shape: BoxShape.circle,
                    color: Neon.panel,
                    border: Border.all(color: Neon.palette[i], width: i == sel ? 4 : 2),
                    boxShadow: i == sel ? [BoxShadow(color: Neon.palette[i], blurRadius: 10)] : null,
                  ),
                  child: Center(child: Container(width: 14, height: 14,
                      decoration: BoxDecoration(shape: BoxShape.circle, color: Neon.palette[i].withOpacity(i == sel ? 1 : 0.6)))),
                ),
              ),
          ]),
          actions: [TextButton(onPressed: () => Navigator.pop(ctx), child: Text('DONE', style: tron(11, Neon.green)))],
        );
      }),
    );
  }
}

// ---------------- DATA ----------------

class DataPage extends StatefulWidget {
  const DataPage({super.key, required this.board, required this.u});
  final Board board;
  final double u;

  @override
  State<DataPage> createState() => _DataPageState();
}

class _DataPageState extends State<DataPage> {
  double? cursor;

  Board get b => widget.board;

  Set<String> get visible {
    final m = b.prefs.plotVisible;
    return {for (final s in plotSeries) if ((m[s.id] as bool?) ?? s.defaultOn) s.id};
  }

  @override
  Widget build(BuildContext context) {
    final graph = b.prefs.dataGraph;
    return Stack(children: [
      Positioned.fill(
        top: 40,
        child: graph ? _graph() : _text(),
      ),
      Positioned(
        right: 8, top: 6,
        child: Container(
          padding: const EdgeInsets.all(2),
          decoration: BoxDecoration(color: Neon.panel, border: Border.all(color: Neon.purple, width: 2), borderRadius: BorderRadius.circular(99)),
          child: Row(children: [
            for (final e in [('GRAPH', true), ('TEXT', false)])
              GestureDetector(
                onTap: () => setState(() => b.prefs.dataGraph = e.$2),
                child: Container(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                  decoration: BoxDecoration(color: graph == e.$2 ? Neon.green : Colors.transparent, borderRadius: BorderRadius.circular(99)),
                  child: Text(e.$1, style: tron(10, graph == e.$2 ? Neon.bg : Neon.lav)),
                ),
              ),
          ]),
        ),
      ),
    ]);
  }

  Widget _graph() {
    final vis = visible;
    final buf = b.plot;
    int idx = buf.count - 1;
    return Padding(
      padding: const EdgeInsets.fromLTRB(6, 0, 6, 4),
      child: Column(children: [
        Expanded(child: LayoutBuilder(builder: (context, c) {
          if (cursor != null && buf.count > 1) {
            final dx = c.maxWidth / (buf.capacity - 1), x0 = c.maxWidth - buf.count * dx;
            idx = ((cursor! * c.maxWidth - x0) / dx).round().clamp(0, buf.count - 1);
          }
          return GestureDetector(
            onPanDown: (d) => setState(() => cursor = (d.localPosition.dx / c.maxWidth).clamp(0.0, 1.0)),
            onPanUpdate: (d) => setState(() => cursor = (d.localPosition.dx / c.maxWidth).clamp(0.0, 1.0)),
            onPanEnd: (_) => setState(() => cursor = null),
            onPanCancel: () => setState(() => cursor = null),
            child: CustomPaint(size: Size(c.maxWidth, c.maxHeight), painter: PlotPainter(b, vis, cursor)),
          );
        })),
        const SizedBox(height: 4),
        Wrap(spacing: 4, runSpacing: 4, children: [
          for (final s in plotSeries)
            GestureDetector(
              onTap: () => setState(() => b.prefs.setPlotVisible(s.id, !vis.contains(s.id))),
              child: Container(
                padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                decoration: BoxDecoration(
                  border: Border.all(color: vis.contains(s.id) ? const Color(0xFF3D2266) : const Color(0xFF2A1A4A)),
                  borderRadius: BorderRadius.circular(4),
                ),
                child: Row(mainAxisSize: MainAxisSize.min, children: [
                  Container(width: 9, height: 9, color: s.color.withOpacity(vis.contains(s.id) ? 1 : 0.35)),
                  const SizedBox(width: 4),
                  Text('${s.name} ', style: TextStyle(fontSize: 10, color: vis.contains(s.id) ? Neon.mint : Neon.dim)),
                  Text(buf.count > 0 ? _fmt(buf.at(s.id, idx) * s.scale) : '', style: mono(10, Neon.text)),
                ]),
              ),
            ),
        ]),
      ]),
    );
  }

  String _fmt(double v) => v.abs() >= 100 ? '${v.round()}' : v.toStringAsFixed(1);

  Widget _text() {
    String f(String id, [double k = 1, int d = 1]) {
      final v = b.v(id) * k;
      return v.isFinite ? v.toStringAsFixed(d) : '-.-';
    }

    final rows = [
      ('Speed', '${f('motor.speed', b.imperial)} ${b.speedUnit}'),
      ('ERPM', f('motor.erpm', 1, 0)),
      ('Duty', '${f('motor.duty_cycle', 100)} %'),
      ('Motor current', '${f('motor.current')} A'),
      ('Battery current', '${f('motor.batt_current')} A'),
      ('Battery voltage', '${f('motor.batt_voltage')} V'),
      ('Motor temp', '${f('motor.motor_temp')} °C'),
      ('ESC temp', '${f('motor.mosfet_temp')} °C'),
      ('Pitch', '${f('imu.pitch')}°'),
      ('Balance pitch', '${f('imu.balance_pitch')}°'),
      ('Roll', '${f('imu.roll')}°'),
      ('Setpoint', '${f('setpoint')}°'),
      ('ADC1', '${f('footpad.adc1', 1, 2)} V'),
      ('ADC2', '${f('footpad.adc2', 1, 2)} V'),
      ('Wheelslip', b.wheelslip ? 'YES' : 'no'),
      ('Torque tilt setpoint', '${f('torque_tilt.setpoint')}°'),
      ('ATR setpoint', '${f('atr.setpoint')}°'),
      ('Requested current', '${f('balance_current')} A'),
    ];
    return ListView(padding: const EdgeInsets.symmetric(horizontal: 10), children: [
      for (final r in rows)
        Container(
          padding: const EdgeInsets.symmetric(vertical: 4),
          decoration: const BoxDecoration(border: Border(bottom: BorderSide(color: Color(0xFF1D1235)))),
          child: Row(children: [
            Expanded(child: Text(r.$1, style: const TextStyle(color: Neon.text, fontSize: 14))),
            Text(r.$2, style: mono(14, Neon.mint)),
          ]),
        ),
    ]);
  }
}
