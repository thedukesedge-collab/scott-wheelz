# Re-Float Rink-Mixed app

Standalone phone app (iPhone + Android, Flutter) for VESC Onewheels running the
Re-Float Rink-Mixed package. It talks to the board over Bluetooth the same way
the package's VESC Tool screen does, and mirrors its look.

## Status
- Phase 1 (this): connect over VESC Express BLE, live home screen, horn, tilt,
  move, flywheel, data graph/text, board style, ride peaks, demo board.
- Phase 2: tunes (save / apply / colors).
- Phase 3: full Refloat Cfg editor.

## Builds
GitHub Actions (`.github/workflows/app.yml`) builds on every push to `apps/`:
- `rinkmixed.apk` (Android, installable directly)
- `rinkmixed-unsigned.ipa` (iPhone, only when the commit message has `[ios]`
  or the workflow is run by hand; needs signing / sideloading to install)

Both are attached to the `app-latest` release.

The platform folders (`android/`, `ios/`) are generated in CI with
`flutter create` and patched by `tools/patch_platforms.py`.
