# LCM firmware (ALCM, VP light control)


> **Bootloader / updates from the app:** `ALCM_VP_BL_full.hex` (bootloader +
> light firmware) is flashed once with the ST-Link; after that the LCM is
> updated from the phone app (LEDS › hold "LCM firmware tools" at the bottom for 2 s). The app only offers FLASH LCM when the LCM reports itself as `VPBL-...` (built with the bootloader). See
> [bootloader/README.md](bootloader/README.md).
> **Compatibility:** for ADV200 controllers (Floatwheel XRV, GTV, PintV) and their light modules only. The package's LEDS tab does nothing unless the LCM runs this ALCM VP firmware.

`alcm/` is [ALCM](https://github.com/MrTheConductor/alcm) v0.1.1 by
Mitchell White (GPL-3.0), the light control module firmware for the
HK32F030M-based LCM, with Re-Float Rink-Mixed changes (`VP_LIGHT_CONTROL`):

- **All light settings come from VP** (the VESC package): headlights on/off
  and brightness, status bar on/off and brightness (separate idle / riding
  levels), the idle, riding, snooze (dozing), boot and shutdown animations,
  the personal color, and the beeper.
- **The power button keeps its power functions**: hold to power off (and the
  normal wake/power-on), plus the factory reset (hold through boot). Clicks
  and click-hold (config mode) no longer change anything, and the footpads
  no longer adjust settings.

Without a VP package that sends light settings, ALCM keeps its saved
settings and behaves like stock v0.1.1 apart from the button changes.

Battery gauge colors: white above 55 %, yellow 55-41 %, orange 40-30 %, red
below 30 %, flashing red at 5 % and below; shown while riding from 40 % down.

A VESC that stops answering (package install/update, VESC reboot) no longer
puts the module into a latched, beeping fault unless you are riding; a
mid-ride comm fault clears itself when the VESC answers again.

## Protocol

VP appends a block to refloat's `COMMAND_LCM_POLL` reply (bytes 0-14 are
unchanged; 12 = headlight %, 14 = status bar % as before - VP sends the
riding or idle level for each depending on whether the board is engaged,
and 13 = the idle headlight %):

| Byte | Meaning |
| --- | --- |
| 15 | magic `0x56` ('V') |
| 16 | version `1` |
| 17 | flags: bit0 headlights on, bit1 status bar on, bit2 beeper on, bit3 toggles once per test-beep request (one 160 ms beep) |
| 18 | idle animation |
| 19 | riding animation |
| 20 | dozing (snooze) animation |
| 21 | boot animation |
| 22 | shutdown animation |
| 23-24 | personal color, degrees (0-359), big endian |

Animation numbers (`animation_option_t`, all animations enabled):
0 none, 1 rainbow scan, 2 rainbow mirror, 3 knight rider, 4 rainbow bar,
5 the fuzz, 6 fire, 7 expanding pulse, 8 imploding pulse, 9 120 scroll,
10 complementary wave, 11 floatwheel classic, 12 personal scan.

## Building

`alcm/gcc/` builds with arm-none-eabi-gcc (`make` → `build/ALCM_VP.hex`,
`make VP=0` → stock behavior). The Keil project in `alcm/Project/MDK5`
still works too (add `VP_LIGHT_CONTROL` to its defines). CI
(`.github/workflows/lcm.yml`) builds both, checks the size, runs ALCM's
unit tests (stock + VP), and publishes the .hex to the `lcm-latest` release.

## Flashing

Same as stock ALCM: ST-Link/SWD with pyocd, see
`alcm/tools/battery_lut_patch/README.md` (`flash` command), or Keil.
Keep your current ALCM v0.1.1 .hex to flash back if needed.
