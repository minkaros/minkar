# Atlas's cursors (gen-cursors.py: drawn from their geometry, Minkar's
# own), in /System/Library/Cursors: NAME.png and NAME@2x.png, their hot
# spots in Cursors.plist, and the window server's arrow, arrowCursor.bgra.
SUBDIR =

DIR =		${SYSROOT_DIR}/System/Library/Cursors

all: install-cursors

cursors.stamp: ${.CURDIR}/gen-cursors.py
	rm -rf ${.OBJDIR}/cursors
	python3 -I ${.CURDIR}/gen-cursors.py ${.OBJDIR}/cursors
	touch ${.TARGET}

# Exactly what was made: a cursor no longer drawn goes.
install-cursors: cursors.stamp .PHONY
	mkdir -p ${DIR}
	rsync -rc --delete ${.OBJDIR}/cursors/ ${DIR}/

.include <bsd.prog.mk>
