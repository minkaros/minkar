#!/bin/sh
# GLib's installed tests, as gnome-desktop-testing runs them: each .test
# file's Exec, from the tests' directory, with G_TEST_SRCDIR and
# G_TEST_BUILDDIR set there. skip.txt names the tests left out, each with
# the reason (what they need that Minkar has not). One line per test:
# "PASS name" or "FAIL name (exit N)"; the last line the totals.
here=$(pwd)
dir=$here/glib
export G_TEST_SRCDIR=$dir G_TEST_BUILDDIR=$dir G_DEBUG=
passed=0 failed=0 skipped=0
for t in "$dir"/*.test; do
	name=${t##*/}; name=${name%.test}
	if [ -f "$here/skip.txt" ] && grep -q "^$name " "$here/skip.txt"; then
		skipped=$((skipped + 1)); continue
	fi
	cmd=$(sed -n 's/^Exec=//p' "$t" | sed "s|/usr/libexec/installed-tests/glib/|$dir/|g")
	(cd "$dir" && eval "timeout ${GLIB_TEST_TIMEOUT:-120} $cmd") > "$here/$name.log" 2>&1
	rc=$?
	if [ $rc -eq 0 ]; then
		passed=$((passed + 1)); echo "PASS $name"
	else
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"
	fi
done
if [ $failed -eq 0 ]; then
	echo "passed $passed (skipped $skipped, skip.txt)"
else
	echo "failed $failed of $((passed + failed)) (skipped $skipped)"
fi
exit $((failed > 0))
