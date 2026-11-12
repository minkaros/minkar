TARGETS = ${TOOLS}/xcbuild ${TOOLS}/xcodebuild ${TOOLS}/xcrun

.if "${.MAKE.OS}" == "Darwin"
RPATH = -rpath,@executable_path
.else
RPATH = -R$$ORIGIN
.endif

all: install-xcbuild

# Built again only when what it is built from changes, by content: the git
# tree of these sources (and any uncommitted change in them) and this
# Makefile. Outside git it is always built (cmake's build tree makes that
# quick when nothing changed).
STAMP =		${TOOLCHAIN}/usr/share/xcbuild/.minkar-built
GIT =		git -c safe.directory=* -C ${.CURDIR}
SUM_CMD =	{ ${GIT} rev-parse HEAD:./ && ${GIT} status --porcelain -- . && \
		    cat ${.CURDIR}/Makefile; } 2>/dev/null | cksum

install-xcbuild: .PHONY
	@if [ -x ${TOOLS}/xcbuild ] && [ -f ${STAMP} ] && \
	    ${GIT} rev-parse -q --verify HEAD:./ > /dev/null 2>&1 && \
	    [ "$$(${SUM_CMD})" = "$$(cat ${STAMP})" ]; then \
		echo "xcbuild: the toolchain's xcbuild is current"; \
	else \
		${MAKE} -C ${.CURDIR} install-xcbuild-now; \
	fi

install-xcbuild-now: .PHONY
	cmake -S ${.CURDIR} -B ${.OBJDIR} -Wno-dev \
	-DCMAKE_C_COMPILER=${HOST_CC} \
	-DCMAKE_CXX_COMPILER=${HOST_CXX} \
	-DROOT_SOURCE_DIR=${ROOT_SOURCE_DIR} \
	-DROOT_BINARY_DIR=${ROOT_BINARY_DIR} \
	'-DCMAKE_BUILD_RPATH=${RPATH}/../lib' \
	'-DCMAKE_INSTALL_RPATH=${RPATH}/../lib' \
	-DCMAKE_INSTALL_PREFIX=${TOOLCHAIN}
	CC=${HOST_CC} CXX=${HOST_CXX} make -C${.OBJDIR}
	CC=${HOST_CC} CXX=${HOST_CXX} make -C${.OBJDIR} install
	mkdir -p ${TOOLCHAIN}/usr/share/xcbuild
	${SUM_CMD} > ${STAMP}
