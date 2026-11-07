#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W0: runs every library's tests in /usr/local/tests/w0 (each
# <library>/run.sh, from its own directory, which prints a last line
# "passed N" or "failed N of M") and says how each went; the last line says
# whether all passed. W0_TIMEOUT: seconds for one library (default 1800).
# The tests run as a user without root's rights (nobody), from a copy in
# its home, where they write their logs: some check that a file they made
# unreadable stays so, which root's rights would hide.
home=/Users/w0-tests
rm -rf "$home" && mkdir -p "$home" || exit 1
cp -R /usr/local/tests/w0 "$home/w0" || exit 1
if [ "$(id -u)" -eq 0 ]; then
	chown -R nobody:nobody "$home"
	as_user() { su -m nobody -c "$1"; }
else
	as_user() { sh -c "$1"; }
fi
export HOME=$home CFFIXED_USER_HOME=$home TMPDIR=$home/tmp
as_user "mkdir -p $TMPDIR" || exit 1
cd "$home/w0" || exit 1
failed=0
for lib in */; do
	lib=${lib%/}
	[ -x "$lib/run.sh" ] || continue
	out=$(as_user "cd $lib && timeout ${W0_TIMEOUT:-1800} ./run.sh" 2>&1)
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
