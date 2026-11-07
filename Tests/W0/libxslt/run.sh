#!/bin/sh
# libxslt's runtest, from the source tree's tests directory, as its 'make
# check' runs it. One line: "PASS runtest" or "FAIL runtest (exit N)";
# the last line the totals.
here=$(pwd)
(cd "$here/tests" && timeout ${XSLT_TEST_TIMEOUT:-900} "$here/runtest") > "$here/runtest.log" 2>&1 < /dev/null
rc=$?
if [ $rc -eq 0 ]; then
	echo "PASS runtest"; tail -1 "$here/runtest.log"; echo "passed 1"
else
	echo "FAIL runtest (exit $rc)"; tail -5 "$here/runtest.log" | sed 's/^/    /'; echo "failed 1 of 1"
fi
exit $((rc != 0))
