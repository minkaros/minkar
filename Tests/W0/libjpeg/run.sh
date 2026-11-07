#!/bin/sh
# libjpeg-turbo's own tests (its CTest list, w0/tests.txt), in order, from
# a scratch directory where they write their outputs and the -cmp tests
# check each output's MD5. One line per test: "PASS name" or "FAIL name
# (exit N)"; the last line the totals.
here=$(pwd)
w0=$here/w0
work=$(mktemp -d /tmp/libjpeg-w0.XXXXXX)
passed=0 failed=0
tab=$(printf '\t')
mkdir -p "$here/logs"
while IFS=$tab read -r name cmd; do
	cmd=$(printf '%s' "$cmd" | sed -e "s|@W@|$w0|g" -e "s|@S@|$w0/src|g")
	(cd "$work" && eval "timeout ${JPEG_TEST_TIMEOUT:-300} $cmd") > "$here/logs/$name.log" 2>&1 < /dev/null
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
