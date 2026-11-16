// Big-endian readers / writers matching the VESC buffer_* helpers.
import 'dart:convert';
import 'dart:math' as math;
import 'dart:typed_data';

/// IEEE-754 half precision -> double (Refloat's realtime data format).
double halfToDouble(int h) {
  final s = (h >> 15) & 1;
  final e = (h >> 10) & 0x1F;
  final m = h & 0x3FF;
  double v;
  if (e == 0) {
    v = m * math.pow(2, -24).toDouble();
  } else if (e == 31) {
    v = m == 0 ? double.infinity : double.nan;
  } else {
    v = (1 + m / 1024.0) * math.pow(2, e - 15).toDouble();
  }
  return s == 1 ? -v : v;
}

/// double -> IEEE-754 half precision (used by the demo board).
int doubleToHalf(double value) {
  final bd = ByteData(4)..setFloat32(0, value);
  final x = bd.getUint32(0);
  final sign = (x >> 16) & 0x8000;
  var exp = ((x >> 23) & 0xFF) - 127 + 15;
  var mant = x & 0x7FFFFF;
  if (exp <= 0) {
    if (exp < -10) return sign;
    mant = (mant | 0x800000) >> (1 - exp);
    return sign | ((mant + 0x1000) >> 13);
  }
  if (exp >= 31) return sign | 0x7BFF;  // clamp to max finite
  final r = sign | (exp << 10) | ((mant + 0x1000) >> 13);
  return r & 0xFFFF;
}

class ByteReader {
  ByteReader(Uint8List data, [this.pos = 0]) : _bd = ByteData.sublistView(data), _data = data;

  final ByteData _bd;
  final Uint8List _data;
  int pos;

  int get remaining => _data.length - pos;
  bool has(int n) => remaining >= n;

  int u8() => _bd.getUint8(pos++);
  int i8() => _bd.getInt8(pos++);
  int u16() { final v = _bd.getUint16(pos); pos += 2; return v; }
  int i16() { final v = _bd.getInt16(pos); pos += 2; return v; }
  int u32() { final v = _bd.getUint32(pos); pos += 4; return v; }
  int i32() { final v = _bd.getInt32(pos); pos += 4; return v; }
  double f32() { final v = _bd.getFloat32(pos); pos += 4; return v; }
  double f16() => halfToDouble(u16());
  double d16(double scale) => i16() / scale;
  double d32(double scale) => i32() / scale;

  /// Refloat style: 1 byte length, then chars.
  String lenString() {
    final n = u8();
    final s = latin1.decode(_data.sublist(pos, pos + n), allowInvalid: true);
    pos += n;
    return s;
  }

  /// VESC style: null-terminated.
  String cString() {
    final start = pos;
    while (pos < _data.length && _data[pos] != 0) {
      pos++;
    }
    final s = utf8.decode(_data.sublist(start, pos), allowMalformed: true);
    if (pos < _data.length) pos++;
    return s;
  }

  Uint8List rest() {
    final r = Uint8List.sublistView(_data, pos);
    pos = _data.length;
    return r;
  }
}

class ByteWriter {
  final BytesBuilder _b = BytesBuilder();

  void u8(int v) => _b.addByte(v & 0xFF);
  void u16(int v) => _b.add([(v >> 8) & 0xFF, v & 0xFF]);
  void i16(int v) => u16(v & 0xFFFF);
  void u32(int v) => _b.add([(v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF]);
  void i32(int v) => u32(v & 0xFFFFFFFF);
  void f16(double v) => u16(doubleToHalf(v));
  void d16(double v, double scale) => i16((v * scale).round());
  void d32(double v, double scale) => i32((v * scale).round());
  void bytes(List<int> v) => _b.add(v);
  void cString(String s) { _b.add(utf8.encode(s)); _b.addByte(0); }
  void lenString(String s) { final e = latin1.encode(s); u8(e.length); _b.add(e); }

  Uint8List toBytes() => _b.toBytes();
}
