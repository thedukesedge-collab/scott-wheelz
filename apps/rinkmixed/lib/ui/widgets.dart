// Small shared widgets: neon texts, spring-back sliders, neon buttons.
import 'dart:async';

import 'package:flutter/material.dart';

import '../theme.dart';
import 'painters.dart';

class NeonLabel extends StatelessWidget {
  const NeonLabel(this.text, {super.key, required this.size, this.color = Neon.lav, this.align = TextAlign.center});
  final String text;
  final double size;
  final Color color;
  final TextAlign align;

  @override
  Widget build(BuildContext context) => FittedBox(
        fit: BoxFit.scaleDown,
        alignment: align == TextAlign.left ? Alignment.centerLeft : Alignment.center,
        child: Text(text, textAlign: align, maxLines: 1, softWrap: false, style: neonText(size, color)),
      );
}

class NeonIconButton extends StatefulWidget {
  const NeonIconButton({super.key, required this.path, required this.size, required this.onTap});
  final Path path;
  final double size;
  final VoidCallback onTap;

  @override
  State<NeonIconButton> createState() => _NeonIconButtonState();
}

class _NeonIconButtonState extends State<NeonIconButton> {
  bool down = false;

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onTapDown: (_) => setState(() => down = true),
      onTapCancel: () => setState(() => down = false),
      onTapUp: (_) {
        setState(() => down = false);
        widget.onTap();
      },
      child: Container(
        width: widget.size,
        height: widget.size,
        margin: EdgeInsets.only(left: widget.size * 0.08),
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          color: down ? Neon.purple : Neon.bg,
          border: Border.all(color: Neon.purple, width: 2),
          boxShadow: [BoxShadow(color: Neon.purple.withOpacity(down ? 0.9 : 0.45), blurRadius: 8)],
        ),
        child: Padding(
          padding: EdgeInsets.all(widget.size * 0.22),
          child: CustomPaint(painter: IconPainter(widget.path, down ? Neon.bg : Neon.green, lineWidth: 3.0)),
        ),
      ),
    );
  }
}

/// A slider that springs back to 0 on release (TILT, Move).
class SpringSlider extends StatefulWidget {
  const SpringSlider({
    super.key,
    required this.value,
    required this.onChanged,
    this.onHeld,
    this.vertical = false,
    this.enabled = true,
    required this.knob,
  });

  final double value;
  final ValueChanged<double> onChanged;
  final ValueChanged<bool>? onHeld;
  final bool vertical, enabled;
  final double knob;

  @override
  State<SpringSlider> createState() => _SpringSliderState();
}

class _SpringSliderState extends State<SpringSlider> {
  Timer? _spring;
  bool held = false;

  void _set(Offset local, Size size) {
    final f = widget.vertical ? 1 - local.dy / size.height : local.dx / size.width;
    widget.onChanged((f * 2 - 1).clamp(-1.0, 1.0));
  }

  void _press(bool on) {
    held = on;
    widget.onHeld?.call(on);
    _spring?.cancel();
    if (!on) {
      _spring = Timer.periodic(const Duration(milliseconds: 10), (t) {
        final v = widget.value;
        if (v == 0) {
          t.cancel();
          return;
        }
        widget.onChanged(v.abs() < 0.05 ? 0 : v - 0.05 * v.sign);
      });
    }
    setState(() {});
  }

  @override
  void dispose() {
    _spring?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(builder: (context, c) {
      final size = Size(c.maxWidth, c.maxHeight);
      final v = widget.value;
      final f = (v + 1) / 2;
      final knobPos = widget.vertical ? Offset(size.width / 2, (1 - f) * size.height) : Offset(f * size.width, size.height / 2);
      return Opacity(
        opacity: widget.enabled ? 1 : 0.4,
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onPanDown: widget.enabled ? (d) { _press(true); _set(d.localPosition, size); } : null,
          onPanUpdate: widget.enabled ? (d) => _set(d.localPosition, size) : null,
          onPanEnd: widget.enabled ? (_) => _press(false) : null,
          onPanCancel: widget.enabled ? () => _press(false) : null,
          child: CustomPaint(
            size: size,
            painter: _SliderPainter(v, knobPos, widget.knob, widget.vertical, held),
          ),
        ),
      );
    });
  }
}

class _SliderPainter extends CustomPainter {
  _SliderPainter(this.v, this.knob, this.knobSize, this.vertical, this.held);
  final double v, knobSize;
  final Offset knob;
  final bool vertical, held;

  @override
  void paint(Canvas c, Size s) {
    final track = vertical
        ? RRect.fromRectAndRadius(Rect.fromCenter(center: Offset(s.width / 2, s.height / 2), width: 6, height: s.height), const Radius.circular(3))
        : RRect.fromRectAndRadius(Rect.fromCenter(center: Offset(s.width / 2, s.height / 2), width: s.width, height: 6), const Radius.circular(3));
    c.drawRRect(track, Paint()..color = const Color(0xFF1D1238));
    c.drawRRect(track, Paint()..style = PaintingStyle.stroke..strokeWidth = 2..color = Neon.purple);
    final mid = vertical ? Offset(s.width / 2, s.height / 2) : Offset(s.width / 2, s.height / 2);
    c.drawLine(mid, knob, Paint()..color = Neon.green..strokeWidth = 4..strokeCap = StrokeCap.round);
    final r = knobSize / 2;
    c.drawCircle(knob, r, Paint()..color = held ? Neon.green : Neon.bg);
    c.drawCircle(knob, r, Paint()..style = PaintingStyle.stroke..strokeWidth = 3..color = Neon.green);
  }

  @override
  bool shouldRepaint(covariant CustomPainter old) => true;
}

/// Neon pill buttons used in Board Style etc.
class Pill extends StatelessWidget {
  const Pill(this.text, {super.key, required this.on, required this.onTap});
  final String text;
  final bool on;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) => GestureDetector(
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 7),
          decoration: BoxDecoration(
            color: on ? Neon.green : Colors.transparent,
            border: Border.all(color: on ? Neon.green : Neon.purple, width: 2),
            borderRadius: BorderRadius.circular(99),
          ),
          child: Text(text, style: neonText(11, on ? Neon.bg : Neon.purple)),
        ),
      );
}

Future<String?> askText(BuildContext context, String title, String initial) {
  final ctl = TextEditingController(text: initial);
  return showDialog<String>(
    context: context,
    builder: (ctx) => AlertDialog(
      backgroundColor: Neon.panel,
      shape: RoundedRectangleBorder(side: const BorderSide(color: Neon.purple, width: 2), borderRadius: BorderRadius.circular(10)),
      title: Text(title, style: neonText(15, Neon.mint)),
      content: TextField(controller: ctl, autofocus: true, maxLength: 20, style: const TextStyle(color: Neon.mint)),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx), child: Text('CANCEL', style: neonText(11, Neon.lav))),
        TextButton(onPressed: () => Navigator.pop(ctx, ctl.text.trim()), child: Text('SAVE', style: neonText(11, Neon.green))),
      ],
    ),
  );
}
