Re-Float Rink-Mixed rider release: a custom remix of Refloat, vibe-coded on top of Refloat (all credit to Refloat's author, Lukas Hrazky, and ALCM's author, Mitchell White).

**Use at your own risk.** Back up your Refloat config first, and test with the wheel off the ground after installing.

Files:
- `refloatrm.vescpkg`: the VESC package (VESC Tool > VESC Packages > load custom).
- `SLCM.hex`: light module firmware with bootloader (ST-Link once; after that, update from the app: LEDS > LCM firmware tools).
- `ALCM_VP_app.bin`: the light module update image.

This release went through the bench checklist in RELEASING.md before it was tagged. Untested builds from the main branch are on the "dev-build" / "lcm-dev" pre-releases and are not for riding.
