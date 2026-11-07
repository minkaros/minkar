#!/bin/sh
# libxml2's tests, as its CMake test list runs them (runtest, runsuite,
# testapi, testchar, testdict, testparser, testrecurse, testThreads): the
# data-driven ones from the directory that holds test/ and result/. One
# line per test: "PASS name" or "FAIL name (exit N)"; the last line the
# totals.
here=$(pwd)
out=$(mktemp -d /tmp/libxml2-w0.XXXXXX)
passed=0 failed=0
for t in "runtest --out $out" runsuite testapi testchar testdict testparser testrecurse testThreads; do
	name=${t%% *}
	(cd "$here" && timeout ${XML_TEST_TIMEOUT:-900} ./$t) > "$here/$name.log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ]; then
		passed=$((passed + 1)); echo "PASS $name"
	else
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"; tail -3 "$here/$name.log" | sed 's/^/    /'
	fi
done
rm -rf "$out"
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
