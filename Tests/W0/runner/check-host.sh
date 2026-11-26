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
echo "w0-check: copying the tests to $host"
tar -C "$sysroot/usr/local/tests" -cf - w0 w0-tools | $ssh "$host" \
    'rm -rf /usr/local/tests/w0 /usr/local/tests/w0-tools && mkdir -p /usr/local/tests &&
    tar -C /usr/local/tests -xf -' || exit 1
# Every file's SHA-1 on the machine, after the copy and after the run:
# once (24 Nov 2026) six pairs of test files came to share their data on
# PC 1; not seen again, so watched for here.
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
if [ "$copied" -ne 0 ] || [ "$changed" -ne 0 ]; then
	echo "w0-check: files not as copied: $copied after the copy, $changed changed by the run"
	LC_ALL=C join "$before" "$after" | awk '$2 != $3 { print "w0-check:   " $1 }' | head -20
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
