# LCM firmware (ALCM, VP light control)

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

## Protocol

VP appends a block to refloat's `COMMAND_LCM_POLL` reply (bytes 0-14 are
unchanged; 12 = headlight %, 14 = status bar % as before):

| Byte | Meaning |
| --- | --- |
| 15 | magic `0x56` ('V') |
| 16 | version `1` |
| 17 | flags: bit0 headlights on, bit1 status bar on, bit2 beeper on |
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
