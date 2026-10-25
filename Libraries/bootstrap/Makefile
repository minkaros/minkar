# Link stubs for a clean build (see regen-stubs.sh).
#
# install-stubs (run first, by ../Makefile) puts a stub in the SDK for
# each base library that is not built yet. all (run last, as a SUBDIR)
# removes the stubs whose library now exists, so the SDK ends up with
# the real libraries only.

SUBDIR =
STUBS !=	cd ${.CURDIR}/stubs && find . -name '*.tbd' | sed 's|^\./||' | sort

# Turns a stub path into the path of the library it stands for.
REAL =	S,libSystem.tbd,libSystem.B.dylib,:S,libc++.tbd,libc++.1.dylib,:S,libobjc.tbd,libobjc.A.dylib,:S,.tbd,.dylib,

# libSystem's other names (-lc, -lm, -lpthread...), as in the macOS SDK.
# Libsystem makes them as links to libSystem.dylib; until then, links to
# its stub. Without -lc, compiler-rt's configure checks cannot link a
# program and it leaves out the profile runtime the kernel links.
ALIASES =	c dbm dl info m poll proc pthread rpcsvc

install-stubs: .PHONY
.for a in ${ALIASES}
	@if [ ! -e ${RAVYN_SDKROOT}/usr/lib/lib${a}.dylib ]; then \
		mkdir -p ${RAVYN_SDKROOT}/usr/lib; \
		ln -sf libSystem.tbd ${RAVYN_SDKROOT}/usr/lib/lib${a}.tbd; \
	fi
.endfor
	# libc++abi is inside libc++.1.dylib (the SDK also gets libc++abi.a).
	@if [ ! -e ${RAVYN_SDKROOT}/usr/lib/libc++abi.a ]; then \
		ln -sf libc++.tbd ${RAVYN_SDKROOT}/usr/lib/libc++abi.tbd; \
	fi
.for s in ${STUBS}
	@if [ ! -e ${RAVYN_SDKROOT}/${s:${REAL}} ]; then \
		mkdir -p ${RAVYN_SDKROOT}/${s:H}; \
		echo "bootstrap stub: ${s}"; \
		cp -f ${.CURDIR}/stubs/${s} ${RAVYN_SDKROOT}/${s}; \
	fi
.endfor

all: .PHONY
.for a in ${ALIASES}
	@if [ -e ${RAVYN_SDKROOT}/usr/lib/libSystem.B.dylib ]; then \
		rm -f ${RAVYN_SDKROOT}/usr/lib/lib${a}.tbd; \
	fi
.endfor
	@if [ -e ${RAVYN_SDKROOT}/usr/lib/libc++.1.dylib ]; then \
		rm -f ${RAVYN_SDKROOT}/usr/lib/libc++abi.tbd; \
	fi
.for s in ${STUBS}
	@if [ -e ${RAVYN_SDKROOT}/${s:${REAL}} ]; then \
		rm -f ${RAVYN_SDKROOT}/${s}; \
	else \
		[ ! -e ${RAVYN_SDKROOT}/${s} ] || \
		    echo "warning: ${s:${REAL}} was not built, its stub stays"; \
	fi
.endfor

.include <bsd.prog.mk>
