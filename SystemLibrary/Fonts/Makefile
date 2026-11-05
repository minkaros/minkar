# System fonts: /System/Library/Fonts, where fontconfig looks first.
#
# SHORTCUT(Phase 5): AppKit's interface font is Inter (SIL Open Font
# License 1.1, Inter-LICENSE.txt), installed as a provisional system font
# so AppKit can draw text (decided 2026-10-31), until Atlas has its own
# font. Only the four faces AppKit names are installed; the other fonts
# under TTF/ are not; taken in WS0.
#
# The fixed-pitch font (NSFont's userFixedPitchFont, Text Editor's plain text)
# is Agave (OFL 1.1, chosen 2026-11-05), fetched at the version
# MANIFEST.agave pins and installed unchanged, with its license as
# Agave-LICENSE.txt.

SUBDIR =

FONTS =		TTF/Inter-Regular.ttf TTF/Inter-Bold.ttf TTF/Inter-Italic.ttf \
		TTF/Inter-BoldItalic.ttf Inter-LICENSE.txt
DIR =		${SYSROOT_DIR}/System/Library/Fonts
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
AGAVE =		Agave-Regular.ttf Agave-Bold.ttf

all: install-fonts

install-fonts: .PHONY
	mkdir -p ${DIR}
.for f in ${FONTS}
	install -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${DIR}/
.endfor
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.agave ${CACHE}) && \
	for f in ${AGAVE}; do \
		install -o 0 -g 0 -m 0644 $$src/dist/$$f ${DIR}/ || exit 1; \
	done && \
	install -o 0 -g 0 -m 0644 $$src/LICENSE ${DIR}/Agave-LICENSE.txt

.include <bsd.prog.mk>
