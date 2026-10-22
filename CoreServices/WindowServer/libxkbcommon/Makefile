# libxkbcommon 1.6.0 (MIT/X11), built as a static library for WindowServer.
# Our own bmake rules replace meson; keep SRCS in sync with
# libxkbcommon_sources in meson.build when upgrading. The keymap registry
# (libxkbregistry) and the X11 support library are not built.
#
# The keymap parser is generated with Berkeley yacc (public domain), not
# GNU bison. byacc spells bison's "%define api.pure" as "%pure-parser".

LIB = xkbcommon
SUBDIR =
MK_WERROR = no
MK_PROFILE = no
MK_INSTALLLIB = no
WARNS = 1

SRCS = \
	src/compose/parser.c \
	src/compose/paths.c \
	src/compose/state.c \
	src/compose/table.c \
	src/xkbcomp/action.c \
	src/xkbcomp/ast-build.c \
	src/xkbcomp/compat.c \
	src/xkbcomp/expr.c \
	src/xkbcomp/include.c \
	src/xkbcomp/keycodes.c \
	src/xkbcomp/keymap.c \
	src/xkbcomp/keymap-dump.c \
	src/xkbcomp/keywords.c \
	src/xkbcomp/parser.c \
	src/xkbcomp/rules.c \
	src/xkbcomp/scanner.c \
	src/xkbcomp/symbols.c \
	src/xkbcomp/types.c \
	src/xkbcomp/vmod.c \
	src/xkbcomp/xkbcomp.c \
	src/atom.c \
	src/context.c \
	src/context-priv.c \
	src/keysym.c \
	src/keysym-utf.c \
	src/keymap.c \
	src/keymap-priv.c \
	src/state.c \
	src/text.c \
	src/utf8.c \
	src/utils.c

# Objects keep the source tree layout; create it at parse time.
_OBJ_DIRS != mkdir -p ${.OBJDIR}/src/compose ${.OBJDIR}/src/xkbcomp && \
	touch ${.OBJDIR}/config.h && echo ok

YACC_BIN ?= byacc

src/xkbcomp/parser.c: ${.CURDIR}/src/xkbcomp/parser.y
	@command -v ${YACC_BIN} >/dev/null || \
	    { echo "byacc (Berkeley yacc) is required on the build host"; exit 1; }
	sed 's/^%define api.pure$$/%pure-parser/' \
	    ${.CURDIR}/src/xkbcomp/parser.y > ${.OBJDIR}/parser.y
	${YACC_BIN} -H ${.OBJDIR}/src/xkbcomp/parser.h -o ${.TARGET} \
	    -p _xkbcommon_ ${.OBJDIR}/parser.y

src/xkbcomp/parser.o src/xkbcomp/scanner.o src/xkbcomp/keywords.o: \
	src/xkbcomp/parser.c

# Settings meson writes to config.h.
CFLAGS += --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-Wno-incompatible-sysroot -std=gnu11 -fvisibility=hidden \
	-I${.OBJDIR} -I${.OBJDIR}/src/xkbcomp -I${.CURDIR}/include -I${.CURDIR}/src \
	-I${.CURDIR}/src/xkbcomp \
	-DHAVE_ASPRINTF -DHAVE_VASPRINTF -DHAVE_STRNDUP -DHAVE_UNISTD_H \
	-DHAVE_MMAP -DHAVE___BUILTIN_EXPECT \
	-D'DFLT_XKB_CONFIG_ROOT="/System/Library/KeyboardLayout"' \
	-D'DFLT_XKB_CONFIG_EXTRA_PATH="/Library/KeyboardLayout"' \
	-D'XLOCALEDIR="/System/Library/KeyboardLayout/locale"' \
	-D'DEFAULT_XKB_RULES="evdev"' \
	-D'DEFAULT_XKB_MODEL="pc105"' \
	-D'DEFAULT_XKB_LAYOUT="us"' \
	-DDEFAULT_XKB_VARIANT=NULL \
	-DDEFAULT_XKB_OPTIONS=NULL

.include <bsd.lib.mk>
