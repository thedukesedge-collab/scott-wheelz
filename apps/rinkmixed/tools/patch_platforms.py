#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Part of Refloat rink-mixed (refloatRM), 2026. See FORK_NOTES.md.
"""Run after `flutter create .` in CI: Bluetooth permissions, names, icons-free tweaks."""
import pathlib, re, sys

root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")

# ---- Android ----
man = root / "android/app/src/main/AndroidManifest.xml"
s = man.read_text()
perms = """    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
    <uses-feature android:name="android.hardware.bluetooth_le" android:required="true" />
"""
if "BLUETOOTH_SCAN" not in s:
    s = re.sub(r"(<manifest[^>]*>\n)", r"\1" + perms, s, count=1)
s = re.sub(r'android:label="[^"]*"', 'android:label="Rink-Mixed"', s, count=1)
man.write_text(s)

for g in [root / "android/app/build.gradle", root / "android/app/build.gradle.kts"]:
    if g.exists():
        t = g.read_text()
        t = re.sub(r"minSdk(Version)?\s*=?\s*flutter\.minSdkVersion", lambda m: m.group(0).replace("flutter.minSdkVersion", "21"), t)
        g.write_text(t)

# ---- iOS ----
plist = root / "ios/Runner/Info.plist"
p = plist.read_text()
add = """	<key>NSBluetoothAlwaysUsageDescription</key>
	<string>Rink-Mixed uses Bluetooth to connect to your board.</string>
	<key>NSBluetoothPeripheralUsageDescription</key>
	<string>Rink-Mixed uses Bluetooth to connect to your board.</string>
"""
if "NSBluetoothAlwaysUsageDescription" not in p:
    p = p.replace("<dict>\n", "<dict>\n" + add, 1)
p = re.sub(r"(<key>CFBundleDisplayName</key>\s*<string>)[^<]*(</string>)", r"\1Rink-Mixed\2", p)
plist.write_text(p)
print("platform files patched")
