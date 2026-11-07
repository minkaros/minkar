#!/bin/sh
# libepoxy's own tests (its meson test list, w0/tests.txt), each from its
# working directory; skip.txt leaves some out, with the reason, and exit
# 77 is meson's skip (khronos_typedefs without <KHR/khrplatform.h>, which
# the reference system does not have either). One line per test: "PASS
# name", "SKIP name" or "FAIL name (exit N)"; the last line the totals.
here=$(pwd)
w0=$here/w0
passed=0 failed=0 skipped=0
tab=$(printf '\t')
while IFS=$tab read -r name wd cmd; do
	if grep -q "^$name " "$here/skip.txt" 2>/dev/null; then
		skipped=$((skipped + 1)); echo "SKIP $name"; continue
	fi
	sub() { printf '%s' "$1" | sed -e "s|@B@|$w0/build|g" -e "s|@S@|$w0/src|g" -e "s|@HERE@|$w0|g"; }
	wd=$(sub "$wd"); cmd=$(sub "$cmd")
	[ -d "$wd" ] || wd=$w0
	log=$here/$(printf '%s' "$name" | tr '/ ' '__').log
	(cd "$wd" && eval "timeout ${EPOXY_TEST_TIMEOUT:-300} $cmd") > "$log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ]; then
		passed=$((passed + 1)); echo "PASS $name"
	elif [ $rc -eq 77 ]; then
		skipped=$((skipped + 1)); echo "SKIP $name"
	else
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"
	fi
done < "$w0/tests.txt"
if [ $failed -eq 0 ]; then
	echo "passed $passed (skipped $skipped)"
else
	echo "failed $failed of $((passed + failed))"
fi
exit $((failed > 0))
