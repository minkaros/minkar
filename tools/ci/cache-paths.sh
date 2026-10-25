#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Prints the toolchain cache's key and paths as GitHub step outputs. Run
# from the top of the source tree, with BUILD_DIR set.
#
# The cache holds the installed toolchain and the toolchain's object
# directories, without LLVM's build tree (llvm_host, about 2 GB): the
# object files keep bmake from rebuilding mig, which LLVM's rule depends
# on. The key is the git tree of Developer/Default.xctoolchain, so any
# change there builds the toolchain again.

set -eu

: "${BUILD_DIR:?}"
SRC=$(pwd)
TREE=$(git rev-parse HEAD:Developer/Default.xctoolchain)
OBJ=$BUILD_DIR$SRC/Developer/Default.xctoolchain

echo "key=minkar-toolchain-${CACHE_VERSION:-v1}-ubuntu22.04-$(uname -m)-$TREE"
echo "toolchain=$BUILD_DIR/Developer/Platforms/Minkar.platform/Developer/Toolchains"
echo "objdir=$OBJ"
echo "paths<<EOF_PATHS"
echo "$BUILD_DIR/Developer/Platforms/Minkar.platform/Developer/Toolchains"
echo "$OBJ"
echo "!$OBJ/llvm_host"
echo "EOF_PATHS"
