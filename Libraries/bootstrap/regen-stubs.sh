#!/bin/sh
# Regenerates the bootstrap link stubs (stubs/**/*.tbd) from a built SDK.
#
# The base libraries link against each other in cycles (libsystem_c and
# CrashReporterClient, libsystem_malloc and libsystem_c, libc++ and
# libSystem...). A clean build links them against these stubs, which only
# list the exported symbols. Run this after a library gains a symbol that
# another base library uses, then commit the stubs.
#
#   regen-stubs.sh [build dir]     (default: ../build next to the sources)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
BUILD=${1:-$SRC/../build}
DEV=$BUILD/Developer/Platforms/Minkar.platform/Developer
SDK=$DEV/SDKs/Minkar.sdk
TAPI=$DEV/Toolchains/Default.xctoolchain/usr/bin/tapi

[ -x "$TAPI" ] || { echo "no tapi in $TAPI" >&2; exit 1; }

# stub path (relative to the SDK)   built library
stub() {
	mkdir -p "$HERE/stubs/$(dirname "$1")"
	"$TAPI" stubify "$SDK/$2" -o "$HERE/stubs/$1"
}

for f in "$SDK"/usr/lib/system/*.dylib; do
	n=$(basename "$f" .dylib)
	stub usr/lib/system/$n.tbd usr/lib/system/$n.dylib
done
stub usr/lib/libSystem.tbd usr/lib/libSystem.B.dylib
stub usr/lib/libc++.tbd usr/lib/libc++.1.dylib
stub usr/lib/libobjc.tbd usr/lib/libobjc.A.dylib
echo "stubs updated in $HERE/stubs"
