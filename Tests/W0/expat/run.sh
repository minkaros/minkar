#!/bin/sh
# Expat's test programs, each a suite of checks.
failed=0
for t in runtests runtests_cxx; do
	out=$(./$t 2>&1); rc=$?
	printf '%s\n' "$out" | tail -2
	[ $rc -eq 0 ] || { failed=$((failed + 1)); echo "FAIL $t"; }
done
[ $failed -eq 0 ] && echo "passed 2 (runtests, runtests_cxx)" || echo "failed $failed of 2"
exit $failed
