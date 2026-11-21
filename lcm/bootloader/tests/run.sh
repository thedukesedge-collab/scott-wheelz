#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Part of Refloat Glow (formerly Refloat rink-mixed), 2026. See FORK_NOTES.md.
# Host tests of the LCM bootloader logic (bootloader.c with -DBL_HOST), and
# the end-to-end relay test with the package's real src/lcm_fw.c.
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
gcc -O2 -Wall -Wextra -Wno-unused-parameter -DBL_HOST -I"$here" -I"$here/.." \
    "$here/../bootloader.c" "$here/test_bootloader.c" -o "$out/t"
"$out/t"
# lcm_fw.c next to stub state.h/time.h/vesc_c_if.h (quoted includes are
# looked up in the including file's own folder first)
mkdir -p "$out/relay"
cp "$here/relay_stub/"*.h "$here/../../../src/lcm_fw.c" "$here/../../../src/lcm_fw.h" "$out/relay/"
gcc -O2 -Wall -Wextra -Wno-unused-parameter -DBL_HOST -I"$here" -I"$here/.." -I"$out/relay" \
    "$here/../bootloader.c" "$out/relay/lcm_fw.c" "$here/test_relay.c" -o "$out/r"
"$out/r"
rm -rf "$out"
