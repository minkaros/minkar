#!/bin/sh
# libffi's own tests (w0/tests.txt): each program must exit 0 and print
# what its dg-output patterns say (dgmatch). One line per test: "PASS
# name" or "FAIL name (exit N)" / "(output)"; the last line the totals.
here=$(pwd)
w0=$here/w0
passed=0 failed=0
tab=$(printf '\t')
mkdir -p "$here/logs"
while IFS=$tab read -r name prog expect; do
	log=$here/logs/$(echo "$name" | tr / .).log
	timeout ${FFI_TEST_TIMEOUT:-300} "$w0/$prog" > "$log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -ne 0 ]; then
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"
	elif ! "$w0/dgmatch" "$w0/$expect" "$log"; then
		failed=$((failed + 1)); echo "FAIL $name (output)"
	else
		passed=$((passed + 1)); echo "PASS $name"
	fi
done < "$w0/tests.txt"
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
