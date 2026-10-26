#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Symbol coverage of the reference apps (tools/symbols), after the build:
# downloads the apps, writes the report to $BUILD_DIR/symbols and, under
# GitHub Actions, adds the per-app table to the run's summary. Run from
# the top of the source tree with BUILD_DIR set.

set -eu

: "${BUILD_DIR:?}"
apps=${RUNNER_TEMP:-$BUILD_DIR}/reference-apps
out=$BUILD_DIR/symbols
tools=$BUILD_DIR/Developer/Platforms/Minkar.platform/Developer/Toolchains/Default.xctoolchain/usr/bin

python3 -B tools/symbols/fetch-reference-apps.py --dest "$apps"
mkdir -p "$out"
python3 -B tools/symbols/symbol-inventory.py --sysroot "$BUILD_DIR/sysroot" \
    --apps "$apps/apps" --out "$out" --tools "$tools"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
	{
		echo "## Symbol coverage of the reference apps"
		echo
		sed -n '/^Imported symbols found in Minkar/p' "$out/coverage.md"
		echo
		sed -n '/^| App |/,/^$/p' "$out/coverage.md"
		echo "The full report (per library, missing symbols) is in the"
		echo "symbol-coverage artifact."
	} >> "$GITHUB_STEP_SUMMARY"
fi
