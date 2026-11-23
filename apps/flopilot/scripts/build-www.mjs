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
const html = sim.slice(0, at) + app + sim.slice(at);

mkdirSync(join(root, 'www'), { recursive: true });
writeFileSync(join(root, 'www/index.html'), html);
console.log('www/index.html written (' + html.length + ' bytes)');
