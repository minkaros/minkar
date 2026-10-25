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

install-stubs: .PHONY
.for s in ${STUBS}
	@if [ ! -e ${RAVYN_SDKROOT}/${s:${REAL}} ]; then \
		mkdir -p ${RAVYN_SDKROOT}/${s:H}; \
		echo "bootstrap stub: ${s}"; \
		cp -f ${.CURDIR}/stubs/${s} ${RAVYN_SDKROOT}/${s}; \
	fi
.endfor

all: .PHONY
.for s in ${STUBS}
	@if [ -e ${RAVYN_SDKROOT}/${s:${REAL}} ]; then \
		rm -f ${RAVYN_SDKROOT}/${s}; \
	else \
		[ ! -e ${RAVYN_SDKROOT}/${s} ] || \
		    echo "warning: ${s:${REAL}} was not built, its stub stays"; \
	fi
.endfor

.include <bsd.prog.mk>
