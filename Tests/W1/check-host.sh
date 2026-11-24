#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Gate W1 (JavaScriptCore): JSTests' suites, run with WebKit's own runner
# (Tools/Scripts/run-jsc-stress-tests) on a machine running Minkar, over
# ssh, with the jsc of the sysroot, in --quick mode (each test in its most
# telling configurations, as WebKit's bots run it on pull requests).
#
#   check-host.sh SYSROOT WEBKIT HOST KEY OUT [SUITE...]
#
# HOST is user@address[:port]; KEY the private key to log in with (may be
# empty); OUT where the results go, a directory per suite (its passed,
# failed and noresult lists). Each suite runs in /var/tmp/w1 on the host,
# emptied before and after it (its tests and outputs take hundreds of MB).
# A failure listed in skip.txt (the test's name, then the reason) does
# not fail the gate. Ends with "w1-check: PASS" or "w1-check: FAIL".
sysroot=$1 webkit=$2 host=$3 key=$4 out=$5; shift 5
here=$(cd "$(dirname "$0")" && pwd)
suites=${*:-"es6.yaml stress modules.yaml complex.yaml typeProfiler.yaml controlFlowProfiler.yaml exceptionFuzz.yaml executableAllocationFuzz.yaml microbenchmarks.yaml ChakraCore.yaml wasm.yaml mozilla/mozilla-tests.yaml"}
jsc=$sysroot/usr/lib/webkit/JavaScriptCore.framework/Versions/Current/Helpers/jsc
[ -x "$jsc" ] || { echo "w1-check: no jsc in $sysroot" >&2; exit 1; }
[ -d "$webkit/JSTests" ] || { echo "w1-check: no JSTests in $webkit" >&2; exit 1; }
mkdir -p "$out"
out=$(cd "$out" && pwd)

# ssh and scp for the runner: the host's key as found at the start of this
# run (a machine just installed, or a VM booted again, has a new one),
# kept in OUT for this run only.
wrap=$out/.ssh-wrap
rm -f "$out/.known_hosts"
mkdir -p "$wrap"
for tool in ssh scp; do
	real=$(command -v $tool)
	printf '#!/bin/sh\nexec %s -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=%s "$@"\n' \
	    "$real" "$out/.known_hosts" > "$wrap/$tool"
	chmod +x "$wrap/$tool"
done
PATH=$wrap:$PATH
export PATH

user=${host%%@*} rest=${host#*@}
addr=${rest%%:*} port=22
case $rest in *:*) port=${rest##*:} ;; esac
remote=/var/tmp/w1
idfile=
[ -n "$key" ] && idfile=", \"idFilePath\": \"$key\""
printf '{ "remotes": [ { "name": "w1", "address": "%s@%s:%s", "remoteDirectory": "%s"%s } ] }\n' \
    "$user" "$addr" "$port" "$remote" "$idfile" > "$out/remote.json"
on_host() { ssh ${key:+-i "$key"} -p "$port" "$user@$addr" "$@"; }

# What the runner hands each test on the host: it exports these even when
# they are not set, and jsc then warns that the empty values are malformed
# (its output no longer matched ChakraCore's baselines). Seconds, as
# WebKit's bots give them (the runner scales the timeout by the number of
# processes); a test's memory up to 4 GiB: without a limit the tests of
# huge strings and arrays, several at once, took all of PC 1's memory and
# the machine went down (skip.txt lists those that need more).
export JSCTEST_timeout=${JSCTEST_timeout:-300}
export JSCTEST_hardTimeout=${JSCTEST_hardTimeout:-1200}
export JSCTEST_memoryLimit=${JSCTEST_memoryLimit:-4294967296}

# A suite whose list names paths this tree does not have (wasm.yaml takes
# JetStream2 from PerformanceTests, which the fetched tree leaves out): it
# runs from a copy of its directory made of links, its list without them,
# under the same name (the tests keep their names, and skip.txt its
# entries). What is left out is said in the gate's output.
suite_file() {
	yaml=$webkit/JSTests/$1
	case $1 in *.yaml) ;; *) echo "JSTests/$1"; return ;; esac
	ydir=$(dirname "$yaml")
	missing=$(sed -n 's/^- path: *//p' "$yaml" | while read -r p; do [ -e "$ydir/$p" ] || echo "$p"; done)
	if [ -z "$missing" ]; then
		echo "JSTests/$1"
		return
	fi
	copy=$out/.tree/$(dirname "$1")
	rm -rf "$copy" && mkdir -p "$copy"
	for e in "$ydir"/* "$ydir"/.[!.]*; do
		[ -e "$e" ] && ln -s "$e" "$copy/$(basename "$e")"
	done
	rm -f "$copy/$(basename "$yaml")"
	echo "$missing" | awk 'NR == FNR { gone[$0] = 1; next }
	    /^- path:/ { p = $0; sub(/^- path: */, "", p); skip = (p in gone) }
	    !skip' - "$yaml" > "$copy/$(basename "$yaml")"
	echo "$missing" | sed "s|^|w1: $(basename "$1" .yaml): left out (not in the tree): |" >&2
	echo "$copy/$(basename "$yaml")"
}

status=0
for suite in $suites; do
	name=$(basename "$suite" .yaml)
	dir=$out/$name
	on_host "rm -rf $remote && mkdir -p $remote" || { echo "w1-check: cannot reach $host"; exit 1; }
	start=$(date +%s)
	file=$(suite_file "$suite")
	(cd "$webkit" && ruby Tools/Scripts/run-jsc-stress-tests --jsc "$jsc" --os darwin --arch x86_64 \
	    --remote-config-file "$out/remote.json" --shell-runner --release --quick \
	    -o "$dir" "$file") > "$out/$name.log" 2>&1
	on_host "rm -rf $remote"
	passed=$(grep -c . "$dir/passed" 2>/dev/null) || passed=0
	failed=$(grep -c . "$dir/failed" 2>/dev/null) || failed=0
	noresult=$(grep -c . "$dir/noresult" 2>/dev/null) || noresult=0
	# Failures skip.txt explains: its first word is the test's name, with
	# its mode (suite/file.js.mode) or without (every mode of the file).
	unexplained=0
	if [ "$failed" -gt 0 ]; then
		unexplained=$(grep -v '^#' "$here/skip.txt" 2>/dev/null | cut -d' ' -f1 |
		    awk 'NR == FNR { if ($0 != "") known[$0] = 1; next }
		        { file = $0; sub(/\.js\..*$/, ".js", file) }
		        !($0 in known) && !(file in known)' - "$dir/failed" | grep -c .) || unexplained=0
	fi
	secs=$(($(date +%s) - start))
	if [ "$passed" -eq 0 ] || [ "$unexplained" -gt 0 ] || [ "$noresult" -gt 0 ]; then
		echo "w1: $name FAILED: passed $passed, failed $failed ($unexplained not in skip.txt), no result $noresult, ${secs}s"
		[ "$failed" -gt 0 ] && head -20 "$dir/failed" | sed 's/^/    /'
		status=1
	else
		echo "w1: $name passed: $passed (failed $failed, all in skip.txt), ${secs}s"
	fi
done
[ $status -eq 0 ] && echo "w1-check: PASS" || echo "w1-check: FAIL"
exit $status
