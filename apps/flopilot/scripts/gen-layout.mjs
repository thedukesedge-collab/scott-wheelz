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
const xml = readFileSync(join(here, '../../../src/conf/settings.xml'), 'utf8');

const ser = [...xml.matchAll(/<ser>([^<]+)<\/ser>/g)].map(m => m[1]);
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

const fields = ser.map(param);

function crc32c(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) {
    crc ^= b;
    for (let j = 0; j < 8; j++) crc = (crc >>> 1) ^ (0x82f63b78 & -(crc & 1));
  }
  return (~crc) >>> 0;
}
let sig = '';
for (const f of fields) sig += f.n + f.t + f.x + (f.e || []).join('');
const signature = crc32c(new TextEncoder().encode(sig));

// every field keeps what reading / writing needs (type, scale, limits); only
// the LED fields keep their long names / enum lists (the rest aren't shown)
const keep = f => f.n.startsWith('leds.');
const slim = f => { const o = { n: f.n, t: f.t, x: f.x }; for (const k of ['s', 'min', 'max']) if (f[k] !== undefined) o[k] = f[k]; if (f.e) o.e = f.e.map(() => ''); return o; };
const out = {
  source: 'Refloat 1.2.3 src/conf/settings.xml',
  signature,
  fields: fields.map(f => keep(f) ? f : slim(f)),
};
writeFileSync(join(here, '../src/refloat-layout.json'), JSON.stringify(out));
console.log('refloat-layout.json: ' + fields.length + ' fields, signature 0x' + signature.toString(16));
