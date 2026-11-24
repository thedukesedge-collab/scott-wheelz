// SPDX-License-Identifier: GPL-3.0-or-later
// MIXER tab, with a board: where the live tiles play. PHONE (default): the
// phone plays them, so playing feels instant; record / test, then PLAY puts the
// take on the board. BOARD: the motor plays them: in real time with RF Glow,
// about half a second late on stock Refloat (one app command per 0.5 s).
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
      : glow() ? 'Tiles play on the board in real time (RF Glow).'
      : 'On the board through the app, each note is about half a second late (the VESC takes one app command per 0.5 s). For real-time playing on the board, install RF Glow: gear › PACKAGE.';
  };
  pill.onclick = () => {
    const next = mixLive() === 'board' ? 'phone' : 'board';
    try { localStorage.setItem('fpMixLive', next); } catch (e) {}
    draw();
    toast(next === 'board' ? (glow() ? 'Tiles play on the board' : 'Tiles play on the board, about half a second late') : 'Tiles play on the phone');
  };
  draw();
  setInterval(draw, 1000);
}
