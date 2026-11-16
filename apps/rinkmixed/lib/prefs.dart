// App settings kept on the phone.
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

class Prefs extends ChangeNotifier {
  Prefs._(this._sp);

  final SharedPreferences _sp;

  static Future<Prefs> load() async => Prefs._(await SharedPreferences.getInstance());

  T _get<T>(String k, T def) {
    final v = _sp.get(k);
    return v is T ? v : def;
  }

  void _set(String k, Object v) {
    if (v is bool) _sp.setBool(k, v);
    if (v is int) _sp.setInt(k, v);
    if (v is double) _sp.setDouble(k, v);
    if (v is String) _sp.setString(k, v);
    notifyListeners();
  }

  String get boardName => _get('boardName', 'Growler');
  set boardName(String v) => _set('boardName', v);

  int get rail => _get('rail', 0);  // 0 straight, 1 WTF, 2 Atom
  set rail(int v) => _set('rail', v);

  int get tire => _get('tire', 0);  // 0 treaded, 1 slick
  set tire(int v) => _set('tire', v);

  bool get flip => _get('flip', false);
  set flip(bool v) => _set('flip', v);

  int get dialMax => _get('dialMax', 40);
  set dialMax(int v) => _set('dialMax', v);

  bool get metric => _get('metric', false);
  set metric(bool v) => _set('metric', v);

  double get sheet => _get('sheet', 0.0);
  set sheet(double v) => _set('sheet', v);

  int get tab => _get('tab', 0);
  set tab(int v) => _set('tab', v);

  bool get dataGraph => _get('dataGraph', true);
  set dataGraph(bool v) => _set('dataGraph', v);

  String get lastDevice => _get('lastDevice', '');
  set lastDevice(String v) => _set('lastDevice', v);

  // ride peaks (raw km/h and 0..1), kept until the trip counter resets
  double get peakSpeed => _get('peakSpeed', 0.0);
  set peakSpeed(double v) => _set('peakSpeed', v);
  double get peakDuty => _get('peakDuty', 0.0);
  set peakDuty(double v) => _set('peakDuty', v);
  double get peakTach => _get('peakTach', -1.0);
  set peakTach(double v) => _set('peakTach', v);

  Map<String, dynamic> _json(String k) {
    try {
      final m = jsonDecode(_get('${k}Json', '{}'));
      return m is Map<String, dynamic> ? m : {};
    } catch (_) {
      return {};
    }
  }

  Map<String, dynamic> get tuneColors => _json('tuneColors');
  void setTuneColor(int slot, int idx) {
    final m = tuneColors..['$slot'] = idx;
    _set('tuneColorsJson', jsonEncode(m));
  }

  Map<String, dynamic> get plotVisible => _json('plotVisible');
  void setPlotVisible(String id, bool on) {
    final m = plotVisible..[id] = on;
    _set('plotVisibleJson', jsonEncode(m));
  }
}
