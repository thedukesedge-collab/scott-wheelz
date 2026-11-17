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
echo "r$n-$hash"
