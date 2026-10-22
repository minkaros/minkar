# Host-side tests for the CGL implementation in Frameworks/OpenGL.
# CGL is portable C with pthreads, so the same sources are built for the
# build host and exercised under AddressSanitizer/UBSan.
#
#   make -C Frameworks/OpenGL/tests          (GNU make or bmake)
#   make -C Frameworks/OpenGL/tests clean
#
# Not part of the ravynOS build; nothing here is installed.

# clang, like the ravynOS build (override with CC=...)
CC =		clang
SRCDIR =	..
BUILDDIR =	build
CGL_SRCS =	$(SRCDIR)/CGLContext.c $(SRCDIR)/CGLDriverRegistry.c \
		$(SRCDIR)/CGLGlobal.c $(SRCDIR)/CGLNullDriver.c \
		$(SRCDIR)/CGLPBuffer.c $(SRCDIR)/CGLPixelFormat.c
TEST_CFLAGS =	-std=gnu17 -g -Wall -Wextra -Werror \
		-fsanitize=address,undefined -fno-omit-frame-pointer \
		-I$(BUILDDIR)/include -I$(SRCDIR) -pthread

all: check

$(BUILDDIR)/include/OpenGL:
	mkdir -p $(BUILDDIR)/include
	ln -sfn ../../$(SRCDIR)/include $(BUILDDIR)/include/OpenGL

$(BUILDDIR)/test_cgl: test_cgl.c $(CGL_SRCS) $(BUILDDIR)/include/OpenGL
	$(CC) $(TEST_CFLAGS) -o $@ test_cgl.c $(CGL_SRCS)

$(BUILDDIR)/test_null_driver: test_null_driver.c $(CGL_SRCS) $(BUILDDIR)/include/OpenGL
	$(CC) $(TEST_CFLAGS) -o $@ test_null_driver.c $(CGL_SRCS)

check: $(BUILDDIR)/test_cgl $(BUILDDIR)/test_null_driver
	env CGL_ENABLE_NULL_RENDERER=0 $(BUILDDIR)/test_cgl
	env CGL_ENABLE_NULL_RENDERER=0 $(BUILDDIR)/test_null_driver
	env CGL_ENABLE_NULL_RENDERER=1 $(BUILDDIR)/test_null_driver

clean:
	rm -rf $(BUILDDIR)

.PHONY: all check clean
