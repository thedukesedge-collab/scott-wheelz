// SPDX-License-Identifier: GPL-3.0-or-later
// LEDS tab: pick the right view for what's on the board, like RF Glow's own
// screen does (ui.qml.in slcmDetect / slcmPresets), and send the SLCM light
// settings to RF Glow (ledSender.send).
//
// What's fitted, from stock Refloat 1.2.3 commands (works without RF Glow):
// - Refloat's LED mode (config hardware.leds.mode, read by ledsync.js)
// - the light module's own name (COMMAND_LCM_DEVICE_INFO): our SLCM calls
//   itself "VPBL-<build>" (with the Bluetooth bootloader) or "VP-<build>"
// View:
// - SLCM + RF Glow on the board -> SLCM view (animations, colors, snooze...)
// - SLCM + stock Refloat        -> Refloat view, and a note that the SLCM
//                                  features need RF Glow (gear > PACKAGE)
// - LED strips on the controller, or another light module -> Refloat view
// - Refloat's LEDs off          -> no light controls
import { Rf } from './vesc.js';

const RF_GLOW = 'Refloat Glow';
export const isSlcm = name => /^VP(BL)?-/.test(name || '');

export function pickSetup({ hw, lcmName, rfGlow, rgb, status }) {
  if (hw === null || hw === undefined) return null;  // config not read yet: leave the view alone
  // RF Glow's slcmPresets: an SLCM and no controller-driven strips
  if (isSlcm(lcmName) && rfGlow && !rgb && !status) return 'slcm';
  if (rgb || status) return 'refloat';  // Refloat's own LED strips (Thor etc.)
  if (hw === 0) return 'none';
  // a light module (LED Mode External) and no controller strips: Floatwheel
  // LCM with Dado's firmware, an SLCM on stock Refloat, ... - Refloat sends
  // it brightness only (headlights riding / idle, status bar)
  if (hw === 2 || hw === 3) return 'lcm';
  return 'refloat';
}

// RF Glow COMMAND_LCM_VP_SETTINGS, the same 30 bytes as ledSender.send() in ui.qml.in
export function vpSettings(P) {
  const d = new Uint8Array(30), v = new DataView(d.buffer);
  const u8 = (i, x) => (d[i - 2] = Math.max(0, Math.min(255, Math.round(+x || 0))));
  u8(2, (P.ledHead ? 1 : 0) | (P.ledBar ? 2 : 0) | (P.ledBeep !== false ? 4 : 0) | (P.snzStyle === 2 ? 0x20 : 0));
  u8(3, P.ledHeadPct); u8(4, P.ledIdlePct); u8(5, P.ledRidePct);
  u8(6, P.ledAnimIdle); u8(7, P.ledAnimRide); u8(8, P.ledAnimDoze); u8(9, P.ledAnimBoot); u8(10, P.ledAnimShut);
  v.setUint16(9, Math.max(0, Math.min(65535, Math.round(+P.ledColor || 0))));  // bytes 11-12
  u8(13, P.ledHeadIdlePct);
  const cu = Array.isArray(P.ledCustom) && P.ledCustom.length === 8 ? P.ledCustom : [0x24, 1, 0, 150, 39, 29, 60, 0];
  cu.forEach((x, i) => u8(14 + i, x));
  u8(22, P.ledSnoozeMin); u8(23, P.rideMode || 0); u8(24, P.snzStyle || 0); u8(25, P.stSnz || 0);
  const ride = P.ride || { c1: P.rfColor1, c2: P.rfColor2, speed: P.rfSpeed };
  const snz = P.snz || { c1: P.rfColor1, c2: P.rfColor2, speed: P.rfSpeed };
  u8(26, ride.c1); u8(27, ride.c2); u8(28, (ride.speed || 0) * 10);
  u8(29, snz.c1); u8(30, snz.c2); u8(31, P.snzAdv ? (snz.speed || 0) * 10 : 0);
  return d;
}

export function initLights(link, ledSync, toast) {
  let told = null, lastSent = '', sentAt = 0;
  setInterval(() => {
    const b = link.board, P = window.__simP ? window.__simP() : null;
    if (!b || !link.live || link.installing || !P) return;
    const rfGlow = !!(b.pkg && b.pkg.name === RF_GLOW);
    window.RF_LCM_NAME = b.lcmName || '';  // the LEDS banner (light module view)
    const setup = pickSetup({ hw: ledSync.hw, lcmName: b.lcmName, rfGlow, rgb: ledSync.rgbAvail, status: ledSync.statusAvail });
    if (setup && P.simSetup !== setup) {
      P.simSetup = setup;
      try { window.__simRender && window.__simRender(); } catch (e) {}
    }
    if (isSlcm(b.lcmName) && !rfGlow && b.pkg && told !== b) {
      told = b;
      toast('SLCM light module found. Its animations, colors and more need RF Glow: gear › PACKAGE');
    }
    // light settings to RF Glow (SLCM view, and the controller-LED view's
    // SNOOZE rows): once the LEDS tab was used (as the package does), not
    // while APPLY is pending, on a change and every 5 s
    if (rfGlow && (setup === 'slcm' || setup === 'refloat') && P.ledCfg && !P.ledPending) {
      const d = vpSettings(P), key = d.join();
      const now = Date.now();
      if (key !== lastSent || now - sentAt > 5000) {
        b.app([Rf.lcmVpSettings, ...d]);
        lastSent = key; sentAt = now;
      }
    }
  }, 500);
}
