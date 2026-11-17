# Re-Float Rink-Mixed fork notes

"Neon Future" is the name of the look (neon green/purple digital style on a
dark grid). Older entries called it "Tron"; renamed in v1.2.3-rf43.

(Renamed from "Re-RinkFloat" in v1.2.3-rf32. The output file is still
`rinkfloat.vescpkg` so the download link stays the same.)

(Renamed from "Rink-Float" in v1.2.3-rf14. The output file is still
`rinkfloat.vescpkg` so the download link stays the same.)

(Renamed from "Scott Wheelz" in v1.2.3-rf1. The GitHub repo is still
`scott-wheelz`, the output is now `rinkfloat.vescpkg`, and the app's saved
settings category is still `scottWheelz` so nothing is lost.)

Base: Refloat v1.2.3 (latest 1.2.x tag, commit 800a46b) — branch `scottwheelz`.
Target firmware: VESC 6.05.

## Firmware compatibility (checked)
`vesc_pkg_lib/vesc_c_if.h` was diffed against bldc `release_6_05`
(`lispBM/c_libs/vesc_c_if.h`, commit a0d40e2). Refloat's copy is a superset:
- extra `CFG_PARAM_foc_motor_*` enum entries are appended at the end (existing
  values unchanged) and are not used by the package;
- `thread_set_priority` / `shutdown_disable` (6.06+) are at the end of the
  interface table and the package NULL-checks `thread_set_priority` before use;
- the rest is only `const` qualifiers.
So the vendored header is safe for 6.05. Do not reorder or insert into
`vesc_c_if.h`; if you call any 6.06+ function, NULL-check it like main.c does.

## What was renamed
- `package_name` -> `Scott Wheelz` (drives the C `PACKAGE_NAME` macro and every
  `{{PACKAGE_NAME}}` in ui.qml.in, including thread names and UI text).
  This also stops VESC Tool's update check offering to "update" you to
  official Refloat, which would overwrite the fork.
- `pkgdesc.qml`, `Makefile`: package name and output `scottwheelz.vescpkg`.
- `src/conf/settings.xml`: config page is now "Scott Wheelz Cfg".
- `lisp/package.lisp`: BMS thread/log names.
- `version` -> `1.2.3-custom1`.

To rename again: edit `package_name`, then the hard-coded name in
`pkgdesc.qml`, `Makefile`, `settings.xml` <longName>, and `lisp/package.lisp`.

## Deliberately NOT changed
- Package ID `101` in `src/main.c` (custom app-data protocol). Keeping it means
  Refloat-aware apps/remotes keep working. Don't run Scott Wheelz and Refloat
  side by side. Change it only if you also want a new, incompatible protocol.
- C identifiers (`RefloatConfig`, `refloat_thd`, `confparser_*_refloatconfig`).
  Renaming them is churn with no runtime effect and makes upstream merges harder.
- GPL copyright headers. Keep them; the fork must stay GPL-3.0 with sources
  available to anyone you give the .vescpkg to.
- Some apps may identify the package by the name it reports (main.c ~line 2156).
  If an app stops recognizing the board, that's the first place to look.

## Building
Needs `arm-none-eabi-gcc` (GNU Arm Embedded) and `vesc_tool` on PATH
(vesc_tool generates config code from settings.xml and packs the .vescpkg):

    make VESC_TOOL=/path/to/vesc_tool

Install: VESC Tool > VESC Packages > load the custom `scottwheelz.vescpkg`.
Back up your XML config first. First test with the wheel off the ground.

## Scott Wheelz features

### Current layout (update 3)
There is no separate Board tab any more. Everything lives on the stock main
screen, restyled:
- Neon theme over the stock stats (all stock features kept): dark grid,
  green/purple colors via `swTheme`, red/orange alerts unchanged.
- Side view: neon tire, hub, fender, rails and footpads with NOSE/TAIL,
  plus the stock balance-pitch outline (dim purple) and setpoint (orange).
  Long-press it to flip direction (saved in `swSettings.invertPitch`).
- Footpad: neon outline, ADC1/ADC2 halves glow green and pulse when engaged.
- HORN pill next to the board name (hold). Riding: full haptics. Idle
  (READY): tone only, via foc_play_tone at 0 A, no vibration current.

### Haptic horn
- App: **HORN** pill next to the board name (hold). Enabled only while riding.
- Command `120` (`COMMAND_SW_HORN`), payload 1 byte: `1` = on/keepalive,
  `0` = off. The app resends every 100 ms while held; the package stops the
  horn by itself 0.3 s after the last request (`HORN_TIMEOUT`), so a dropped
  connection can't leave it stuck on.
- Plays the **Duty** haptic tone and the **Vibrate** tone from Haptic Feedback
  settings at full scale (1.0 = the strength normally reached at Max Strength
  Speed), ignoring current speed. If both strengths are 0, the horn is silent.
- Lowest priority: pushback, voltage/temperature and fatal-error haptics
  always override it. Idle (READY) plays the Duty tone only, never the
  current-based vibrate, so the wheel isn't pushed. Not active in Handtest
  or when the package is disabled.
- Code: `src/haptic_feedback.c/.h` (HAPTIC_FEEDBACK_HORN), `src/main.c`
  (command handler), `ui.qml.in` (`sendHorn`, hornButton, 10 Hz timer).

### Board orientation view
- App: **Board** tab shows side view (pitch), rear view (roll) and top view
  (yaw) drawn from the IMU's fused angles, updated at the existing 10 Hz
  realtime-data rate, with numeric readouts.
- `imu.yaw` was appended to `RT_DATA_ITEMS` in `src/rt_data.h` (items are
  matched by name, so appending is safe). Yaw is gyro-integrated and drifts.
- Neon Future theme on the Board tab: neon green + purple on a dark grid, detailed
  side view (tire tread, hub, fender, rails, bumpers, both footpads) with
  NOSE / TAIL labels, rear view, and top view with heading ticks.
- Front footpad panel: ADC1 (left half) and ADC2 (right half) light up green
  when the package considers that sensor engaged (`fpState`, which uses your
  fault_adc1 / fault_adc2 thresholds), with live voltages. The nose pad in the
  side and top views lights up too. NOSE = the sensor-pad end.
- "Invert direction" switch flips the drawing if your IMU is mounted so the
  board appears to tilt the wrong way (orange marker = front).

### Main screen side view
- The stock pitch drawing on the main (App UI) screen is now a detailed board:
  tire tread, hub/axle, fender, both footpads and NOSE / TAIL labels. The
  stock balance-pitch outline (accent) and setpoint lines (orange) are kept.
- The front pad and NOSE label switch to the accent color while ADC1 or ADC2
  is engaged. The Board tab's "Invert direction" switch applies here too.
- Code: `pitchCanvas.paintDetailedBoard` in ui.qml.in.

### Main screen footpad
- The stock footpad widget (top right of the main screen) now labels each half
  ADC1 / ADC2, and an engaged half gets a pulsing glow and a pulsing label, so
  it is obvious at a glance which sensor halves are active. Voltages below
  each half are unchanged.

### Fixes / theme (v1.2.3-sw1, update 2)
- Board tab pitch/roll drawings didn't move: inside the Board tab, the bare
  names `pitch`/`roll` resolved to the main screen's `id: pitch` / `id: roll`
  items instead of the angles, so the "has data" check was always false.
  Angles are now always read as `boardPage.*`, and the side and rear views
  rotate inside their canvases (like the main screen does).
- Main screen stats keep the stock layout but use the neon theme: `swTheme`
  maps the app's theme colors to green/purple (red and orange alerts kept),
  `SwNText`/`SwLText`/`SwDText` replace the stock text colors there, the Dial
  and StatusText components use it too, and a dark grid sits behind the stats.
  The battery bar fill is purple so the % stays readable; the balance-pitch
  outline and value are purple to stand apart from the green board.

### Horn safety limits (update 4)
Enforced in the package (`src/haptic_feedback.c`), mirrored in the UI:
- min 0.2 s per tap, max 1.0 s per press (holding longer is silent),
- 1.0 s cooldown after release (presses ignored), so <= ~50% duty,
- stops 0.3 s after the app's last keepalive (disconnect-safe),
- lowest priority: pushback / error haptics always win,
- idle (READY): audible tone only, no current-based vibration.
Settings it uses: Warning Audible Strength (V, 0-12, default 3) and
Vibrate Strength (A, 0-25, default 0 = off). Higher values mean more motor
current and heat, especially at standstill.

### Horn sound modes and external beeper (update 5)
- (Update 6: the mode chip was removed from the app, which now always sends
  BOTH = the regular haptic horn. The package still accepts the other modes.)
- Mode chip next to HORN (tap to cycle, remembered): BEEP (audible motor
  tone), BUZZ (vibration), B/B (alternates beep and buzz every 0.125 s),
  BOTH. Sent as byte [3] of command 120 (`HornMode`).
- Buzz is current-based, so it only plays while riding and only if Vibrate
  Strength > 0 (default is 0 = off). Otherwise BUZZ falls back to the beep and
  B/B becomes beep-pause-beep.
- If an external beeper is installed on the Servo/PPM pin and "Beeper" is
  enabled in settings, it sounds along with the horn. Real beeper alerts keep
  priority (beep_on/off with force = false).
- All horn limits (0.2 s min, 1 s max, 1 s cooldown, 0.3 s disconnect
  timeout, alerts first) apply to every mode.

### Full-screen main view and pull-up sheet (update 6, v1.2.3-sw6)
- Horn: one regular haptic horn (tone + vibrate while riding, tone only when
  idle); no mode chip. The HORN pill is larger (2.4 x 0.8 units).
- Battery bar fill: green >= 65%, yellow 35-64%, red < 35%. The % and voltage
  text have a black outline so they stay readable on any fill.
- The main stats now fill the whole screen (spacers spread the sections).
- Tunes / Control / Data moved into a pull-up sheet (`bottomSheet`) over the
  main view. Drag the handle bar to any height, tap it to open/close. Fully
  up it stops below the board name row, so the HORN stays reachable. Position
  is remembered in `swSettings.sheetFraction` / `sheetOpenFraction`.
- The sheet uses the neon theme: dark grid, purple tabs bar, green selected
  tab / sliders / checkbox (`Material.accent`), purple/green tune buttons.
  Plots get `neon: true` (dark background, purple axes and labels); series
  keep their own colors.

### Rename and neon battery bar (v1.2.3-rf1)
- Package renamed to **Rink-Float** (`package_name`, `pkgdesc.qml`,
  `Makefile`, `settings.xml` -> "Rink-Float Cfg", `lisp/package.lisp`, CI
  workflows, README, code comments). Output: `rinkfloat.vescpkg`.
- Battery bar: neon fill whose hue fades smoothly from green (100%) through
  yellow/orange to red (0%), `Qt.hsva(level * 140/360, ...)`, with a Glow that
  gets stronger as the level drops. Color changes animate over 1.5 s.
  (Replaces the 3-step green/yellow/red from update 6.)

### Neon Future speedo, TILT slider, big HORN bar (v1.2.3-rf2)
- Main screen order: battery bar, name row (bigger edit/settings buttons),
  Neon Future speedo, footpad + roll | side view, temps/status row, TILT slider,
  HORN bar, pull-up sheet.
- `NeonSpeedo` component: 40-segment neon arc for speed (green to 70%,
  purple to 90%, red above, lit segments glow), thin inner duty arc
  (purple, orange >= 80%, red >= 90%), scale labels, big glowing "Exan"
  digits with unit, ERPM and DUTY readouts, WHEELSLIP flash. The current and
  duty dials sit in its bottom corners (`hudDials`). Its height takes what's
  left on the screen (2.6-5.5 units).
- Left column: footpad + roll (bigger), Wh/mi, battery current. Right
  column: side view (bigger).
- HORN is now a full-width bar at the bottom (hold), same limits as before,
  faint neon glow that brightens while honking.
- TILT slider (`swTiltSlider`): +-4 deg, springs back to 0 on release, sent
  as `(set-remote-state x 0 0 0 0)` at 10 Hz like the Control tab's tilt
  (one final 0 after release). On first touch while not riding it sets
  Remote Tilt input to UART (config write). Firmware also hard-caps remote
  tilt at +-4 deg (`REMOTE_TILT_SAFE_MAX` in `src/remote.c`), whatever
  Tiltback Angle Limit says.

### Horn heat budget (v1.2.3-rf3)
Replaces the fixed 1 s max / 1 s cooldown (update 4):
- Per press: up to 3 s while riding, 2 s idle (READY).
- Heat budget: 3 s of horn; sounding uses it 1 s/s riding, 1.5 s/s idle;
  silence refills 1 s/s. So taps and holds are free while the long-run duty
  stays <= ~50% riding / ~40% idle. When it runs dry the press ends and new
  presses wait until 0.2 s is back. (`HORN_BUDGET`, `HORN_DRAIN_*`,
  `HORN_REFILL` in `src/haptic_feedback.c`.)
- Unchanged: 0.2 s minimum tap, 0.3 s disconnect timeout, lowest priority
  (pushback/alerts win), idle = tone only.
- App: HORN bar mirrors the budget with a gauge along its bottom edge
  (orange under 1 s), shows WAIT when empty and MAX past the per-press limit.

### Magnetic sheet, Neon Future tabs (v1.2.3-rf4)
- The pull-up sheet snaps fully up or fully down on release, whichever is
  closer; a tap on the handle toggles between the two.
- TUNES / CONTROL / DATA tab labels use the "Exan" digital font
  (`SwTabButton`).

### Rail shapes (v1.2.3-rf5)
- Side view rails are drawn from a profile (`railY(x)` in pitchCanvas)
  instead of the fixed stock path: STOCK (flat), WTF ("W": kicked ends,
  dips under the pads, hump over the tire), DUB TAIL (only the tail kicked),
  DOUBLE KICK (nose and tail kicked). Footpads and the balance outline
  follow the same shape.
- Pick it in Control tab > Rails, or tap the side view to cycle (name
  flashes). Long-press still flips direction. Saved in
  `swSettings.railStyle`. Display only; no effect on riding.

### Neon Future front (roll) view (v1.2.3-rf6)
- Redrawn in neon: green tire with tread lines and purple hub, purple nose
  block with a glowing lightbar, a fixed roll scale (ticks every 15 deg)
  with a green pointer. ROLL readout in the Exan font.
- The nose block slides along the tire with pitch (same sign as the side
  view, honors the invert setting): level = middle of the tire, nose up
  20 deg or more (tail down at rest) = top of the tire, nose down = lower.
  Everything still rotates with roll.

### Tire style (v1.2.3-rf7)
- Control tab > Tire: TREADED (tread ticks, the original look) or SLICK
  (smooth, slightly bolder ring in the side view; rounder, no tread lines in
  the front view). Saved in `swSettings.tireStyle`. Display only.

### Board Style tab (v1.2.3-rf8)
- New 4th sheet tab BOARD STYLE holds Rails, Tire and Side view
  (normal/flipped) options; removed from the Control tab. Tapping the side
  view no longer cycles rails (long-press still flips it). Tab labels are
  smaller (0.27 unit) so four fit.

### Fixes (v1.2.3-rf9)
- Horn could run 4-5 s at idle: a Bluetooth gap > 0.3 s ended the press and
  the next keepalive started a new one (resetting the max-time clock).
  COMMAND_SW_HORN byte [2] is now a `HornRequest`: 0 off, 1 keepalive
  (only keeps an existing press alive), 2 start. HORN_TIMEOUT 0.3 -> 0.5 s.
  The app also sends "off" when its own per-press timer runs out.
- Sheet: at most 1/3 of the screen. The main view now lays out above the
  sheet's current height instead of being covered: the speedo shrinks (down
  to 1.8 units, corner dials hidden below 2.6) so battery, name row, stats,
  temps, TILT and HORN stay visible.
- Rails: DOUBLE KICK removed (a saved value falls back to STOCK). WTF and
  DUB TAIL are straight segments with sharp kinks (`railY` uses linear
  interpolation between knots).

### Fix: UI tab missing in rf9 (v1.2.3-rf10)
- rf9 made the main view's bottom margin depend on the sheet height while
  the sheet's max height was computed from the main view's laid-out
  heights - a layout feedback loop. It rendered on desktop Qt but VESC Tool
  on the phone never showed the tab. The sheet now uses only fixed,
  unit-based sizes (`hudFixedHeight`), `minHeight` uses implicit heights,
  and rail profiles use a plain helper (`railLerp`) instead of a nested
  function.
- CI: `QML lint` workflow syntax-checks ui.qml and loads it in qmlscene
  with stub VESC Tool modules (tools/qmltest/run.sh), publishing a
  screenshot to the `qml-shots` branch. `Release a commit` workflow builds
  any commit as its own fixed-download release.

### Sheet up to 2/5, main view scales (v1.2.3-rf11)
- Sheet max height is 40% of the screen. The main view keeps its normal
  layout (sized for the collapsed sheet) and is scaled down uniformly from
  the top center (`swHudTop.fitScale`, a Scale transform) to fit above the
  sheet, so everything stays visible and usable, just smaller. The grid
  background follows the visible area.

### Rails: STRAIGHT or WTF (v1.2.3-rf12)
- DUB TAIL removed (a saved value falls back to STRAIGHT). WTF redrawn from
  a reference image: slimmer rail, center raised over the tire, shallow
  dips at about +-33, ends level again, straight segments between kinks,
  ends cut at an angle. Footpads sit on the slimmer rail.

### Rename to Re-RinkFloat, README credits (v1.2.3-rf14)
- Package name is now **Re-RinkFloat** (`package_name`, `pkgdesc.qml`,
  `Makefile`, `settings.xml` "Re-RinkFloat Cfg", `lisp/package.lisp`, UI
  text, comments). Output file is still `rinkfloat.vescpkg`; app settings
  category is still `scottWheelz`.
- `package_README.md` (shown by VESC Tool when the package loads) now
  lists the Re-RinkFloat additions and says the fork was vibe-coded on top
  of Refloat, with all credit to Lukas Hrazky (lukash).

### Side view cleanup (v1.2.3-rf15)
- Removed the green tail footpad line; only the nose pad (lights up when
  ADC1/ADC2 engaged) is drawn.
- Removed the flip feature (long-press and the Board Style "Side view"
  option). Side and front views always use the IMU pitch sign as-is.

### Battery % (v1.2.3-rf16)
- Battery % uses the Exan digital font in light purple with a dark outline
  and a neon purple Glow behind it.

### Collapsing sheet, bigger horn (v1.2.3-rf17)
- The pull-up panel (SUW) collapses all the way down to just the handle
  bar. It is magnetic: it snaps fully open or fully closed.
- HORN bar is larger, with a small gap above it.
- Board name pencil and settings gear restyled as neon round buttons.

### Neon home labels, engine icon, ESC (v1.2.3-rf18)
- BATTERY AMPS, PITCH, BALANCE, MOTOR, ESC, TRIP and LIFE use the digital
  speedo font in neon purple. MOTOR is shown as an engine icon,
  CONTROLLER is now ESC.

### Neon Future makeover (v1.2.3-rf19)
- Neon digital fonts for CURRENT, DUTY, WH/MI and TILT, neon tabs, and a
  purple/green MAGIC FLYWHEEL button.
- TUNES presets are big rounded neon buttons; long-press one to pick its
  neon color.

### Vertical move slider (v1.2.3-rf20)
- CONTROL tab slider is vertical: up = forward, down = backward. Works the
  same as before and springs back to 0.

### Ride peaks, 40 mph speedo (v1.2.3-rf21, rf22)
- TOP SPEED and MAX DUTY for the ride (long-press to reset).
- The speedo digits read any speed; the segmented gauge fills up to 40 mph.
- Peaks ignore wheelslip / free spin so a spinning tire doesn't count.

### ATOM rails (v1.2.3-rf23, rf25)
- Board Style: ATOM truss rails for the side view, green wheel center kept.
- rf25 removed the purple fender over the ATOM rails.

### Bigger speedo (v1.2.3-rf24)
- Larger dial. TRIP and LIFE sit small and see-through inside it; TOP
  SPEED and MAX DUTY get their own digital boxes in the top corners.

### Side view position, FLIP (v1.2.3-rf26, rf27)
- Side profile view moved up to line up with the footpad / roll row.
- Board Style FLIP is back: mirrors the drawing for boards mounted the
  other way, while the nose always stays on the same side.

### DATA graph / text, neon name and voltage (v1.2.3-rf28 to rf30)
- DATA tab: GRAPH / TEXT switch (live chart or plain values).
- Board name in neon green digital font; battery voltage in glowing purple.

### Web simulator, sharper rendering (v1.2.3-rf31)
- `tools/simulator/index.html`: stand-alone browser simulator of the home
  screen and panel, kept in step with the package.
- Package drawings rendered sharper to match the simulator.

### Rename to Re-Float Rink-Mixed (v1.2.3-rf32)
- New name and RE:FLOAT — RINK-MIXED title strip; the config tab is back
  to Refloat's own "Refloat Cfg".
- Rink-Mixed phone app started in `apps/rinkmixed` (Flutter, phase 1:
  connect + live dashboard).

### Free-spin limiter (v1.2.3-rf33, rf34)
- CONTROL tab: optional top-speed limit that only acts while the tire is
  free-spinning (wheelslip). Warning pop-up first; minimum 40 mph
  (64.3 km/h). A smooth governor holds the tire near the limit instead of
  stopping it, and normal braking / deceleration always goes through.

### Home layout, °C / °F (v1.2.3-rf35, rf36)
- WH/MI, BATTERY AMPS, PITCH and BALANCE evenly spaced.
- Tap the engine icon to switch temperatures between °C and °F.

### Horn out of the box, horn settings (v1.2.3-rf37 to rf39)
- Horn uses its own minimums (beep 2.5 V, vibrate 8 A) when the Haptic
  Feedback strengths are lower (Refloat's default Vibrate Strength is 0).
- HORN SETTINGS in the CONTROL tab, used only for the horn (not for
  pushback/alerts), with up/down arrows and safe limits.
- SIMPLE mode (default): STRENGTH, VOLUME, PITCH. ADVANCED mode:
  beep 0.5-6 V, pitch 300-1000 Hz, buzz 0-15 A, buzz 30-150 Hz.

### Boot melody (v1.2.3-rf40)
- Plays when the board finishes starting up (READY): pitch 1, 2, 3, half a
  second each. OFF / DEFAULT / CUSTOM. Horn settings and melody are saved
  on the board (EEPROM, never written while riding).
- Volume rescaled: the old 5/10 is now 10/10.

### Custom melody pad (v1.2.3-rf41, rf42)
- CUSTOM MELODY button opens a half-screen tile keyboard: 10 tiles, pitch
  1 (low) to 10 (high). Hold a tile to record that note for as long as you
  hold it. UNDO / CLEAR / PLAY / DONE. Up to 6 notes.

### Longer melodies, tile preview, Neon Future name (v1.2.3-rf43)
- Melody can be up to 5 s total; each note is at most 2 s, then it ends
  (a short break is always left between notes).
- Every tile tap plays its note on the board so you can hear it, even once
  the melody is full (then it's a preview and isn't recorded). The board
  only makes sound while idle (READY), and a held tile stops after 2 s.
- "Tron" renamed to "Neon Future" in the docs, code comments and component
  names. No look changes.

### MIXER tab (v1.2.3-rf44)
- New MIXER tab in the panel (TUNES / CONTROL / MIXER / DATA / BOARD
  STYLE) with the 10 note tiles. The panel opens to half the screen on
  this tab.
- LIVE mode: tap or hold tiles and the board plays the notes (idle /
  READY only, 2 s max per hold). Nothing is recorded.
- REC mode: the custom melody recorder (timeline, UNDO / CLEAR / PLAY /
  DONE, 6 notes / 5 s / 2 s per note). The CONTROL tab's CUSTOM MELODY
  button jumps to MIXER in REC mode; DONE sets it as the boot melody and
  goes back to LIVE. The old pop-up pad is gone.

### Longer melodies, saved melodies, sound budget (v1.2.3-rf45)
- Custom melody: up to 32 notes and 10 s total (was 6 notes / 5 s); each
  note still at most 2 s, with a short break between notes. The note count
  is set by board storage (the melody is now in EEPROM words 100-115;
  older 6-note saves are read and moved over automatically).
- Sound budget on the board, shared by melodies and LIVE tiles: 10 s of
  sound, refilled at half speed while quiet. If it runs out (non-stop
  jamming), tones pause until 3 s is back. Tones stay at the horn's beep
  voltage (max 6 V) and only play while idle (READY).
- MIXER REC: SAVE names the melody and keeps it in the app (up to 12).
- CONTROL ▸ BOOT MELODY lists saved melodies: tap a name to make it the
  boot melody, ✎ to edit it in the MIXER, ✕ (tap twice) to delete.

### Real notes, iOS-style melody recorder (v1.2.3-rf46)
- The 10 tiles are real musical notes now (equal temperament, A4 = 440 Hz):
  the white keys E4 F4 G4 A4 B4 C5 D5 E5 F5 G5 (330-784 Hz), labelled by
  name. Melodies use the same notes (older ones keep their tile numbers and
  shift slightly in pitch). Enough for tunes like the Super Mario intro.
- MIXER REC works like making a custom vibration on iOS: the first tap
  starts a 5 s recording, a playhead sweeps across the timeline at a steady
  rate, and each tap lands where it happened, so the gaps become rests. It
  stops by itself at 5 s (or STOP). Each note still stops at 2 s, like the
  horn. CLEAR gets ready for a new take; PLAY shows a playhead too.
- Board: melody = up to 64 notes/rests, 5 s total (stored in EEPROM words
  88-119; rf45 and older saves are moved over automatically). Rests are not
  limited to 2 s. The sound budget from rf45 still applies.

### Original boot melody restored (v1.2.3-rf47)
- The DEFAULT boot melody is back to the original rf40 sound: 300, 375 and
  450 Hz, half a second each, as one smooth rising tone (no gaps). It no
  longer follows the MIXER note tiles, which had made it E4 F4 G4 in rf46.
  Custom melodies keep the real notes and the short gaps between notes.

### Fixes: boot melody save, horn settings, tune buttons; 4 s recorder (v1.2.3-rf48)
- Custom boot melody now survives a power cycle. Unused (zero) melody
  words are never written to the EEPROM, and reading an unwritten variable
  fails; the loader treated that as "no saved settings" and fell back to
  the defaults. Missing words now read as empty, the magic word is written
  last, and a failed save is retried.
- Horn settings apply reliably: each arrow tap sends the new settings at
  once and again 0.3 s later, the big melody packet is only sent when the
  melody changes (or every 10 s) instead of with every setting, and
  changing VOLUME / PITCH (or beep V / Hz) plays a short preview beep when
  the board is idle. The buzz is only felt while riding; labels say so.
- Tune buttons: opaque background so the glow can't wash the name out
  (some phones rendered them solid), and only one tune shows ACTIVE: the
  last one applied from the app if it still matches, else the first match
  (in VESC Tool's limited mode every tune looked like a match).
- MIXER recorder window is 4 s (board limit 4 s).

### Safer save retry (v1.2.3-rf49)
- A failed horn/melody save is retried at most 3 times (rf48 kept retrying
  every 2 s), so the board never keeps stalling on flash writes, which can
  interrupt VESC Tool reading the package UI over Bluetooth.

### Save retry cap (v1.2.3-rf49), no horn preview beep (v1.2.3-rf50)
- rf49: a failed horn/melody save is retried at most 3 times.
- rf50: removed the rf48 preview beep after changing horn settings. It
  was a delayed horn press from the app, so going back and forth between
  the HORN button and the settings made the horn seem to fire on its own,
  and it used up the board's horn time without the app knowing. Only the
  HORN button sounds the horn now; setting changes are still sent at once
  plus one retry.

### MY MELODIES window (v1.2.3-rf51)
- The saved-melody list moved off the CONTROL tab into its own MY MELODIES
  window, so CONTROL stays short. BOOT MELODY now shows OFF / DEFAULT /
  CUSTOM, the name of the custom melody in use, and MY MELODIES (count),
  RECORD and TEST buttons.
- MY MELODIES: tap a melody to use it at bootup, pencil to re-record it,
  x twice to delete; DEFAULT 1-2-3 and + RECORD NEW at the bottom.

### Lights moved to BOARD STYLE (v1.2.3-rf52)
- The Lights / Headlights buttons moved from the CONTROL tab to the bottom
  of BOARD STYLE, as neon toggle pills (LIGHTS ON/OFF, HEADLIGHTS ON/OFF;
  green = on). Still only shown when the board reports lights.

### Neon green headings (v1.2.3-rf53)
- The white section headings (Move, Tilt, Rails, Tire, View, Lights) are
  now neon green in the digital font, like the rest of the UI.
