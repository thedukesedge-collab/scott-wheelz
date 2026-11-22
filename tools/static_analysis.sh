#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Part of Refloat Glow, 2026. See FORK_NOTES.md.
# refloatRM: static analysis of the package C code (clang-tidy: the clang
# static analyzer + bug-prone patterns), host build of the sources.
#
# Run from the repo root after a normal build (src/conf has its generated
# files). Prints every finding. A finding on a line that is word-for-word in
# upstream Refloat (given as $1) is upstream's and only reported; any other
# finding fails the step.
#
# usage: tools/static_analysis.sh [upstream_src_dir]
set -e
UP=$1
LIB=$(pwd)/vesc_pkg_lib
CHECKS='-*,clang-analyzer-*,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-narrowing-conversions,-bugprone-implicit-widening-of-multiplication-result,-bugprone-reserved-identifier,-bugprone-assignment-in-if-condition,-clang-analyzer-security.insecureAPI.*'
FLAGS="-std=gnu99 -DIS_VESC_LIB -Isrc -Isrc/conf -I$LIB -I$LIB/utils -I$LIB/stdperiph_stm32f4/CMSIS/include -I$LIB/stdperiph_stm32f4/CMSIS/ST -I$LIB/stdperiph_stm32f4/inc -Wno-unknown-attributes"

for f in src/*.c; do
    name=$(basename "$f")
    out=$(clang-tidy --quiet -checks="$CHECKS" "$f" -- $FLAGS 2>/dev/null | grep -E "^[^ ].*(warning|error):" || true)
    [ -z "$out" ] && continue
    echo "== $name"
    echo "$out" | while IFS= read -r line; do
        file=$(echo "$line" | cut -d: -f1)
        n=$(echo "$line" | cut -d: -f2)
        text=$(sed -n "${n}p" "$file" 2>/dev/null)
        tag="NEW"
        if [ -n "$UP" ] && [ -f "$UP/$(basename "$file")" ] && grep -Fxq -- "$text" "$UP/$(basename "$file")"; then
            tag="upstream"
        fi
        echo "[$tag] $(echo "$line" | sed "s|$(pwd)/||")"
    done
done > /tmp/sa.txt
cat /tmp/sa.txt
if grep -q "^\[NEW\]" /tmp/sa.txt; then
    echo "FAIL: findings on lines added or changed by refloatRM (marked NEW)"
    exit 1
fi
echo "OK: no findings on lines added or changed by refloatRM"
