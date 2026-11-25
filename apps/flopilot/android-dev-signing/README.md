# FloPilot Android development signing key

`flopilot-dev.jks` (password `flopilot-dev`, alias `flopilot`) signs the test
APKs that CI builds from the `flopilot` branch, so a new build installs over
the old one and the app keeps its saved board and settings.

It is public on purpose (test builds only): anyone could sign an APK with it.
Before sharing the app beyond a few testers (or for Google Play), make a
private key and store it as repository secrets `FP_ANDROID_KEYSTORE_B64`
(base64 of the .jks) and `FP_ANDROID_KEYSTORE_PASS` (store and key password,
alias `flopilot`); the workflow then uses those instead. Switching keys means
uninstalling the test build once.
