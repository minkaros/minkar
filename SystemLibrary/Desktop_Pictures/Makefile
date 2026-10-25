# Minkar desktop pictures, installed in /System/Library/Desktop Pictures:
# the four original designs (PNG) and their high-resolution versions.
PICSDIR=	/System/Library/Desktop Pictures

all: install-pictures
install-pictures: .PHONY
	mkdir -p "${SYSROOT_DIR}${PICSDIR}"
	cd ${.CURDIR} && find . -type f \( -name '*.png' -o -name '*.jpg' \) | \
	    while read -r pic; do \
		mkdir -p "${SYSROOT_DIR}${PICSDIR}/$$(dirname "$$pic")"; \
		install -m 0644 "$$pic" "${SYSROOT_DIR}${PICSDIR}/$$pic"; \
	    done

.include <bsd.prog.mk>
