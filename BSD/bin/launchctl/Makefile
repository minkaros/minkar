# launchctl: loads and manages launchd jobs; "bootstrap" brings the system
# up for launchd (architecture plan, Phase 2). Property lists are read with
# CoreFoundation, so XML plists work as on macOS.

SUBDIR =
MK_WERROR = no
MK_PIE = no

PROG =	launchctl
MAN =	launchctl.1

LIBLAUNCH =	${ROOT_SOURCE_DIR}/Libraries/Libsystem/liblaunch
LAUNCHD =	${ROOT_SOURCE_DIR}/BSD/sbin/launchd
LIBSYSTEM =	${ROOT_SOURCE_DIR}/Libraries/Libsystem

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-DPRIVATE -fblocks -Wno-nullability-completeness \
	-I${LAUNCHD} -I${LIBLAUNCH} -I${LIBSYSTEM}/private \
	-I${MINKAR_SDKROOT}/System/Library/Frameworks/System.framework/PrivateHeaders \
	-I${MINKAR_SDKROOT}/usr/local/include
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -framework CoreFoundation -framework IOKit -ledit \
	-lbsm -lauditd	# audit_quick_start() at boot, as on macOS

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/bin
	cp -f ${PROG} ${SYSROOT_DIR}/bin/

.include <bsd.prog.mk>
