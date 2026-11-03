#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Runs Khronos's Vulkan conformance tests (deqp-vk) from this directory on
# the machine under test, through /usr/lib/libvulkan.1.dylib:
#
#	sh run.sh [CASELIST]	(default caselist.txt, G3's subset)
#
# The full log goes to results.qpa; the totals and each case that did not
# pass or was not supported are printed, then the totals again. The exit
# status is 0 only when no case failed.
cd "$(dirname "$0")" || exit 1
list=${1:-caselist.txt}
./deqp-vk --deqp-caselist-file="$list" --deqp-log-filename=results.qpa \
    --deqp-log-images=disable --deqp-log-shader-sources=disable \
    --deqp-archive-dir=. > results.txt 2>&1
status=$?
grep -B1 -E "^  (Fail|Crash|Timeout|InternalError|ResourceError|QualityWarning|CompatibilityWarning)" \
    results.txt | grep -E "^Test case|^  " | sed -n '1,200p'
tail -12 results.txt | grep -E "Test run totals|Passed|Failed|Not supported|Warnings|Waived"
grep -qE "^  +Failed: +0/" results.txt || status=1
exit $status
