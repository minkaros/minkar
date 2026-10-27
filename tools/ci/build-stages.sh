#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Builds the system one top-level stage at a time, each with its own log
# and timing, for CI and for timed local builds. Run it as root from the
# top of the source tree:
#
#   sudo tools/ci/build-stages.sh                    # every stage
#   sudo STAGES="Developer" tools/ci/build-stages.sh # some of them
#
# STAGES   stages to build, in order (default: all of "world")
# LOGDIR   where the logs go (default: ../build/logs)
# JOBS     parallel jobs (default: the number of CPUs)
#
# On failure it prints the end of the failing stage's log; under GitHub
# Actions it also becomes an error annotation, so it can be read without
# downloading the logs.

set -u

SRC=$(pwd)
if [ ! -f "$SRC/BSD/share/mk/sys.mk" ]; then
	echo "build-stages.sh: run it from the top of the Minkar source tree" >&2
	exit 1
fi
LOGDIR=${LOGDIR:-$SRC/../build/logs}
JOBS=${JOBS:-$(nproc)}
STAGES=${STAGES:-"BSD/hier Developer Kernel Libraries/libfirehose_kernel \
Libraries Frameworks Kernel/Extensions CoreServices BSD SystemLibrary"}
TAIL_LINES=${TAIL_LINES:-60}

mkdir -p "$LOGDIR"
TIMES=$LOGDIR/times.txt
echo "stage wall_s user_s sys_s cpu% rc" > "$TIMES"

# The lines that say what went wrong: in a parallel build they are rarely
# at the end of the log. Compiler and linker errors and make's "***"
# lines; not configure's "checking..." chatter or source code that a tool
# echoes.
errors() {
	sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\r//g' "$1" |
	    grep -n -E ': (fatal )?error:|^ld: |\*\*\* \[|\*\*\* Error|Error [0-9]+$|undefined reference|Undefined symbols|referenced from:|cannot find -l|command not found|No rule to make target|CMake Error' |
	    grep -v -E -- '-W[a-z-]*error|warning:' | head -n "${ERROR_LINES:-10}"
}

# One GitHub annotation, which GitHub cuts at 4096 characters: the error
# lines and the end of the log, each line cut to fit. No colors, newlines
# as %0A.
annotate() {
	printf '::error title=%s failed::' "$1"
	{ echo "Error lines:"; errors "$2" | cut -c1-220
	  echo "End of the log:"; tail -n 15 "$2" | cut -c1-140; } |
	    sed -e 's/\x1b\[[0-9;]*m//g' -e 's/\r//g' -e 's/%/%25/g' |
	    awk '{ printf "%s%%0A", $0 }' | cut -c1-4000
	echo
}

run() {
	name=$1
	shift
	log=$LOGDIR/$(echo "$name" | tr / _).log
	[ -n "${GITHUB_ACTIONS:-}" ] && echo "::group::$name"
	echo "build-stages: $name -> $log"
	/usr/bin/time -f "%e %U %S %P" -o "$LOGDIR/t.tmp" "$@" > "$log" 2>&1
	rc=$?
	echo "$name $(tail -1 "$LOGDIR/t.tmp") $rc" >> "$TIMES"
	echo "build-stages: $name: $(tail -1 "$LOGDIR/t.tmp") rc=$rc"
	[ -n "${GITHUB_ACTIONS:-}" ] && echo "::endgroup::"
	if [ $rc -ne 0 ]; then
		echo "FAILED at $name" >> "$TIMES"
		echo "build-stages: $name failed; error lines:"
		errors "$log"
		echo "build-stages: end of $log:"
		tail -n "$TAIL_LINES" "$log" | sed 's/\x1b\[[0-9;]*m//g'
		[ -n "${GITHUB_ACTIONS:-}" ] && annotate "$name" "$log"
		exit $rc
	fi
}

T0=$(date +%s)
run version env SUBDIR= bmake -m "$SRC/BSD/share/mk" version
for s in $STAGES; do
	run "$s" bmake -m "$SRC/BSD/share/mk" -j"$JOBS" ONLY_STAGE="$s" "$s"
done
echo "TOTAL $(( $(date +%s) - T0 ))s" >> "$TIMES"
cat "$TIMES"
