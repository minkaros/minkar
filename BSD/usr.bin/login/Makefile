# login: log into the system through PAM (architecture plan, Phase 2:
# console login). Set-user-ID root, as on macOS.

SUBDIR =

PROG =	login
SRCS =	login.c login_audit.c
MAN =

CFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wall -Wextra -Wno-nullability-completeness
LDFLAGS = --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -lpam -lbsm

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -o 0 -g 0 -m 4555 ${PROG} ${SYSROOT_DIR}/usr/bin/${PROG}

.include <bsd.prog.mk>
