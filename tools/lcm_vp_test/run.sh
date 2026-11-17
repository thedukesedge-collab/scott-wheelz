#!/bin/sh
# Host test of the light module (ALCM VP) settings in src/lcm.c: clamping,
# EEPROM pack/unpack, and the exact LCM poll reply bytes ALCM reads.
# lcm.c/lcm.h are copied next to small stub headers, since quoted includes
# are looked up in the including file's own folder first.
set -e
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
cp -r "$here/stub/." "$work/"
cp "$here/../../src/lcm.c" "$here/../../src/lcm.h" "$here/test.c" "$work/"
gcc -Wall -Wno-unused-parameter -I"$work" "$work/test.c" "$work/lcm.c" -lm -o "$work/t"
"$work/t"
rm -rf "$work"
