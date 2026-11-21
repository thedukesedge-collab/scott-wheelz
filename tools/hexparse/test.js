// SPDX-License-Identifier: GPL-3.0-or-later
// Part of Refloat Glow (formerly Refloat rink-mixed), 2026. See FORK_NOTES.md.
// CI: runs the app's own parseHex()/crc16() (pulled out of ui.qml.in) in node.
const fs = require('fs');
const qml = fs.readFileSync(process.argv[2], 'utf8');
const a = qml.indexOf('        function crc16(b, n) {'), b = qml.indexOf('        function loadHexText(');
eval(qml.slice(a, b));
const dir = process.argv[3];
const full = parseHex(fs.readFileSync(dir + 'ALCM_VP_BL_full.hex', 'utf8'), 'ALCM_VP_BL_full.hex');
const bin = fs.readFileSync(dir + 'ALCM_VP_app.bin');
if (full.error) throw new Error(full.error);
let same = full.size === bin.length;
for (let i = 0; same && i < bin.length; i++) same = full.bytes[i] === bin[i];
// the build tool's CRC of the .bin
let crc = 0; for (const x of bin) { crc ^= x << 8; for (let k = 0; k < 8; k++) crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) & 0xFFFF : (crc << 1) & 0xFFFF; }
console.log('BL_full.hex -> SLCM', full.slcm, 'size', full.size, 'crc', full.crc.toString(16), '| release .bin size', bin.length, 'crc', crc.toString(16), '| identical:', same && crc === full.crc);
const plain = parseHex(fs.readFileSync(dir + 'ALCM_VP.hex', 'utf8'), 'ALCM_VP.hex');
console.log('plain ALCM_VP.hex ->', plain.error);
const broken = fs.readFileSync(dir + 'ALCM_VP_BL_full.hex', 'utf8').replace(/:10060000/, ':10060001');
console.log('corrupted line ->', parseHex(broken, 'x').error);
console.log('not hex ->', parseHex('hello world', 'x').error);
if (!(same && crc === full.crc) || !plain.error) process.exit(1);
