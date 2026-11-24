// SPDX-License-Identifier: GPL-3.0-or-later
// With a board connected: Settings › SETUP › BACKUP / RESTORE kept on this
// phone (RF Glow's live in VESC Tool's storage, which an app can't reach):
// the board's Refloat settings + motor settings, exactly as the board sent
// them. Restore writes them back as they were (same layout / signature only),
// board stopped, after a confirm. And CONTROL › MAX SPEED on the real board
// (mcconf.js).
import { McConf } from './mcconf.js';
import { RefloatConfig } from './refcfg.js';
import { Comm, Rf } from './vesc.js';

const KEY = 'fpBackups';
const MAX = 6;  // per board
const b64 = u8 => { let s = ''; for (const x of u8) s += String.fromCharCode(x); return btoa(s); };
const unb64 = s => Uint8Array.from(atob(s), c => c.charCodeAt(0));
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

export function initBackup(link, ledSync, toast) {
  const mc = new McConf(link, toast);
  setInterval(() => mc.attach(link.live ? link.board : null), 500);
  const all = () => { try { return JSON.parse(localStorage.getItem(KEY) || '{}'); } catch (e) { return {}; } };
  const keep = o => { try { localStorage.setItem(KEY, JSON.stringify(o)); return true; } catch (e) { return false; } };
  const id = () => link.deviceId || 'board';

  // ---------- MAX SPEED (simulator hooks) ----------
  const P = () => (window.__simP ? window.__simP() : null);
  async function syncMs() {
    if (!(await mc.read())) return;
    const ms = window.__simMS ? window.__simMS() : null, p = P();
    if (ms) { ms.d = mc.get('si_wheel_diameter') || ms.d; ms.poles = mc.get('si_motor_poles') || ms.poles; ms.gear = mc.get('si_gear_ratio') || 1; }
    if (p) { p.msCap = mc.capMph; try { window.__simRender && window.__simRender(); } catch (e) {} }
  }
  let msBoard = null;
  setInterval(() => { if (link.live && link.board && link.board !== msBoard && link.board.fw) { msBoard = link.board; syncMs(); } }, 1000);
  window.RF_MAXSPEED = {
    set(mph, show) {
      if (!link.live) return false;
      show('Saving to the board…', false);
      mc.setLimit(mph).then(e => { if (e) show(e, false); else { const p = P(); if (p) p.msCap = mc.capMph; show('✔ Confirmed on the board: Max ERPM ' + Math.round(mc.get('l_max_erpm')) + ' = ' + mc.capMph + ' mph', true); } });
      return true;
    },
    remove(show) {
      if (!link.live) return false;
      show('Saving to the board…', false);
      mc.removeLimit().then(e => { if (e) show(e, false); else { const p = P(); if (p) p.msCap = 0; show('✔ Limit removed on the board', true); } });
      return true;
    },
  };

  // ---------- BACKUP / RESTORE (Settings › SETUP) ----------
  async function backup() {
    if (!ledSync.cfg) return "Refloat's settings aren't loaded yet";
    const okMc = await mc.read();
    const list = (all()[id()] || []).slice(0, MAX - 1);
    list.unshift({ t: Date.now(), board: link.name, pkg: link.board && link.board.pkg ? link.board.pkg.name + ' ' + link.board.pkg.version : '',
      rf: b64(ledSync.cfg.raw), mc: okMc && mc.cfg ? b64(mc.cfg.raw) : '' });
    const o = all(); o[id()] = list;
    if (!keep(o)) return 'Not enough space on the phone';
    return okMc && mc.cfg ? '' : 'Refloat settings saved (motor settings: this firmware isn\'t supported yet)';
  }
  async function restore(bk) {
    const b = link.board;
    if (!b || !link.live) return 'Connect your board first';
    if (b.pkgState === Rf.sRunning) return 'Stop the board first';
    const rf = new RefloatConfig(unb64(bk.rf));
    if (!rf.ok) return 'This backup is from another Refloat version: not restored';
    // Ghosting Pad Emergency active: keep its footpad settings
    if (ledSync.protect) for (const [k, v] of ledSync.protect) if (rf.has(k)) rf.set(k, v, true);
    // Refloat settings first (the whole config, as it was)
    const okRf = await new Promise(done => {
      const t = setTimeout(() => done(false), 4000);
      const prev = b.onConfigAck;
      b.onConfigAck = () => { clearTimeout(t); b.onConfigAck = prev; done(true); };
      b.send([Comm.setCustomConfig, 0, ...rf.raw]);
    });
    ledSync.reset(b);  // read everything back
    if (!okRf) return 'The board didn\'t confirm the Refloat settings';
    if (bk.mc) {
      const raw = unb64(bk.mc);
      if (!(await mc.read()) || !mc.cfg || mc.cfg.signature !== new DataView(raw.buffer).getUint32(0)) return 'Refloat settings restored; motor settings skipped (different firmware)';
      if (b.pkgState === Rf.sRunning) return 'Refloat settings restored; stopped before the motor settings';
      if (!(await mc.ask('write', [Comm.setMcconf, ...raw], 4000))) return 'Refloat settings restored; the board didn\'t confirm the motor settings';
    }
    return '';
  }

  function card(box) {
    const list = all()[id()] || [];
    box.innerHTML = '<b>BACKUP / RESTORE</b><p>Saved on this phone: your board\'s Refloat settings and motor settings, exactly as they are now.</p>'
      + '<div style="text-align:center"><span class="nb" id="bkNow">BACKUP NOW</span></div>'
      + (list.length ? list.map((x, i) => '<div class="row"><div class="l">' + esc(new Date(x.t).toLocaleString()) + '<small>' + esc((x.pkg || '') + (x.mc ? ' · + motor' : '')) + '</small></div><span class="nb" data-r="' + i + '">RESTORE</span><span class="nb" data-d="' + i + '">✕</span></div>').join('')
        : '<p style="opacity:.7">No backups of this board yet.</p>');
    box.querySelector('#bkNow').onclick = () => backup().then(e => { toast(e || 'Backup saved on this phone'); card(box); });
    box.querySelectorAll('[data-d]').forEach(x => (x.onclick = () => { const o = all(); (o[id()] || []).splice(+x.dataset.d, 1); keep(o); card(box); }));
    box.querySelectorAll('[data-r]').forEach(x => (x.onclick = () => {
      const bk = list[+x.dataset.r];
      if (!confirm('Restore the backup from ' + new Date(bk.t).toLocaleString() + '?\n\nAll Refloat settings' + (bk.mc ? ' and motor settings' : '') + ' go back to how they were then. Board stopped, nobody on it.')) return;
      toast('Restoring…');
      restore(bk).then(e => toast(e || 'Backup restored'));
    }));
  }
  const btn = document.getElementById('btnSettings');
  if (btn) btn.addEventListener('click', () => setTimeout(() => {
    if (!link.live) return;
    const bk = document.querySelector('#modalBox [data-t="Backed up (simulated)"]');
    const box = bk && bk.closest('.card');
    if (box) { box.style.display = ''; card(box); }
  }, 10));
}
