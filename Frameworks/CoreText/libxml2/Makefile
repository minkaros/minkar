MK_WERROR = no
SUBDIR =

# We have to trick shitty cmake into not producing broken ELF shared
# objects named *.dylib.

all: libintl
	CC=${CC} CXX=${CXX} LD=${CC} AR=${AR} RANLIB=${RANLIB} LIBTOOL=${LIBTOOL} \
	LDFLAGS="-fuse-ld=${LD} --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	cmake -G "Unix Makefiles" \
		-DCMAKE_C_COMPILER="${CC}" \
		-DCMAKE_CXX_COMPILER="${CXX}" \
		-DCMAKE_AR="${AR}" \
		-DCMAKE_RANLIB="${RANLIB}" \
		-DCMAKE_C_FLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
		-DCMAKE_SYSTEM_NAME="Darwin" \
		-DCMAKE_SHARED_LIBRARY_VERSION_SCRIPT_FLAG="" \
		-DCMAKE_SHARED_LIBRARY_SONAME_FLAG="" \
		-DCMAKE_SHARED_LINKER_FLAGS="${LDFLAGS}" \
		-DBUILD_SHARED_LIBS=OFF \
		-DPKG_CONFIG_EXECUTABLE=/usr/bin/true \
		-DCMAKE_INSTALL_PREFIX=/usr \
		-DLIBXML2_WITH_SAX1=ON \
		-DLIBXML2_WITH_PYTHON=OFF \
		-DIconv_LIBRARY=${.OBJDIR:H}/libiconv/lib/.libs/libiconv.2.dylib \
		-DIconv_INCLUDE_DIR=${.OBJDIR:H}/libiconv/include \
		-DLIBLZMA_LIBRARY=${.OBJDIR:H}/xz/src/liblzma/.libs/liblzma.5.dylib \
		-DLIBLZMA_INCLUDE_DIR=${.CURDIR:H}/xz/src/liblzma/api \
		-DLIBLZMA_HAS_AUTO_DECODER=ON -DLIBLZMA_HAS_EASY_ENCODER=ON \
		-DLIBLZMA_HAS_LZMA_PRESET=ON \
		-DZLIB_LIBRARY=${ROOT_BINARY_DIR}${ROOT_SOURCE_DIR}/Libraries/zlib/libz.a \
		-DZLIB_INCLUDE_DIR=${ROOT_SOURCE_DIR}/Libraries/zlib \
		-S ${.CURDIR} -B ${.OBJDIR}
	${MAKE} -C ${.OBJDIR}
	${MAKE} install DESTDIR=${SYSROOT_DIR}
	${MAKE} install DESTDIR=${MINKAR_SDKROOT}



# gettext-tiny builds where its Makefile is (and leaves an a.out from a
# compiler check): build a copy of its sources in the object directory,
# so the source tree stays clean.
GT_OBJDIR =	${.OBJDIR}/gettext-tiny

libintl: ${.CURDIR:H}/gettext-tiny/Makefile
	mkdir -p ${GT_OBJDIR}
	cd ${.CURDIR:H}/gettext-tiny && cp -Rp Makefile install.sh data include \
	    libintl m4 src ${GT_OBJDIR}/
	rm -f ${GT_OBJDIR}/src/*.o ${GT_OBJDIR}/libintl/*.o
	cd ${GT_OBJDIR} && \
	CC=${CC} CXX=${CXX} LD=${CC} AR=${AR} RANLIB=${RANLIB} \
	NM=${NM} LIBTOOL=${LIBTOOL} \
	LDFLAGS="-fuse-ld=${LD} --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -L${.OBJDIR:H}/libiconv/lib/.libs/" \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -I${.OBJDIR:H}/libiconv/include" \
	MAKEFLAGS="" ${GMAKE} LIBINTL=1 -C ${GT_OBJDIR}
	cp -fv ${GT_OBJDIR}/libintl.a ${MINKAR_SDKROOT}/usr/lib
	cp -fv ${GT_OBJDIR}/msgfmt ${GT_OBJDIR}/msgmerge \
		${SYSROOT_DIR}/usr/bin/


.include <rvn.framework.mk>
.include <bsd.subdir.mk>
