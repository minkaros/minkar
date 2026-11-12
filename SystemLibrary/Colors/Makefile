# System color lists: /System/Library/Colors, where NSColorList looks
# first. AppKit's catalog colors (+[NSColor colorWithCatalogName:...],
# e.g. menuItemTextColor) come from System.clr; without it they are nil.

SUBDIR =

LISTS =		Basic.clr System.clr Web.clr
DIR =		${SYSROOT_DIR}/System/Library/Colors

all: install-colors

install-colors: .PHONY
	mkdir -p ${DIR}
.for f in ${LISTS}
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${DIR}/
.endfor

.include <bsd.prog.mk>
