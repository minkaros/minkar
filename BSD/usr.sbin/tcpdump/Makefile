# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# tcpdump 4.99.7 with libpcap 1.11.0 (BSD-3, dist/LICENSE and
# dist-libpcap/LICENSE; tcpdump.org), in /usr/sbin as on macOS. dist/ and
# dist-libpcap/ hold the Tcpdump Group's signed release tarballs (sha256
# 39675dfa06fb0e99be8da5d4a1784acbaab58ae0870231d956ff7870cb99eec1 and
# 9237f5bae9dcf3a91823d9963ec43b7c0e2e3374ef2ad57d92c8cd39530f4723), the
# first without its tests/. libpcap captures through Darwin's BPF and is
# built here, static, in ${.OBJDIR}/libpcap, where tcpdump's configure
# finds it; its grammar and scanner are made with the build machine's
# bison and flex. Without libcrypto (ESP decryption) or libsmi.
# MACOSX_DEPLOYMENT_TARGET is left out of their environment: configure
# compiles some tests without our flags, and the toolchain's clang then
# fails on it alone ("conflicting deployment targets", a toolchain bug).

all: ${.OBJDIR}/.installed

PCAP_FLAGS =	--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}

${.OBJDIR}/tcpdump/tcpdump: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/libpcap ${.OBJDIR}/tcpdump
	mkdir -p ${.OBJDIR}/libpcap ${.OBJDIR}/tcpdump
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist-libpcap
	sh ${ROOT_SOURCE_DIR}/tools/build/autotools-times.sh ${.CURDIR}/dist
	cd ${.OBJDIR}/libpcap && env -u CFLAGS -u LDFLAGS -u MACOSX_DEPLOYMENT_TARGET \
	    CC=${CC} AR=${AR} RANLIB=${RANLIB} PKG_CONFIG=false \
	    CFLAGS="${PCAP_FLAGS} -O2 -Wno-nullability-completeness" \
	    LDFLAGS="${PCAP_FLAGS} -fuse-ld=${LD}" \
	    ${.CURDIR}/dist-libpcap/configure --prefix=/usr \
	    --host=${CpuArch}-apple-darwin19 --with-pcap=bpf --disable-shared \
	    --disable-universal --disable-usb --disable-netmap \
	    --disable-bluetooth --disable-dbus --disable-rdma --without-libnl
	cd ${.OBJDIR}/libpcap && env -u CFLAGS -u LDFLAGS -u MACOSX_DEPLOYMENT_TARGET MAKEFLAGS="" \
	    ${TOOLS}/gmake -j${.MAKE.JOBS:U1} libpcap.a pcap-config
	cd ${.OBJDIR}/tcpdump && env -u CFLAGS -u LDFLAGS -u MACOSX_DEPLOYMENT_TARGET \
	    CC=${CC} AR=${AR} RANLIB=${RANLIB} PKG_CONFIG=false \
	    ac_cv_buggy_ether_ntohost=no \
	    CFLAGS="${PCAP_FLAGS} -O2 -Wno-nullability-completeness -I${.CURDIR}/dist-libpcap" \
	    LDFLAGS="${PCAP_FLAGS} -fuse-ld=${LD}" \
	    ${.CURDIR}/dist/configure --prefix=/usr \
	    --host=${CpuArch}-apple-darwin19 --without-crypto --without-smi \
	    --without-cap-ng
	cd ${.OBJDIR}/tcpdump && env -u CFLAGS -u LDFLAGS -u MACOSX_DEPLOYMENT_TARGET MAKEFLAGS="" \
	    ${TOOLS}/gmake -j${.MAKE.JOBS:U1} tcpdump

${.OBJDIR}/.installed: ${.OBJDIR}/tcpdump/tcpdump
	mkdir -p ${SYSROOT_DIR}/usr/sbin ${SYSROOT_DIR}/usr/share/man/man1 \
	    ${SYSROOT_DIR}/usr/share/man/man7
	install -m 0555 -o 0 -g 0 ${.OBJDIR}/tcpdump/tcpdump ${SYSROOT_DIR}/usr/sbin/tcpdump
	install -m 0444 ${.OBJDIR}/tcpdump/tcpdump.1 ${SYSROOT_DIR}/usr/share/man/man1/
	install -m 0444 ${.OBJDIR}/libpcap/pcap-filter.manmisc \
	    ${SYSROOT_DIR}/usr/share/man/man7/pcap-filter.7
	touch ${.TARGET}
