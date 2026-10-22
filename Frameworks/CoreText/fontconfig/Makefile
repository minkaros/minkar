# fontconfig, built as a static library and linked into CoreText.
# GNU gperf is not used: src/fcobjshash.h comes from our own BSD-2
# generator, ../fcobjshash-gen.py.

ONYX2D_OBJ = ${.OBJDIR:H:H}/Onyx2D
SDK_FLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}

FREETYPE_CFLAGS = -I${ONYX2D_OBJ}/freetype/include \
	-I${ROOT_SOURCE_DIR}/Frameworks/Onyx2D/freetype/include
# Use -L/-l: libtool would embed .a paths inside libfontconfig.a
FREETYPE_LIBS = -L${ONYX2D_OBJ}/freetype -lfreetype \
	-L${ONYX2D_OBJ}/png -lpng16 -lz
LIBXML2_CFLAGS = -I${RAVYN_SDKROOT}/usr/include/libxml2 \
	-I${.OBJDIR:H}/libiconv/include
LIBXML2_LIBS = -lxml2 -L${.OBJDIR:H}/libiconv/lib/.libs -liconv \
	-L${.OBJDIR:H}/xz/src/liblzma/.libs -llzma -lz

all: ${.OBJDIR}/Makefile build

${.OBJDIR}/Makefile: ${.CURDIR}/configure
	CC=${CC} CXX=${CXX} LD=${CC} AR=${AR} RANLIB=${RANLIB} LIBTOOL=${LIBTOOL} \
	LDFLAGS="-fuse-ld=${LD} ${SDK_FLAGS}" \
	CFLAGS="${SDK_FLAGS}" \
	FREETYPE_CFLAGS="${FREETYPE_CFLAGS}" FREETYPE_LIBS="${FREETYPE_LIBS}" \
	LIBXML2_CFLAGS="${LIBXML2_CFLAGS}" LIBXML2_LIBS="${LIBXML2_LIBS}" \
	GPERF="python3 -I ${.CURDIR:H}/fcobjshash-gen.py" \
	${.CURDIR}/configure --prefix=/usr --sysconfdir=/etc \
	--localstatedir=/var --host=${CpuArch}-apple-darwin \
	--with-sysroot=${RAVYN_SDKROOT} \
	--enable-static --disable-shared --with-pic \
	--enable-libxml2 --disable-docs --disable-nls --disable-cache-build \
	--with-templatedir=/System/Library/Frameworks/CoreText.framework/Resources \
	--with-xmldir=/System/Library/Frameworks/CoreText.framework/Resources \
	--with-default-fonts=/System/Library/Fonts,/Library/Fonts

build: ${.OBJDIR}/Makefile
	MAKEFLAGS="" ${GMAKE} -C ${.OBJDIR}/src all
	mkdir -p ${SYSROOT_DIR}/etc/fonts/conf.d
	cp -f ${.CURDIR}/fonts.conf ${SYSROOT_DIR}/etc/fonts/

clean:
	rm -rf ${.OBJDIR}/*
