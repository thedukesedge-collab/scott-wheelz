#!/bin/sh
# Host test of the light module (ALCM VP) settings in src/lcm.c: clamping,
# EEPROM pack/unpack, and the exact LCM poll reply bytes ALCM reads.
set -e
cd "$(dirname "$0")"
gcc -Wall -Wno-unused-parameter -Istub -I../../src test.c ../../src/lcm.c -lm -o /tmp/lcm_vp_test
/tmp/lcm_vp_test
