#!/bin/sh
# Host tests of the LCM bootloader logic (bootloader.c with -DBL_HOST).
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
gcc -O2 -Wall -Wextra -Wno-unused-parameter -DBL_HOST -I"$here" -I"$here/.." \
    "$here/../bootloader.c" "$here/test_bootloader.c" -o "$out/t"
"$out/t"
rm -rf "$out"
