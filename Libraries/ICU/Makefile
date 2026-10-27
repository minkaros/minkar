MK_WERROR = no
LIB = icu

# As on macOS, libicucore exports the plain names (u_strlen, not
# u_strlen_78): macOS programs import them so. Whatever includes ICU's
# headers must define U_DISABLE_RENAMING=1 too (CoreFoundation does). Only
# the target build: the host tools in native/ keep their renamed symbols,
# away from the host's own ICU.
ICU_DEFS = -DU_DISABLE_RENAMING=1
CFLAGS =--sysroot=${MINKAR_SDKROOT} -fuse-ld=${LD} \
	-mmacos-version-min=${MACOS_VERSION_MIN} ${ICU_DEFS}
CXXFLAGS =-std=gnu++17 --sysroot=${MINKAR_SDKROOT} -fuse-ld=${LD} \
	-mmacos-version-min=${MACOS_VERSION_MIN} ${ICU_DEFS}
LDFLAGS =--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD} -licudata -lc++
# clang links the compiler-rt builtins itself (lib/darwin/libclang_rt.osx.a
# in its resource directory); naming them here too duplicates them under
# -all_load.

EXTRA_FLAGS = --prefix=/usr --localstatedir=/var --sysconfdir=/etc

.if "${.MAKE.OS}" != "Darwin"
CFG_GUESS != sh ${.CURDIR}/source/config.guess
EXTRA_FLAGS += --with-cross-build=${.OBJDIR}/native \
	--host=${CpuArch}-apple-darwin --build=${CFG_GUESS} \
	--disable-samples --disable-tests --disable-extras
.endif

all: libicucore.A.dylib

# gmake gets as many jobs as this make (MAKEFLAGS is cleared: bmake's
# flags mean nothing to it).
ICU_JOBS =	-j${.MAKE.JOBS:U1}

lib${LIB}.a: ${.CURDIR}/Makefile
.if "${.MAKE.OS}" != "Darwin"
	mkdir -p ${.OBJDIR}/native
	# Only the build tools are used from the host build.
	cd ${.OBJDIR}/native; ${.CURDIR}/source/configure \
	  --disable-samples --disable-tests --disable-extras \
	  && MAKEFLAGS="" ${GMAKE} ${ICU_JOBS}
.endif
	mkdir -p ${.OBJDIR}/target
	cd ${.OBJDIR}/target; ICU_CROSS_BUILD=1 SDKROOT=${MINKAR_SDKROOT} \
	  CFLAGS="${CFLAGS}" CXXFLAGS="${CXXFLAGS}" LDFLAGS="${LDFLAGS:N-licudata}" \
	  CC=${CC} CXX=${CXX} LD=${LD} AR=${AR} STRIP=${STRIP} MAKEFLAGS="" \
	  ${.CURDIR}/source/runConfigureICU MacOSX ${EXTRA_FLAGS}
	MAKEFLAGS="" ${GMAKE} ${ICU_JOBS} -C ${.OBJDIR}/target

libicucore.A.dylib: lib${LIB}.a
	${TOOLS}/install_name_tool -id /usr/lib/libicudata.dylib \
	  ${.OBJDIR}/target/lib/libicudata.dylib
	${CC} -v -o ${.TARGET} -shared -dylib -Wl,-all_load \
	  -Wl,-install_name,/usr/lib/${.TARGET} \
	  -Wl,-current_version,78.3 -Wl,-compatibility_version,1 \
	  $$(find ${.OBJDIR}/target/common ${.OBJDIR}/target/i18n \
	  ${.OBJDIR}/target/io -name '*.o') -L${.OBJDIR}/target/lib ${LDFLAGS}
.for dir in ${MINKAR_SDKROOT} ${SYSROOT_DIR}
	mkdir -p ${dir}/usr/lib ${dir}/usr/include
	cp -f ${.OBJDIR}/${.TARGET} ${dir}/usr/lib/
	ln -sf ${.TARGET} ${dir}/usr/lib/libicucore.dylib
	cp -fd ${.OBJDIR}/target/lib/libicudata* ${dir}/usr/lib/
	cp -Rf ${.CURDIR}/source/i18n/unicode ${.CURDIR}/source/i18n/*.h \
	  ${dir}/usr/include
.endfor
