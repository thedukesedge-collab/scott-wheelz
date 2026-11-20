# Development signing key

`rinkmixed-dev.jks` (password `rinkmixed-dev`, alias `rinkmixed`) signs the
dev APKs built by CI on the `app-android` branch, so a new build installs over
the old one and the app keeps its saved board and settings.

It is public on purpose (development only): anyone could build an APK with it.
Before sharing the app with other riders, make a private key and store it as
repository secrets `RM_APP_KEYSTORE_B64` (base64 of the .jks) and
`RM_APP_KEYSTORE_PASS`; the workflow uses those instead when they exist.
(Switching keys means uninstalling the dev build once.)
