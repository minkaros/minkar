#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Lane ARM's ARM0 (Docs/ROADMAP.md): the parts of the userland and the
# frameworks that already compile for arm64 (tools/ci/arm64-dirs.txt) are
# compiled for it, objects only, after the x86-64 build (they use its
# Minkar.sdk and its generated files). Run from the top of the source tree
# with BUILD_DIR set; it fails if one object does not compile.
#
# The tree's own clang is built for x86 only so far (pending: AArch64 in
# LLVM_TARGETS_TO_BUILD), so the compiler is the clang of the swift.org
# release the toolchain already uses as a build tool
# (Developer/Default.xctoolchain/swift/MANIFEST.host-swift, pinned by
# SHA-256). The SDK has no arm64 headers yet (pending: XNU's headers
# installed for ARM64), so the ones these parts need are copied here from
# Kernel/xnu, and BSD/include's TargetConditionals.h, which knows arm64,
# goes before the SDK's.

set -eu

: "${BUILD_DIR:?}"
SRC=$(pwd)
OUT=$BUILD_DIR/arm64
XNU=$SRC/Kernel/xnu
JOBS=${JOBS:-4}

host=$(sh BSD/lib/libdrm/fetch.sh Developer/Default.xctoolchain/swift/MANIFEST.host-swift \
    "$BUILD_DIR/src-cache")
inc=$OUT/include
rm -rf "$inc"
mkdir -p "$inc/first" "$inc/mach/arm" "$inc/mach/arm64" "$inc/arm" "$inc/libkern/arm" \
    "$inc/frameworks"
# System.framework's headers, with the arm ones added: found before the SDK's.
cp -R "$BUILD_DIR/Developer/Platforms/Minkar.platform/Developer/SDKs/Minkar.sdk/System/Library/Frameworks/System.framework" \
    "$inc/frameworks/"
mkdir -p "$inc/frameworks/System.framework/PrivateHeaders/arm"
cp "$XNU"/osfmk/mach/arm/*.h "$XNU"/osfmk/arm/asm.h "$inc/mach/arm/"
cp "$XNU"/osfmk/arm64/asm.h "$inc/mach/arm64/"
cp "$XNU"/bsd/arm/*.h "$XNU"/osfmk/arm/arch.h "$inc/arm/"
cp "$XNU"/libkern/libkern/arm/OSByteOrder.h "$inc/libkern/arm/"
cp "$XNU"/osfmk/arm/cpu_capabilities.h "$inc/frameworks/System.framework/PrivateHeaders/arm/"
cp "$SRC"/BSD/include/TargetConditionals.h "$inc/first/"

export ARM64_CLANG="$host/usr/bin/clang" ARM64_INCLUDE="$inc"
"$ARM64_CLANG" --print-targets | grep -q aarch64 || { echo "arm64-compile: $ARM64_CLANG has no arm64" >&2; exit 1; }
dirs=$(grep -v '^#' "$SRC/tools/ci/arm64-dirs.txt" | tr '\n' ' ')
bmake -m "$SRC/BSD/share/mk" -j"$JOBS" arm64-objs ARM64_DIRS="$dirs" \
    ARM64_CC="$SRC/tools/ci/arm64-cc" ARM64_CXX="$SRC/tools/ci/arm64-c++" \
    ARM64_OBJ="$OUT/obj"
echo "arm64-compile: every object of $(echo $dirs | wc -w) directories compiled for arm64"
