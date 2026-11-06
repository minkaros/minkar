#!/bin/sh
# libtasn1's own test programs (its Makefile.am's TESTS), run from their data
# directory with srcdir set as 'make check' sets it.
here=$(pwd)
data=$here/tests
[ -d "$data" ] || data=$here
export srcdir=$data abs_srcdir=$data top_srcdir=$data/.. abs_top_srcdir=$data/..
# The data files, as the tests' TESTS_ENVIRONMENT names them.
export ASN1PARSER=$data/Test_parser.asn ASN1TREE=$data/Test_tree.asn ASN1CHOICE=$data/choice.asn \
    ASN1CODINGDECODING2=$data/coding-decoding2.asn ASN1PKIX=$data/pkix.asn ASN1SETOF=$data/setof.asn \
    ASN1CRLDER=$data/crl.der ASN1INDEF=$data/TestIndef.p12 ASN1INDEF2=$data/TestIndef2.p12 \
    ASN1INDEF3=$data/TestIndef3.der ASN1ENCODING=$data/Test_encoding.asn \
    ASN1CHOICE_OCSP=$data/pkix.asn ASN1CHOICE_OCSP_DATA=$data/ocsp.der \
    ASN1_RESPONSE_OCSP_DATA=$data/ocsp-basic-response.der ASN1_MSCAT=$data/mscat.asn \
    ASN1_SPC_PE_IMAGE_DATA=$data/spc_pe_image_data.der
passed=0 failed=0
for t in Test_parser Test_tree Test_encoding Test_indefinite Test_errors Test_simple Test_overflow Test_strings Test_choice Test_encdec copynode coding-decoding2 strict-der Test_choice_ocsp ocsp-basic-response octet-string coding-long-oid object-id-decoding spc_pe_image_data setof CVE-2018-1000654 reproducers object-id-encoding version; do
	(cd "$data" && timeout ${W0_TEST_TIMEOUT:-600} "$here/.libs/$t") > "$here/$t.log" 2>&1
	rc=$?
	if [ $rc -eq 0 ] || [ $rc -eq 77 ]; then passed=$((passed + 1)); echo "PASS $t"; \
	else failed=$((failed + 1)); echo "FAIL $t (exit $rc)"; tail -3 "$here/$t.log" | sed 's/^/    /'; fi
done
[ $failed -eq 0 ] && echo "passed $passed" || echo "failed $failed of $((passed + failed))"
exit $((failed > 0))
