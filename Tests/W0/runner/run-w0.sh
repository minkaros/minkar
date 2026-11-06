#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W0: runs every library's tests in /usr/local/tests/w0 (each
# <library>/run.sh, from its own directory, which prints a last line
# "passed N" or "failed N of M") and says how each went; the last line says
# whether all passed. W0_TIMEOUT: seconds for one library (default 1800).
cd /usr/local/tests/w0 || exit 1
home=/Users/w0-tests
rm -rf "$home" && mkdir -p "$home"
export HOME=$home CFFIXED_USER_HOME=$home TMPDIR=/tmp
failed=0
for lib in */; do
	lib=${lib%/}
	[ -x "$lib/run.sh" ] || continue
	out=$(cd "$lib" && timeout "${W0_TIMEOUT:-1800}" ./run.sh 2>&1)
	rc=$?
	last=$(printf '%s\n' "$out" | tail -1)
	if [ $rc -eq 0 ]; then
		echo "w0: $lib passed: $last"
	else
		echo "w0: $lib FAILED (exit $rc): $last"
		printf '%s\n' "$out" | grep -E '^(FAIL|not ok)' | head -10 | sed 's/^/w0:   /'
		failed=1
	fi
done
[ $failed -eq 0 ] && echo "w0: every library passed" || echo "w0: some library FAILED"
exit $failed
