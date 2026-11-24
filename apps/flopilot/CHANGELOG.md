# FloPilot app change log

The iPhone app (TestFlight). One section per day; later rolled up into weekly
summaries and milestones. Builds are TestFlight build numbers ("1.0 (N)").
Corrections are noted in place ("Correction: ...").

Names: **FloPilot** = the app; **RF Glow** = the VESC package (Refloat Glow).
Nothing here is bench- or ride-tested unless it says so.

---

## Milestone 1: FloPilot on TestFlight (2026-11-23)

The app connects to real boards (stock Refloat 1.2.3 and RF Glow, including
VESC Express / CAN boards such as the Thor 400), shows live data, and drives
the board's own settings and features the way RF Glow's screen does.

## 2026-11-23 (Mon)

**Setup and look**
- Builds 1-5: TestFlight pipeline (GitHub Actions, App Store Connect API key,
  automatic signing at export). Build 5 was the first on TestFlight.
- Build 6-7: first app icon; screen kept inside the iPhone safe area
  (battery bar clear of the camera).
- Build 15: the user's FloPilot icon. Build 17: app text says FloPilot
  (Flo purple, Pilot green). Build 18: the package is called "RF Glow" in the app.
- Build 21-23: no board = Home Screen dimmed, no board name; only CONNECT
  BOARD and DEMO work; DEMO opens the simulator panel and is always off at start;
  settings tabs wrap instead of running off the window.
- Build 11: Neon Future SEARCH FOR BOARD window (VESC Tool-style scan,
  signal bars, LAST USED, RENAME).
- Build 27: HUD scales to fit so the HORN stays above the swipe-up window.

**Connection**
- Build 9: keeps the Bluetooth link while locked; keeps the last readings and
  reconnects to the same board when the link drops.
- Build 14: reads the VESC firmware version.
- Build 24: finds the motor controller behind a VESC Express and forwards
  over CAN (fixes "Refloat not found" on the Thor 400). Confirmed working on
  the user's Thor 400 + RF Glow.

**Lock screen**
- Build 10: Live Activity ride card (up to 4 readings, light button,
  settings in gear › LOCK SCREEN). Not yet confirmed whether it updates
  while the phone is locked.

**Board features**
- Build 12: LEDS tab reads / writes Refloat's own LED settings (stock 1.2.3 too).
- Build 13-16: horn, melodies, MIXER through VESC firmware motor tones
  (6.05+), with an on-board watchdog and time caps.
  Correction (build 16): the first version broke the firmware's REPL rules
  (one command per 0.5 s, 512-byte packets) and would mostly not have worked;
  now checks Lisp memory first (the Floaty "high memory usage" case).
- Build 17: gear › PACKAGE installs the latest RF Glow release over
  Bluetooth (needs a tagged release; none yet).
- Build 18: MIXER live tiles in real time on RF Glow; stock boards choose
  LIVE ON PHONE / ON BOARD. (Changed in build 32: phone is the default for all boards.)
- Build 19: detects the light module (SLCM) and picks the LEDS view; SLCM
  settings sent to RF Glow.
- Build 20: real SLCM check and Bluetooth update (LEDS › UPDATES). Found a
  page-slot bug that also affects RF Glow's own updater (not fixed in the
  package yet).
- Build 25: TEST BEEP / MIXER BEEP on the board (RF Glow).
- Build 26: ALL LIGHTS / HEAD-TAIL switch the board; tilt refreshed in time
  for Refloat; simulator-only bits hidden with a board.
- Build 27: MOVE slider drives Refloat's RC move while idle.
- Build 28: full pass: Pushback / Haptic windows, Disable package, Handtest,
  Flywheel, TUNES slots + Tune Archive, boot melody / horn settings and
  battery gauge to RF Glow, tilt remote type. Correction: the app misread
  non-LED Refloat settings (missing scale) - fixed before anything used them.
- Build 29: Max speed limit on the board (motor config, VESC 6.05 / 6.06,
  signatures checked against the firmware); backups on the phone (Refloat +
  motor settings); SLCM vs Refloat-strips view chosen like RF Glow (strip
  counts too).
- Build 30: CONTROL › DMF toggle (Refloat's Disable Moving Faults, with
  Refloat's warning before it turns on; saved with the board stopped); DATA
  moved from the swipe-up window to Settings (gear) › DATA; Onboarding Tour
  swipe-up page says so.
- Build 31: connected board: the name pill over the speedo is now a small
  green dot right after the board name (tap it to disconnect).
- Build 32: MIXER tiles play on the phone by default (every board), with
  LIVE ON BOARD as the option and a note about the half-second delay on
  stock Refloat (real time with RF Glow).
- Build 33: CONNECT BOARD and DEMO pills hide while the SIMULATOR panel is
  open (they were covering its buttons).
- Build 34: SNOOZE works like RF Glow: with RF Glow and controller LEDs
  (e.g. Thor) the SNOOZE settings now reach the board (before, only the SLCM
  view sent light settings). On stock Refloat the SNOOZE rows hide, with a
  note that SNOOZE needs RF Glow.

**On hold**
- SPEED TONE (hapticspeedsplit test build): not in the app until the user
  has bench-tested that build.
- Basic Tuning redesign (waiting for the user's example tunes).
