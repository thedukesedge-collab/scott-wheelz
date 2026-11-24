// Builds www/ for the app: the simulator page (one source for the simulator and
// the app) with the app layer (www/app.js, bundled by esbuild) added before
// the simulator's own script, so window.RF_LINK exists when it starts.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, '..');
const sim = readFileSync(join(root, '../../tools/simulator/index.html'), 'utf8');

const marker = '<script>';
const at = sim.lastIndexOf(marker);
if (at < 0) throw new Error('simulator script not found');

const app = '<script>window.RF_APP = true;</script>\n<script src="app.js"></script>\n';
// iPhone: draw under the status bar / home bar, then keep the UI inside the
// safe area (battery bar clear of the camera, swipe-up panel clear of the home bar)
const safe = '<style>#phone { margin-top: env(safe-area-inset-top, 0px); ' +
  'height: calc(100% - env(safe-area-inset-top, 0px) - env(safe-area-inset-bottom, 0px)); }</style>\n';
let html = sim.slice(0, at) + safe + app + sim.slice(at);
const vp = 'user-scalable=no">';
if (!html.includes(vp)) throw new Error('viewport meta not found');
html = html.replace(vp, 'user-scalable=no, viewport-fit=cover">');

// App-only naming: the app is FloPilot ("Flo" neon purple, "Pilot" neon
// green). The VESC package keeps its own name; in the app it is "the FloPilot
// package". Credits to Refloat / Float stay as they are.
const BRAND_OLD = '<span style="color:#b14cff">RE</span><span style="color:#ead6ff">FLOAT</span><span style="color:#7a3fc0"> — </span><span style="color:#39ff7a">GLOW</span>';
const BRAND_NEW = '<span style="color:#b14cff">Flo</span><span style="color:#39ff7a">Pilot</span>';
const before = html;
html = html.split(BRAND_OLD).join(BRAND_NEW)
  .replace('<title>Refloat Glow Simulator</title>', '<title>FloPilot</title>')
  .replace(/"REFLOAT GLOW"/g, '"FLOPILOT"')
  .replace('New Refloat Glow releases show up here too.', 'New FloPilot package releases: gear icon › PACKAGE.')
  .replace(/Refloat Glow/g, 'FloPilot');
if (html === before || /Refloat Glow|REFLOAT GLOW|>GLOW</.test(html)) throw new Error('app naming: something was not renamed');

mkdirSync(join(root, 'www'), { recursive: true });
writeFileSync(join(root, 'www/index.html'), html);
console.log('www/index.html written (' + html.length + ' bytes)');
