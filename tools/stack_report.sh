#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Part of Refloat Glow, 2026. See FORK_NOTES.md.
# Refloat Glow (rf75): stack-usage report for the package.
#
# A stack overflow on one of the controller's threads is a silent crash, and
# a crash anywhere in the package stops balancing. This compiles every source
# file without LTO and with -fstack-usage (same target flags as rules.mk),
# prints the largest frames, and compares against upstream Refloat v1.2.3 if
# its sources are given. Fails if any single frame is over MAX_FRAME bytes.
#
# usage: tools/stack_report.sh [upstream_src_dir]   (run from the repo root,
#        after a normal build, so src/conf has its generated files)
set -e
MAX_FRAME=${MAX_FRAME:-512}
LIB=$(pwd)/vesc_pkg_lib
FLAGS="-fpic -Os -std=gnu99 -fomit-frame-pointer -mthumb -fsingle-precision-constant \
 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -mcpu=cortex-m4 -DIS_VESC_LIB -fstack-usage \
 -I$LIB -I$LIB/stdperiph_stm32f4/CMSIS/include -I$LIB/stdperiph_stm32f4/CMSIS/ST \
 -I$LIB/stdperiph_stm32f4/inc -I$LIB/utils"

report() {  # $1 = source dir, $2 = output dir
    rm -rf "$2"; mkdir -p "$2"
    for f in "$1"/*.c "$1"/conf/confparser.c "$1"/conf/buffer.c; do
        [ -f "$f" ] || continue
        arm-none-eabi-gcc $FLAGS -I"$1" -I"$1/conf" -c "$f" -o "$2/$(basename "$f" .c).o" \
            -fstack-usage 2>"$2/err.txt" || { echo "  (could not compile $f)" >&2; head -5 "$2/err.txt" >&2; }
        mv "$2/$(basename "$f" .c).su" "$2/" 2>/dev/null || true
    done
    # file:line:col:function  bytes  type  ->  "bytes function type"
    cat "$2"/*.su 2>/dev/null | awk -F'\t' '{n=split($1,a,":"); print $2, a[n], $3}' | sort -rn
}

echo "== Refloat Glow: largest stack frames (bytes, function, type) =="
report src /tmp/su_ours > /tmp/su_ours.txt
head -25 /tmp/su_ours.txt

if [ -n "$1" ] && [ -d "$1" ]; then
    # upstream's settings.xml is identical, so the generated conf files fit
    cp src/conf/conf_general.h src/conf/conf_default.h src/conf/confparser.h \
       src/conf/confparser.c src/conf/confxml.h src/conf/confxml.c "$1/conf/" 2>/dev/null || true
    report "$1" /tmp/su_up > /tmp/su_up.txt
    echo
    echo "== Frames that are new or bigger than upstream v1.2.3 (>= 64 bytes) =="
    awk 'NR==FNR {up[$2]=$1; next} $1 >= 64 && (!($2 in up) || $1 > up[$2]) \
        {printf "%5d  %-40s (upstream: %s)\n", $1, $2, ($2 in up ? up[$2] : "new")}' \
        /tmp/su_up.txt /tmp/su_ours.txt
fi

if grep -q "dynamic" /tmp/su_ours.txt; then
    echo; echo "== Dynamic (variable-size) frames =="; grep "dynamic" /tmp/su_ours.txt
fi

# fail on a frame over MAX_FRAME that is ours (new, or bigger than upstream's)
echo
if [ -f /tmp/su_up.txt ]; then
    bad=$(awk -v m="$MAX_FRAME" 'NR==FNR {up[$2]=$1; next} $1 > m && (!($2 in up) || $1 > up[$2]) \
        {print $1, $2}' /tmp/su_up.txt /tmp/su_ours.txt)
else
    bad=$(awk -v m="$MAX_FRAME" '$1 > m {print $1, $2}' /tmp/su_ours.txt)
fi
if [ -n "$bad" ]; then
    echo "FAIL: frames over $MAX_FRAME bytes that upstream doesn't have:"; echo "$bad"
    exit 1
fi
echo "OK: no frame over $MAX_FRAME bytes beyond upstream's own"
