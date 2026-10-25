# ------------------------------------------------------------------------
#  RAVYNOS BUILD SYSTEM - Developer Toolchain
# ------------------------------------------------------------------------

# The host compiler must be clang: the toolchain's sources (cctools, the
# Apple headers in Default.xctoolchain/include) reject gcc. On Linux,
# /usr/bin/cc is often gcc.
HOST_CC ?= clang
HOST_CXX ?= clang++
HOST_AR ?= /usr/bin/ar
.export HOST_CC HOST_AR HOST_CXX

SUBDIR = ravynOS.platform Default.xctoolchain ravynOS.sdk

.include <bsd.subdir.mk>
