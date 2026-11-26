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
  LIVE ON PHONE / ON BOARD. (Build 32 made the phone the default; build 37 went back to the board.)
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
- Build 35: log only (SPEED TONE on hold).
- Build 36: LEDS: a small gap between ALL LIGHTS and the HEAD/TAIL / STATUS
  buttons.
- Build 37: MIXER tiles play on the board by default again (LIVE ON BOARD),
  with an info window when MIXER opens: why notes can lag on stock Refloat
  and how LIVE ON PHONE helps work out a melody before saving it to the
  board. "Don't show this message again" check box. (Changes build 32.)
- Build 38: CONTROL: DISABLE MOVING FAULTS (DMF) moved up, between MOVE and
  HORN SETTINGS.
- Build 39: WATCHDOG / Ghosting Protection (gear › WATCHDOG):
  - Watches ADC1 / ADC2 (Refloat's realtime footpad volts) while connected.
    Checks only count with the board stopped and parked on its tail
    (tilted 10°+). EARLY > 0.2 V, HIGH > 1.0 V, STUCK at/over the pad's
    switch voltage, held 10 s (all editable in ADVANCED).
  - Two pad gauges, status (MONITORING / WAITING / NOT MONITORING), switch
    voltages + Posi, last 50 warnings, daily resting voltage with a
    "rising" note vs last week.
  - Alerts: banner for EARLY / HIGH, window for STUCK; both pads stuck =
    "CRITICAL — POSSIBLE GHOSTING HAZARD" with diagnostics, DISABLE PACKAGE
    (confirm, stopped only) and ACKNOWLEDGE (changes nothing). iPhone
    notification when the app is in the background.
  - Ghosting Pad Emergency (user's tested method): bad pad 5.0 V (past
    Refloat's 3.3 V editor limit) + Posi ON. Confirm box, stopped only,
    refuses if the good pad reads high or is off, or the bad pad reads 4 V+.
    Read back; originals kept; PAD TEST; RESTORE ORIGINAL SETTINGS.
    While on, tunes / backups can't put the footpad settings back.
  - Home screen ADC volts now show the real readings when connected (they
    were a fixed 2.9 / 3.0 V stand-in).
  - Onboarding Tour: new GHOSTING PROTECTION page before CREDIT.
- Build 40: new app icon (the user's FP neon logo).
- Build 41: MAX AIR (Settings › PREFERENCES › MAX AIR, off by default):
  - Third home-screen badge between TOP SPEED and MAX DUTY (the MAX SPEED
    badge moves just under it while it's on). Tap: this trip's max / total
    air / jumps; hold: reset max air.
  - Strict detection from Refloat's live data: balancing + a footpad
    pressed the whole time, wheelslip with low motor current, then a landing
    current spike; 0.3-3 s; any data gap = not counted. Display only.
  - Per-board AIR RECORDS (last 50 trips + board best) in PREFERENCES; a
    new trip starts when the board's trip goes back to 0 (power cycle).
  - Simulator: "Jump (air time)" scenario for DEMO.
  - Onboarding Tour: HOME SCREEN page mentions MAX AIR.
- Build 42: MAX AIR also needs the motor carrying a rider: 8 A+ in the
  second before take-off and 6 A+ average after the landing (rules out a
  free spin with the pads held by hand).
- Build 43: DEMO on: CONNECT BOARD and the demo pill (now "■ STOP DEMO")
  go small, side by side in the gap under the speedo, clear of the TOP
  SPEED / MAX AIR / MAX DUTY badges.
- Build 44: MAX AIR badge moved above the speedo (under the board name
  row, centered); HAPTIC OFF note moved to the top left, under the board
  name.
- Build 45: HORN back above the swipe-up window: if the home screen's rows
  outgrow the fit scaling, the app shrinks them a little more.
- Build 46: the swipe-up window now slides up OVER the Home Screen (it keeps
  its size) instead of shrinking it. Settings › PREFERENCES › UI switches
  back to the old way.
- Build 47: MAX AIR badge stays above the speedo after using the swipe-up
  window (placed from layout positions, re-placed on every re-layout).
- Build 48: Settings switches: knobs centered in their tracks.
- Build 49: LEDS: the NON-RGB HEAD/TAIL LIGHTS view switch removed (as in
  RF Glow rf183); the tour's LIGHTS page no longer mentions it.
- Build 50: home screen stops shifting around:
  - The simulator page's functions the app relies on (window.modal /
    applySheet / layout) weren't reachable (the page's script is a closure),
    so the MIXER info window never opened and re-placing after a re-layout
    didn't happen. Now exposed, with RF_AFTER_LAYOUT / RF_AFTER_NOTES hooks.
  - MAX AIR on: the speedo moves down (and shrinks) by the box's height, so
    the box sits fully above it, like RF Glow rf184.
  - HORN fit check moved into the page's own layout pass (no second pass
    0.7 s later, so nothing jumps when the swipe-up window moves or tabs
    change).
  - DEMO: CONNECT BOARD and "■ STOP" sit in the board name row when they fit.
- Build 51: TOP SPEED / MAX DUTY / MAX AIR text shrinks to fit its box.
- Build 52: HORN stays above the closed swipe-up window when the home
  screen grows after it was laid out (fonts arriving, notes, badges): the
  page fits it again (once the fonts load, and a check every 0.4 s that only
  acts when the HORN is covered).

**On hold**
- Battery curve for boards without an SLCM (app % with cell curve + sag
  compensation + AUTO-LEARN; RF Glow controller status bar would need a
  firmware change; stock Refloat's LED bar can't take it): skipped for now.
- SPEED TONE (hapticspeedsplit test build): not in the app until the user
  has bench-tested that build.
- Basic Tuning redesign (waiting for the user's example tunes).

## 2026-11-25 (Wed)

**Lights**
- Build 53: LEDS view for light modules on Refloat (LED Mode External, no
  controller strips): Floatwheel LCMs with Dado's firmware (tester: Floatwheel
  Atom), an SLCM on stock Refloat. Refloat only sends these brightness, so
  the view shows just that: HEAD/TAIL LIGHTS on/off with RIDING / IDLE
  brightness, STATUS BAR on/off with one BRIGHTNESS (Refloat's Status
  Brightness, which Dado's firmware uses riding and idle). No colors,
  animations, snooze or LCM tools there. HEAD/TAIL OFF also writes Front
  Brightness 0 (Refloat sends that to the LCM while the headlights are off),
  STATUS BAR OFF writes Status Brightness 0 (written when the board is
  stopped). The banner shows the light module's name. Simulator: FLOATWHEEL
  LCM option. Not tested on hardware.

**Android**
- First Android test build (APK, sideloaded; GitHub release
  "flopilot-android"): the same app as the iPhone build. Not on Android yet:
  the lock-screen card and WATCHDOG phone notifications (in-app alerts
  work). Signed with a public test key (apps/flopilot/android-dev-signing)
  until a private one is set up. Not tested on a phone yet.

**Dashboard**
- Build 54 (tester report: speed and duty off, right in Floaty): speed and
  duty now come from the VESC's own values (what VESC Tool and Floaty show).
  Speed was read from Refloat's realtime data as m/s, but Refloat sends it in
  km/h, so it showed about 3.6x too high (TOP SPEED too); Refloat's duty is
  smoothed. Refloat's data stays as the fallback (km/h converted right).
  Old TOP SPEED records from before this build are too high: reset them.
