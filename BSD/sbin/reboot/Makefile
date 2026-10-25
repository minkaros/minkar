.include <src.opts.mk>

SUBDIR =
MK_WERROR = no
MAN =
CFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -Wno-nullability-completeness -I${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat/include -include bsdcompat.h -I${ROOT_SOURCE_DIR}/BSD/lib/libutil
LDFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libutil -lutil -L${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/BSD/lib/libbsdcompat -lbsdcompat
PROG=	reboot
MLINKS=	reboot.8 halt.8 reboot.8 fastboot.8 reboot.8 fasthalt.8
MLINKS+= boot_${MACHINE}.8 boot.8
MLINKS+= boot_i386.8 boot.8
LINKS=	${BINDIR}/reboot ${BINDIR}/halt  	${BINDIR}/reboot ${BINDIR}/fastboot  	${BINDIR}/reboot ${BINDIR}/fasthalt  	${BINDIR}/reboot ${BINDIR}/nextboot

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/sbin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/sbin/
	ln -f ${SYSROOT_DIR}/sbin/reboot ${SYSROOT_DIR}/sbin/halt
	ln -f ${SYSROOT_DIR}/sbin/reboot ${SYSROOT_DIR}/sbin/poweroff
	ln -f ${SYSROOT_DIR}/sbin/reboot ${SYSROOT_DIR}/sbin/fastboot
	ln -f ${SYSROOT_DIR}/sbin/reboot ${SYSROOT_DIR}/sbin/fasthalt
	ln -f ${SYSROOT_DIR}/sbin/reboot ${SYSROOT_DIR}/sbin/nextboot

.include <bsd.prog.mk>
