#!/bin/sh
# nghttp2's CTest list: main (the munit suite) and failmalloc. One line per
# test: "PASS name" or "FAIL name (exit N)"; the last line the totals.
here=$(pwd)
passed=0 failed=0
for t in main failmalloc; do
	(cd "$here" && timeout ${NGHTTP2_TEST_TIMEOUT:-900} ./$t) > "$here/$t.log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ]; then passed=$((passed + 1)); echo "PASS $t"
	else failed=$((failed + 1)); echo "FAIL $t (exit $rc)"; tail -5 "$here/$t.log" | sed 's/^/    /'; fi
done
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
