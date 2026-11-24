// SPDX-License-Identifier: GPL-3.0-or-later
// With a board connected, the remaining controls do what RF Glow's own screen
// does (ui.qml.in) instead of the simulator's pretend version:
// - Settings › SETUP: DISABLE / ENABLE PACKAGE (Refloat config "disabled"),
//   HANDTEST (COMMAND_HANDTEST); backups are VESC Tool's own storage
// - CONTROL: FLYWHEEL (COMMAND_FLYWHEEL, as sendFlywheel)
// - TUNES: slots save / apply the board's real tune (tuneManager: every
//   Refloat setting except LEDs, haptic, hardware and the not-a-tune list),
//   TUNE ARCHIVE downloads the real archive (the same CSV as RF Glow)
// - RF Glow only: horn / boot melody settings (COMMAND_HORN_SETTINGS +
//   COMMAND_MELODY_SET) and the battery gauge curve (COMMAND_LCM_BATTERY)
// - tilt slider: switches Refloat's Input Tilt remote to UART (as RF Glow's
//   setInputTiltRemoteType)
// Every config write: board stopped only (ledsync.js writeValues).
import { CapacitorHttp } from '@capacitor/core';
import { Rf } from './vesc.js';
import { isTune } from './ledsync.js';

const ARCHIVE = 'https://us-central1-mimetic-union-377520.cloudfunctions.net/float_package_tunes_via_http';
const KEY = 'fpTunes';
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

// RF Glow tuneManager.parseCsv
export function parseCsv(csv) {
  const lines = csv.split(/\r?\n/);
  const n = lines[0].split(',').length - 1;
  const out = Array.from({ length: n }, () => ({ name: '', settings: {} }));
  for (const line of lines) {
    const c = line.split(',');
    const key = c[0];
    for (let j = 0; j < n; j++) {
      let v = c[j + 1];
      if (!v) continue;
      if (key === '_name') { out[j].name = v; continue; }
      let name;
      if (key.startsWith('double_')) { name = key.slice(7); v = parseFloat(v); }
      else if (key.startsWith('int_')) { name = key.slice(4); v = parseInt(v, 10); }
      else if (key.startsWith('bool_')) { name = key.slice(5); v = !!parseInt(v, 10); }
      else if (key.startsWith('enum_')) { name = key.slice(5); v = parseInt(v, 10); }
      else continue;
      if (Number.isNaN(v)) continue;
      out[j].settings[name] = v;
    }
  }
  return out.filter(t => t.name);
}

export function initControl(link, ledSync, toast) {
  const rfGlow = () => { const b = link.board; return !!(b && link.live && b.pkg && b.pkg.name === 'Refloat Glow'); };
  const P = () => (window.__simP ? window.__simP() : null);
  const render = () => { try { window.__simRender && window.__simRender(); } catch (e) {} };
  const stored = () => { try { return JSON.parse(localStorage.getItem(KEY) || '{}'); } catch (e) { return {}; } };
  const store = o => { try { localStorage.setItem(KEY, JSON.stringify(o)); } catch (e) {} };

  // ---------- Settings › SETUP, CONTROL › FLYWHEEL ----------
  window.RF_ACTION = what => {
    const b = link.board;
    if (!b || !link.live) return false;  // no board: the simulator
    if (what === 'Package disabled (simulated)') {
      const dis = b.pkgState === Rf.sDisabled;
      const e = ledSync.writeValues({ disabled: !dis }, dis ? 'Package enabled' : 'Package disabled: the board won\'t balance until you enable it');
      if (e) toast(e);
      return true;
    }
    if (what === 'Handtest on (simulated)') {
      if (b.pkgState === Rf.sRunning) { toast('Stop the board first'); return true; }
      const on = b.pkgMode !== Rf.mHandtest;
      b.app([13, on ? 1 : 0]);
      toast(on ? 'Handtest on: engage by hand, currents are limited' : 'Handtest off');
      return true;
    }
    if (what === 'flywheel') {
      const on = b.pkgMode !== Rf.mFlywheel;
      if (on && b.pkgState === Rf.sRunning) { toast('Stop the board first'); return true; }
      b.app(on ? [22, 130, 0, 0, 0, 0, 1, 0] : [22, 128, 0, 0, 0, 0, 0, 0]);
      toast(on ? 'Flywheel on' : 'Flywheel off');
      return true;
    }
    if (/simulated/.test(what) && /Backed up|View/.test(what)) {
      toast('Backups live in VESC Tool\'s own storage: use VESC Tool for these');
      return true;
    }
    return false;
  };

  // the SETUP buttons say what they'll do on this board
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(() => {
    const b = link.board;
    if (!b || !link.live) return;
    const pk = document.querySelector('#modalBox [data-t="Package disabled (simulated)"]');
    if (pk) pk.textContent = b.pkgState === Rf.sDisabled ? 'ENABLE PACKAGE' : 'DISABLE PACKAGE';
    const ht = document.querySelector('#modalBox [data-t="Handtest on (simulated)"]');
    if (ht) ht.textContent = b.pkgMode === Rf.mHandtest ? 'HANDTEST OFF' : 'HANDTEST ON';
  }, 0));

  // ---------- TUNES ----------
  function applyValues(settings, label, slot) {
    const vals = {};
    for (const [k, v] of Object.entries(settings)) if (isTune(k)) vals[k] = v;
    const e = ledSync.writeValues(vals, label + ' applied', ok => {
      const p = P();
      if (ok && p && slot) { p.activeTune = slot; render(); }
      if (!ok) toast(label + ': the board didn\'t confirm, check it in VESC Tool');
    });
    if (e) toast(e);
  }
  window.RF_TUNES = {
    save(slot, name) {
      if (!link.live) return '';  // no board: the simulator keeps just the name
      const vals = ledSync.tuneValues();
      if (!vals) return "Refloat's settings aren't loaded yet";
      const all = stored();
      all[slot] = { name, settings: vals };
      store(all);
      return '';
    },
    apply(slot, name) {
      if (!link.live) return false;
      const t = stored()[slot];
      if (!t) { toast('No tune saved in this slot yet: hold it to save the board\'s current tune'); return true; }
      applyValues(t.settings, name, slot);
      return true;
    },
    erase(slot) { const all = stored(); delete all[slot]; store(all); },
    archive() {
      if (!link.live) return false;
      openArchive();
      return true;
    },
  };

  let archive = null;
  function overlay(html) {
    let o = document.getElementById('fpTa');
    if (!o) {
      o = document.createElement('div'); o.id = 'fpTa';
      o.style.cssText = 'position:fixed;inset:0;z-index:85;background:rgba(4,2,10,.85);display:flex;align-items:center;justify-content:center';
      document.body.appendChild(o);
    }
    o.innerHTML = '<div style="width:min(92vw,400px);max-height:82vh;overflow:auto;background:#0b0718;border:2px solid #b14cff;border-radius:14px;box-shadow:0 0 22px rgba(177,76,255,.55);padding:14px;color:#ead6ff;font:13px/1.45 -apple-system,system-ui,sans-serif">' + html + '</div>';
    return o;
  }
  const close = () => { const o = document.getElementById('fpTa'); if (o) o.remove(); };
  const H3 = t => '<h3 style="margin:0 0 10px;text-align:center;font:700 15px Orbitron,monospace;letter-spacing:2px;color:#39ff7a;text-shadow:0 0 8px rgba(57,255,122,.7)">' + t + '</h3>';
  const BTN = (id, t, go) => '<div id="' + id + '" style="flex:1;text-align:center;padding:10px 6px;border-radius:999px;font:700 11px Orbitron,monospace;letter-spacing:1px;border:2px solid ' + (go ? '#39ff7a;color:#39ff7a' : '#b14cff') + '">' + t + '</div>';
  const NEON = ['#39ff7a', '#b14cff', '#00f0ff', '#ff2bd6', '#ff8a1f', '#f5ff3b', '#ff2b4f', '#3b7bff'];

  function drawArchive(msg) {
    const list = archive || [];
    const o = overlay(H3('TUNE ARCHIVE') + '<p style="text-align:center;color:#b98cff;font-size:11px">' + esc(msg || (list.length + ' tunes from the Float package archive')) + '</p>'
      + list.map((t, i) => '<div data-i="' + i + '" style="border:1.5px solid ' + NEON[i % 8] + ';border-radius:10px;padding:10px;margin:6px 0;font:700 12px Orbitron,monospace;color:' + NEON[i % 8] + '">' + esc(t.name.toUpperCase()) + '</div>').join('')
      + '<div style="display:flex;gap:8px;margin-top:10px">' + BTN('taR', 'REFRESH') + BTN('taC', 'CLOSE', true) + '</div>');
    o.querySelector('#taC').onclick = close;
    o.querySelector('#taR').onclick = () => { archive = null; openArchive(); };
    o.querySelectorAll('[data-i]').forEach(x => (x.onclick = () => confirmTune(list[+x.dataset.i])));
  }
  function confirmTune(t) {
    const n = Object.keys(t.settings).filter(isTune).length;
    const o = overlay(H3('APPLY TUNE') + '<p style="text-align:center"><b>' + esc(t.name) + '</b></p><p>Changes ' + n + ' tune settings on your board. LEDs, pushback, haptic and safety settings stay as they are. Board stopped only.</p>'
      + '<div style="display:flex;gap:8px;margin-top:10px">' + BTN('tcN', 'CANCEL') + BTN('tcY', 'APPLY', true) + '</div>');
    o.querySelector('#tcN').onclick = () => drawArchive();
    o.querySelector('#tcY').onclick = () => { close(); applyValues(t.settings, t.name); };
  }
  async function openArchive() {
    if (archive) return drawArchive();
    drawArchive('Downloading tunes…');
    try {
      const r = await CapacitorHttp.get({ url: ARCHIVE, responseType: 'text' });
      if (r.status !== 200) throw new Error('the archive answered ' + r.status);
      archive = parseCsv(String(r.data));
      drawArchive();
    } catch (e) {
      drawArchive('Download failed: ' + (e && e.message || e));
    }
  }

  // ---------- RF Glow: horn / boot melody settings, battery gauge ----------
  let hornKey = '', melKey = '', batKey = null, batAsk = 0, lastBoard = null;
  setInterval(() => {
    const b = link.board, p = P();
    if (!b || !link.live || link.installing || !p) return;
    if (b !== lastBoard) { lastBoard = b; hornKey = melKey = ''; batKey = null; }
    if (!rfGlow()) return;
    // horn settings (ui.qml.in hornSettings.values / send)
    const adv = !!p.hAdv;
    const beepV = adv ? p.hBeep : (p.hsVol || 5) * 0.4;
    const beepHz = adv ? p.hBeepHz : Math.round((300 + ((p.hsPitch || 3) - 1) * 700 / 9) / 25) * 25;
    const buzzA = adv ? p.hBuzz : (p.hsStr || 0) * 1.5, buzzHz = adv ? p.hBuzzHz : 70;
    const flags = (p.hMelody ? 1 : 0) | (p.hBootCustom ? 2 : 0);
    const vol = Math.round(Math.max(1, Math.min(10, p.bootVol || 6)) * 0.25 * 10);
    const hb = [122, Math.round(beepV * 10) & 255, (beepHz >> 8) & 255, beepHz & 255, Math.round(buzzA * 10) & 255, (buzzHz >> 8) & 255, buzzHz & 255, flags, vol];
    if (hb.join() !== hornKey) { hornKey = hb.join(); b.app(hb); }
    // custom melody (ui.qml.in sendMelody): level, length in 20 ms
    const rec = Array.isArray(p.rec) ? p.rec : [];
    const mk = JSON.stringify(rec);
    if (mk !== melKey) {
      melKey = mk;
      b.app([124, ...rec.slice(0, 64).flatMap(([l, ms]) => [Math.max(0, Math.min(11, l)), Math.max(2, Math.min(100, Math.round(ms / 20)))])]);
    }
    // battery gauge: ask, then send only what the rider changed (the board's
    // own settings stay until then, as RF Glow's "synced")
    if (Date.now() - batAsk > 5000) { batAsk = Date.now(); b.app([129, 0]); }
    const bat = window.__simBat ? window.__simBat() : null;
    if (!bat || !b.battery) return;
    const k = JSON.stringify([p.batOn, p.batCell, p.batSize, p.batPar, p.batStyle, p.batResAuto, p.batRes, p.batCustom]);
    if (batKey === null) { batKey = k; if (p.batOn !== b.battery.on) { p.batOn = b.battery.on; batKey = JSON.stringify([p.batOn, p.batCell, p.batSize, p.batPar, p.batStyle, p.batResAuto, p.batRes, p.batCustom]); render(); } return; }
    if (k === batKey) return;
    batKey = k;
    const cells = b.battery.cells || 18;
    const res = p.batResAuto ? Math.max(0, Math.min(255, Math.round(bat.cell.ir * cells / bat.par / 2))) : Math.max(0, Math.min(255, p.batRes | 0));
    b.app([129, 1, p.batOn ? 1 : 0, res, ...bat.points.slice(0, 11)]);
    toast('Battery gauge sent to the board');
  }, 1000);

  // ---------- tilt: Refloat's remote input must be UART ----------
  let tiltDone = null;
  setInterval(() => {
    const b = link.board, S = link.S || window.__sim;
    if (!b || !link.live || !S || !S.tilt || tiltDone === b || !ledSync.cfg) return;
    tiltDone = b;
    if (ledSync.get('inputtilt_remote_type') === 1) { toast('Tilt works while riding (Refloat only uses it when engaged)'); return; }
    const e = ledSync.writeValues({ inputtilt_remote_type: 1 }, "Remote tilt turned on in Refloat. It works while riding");
    if (e) { tiltDone = null; toast('Tilt: ' + e); }
  }, 500);
}
