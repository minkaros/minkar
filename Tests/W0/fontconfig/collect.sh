#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs on the build machine after fontconfig's 'make check' build: lays
# out in BUILD/w0 the tree its tests expect (test/ beside fc-cache/,
# fc-list/ ... and conf.d/), with the programs themselves in place of
# libtool's wrapper scripts, and tests.txt, the TESTS of test/Makefile.
# Usage: collect.sh BUILD SRC
build=$1 src=$2 out=$1/w0
rm -rf "$out" && mkdir -p "$out/test" || exit 1
cp -RL "$src/test/." "$out/test/" && cp -RL "$src/conf.d" "$out/" || exit 1
cp "$build/test/out.expected" "$out/test/" || exit 1
# run-test.sh's GNU forms of touch, date and stat (gnu.sh), after its set -e.
cp "$(dirname "$0")/gnu.sh" "$out/test/" &&
    sed -i '0,/^set -e$/s//set -e\n. "$(dirname "$0")"\/gnu.sh/' "$out/test/run-test.sh" || exit 1
real() { [ -f "$(dirname "$1")/.libs/$(basename "$1")" ] && echo "$(dirname "$1")/.libs/$(basename "$1")" || echo "$1"; }
# Not test-crbug1004254: run-test.sh runs it only after downloading fonts
# from the network, which the gate has not.
for p in "$build"/test/test-*; do
	[ -f "$p" ] && [ -x "$p" ] || continue
	case $p in */test-crbug1004254) continue;; esac
	cp -L "$(real "$p")" "$out/test/" || exit 1
done
for d in "$build"/fc-*/; do
	t=$(basename "$d")
	[ -x "$d/$t" ] || continue
	mkdir -p "$out/$t" && cp -L "$(real "$d/$t")" "$out/$t/" || exit 1
done
gmake -C "$build/test" -s --eval 'w0-tests: ; @echo $(TESTS)' w0-tests | tr ' ' '\n' | grep . > "$out/tests.txt"
echo "collect.sh: $(wc -l < "$out/tests.txt") tests"
