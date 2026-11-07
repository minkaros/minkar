#!/bin/sh
# libpng's own tests (its CTest list, w0/tests.txt): each program with the
# options and PNG files test.cmake would give it, from a scratch directory
# (pngtest and pngstest write files there). One line per test: "PASS name"
# or "FAIL name (exit N)"; the last line the totals.
here=$(pwd)
w0=$here/w0
work=$(mktemp -d /tmp/libpng-w0.XXXXXX)
passed=0 failed=0
tab=$(printf '\t')
while IFS=$tab read -r name cmd; do
	cmd=$(printf '%s' "$cmd" | sed "s|@S@|$w0/src|g")
	(cd "$work" && eval "timeout ${PNG_TEST_TIMEOUT:-900} $w0/$cmd") > "$here/$name.log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ]; then
		passed=$((passed + 1)); echo "PASS $name"
	else
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"
	fi
done < "$w0/tests.txt"
rm -rf "$work"
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
