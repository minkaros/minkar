#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# The toolchain's manifest (tools/toolchain/MANIFEST): what
# each component is built from, pinned by the git tree of its sources,
# and the version line its built tool must print.
#
#   manifest.sh update              rewrite the trees after a change
#   manifest.sh check [toolchain]   verify the trees; with a built
#                                   toolchain, the tools' versions and
#                                   that its output is deterministic
#
# Exits 1 on a mismatch, printing each one.

set -u
cd "$(dirname "$0")/../.." || exit 2
TOP=Developer/Default.xctoolchain
MANIFEST=${MANIFEST:-tools/toolchain/MANIFEST}
GIT="git -c safe.directory=*"

tree_of() {
	$GIT rev-parse "HEAD:$TOP/$1" 2>/dev/null
}

update() {
	tmp=$(mktemp) || exit 2
	while IFS= read -r line; do
		case "$line" in
		'#'*|'') printf '%s\n' "$line" ;;
		*)
			dir=$(printf '%s' "$line" | awk -F' [|] ' '{print $2}')
			old=$(printf '%s' "$line" | awk -F' [|] ' '{print $4}')
			new=$(tree_of "$dir") || { echo "$dir: not in git" >&2; rm -f "$tmp"; exit 2; }
			printf '%s\n' "$line" | sed "s/ | $old | / | $new | /"
			;;
		esac
	done < "$MANIFEST" > "$tmp"
	mv "$tmp" "$MANIFEST"
	echo "updated $MANIFEST"
}

fails=0
bad() {
	echo "MISMATCH: $*"
	fails=$((fails + 1))
}

check_trees() {
	while IFS= read -r line; do
		case "$line" in '#'*|'') continue ;; esac
		name=$(printf '%s' "$line" | awk -F' [|] ' '{print $1}')
		dir=$(printf '%s' "$line" | awk -F' [|] ' '{print $2}')
		want=$(printf '%s' "$line" | awk -F' [|] ' '{print $4}')
		have=$(tree_of "$dir")
		if [ "$have" != "$want" ]; then
			bad "$name: sources $TOP/$dir are tree $have, the manifest pins $want (run manifest.sh update)"
		elif ! $GIT diff --quiet HEAD -- "$TOP/$dir" 2>/dev/null; then
			echo "note: $name: $TOP/$dir has uncommitted changes"
		fi
	done < "$MANIFEST"
}

check_tools() {
	bin=$1/usr/bin
	while IFS= read -r line; do
		case "$line" in '#'*|'') continue ;; esac
		name=$(printf '%s' "$line" | awk -F' [|] ' '{print $1}')
		tool=$(printf '%s' "$line" | awk -F' [|] ' '{print $5}')
		[ "$tool" = "-" ] && continue
		cmd=${tool%%: *}
		want=${tool#*: }
		# The tool's own words, whatever its exit status.
		have=$(cd /tmp && $bin/$cmd 2>&1 | tr -s ' ' | grep -F -- "$want" | head -1)
		[ -n "$have" ] || bad "$name: '$cmd' does not print '$want'"
	done < "$MANIFEST"
}

# The same input, compiled, linked and archived twice, gives the same bytes.
check_determinism() {
	bin=$1/usr/bin
	sdk=$(dirname "$(dirname "$1")")/SDKs/Minkar.sdk
	work=$(mktemp -d) || exit 2
	cat > "$work/hello.c" <<-'EOF'
	#include <stdio.h>
	static const char when[] = __DATE__ " " __TIME__;
	int main(void) { printf("%s\n", when); return 0; }
	EOF
	# Both runs in the same directory: the debug information keeps its
	# path and the objects'; ZERO_AR_DATE keeps out their times.
	for run in 1 2; do
		mkdir -p "$work/run"
		(cd "$work/run" && export SOURCE_DATE_EPOCH=0 ZERO_AR_DATE=1 &&
		    cp ../hello.c . &&
		    $bin/clang --sysroot="$sdk" -mmacos-version-min=10.15 -g -O2 \
		    -c hello.c -o hello.o &&
		    $bin/clang --sysroot="$sdk" -mmacos-version-min=10.15 \
		    -fuse-ld="$bin/ld64" -o hello hello.o &&
		    $bin/libtool -static -o libhello.a hello.o &&
		    mv "$work/run" "$work/out$run") > "$work/build.$run.txt" 2>&1 ||
		    { bad "determinism: the test does not build: $(head -2 "$work/build.$run.txt")"; rm -rf "$work"; return; }
		sleep 1
	done
	for f in hello.o hello libhello.a; do
		cmp -s "$work/out1/$f" "$work/out2/$f" || bad "determinism: $f differs between two builds"
	done
	rm -rf "$work"
}

case "${1:-}" in
update)
	update
	;;
check)
	check_trees
	if [ -n "${2:-}" ]; then
		check_tools "$2"
		check_determinism "$2"
	fi
	if [ "$fails" -eq 0 ]; then
		echo "toolchain manifest: ok"
		exit 0
	fi
	echo "toolchain manifest: $fails mismatch(es)"
	exit 1
	;;
*)
	echo "usage: manifest.sh update | check [toolchain]" >&2
	exit 2
	;;
esac
