# System fonts: /System/Library/Fonts, where fontconfig looks first.
#
# SHORTCUT(Phase 5): AppKit's interface font is Inter (SIL Open Font
# License 1.1, Inter-LICENSE.txt), installed as a provisional system font
# so AppKit can draw text (decided 2026-10-31), until Gienah has its own
# font. Only the four faces AppKit names are installed; the other fonts
# under TTF/ are not; taken in WS0.

SUBDIR =

FONTS =		TTF/Inter-Regular.ttf TTF/Inter-Bold.ttf TTF/Inter-Italic.ttf \
		TTF/Inter-BoldItalic.ttf Inter-LICENSE.txt
DIR =		${SYSROOT_DIR}/System/Library/Fonts

all: install-fonts

install-fonts: .PHONY
	mkdir -p ${DIR}
.for f in ${FONTS}
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${DIR}/
.endfor

.include <bsd.prog.mk>
