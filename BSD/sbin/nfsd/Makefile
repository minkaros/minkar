# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# nfsd: Apple's NFS server daemon (nfsd with mountd in it), from
# NFS-136.100.3 (macOS 10.15.6; APSL and BSD, each file's header;
# github.com/apple-oss-distributions/NFS, the tarball mount_nfs came from),
# for the NFS server xnu 19.6 carries. dist/ holds Apple's files, with
# rpc.lockd's Mach interface; changes for Darwin are the lines marked
# Minkar. Ours (BSD-2): oncrpc/, what Apple's private oncrpc adds to
# libinfo's Sun RPC (registering with rpcbind by transport and address),
# and compat/, the ServiceManagement calls it makes, over launchd.

SUBDIR =
MK_WERROR = no
WARNS ?= 0
PROG =	nfsd
SRCS =	main.c mountd.c nfsd.c lockd_machUser.c oncrpc.c sm_compat.c
MAN =
.PATH: ${.CURDIR}/dist ${.CURDIR}/oncrpc ${.CURDIR}/compat
LIBUTIL =	${ROOT_SOURCE_DIR}/BSD/lib/libutil
CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -DPRIVATE -I${LIBUTIL} \
	-I${.CURDIR} -I${.CURDIR}/compat -I${.CURDIR}/dist -I${.OBJDIR}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${LIBUTIL} -lutil -framework CoreFoundation \
	-framework IOKit
CLEANFILES += lockd_mach.h lockd_machUser.c

# The client side of rpc.lockd's Mach interface (nfsd pings lockd).
lockd_mach.h lockd_machUser.c: ${.CURDIR}/dist/lockd_mach.defs
	${MIG} -cc ${CC} -novouchers -I${.CURDIR}/dist \
	    -I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	    -mmacos-version-min=${MACOS_VERSION_MIN} \
	    -header lockd_mach.h -user lockd_machUser.c -server /dev/null \
	    -sheader /dev/null ${.ALLSRC}

main.o main.pieo: lockd_mach.h

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin ${SYSROOT_DIR}/usr/share/man/man5 \
	    ${SYSROOT_DIR}/usr/share/man/man8 \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons
	install -m 0555 -o 0 -g 0 ${PROG} ${SYSROOT_DIR}/sbin/nfsd
	install -m 0444 ${.CURDIR}/dist/nfsd.8 ${.CURDIR}/dist/mountd.8 \
	    ${SYSROOT_DIR}/usr/share/man/man8/
	install -m 0444 ${.CURDIR}/dist/exports.5 ${SYSROOT_DIR}/usr/share/man/man5/
	install -m 0644 ${.CURDIR}/dist/org.minkar.nfsd.plist \
	    ${SYSROOT_DIR}/System/Library/LaunchDaemons/

.include <bsd.prog.mk>
