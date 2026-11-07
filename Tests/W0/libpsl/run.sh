#!/bin/sh
# libpsl's own tests (its meson test list, w0/tests.txt), each from
# its working directory; the Python ones are in w0/skipped.txt (Minkar has
# no Python), the others left out in skip.txt, with the reason. One line
# per test: "PASS name" or "FAIL name (exit N)"; the last line the totals.
here=$(pwd)
w0=$here/w0
export LC_ALL=C
passed=0 failed=0
skipped=$(grep -c . "$w0/skipped.txt")
tab=$(printf '\t')
while IFS=$tab read -r name wd cmd; do
	# skip.txt: tests left out, each with the reason.
	if [ -f "$here/skip.txt" ] && grep -q "^$name " "$here/skip.txt"; then
		skipped=$((skipped + 1)); continue
	fi
	sub() { printf '%s' "$1" | sed -e "s|@B@|$w0/build|g" -e "s|@S@|$w0/src|g" -e "s|@HERE@|$w0|g"; }
	wd=$(sub "$wd"); cmd=$(sub "$cmd")
	[ -d "$wd" ] || wd=$w0
	log=$here/$(printf '%s' "$name" | tr '/ ' '__').log
	(cd "$wd" && eval "timeout ${PSL_TEST_TIMEOUT:-600} $cmd") > "$log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ] || [ $rc -eq 77 ]; then
		passed=$((passed + 1)); echo "PASS $name"
	else
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"
	fi
done < "$w0/tests.txt"
if [ $failed -eq 0 ]; then
	echo "passed $passed (skipped $skipped, w0/skipped.txt)"
else
	echo "failed $failed of $((passed + failed)) (skipped $skipped)"
fi
exit $((failed > 0))
