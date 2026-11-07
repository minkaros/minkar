#!/bin/sh
# PCRE2's RunTest and RunGrepTest from this directory: they run
# ./pcre2test and ./pcre2grep over testdata/ and compare the output with
# the expected one.
failed=0
for t in RunTest RunGrepTest; do
	srcdir=. ./$t > $t.log 2>&1 || { failed=$((failed + 1)); echo "FAIL $t"; tail -20 $t.log; }
done
[ $failed -eq 0 ] && echo "passed 2 (RunTest, RunGrepTest)" || echo "failed $failed of 2"
exit $failed
