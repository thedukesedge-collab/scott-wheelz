Refloat Glow rider release: a custom remix of Refloat, vibe-coded on top of Refloat (all credit to Refloat's author, Lukas Hrazky, and ALCM's author, Mitchell White).

**Use at your own risk.** Back up your Refloat config first, and test with the wheel off the ground after installing.

Files:
- `refloatglow.vescpkg`: the VESC package (VESC Tool > VESC Packages > load custom).
- `SLCM.hex`: light module firmware with bootloader (ST-Link once; after that, update from the app: LEDS > LCM firmware tools).
- `ALCM_VP_app.bin`: the light module update image.

This release went through the bench checklist in RELEASING.md before it was tagged. Untested builds from the main branch are on the "dev-build" / "lcm-dev" pre-releases and are not for riding.

**License and source.** Refloat Glow is an unofficial modification of [Refloat](https://github.com/lukash/refloat) and is not affiliated with or endorsed by the Refloat developers. It is licensed under the GNU GPL v3.0 or later ([LICENSE](https://github.com/thedukesedge-collab/scott-wheelz/blob/main/LICENSE)). The complete source for this release is the tagged commit (the "Source code" downloads below). The light module firmware is ALCM by Mitchell White (GPL-3.0) with Refloat Glow changes, and contains ARM CMSIS (BSD-3-Clause). See [THIRD_PARTY_NOTICES.md](https://github.com/thedukesedge-collab/scott-wheelz/blob/main/THIRD_PARTY_NOTICES.md). Provided without warranty.
