// SPDX-License-Identifier: GPL-3.0-or-later
// MIXER tab, stock Refloat boards: where the live tiles play. PHONE (default):
// the phone plays them, so playing feels instant; record / test, then PLAY puts
// the take on the board. BOARD: the motor plays them, about half a second late
// (the VESC firmware takes one command per 0.5 s). With RF Glow on the board
// the tiles always play on the board in real time, so the choice is hidden.
import { mixLive } from './sound.js';

export function initMixer(link, toast) {
  const notes = document.getElementById('mxNotes');
  if (!notes) return;
  const pill = document.createElement('div');
  pill.className = 'pill';
  pill.id = 'mxWhere';
  notes.after(pill);
  const draw = () => {
    const b = link.board;
    const stock = !!(b && link.live && b.pkg && b.pkg.name !== 'Refloat Glow');
    pill.style.display = stock ? '' : 'none';
    const board = mixLive() === 'board';
    pill.textContent = board ? 'LIVE ON BOARD' : 'LIVE ON PHONE';
    pill.classList.toggle('on', board);
  };
  pill.onclick = () => {
    const next = mixLive() === 'board' ? 'phone' : 'board';
    try { localStorage.setItem('fpMixLive', next); } catch (e) {}
    draw();
    toast(next === 'board' ? 'Tiles play on the motor, about half a second late (stock Refloat)'
      : 'Tiles play on the phone. Record, then PLAY puts it on the board');
  };
  draw();
  setInterval(draw, 1000);
}
