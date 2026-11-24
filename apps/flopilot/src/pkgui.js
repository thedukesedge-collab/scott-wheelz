// SPDX-License-Identifier: GPL-3.0-or-later
// Gear icon > PACKAGE: what's on the board, the latest FloPilot package release,
// and INSTALL (see pkg.js). Neon Future look, short words.
import { CapacitorHttp } from '@capacitor/core';
import pako from 'pako';
import { REPO, ASSET, PKG_NAME, unpack, check, Installer } from './pkg.js';

const API = 'https://api.github.com/repos/' + REPO + '/releases/latest';
const deflate = b => pako.deflate(b, { level: 9 });
const inflate = b => pako.inflate(b);

let link = null, toast = () => {};
let latest = null;      // { tag, version, notes, url } | { none: true } | { error }
let busy = false;

const css = `
#fpPkg .big { font: 700 14px Orbitron, monospace; color: #39ff7a; text-shadow: 0 0 6px rgba(57,255,122,.5); }
#fpPkg .dim { color: #b98cff; font-size: 12px; }
#fpPkg .notes { max-height: 120px; overflow: auto; white-space: pre-wrap; font-size: 11px; color: #ead6ff; border: 1px solid #3d2266; border-radius: 8px; padding: 8px; margin-top: 6px; }
#fpInst { position: fixed; inset: 0; z-index: 90; background: rgba(4,2,10,.85); display: flex; align-items: center; justify-content: center; }
#fpInst .box { width: min(92vw, 400px); background: #0b0718; border: 2px solid #b14cff; border-radius: 14px; box-shadow: 0 0 22px rgba(177,76,255,.55);
  padding: 16px; color: #ead6ff; font: 13px/1.45 -apple-system, system-ui, sans-serif; }
#fpInst h3 { margin: 0 0 8px; text-align: center; font: 700 15px Orbitron, monospace; letter-spacing: 2px; color: #39ff7a; text-shadow: 0 0 8px rgba(57,255,122,.7); }
#fpInst ul { margin: 6px 0 10px 18px; padding: 0; }
#fpInst .bar { height: 12px; border-radius: 6px; border: 1px solid #7a3fc0; overflow: hidden; margin: 12px 0 6px; }
#fpInst .bar i { display: block; height: 100%; width: 0; background: linear-gradient(90deg, #b14cff, #39ff7a); box-shadow: 0 0 8px #39ff7a; }
#fpInst .row { display: flex; gap: 8px; margin-top: 12px; }
#fpInst .btn { flex: 1; text-align: center; padding: 10px 6px; border-radius: 999px; font: 700 11px Orbitron, monospace; letter-spacing: 1px; border: 2px solid #b14cff; }
#fpInst .btn.go { border-color: #39ff7a; color: #39ff7a; box-shadow: 0 0 10px rgba(57,255,122,.35); }`;

const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const onBoard = () => {
  const b = link.board;
  if (!b || !link.live) return null;
  return b.pkg || null;
};
const isOurs = p => p && p.name === PKG_NAME;
const shown = p => !p ? 'Not connected' : isOurs(p) ? 'FloPilot ' + p.version : p.name + ' ' + p.version;

async function fetchLatest() {
  try {
    const r = await CapacitorHttp.get({ url: API, headers: { Accept: 'application/vnd.github+json' } });
    if (r.status === 404) return { none: true };
    if (r.status !== 200) return { error: 'GitHub answered ' + r.status };
    const d = typeof r.data === 'string' ? JSON.parse(r.data) : r.data;
    const a = (d.assets || []).find(x => x.name === ASSET);
    if (!a) return { error: 'The release has no package file' };
    return { tag: d.tag_name, version: String(d.tag_name || '').replace(/^v/, ''), notes: String(d.body || '').slice(0, 1500), url: a.browser_download_url };
  } catch (e) {
    return { error: 'No internet? ' + (e && e.message || e) };
  }
}

async function download(url) {
  const r = await CapacitorHttp.get({ url, responseType: 'arraybuffer' });
  if (r.status !== 200) throw new Error('download failed (' + r.status + ')');
  const bin = atob(r.data);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i);
  return out;
}

function draw(pg) {
  const p = onBoard();
  const l = latest;
  const same = p && isOurs(p) && l && l.version && p.version === l.version;
  pg.innerHTML = '<div class="card"><b>ON THE BOARD</b><div class="big" style="margin-top:4px">' + esc(shown(p)) + '</div>'
    + (p && !isOurs(p) ? '<p class="dim">Installing the FloPilot package adds the horn settings, melodies, light module (SLCM) features and more. Your Refloat settings are kept.</p>' : '')
    + '</div><div class="card"><b>LATEST RELEASE</b>'
    + (!l ? '<p class="dim">Checking…</p>'
      : l.none ? '<p class="dim">No release yet. Tested releases show up here.</p>'
      : l.error ? '<p class="dim">' + esc(l.error) + '</p>'
      : '<div class="big" style="margin-top:4px">FloPilot ' + esc(l.version) + '</div>' + (same ? '<p class="dim">Already on your board.</p>' : '')
        + (l.notes ? '<div class="notes">' + esc(l.notes.replace(/Refloat Glow/g, 'FloPilot')) + '</div>' : ''))
    + '<div class="pills" style="margin-top:8px"><span class="pill" id="pkCheck">CHECK AGAIN</span>'
    + (l && l.url ? '<span class="pill on" id="pkInstall">' + (same ? 'REINSTALL' : 'INSTALL') + '</span>' : '') + '</div></div>';
  pg.querySelector('#pkCheck').onclick = () => { latest = null; draw(pg); fetchLatest().then(r => { latest = r; draw(pg); }); };
  const ib = pg.querySelector('#pkInstall');
  if (ib) ib.onclick = () => confirmInstall(pg);
}

// safety: board connected, stopped, nobody on it
function blocked() {
  const b = link.board, S = link.S || window.__sim;
  if (!b || !link.live) return 'Connect your board first';
  if (b.pkgState === 3 || (S && Math.abs(S.speed) > 0.5)) return 'Stop the board first';
  if (S && (S.fpL || S.fpR)) return 'Step off the board: the footpad is pressed';
  return '';
}

function overlay(html) {
  let o = document.getElementById('fpInst');
  if (!o) { o = document.createElement('div'); o.id = 'fpInst'; document.body.appendChild(o); }
  o.innerHTML = '<div class="box">' + html + '</div>';
  return o;
}
const closeOverlay = () => { const o = document.getElementById('fpInst'); if (o) o.remove(); };

function confirmInstall(pg) {
  const why = blocked();
  if (why) return toast(why);
  const o = overlay('<h3>INSTALL FLOPILOT ' + esc(latest.version) + '</h3>'
    + '<p>This replaces the package on your board. It takes about a minute.</p><ul>'
    + '<li>Board on a stand or on its side, <b>nobody on it</b>.</li>'
    + '<li>Keep the phone close and FloPilot open until it says done.</li>'
    + '<li>Your Refloat settings and tunes are kept.</li>'
    + '<li>The board won\'t balance while it installs.</li></ul>'
    + '<div class="row"><div class="btn" id="piNo">CANCEL</div><div class="btn go" id="piGo">INSTALL</div></div>');
  o.querySelector('#piNo').onclick = closeOverlay;
  o.querySelector('#piGo').onclick = () => run(pg);
}

async function run(pg) {
  const why = blocked();
  if (why) { closeOverlay(); return toast(why); }
  busy = true;
  link.installing = true;
  const o = overlay('<h3>INSTALLING</h3><p id="piTxt">Downloading…</p><div class="bar"><i id="piBar"></i></div><p class="dim" style="color:#b98cff;font-size:11px">Don\'t close FloPilot or ride the board.</p>');
  const set = (f, t) => { o.querySelector('#piBar').style.width = Math.round(f * 100) + '%'; if (t) o.querySelector('#piTxt').textContent = t + ' ' + Math.round(f * 100) + '%'; };
  try {
    const file = await download(latest.url);
    const pkg = unpack(file, inflate);
    const bad = check(pkg);
    if (bad) throw new Error(bad);
    if (blocked()) throw new Error(blocked());
    await new Installer(link.board).install(pkg, deflate, set);
    await new Promise(r => setTimeout(r, 1500));
    if (link.board) { link.board.pkg = null; link.board.comms = 'info'; link.board.started = Date.now(); }  // read the new package
    if (link.onPackageChanged) link.onPackageChanged();
    overlay('<h3>DONE</h3><p>FloPilot ' + esc(latest.version) + ' is on your board. It restarts its package now; give it a few seconds.</p><div class="row"><div class="btn go" id="piOk">OK</div></div>')
      .querySelector('#piOk').onclick = closeOverlay;
  } catch (e) {
    overlay('<h3>NOT INSTALLED</h3><p>' + esc(e && e.message || e) + '</p><p>If it stopped partway, the board has no working package until you install again: it won\'t balance. Try again, or use VESC Tool.</p><div class="row"><div class="btn" id="piOk">CLOSE</div><div class="btn go" id="piAgain">TRY AGAIN</div></div>');
    const ob = document.getElementById('fpInst');
    ob.querySelector('#piOk').onclick = closeOverlay;
    ob.querySelector('#piAgain').onclick = () => confirmInstall(pg);
  } finally {
    busy = false;
    link.installing = false;
    draw(pg);
  }
}

function addTab() {
  const box = document.getElementById('modalBox');
  const tabs = box && box.querySelector('.tabs');
  if (!tabs || !box.querySelector('[data-pg="2"]') || box.querySelector('[data-tab="pk"]')) return;
  const t = document.createElement('div');
  t.className = 'pill'; t.dataset.tab = 'pk'; t.textContent = 'PACKAGE';
  tabs.appendChild(t);
  const pg = document.createElement('div');
  pg.dataset.pg = 'pk'; pg.id = 'fpPkg';
  pg.style.cssText = 'display:none;max-height:52vh;overflow:auto';
  box.querySelector('[data-pg="2"]').after(pg);
  t.onclick = () => {
    box.querySelectorAll('[data-tab]').forEach(x => x.classList.toggle('on', x === t));
    box.querySelectorAll('[data-pg]').forEach(g => (g.style.display = g === pg ? '' : 'none'));
    draw(pg);
    if (!latest || latest.error) fetchLatest().then(r => { latest = r; if (pg.isConnected) draw(pg); });
  };
  box.querySelectorAll('[data-tab]').forEach(x => x !== t && x.addEventListener('click', () => { pg.style.display = 'none'; t.classList.remove('on'); }));
}

export function initPkg(l, toastFn) {
  link = l; toast = toastFn;
  const st = document.createElement('style'); st.textContent = css; document.head.appendChild(st);
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(addTab, 0));
  fetchLatest().then(r => { latest = r; });
}
export const installing = () => busy;
