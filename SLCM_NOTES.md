# SLCM for Refloat: what differs from Refloat 1.2.3

Base: Refloat v1.2.3, commit 800a46b (lukash/refloat), unchanged except for
the list below. Verify with `git diff 800a46b slcm-for-refloat`.
Part of the Refloat Glow side project; not affiliated with the Refloat developers.

## v1.2.3-slcm1

### Added (SLCM support, the same code as Refloat Glow)
- `src/lcm.c` / `src/lcm.h`: when an SLCM (light module named `VP-` /
  `VPBL-`, or an older SLCM that doesn't name itself) polls, the poll reply
  carries the SLCM settings block (headlights, status bar, beeper, test beep,
  animations, color, battery %, update flag). Any other light module gets the
  stock reply, exactly as before. Once SLCM settings have been sent from the
  app, the SLCM gets its reply whatever the LED Mode; any other module still
  needs LED Mode External, as in stock.
- `src/lcm_fw.c` / `.h` (new): SLCM firmware update relay (bootloader SLCMs).
- `src/battery_curve.c` / `.h` (new): battery % from a per-cell curve for the
  SLCM status bar.
- `src/main.c`: four commands (126 settings, 127 test beep, 128 firmware
  relay, 129 battery curve; the same numbers as Refloat Glow). Settings and
  the curve are saved in custom EEPROM words 80-87 and 125-127 (the Refloat
  config uses 0-79), from the aux thread, 2 s after the last change and only
  with the board at rest (not running, footpads empty, wheel still).
- `ui.qml.in`: an **SLCM Light Module** button in Control and its window
  (stock Refloat styling). Nothing else in the UI changed.

### Safety-related additions (they can only prevent starting, never stop a ride)
- No starting (normal, darkride or push-start) while an SLCM settings save is
  writing flash or while the SLCM is updating / in its bootloader; three
  short beeps if someone tries.
- One flash writer at a time: the stock config write and the SLCM saves take
  the same mutex.

### Kept from Refloat 1.3 (bounds fixes in the light module code path)
- Light control request: brightness clamped to 100 %, payload length capped
  to the buffer (a bad packet could overflow it in 1.2.3).
- LCM name parsing: off-by-one read past the received bytes fixed.

### Not included (Refloat Glow only)
Horn and melodies, Neon theme and UI, controller-LED features, remote tilt
cap, IMU timeout and flywheel backports, config-save-at-rest changes, and
every other Refloat Glow change.

### Unchanged
Balance, motor control, PID, ATR, torque / turn / brake tilt, booster,
footpad logic, alerts, the Refloat config layout (tunes carry over to and
from stock 1.2.3), and the stock light module behavior for non-SLCM modules.
