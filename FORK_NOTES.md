# Scott Wheelz fork notes

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
- HORN pill next to the board name (hold; riding only).

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
  always override it. Not active when idle or in Handtest.
- Code: `src/haptic_feedback.c/.h` (HAPTIC_FEEDBACK_HORN), `src/main.c`
  (command handler), `ui.qml.in` (`sendHorn`, hornButton, 10 Hz timer).

### Board orientation view
- App: **Board** tab shows side view (pitch), rear view (roll) and top view
  (yaw) drawn from the IMU's fused angles, updated at the existing 10 Hz
  realtime-data rate, with numeric readouts.
- `imu.yaw` was appended to `RT_DATA_ITEMS` in `src/rt_data.h` (items are
  matched by name, so appending is safe). Yaw is gyro-integrated and drifts.
- Tron theme on the Board tab: neon green + purple on a dark grid, detailed
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
