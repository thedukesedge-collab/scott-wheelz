# Refloat rink-mixed (refloatRM)

**refloatRM is an independent, unofficial modification of [Refloat](https://github.com/lukash/refloat) v1.2.3**, the VESC package for self-balancing skateboards by Lukáš Hrázký (lukash). It adds a Neon Future app UI, a motor horn and melodies, controller- and LCM-driven lights, and an optional light module firmware. The balance and motor-control code is Refloat's, unchanged.

- **Not an official Refloat release**, and not endorsed by or affiliated with the Refloat developers. Please don't report refloatRM issues to the Refloat project; open them here.
- **Credit:** None of this would be possible without all the hard work Lukáš Hrázký (lukash) and the Refloat community have put into Refloat. Thank you! Refloat by Lukáš Hrázký; Float by Mitch Lustig, Dado Mista and Nico Aleman, based on work by Benjamin Vedder. The light module firmware in `lcm/alcm` is [ALCM](https://github.com/MrTheConductor/alcm) by Mitchell White. Third-party code: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
- **License:** GNU GPL v3.0 or later, the same as Refloat ([LICENSE](LICENSE)). Every build names the commit it was built from, and its full source is in this repository.
- **What changed and why:** [FORK_NOTES.md](FORK_NOTES.md). Files changed from Refloat carry the line *"Modified by Refloat rink-mixed (refloatRM)"*.
- **Made with AI assistance.** Successful builds and limited testing don't guarantee safety.

### Downloads
- **Rider releases** are tagged `v1.2.3-rfN` on the [Releases](https://github.com/thedukesedge-collab/scott-wheelz/releases) page, published only after the bench checklist in [RELEASING.md](RELEASING.md).
- **`dev-build`** is the latest untested build of `main`: bench testing only, wheel off the ground, **not for riding**.
- **Stock Refloat** (unmodified, as published by lukash) is mirrored on the `refloat-stock` release, and the package can switch back to it.

> **Warning:** this firmware runs self-balancing vehicles that can cause serious injury. Modified firmware may behave unexpectedly, including loss of balance or motor control. Back up your Refloat config, test on a stand first, and ride at your own risk. Provided without warranty.

---

*Everything below is Refloat's own README, unchanged. "Refloat" there means the upstream project.*

# Refloat - VESC Package
Refloat is a VESC Package for self-balancing skateboards. It aims to:
- Provide a polished and full-featured user experience
- Maintain a clean and reliable codebase that is easy to extend
- Make it easy for anyone to use Refloat as a base for experimentation
- Standardize interfaces so that package clones can interact with other parts of the ecosystem (3rd party apps, light modules, VESC Express addons etc.) in a compatible way

_**If you're looking for README of the actual package, you can find it [here](package_README.md).**_

## Contributing
Contributions are welcome and appreciated, please refer to [Contributing](CONTRIBUTING.md).

## Building
### Requirements
- `gcc-arm-embedded` version 13 or higher
- `make`
- `vesc_tool`

To build the package, run:
```sh
make
```

Note a new beta (as of writing this, unreleased) version of `vesc_tool` is needed for the above to work. To build the package with the current / old `vesc_tool` version, use:
```sh
make OLDVT=1
```

If you don't have `vesc_tool` in your `$PATH` (but you have, for example, a downloaded `vesc_tool` binary), you can specify the `vesc_tool` to use:
```sh
make VESC_TOOL=/path/to/vesc_tool
```
For macOS, the path to VESC Tool when installed using the official installer is as follows:
```sh
make VESC_TOOL="/Applications/VESC Tool.app/Contents/MacOS/VESC Tool"
```

## Documentation
[Development Documentation](doc/index.md)
