# less (Mark Nudelman, Less License or BSD-2: LICENSE), from FreeBSD's
# contrib/less in git history, with FreeBSD's defines.h. more is a link,
# as on macOS. Terminal handling from ncurses.

SUBDIR =
MK_WERROR = no
MAN =
PROG =	less
SRCS =	main.c screen.c brac.c ch.c charset.c cmdbuf.c command.c cvt.c \
	decode.c evar.c edit.c filename.c forwback.c help.c ifile.c input.c \
	jump.c lesskey_parse.c \
	line.c linenum.c lsystem.c mark.c optfunc.c option.c \
	opttbl.c os.c output.c pattern.c position.c prompt.c search.c \
	signal.c tags.c ttyin.c version.c xbuf.c

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-nullability-completeness -I${.CURDIR}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}
LDADD += -lncurses

all: to-sysroot

to-sysroot: ${PROG}
	mkdir -p ${SYSROOT_DIR}/usr/bin
	install -m 0555 ${PROG} ${SYSROOT_DIR}/usr/bin/
	ln -f ${SYSROOT_DIR}/usr/bin/less ${SYSROOT_DIR}/usr/bin/more

.include <bsd.prog.mk>
