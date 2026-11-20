# Rink-Mixed app (Android + iPhone)

The phone app for VESC Onewheels running the Re-Float Rink-Mixed package. It is
the simulator UI (`tools/simulator/index.html`, one source for both) wrapped as
an Android app with [Capacitor](https://capacitorjs.com), plus a Bluetooth link
to the board's VESC Express.

- **CONNECT BOARD** (top of the screen) picks the board over Bluetooth; it
  reconnects to the last board on start. While connected the home screen shows
  the board's real data, and HORN and TILT work on the board.
- Not connected: the simulator's demo board, as before.

## Build
GitHub Actions (`.github/workflows/app-android.yml`) builds a signed APK on
every push to the `app-android` branch: release **app-android-latest**,
`rinkmixed-android.apk`.

iPhone: `.github/workflows/app-ios.yml` checks the iOS app builds (iOS
Simulator, unsigned) on every push. Installing on an iPhone needs Apple
signing: TestFlight once an Apple Developer account's App Store Connect API key
is added as repository secrets.

Locally: `npm install`, `npm test`, `npm run www`, `npx cap add android`,
`npx cap sync android`, then build in Android Studio.

## Layout
- `src/vesc.js` VESC packet framing + byte helpers (ported from the Flutter app)
- `src/board.js` Refloat polling (INFO, realtime IDs, realtime data, values),
  horn, tilt -> the simulator's state
- `src/app.js` Bluetooth (Nordic UART on VESC Express) + the CONNECT pill
- `scripts/build-www.mjs` simulator page + app layer -> `www/`
- `scripts/test-protocol.mjs` protocol tests with a fake board

## Status (phase 1)
Live home screen, horn, tilt. Tunes, LEDs, mixer, config: next phases (they
still run on the demo board in the app for now).
