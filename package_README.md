# Re-RinkFloat

**Re-RinkFloat is a vibe-coded fork of [Refloat](https://github.com/lukash/refloat) 1.2.3.** All the changes below were made with an AI coding assistant on top of Refloat. All credit for the package itself goes to **Lukáš Hrázký (lukash)**, the author of Refloat, and to the original Float authors (Mitch Lustig, Dado Mista, Nico Aleman, based on work by Benjamin Vedder). Licensed under GPL-3.0. Not an official Refloat release, so please don't report Re-RinkFloat issues to the Refloat project.

## New in Re-RinkFloat
- **Haptic horn**: a big HORN bar on the main screen. Hold or tap it: full haptics while riding (up to 3 s per press), tone only when idle (up to 2 s). A heat budget allows repeated taps as long as it stays safe. Real pushback and alerts always override it. If the app disconnects, the horn stops within 0.5 s.
- **TILT slider** on the main screen: +-4 deg remote tilt that springs back to 0 when released. The package hard-caps remote tilt at +-4 deg.
- **Tron-style main screen**: neon green/purple theme, digital segmented speedometer with a duty ring, a battery bar that fades from green to red, and a full-screen layout.
- **Side view**: detailed board drawing (tire, hub, fender, footpads, NOSE/TAIL). Long-press it to flip the direction.
- **Front (roll) view**: the nose block with its lightbar slides up the tire as the nose rises.
- **Footpad view**: the ADC1/ADC2 halves glow when engaged.
- **Pull-up panel**: Tunes / Control / Data / Board Style live in a panel that snaps open or closed. The main screen shrinks to fit above it.
- **Board Style tab**: Straight or WTF rails, treaded or slick tire, normal or flipped side view.

A full-featured self-balancing skateboard package.

## New in Refloat 1.2
- BMS alerting support (Pushback and Haptic)
- Speed-based alerting (Pushback and Haptic)
- Automatic config migration when updating the package
- Support for per-cell High/Low Voltage Thresholds (no more needing to adjust them in Specs after a fresh install, 6.05+ only)
- Groundworks for a new alerting system, for now only providing firmware fault reporting
- New Rainbow Fade, Rainbow Cycle and Rainbow Roll LED effects

For more details, read the [1.2 release post](https://pev.dev/t/refloat-version-1-2/2795).

## Installation
### Upgrading
Back up your package config (either by **Backup Configs** on the Start page, or by saving the XML on **Re-RinkFloat Cfg**).

If upgrading from Refloat 1.1 to 1.2, an automatic config restore dialog should pop up. Confirm it to restore. This is the preferred way to update the config from now on, as it will also migrate any changed options to the new version. Other methods of restoring the config will not do that.

In case the automatic config restore fails, restore it the traditional way and please report the issue, so that it can be fixed.

### Fresh Installation
If doing a fresh board installation, you need to do the **motor** and **IMU** calibration and configuration. If you install the package before that, you need to disable the package before running the **motor** _calibration_ and re-enable it afterwards.

For a detailed guide, read the [Initial Board Setup guide on pev.dev](https://pev.dev/t/initial-board-setup-in-vesc-tool/2190).

On 6.05+ firmware, the package should ride well without the need to configure anything in Refloat Cfg. On 6.02, the **Low and High Tiltback voltages** on the **Specs** tab of **Refloat Cfg** still need to be set according to your battery specs.

## Disclaimer
**Use at your own risk!** Electric vehicles are inherently dangerous, authors of this package shall not be liable for any damage or harm caused by errors in the software. Not endorsed by the VESC project.

## Credits
Refloat author: Lukáš Hrázký (lukash). All credit for Refloat goes to him.

Re-RinkFloat changes: vibe-coded with an AI coding assistant.

Original Float package authors: Mitch Lustig, Dado Mista, Nico Aleman

## Downloads and Changelogs
Refloat: [https://github.com/lukash/refloat/releases](https://github.com/lukash/refloat/releases)

Re-RinkFloat: [https://github.com/thedukesedge-collab/scott-wheelz](https://github.com/thedukesedge-collab/scott-wheelz)
