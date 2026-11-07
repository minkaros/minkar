#!/bin/sh
# fontconfig's own tests (the TESTS of its test/Makefile, w0/tests.txt),
# from w0/test as 'make check' runs them: the .sh ones with sh, the
# others directly. One line per test: "PASS name" or "FAIL name (exit N)"
# (77 is a skip, as in Automake); the last line the totals.
here=$(pwd)
t=$here/w0/test
export srcdir=$t builddir=$t src=$t EXEEXT= LOG_COMPILER= TMPDIR=${TMPDIR:-/tmp}
passed=0 failed=0 skipped=0
for name in $(cat "$here/w0/tests.txt"); do
	case $name in *.sh) cmd="sh ./$name";; *) cmd=./$name;; esac
	(cd "$t" && timeout ${FC_TEST_TIMEOUT:-600} $cmd) > "$here/$name.log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ]; then passed=$((passed + 1)); echo "PASS $name"
	elif [ $rc -eq 77 ]; then skipped=$((skipped + 1)); echo "SKIP $name"
	else failed=$((failed + 1)); echo "FAIL $name (exit $rc)"; tail -3 "$here/$name.log" | sed 's/^/    /'; fi
done
[ $failed -eq 0 ] && echo "passed $passed (skipped $skipped)" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
