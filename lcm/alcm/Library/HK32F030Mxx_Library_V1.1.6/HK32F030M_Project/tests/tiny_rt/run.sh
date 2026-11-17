#!/bin/sh
# Host test of src/tiny_rt.c (the GCC firmware build's division and mem*).
set -e
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
gcc -O2 -Wall -DTINY_RT_HOST_TEST "$here/../../src/tiny_rt.c" "$here/tiny_rt_test.c" -o "$out/t"
"$out/t"
rm -rf "$out"
