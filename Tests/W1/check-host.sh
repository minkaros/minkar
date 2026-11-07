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

status=0
for suite in $suites; do
	name=$(basename "$suite" .yaml)
	dir=$out/$name
	on_host "rm -rf $remote && mkdir -p $remote" || { echo "w1-check: cannot reach $host"; exit 1; }
	start=$(date +%s)
	(cd "$webkit" && ruby Tools/Scripts/run-jsc-stress-tests --jsc "$jsc" --os darwin --arch x86_64 \
	    --remote-config-file "$out/remote.json" --shell-runner --release --quick \
	    -o "$dir" "JSTests/$suite") > "$out/$name.log" 2>&1
	on_host "rm -rf $remote"
	passed=$(grep -c . "$dir/passed" 2>/dev/null) || passed=0
	failed=$(grep -c . "$dir/failed" 2>/dev/null) || failed=0
	noresult=$(grep -c . "$dir/noresult" 2>/dev/null) || noresult=0
	# Failures skip.txt explains (its first word is the test's name).
	unexplained=0
	if [ "$failed" -gt 0 ]; then
		unexplained=$(cut -d' ' -f1 "$here/skip.txt" 2>/dev/null | grep -v '^#' |
		    grep -vxF -f - "$dir/failed" | grep -c .) || unexplained=0
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
