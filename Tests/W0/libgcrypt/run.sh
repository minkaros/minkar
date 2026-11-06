#!/bin/sh
# libgcrypt's own test programs (its Makefile.am's TESTS), run from their data
# directory with srcdir set as 'make check' sets it.
# Left out: benchmark and bench-slope (timings, not checks), and
# hashtest-6g and hashtest-256g (they hash 6 and 256 GB).
here=$(pwd)
data=$here/tests
[ -d "$data" ] || data=$here
export srcdir=$data abs_srcdir=$data top_srcdir=$data/.. abs_top_srcdir=$data/..
passed=0 failed=0
for t in version t-secmem mpitests t-sexp t-convert t-mpi-bit t-mpi-point t-lock prime basic keygen pubkey hmac hashtest t-kdf keygrip aeswrap random t-kem t-mlkem t-thread-local t-fips-service-ind pkcs1v2 t-rsa-pss t-rsa-15 t-rsa-testparm fips186-dsa dsa-rfc6979 t-dsa curves t-ecdsa t-ed25519 t-cv25519 t-x448 t-ed448; do
	(cd "$data" && timeout ${W0_TEST_TIMEOUT:-600} "$here/.libs/$t") > "$here/$t.log" 2>&1
	rc=$?
	if [ $rc -eq 0 ] || [ $rc -eq 77 ]; then passed=$((passed + 1)); echo "PASS $t"; \
	else failed=$((failed + 1)); echo "FAIL $t (exit $rc)"; tail -3 "$here/$t.log" | sed 's/^/    /'; fi
done
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
