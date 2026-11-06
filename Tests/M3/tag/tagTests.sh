#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate M3: tag's commands, on files made here (tag has no tests of its
# own). Tags go through NSURL's tag names, kept in the files' extended
# attributes; --find and --usage go through NSMetadataQuery. Each check
# compares what tag prints with what it prints on the reference system.
here=$(cd "$(dirname "$0")" && pwd)
tag=$here/tag
work=$(cd "$(mktemp -d /tmp/tag-tests.XXXXXX)" && pwd -P) || exit 1
trap 'rm -rf "$work" "$HOME/Documents/tag-tests-home"' EXIT
tests=0
failures=0

# check NAME EXPECTED ACTUAL
check() {
	tests=$((tests + 1))
	if [ "$2" = "$3" ]; then
		echo "Test Case '$1' passed"
	else
		echo "$here/tagTests: error: $1 failed: expected [$2], got [$3]"
		failures=$((failures + 1))
	fi
}

pad() {
	printf '%-31s\t%s' "$1" "$2"
}

cd "$work" || exit 1
mkdir sub
touch a b c sub/d .hidden

"$tag" --add red,blue a
check "add" "$(pad a blue,red)" "$("$tag" --list a)"
check "the tags are in the extended attribute" "yes" \
    "$(xattr -p com.apple.metadata:_kMDItemUserTags a >/dev/null 2>&1 && echo yes)"
check "list without tags" "a" "$("$tag" -l -T a)"
check "list without names" "blue,red" "$("$tag" -l -N a)"
check "list a tag on each line" "$(printf 'a\n    blue\n    red')" "$("$tag" -l -g a)"
"$tag" -a RED a
check "adding a tag in other case keeps the one there" "blue,red" "$("$tag" -l -N a)"
"$tag" --remove red a
check "remove" "blue" "$("$tag" -l -N a)"
"$tag" -r green a
check "remove a tag not there" "blue" "$("$tag" -l -N a)"
"$tag" --set yellow,green a
check "set" "green,yellow" "$("$tag" -l -N a)"
"$tag" -a red b
"$tag" -a Red sub/d
"$tag" -a red .hidden

check "match" "b" "$("$tag" --match red a b c sub)"
check "match, case aside" "b" "$("$tag" -m RED a b c sub)"
check "match every tag given" "a" "$("$tag" -m green,yellow a b c sub)"
check "match no file" "" "$("$tag" -m green,red a b c sub)"
check "match any tag" "$(printf 'a\nb')" "$("$tag" -m '*' a b c sub)"
check "match no tag" "$(printf 'c\nsub')" "$("$tag" -m '' a b c sub)"
check "match, with tags" "$(pad b red)" "$("$tag" -m red -t a b c sub)"
check "enter a directory" "$(printf 'sub\n%s' "$(pad d Red)")" "$("$tag" -l -e sub)"
check "recurse" "$(printf 'sub\n%s' "$(pad d Red)")" "$("$tag" -l -R sub)"
check "hidden files left out" "$(printf 'a\nb')" "$("$tag" -m '*' -e .)"
check "hidden files with --all" "$(printf '.hidden\na\nb')" "$("$tag" -m '*' -A -e .)"
check "directories with a slash" "sub/" "$("$tag" -l -p sub)"
check "NUL terminated" "62 00" "$("$tag" -m red -0 b | od -An -tx1 | tr -s ' ' | sed 's/^ //;s/ $//')"

check "find" "$(printf '%s\n%s' "$work/b" "$work/sub/d")" "$("$tag" --find red "$work")"
check "find, with tags" "$(printf '%s\n%s' "$(pad "$work/b" red)" "$(pad "$work/sub/d" Red)")" \
    "$("$tag" -f red -t "$work")"
check "find any tag" "$(printf '%s\n%s\n%s' "$work/a" "$work/b" "$work/sub/d")" "$("$tag" -f '*' "$work")"
check "find every tag given" "$work/a" "$("$tag" -f green,yellow "$work")"
mkdir -p "$HOME/Documents/tag-tests-home"
touch "$HOME/Documents/tag-tests-home/note"
"$tag" -a tag-tests-home "$HOME/Documents/tag-tests-home/note"
check "find in the home directory" "$HOME/Documents/tag-tests-home/note" "$("$tag" -f tag-tests-home --home)"
check "usage" "$(printf '1\tRed\n1\tgreen\n1\tred\n1\tyellow')" "$("$tag" --usage '*' "$work" | sort -k2)"

check "version" "tag v0.10.0" "$("$tag" --version)"
"$tag" -a red "$work/missing" 2>/dev/null
check "a missing file is an error" "2" "$?"

echo "Executed $tests tests, with $failures failures"
[ $failures = 0 ]
