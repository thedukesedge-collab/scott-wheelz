#!/bin/sh
# LCM firmware version: r<N>-<hash>
#   N    = number of commits that changed lcm/alcm (only goes up, so the app
#          can tell older from newer)
#   hash = git tree hash of lcm/alcm (changes only when the LCM code does)
# Needs the full git history for N (CI checks out with fetch-depth: 0); a
# shallow clone gives r0 (the app then only compares the hash).
cd "$(dirname "$0")/../.." || exit 1
hash=$(git rev-parse --short=7 HEAD:lcm/alcm 2>/dev/null || echo 0000000)
if [ "$(git rev-parse --is-shallow-repository 2>/dev/null)" = "true" ]; then
    n=0
else
    n=$(git rev-list --count HEAD -- lcm/alcm 2>/dev/null || echo 0)
fi
# lcm-powerclicks: + the SLCM feature mask (SLCM_FEATURES in config.h) as
# hex digits right after the 7-digit hash, only when not 0, e.g.
# r36-2fcd8f51. Still "r<N>-<hex>", so older packages read the same rev and
# compare builds as before; newer ones split hash (7) / features (the rest).
hash=$(printf '%s' "$hash" | cut -c1-7)
feat=$(sed -n 's/^#define SLCM_FEATURES 0x\([0-9a-fA-F]*\)U\{0,1\}.*/\1/p' \
    lcm/alcm/Library/HK32F030Mxx_Library_V1.1.6/HK32F030M_Project/inc/config.h | tr 'A-F' 'a-f')
case "$feat" in ""|0|00) feat="" ;; esac
echo "r$n-$hash$feat"
