# ------------------------------------------------------------------------
#  RAVYNOS BUILD SYSTEM - Developer Toolchain
# ------------------------------------------------------------------------

# The host compiler must be clang: the toolchain's sources (cctools, the
# Apple headers in Default.xctoolchain/include) reject gcc. On Linux,
# /usr/bin/cc is often gcc. Absolute paths: the kernel build puts the
# Minkar toolchain (whose clang targets Darwin) first in PATH.
HOST_CC ?= /usr/bin/clang
HOST_CXX ?= /usr/bin/clang++
HOST_AR ?= /usr/bin/ar
.export HOST_CC HOST_AR HOST_CXX

SUBDIR = Minkar.platform Default.xctoolchain Minkar.sdk

.include <bsd.subdir.mk>
