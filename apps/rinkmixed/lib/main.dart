import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'prefs.dart';
import 'refloat/board.dart';
import 'theme.dart';
import 'ui/dashboard.dart';
import 'ui/scan_screen.dart';
import 'vesc/link.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations([DeviceOrientation.portraitUp]);
  final prefs = await Prefs.load();
  runApp(RinkMixedApp(prefs: prefs));
}

class RinkMixedApp extends StatefulWidget {
  const RinkMixedApp({super.key, required this.prefs});
  final Prefs prefs;

  @override
  State<RinkMixedApp> createState() => _RinkMixedAppState();
}

class _RinkMixedAppState extends State<RinkMixedApp> {
  late final Board board = Board(widget.prefs);

  Future<void> _onLink(VescLink link) async {
    await board.attach(link);
    setState(() {});
  }

  Future<void> _disconnect() async {
    if (board.link != null && !board.link!.isDemo) {
      widget.prefs.lastDevice = '';  // don't auto-reconnect right away
    }
    await board.detach();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Refloat Glow',
      debugShowCheckedModeBanner: false,
      theme: appTheme(),
      home: board.link == null
          ? ScanScreen(prefs: widget.prefs, onLink: _onLink)
          : Dashboard(board: board, onDisconnect: _disconnect),
    );
  }
}
