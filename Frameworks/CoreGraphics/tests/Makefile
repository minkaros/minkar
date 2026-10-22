# Host-side tests for plain-C parts of CoreGraphics.
#
#   make -C Frameworks/CoreGraphics/tests          (GNU make or bmake)
#
# Not part of the ravynOS build; nothing here is installed.

CC =		clang
TOP =		../../..
BUILDDIR =	build
TEST_CFLAGS =	-std=gnu17 -g -Wall -Wextra -Werror -Wno-fixed-enum-extension \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I$(TOP)/Frameworks -I$(TOP)/CoreServices -include stdint.h

all: check

$(BUILDDIR)/test_window_list: test_window_list.c ../CGWindowListFilter.c ../CGWindowListFilter.h
	mkdir -p $(BUILDDIR)
	$(CC) $(TEST_CFLAGS) -o $@ test_window_list.c ../CGWindowListFilter.c

check: $(BUILDDIR)/test_window_list
	$(BUILDDIR)/test_window_list

clean:
	rm -rf $(BUILDDIR)

.PHONY: all check clean
