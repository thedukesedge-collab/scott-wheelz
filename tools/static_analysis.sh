#!/bin/sh
# refloatRM: static analysis of the package C code (clang-tidy: the clang
# static analyzer + bug-prone patterns), host build of the sources.
#
# Run from the repo root after a normal build (src/conf has its generated
# files). Prints every finding; exits 1 if a finding is in a file this fork
# added or changed (upstream Refloat's own findings are reported, not failed).
#
# usage: tools/static_analysis.sh
set -e
LIB=$(pwd)/vesc_pkg_lib
CHECKS='-*,clang-analyzer-*,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-implicit-widening-of-multiplication-result,-bugprone-reserved-identifier,-bugprone-assignment-in-if-condition,-clang-analyzer-security.insecureAPI.*'
FLAGS="-std=gnu99 -DIS_VESC_LIB -Isrc -Isrc/conf -I$LIB -I$LIB/utils -Wno-unknown-attributes"

# files added or changed by refloatRM (vs Refloat v1.2.3)
OURS="main.c haptic_feedback.c lcm.c lcm_fw.c leds.c imu.c state.c remote.c battery_curve.c"

fail=0
for f in src/*.c; do
    name=$(basename "$f")
    out=$(clang-tidy --quiet -checks="$CHECKS" "$f" -- $FLAGS 2>/dev/null | grep -E "warning:|error:" || true)
    [ -z "$out" ] && continue
    echo "== $name"
    echo "$out"
    case " $OURS " in
        *" $name "*) fail=1 ;;
    esac
done

if [ "$fail" = 1 ]; then
    echo "FAIL: findings in files changed by refloatRM (see above)"
    exit 1
fi
echo "OK: no findings in files changed by refloatRM"
