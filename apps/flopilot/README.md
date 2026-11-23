# FloPilot

FloPilot is the phone app for Refloat Glow boards: the Refloat Glow UI
(`tools/simulator/index.html`) wrapped with Capacitor, talking to the board
over VESC Express Bluetooth (Nordic UART). With no board connected it runs as
the demo board.

- Bundle ID: `com.flopilot.app` (App Store name "FloPilot (Float Pilot)")
- Build: `npm install && npm test && npm run www`, then `npx cap add ios`
- iPhone / TestFlight: `.github/workflows/flopilot-ios.yml` (branch `flopilot`)

License: GPL-3.0-or-later (see `LICENSE` in the repo root). Refloat Glow is an
independent modification of Refloat and is not affiliated with its authors.
