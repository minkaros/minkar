# Use the generated files shipped in the tree: a git checkout leaves
# Makefile.am & co. as new as Makefile.in, and make would try to run
# automake/autoconf again.
NO_AUTOTOOLS =	ACLOCAL=: AUTOCONF=: AUTOMAKE=: AUTOHEADER=:

all: ${.OBJDIR}/Makefile build

${.OBJDIR}/Makefile: ${.CURDIR}/configure
	CC=${CC} CXX=${CXX} LD=${CC} AR=${AR} RANLIB=${RANLIB} LIBTOOL=${LIBTOOL} \
	LDFLAGS="-fuse-ld=${LD} --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	${.CURDIR}/configure --prefix=/usr --sysconfdir=/etc \
        --localstatedir=/var --host=${CpuArch}-apple-darwin \
        --disable-doc --disable-nls --disable-rpath  \
        --with-sysroot=${MINKAR_SDKROOT}

build: ${.OBJDIR}/Makefile
	MAKEFLAGS="" ${GMAKE} ${NO_AUTOTOOLS} -C ${.OBJDIR}
	# Into CoreText's bundle once it exists: Frameworks also builds xz
	# before Foundation (for libcompression), and CoreText builds it again.
	if [ -d ${.OBJDIR:H}/CoreText.framework/Resources ]; then \
		rsync -cL ${.OBJDIR}/src/liblzma/.libs/liblzma.5.dylib \
		    ${.OBJDIR:H}/CoreText.framework/Resources/; \
	fi
.for dir in ${SYSROOT_DIR}/usr/lib ${MINKAR_SDKROOT}/usr/lib
	mkdir -p ${dir}
	rsync -cL ${.OBJDIR}/src/liblzma/.libs/liblzma.5.dylib ${dir}/
	ln -sf liblzma.5.dylib ${dir}/liblzma.dylib
.endfor
