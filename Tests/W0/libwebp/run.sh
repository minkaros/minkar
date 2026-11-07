#!/bin/sh
# libwebp's test vectors: test_dwebp.sh, again with threads (--mt), and
# test_lossless.sh, each with dwebp, in a scratch copy of the vectors
# (the scripts write their outputs next to them). Minkar has md5, not
# md5sum: md5c checks "sum  file" lines as md5sum -c does. One line per
# script: "PASS name" or "FAIL name (exit N)"; the last line the totals.
here=$(pwd)
work=$(mktemp -d /tmp/libwebp-w0.XXXXXX)
cp -R "$here/libwebp-test-data/." "$work/"
cat > "$work/md5c" <<'EOS'
#!/bin/sh
# md5sum -c -: each "sum  file" line of stdin against md5 -q file.
[ "$1" = -c ] || exit 2
rc=0
while read -r sum file; do
	if [ "$(md5 -q "$file")" = "$sum" ]; then echo "$file: OK"; else echo "$file: FAILED"; rc=1; fi
done
exit $rc
EOS
chmod +x "$work/md5c"
passed=0 failed=0
run() {
	name=$1; shift
	(cd "$work" && timeout ${WEBP_TEST_TIMEOUT:-1800} sh "$@") > "$here/$name.log" 2>&1 < /dev/null
	rc=$?
	if [ $rc -eq 0 ]; then
		passed=$((passed + 1)); echo "PASS $name"
	else
		failed=$((failed + 1)); echo "FAIL $name (exit $rc)"; tail -3 "$here/$name.log" | sed 's/^/    /'
	fi
}
run dwebp ./test_dwebp.sh --exec="$here/dwebp" --md5exec="$work/md5c" ./libwebp_tests.md5
run dwebp-mt ./test_dwebp.sh --exec="$here/dwebp" --md5exec="$work/md5c" --mt ./libwebp_tests.md5
run lossless ./test_lossless.sh --exec="$here/dwebp"
rm -rf "$work"
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
