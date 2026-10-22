all: ${.OBJDIR}/Makefile build

${.OBJDIR}/Makefile: ${.CURDIR}/configure
	CC=${CC} CXX=${CXX} LD=${CC} AR=${AR} LIBTOOL=${LIBTOOL} RANLIB=${RANLIB} \
	LDFLAGS="-fuse-ld=${LD} --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	CFLAGS="--sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	${.CURDIR}/configure --prefix=/usr --sysconfdir=/etc \
        --localstatedir=/var --host=${CpuArch}-apple-darwin \
        --disable-doc --disable-nls --disable-rpath --disable-largefile \
        --with-sysroot=${RAVYN_SDKROOT}

build: ${.OBJDIR}/Makefile
	MAKEFLAGS="" ${GMAKE} -C ${.OBJDIR}
	cp -f ${.OBJDIR}/lib/.libs/libiconv.2.dylib \
		${.OBJDIR}/lib/libcharset.1.dylib \
		${.OBJDIR:H}/CoreText.framework/Resources/
.for dir in ${SYSROOT_DIR}/usr/lib ${RAVYN_SDKROOT}/usr/lib
	mkdir -p ${dir}
	cp -f ${.OBJDIR}/lib/.libs/libiconv.2.dylib ${.OBJDIR}/lib/libcharset.1.dylib ${dir}/
	ln -sf libiconv.2.dylib ${dir}/libiconv.dylib
.endfor
