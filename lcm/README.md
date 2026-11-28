# LCM firmware (ALCM, VP light control)


> **Bootloader / updates from the app:** `SLCM.hex` (bootloader +
> light firmware) is flashed once with the ST-Link; after that the LCM is
> updated from the phone app (LEDS › hold "LCM firmware tools" at the bottom for 2 s). The app only offers FLASH LCM when the LCM reports itself as `VPBL-...` (built with the bootloader). See
> [bootloader/README.md](bootloader/README.md).
> **Compatibility:** for ADV200 controllers (Floatwheel XRV, GTV, PintV) and their light modules only. The package's LEDS tab does nothing unless the LCM runs this ALCM VP firmware.

`alcm/` is [ALCM](https://github.com/MrTheConductor/alcm) v0.1.1 by
Mitchell White (GPL-3.0), the light control module firmware for the
HK32F030M-based LCM, with Refloat Glow changes (`VP_LIGHT_CONTROL`):

- **All light settings come from VP** (the VESC package): headlights on/off
  and brightness, status bar on/off and brightness (separate idle / riding
  levels), the idle, riding, snooze (dozing), boot and shutdown animations,
  the personal color, and the beeper.
- **The power button keeps its power functions**: hold to power off (and the
  normal wake/power-on), plus the factory reset (hold through boot).
  Click-hold (config mode) no longer does anything, and the footpads no
  longer adjust settings.

**On stock Refloat** (no VP block in the poll reply; SLCM r26+) the SLCM
follows Refloat's own light settings from the first poll: Lights On/Off,
headlights at Refloat's riding brightness while riding and its idle
brightness otherwise, and the status bar brightness (Refloat Cfg › LEDs).
Lights are switched on in RAM so a "headlights off" saved earlier from the
app can't keep them dark (nothing is written to EEPROM on this path).
**Click-click** (not while riding) steps the light levels with an ACK:
30 % → 60 % → 100 % → headlights off (status bar stays at 30 % for battery
and footpads) → 30 % ... (headlights / status bar). On stock Refloat the
level holds until Refloat sends different values. Animations, color and
beeper keep their saved settings. With the VP block (Refloat Glow, SLCM for
Refloat) none of this applies and clicks do nothing. The plain ALCM build
(`VP=0`) is unchanged.

**No light data at all** (SLCM r34+): the VESC answers, but for 5 s no
reply carries light data - Refloat's LED Mode not External (Refloat then
answers with empty data), another package (Float...) or no package. The SLCM
then runs its lights itself: switched on at 60 %, click-click steps the
levels above. As soon as light data arrives, Refloat's values (or the VP
block) take over again. With no VESC answer at all the lights stay as they
are (booting / lost link).

**Power** never depends on the controller (SLCM r36+, like Dado's
Floatwheel LCM firmware): the press powers the module and the bootloader
latches it at once; booting ends after the VESC's first answer or after 4 s,
whichever comes first, so the normal hold-to-off (0.5 s + 1 s with the
shutdown beeps) always works - also while the board is locked (Refloat
Lock / Disable Package; letting go early goes back to locked). The button
held 3 s always powers off (r32+), as a backstop.

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
| 25 | battery % from the package's curve, `0xFF` = use the VESC's |
| 26-33 | CUSTOM animation (SLCM r19-r40; still sent to r41+, which ignore it - the layout never moves) |
| 34-48 | SLCM r41+: own colors / speed per animation, 3 bytes each for idle, riding, snooze, boot, shutdown: hue 1 / 2, hue 2 / 2, speed in tenths (10 = normal); speed 0 = the animation's built-in colors and speed |
| 49 | SLCM r41+: riding duty bar from this % (1-85), 0 = 70 % |

The package sends each part only to an SLCM whose revision (from its name,
`VPBL-r<N>-...`) understands it; an SLCM accepts shorter replies. See
CLAUDE.md "SLCM update rules".

Animation numbers (`animation_option_t`, all animations enabled):
0 none, 1 rainbow scan, 2 rainbow mirror, 3 knight rider, 4 rainbow bar,
5 the fuzz, 6 fire, 7 expanding pulse, 8 imploding pulse, 9 120 scroll,
10 complementary wave, 11 floatwheel classic, 12 personal scan, 13 battery
gauge (r16+); 14 was CUSTOM (r19-r40).

With own colors (r41+), color 1 is the RGB color of the single-color
animations (knight rider, personal scan, floatwheel classic) and colors
1..2 are the hue range of the others; speed runs the whole animation faster
or slower.

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
