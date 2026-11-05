#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: runs the test programs of every tool in /usr/local/tests/m3
# (each executable ending in Tests) from its own directory, and says how
# each went; the last line says whether all passed.
# A home of their own, as a user's: CFFIXED_USER_HOME and HOME, with the
# folders a user's home has (root's is under a symbolic link, /var).
home=/Users/m3-tests
rm -rf "$home"
mkdir -p "$home/Documents" "$home/Desktop" "$home/Library"
export CFFIXED_USER_HOME=$home HOME=$home
cd /usr/local/tests/m3 || exit 1
failed=0
for pkg in */; do
	pkg=${pkg%/}
	for t in "$pkg"/*Tests; do
		[ -f "$t" ] && [ -x "$t" ] || continue
		out=$(cd "$pkg" && { [ -f run.env ] && set -a && . ./run.env && set +a; true; } &&
		    timeout 600 "./${t##*/}" 2>&1)
		rc=$?
		summary=$(printf '%s\n' "$out" | grep -E "Executed [0-9]+ tests?" | tail -1 | sed 's/^[[:space:]]*//')
		if [ $rc -eq 0 ]; then
			echo "m3: $t passed: $summary"
		else
			echo "m3: $t FAILED (exit $rc): $summary"
			printf '%s\n' "$out" | grep -E ' error: ' | head -5 | sed 's/^/m3:   /'
			failed=$((failed + 1))
		fi
	done
done
rm -rf "$home"
if [ $failed -eq 0 ]; then
	echo "m3: every suite passed"
else
	echo "m3: $failed suites failed"
	exit 1
fi
