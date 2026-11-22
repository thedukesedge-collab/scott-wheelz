# SLCM for Refloat (branch `slcm-for-refloat`)

**Unofficial.** Stock Refloat **v1.2.3** (commit 800a46b) by Lukáš Hrázký (lukash) with **only** SLCM light module support added. Built as a controlled variant of the [Refloat Glow](https://github.com/thedukesedge-collab/scott-wheelz) side project: the same SLCM features, nothing else. Not affiliated with or endorsed by the Refloat developers. GPL-3.0-or-later ([LICENSE](LICENSE)).

*None of this would be possible without all the hard work Lukáš Hrázký (lukash) and the Refloat community have put into Refloat. Thank you!*

- **Exactly what changed:** [SLCM_NOTES.md](SLCM_NOTES.md), or `git diff 800a46b slcm-for-refloat`.
- **Dev builds** (bench testing only, not for riding): release `slcm-for-refloat-dev`.
- The SLCM firmware itself is built from `lcm/` on the `main` branch (ALCM by Mitchell White, GPL-3.0, with SLCM changes) and published on the `lcm-latest` / `lcm-dev` releases.

> **Warning:** firmware for self-balancing vehicles can cause serious injury. Test on a stand first. Provided without warranty.

---

*Everything below is Refloat's own README, unchanged.*

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
