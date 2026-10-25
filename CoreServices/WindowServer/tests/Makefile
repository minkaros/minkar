# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# Host-side tests for WindowServer's portable C code.
#
#   make -C CoreServices/WindowServer/tests          (GNU make or bmake)
#   make -C CoreServices/WindowServer/tests clean
#
# libxkbcommon is built for the build host from the vendored sources
# (parser generated with byacc, as in the Minkar build). The keymaps come
# from a finished build's sysroot; set XKB_ROOT to use another copy.
# Not part of the Minkar build; nothing here is installed.

CC =		clang
SRCDIR =	..
XKB =		$(SRCDIR)/libxkbcommon
BUILDDIR =	build
XKB_ROOT =	$(SRCDIR)/../../../build/sysroot/System/Library/KeyboardLayout

XKB_SRCS =	$(XKB)/src/compose/parser.c $(XKB)/src/compose/paths.c \
		$(XKB)/src/compose/state.c $(XKB)/src/compose/table.c \
		$(XKB)/src/xkbcomp/action.c $(XKB)/src/xkbcomp/ast-build.c \
		$(XKB)/src/xkbcomp/compat.c $(XKB)/src/xkbcomp/expr.c \
		$(XKB)/src/xkbcomp/include.c $(XKB)/src/xkbcomp/keycodes.c \
		$(XKB)/src/xkbcomp/keymap.c $(XKB)/src/xkbcomp/keymap-dump.c \
		$(XKB)/src/xkbcomp/keywords.c $(XKB)/src/xkbcomp/rules.c \
		$(XKB)/src/xkbcomp/scanner.c $(XKB)/src/xkbcomp/symbols.c \
		$(XKB)/src/xkbcomp/types.c $(XKB)/src/xkbcomp/vmod.c \
		$(XKB)/src/xkbcomp/xkbcomp.c $(XKB)/src/atom.c $(XKB)/src/context.c \
		$(XKB)/src/context-priv.c $(XKB)/src/keysym.c $(XKB)/src/keysym-utf.c \
		$(XKB)/src/keymap.c $(XKB)/src/keymap-priv.c $(XKB)/src/state.c \
		$(XKB)/src/text.c $(XKB)/src/utf8.c $(XKB)/src/utils.c \
		$(BUILDDIR)/parser.c
XKB_CFLAGS =	-std=gnu11 -w -I$(BUILDDIR) -I$(XKB)/include -I$(XKB)/src \
		-I$(XKB)/src/xkbcomp -DHAVE_ASPRINTF -DHAVE_VASPRINTF \
		-DHAVE_STRNDUP -DHAVE_UNISTD_H -DHAVE_MMAP \
		-D'DFLT_XKB_CONFIG_ROOT="/nonexistent"' \
		-D'DFLT_XKB_CONFIG_EXTRA_PATH="/nonexistent"' \
		-D'XLOCALEDIR="/nonexistent"' -D'DEFAULT_XKB_RULES="evdev"' \
		-D'DEFAULT_XKB_MODEL="pc105"' -D'DEFAULT_XKB_LAYOUT="us"' \
		-DDEFAULT_XKB_VARIANT=NULL -DDEFAULT_XKB_OPTIONS=NULL
TEST_CFLAGS =	-std=gnu17 -g -Wall -Wextra -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I$(SRCDIR) -I$(XKB)/include

all: check

$(BUILDDIR)/parser.c: $(XKB)/src/xkbcomp/parser.y
	mkdir -p $(BUILDDIR)
	sed 's/^%define api.pure$$/%pure-parser/' $(XKB)/src/xkbcomp/parser.y > $(BUILDDIR)/parser.y
	byacc -H $(BUILDDIR)/parser.h -o $@ -p _xkbcommon_ $(BUILDDIR)/parser.y
	touch $(BUILDDIR)/config.h

$(BUILDDIR)/libxkbcommon.a: $(XKB_SRCS)
	rm -rf $(BUILDDIR)/xkbobj && mkdir -p $(BUILDDIR)/xkbobj
	for f in $(XKB_SRCS); do \
	    o=$(BUILDDIR)/xkbobj/$$(echo $$f | tr '/.' '__').o; \
	    $(CC) $(XKB_CFLAGS) -fsanitize=address -c $$f -o $$o || exit 1; \
	done
	ar rcs $@ $(BUILDDIR)/xkbobj/*.o

$(BUILDDIR)/test_keymap: test_keymap.c $(SRCDIR)/WSKeymap.c $(SRCDIR)/WSKeymap.h $(BUILDDIR)/libxkbcommon.a
	$(CC) $(TEST_CFLAGS) -o $@ test_keymap.c $(SRCDIR)/WSKeymap.c $(BUILDDIR)/libxkbcommon.a

check: $(BUILDDIR)/test_keymap
	$(BUILDDIR)/test_keymap $(XKB_ROOT)

clean:
	rm -rf $(BUILDDIR)
