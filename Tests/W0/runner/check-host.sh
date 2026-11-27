#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W0 on a physical machine running Minkar ('bmake w0-check
# W0_HOST=root@address'): copies the tests the sysroot has
# (/usr/local/tests/w0 and the tools they use, w0-tools, from 'bmake
# w0-tests') there, runs them (run.sh,
# as a user without root's rights) and passes when every library of
# Tests/W0 passed. In VMs nested in another hypervisor, waking a thread
# costs about a millisecond and GLib's lock tests do not finish, so the
# gate runs on hardware, as release gates do.
# Usage: check-host.sh SYSROOT HOST [SSH_KEY] LIBRARY...
sysroot=$1 host=$2 key=$3; shift 3
ssh="ssh -o BatchMode=yes -o ConnectTimeout=20 ${key:+-i $key}"
log=$(mktemp)
# The archive first, whole, and only from a sysroot nothing is writing:
# on 24 Nov 2026 the gate ran while 'w0-tests' was still installing the
# tests, the rebuild gave new files the inodes of the ones it had just
# deleted, and GNU tar (given two directories, it takes any inode it has
# already archived for a hard link, even with one link) stored six new
# files as links to old ones: on PC 1 six pairs shared their data.
# --hard-dereference keeps tar from making links, and a file changed
# while it was read (tar's status 1) fails the gate.
archive=$(mktemp)
echo "w0-check: archiving the tests"
if ! tar -C "$sysroot/usr/local/tests" --hard-dereference -cf "$archive" w0 w0-tools; then
	echo "w0-check: the sysroot changed while it was archived (is 'w0-tests' still running?)"
	rm -f "$archive"
	exit 1
fi
echo "w0-check: copying the tests to $host"
$ssh "$host" 'rm -rf /usr/local/tests/w0 /usr/local/tests/w0-tools && mkdir -p /usr/local/tests &&
    tar -C /usr/local/tests -xf -' < "$archive" || { rm -f "$archive"; exit 1; }
rm -f "$archive"
# Every file's SHA-1 on the machine, after the copy and after the run,
# against the sysroot's.
sums() { $ssh "$host" 'cd /usr/local/tests && find w0 w0-tools -type f -exec shasum {} +' | awk '{ print $2, $1 }' | LC_ALL=C sort; }
want=$(mktemp); before=$(mktemp); after=$(mktemp)
(cd "$sysroot/usr/local/tests" && find w0 w0-tools -type f -exec shasum {} +) | awk '{ print $2, $1 }' | LC_ALL=C sort > "$want"
sums > "$before"
echo "w0-check: running them on $host"
$ssh "$host" '/usr/local/tests/w0/run.sh' 2>&1 | tee "$log" | grep '^w0: '
sums > "$after"
failed=0
copied=$(LC_ALL=C join "$want" "$before" | awk '$2 != $3' | wc -l)
changed=$(LC_ALL=C join "$before" "$after" | awk '$2 != $3' | wc -l)
other=$(LC_ALL=C join -v 1 -v 2 "$want" "$before" | wc -l)
if [ "$copied" -ne 0 ] || [ "$changed" -ne 0 ] || [ "$other" -ne 0 ]; then
	echo "w0-check: files not as copied: $copied after the copy, $changed changed by the run, $other only on one side"
	LC_ALL=C join -v 1 -v 2 "$want" "$before" | awk '{ print "w0-check:   only on one side: " $1 }' | head -20
	LC_ALL=C join "$want" "$before" | awk '$2 != $3 { print "w0-check:   copied wrong: " $1 }' | head -20
	LC_ALL=C join "$before" "$after" | awk '$2 != $3 { print "w0-check:   changed: " $1 }' | head -20
	failed=1
fi
rm -f "$want" "$before" "$after"
for lib in "$@"; do
	grep -q "^w0: $lib passed" "$log" || { echo "w0-check: $lib did not pass"; failed=1; }
done
grep -q '^w0: every library passed$' "$log" || failed=1
rm -f "$log"
[ $failed -eq 0 ] && echo "w0-check: PASS" || echo "w0-check: FAIL"
exit $failed
