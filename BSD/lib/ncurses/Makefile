
all: ${.OBJDIR}/.installed

# gmake gets as many jobs as this make (MAKEFLAGS is cleared: bmake's
# flags mean nothing to it).
NCURSES_JOBS =	-j${.MAKE.JOBS:U1}

# The terminfo database is compiled on the build host. Use a tic built from
# these sources without mixed-case names, so /usr/share/terminfo uses the
# hex directory names (61/, 41/, ...) that case-insensitive volumes need,
# as on macOS. The host's own tic would use letters (a/, A/, ...).
HOST_TIC = ${.OBJDIR}/host-tic/progs/tic

${HOST_TIC}: ${.CURDIR}/Makefile
	rm -rf ${.OBJDIR}/host-tic && mkdir -p ${.OBJDIR}/host-tic
	cd ${.OBJDIR}/host-tic && CC=${HOST_CC} CFLAGS= LDFLAGS= \
	    ${.CURDIR}/configure --without-ada --without-tests \
	    --without-cxx --without-cxx-binding --without-manpages \
	    --without-shared --with-normal --without-debug \
	    --enable-ext-colors --disable-mixed-case
	cd ${.OBJDIR}/host-tic && MAKEFLAGS="" ${TOOLS}/gmake ${NCURSES_JOBS} -C include && \
	    MAKEFLAGS="" ${TOOLS}/gmake ${NCURSES_JOBS} -C ncurses && \
	    MAKEFLAGS="" ${TOOLS}/gmake ${NCURSES_JOBS} -C progs tic

# The host tic is only needed to install: both builds run at the same time.
${.OBJDIR}/lib/libncurses.6.dylib: ${.CURDIR}/Makefile
	CC=${CC} CXX=${CXX} \
	CFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	CXXFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN}" \
	LDFLAGS="--sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} -fuse-ld=${LD}" \
	${.CURDIR}/configure --prefix=/usr \
        --sysconfdir=/private/etc \
        --localstatedir=/private/var \
        --target=${CpuArch}-apple-darwin19 \
        --host=${CpuArch}-apple-darwin19 \
	--with-strip-program=${TOOLS}/strip \
        --without-ada --without-tests --enable-pc-files \
        --with-shared --with-normal --without-debug \
        --enable-rpath --without-dlsym --disable-lib-suffixes \
        --enable-termcap --enable-symlinks --enable-ext-colors \
        --enable-rgb-color --with-pthread --enable-pthreads-eintr \
        --enable-reentrant \
        --disable-mixed-case
	MAKEFLAGS="" ${TOOLS}/gmake ${NCURSES_JOBS}

${.OBJDIR}/.installed: ${.OBJDIR}/lib/libncurses.6.dylib ${HOST_TIC}
	# The SDK takes the libraries and headers; the full install (programs,
	# terminfo database, compiled with tic) goes to the root only.
	MAKEFLAGS="" ${TOOLS}/gmake install.libs install.includes DESTDIR=${MINKAR_SDKROOT}
	MAKEFLAGS="" tic_path=${HOST_TIC} ${TOOLS}/gmake install DESTDIR=${SYSROOT_DIR}
.for prog in clear infocmp tabs tic toe tput tset
	ln -sf ${CpuArch}-apple-darwin19-${prog} ${SYSROOT_DIR}/usr/bin/${prog}
.endfor
	touch ${.TARGET}
