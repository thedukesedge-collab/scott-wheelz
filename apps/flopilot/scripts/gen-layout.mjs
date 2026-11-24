// SPDX-License-Identifier: GPL-3.0-or-later
// Builds src/refloat-layout.json from Refloat's settings.xml (src/conf, the
// same as upstream Refloat 1.2.3): the serialization order with each field's
// type, so the app can read the board's Refloat config and patch only the
// fields it changes. Also computes VESC Tool's config signature (crc32c), which
// the board sends first: the app only writes when it matches.
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
let xml = '';
const unesc = s => s.replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&quot;/g, '"').replace(/&apos;/g, "'").replace(/&amp;/g, '&');

function param(name) {
  const open = '<' + name + '>', close = '</' + name + '>';
  // the parameter's own block: the first one that has a <type> (names are unique)
  let at = 0;
  for (;;) {
    const i = xml.indexOf(open, at);
    if (i < 0) throw new Error('param not found: ' + name);
    const j = xml.indexOf(close, i);
    const body = xml.slice(i + open.length, j);
    if (body.includes('<type>')) {
      const get = (tag, d) => { const m = body.match(new RegExp('<' + tag + '>([^<]*)</' + tag + '>')); return m ? m[1] : d; };
      const p = { n: name, t: +get('type', 0), x: +get('vTx', 0) };
      const sc = get('vTxDoubleScale', null);
      if (sc !== null) p.s = +sc;
      const en = [...body.matchAll(/<enumNames>([^<]*)<\/enumNames>/g)].map(m => unesc(m[1]));
      if (en.length) p.e = en;
      const ln = get('longName', null);
      if (ln !== null) p.l = unesc(ln);
      for (const [k, tag] of [['min', 'minDouble'], ['max', 'maxDouble'], ['min', 'minInt'], ['max', 'maxInt'], ['es', 'editorScale']]) {
        const v = get(tag, null);
        if (v !== null && p[k] === undefined) p[k] = +v;
      }
      return p;
    }
    at = j + close.length;
  }
}


function crc32c(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) {
    crc ^= b;
    for (let j = 0; j < 8; j++) crc = (crc >>> 1) ^ (0x82f63b78 & -(crc & 1));
  }
  return (~crc) >>> 0;
}
function build(xmlPath, outName, keep, source) {
  xml = readFileSync(join(here, xmlPath), 'utf8');
  const fields = [...xml.matchAll(/<ser>([^<]+)<\/ser>/g)].map(m => param(m[1]));
  let sig = '';
  for (const f of fields) sig += f.n + f.t + f.x + (f.e || []).join('');
  const signature = crc32c(new TextEncoder().encode(sig));
  const slim = f => { const o = { n: f.n, t: f.t, x: f.x }; for (const k of ['s', 'min', 'max']) if (f[k] !== undefined) o[k] = f[k]; if (f.e) o.e = f.e.map(() => ''); return o; };
  writeFileSync(join(here, '../src/' + outName), JSON.stringify({ source, signature, fields: fields.map(f => keep(f) ? f : slim(f)) }));
  console.log(outName + ': ' + fields.length + ' fields, signature 0x' + signature.toString(16));
}
// Refloat's own config (same as stock 1.2.3): LED fields keep names / enum lists
build('../../../src/conf/settings.xml', 'refloat-layout.json', f => f.n.startsWith('leds.'), 'Refloat 1.2.3 src/conf/settings.xml');
// VESC motor config (Max ERPM, backups), from VESC Tool's own layouts
// (vendor/vesc_tool, GPL-3.0, github.com/vedderb/vesc_tool res/config); the
// signatures match the firmware's MCCONF_SIGNATURE (6.05: 1065524471, 6.06: 788332866)
build('../vendor/vesc_tool/6.05/parameters_mcconf.xml', 'mcconf-6.05.json', () => false, 'VESC Tool res/config/6.05/parameters_mcconf.xml');
build('../vendor/vesc_tool/6.06/parameters_mcconf.xml', 'mcconf-6.06.json', () => false, 'VESC Tool res/config/6.06/parameters_mcconf.xml');
