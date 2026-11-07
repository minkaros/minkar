#!/bin/sh
# libgpg-error's own test programs (its Makefile.am's TESTS), run from their data
# directory with srcdir set as 'make check' sets it. (t-lock-single-posix
# is built only with HAVE_LOCK_OPTIMIZATION, which Darwin does not have.)
here=$(pwd)
data=$here/tests
[ -d "$data" ] || data=$here
export srcdir=$data abs_srcdir=$data top_srcdir=$data/.. abs_top_srcdir=$data/..
passed=0 failed=0
for t in t-version t-strerror t-syserror t-lock t-printf t-poll t-b64 t-argparse t-logging t-stringutils t-malloc t-spawn t-strlist t-name-value; do
	(cd "$data" && timeout ${W0_TEST_TIMEOUT:-600} "$here/.libs/$t") > "$here/$t.log" 2>&1
	rc=$?
	if [ $rc -eq 0 ] || [ $rc -eq 77 ]; then passed=$((passed + 1)); echo "PASS $t"; \
	else failed=$((failed + 1)); echo "FAIL $t (exit $rc)"; tail -3 "$here/$t.log" | sed 's/^/    /'; fi
done
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
