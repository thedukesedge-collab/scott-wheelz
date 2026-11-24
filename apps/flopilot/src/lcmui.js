// SPDX-License-Identifier: GPL-3.0-or-later
// LEDS > UPDATES and LCM firmware tools, with a board connected: the real
// light module check and Bluetooth update (lcmfw.js) instead of the
// simulator's pretend one. Without a board the simulator's demo stays.
import { LcmUpdater, fetchRelease, downloadHex, parseVer, verText, fitOf } from './lcmfw.js';

const css = `
#fpLcm { position: fixed; inset: 0; z-index: 85; background: rgba(4,2,10,.85); display: flex; align-items: center; justify-content: center; }
#fpLcm .box { width: min(92vw, 400px); max-height: 84vh; overflow: auto; background: #0b0718; border: 2px solid #b14cff; border-radius: 14px;
  box-shadow: 0 0 22px rgba(177,76,255,.55); padding: 16px; color: #ead6ff; font: 13px/1.45 -apple-system, system-ui, sans-serif; }
#fpLcm h3 { margin: 0 0 10px; text-align: center; font: 700 15px Orbitron, monospace; letter-spacing: 2px; color: #39ff7a; text-shadow: 0 0 8px rgba(57,255,122,.7); }
#fpLcm .card { border: 1px solid #3d2266; border-radius: 10px; padding: 10px; margin: 8px 0; }
#fpLcm .card b { font: 700 11px Orbitron, monospace; letter-spacing: 1px; color: #b98cff; }
#fpLcm .big { font: 700 13px Orbitron, monospace; color: #39ff7a; margin-top: 4px; }
#fpLcm .warn { color: #ffd23f; }
#fpLcm .bar { height: 12px; border-radius: 6px; border: 1px solid #7a3fc0; overflow: hidden; margin: 10px 0 4px; }
#fpLcm .bar i { display: block; height: 100%; background: linear-gradient(90deg, #b14cff, #39ff7a); box-shadow: 0 0 8px #39ff7a; }
#fpLcm .row { display: flex; gap: 8px; margin-top: 10px; }
#fpLcm .btn { flex: 1; text-align: center; padding: 10px 6px; border-radius: 999px; font: 700 11px Orbitron, monospace; letter-spacing: 1px; border: 2px solid #b14cff; }
#fpLcm .btn.go { border-color: #39ff7a; color: #39ff7a; box-shadow: 0 0 10px rgba(57,255,122,.35); }
#fpLcm .btn.off { opacity: .35; }`;

const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

export function initLcmUi(link, toast) {
  const st = document.createElement('style'); st.textContent = css; document.head.appendChild(st);
  const up = new LcmUpdater(link);
  let rel = null, img = null, imgErr = '', checking = 0, checked = false, confirm = false, open = false;
  setInterval(() => up.attach(link.live ? link.board : null), 500);

  const stopped = () => {
    const b = link.board, S = link.S || window.__sim;
    return !!b && link.live && b.pkgState !== 3 && !(S && Math.abs(S.speed) > 0.5);
  };

  function verdict() {
    if (!up.hasBootloader || !rel || !rel.slcm) return null;
    const fit = fitOf(up.lcmVer, parseVer(rel.slcm));
    const r = verText(parseVer(rel.slcm)), l = verText(up.lcmVer);
    return {
      fit,
      title: { same: 'UP TO DATE', older: 'UPDATE AVAILABLE', newer: 'LCM NEWER THAN THE RELEASE', other: 'DIFFERENT BUILD' }[fit],
      text: { same: 'Your SLCM ' + l + ' is the same build as the release (' + rel.tag + '). You can still reflash it.',
        older: 'Your SLCM ' + l + '. The release (' + rel.tag + ') has ' + r + ', tested with it. Updating is recommended.',
        newer: 'Your SLCM ' + l + ' is newer than the ' + r + ' in the release (' + rel.tag + '). Flashing it would go back.',
        other: 'Your SLCM ' + l + ' is a different build from the release\'s ' + r + ' (a custom or test build?).' }[fit],
      button: { same: 'REFLASH ANYWAY', older: 'UPDATE LCM', newer: 'DOWNGRADE ANYWAY', other: 'FLASH RELEASE BUILD' }[fit],
    };
  }

  function fittedText() {
    if (checking) return 'Checking the light module…';
    if (!checked) return 'Tap CHECK LCM to see what light module firmware is fitted.';
    return {
      recovery: 'The LCM bootloader is waiting (an update didn\'t finish). Flash now to recover it.',
      legacy: 'Older SLCM (no version shown). TRY UPDATE works if it has the bootloader.',
      vpbl: 'SLCM with bootloader · ' + verText(up.lcmVer),
      vp: 'SLCM ' + verText(up.lcmVer) + ' without the bootloader: flash SLCM.hex once with an ST-Link, then updates work from here.',
      other: 'Light module firmware "' + up.name + '" can\'t be updated from the app. Flash SLCM.hex once with an ST-Link first.',
      none: 'No SLCM answered (stock, other or no LCM). Check Refloat\'s LED mode is External.',
    }[up.fitted];
  }

  function draw() {
    if (!open) return;
    let o = document.getElementById('fpLcm');
    if (!o) { o = document.createElement('div'); o.id = 'fpLcm'; document.body.appendChild(o); }
    const v = verdict(), rfg = up.rfGlow;
    const canFlash = rfg && checked && up.capable && img && !up.active && (!v || v.fit) ;
    let h = '<div class="box"><h3>LIGHT MODULE (SLCM)</h3>';
    if (up.active || up.done !== undefined && up.message) {
      h += '<div class="card"><b>UPDATE</b><p>' + esc(up.message) + '</p>' + (up.active ? '<div class="bar"><i style="width:' + Math.round(up.progress * 100) + '%"></i></div><p class="warn">Keep FloPilot open and close by. Don\'t ride or power off. The board won\'t start while it updates.</p>' : '') + '</div>';
    }
    if (!up.active) {
      h += '<div class="card"><b>FITTED</b><div class="big">' + esc(fittedText()) + '</div></div>';
      h += '<div class="card"><b>LATEST RELEASE</b><p>' + (!rel ? 'Checking…' : rel.none ? 'No release yet. Tested SLCM builds show up here with RF Glow releases.'
        : rel.error ? esc(rel.error) : !rel.hexUrl ? 'This release has no SLCM.hex.' : 'SLCM ' + esc(rel.slcm ? verText(parseVer(rel.slcm)) : '(version not listed)') + ' · ' + esc(rel.tag)) + '</p>'
        + (imgErr ? '<p class="warn">' + esc(imgErr) + '</p>' : '') + '</div>';
      if (v && checked) h += '<div class="card"><b>' + esc(v.title) + '</b><p>' + esc(v.text) + '</p></div>';
      if (checked && up.capable && !rfg) h += '<div class="card warn">Updating the SLCM from the app needs RF Glow on the board: gear › PACKAGE.</div>';
    }
    if (confirm) {
      h += '<div class="card"><b>BEFORE YOU FLASH</b><p>Board powered on and idle, nobody on it. About a minute. The board won\'t start while it updates. Keep FloPilot open until it says done.</p>'
        + '<div class="row"><div class="btn" data-a="no">CANCEL</div><div class="btn go" data-a="go">FLASH NOW</div></div></div>';
    } else {
      h += '<div class="row">' + (up.active ? '<div class="btn" data-a="cancel">CANCEL UPDATE</div>'
        : '<div class="btn" data-a="check">' + (checked ? 'CHECK AGAIN' : 'CHECK LCM') + '</div>'
          + '<div class="btn go' + (canFlash ? '' : ' off') + '" data-a="flash">' + (v ? v.button : up.fitted === 'legacy' ? 'TRY UPDATE' : 'FLASH LCM') + '</div>') + '</div>';
      if (!up.active) h += '<div class="row"><div class="btn" data-a="close">CLOSE</div></div>';
    }
    o.innerHTML = h + '</div>';
    o.querySelectorAll('[data-a]').forEach(x => (x.onclick = () => act(x.dataset.a, canFlash)));
  }

  async function loadImage() {
    if (!rel || !rel.hexUrl || img) return;
    try { img = await downloadHex(rel); imgErr = ''; } catch (e) { imgErr = 'SLCM.hex: ' + (e && e.message || e); }
    draw();
  }

  function check() {
    checking = 1; checked = false; draw();
    let n = 0;
    const t = setInterval(() => {
      up.ask();
      if (++n >= 6) { clearInterval(t); checking = 0; checked = true; draw(); }
    }, 300);
  }

  function act(a, canFlash) {
    if (a === 'close') { open = false; const o = document.getElementById('fpLcm'); if (o) o.remove(); return; }
    if (a === 'check') return check();
    if (a === 'cancel') return up.cancel();
    if (a === 'no') { confirm = false; return draw(); }
    if (a === 'flash') {
      if (!canFlash) {
        toast(!up.rfGlow ? 'Needs RF Glow on the board (gear › PACKAGE)' : !checked ? 'Tap CHECK LCM first'
          : !up.capable ? 'This light module can\'t be updated from the app' : !img ? 'No SLCM image from a release yet' : 'Not available');
        return;
      }
      confirm = true; return draw();
    }
    if (a === 'go') {
      confirm = false;
      if (!stopped()) { toast('Stop the board first'); return draw(); }
      up.done = undefined;
      up.start(img);
    }
  }

  up.onChange = draw;
  // the simulator's LEDS > UPDATES / LCM firmware tools call this first
  window.RF_LCMUI = () => {
    if (!link.live || !link.board) return false;  // no board: the simulator's demo
    open = true; confirm = false;
    draw();
    if (!rel || rel.error) fetchRelease().then(r => { rel = r; draw(); loadImage(); });
    else loadImage();
    if (!checked && !checking) check();
    return true;
  };
}
