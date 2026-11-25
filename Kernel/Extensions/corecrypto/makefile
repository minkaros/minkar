MK_WERROR = no
SUBDIR =
KEXT = corecrypto
KERNEL_PRIVATE = yes
INFO_PLIST = kext/Info.plist
BUNDLE_IDENTIFIER = com.apple.kec.corecrypto
BUNDLE_VERSION = 1.0.0
MAIN_FUNCTION = cc_kext_start
ANTIMAIN_FUNCTION = cc_kext_stop

SRCS = \
    cc_clear.c \
    cc_cmp_safe.c \
    cc_debug.c \
    cc_digest.c \
    kext/cc_functions.c \
    kext/cc_kext_main.c \
    algorithms/cckprng.c \
    algorithms/digest.c \
    algorithms/md5.c \
    algorithms/sha1.c \
    algorithms/sha2.c \
    algorithms/aes.c \
    algorithms/unsupported.c \
    algorithms/yarrow/comp.c \
    algorithms/yarrow/smf.c \
    algorithms/yarrow/sha1mod.c \
    algorithms/yarrow/prng.c \
    algorithms/yarrow/yarrowUtils.c

# Optimized (-Os, as kexts are built; without it the kernel's encrypted
# swap ran at 1.7 MB/s, AES taking 152 ms a 256 KB segment), and no
# implicit SSE: kernel code must not touch the user's vector registers;
# no builtins: printf must not become puts, which the kernel lacks.
CFLAGS = -Os -fno-builtin -mno-implicit-float \
	-DXNU_KERNEL_PRIVATE=1 -DPRIVATE=1 -DCC_KERNEL=1 \
	-Wno-sign-conversion -Wno-int-conversion \
	-Wno-implicit-function-declaration \
	-Wno-incompatible-library-redeclaration \
	-Wno-undef-prefix \
	-I${.CURDIR} -I${.CURDIR}/kext -I${.CURDIR}/include \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/bsd \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/libkern \
	-I${ROOT_SOURCE_DIR}/Kernel/xnu/osfmk \
	-I${.OBJDIR}/include

all: copyheaders

copyheaders:
	mkdir -p ${.OBJDIR}/include/corecrypto
	cp -f ${.CURDIR}/cc_abort.h ${.OBJDIR}/include/corecrypto

SUBDIR =

.include <minkar.kext.mk>
.include <bsd.subdir.mk>
