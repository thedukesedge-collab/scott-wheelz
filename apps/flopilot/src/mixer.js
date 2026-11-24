// SPDX-License-Identifier: GPL-3.0-or-later
// MIXER tab, with a board: where the live tiles play. BOARD (default): the
// motor plays them: in real time with RF Glow, about half a second late on
// stock Refloat (one app command per 0.5 s). PHONE: the phone plays them, so
// playing feels instant; record / test, then PLAY puts the take on the board.
// Opening MIXER with a board shows a short info window (until "don't show
// this message again").
import { mixLive } from './sound.js';

export function initMixer(link, toast) {
  const notes = document.getElementById('mxNotes');
  if (!notes) return;
  const pill = document.createElement('div');
  pill.className = 'pill';
  pill.id = 'mxWhere';
  notes.after(pill);
  // a short note under the MIXER buttons
  const row = notes.parentElement;
  const hint = document.createElement('p');
  hint.className = 'ledHint';
  hint.id = 'mxWhereHint';
  hint.style.cssText = 'text-align:center;margin:4px 6px';
  row.after(hint);
  const glow = () => { const b = link.board; return !!(b && b.pkg && b.pkg.name === 'Refloat Glow'); };
  const draw = () => {
    const on = !!(link.board && link.live);
    pill.style.display = hint.style.display = on ? '' : 'none';
    const board = mixLive() === 'board';
    pill.textContent = board ? 'LIVE ON BOARD' : 'LIVE ON PHONE';
    pill.classList.toggle('on', board);
    hint.textContent = !board ? 'Tiles play on the phone so you hear them right away. Record, then PLAY puts it on the board. Tap LIVE ON PHONE to play on the board.'
      : glow() ? 'Tiles play on the board in real time (RF Glow). Tap LIVE ON BOARD to play on the phone.'
      : 'Tiles play on the board, each note about half a second late. Tap LIVE ON BOARD to play on the phone instead.';
  };
  pill.onclick = () => {
    const next = mixLive() === 'board' ? 'phone' : 'board';
    try { localStorage.setItem('fpMixLive', next); } catch (e) {}
    draw();
    toast(next === 'board' ? (glow() ? 'Tiles play on the board' : 'Tiles play on the board, about half a second late') : 'Tiles play on the phone');
  };
  draw();
  setInterval(draw, 1000);
  // info window when MIXER opens with a board (until "don't show again")
  const info = () => {
    if (!(link.board && link.live) || typeof window.modal !== 'function') return;
    try { if (localStorage.getItem('fpMixInfo') === 'hide') return; } catch (e) {}
    const g = glow();
    window.modal('<h3>MIXER</h3>'
      + '<p>Tiles start in <b>LIVE ON BOARD</b>: they play through your board\'s motor.</p>'
      + (g ? '<p>With RF Glow each note plays in real time. Bluetooth can still add a tiny delay now and then.</p>'
           : '<p>Each note can be about <b>half a second late</b>. Stock Refloat\'s VESC firmware takes only one app command every 0.5 s, so live notes queue up. (RF Glow plays them in real time: gear › PACKAGE.)</p>')
      + '<p>To work out a melody first, tap <b>LIVE ON BOARD</b> to switch to <b>LIVE ON PHONE</b>: the tiles play on the phone speaker right away. Then REC it, SAVE it, and PLAY sends the saved melody to the board, where it plays in time.</p>'
      + '<label style="display:flex;gap:8px;align-items:center;margin-top:10px;color:#ead6ff;font-size:13px"><input type="checkbox" id="mxInfoHide"> Don\'t show this message again</label>'
      + '<div class="btns"><button class="pri" data-ok>GOT IT</button></div>',
      () => { const c = document.getElementById('mxInfoHide'); if (c && c.checked) { try { localStorage.setItem('fpMixInfo', 'hide'); } catch (e) {} } });
  };
  const tab = document.querySelector('.tab[data-i="2"]');
  if (tab) tab.addEventListener('click', () => setTimeout(info, 50));
}
