// VESC packet framing (same as VESC Tool / bldc comm/packet.c):
//   short: 0x02, len(1),  payload, crc16(2), 0x03
//   long:  0x03, len(2),  payload, crc16(2), 0x03
//   huge:  0x04, len(3),  payload, crc16(2), 0x03
// CRC is CRC-16/XMODEM (poly 0x1021, init 0) over the payload, big-endian.
import 'dart:typed_data';

int crc16(List<int> data) {
  var crc = 0;
  for (final b in data) {
    crc ^= (b & 0xFF) << 8;
    for (var i = 0; i < 8; i++) {
      crc = (crc & 0x8000) != 0 ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF;
    }
  }
  return crc;
}

Uint8List encodePacket(List<int> payload) {
  final len = payload.length;
  final out = BytesBuilder(copy: false);
  if (len < 256) {
    out.add([2, len]);
  } else if (len < 65536) {
    out.add([3, (len >> 8) & 0xFF, len & 0xFF]);
  } else {
    out.add([4, (len >> 16) & 0xFF, (len >> 8) & 0xFF, len & 0xFF]);
  }
  out.add(payload);
  final crc = crc16(payload);
  out.add([(crc >> 8) & 0xFF, crc & 0xFF, 3]);
  return out.toBytes();
}

/// Reassembles packets from a byte stream (BLE notifications arrive in pieces).
class PacketDecoder {
  PacketDecoder(this.onPacket, {this.maxLen = 8192});

  final void Function(Uint8List payload) onPacket;
  final int maxLen;
  final List<int> _buf = [];

  void add(List<int> data) {
    _buf.addAll(data);
    _process();
  }

  void reset() => _buf.clear();

  void _process() {
    while (_buf.isNotEmpty) {
      final start = _buf[0];
      if (start != 2 && start != 3 && start != 4) {
        _buf.removeAt(0);
        continue;
      }
      final hdr = start == 2 ? 2 : start == 3 ? 3 : 4;
      if (_buf.length < hdr) return;
      int len;
      if (start == 2) {
        len = _buf[1];
      } else if (start == 3) {
        len = (_buf[1] << 8) | _buf[2];
      } else {
        len = (_buf[1] << 16) | (_buf[2] << 8) | _buf[3];
      }
      if (len == 0 || len > maxLen) {
        _buf.removeAt(0);
        continue;
      }
      final total = hdr + len + 3;
      if (_buf.length < total) return;
      final payload = _buf.sublist(hdr, hdr + len);
      final crc = (_buf[hdr + len] << 8) | _buf[hdr + len + 1];
      final end = _buf[hdr + len + 2];
      if (end != 3 || crc != crc16(payload)) {
        _buf.removeAt(0);  // resync on the next possible start byte
        continue;
      }
      _buf.removeRange(0, total);
      onPacket(Uint8List.fromList(payload));
    }
  }
}
