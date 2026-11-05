#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: mogenerator's tests (its test/Rakefile, without Xcode). From
# test.xcdatamodel it generates classes in Objective-C and in Swift: they
# must be the expected ones (the test programs were built on those), with
# no class for the entity marked mogenerator.ignore. Then the programs,
# which save objects in an in-memory store, run on the model mogenerator
# compiled.
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d /tmp/mogenerator-tests.XXXXXX) || exit 1
trap 'rm -rf "$work"' EXIT
export TMPDIR=$work MOMC_NO_INVERSE_RELATIONSHIP_WARNINGS=1
tests=0
failures=0

check() {
	tests=$((tests + 1))
	if [ "$2" = 0 ]; then
		echo "Test Case '$1' passed"
	else
		echo "$here/mogeneratorTests: error: $1 failed: $3"
		failures=$((failures + 1))
	fi
}

for lang in objc swift; do
	flag=
	[ $lang = swift ] && flag=--swift
	out=$(cd "$work" && "$here/mogenerator" $flag --model "$here/test.xcdatamodel" \
	    --output "$work/$lang" --baseClass MyBaseClass 2>&1)
	rc=$?
	check "$lang: mogenerator runs" $rc "$out"
	n=10
	[ $lang = swift ] && n=5
	echo "$out" | grep -q "^$n machine files and $n human files generated\.$"
	check "$lang: it generates $n machine and $n human files" $? "$(echo "$out" | tail -1)"
	diff=$(diff -r "$here/expected/$lang" "$work/$lang" 2>&1)
	check "$lang: they are the expected ones" $? "$(echo "$diff" | head -5)"
	! ls "$work/$lang" | grep -q Uncle
	check "$lang: no class for an ignored entity" $? "$(ls "$work/$lang" | grep Uncle)"
done

# The compiled model, which momcom writes to the temporary directory.
[ -f "$work/test.mom" ]
check "the model is compiled" $? "no $work/test.mom"
out=$(cd "$work" && "$here/testbin" 2>&1)
rc=$?
echo "$out" | grep -q '^success$'
check "objc: the program saves objects" $((rc + $?)) "$(echo "$out" | tail -3)"
out=$(cd "$work" && "$here/swifttest" 2>&1)
rc=$?
echo "$out" | grep -q '^Success$'
check "swift: the program saves objects" $((rc + $?)) "$(echo "$out" | tail -3)"

echo "Executed $tests tests, with $failures failures"
[ $failures = 0 ]
