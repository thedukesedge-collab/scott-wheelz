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

### Lights reach the light module (ALCM / LCM) (v1.2.3-rf54)
- HEADLIGHTS OFF now sends headlight brightness 0 to an external light
  module. Before, it sent the "front strip" brightness, whose default
  (50%) equals the headlight default, so ALCM saw no change. LIGHTS OFF
  still sends 0 for headlight and status bar.
- Changing Refloat Cfg > Hardware > LED Mode to/from External takes effect
  when the config is written (was: only after a power cycle).
- BOARD STYLE > Lights shows a hint when LED Mode isn't External, since a
  light module only follows the buttons in that mode.
- Checked against ALCM v0.1.1 (from its .hex): it reads headlight
  brightness (poll byte 12) and status-bar brightness (byte 14), 0-100%,
  applying changes after the first value it sees; it ignores the idle
  brightness and any extra payload bytes, and drops packets > 32 bytes.

### LEDS tab + ALCM VP light module firmware (v1.2.3-rf55)
- New LEDS tab in the panel (after BOARD STYLE): headlights on/off and
  brightness, status bar on/off with separate idle and riding brightness,
  the idle / riding / snooze / boot / shutdown animations, the personal
  color, and the light module's beeper. The Lights buttons on BOARD STYLE
  are gone (the LEDS tab replaces them).
- The package stores these in EEPROM words 80-82 (COMMAND_LCM_VP_SETTINGS =
  126) and appends them to every LCM poll reply as a "VP block", whatever
  the LED Mode, so they apply even without the phone. Nothing is sent until
  the LEDS tab is used once.
- lcm/alcm: ALCM v0.1.1 (MrTheConductor/alcm, GPL-3.0) with VP light control:
  reads the VP block, the button keeps only power off (hold) + factory reset,
  clicks / config mode / footpad adjustments are gone. GCC build + ALCM's
  unit tests (stock and VP) in CI; ALCM_VP.hex on the lcm-latest release.
  See lcm/README.md.

### LEDS tab compatibility notice (v1.2.3-rf56)
- The top of the LEDS tab now shows an amber "UPDATED LCM FIRMWARE REQUIRED"
  notice: the settings only work on ADV200 controllers (XRV, GTV, PintV) with
  the light module flashed to the ALCM VP firmware, and do nothing elsewhere.
- Same notice in the simulator and in the package README.

### ALCM VP: no beeping fault on package install (lcm-latest, 2026-11-17)
- Installing/updating the package (or a VESC reboot) stops the VESC answering
  the LCM for a few seconds. Stock ALCM latches an internal fault (continuous
  beep + fault animation) that only a button press (power off) clears.
- VP build: a comm timeout is ignored unless riding (board stays on, carries
  on when the VESC answers). Mid-ride it is still a warning fault but clears
  itself on the next answer. Other internal faults are unchanged.
- Test: tests/board_mode_vp_main.c (BoardModeTestVP). Size 16,068 / 16,320 B.

### LEDS windows, idle/riding headlights, test beep, Refloat sync (v1.2.3-rf57)
- LEDS tab: four tiles (Headlights, Status bar, Animations, Sound); each opens
  a full-screen window. Same in the simulator.
- Headlights now have RIDING and IDLE brightness. The package sends the level
  for the current state (byte 12) like the status bar, plus the idle level
  (byte 13). COMMAND_LCM_VP_SETTINGS (126) is 12 bytes (11 still accepted,
  idle = riding); EEPROM word 83 = magic 0x49 + idle % (missing = riding).
- Test beep: COMMAND_LCM_TEST_BEEP (127, no data) toggles VP flag bit 3; the
  ALCM VP build plays one 160 ms beep per toggle (needs the beeper on).
- Two-way sync with Refloat's LED config: headlights on <-> leds.headlights_on,
  riding % <-> leds.headlights.brightness, idle % <-> leds.front.brightness,
  status idle % <-> leds.status_idle.brightness, status riding % <->
  leds.status.brightness_headlights_on/_off. Written 1.5 s after the last
  change; read back whenever the config loads or is written in Refloat Cfg.
- leds.* are no longer part of tunes (not saved, not matched for ACTIVE, not
  applied from older tunes that contain them). Full backups still include them.

### ALCM VP: battery gauge colors (lcm-latest, 2026-11-17)
- Idle status bar = battery gauge, 1 LED per 10 % (unchanged). Colors now:
  white above 35 %, yellow 35-20 %, red 20-5 %, flashing red at 5 % and
  below (stock: orange at 15 %). While riding the gauge shows from 35 % down.
- Accuracy depends on the VESC's battery % (Motor Cfg > Additional Info >
  battery cells in series / type) unless a battery LUT is patched in.
- Test: StatusLedsTestVP. Size 16,136 / 16,320 B.
- Updated: white above 55 %, yellow 55-41 %, orange 40-30 %, red below 30 %,
  flashing red at 5 % and below; shown while riding from 40 % down.

### LCM bootloader: LCM firmware updates from the app (v1.2.3-rf58)
- Started on the PC (branch `lcm-bootloader`), finished here. Details,
  protocol, flash map and first-install steps: `lcm/bootloader/README.md`.
- 1.5 KB bootloader at 0x08000000, app at 0x08000600 (14720 bytes max),
  battery LUT page never written. No chip resets (PA2 holds the power);
  page 0 written last and back to front; per-page read-back + image CRC;
  max 3 bad images per power-up; button hold 2 s / 5 min idle = power off.
  Host tests: power cut at every flash op never leaves a partial app; an
  end-to-end phone -> package -> bootloader run.
- Package: COMMAND_LCM_FW (128) relay in src/lcm_fw.c, VP flag 0x10 (only
  while not running); two 128-byte pages buffered.
- App: LEDS tab > LCM FIRMWARE tile (UPDATE LCM, progress, cancel). The
  package build compiles the LCM firmware and embeds the update image
  (`make LCM_FW=0` to skip).
- ALCM (all builds, no behavior change): small division / mem* routines
  (tiny_rt.c, host-checked against the real ones), exact 32-bit ratio in the
  function generator, USART/EXTI/NVIC set up with direct register writes:
  16140 -> 14960 bytes. Bootloader build only: battery LUT patch left out
  (battery % from the VESC).
- Releases (lcm-latest): ALCM_VP.hex (no bootloader), ALCM_VP_BL_full.hex
  (ST-Link, once), ALCM_VP_app.bin.

### FLASH LCM warning, ADV200 (v1.2.3-rf59)
- The LCM update button is now FLASH LCM and opens a warning that must be
  ticked ("I understand and my board is idle on a stand") before it starts.
- ADV100 corrected to ADV200 everywhere (XRV, GTV, PintV).
- Checked: the STOCK LCM firmware cannot be flashed or changed from the
  package. Its source (floatwheel LCM) has no flash-write / update code, it
  only polls COMM_GET_VALUES (answered by the VESC firmware, not the
  package), and its UART receive (8-bit index into a 256-byte buffer) has no
  overflow to exploit. The HK32F030M's own boot modes need hardware pins,
  not reachable over the VESC link. So the ALCM VP bootloader still needs one
  ST-Link flash per board.

### Battery curve for the LCM status bar (v1.2.3-rf60)
- LEDS tab > BATTERY GAUGE: battery curve on/off (off = the VESC's own %),
  presets from floatwheel's own tables (P42A, 50S, DG40, VTC6, RS50) or a
  custom curve (11 points, cell volts at 100..0 %, 10 mV steps), sag
  compensation (pack resistance), live readout (V, cells, V/cell, %).
- The package computes the % (src/battery_curve.c: per-cell voltage from
  the VESC's filtered input voltage + I x R, smoothed, interpolated) and
  sends it as byte 25 of the VP block (0xFF = none). COMMAND_LCM_BATTERY
  (129): 0 = get, 1 = set; stored in EEPROM words 84-87 (magic 0x42).
- ALCM VP (both builds): uses that % instead of the VESC's; falls back to
  the VESC's % when the byte is 0xFF or the VP block stops coming. Replaces
  the LUT patch tool for the bootloader build (no flashing to change the
  curve). Cells: VESC motor config (battery cells in series).

### Half-height LED panels, FLASH LCM from a .hex file (v1.2.3-rf61)
- LEDS tiles unchanged; each now opens a half-height panel from the bottom
  with a BACK button (tapping above it closes it too).
- LCM FIRMWARE: "Firmware to flash" = BUILT-IN or PICK .HEX FILE. The app
  parses the Intel .hex itself (line checksums), keeps only the app area
  (0x08000600-0x08003F7F; a full BL image's bootloader part is ignored),
  refuses anything on the battery LUT page or without a valid ALCM vector
  table there (a plain non-bootloader .hex), pads to 128-byte pages and
  computes the CRC like the package build. CI (lcm.yml) runs the app's own
  parser on the freshly built ALCM_VP_BL_full.hex and checks it gives the
  same update image (tools/hexparse/test.js). The FLASH LCM warning names
  the firmware about to be sent.

### Battery gauge: simple + advanced modes, more cells (v1.2.3-rf62)
- Simple mode (half panel): curve on/off, 1. cell size (18650 / 21700),
  2. cell (18650: P28A, P30B, VTC6; 21700: P42A, P45B, P50B, 50S, RS50,
  DG40, plus MY CELL), 3. pack parallel groups (1-4P), 4. gauge style
  (conservative = every point 30 mV higher so it reads lower; balanced;
  optimistic = 30 mV lower). Sag compensation is automatic: cell DC
  resistance x series (from the VESC) / parallel.
- Advanced mode (full screen): my own cell (name, size, DC resistance,
  capacity, 11-point curve, start from any known cell), USE MY CELL, sag
  compensation AUTO / MANUAL.
- Curves: floatwheel's tables for P42A, 50S, DG40, VTC6, RS50. Molicel
  publishes no OCV tables, so P28A / P30B / P45B / P50B use the P42A table
  (same maker) and are marked approximate. DC resistance from Molicel
  datasheets: P28A 20, P30B 17, P45B 15, P50B 12.8 mOhm; others estimated.
- Package unchanged (the app sends the resulting points + resistance).

### LCM firmware: tucked away, auto-detected, fail-safe (v1.2.3-rf63)
- The LCM FIRMWARE tile is gone from the LEDS grid. The tools sit behind a
  dim "LCM firmware tools" link at the very bottom of the LEDS tab that
  opens only after a 2 s press and hold (a tap just says "HOLD 2 s TO
  OPEN"). While an update runs the link shows its progress and opens at
  once.
- The ALCM VP firmware names itself in its LCM_POLL packets (Refloat's own
  name field): `VPBL-<hash>` built for the bootloader, `VP-<hash>` without
  it. `<hash>` is the git tree hash of `lcm/alcm` (`git rev-parse
  --short=7 HEAD:lcm/alcm`), so it changes only when the LCM code does,
  and the package's embedded image carries the same hash as its version.
  `lcm/tools/lcm_tag.py` precomputes the packet bytes and CRC (the BL app
  has ~30 bytes free; the name costs ~12).
- Package: the LCM's name is stored whatever the LED mode (stock Refloat
  only did with LCM lights enabled, and read one byte past the payload)
  and always reported by COMMAND_LCM_DEVICE_INFO (27).
- Fail-safe in the package (lcm_fw.c): START is refused with "no
  bootloader" unless the LCM named itself `VPBL-...`, or a bootloader asked
  for a page in the last 5 s (recovery after an interrupted update, when
  there's no app to name itself). An LCM without the bootloader is never
  sent the update flag. STATUS gains a flags byte (14 bytes): bit0 a
  bootloader is asking, bit1 the LCM is VPBL.
- The app polls the name and STATUS while the tools are open and shows what
  is fitted: VPBL (with "up to date" / "this package carries build ..."),
  VP without bootloader, other/stock, none, or bootloader waiting
  (recovery). FLASH LCM and the firmware picker exist only for VPBL or
  recovery; otherwise it explains the one-time ST-Link install.
- Every package build embeds the bootloader-build LCM image built from the
  same commit (`make`, `LCM_FW ?= 1`), so app, package and LCM stay
  matched.

### Boot melody volume, CHECK LCM before flashing (v1.2.3-rf64)
- Boot melody has its own MELODY VOLUME / INTENSITY (CONTROL > BOOT
  MELODY), 1-10 = 0.25-2.5 V beep, default 6 (1.5 V). 10 is half the
  horn's 5 V maximum on purpose; the package caps it at 2.5 V
  (BOOT_MELODY_MAX_V) whatever it's sent. Also used for TEST and the MIXER
  note tiles. COMMAND_HORN_SETTINGS byte 9 (0.1 V); stored in horn EEPROM
  word 0 bits 18-22 (0 = older save: horn volume, capped).
- MY MELODIES: a copy button copies a melody as text
  (`RFMELODY <name> [[level, ms], ...]`) to share it.
- LCM firmware tools: no automatic detection. CHECK LCM must be pressed;
  it asks the package a few times (~2 s). Passed (VPBL, or a bootloader
  waiting): a popup confirms it and offers CONTINUE TO FLASH, which reveals
  the firmware choice and FLASH LCM (then the usual acknowledgement).
  Failed: a popup explains the LCM firmware most likely has no bootloader
  for flashing over BLE and must be flashed once with an ST-Link
  (ALCM_VP_BL_full.hex), plus what was seen. Closing the window forgets
  the check. The package-side fail-safe from rf63 is unchanged.

### CHECK LCM also checks the version / best fit (v1.2.3-rf65)
- LCM firmware version is now `r<N>-<hash>` (lcm/tools/lcm_version.sh): N =
  commits that changed lcm/alcm (only goes up), hash = git tree hash of
  lcm/alcm. The LCM names itself `VPBL-r<N>-<hash>` / `VP-r<N>-<hash>`,
  and the package's embedded image carries the same version (CI checks out
  the full history for N; a shallow clone gives r0 = compare hash only).
- After a passed check the app compares the LCM's version with the one this
  package carries (the LCM firmware built and tested with this package):
  UP TO DATE · BEST FIT (exact match: REFLASH ANYWAY), UPDATE AVAILABLE
  (older: UPDATE LCM, recommended), LCM NEWER THAN THIS PACKAGE (best fit:
  update the VESC package; DOWNGRADE ANYWAY), DIFFERENT BUILD (same
  revision, other code, e.g. custom: FLASH PACKAGE BUILD). rf63/rf64 LCM
  builds (hash-only names) count as older.

### Animation names light up like the animation (v1.2.3-rf66)
- LEDS > ANIMATIONS: each animation's name is drawn letter by letter as if
  the letters were the status bar LEDs (AnimName in ui.qml.in, following
  status_leds.c): RAINBOW SCAN a sweeping spot cycling hues, RAINBOW MIRROR
  a mirrored flowing rainbow, KNIGHT RIDER a red scanner, RAINBOW BAR a
  scrolling rainbow, THE FUZZ red/blue strobes, FIRE flicker, PULSE OUT/IN
  the personal color pulsing out from / in to the centre, 120 SCROLL and
  COMP. WAVE gradients from the personal color, CLASSIC a left-to-right
  fill, COLOR SCAN a personal-color scanner, STANDARD plain white. A
  blurred copy of the letters behind them gives the neon glow. Animates
  only while the window is open.

### SLCM name, works on any controller (v1.2.3-rf67)
- Our ALCM-based light module firmware is now called **SLCM** (Smart LCM) in
  the app. File names (ALCM_VP_BL_full.hex etc.) unchanged for now.
- Package: the VP block (and the app's brightness values) only go to an LCM
  that named itself SLCM ("VP-..."/"VPBL-..." in its poll). Any other LCM
  gets exactly stock Refloat's poll reply again, whatever was set in the
  LEDS tab (before, a stock / third-party LCM got the extra VP bytes once
  the LEDS tab had been used).
- App, LEDS tab: Refloat's own LIGHTS ON/OFF and HEADLIGHTS ON/OFF buttons
  are back (they were dropped when the LEDS tab replaced them in rf~53):
  they work on every controller (Thor, Floatwheel, ...) like stock Refloat.
  Headlight on/off and headlight / status bar brightness keep syncing with
  Refloat's LED config, so they work on any controller too.
- SLCM detection: while the LEDS tab is open the app asks the package for
  the LCM's name every 3 s. Banner: SLCM DETECTED (with version) or NO
  SLCM DETECTED (explains what works anyway). ANIMATIONS, SOUND and BATTERY
  GAUGE tiles carry an SLCM badge and are dimmed without one. No controller
  picker: what matters is whether an SLCM answers, not the controller.

### Rider's melody as the default, better LCM check diagnosis (v1.2.3-rf68)
- The default boot melody is now the rider's "Melody 2" (16 notes/rests,
  3.46 s; table in haptic_feedback.c), played like a custom melody (short
  gaps). "DEFAULT 1-2-3" is now "DEFAULT MELODY".
- Boot melody / note tiles: the 0.5 V floor is now 0.2 V, so volume levels
  1 and 2 (0.25 / 0.5 V) really differ.
- Package: STATUS flags bit2 = an LCM polled in the last 2 s (any firmware,
  named or not), so a failed CHECK LCM can say which case it is: no LCM
  talking (stock Floatwheel LCM firmware never polls, or not connected) vs
  an LCM polling without a name (pre-rf63 SLCM/ALCM VP, original ALCM,
  other firmware). The failure also names the SLCM build the package
  carries.

### Lights: off really off, running / riding lights, snooze breathing, battery gauge animation (v1.2.3-rf69)
- Status bar animations: what "0" does now has a name per context: IDLE /
  BOOT / SHUTDOWN = STANDARD (battery + footpads), RIDING / SNOOZE = OFF
  (riding still warns on a low battery).
- New animation BATTERY LEVEL (13) for any context (SLCM firmware change:
  ANIMATION_OPTION_BATTERY; riding shows it live). Package accepts 0-13.
- Headlights: HEADLIGHTS OFF is now fully off. The package sends 0
  brightness (riding and idle) when off - for the SLCM and, in the stock
  path, for any LCM (Refloat used to keep sending the "front" / idle level,
  which is why "off" only dimmed at idle). Riding / idle brightness are now
  RIDING LIGHTS / RUNNING LIGHTS (IDLE), 0 = OFF.
- SNOOZE BREATHING on/off: VP flag 0x20 (the SLCM already supported it, the
  package now passes and stores it).

### refloatrm, lighter package, older SLCMs back, quick light switches, neon dialogs (v1.2.3-rf70)
- The package file is now **refloatrm.vescpkg** (release asset too).
- The SLCM firmware image is no longer embedded (it made the package heavy
  and slow to install / load). The package carries only the SLCM version
  it goes with (for CHECK LCM); to flash, the rider picks
  ALCM_VP_BL_full.hex from the lcm-latest release. The app reads the SLCM
  version out of the picked .hex ("VPBL-r<N>-<hash>"). `make LCM_FW=1`
  still embeds the image.
- Fix (regression from rf67): an LCM that polls without naming itself
  (SLCM / ALCM VP builds from before r15) gets the VP block again, so its
  LED settings work again. Only an LCM that names itself as something else
  gets the plain stock reply. CHECK LCM shows such an LCM as "older SLCM"
  and offers TRY UPDATE (safe: a build without the bootloader ignores the
  update flag and the package gives up after 15 s); the package accepts
  START for an unnamed LCM.
- LEDS > Lights: HEAD/TAIL, STATUS and ALL LIGHTS quick switches (any
  controller; riding and idle). They override the other light settings
  (brightness etc. are kept). ALL switches both (and Refloat's lights).
  A small glowing neon-orange note next to the board name on the dashboard
  says which lights are off.
- Neon dialogs: every dialog (BaseDialog) has the dark neon panel, purple
  frame and a neon green title. Board name (pencil): neon input, counter,
  CANCEL / SAVE pills. Settings (gear): SETUP / PREFERENCES pills, sections
  as framed cards (Package, Handtest, Backup / Restore with one row per
  backup; Dashboard, Tunes, Data & App), neon buttons, CLOSE pill.
- lcm.yml: apt install retried with a timeout (a stuck mirror hung runs).

### Power off from the app removed (v1.2.3-rf78)
- The package is for a working light module, where the button powers the
  board off, so the app POWER OFF (rf74, kept in rf77 as S5) is gone: it was
  the only way software could release the board's power. The LEDS tab has no
  POWER OFF control; COMMAND_LCM_POWER_OFF (130) is accepted and ignored
  (older apps); the package never sets VP flag bit 6 (lcm_vp_test checks it,
  idle and riding). SLCM r22's handler stays until the next SLCM build (it
  only acts on that bit, which nothing sends now).
- tools/lcm_vp_test runs in CI (it broke unnoticed in rf77: the stub had no
  min(); added).

### SLCM presets back in STATUS BAR (v1.2.3-rf115)
- On an SLCM board (SLCM answering, no controller-driven status strip:
  ledSender.slcmPresets) the STATUS BAR window shows the SLCM's own presets
  again, like when the SLCM was first tied in: IDLE / RIDING / SNOOZE /
  BOOT / SHUTDOWN rows (rainbow scan, fire, knight rider, ... with live
  previews), COLOR and DESIGN CUSTOM. IDLE keeps STATUS BAR (Refloat's
  Status Idle animation) as one choice. Controller-driven LEDs (Thor) keep
  the Refloat idle / snooze / riding settings.

### Riding and snooze animations: own colors / speed (v1.2.3-rf114)
- Riding animation now has the same adjustments as the idle animation:
  COLOR 1, COLOR 2 / BACKGROUND (ON / OFF) and SPEED (RfColorsBlock),
  starting from the idle colors the first time.
- STATUS BAR > Snooze: SIMPLE (default; the idle animation's colors /
  speed) or ADVANCED > (its own COLOR 1 / BACKGROUND / SPEED) when an
  animation is picked.
- Package: bytes 24-29 of COMMAND_LCM_VP_SETTINGS (ride c1, c2, speed in
  0.1/s; snooze c1, c2, speed; speed 0 = use the Status Idle colors /
  speed), stored in a 7th EEPROM word (127; older saves load as 0 =
  idle colors). leds.c overrides the LedBar colors / speed when set.
  Unit test covers clamping, keeping on short commands and pack / unpack.

### Pointing-finger hint hand (v1.2.3-rf113)
- The swipe-up hint's hand is now a proper pointing hand: index finger
  down, curled fingers, thumb and cuff (Canvas, drawn on a 40 x 56 grid;
  SVG in the simulator).

### Snooze controls look like the SLCM ANIMATIONS rows (v1.2.3-rf112)
- STATUS BAR > Snooze and HEAD/TAIL LIGHTS > SNOOZE LOOK are labelled rows
  with the animated name on the right, the same as the SLCM ANIMATIONS
  window (IDLE / RIDING / SNOOZE ...). BREATHE / DIM / DARK previews breathe,
  dim or go dark (AnimName rf mode -2 / -3 / -4). An SLCM only offers
  BREATHE / DARK (DIM is skipped).

### Status bar snooze: any animation (v1.2.3-rf111)
- STATUS BAR > Snooze is now a selector like idle / riding: BREATHE, DIM,
  DARK, NORMAL, then every Refloat animation (played in the Status Idle
  colors / speed while snoozing; status_animate draws it instead of the
  idle / battery display). status_snooze 0-12; stored in EEPROM word 83 bits
  20-23 (rf109 saves in word 82 bits 9-11 still load); ride_mode masked to
  bits 16-19. Unit test covers both layouts.

### Tour welcome wording (v1.2.3-rf110)
- "Refloat 1.2.3 under the hood, reimagined with a Neon Future style theme
  and a bunch of extra features."

### Status bar snooze, bigger tune buttons, neon tab box (v1.2.3-rf109)
- STATUS BAR > Snooze (controller-driven status strip): BREATHE / DIM /
  DARK / NORMAL (keeps its idle look) while snoozing; same timing as the
  head / tail lights. Byte 23 of COMMAND_LCM_VP_SETTINGS
  (LcmVpSettings.status_snooze), EEPROM word 82 bits 9-11; leds.c applies
  it to the status strip only (the power-off blink still shows on it).
- TUNES: buttons bigger (grid spacing 0.35 instead of 0.6 unit, min height
  1.5 unit) with a 4 px outline.
- Sheet tabs: the active tab is a rounded neon-green box with a soft glow
  (no underline).

### Tilt only while riding, HAPTIC FEEDBACK window, BEEPER in CONTROL (v1.2.3-rf108)
- TILT (main screen and CONTROL): only usable while engaged and moving
  (> 0.9 m/s); the CONTROL "enable" box is gone. Remote tilt input = UART is
  set once when the board is READY (only if not already; never mid-ride).
  The app sends the tilt only while usable, plus one final 0.
- CONTROL > HAPTIC FEEDBACK > (window): ON / OFF (haptic.duty.strength 0 vs
  the last strength), STRENGTH 0.5-12 V, AT DUTY (tiltback_duty, 50-100%,
  100 = off), AT SPEED (tiltback_speed km/h, shown in mph, 0 = off) - all
  Refloat's own settings; the text says the duty / speed are Refloat's
  pushback thresholds (nose lifts there too). Written 1.5 s after a change,
  never while riding.
- CONTROL also has BEEPER ON / OFF (is_beeper_enabled, same as LEDS >
  BEEPER).
- Tabs: less side padding so CONTROL keeps a bigger font.

### MAX SPEED: read-back check, limited mode refused, red warning (v1.2.3-rf107)
- Rider report: SET at 41 mph, the first hand test topped out at 16 mph, the
  second (after OFF / ON) at exactly 41. Cause not pinned down from here.
  Safety changes:
  - After every SET / REMOVE the app reads the motor config back from the
    board 1.5 s later and checks Max / Min ERPM: "Confirmed on the board:
    Max ERPM x = y mph" (green) or a red warning to check VESC Tool's Motor
    Settings > General > RPM before riding (mismatch or no answer).
  - Refused while VESC Tool is in limited mode
    (VescIf.getFwSupportsConfiguration() false: firmware / app versions
    differ, motor config can't be read / written reliably).
  - The window shows the maths (ERPM, wheel diameter, poles, gear) so it
    can be checked against VESC Tool.
- Terms window: red title (BaseDialog.titleColor) and the WARNING in bold
  red.

### MY MELODIES TEST, Jingle Bells default (v1.2.3-rf106)
- MY MELODIES has a big neon TEST button above DEFAULT MELODY / RECORD NEW:
  plays the current boot melody (board READY), like MIXER > TEST.
- The default boot melody (haptic_feedback.c default_melody, DEFAULT
  MELODY) is now Jingle Bells (public domain), 21 notes / rests, 3.48 s,
  same as the CLASSICS one. The rider's old default is still in their MY
  MELODIES as "Melody 2".

### Home screen first, swipe-up hint (v1.2.3-rf105)
- The UI always opens on the full home screen (sheet down; its last
  position is no longer restored).
- After the disclaimer / tour, a neon "TAP OR SWIPE UP HERE" label with a
  bobbing neon hand (Canvas) points at the sheet handle, with a ripple on
  it; gone once the sheet is opened or after 10 s. It doesn't take taps.

### Disclaimer: credits, ride at your own risk, back after every update (v1.2.3-rf104)
- BEFORE YOU START also credits Lukáš Hrázký (lukash) and the Float authors
  (no credit taken) and ends with "ride at your own risk". "Don't show this
  again" now only hides it for the installed version
  (swSettings.hideDisclaimerVer = {{VERSION}}): every package update shows
  it once more.

### Snooze on VESC's power-off clock (v1.2.3-rf103)
- Snooze no longer tracks its own activity: it uses motor_active_time, the
  same clock as VESC's idle power-off (bldc: mc_interface_set_current /
  brake / duty with a non-zero value resets it; in Refloat that's riding,
  braking a wheel that turned in the last second, and its motor tones).
  Footpads, lifting or parking on the tail don't restart it, for snooze or
  power-off. One clock, less code (Leds.snooze_activity_time removed).

### Once-over: one-color animations, snooze wake, clean-up (v1.2.3-rf102)
- FADE, PULSE, STROBE, KNIGHT RIDER are one-color animations: color 2 is
  their background / "off" color (BACKGROUND row with ON / OFF for all four).
  Picking one of them from another kind of animation starts it with a BLACK
  background (Status Idle and RGB LIGHTS). FELONY keeps COLOR 2.
- Snooze wake: engaging, a footpad, or anything that restarts Refloat's
  own idle timer (rolling wheel, lifting / setting down). A board parked
  standing on its tail now snoozes (it never did: "upright" counted as
  activity).
- Controller TEST BEEP is ignored while riding (never over riding alerts).
- Removed: the hidden free-spin limiter UI / dialog and its once-a-second
  COMMAND_FREESPIN_LIMIT packet (ignored by the package since rf75), and the
  unused rf92 headlight tile art.
- Sizes: QML ~85 KB compressed of VESC's 128 KB QML area; lisp + native
  library 89 KB of 128 KB; largest new stack frame status_animate 96 B
  (aux thread 1536 B).

### BACKGROUND ON / OFF, white background preview (v1.2.3-rf101)
- BACKGROUND rows (PULSE / KNIGHT RIDER; STATUS BAR and RGB LIGHTS) have an
  ON / OFF switch between the name and the arrows (SwStepper.toggle): OFF
  sets Refloat's color 2 to BLACK, ON brings back the last background
  (swSettings.rfBgLast, WHITE at first).
- ledSender.rfQColor returned strings for BLACK / WHITE, so blending to a
  WHITE background gave NaN and the preview name vanished; now Qt.rgba.

### Snooze = minutes before power-off (v1.2.3-rf100)
- SNOOZE BEFORE OFF (default 5, 0 = never): the head/tail lights and status
  bar snooze that many minutes before VESC's idle power-off (off after
  30 min, 5 -> snooze at 25 min). Without a power-off timer: after 10 min.
  The stored byte is now "minutes before" (255 from rf97 = 5). The window
  says when snooze starts ("so the lights snooze at 25 min").

### Classic melodies (v1.2.3-rf99)
- MIXER > MY MELODIES > CLASSICS: Ode to Joy, Twinkle Twinkle, Jingle Bells,
  Saints Go Marching, Charge! (public domain), written for the tiles
  E4 F4 G4 A4 B4 C5 D5 E5 F5 G5 within the 4 s / 32-note limits. A tap saves
  it to MY MELODIES and makes it the boot melody (editable on the tiles).

### HEAD/TAIL LIGHTS tile with the neon board (v1.2.3-rf98)
- LEDS: HEADLIGHTS is now HEAD/TAIL LIGHTS (tile, window, switch). The tile
  is full width with a Canvas drawing of the rider's neon board picture
  (deck, wheel, end lights, ground glow); its end lights fade between neon
  (purple tail / green nose) and regular (red tail / white head), left to
  right, every 7 s while shown and on. STATUS BAR is full width below it.

### Riding animation, snooze styles, AUTO snooze (v1.2.3-rf97)
- STATUS BAR > Riding animation (controller-driven status strip): BATTERY /
  DUTY (Refloat's own riding bar, default) or any Refloat animation, played
  with the Status Idle colors / speed while riding (leds.c status_animate,
  riding only; the duty bar still takes over above DUTY BAR FROM). Byte 21
  of COMMAND_LCM_VP_SETTINGS (0, or Refloat mode + 1), EEPROM word 83 bits
  16-23. Light code only.
- Snooze style BREATHE / DIM (steady 10%, controller LEDs) / DARK replaces
  the SNOOZE BREATHING pill (byte 22, word 82 bits 12-15; DARK also sets
  LCM_VP_FLAG_DOZE_DARK for an SLCM). SNOOZE BREATHING OFF from before
  becomes DARK.
- SNOOZE AFTER defaults to AUTO (255): 3 min before VESC powers off (10 min
  without a power-off timer, none if it powers off within 4 min).
- tools/lcm_vp_test covers the new bytes and the color bits in word 2.

### MAX SPEED tucked away, terms first, speedo badge (v1.2.3-rf96)
- MAX SPEED left the CONTROL section list: a dim "Max speed limiter >" line
  at the bottom of CONTROL (amber "MAX SPEED LIMIT: 45 MPH" when one is
  set). Tapping it opens terms every time (meant to limit airtime
  free-spin; reaching it while riding = nosedive) with an "I understand and
  accept" box that has to be ticked before CONTINUE, then the MAX SPEED
  window (LIMIT, SET LIMIT, REMOVE). Off by default (no limit until set).
- Minimum 40 mph (64.4 km/h).
- While a limit is set (cap under 100 mph) an amber "MAX 45 MPH" badge sits
  at the top of the speedo, between TOP SPEED and MAX DUTY.

### Tour wording (v1.2.3-rf95)
- Home screen page: "TILT slider and MAX SPEED / DUTY"; horn page: "stays
  well within safe, low motor current".

### Disclaimer wording (v1.2.3-rf94)
- Disclaimer: no "(Refloat's own LEDs or a light module)"; adds that the
  package is based on Refloat 1.2.3, later Refloat versions ride quite
  differently, and coming back from a newer version will likely feel
  different. App texts say "controller-driven LEDs" instead of "Refloat's
  own LEDs" (Refloat drives LEDs, it doesn't have any).

### Start disclaimer (v1.2.3-rf93)
- Each time the package UI opens, a BEFORE YOU START dialog says everything
  works off the current Refloat settings and needs a working board (rides
  correctly on Refloat) with working LEDs for the light features. "Don't
  show this again" (swSettings.hideDisclaimer) turns it off. On first start
  the tour opens after it. Same in the simulator.

### Animated HEADLIGHTS / STATUS BAR tiles, readable previews (v1.2.3-rf92)
- LEDS: the HEADLIGHTS tile shows a little board whose beam and taillight
  breathe, the STATUS BAR tile ten LEDs filling up like the battery bar
  (LedTile.art; animations run only while shown and lit; dim when off).
- Animation previews never go invisible with BLACK: FADE to black fades the
  name to dim, a black color 1 swaps to color 2, all black shows dim grey
  (FADE with WHITE / BLACK looked blank after pressing up).
- The "Needs the SLCM light module firmware" note no longer shows in RGB
  LIGHTS, BEEPER or UPDATES.

### Knight Rider background color, neon Tune Archive (v1.2.3-rf91)
- Refloat (leds.c) uses color 2 in FADE, STROBE, FELONY and as the
  background of PULSE and KNIGHT RIDER (color_blend(color2, color1, k)); the
  app only showed COLOR 2 for FADE / FELONY, so a leftover color 2 lit the
  Knight Rider background with no way to change it here. COLOR 2 now shows
  for modes 1-5, called BACKGROUND for PULSE / KNIGHT RIDER (BLACK = one
  color). Previews blend to it. Refloat's code unchanged.
- TUNE ARCHIVE: neon cards (title in a neon color, rider below, date tag),
  purple status line, REFRESH / CLOSE neon buttons. Same in the simulator
  (TUNES > TUNE ARCHIVE).

### Tour on first start, BEEPER, credits page (v1.2.3-rf90)
- The tips & tricks tour opens by itself the first time the package is
  opened (swSettings.tourSeen; SKIP or DONE closes it for good; SETTINGS >
  Show tips & tricks opens it again). Upstream's welcome dialog and hint
  pages no longer open on start (the "Show Welcome dialog" switch is hidden).
- LEDS > SOUND is now BEEPER.
- Tour: "BUILT SAFE" replaced by "CREDIT WHERE IT'S DUE" (Refloat by Lukáš
  Hrázký, Float by Mitch Lustig, Dado Mista, Nico Aleman; no credit taken)
  and "SAME RIDE" (riding is Refloat's own code; the rest is reviewed
  quality-of-life / user-experience upgrades; MAX SPEED is opt-in).

### MAX SPEED from VESC's Max ERPM (v1.2.3-rf89)
- CONTROL > MAX SPEED (30-75 mph): sets VESC's own Max ERPM / Min ERPM
  (motor config) from the wheel diameter, motor poles and gear ratio set in
  VESC Tool: ERPM = km/h x 1000/60 / (pi x diameter) x poles/2 x gear
  (280 mm, 30 poles: 45 mph = 20,582 ERPM). ERPM Limit Start raised to 0.95
  if lower. bldc motor/mc_interface.c: between start and max the forward
  current limit fades to 0, braking unaffected -> a free-spinning tire
  coasts at about the cap; on the ground the cap means a nosedive, hence the
  pop-up with an "I understand the risk" box (SET LIMIT disabled until
  ticked). REMOVE puts back 100,000 ERPM.
- Refused if VESC's Overspeed fault (l_additional_faults bit 1) is on (it
  would stop the motor at the cap), while the board is engaged (writing the
  motor config resets the motor), or without wheel diameter / poles.
- Read-modify-write: the motor config is read fresh from the board
  (getMcconf), then changed and written (setMcconf); 4 s timeout = nothing
  changed. No package (C) change.

### Power-off warning, snooze before power-off (v1.2.3-rf88)
- VESC's own power-off timer (App Settings > General > Shutdown Mode, OFF
  AFTER 10 s .. 5 h; read with get_cfg_int(CFG_PARAM_app_shutdown_mode)) is
  reset in the firmware only by motor commands with current
  (mc_interface_set_current / brake / duty / pid; bldc hwconf/shutdown.c,
  motor/mc_interface.c). The package mirrors that clock: riding, a rolling
  wheel (Refloat brakes it) or a Refloat motor tone. Footpads, lifting the
  board or the app do NOT keep the board on.
- Refloat's own LEDs: in the last minute (last quarter for 1 min, none for
  10 s) all strips blink twice every 15 s (to 10% of the set level, or up
  to 70% while snoozing); stops 30 s past the timeout (hardware that
  doesn't power off). Light code only, no motor or balance change.
- LEDS > HEADLIGHTS shows "Board powers off after X" (VescIf.appConfig()
  shutdown_mode) and SNOOZE AFTER stays below it.

### Neon tips & tricks tour, simpler colors (v1.2.3-rf87)
- SETTINGS > Show tips & tricks opens a Neon Future tour (rinkTour) of the
  Rink-Mixed features: home screen, sheet, horn, the MIXER (play live, REC,
  PLAY / SAVE / DONE, MY MELODIES), board style, lights, battery gauge,
  sound, updates, safety. Swipe or BACK / NEXT. (Upstream's hints still
  show on first start.) Same tour in the simulator.
- Color steppers offer the basic colors only (ledSender.rfColorSet: black,
  white, red, orange, yellow, lime, green, mint, tiffany, cyan, sky, blue,
  violet, magenta, pink); WHITE (FULL) shows as WHITE. A color set in VESC
  Tool outside the list still shows by name and is kept until changed.

### Snooze for Refloat's own LEDs (v1.2.3-rf86)
- LEDS > HEADLIGHTS > SNOOZE AFTER (NEVER, 1-60 min; shown with Refloat's
  own LEDs, e.g. Thor): after that long in READY with no movement, footpad
  or lift, leds_update dims the front, rear and status strips to a 6 s
  breath (3-30% of their set brightness), or dark with SNOOZE BREATHING off
  (LCM_VP_FLAG_DOZE_DARK). Anything of those wakes them (normal fade).
- Stored with the light settings, not in Refloat's config: byte 20 of
  COMMAND_LCM_VP_SETTINGS (older apps keep the stored value), EEPROM word 83
  bits 8-15 (0 in older saves = never). Survives power cycles; works with
  the app closed. An SLCM keeps its own snooze (not sent to it).
- tools/lcm_vp_test: clamp / keep / pack-unpack of the snooze minutes.

### LCM tools only with a light module, blank melody tiles (v1.2.3-rf85)
- "LCM firmware tools" is hidden when Refloat's LED Mode has no external
  light module (e.g. a Thor on Refloat's own LEDs) and no SLCM answers; the
  LCM FIRMWARE window says it only applies to Floatwheel / XRV / Pint V / GTV
  light modules with SLCM firmware while no SLCM is detected.
- Melody tiles are blank: same notes and outline colors, no note names.

### Riding animation, BATTERY idle choice, controller beeper, colored color names (v1.2.3-rf84)
- STATUS BAR > Idle animation: BATTERY is the first choice of the selector
  (Refloat Status Idle Timeout 0) instead of a separate ON/OFF pill; picking
  an animation from BATTERY sets the timeout to 10 s.
- STATUS BAR > Riding animation (only with Refloat's own status strip, e.g.
  Thor): DUTY BAR FROM 15-100% (Refloat Status Duty Threshold; 100% = NEVER,
  battery only) and FOOTPAD SENSORS ON/OFF (Show Sensor Indicators While
  Running). Refloat's riding bar is battery level / duty bar, nothing else.
- SOUND > Controller: BEEPER ON/OFF = Refloat's Enable Beeper on Servo/PPM
  (is_beeper_enabled). Refloat has no separate write-beep switch: this one
  gates the write beep and every warning beep. TEST BEEP: COMMAND_LCM_TEST_BEEP
  with [2] = 1 beeps the controller (beep_alert); the 2-byte form still
  beeps the light module.
- COLOR 1 / COLOR 2 values (RGB LIGHTS, Status Idle) are shown in their own
  color (black as dim grey). SwStepper.valueColor.

### IN MOTION / IDLE light naming (v1.2.3-rf83)
- HEADLIGHTS window: the two brightness rows are now IN MOTION (Refloat's
  Headlight Brightness, board engaged) and IDLE (Front Brightness, the rest
  of the time); tile reads "ON · MOTION x% · IDLE y%".
- RGB LIGHTS: the HEAD / TAIL / FRONT / REAR pills are two rows, IN MOTION
  [HEAD][TAIL] (Refloat headlights / taillights) and IDLE [HEAD][TAIL]
  (Refloat front / rear). Same settings as before, only grouped and renamed.
  Same in the simulator.

### Animation selector, tap-to-close sheet, neon number pad, 4 V horn (v1.2.3-rf81, rf82)
- RGB LIGHTS and STATUS BAR > Idle animation: the separate ANIMATION row is
  gone; the animated name itself sits between the down / up arrows
  (SwStepper bigName + rf / rfAnim). Same in the simulator.
- With the sheet (SUW) up, a tap anywhere on the main view above it puts it
  down (that tap does nothing else).
- Settings number fields (IntTextField: speed dial max, temperature warning
  offset, plot window) open our own neon number pad with DONE (applies the
  value, kept within the field's range, and closes) instead of the phone's
  grey keyboard. Settings labels, values and units are neon green.
- Horn: 4 V is the top beep strength (VOLUME 10/10 = 4 V, 1/10 = 0.4 V;
  ADVANCED BEEP STRENGTH 0.4-4 V in 0.2 V steps; the package caps the horn
  at 4 V, also when it uses Refloat's haptic settings).

### STATUS BAR ON/OFF works with Refloat's own LEDs (v1.2.3-rf80)
- Refloat's lights control only switches all LEDs or the headlights, so the
  LEDS tab's STATUS BAR ON/OFF only reached an SLCM. Now the package also
  dims Refloat's own status strip to 0 when it's off (Leds.status_disabled,
  set each aux pass from the stored light settings' STATUS_BAR flag), e.g.
  on a Thor. Kept across power cycles with the other light settings; no
  Refloat config value is changed.

### RGB LIGHTS, LED settings kept exactly (v1.2.3-rf79)
- LEDS > RGB LIGHTS (tile, only when Refloat's own LEDs are set up:
  hardware.leds.mode not off and a front or rear strip): Refloat's four light
  bars - HEAD / TAIL (headlights on), FRONT / REAR (headlights off, running
  lights) - with ANIMATION, COLOR 1/2, SPEED, BRIGHTNESS and OFF WHEN LIFTED,
  plus a live preview (AnimName anim 15 with the bar's settings). Nothing new
  in the package: it reads / writes leds.<bar>.* and
  leds.lights_off_when_lifted, linked both ways with Refloat Cfg > LEDs.
  HEAD / FRONT brightness are the HEADLIGHTS window's RIDING / RUNNING levels.
  Hidden on XRV / GTV / Pint V (plain head/tail strips via the LCM).
- SLCM ANIMATIONS tile renamed STATUS BAR ANIMATIONS (it only drives the
  status bar), with a one-line note.
- LED sync writes only what was changed in the app since it was last read
  (ledSender.cfgSnap / rgbSnap): custom values set in VESC Tool are never
  rounded to whole percent or overwritten, and the status brightness with
  headlights on / off stay separate unless RIDING BRIGHTNESS is changed.

### Safety fixes from Robert's code review, tagged releases (v1.2.3-rf77)
An independent review of rf74 against stock Refloat (v1.2.3 / v1.3.1) and
stock ALCM v0.1.1 found these; each was checked against the code first.
- S1 (critical) LCM bootloader: the 5-minute idle power-off is GONE. The
  LCM holds the board's power latch and the VESC balances without it, so the
  bootloader can't know nobody is riding; an update interrupted after the
  erase would boot into it every time and cut power 5 min later, mid-ride.
  Power is now only released by a 2 s button hold. While in the bootloader the
  headlights (white front) blink at about 1 Hz while waiting and stay steady
  while pages arrive, so a stuck LCM is obvious (PB4 as a plain output, PC7
  forward; the app takes PB4 back as PWM at init).
- S2: the package refuses to START (pads, push-start) while an LCM update is
  staged or a bootloader asked for a page in the last 5 s (update running,
  or an LCM stuck in its bootloader), and beeps 3x every 2 s if you try.
  SENDING times out after 10 s of silence instead of staying forever. These
  checks are start-only (not in can_engage, which also runs mid-ride).
- S3 SLCM: a lost VESC link is a fault again in every mode, as in stock ALCM
  (idle shutdown cancelled, power stays on), but it clears itself on the
  next answer and is silent when it began while not riding - no beeping
  after package installs, no idle countdown on stale data.
- S4: SysTick stopped and its pending bit cleared (ICSR.PENDSTCLR) on the
  way into the bootloader and back - NVIC->ICPR doesn't clear system
  exceptions, a stale tick could vector through the bootloader's 8-entry
  table into code bytes and hang the LCM with power latched.
- P1/P7: every flash write (Refloat config, horn, lights, battery curve,
  odometer) goes through flash_begin(): only at rest (not running, footpads
  off, wheel still), and starting is blocked while it runs (flash_busy). A
  config write that arrives while riding (the LEDS tab mirrors lights into
  the Refloat config 1.5 s after a change) is applied in RAM at once and
  saved after you step off (cfg_save_if_due; not in flywheel mode). Aux
  thread stack 1024 -> 1536 bytes, as upstream v1.3.
- P3: light-control packet clamped as upstream v1.3 (brightness <= 100,
  payload <= 64 bytes - payload_size sits right after the array).
- P4: IMU timeout watchdog ported from upstream v1.3: the IMU read callback
  stamps each sample; no data for 0.1 s while riding stops the board
  (STOP_IMU_TIMEOUT, "IMU Timeout" in the UI, 3 long beeps); starting waits
  0.2 s after the data resumes.
- P5: flywheel off stops the board before the mode change
  (state_flywheel_off, upstream v1.3); lisp/bms.lisp is upstream v1.3.1's
  (errors trapped and printed, temp sensor count checked).
- E1: riders only get tagged releases (see RELEASING.md). Main pushes
  publish "dev-build" / "lcm-dev" pre-releases (bench testing only); the old
  "latest-build" is retired. A tag v<version> publishes the rider release
  (kept, marked Latest) and refreshes lcm-latest.
- LEDS › UPDATES (a tile, opens a window): CHECK FOR UPDATES asks GitHub for the latest rider release
  (releases/latest = newest tagged, non-prerelease; dev builds never show),
  compares its package version with this one and its SLCM version (asset
  `slcm-r<N>-<hash>.txt`, added by lcm.yml on a tag) with the light module's
  name. DOWNLOAD UPDATE fetches SLCM.hex, runs parseHex (address range,
  vector table, checksums) and refuses it unless the version inside matches
  the release. Then a prompt: FLASH UPDATE opens LCM firmware tools with the
  file picked and runs CHECK LCM - the normal CONTINUE / FLASH LCM / confirm
  steps follow. A newer package is offered as a download link (VESC Tool
  installs packages). Uses QML's XMLHttpRequest, like upstream's tune archive.
  CHECK LCM compares a downloaded SLCM with its release's build (lcmFw.relTag
  / refVer), not with this package's built-in one. Simulator: UPDATES tab
  too (pretend newer / same / none / offline release, or LIVE = real GitHub
  check), versions updated to rf78 / SLCM r23. UPDATES lives under LEDS (tile), not as its own tab.
- Note: the SLCM version (r<N>) counts lcm/alcm commits only. A bootloader
  change (lcm/bootloader) never reaches a board through the app - the app
  update only rewrites the light firmware. Boards flashed with an rf76-era
  SLCM.hex keep the timed-power-off bootloader until SLCM.hex is flashed
  once more with an ST-Link.
- Not changed: S5 (power off from the app) - kept on purpose, idle-only on
  both ends.

### SLCM.hex (v1.2.3-rf76)
- The ST-Link file on the lcm-latest release is now SLCM.hex (built as
  ALCM_VP_BL_full.hex, bootloader + app; the old asset name is removed from
  the release). App, simulator and docs say SLCM.hex. Firmware unchanged
  (r22).

### Safety hardening (v1.2.3-rf75)
Prompted by lukash's point on refloat PR #119: the package runs on the
controller's own CPU, so a crash anywhere in it (LEDs included) stops
balancing. Audit of every change against upstream Refloat v1.2.3:
- Free-spin limiter REMOVED from the balance loop. It was the only change to
  the balance current: within 3 km/h of its limit it scaled drive current
  down while Refloat flagged wheelslip - a false flag (bump, lost traction)
  could cut drive current at speed. refloat_thd's current path is now
  upstream's. COMMAND_FREESPIN_LIMIT (121) is accepted and ignored; the app
  hides the setting; rt-data flag bit 2 is always 0.
- Horn never adds motor current: the buzz (up to 15 A square wave on top of
  the balance current; 8 A even with Refloat's default Vibrate Strength 0)
  is gone - riding or idle, the horn is the foc_play_tone beep only, the
  same mechanism upstream uses for its duty warning (<= 6 V vs upstream's
  12 V max). While riding it also only sounds below 60% duty, and stays
  lowest priority (never masks pushback / alerts). App hides the buzz.
- Package settings saves (horn/melody, light module, battery curve) only
  with the board at rest: not running, footpads off, < 100 ERPM (stricter
  than upstream's own config save). The horn save writes straight from
  horn_pending (no 136-byte copy on the 1 kB aux-thread stack).
- CI: tools/stack_report.sh compiles every file with -fstack-usage (no LTO),
  lists the largest frames and those new / bigger than upstream v1.2.3, and
  fails above 512 bytes in one frame.
- Reviewed, unchanged: command parsing (all lengths checked, len >= 2
  before any `len - 2`), lcm poll buffer (84 B, worst case ~55 used), LCM
  name parse (bounded, fixes upstream's one-byte over-read), battery curve
  (cells / voltage 0 guarded, strictly decreasing points so no divide by 0),
  idle-animation translation (speed 0 / color / mode bounds checked), no
  writable statics (VESC packages have no .data/.bss), melody / live tones
  READY only, LCM update and power-off refused while running.

### POWER OFF from the app, SLCM r22 (v1.2.3-rf74)
- For a board whose LCM can't read its button any more (here: damaged by an
  ST-Link connected with the battery on - the button still powers the
  board up, but the LCM never sees presses).
- LEDS > Lights: POWER OFF BOARD (SLCM r22+ only). Tap once to arm (4 s),
  then tap again and hold 5 s (progress fill; letting go early cancels,
  stays armed). Disabled while riding.
- App -> package COMMAND_LCM_POWER_OFF (130), reply [101, 130, ok]; refused
  while RUNNING. The package sets VP flag bit 6 (0x40) in the next 8 poll
  replies (~2 s, cancelled if it starts running). The SLCM only takes it
  while idle and pushes EVENT_COMMAND_SHUTDOWN: the same shutdown as
  holding the button (shutdown animation, settings saved, power released).
  Older SLCMs ignore the bit.

### SLCM r21: power button hold-to-off fixed (v1.2.3-rf73)
- r20 (rf71) skipped the clock setup in the bootloader build's SystemInit,
  assuming the bootloader had done it - but on a normal power-up the
  bootloader jumps to the app without touching the clock. The app then ran
  on the untrimmed reset clock with reset flash timing (wait states, flash
  clock prescaler for EEPROM programming). Reported: holding the button no
  longer powered the board off (shutdown saves the settings to EEPROM).
- SystemInit (LCM_BOOTLOADER) now sets the same clock as SetSysClock()
  (HSI 32 MHz + factory trim, 2 wait states, FLITFCLK_PRE 7, HCLK = PCLK =
  SYSCLK) in a compact form. Bootloader unchanged (no ST-Link needed: the
  update goes over Bluetooth from an r20 board).

### One set of light settings for every controller (v1.2.3-rf72)
- The LEDS tab now edits Refloat's own LED settings for everything that every
  controller understands: lights on/off, headlight riding / idle level,
  status bar riding / idle level and (new) the Status Idle animation
  (STATUS BAR > Idle animation: Refloat's 9 modes, color 1 / 2 from Refloat's
  palette, speed, start after = leds.status_idle.*, leds.status.idle_timeout).
  They are kept in step with VESC Tool > Refloat Cfg > LEDs both ways
  (rfLoaded: the app never writes its defaults over a config it hasn't read).
- Refloat's own LEDs (Thor, Floatwheel, internal strips) play them directly.
  For an SLCM the package translates: IDLE animation 128 (new default) =
  "follow Refloat" - lcm.c refloat_idle_anim() turns Refloat's mode into the
  closest SLCM animation each poll: Felony -> THE FUZZ, everything else ->
  CUSTOM (fill / scan engine, Refloat colors -> hues, speed -> steps).
  Idle timeout 0 (Refloat's default) = STANDARD (battery), as before.
  SLCM before r19 (no CUSTOM) gets STANDARD. While riding the rider's own
  CUSTOM block is sent (the SLCM has one CUSTOM slot). No SLCM change.
- SLCM EXTRAS (ANIMATIONS for the other moments, CUSTOM designer, SOUND,
  BATTERY GAUGE, snooze breathing) are only shown when an SLCM is detected.
  ANIMATIONS > IDLE has STATUS BAR as an extra choice (= follow Refloat);
  picking another SLCM animation there overrides it until something in
  STATUS BAR > Idle animation is changed.
- tools/lcm_vp_test: stub has Refloat's LED types; follow-Refloat tests.

### SLCM r19+: settings kept right away, CUSTOM animation (v1.2.3-rf71)
- Plan agreed with the rider: the SLCM keeps all 13 animations and runs the
  lights itself, so status bar and head / tail lights work with the package
  off. What the app sets is written to the LCM's own EEPROM ~3 s after the
  last change (never while riding; only if different), not only at a clean
  shutdown - a power cut no longer loses it. Snooze breathing on/off is now
  stored on the LCM too (settings_t.doze_dark).
- New status bar animation CUSTOM (14): designed in the app (LEDS >
  ANIMATIONS > DESIGN CUSTOM): FILL or SCANNER engine, colors (rainbow
  either way, wave or flip between two colors, my color), pattern (solid /
  gradient / mirror, or 7 scan directions), colors A / B, color speed,
  motion speed, brightness steady / breathe / flash, lowest brightness.
  The LCM runs it with its own fill / scan engines (smooth, no frames over
  the link) and stores it, so it plays with the package off as well. New
  looks come from package / app updates, no reflash.
- Why not streamed frames (the first idea): the LCM polls every 250 ms, so
  package-driven frames would be 4 fps.
- Protocol: COMMAND_LCM_VP_SETTINGS takes 20 bytes (12 + 8 custom);
  package EEPROM words 125-126; the poll reply carries the 8 custom bytes
  (reply 34 bytes) only to an SLCM named r19 or newer (older builds drop
  replies over 32 bytes). LCM MAX_PACKET_LENGTH 40.
- LCM flash: the bootloader build no longer repeats the clock setup the
  bootloader already did (SystemInit in system_hk32f030m.c, bootloader build
  only), which pays for the above.

### SLCM LED windows back to how they were with the SLCM (v1.2.3-rf116)
- All the new LED features (rf83+: IN MOTION / IDLE naming, snooze before
  power-off, snooze looks, riding / snooze animations with own colors,
  power-off blink, BACKGROUND switch ...) are for controller-driven LEDs
  (Refloat's own LED section: Thor, RGB strips) only.
- On an SLCM board (ledSender.slcmPresets: SLCM answering, no
  controller-driven LEDs) HEAD/TAIL LIGHTS and STATUS BAR are the rf82
  windows again: HEADLIGHTS ON/OFF, RIDING / RUNNING LIGHTS, SNOOZE
  BREATHING ON/OFF; brightness + Refloat's Status Idle animation turned into
  the closest SLCM look. SLCM EXTRAS > ANIMATIONS is unchanged. rf115's copy
  of the SLCM presets in STATUS BAR is gone (they stay in SLCM EXTRAS).
- SNOOZE BREATHING also sets the snooze style (BREATHE / DARK) so the
  DARK flag sent to the SLCM is the same as before.
- SLCM firmware, the SLCM poll reply and the LCM settings bytes the SLCM
  uses are unchanged (lcm/ untouched since rf82).

### SLCM STATUS BAR as when the SLCM was first tied in (v1.2.3-rf117)
- SLCM boards: STATUS BAR = on/off + idle / riding brightness only (rf71),
  with an ANIMATIONS › link. The animations are the SLCM's own presets
  (THE FUZZ, FIRE, RAINBOW SCAN, ...) in SLCM EXTRAS › ANIMATIONS, with
  their animated names and one COLOR. Refloat's Status Idle animation is
  no longer in this window for SLCM boards (no more two places for the
  idle animation); STATUS BAR stays one optional IDLE choice there.
- Default IDLE for a fresh install: STANDARD (was STATUS BAR, 128).
  Saved settings are kept as they are.
- STATUS BAR tile on an SLCM board shows the SLCM IDLE preset.
- Controller-driven LEDs (Thor) unchanged.

### SIMPLE / ADVANCED for Refloat's LEDs, RIDING naming (v1.2.3-rf118)
- STATUS BAR (controller-driven LEDs): SIMPLE (default) = one row per
  moment like the SLCM ANIMATIONS screen, with Refloat's animation names
  drawn as the animation: IDLE (BATTERY + Refloat modes), RIDING
  (BATTERY/DUTY + modes), SNOOZE (BREATHE / DIM / DARK / NORMAL + modes),
  and one COLOR for all of them (sets Status Idle color 1; riding and
  snooze follow the idle colors again). ADVANCED = the full screen as
  before. swSettings.barAdvanced.
- RGB LIGHTS: SIMPLE (default) = HEAD / TAIL (riding) and FRONT / REAR
  (idle) animation rows, each keeping its own colors, plus riding / idle
  brightness; ADVANCED = the full screen (colors, speed, tail / rear
  brightness, OFF WHEN LIFTED). swSettings.rgbAdvanced. The IDLE group
  pills read FRONT / REAR.
- "IN MOTION" renamed "RIDING" (HEAD/TAIL LIGHTS, RGB LIGHTS, tour).
- All values stay Refloat's own settings; SLCM boards unchanged.

### FELONY starts in red / blue (v1.2.3-rf119)
- Picking FELONY (SIMPLE or ADVANCED) sets its colors to RED / BLUE:
  status bar idle (Status Idle color 1 / 2), riding and snooze (their own
  colors; snooze switches to its ADVANCED colors) and every RGB light bar.
  Colors can still be changed in ADVANCED afterwards.
- SIMPLE: COLOR leaves a FELONY riding / snooze alone; leaving FELONY
  there goes back to the shared COLOR.

### RGB LIGHTS SIMPLE: riding SOLID = white head / red tail (v1.2.3-rf120)
- In RGB LIGHTS SIMPLE, picking SOLID for RIDING HEAD sets color 1 to
  WHITE, for RIDING TAIL to RED (Refloat's headlights / taillights bars).

### RGB LIGHTS: HEADLIGHT / TAIL LIGHT names (v1.2.3-rf121)
- RGB LIGHTS riding rows / buttons read HEADLIGHT and TAIL LIGHT (were
  HEAD / TAIL), also in the hint and the LEDS tile summary.

### RGB LIGHTS SIMPLE: a COLOR per light (v1.2.3-rf122)
- Under each light in SIMPLE (HEADLIGHT, TAIL LIGHT, FRONT, REAR) a
  COLOR row (Refloat color 1 of that bar); hidden for the rainbows.

### Effect colors, COLOR rows on top with left / right arrows (v1.2.3-rf123)
- Picking an effect starts it in its own colors, loosely like the SLCM
  presets (ledSender.effectColors): SOLID SKY (status bar; RGB: white
  headlight / front, red tail light / rear), FADE SKY <-> FLAME (COMP.
  WAVE), PULSE SKY, STROBE WHITE, KNIGHT RIDER RED, FELONY RED / BLUE
  (THE FUZZ); rainbows unchanged. Status bar idle (Status Idle colors),
  riding and snooze (their own colors), every RGB light bar; SIMPLE and
  ADVANCED. Replaces the rf119 / rf120 FELONY / SOLID special cases.
- STATUS BAR SIMPLE: COLOR on top of the rows; it changes color 1 of
  idle, riding and snooze (FELONY keeps red / blue).
- RGB LIGHTS SIMPLE: each light's COLOR above its animation.
- Refloat color rows (SIMPLE and ADVANCED) use left / right arrows
  (SwStepper.sideArrows).

### Tidy SIMPLE screens: neon cards, lined-up arrows (v1.2.3-rf124)
- New LedCard (rounded neon card with a centered title).
- RGB LIGHTS SIMPLE: each light in its own card (HEADLIGHT, TAIL LIGHT,
  FRONT, REAR) with COLOR and EFFECT rows; brightness in a card too.
- STATUS BAR SIMPLE: COLOR / IDLE / RIDING / SNOOZE in one card.
- Every row in these screens has the same value width, so the arrows line
  up; the hints are centered.

### BATTERY text animates as a draining gauge (v1.2.3-rf125)
- Refloat view only: the BATTERY / BATTERY/DUTY choices (status bar idle
  and riding, SIMPLE and ADVANCED) light their letters like a battery
  gauge: full -> empty over ~5 s, green -> yellow -> red, blinking red at
  the end, then refill (AnimName.batteryCol). SLCM screens unchanged
  (BATTERY LEVEL / STANDARD keep their old look).

### RGB LIGHTS SIMPLE: one card per light, RESTORE DEFAULTS (v1.2.3-rf126)
- HEADLIGHT card = Refloat's headlights (RIDING) + front (IDLE) bars,
  TAIL LIGHT card = taillights (RIDING) + rear (IDLE); in each, the effect
  row with its COLOR below it. BRIGHTNESS card: RIDING / IDLE.
- RESTORE DEFAULTS: every bar SOLID, headlight white, tail light red
  (ledSender.rgbRestore).
- ADVANCED: the idle buttons read HEADLIGHT / TAIL LIGHT as well; the
  LEDS tile shows HEADLIGHT / TAIL.

### RGB LIGHTS: IDLE can be OFF, off by default after RESTORE (v1.2.3-rf127)
- SIMPLE: the IDLE row of HEADLIGHT / TAIL LIGHT starts with OFF (that
  light's idle brightness 0 - Refloat's front / rear brightness); picking
  an effect again brings back the brightness it had (default 50 %).
- RESTORE DEFAULTS: solid white headlight / solid red tail light while
  riding, idle OFF.
- Refloat view only; SLCM screens unchanged.

### HEAD/TAIL LIGHTS and RGB LIGHTS merged (v1.2.3-rf128)
- Controller-driven RGB LEDs (Thor): one HEAD/TAIL LIGHTS window with the
  ON/OFF switch, SIMPLE / ADVANCED, the HEADLIGHT / TAIL LIGHT cards,
  BRIGHTNESS card, RESTORE DEFAULTS, then a SNOOZE card (BEFORE OFF, LOOK,
  power-off note). The RGB LIGHTS tile is gone; the old RIDING / IDLE
  brightness rows are hidden there (the BRIGHTNESS card has them).
- Refloat boards without RGB strips: the same window without the effect
  cards (brightness rows + snooze card). SLCM boards: unchanged.

### Snooze timer on the LEDS page (v1.2.3-rf129)
- Controller-driven LEDs: SNOOZE card on the LEDS page under the tiles:
  BEFORE OFF (minutes before VESC's power-off) + the power-off note. The
  HEAD/TAIL LIGHTS window keeps a SNOOZE LOOK card. SLCM boards unchanged.
- Simulator: version label rf129.

### No UPDATES tile in the Refloat view (v1.2.3-rf130)
- The UPDATES (CHECK FOR UPDATES) tile on the LEDS page shows only when
  an SLCM is detected; SLCM boards unchanged.

### AUTO APPLY in ADVANCED, controller BEEPER on the LEDS page (v1.2.3-rf131)
- SIMPLE views always apply LED changes right away. The ADVANCED views of
  HEAD/TAIL LIGHTS and STATUS BAR (Refloat view) have an APPLY card with
  AUTO APPLY ON (default) / OFF. With it off, changes made there wait:
  APPLY CHANGES at the top of the window sends them (the LCM packet and
  the Refloat config write, still only once the board is stopped); waiting
  changes are kept when leaving the window (APPLY WAITING LED CHANGES on
  the LEDS page). Light switches always apply right away. The periodic
  resend and config reads hold off while changes wait. SLCM boards always
  apply (unchanged).
- Controller beeper: BEEPER card (ON/OFF + TEST BEEP) on the LEDS page in
  the Refloat view; the beeper writes on its own (not held by AUTO APPLY,
  doesn't write waiting LED changes).

### ALL LIGHTS on top, a bit bigger (v1.2.3-rf132)
- Refloat view: in Lights on the LEDS page, ALL LIGHTS sits above
  HEAD/TAIL and STATUS and is a bit bigger (SwPill.labelSize). SLCM
  boards: unchanged.

### MIXER: BEEP button, recordable controller beep (v1.2.3-rf133)
- MIXER: ♪ BEEP next to ● REC sounds the controller's own beeper (Refloat's
  beeper on Servo/PPM, COMMAND_LCM_TEST_BEEP [2]=1; ignored while riding).
  In REC it is placed in the take like a tile tap (200 ms, orange ♪ on
  the timeline).
- Melody format: pitch level 11 = controller beep (MELODY_BEEP_LEVEL). On
  playback (TEST, saved melodies, boot melody; READY only) that step keeps
  the motor quiet and main sounds one short beep_alert (silent when the
  beeper is off, never while running). Old melodies are unchanged.
- Package only; VESC firmware unchanged.

### MIXER: orange BEEP button (v1.2.3-rf134)
- ♪ BEEP is orange (#ff8a1f, like its timeline marks) so it stands apart
  from LIVE / REC (SwPill.litColor).

### Project standards, GPL notices (v1.2.3-rf135)
- Official project name: Refloat rink-mixed (refloatRM). CLAUDE.md holds
  the permanent development standards (attribution, GPL-3.0 compliance,
  safety, build sign-off) and the project facts.
- GPL-3.0 section 5a: every upstream Refloat file changed by this fork now
  carries "Modified by Refloat rink-mixed (refloatRM), 2026. See
  FORK_NOTES.md." under the original copyright lines (comments only, no
  code change).

### STATUS BAR tile: the board's front with a live status bar (v1.2.3-rf136)
- Refloat view (no SLCM): the STATUS BAR tile on the LEDS page shows the
  user's neon drawing of the board's front (Canvas, LedTile art 4) with a
  ten-LED status bar behind a purple light pipe, playing Refloat's Status
  Idle animation from the current settings (mode, colors, speed;
  approximate), or the draining battery gauge when the idle animation is
  off. Still and dimmed while the status bar is OFF; pauses in the
  background. SLCM boards keep their tile (art 2).
- UI only: no package C code, no settings, no board communication changed.

### "Scout's Honor" easter egg in the start notice (v1.2.3-rf137)
- Under the "Before you start" notice (after its checkbox, behind a divider
  and an "EASTER EGG" label): the Code Compliance Scout portrait (embedded
  JPEG, 220x269, ~9 KB, centered, neon-green frame) and the Scout's Honor
  pledge, "Unofficially supervised by the Code Compliance Scout. Trust, but
  verify.", and a line saying it's a joke: the person pictured hasn't
  reviewed, approved or certified the package and isn't responsible for it.
  The notice can scroll now.
- The notice's own text (settings, Refloat 1.2.3, credits, ride at your
  own risk) is unchanged. UI only; no firmware, settings or licensing
  change. Compressed QML ~103 KB of the 128 KB limit (was ~91 KB).

### Scout's Honor: no "EASTER EGG" label (v1.2.3-rf138)
- The "— EASTER EGG —" label above the portrait is gone; the divider
  still separates it from the notice. UI only.

### Scout's Honor: new portrait (v1.2.3-rf139)
- The Code Compliance Scout portrait replaced with the user's new
  illustration (same size: JPEG 220x269, ~10.5 KB embedded). UI only.

### Safety review, static analysis in CI, dead code removed (v1.2.3-rf140)
- CI: new "Static analysis" step (tools/static_analysis.sh, clang-tidy:
  clang static analyzer + bugprone checks on every src/*.c). Findings on
  lines that are word-for-word upstream Refloat v1.2.3 are reported only;
  any finding on a line added or changed by refloatRM fails the build.
- Fixed the three findings on fork lines (no behaviour change):
  lcm_light_ctrl_request reads the 3 brightness bytes by fixed index (the
  `min(cfg[idx++], ..)` form was correct - the macro evaluates once - but
  read as a double increment); lcm_vp_set's whole-struct compare is
  documented (both copies are zeroed, padding included); lcm_fw_command
  has an explicit `default:` (unknown sub-commands ignored, as before).
- haptic_feedback.c: removed the horn "buzz" branches that could never run
  (since rf75 the horn never adds motor current: `can_buzz` was always
  false) and the always-true `melody_gaps` flag. Same output in every
  case: horn = beep only (ALT = beep / pause), other alerts unchanged.
  Removed the then-unused HORN_MIN_VIBRATE_STRENGTH /
  HORN_DEFAULT_VIBRATE_FREQUENCY.
- Kept on purpose: COMMAND_LCM_POWER_OFF / COMMAND_FREESPIN_LIMIT stubs
  (old apps still send them; they're accepted silently), the stored horn
  buzz settings (part of the saved horn words and the app protocol).

### Tap the version line to read the notice again (v1.2.3-rf141)
- Tapping the RE:FLOAT — RINK-MIXED version line at the bottom of the
  screen opens the "Before you start" notice again (Scout's Honor at its
  bottom), also after "Don't show this again". UI only.

### MIXER: NOTES pill (v1.2.3-rf142)
- NOTES (after ♪ BEEP) shows the note names on the tiles again (E4 ... G5,
  as before rf85); tap again for blank tiles. Remembered
  (swSettings.mixerNotes, default off). UI only.

### Start notice page 2: BACK TO REFLOAT 1.2.3 (v1.2.3-rf143)
- The "Before you start" notice has two pages: NEXT › leads to a "don't
  trust the AI code" picture (240x240 JPEG, 11.7 KB, embedded) with
  CONTINUE WITH refloatRM, ↶ BACK TO REFLOAT 1.2.3 and ‹ BACK.
- BACK TO REFLOAT 1.2.3 opens a confirm window (board connected and
  stopped, back up the Refloat Cfg, tune carries over, LCM firmware not
  changed, how to come back). INSTALL 1.2.3 downloads the official,
  unmodified Refloat 1.2.3 package, mirrored byte-for-byte from
  lukash/refloat by refloat-stock.yml to release `refloat-stock`, and only
  accepts it if the size (97317) and SHA-256 (87110e8f…5daf) match. The
  board must be connected and not running both before the download and
  right before installing. The install is VESC Tool's own
  CodeLoader.installVescPackage (the same as the Packages page). Nothing is
  written to the board until the file passes its checks.
- CI: new step fails the build if the Refloat settings signature ever
  differs from stock 1.2.3's config_signature.txt, because the tune only
  carries over while the two match. src/conf is unchanged from v1.2.3.
- No firmware (C) changes. QML only, plus CI.

### Repository: attribution, license notices, upstream history (v1.2.3-rf144)
- README.md opens with what refloatRM is (an unofficial Refloat
  modification, not affiliated with the Refloat developers), credits,
  license, AI-assisted note, download channels and a safety warning;
  Refloat's own README follows unchanged. CONTRIBUTING.md got a one-line
  note on where refloatRM issues go.
- License notices (comments only, no code change): the "Modified by refloatRM"
  line on lisp/package.lisp, lisp/bms.lisp and pkgdesc.qml; SPDX
  GPL-3.0-or-later headers on 28 refloatRM-written files (src/battery_curve,
  src/lcm_fw, the LCM bootloader, the flasher, tests and tools).
- New THIRD_PARTY_NOTICES.md (ALCM, ARM CMSIS license text, HK32 library,
  flasher and app dependencies; open items marked NEEDS REVIEW).
- Release notes (dev, LCM, rider, stock mirror) now state the license, the
  non-affiliation and link the exact source commit. Text only.
- RELEASING.md: lcm-latest status, licensing checks before tagging, and how
  to work with upstream (remote setup, never push upstream tags, upstream
  merges on a branch, contributing back through a clean fork).
- Upstream Refloat history (through v1.2.3, 800a46b) joined into main with
  a merge that changes no files, so lukash's commits are in our history.

### Scout's Honor easter egg removed (v1.2.3-rf145)
- The Scout's Honor block (portrait, pledge and joke note under the start
  notice, added in rf137-rf139) is gone from the package and the simulator,
  at the user's request. The notice text, "Don't show this again", NEXT ›
  and page 2 are unchanged; tapping the version line still reopens it.
  UI only. The portrait remains in older commits and builds (history is not
  rewritten).

### More credit to Refloat (v1.2.3-rf146)
- New first line on the start notice (neon green, centered): "None of this
  would be possible without all the hard work Lukáš Hrázký (lukash) and the
  Refloat community have put into Refloat. Thank you!" The existing credits
  paragraph (Refloat and Float authors) stays.
- The same line in the VESC Tool package description (package_README.md) and
  the repository README credits.
- Tour WELCOME page: "Built on Refloat 1.2.3 by Lukáš Hrázký (lukash) and the
  Refloat community, reimagined with ...". UI text only.

### Renamed to Refloat Glow (v1.2.3-rf147)
- The project is now **Refloat Glow** (the user dropped "rink" from the
  name). Package name (package_name, pkgdesc.qml, Makefile), the UI title
  ("REFLOAT — GLOW"), the start notice and page 2, the tour WELCOME page,
  the update windows, the simulator, README / package README, RELEASING.md,
  THIRD_PARTY_NOTICES.md, release-note text, and the "Modified by" notices
  in file headers ("Refloat Glow (formerly Refloat rink-mixed)").
- Lisp console messages say "[Refloat Glow]"; the BMS thread is named
  "Refloat Glow BMS" (same length as before).
- Kept on purpose: the GitHub repo name, the download file names
  refloatrm.vescpkg / refloatrm-dev.vescpkg (links and the in-app update
  check), the QML settings category scottWheelz (saved app settings), old
  names in this changelog and in older code comments.
- The package name is only a label (saved tunes record it; VESC Tool's store
  lookup); installing over an older build works as before. No firmware
  logic changes.

### "Unofficial side project" line (v1.2.3-rf148)
- Start notice (after the thank-you line), package README and repository
  README: "Refloat Glow is an unofficial side project, built for fun, to make
  a few things simpler, like the LED controls. It's not an official Refloat
  release." UI text only.

### SLCM r26: works properly with stock Refloat (LCM firmware only)
- With stock Refloat 1.2.3 (no VP settings block) the SLCM now follows
  Refloat's light settings from the first poll instead of only after they
  change: Lights On/Off, headlights at the riding level while riding and
  the idle level otherwise, status bar brightness. Behavior modeled on
  Floatwheel's own LCM firmware (the idea only; no code reused - that repo
  has no license).
- Click-click on stock Refloat steps three light levels (headlights
  0/60/100 %, status bar 30/60/100 %), not while riding; it holds until
  Refloat sends different values. With the VP block, clicks still do
  nothing.
- A "lights off" saved from the app is overridden in RAM on stock Refloat
  (never written), so Lights On always means on.
- Unchanged: everything with the VP block (Refloat Glow / SLCM for
  Refloat), the plain ALCM build. SLCM flash: 14,704 of 14,720 bytes (16
  free). New host tests: stock-Refloat path, click-click levels.
- The VESC package is unchanged (no new rf version).

### Mixer tile labels: BLANK / NOTES / 123 (v1.2.3-rf149)
- The MIXER's NOTES pill is now a three-way pill (tap to cycle):
  BLANK (default, plain tiles as before), NOTES (note names E4 ... G5),
  123 (tiles numbered 1-10). Saved in the app (mixerLabels; the rf142
  NOTES on/off setting is no longer used, so everyone starts on BLANK).
  UI only.

### Layout fixes, start notice timing, HORN with the sheet up (v1.2.3-rf150)
- LEDS › HEAD/TAIL LIGHTS (simple view): the RIDING / IDLE rows of the
  HEADLIGHT and TAIL LIGHT cards were drawn on top of each other (the rows
  sat outside the card's layout). Each card now has its own content
  column. UI only.
- MIXER: the header row (4 pills + hint) is narrower so all five tile
  columns fit a phone again (broken since rf142). Tiles unchanged.
- The start notice ("before you start") now opens only once the Refloat
  Glow page is actually on screen, not right after install.
- With the sheet (SUW) up, a tap on the home screen still puts it down,
  except on the HORN button: it sounds the horn as usual and the sheet
  stays up, so horn settings can be tried right away. Same horn limits as
  before (commands.sendHorn, per-press timer, budget).
- Old project names ("Rink-Float", "Re-Float Rink-Mixed", ...) replaced
  with Refloat Glow in docs, comments and file notices (comments only in
  C / Lisp). Identifiers (rinkmixed app id, file names, settings
  category) kept on purpose.

### COLOR 2 for FADE / PULSE / STROBE (v1.2.3-rf151)
- Refloat LED view (HEAD/TAIL ADVANCED, STATUS BAR): the second color of
  FADE, PULSE and STROBE is now labelled COLOR 2 instead of BACKGROUND.
  Same setting and same ON / OFF switch (OFF = black), only the label.
  KNIGHT RIDER keeps BACKGROUND. UI only; SLCM side unchanged.

### LEDS page: BEEPER above SNOOZE, shorter STATUS BAR tile (v1.2.3-rf152)
- Refloat view of the LEDS page: the BEEPER and SNOOZE cards swapped
  places (BEEPER now first). The STATUS BAR tile is the same height as
  HEAD/TAIL LIGHTS (was taller); same picture, same live status bar, same
  function. SLCM view unchanged. UI only.

### LEDS page: SNOOZE under the tiles, compact BEEPER (v1.2.3-rf153)
- Refloat view of the LEDS page, top to bottom: HEAD/TAIL LIGHTS, STATUS
  BAR, SNOOZE, BEEPER (SNOOZE moved back up, between STATUS BAR and
  BEEPER).
- The BEEPER card is smaller: no title (the pill already says BEEPER
  ON / OFF), the two pills and one short line ("Refloat's beeper. OFF also
  mutes the warnings."). Same buttons, same function. UI only; SLCM view
  unchanged.

### SNOOZE reads "5 min BEFORE BOARD POWERS OFF" (v1.2.3-rf154)
- LEDS › SNOOZE (Refloat view): the minutes with their up / down arrows
  sit on the left, followed by "BEFORE BOARD POWERS OFF" (was "BEFORE
  OFF" on the left, minutes on the right). Same setting, same range. UI
  only.

### SNOOZE: one plain line about the power-off time (v1.2.3-rf155)
- The green note under SNOOZE is replaced by one line: "Your board is set
  to power off after idling for 10 minutes." The time comes live from
  VESC's App Settings › General › Shutdown Mode (10 seconds ... 5 hours)
  and is highlighted in green. With no power-off timer: "Your board isn't
  set to power off when idle." UI only.

### SNOOZE (?) window (v1.2.3-rf156)
- The SNOOZE card on the LEDS page (Refloat view) is just the title, a
  (?) button and the minutes row. The explanation moved into a window
  that opens with (?), key words in green:
  - the board's power-off time, read live from VESC (App Settings ›
    General › Shutdown Mode); with no power-off timer, snooze after 10 min
  - snooze kicks in N min before that time to let the rider know the board
    is getting close to powering off; in the last minute the lights blink
    twice every 15 s
  - rolling the wheel forward (or riding) restarts the idle power-off
    clock, ends snooze and brings back the normal idle look; footpads or
    lifting don't
  - the arrows set how many minutes early (NEVER = no snooze)
- Behaviour unchanged (leds.c: the clock restarts on riding, a rolling
  wheel or a motor tone). UI only.

### PERSISTENT FATAL BEEP toggle (v1.2.3-rf157)
- LEDS › BEEPER card (Refloat view): a PERSISTENT FATAL BEEP ON / OFF pill
  with a (?) window and one short line. It is Refloat's own Persistent
  Fatal Error setting (persistent_fatal_error, Refloat Cfg; default ON),
  read from and written to the board like BEEPER ON / OFF: only while the
  board is stopped, and linked both ways with VESC Tool.
- (?) explains: ON latches any firmware fault, even a momentary one, into
  a fatal error (pushback + four beeps) until the board is power-cycled or
  it's cleared in VESC Tool; commonly recommended OFF on known-good boards
  because airtime / deweighting (curb drop, bonk) can cause a brief fault;
  with OFF a fault still beeps while it lasts.
- No firmware change: the package already applies this setting
  (alert_tracker.c). It changes how faults are signalled, so it is a
  safety-relevant setting; the rider chooses it.

### Help windows: plain text, power-off warning explained (v1.2.3-rf158)
- SNOOZE (?) and PERSISTENT FATAL BEEP (?) windows: no highlighted words.
- SNOOZE (?) no longer says the snooze blinks. As leds.c does it: snooze
  uses its own look (BREATHE / DIM / DARK ...); separately, just before
  power-off the lights flash twice every 15 s as a last warning (last
  minute, last 15 s with a 1 min timer), whatever the snooze look, even
  with snooze NEVER. Text only.

### PERSISTENT FATAL BEEP help wording (v1.2.3-rf159)
- (?) window: "any fault, even a momentary one, sets a fatal error that
  keeps beeping until you power-cycle the board". The words "firmware"
  and "pushback and four beeps" were dropped at the user's request. Text
  only; Refloat's behaviour is unchanged.

### HAPTIC FEEDBACK › AT SPEED: 1 mph steps, tap to type (v1.2.3-rf160)
- AT SPEED steps by 1 in the rider's unit (mph or km/h; was 5 km/h).
  Refloat stores Speed Pushback (tiltback_speed) as whole km/h; a whole mph
  rounds to whole km/h and back to the same mph, so every step sticks.
- Tap the number to type it on the neon number pad; DONE sets it (kept
  within 0 - 62 mph / 0 - 100 km/h, 0 = OFF) and closes the pad. A dashed
  line under the number shows it can be tapped.
- Same setting, same write path as before (only while the board is
  stopped). It is also Refloat's Speed Pushback threshold, so the nose
  lifts there too; finer steps only.
- SwStepper gets an optional `editable` / valueClicked() (unused elsewhere).

### Bigger MAX SPEED badge on the home screen (v1.2.3-rf161)
- When a max speed limit is set, the "⚠ MAX 25 MPH" badge at the top of
  the speedo is larger and bolder (about 1.8x text, thick amber border,
  amber-tinted fill), still between the TOP SPEED and MAX DUTY boxes
  (width capped, text shrinks to fit). UI only.

### Optional Refloat donation link in BEFORE YOU START (v1.2.3-rf162)
- Page 1 of the start notice, right after the green thank-you line: a
  small "SUPPORT REFLOAT DEVELOPMENT" section (short text, marked
  completely optional) and an outlined DONATE TO REFLOAT ↗ link that opens
  https://riddimrider.one/donate/ (Refloat's donation page, lukash) in the
  external browser (Qt.openUrlExternally). No reminders, no extra popups;
  the rest of the notice, its scrolling and NEXT are unchanged. UI only.

### Start notice picture back to the original (v1.2.3-rf164)
- rf163 (picture with "vibe code" and pixel sunglasses) reverted at the
  user's request: page 2 of BEFORE YOU START shows the original
  "don't trust the AI code" picture again. Nothing else changed.

### Tour: UPDATES, CREDIT WHERE IT'S DUE, SAME RIDE (v1.2.3-rf165)
- Same 15 tour pages, same order, same icons, dots, SKIP / BACK / NEXT /
  DONE, first-start and "show tips & tricks" behaviour. Three existing
  pages updated in place (no new pages):
  - UPDATES: new text, plus a non-interactive picture of the real
    UPDATES section (SWIPE UP › LEDS › UPDATES, the CHECK FOR UPDATES
    button with a pulsing ring, PACKAGE = DOWNLOAD PACKAGE then install in
    VESC Tool, LIGHT MODULE = FLASH UPDATE over Bluetooth with the board at
    rest). Notes it shows with an SLCM connected and that the tour never
    downloads or flashes. Real update checks / warnings unchanged.
  - CREDIT WHERE IT'S DUE: new text, plus a quiet "Refloat · GPL-3.0 ·
    github.com/lukash/refloat" line. Star icon kept.
  - SAME RIDE: new text (core riding behaviour retained; optional settings
    like MAX SPEED change how the board behaves). Still the last page with
    DONE; warning icon kept.
- Each tour page scrolls if it doesn't fit (small phones). UI only.

### Onboarding Tour walks through the real interface (v1.2.3-rf166)
- Terms from now on: "Onboarding Tour" (the guided slides, `rinkTour`)
  and "SUW" (the Swipe-Up Window, `bottomSheet`).
- Each Onboarding Tour page now shows its feature in the real app: it
  opens the SUW on the right tab, scrolls the control into view and puts a
  pulsing neon spotlight on it, with the rest of the screen dimmed. The
  card moves to the other half of the screen and points at the control.
  - HOME SCREEN: the speedo. PULL-UP SHEET: the SUW tab bar. HORN: the
    HORN bar. THE MIXER: the tiles. RECORD A MELODY: LIVE / REC / BEEP.
    PLAY & SAVE: CLEAR / PLAY / SAVE / DONE (shown in REC mode), else REC.
    MY MELODIES: the MY MELODIES / RECORD row in CONTROL. BOARD STYLE: the
    tab. LIGHTS: HEAD/TAIL LIGHTS + STATUS BAR tiles. BATTERY GAUGE,
    BEEPER, UPDATES: their LEDS tiles / cards; when a board doesn't have
    one (e.g. SLCM-only tiles on a Refloat-LED board) the LEDS tab itself
    is spotlighted, and UPDATES keeps its picture of the section.
  - WELCOME, CREDIT WHERE IT'S DUE and SAME RIDE stay centered cards (SUW
    down).
- Strictly visual: the whole screen stays covered by the tour's blocking
  layer (spotlight included), nothing is pressed, no setting is written and
  no board command is sent. The SUW position and tab are put back when the
  tour closes. The SUW itself is unchanged.
- Fix: MY MELODIES page said "MIXER › MY MELODIES"; the button is in
  CONTROL ("CONTROL › MY MELODIES").
- Ids added to the controls the tour points at (no behaviour change).

### Settings › RIDE: HAPTIC FEEDBACK moved here, new PUSHBACK page (v1.2.3-rf167)
- Settings (gear) has a third tab, RIDE, with HAPTIC FEEDBACK and
  PUSHBACK. The HAPTIC FEEDBACK button is gone from SUW › CONTROL (BEEPER
  stays there). The haptic window itself is unchanged (STRENGTH, AT DUTY,
  AT SPEED in 1 mph steps with tap-to-type).
- PUSHBACK window: Refloat 1.2.3's own settings (Refloat Cfg), with
  Refloat's ranges and steps, no new behaviour:
  - DUTY & SPEED: AT DUTY (tiltback_duty, 50-100 %, 100 = off, same as
    haptic), AT SPEED (tiltback_speed, 1 mph steps / tap to type, 0 = off),
    ANGLE (tiltback_duty_angle 0-30°), RATE (tiltback_duty_speed 0-30 °/s);
    red note above 85 % duty.
  - HIGH VOLTAGE / LOW VOLTAGE: AT (tiltback_hv / _lv 0-150 V, 0.05 steps,
    per cell on VESC 6.05+), ANGLE, RATE.
  - RETURN RATE (tiltback_return_speed 0-10 °/s).
  - SHOW ADVANCED: CONSTANT TILTBACK (angle -10..10°, above ERPM 200-100000)
    and VARIABLE TILTBACK (rate 0-5 °/1000 ERPM, target -10..10°, start
    ERPM 0-100000); ERPMs can be typed on the number pad.
  - Each card has a (?) with Refloat's explanation in plain words.
- Values are read from the board's Refloat config when the window opens;
  changes are written 1.5 s after the last tap, only while the board is
  stopped (they wait otherwise), then confirmed: after the board ACKs the
  write the app reads the config back and compares ("Saved and confirmed
  on the board" / a warning naming any field that differs, or "Couldn't
  confirm" after 5 s). The haptic window uses the same confirmation.
- Number pad: accepts as many digits as the field's maximum needs (ERPM).
- Onboarding Tour: new RIDE SETTINGS page after HORN: spotlights the gear
  and shows a picture of Settings › RIDE (Settings is a window above the
  tour, so it isn't opened; nothing is applied). Tour steps are now looked
  up by page title. 16 pages.

### Onboarding Tour: no flicker on NEXT / BACK (v1.2.3-rf168)
- Cause (app and simulator alike): on every page change the tour dropped
  its spotlight and target, then picked the new control 300 ms later once
  the SUW had slid into place. For those 300 ms the card had no spot, so it
  jumped to the centre at full size with the whole screen dimmed, then
  jumped back next to the new control.
- Fix: the new control is picked right away and the spotlight stays on
  while the SUW moves; it glides (140 ms ease) to the new control and
  follows the SUW as it slides. The 300 ms check only re-confirms the
  control and scrolls it into view. The spot only switches off on pages
  without a control (WELCOME, CREDIT, SAME RIDE). A spot that can't be
  measured for a frame keeps its last position instead of blinking off.
- Simulator: same fix; one pending settle and one content swap at a time
  (rapid NEXT / BACK can't stack timers or land on a stale page).
- Measured in the simulator, frame by frame: before, the spotlight dropped
  out on every NEXT between pages with a control (12 of 12); after, 0
  (it only clears when moving to a centred page). CI load test pages
  through the tour every ~190 ms and logs spotlight drops (TOURDROPS).
