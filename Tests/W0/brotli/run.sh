#!/bin/sh
# Brotli's CTest list (CMakeLists.txt, tests/run-*-test.cmake) in sh:
# roundtrip/INPUT/QUALITY compresses each input at qualities 1, 6, 9 and
# 11 and decompresses it, from a file and as a stream, and must give the
# input back; compatibility/FILE decompresses each test file brotli made
# and compares it with the original. The inputs the release has (its
# sources; the text corpus is not in it). One line per test: "PASS name"
# or "FAIL name"; the last line the totals.
here=$(pwd)
brotli=$here/brotli
tmp=$(mktemp -d /tmp/brotli-w0.XXXXXX)
passed=0 failed=0
result() {
	if [ "$2" -eq 0 ]; then passed=$((passed + 1)); echo "PASS $1"
	else failed=$((failed + 1)); echo "FAIL $1"; fi
}
for input in tests/testdata/alice29.txt tests/testdata/asyoulik.txt tests/testdata/lcet10.txt \
    tests/testdata/plrabn12.txt c/enc/encode.c c/common/dictionary.h c/dec/decode.c; do
	f=$here/$input
	[ -f "$f" ] || continue
	for q in 1 6 9 11; do
		out=$tmp/${input##*/}.$q
		rc=0
		"$brotli" -fq $q "$f" -o "$out.br" && "$brotli" "$out.br" -fdo "$out.unbr" &&
		    cmp -s "$f" "$out.unbr" || rc=1
		[ $rc -eq 0 ] && { "$brotli" -cq $q < "$f" | "$brotli" -cd > "$out.stream" &&
		    cmp -s "$f" "$out.stream" || rc=1; }
		result "roundtrip/$input/$q" $rc
	done
done
for f in "$here"/tests/testdata/*.compressed*; do
	expected=${f%.compressed*}
	out=$tmp/${expected##*/}.uncompressed
	rc=0
	"$brotli" "$f" -fdo "$out" && cmp -s "$out" "$expected" || rc=1
	[ $rc -eq 0 ] && { "$brotli" -dc < "$f" > "$out" && cmp -s "$out" "$expected" || rc=1; }
	result "compatibility/tests/testdata/${f##*/}" $rc
done
rm -rf "$tmp"
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
