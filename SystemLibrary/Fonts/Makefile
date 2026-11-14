# System fonts: /System/Library/Fonts, where fontconfig looks first.
#
# SHORTCUT(Phase 5): AppKit's interface font is Inter (SIL Open Font
# License 1.1, Inter-LICENSE.txt), installed as a provisional system font
# so AppKit can draw text (decided 2026-10-31), until Atlas has its own
# font. All its weights (Thin to Black, each with its italic) are
# installed, for the font panel's typefaces; the other fonts under TTF/
# are not; taken in WS0.
#
# The fixed-pitch font (NSFont's userFixedPitchFont, Text Editor's plain text)
# is Agave (OFL 1.1, chosen 2026-11-05), fetched at the version
# MANIFEST.agave pins and installed unchanged, with its license as
# Agave-LICENSE.txt.
#
# Free faces for the fonts applications ask for by name and the system
# does not have (approved 2026-11-11), each fetched at the version its
# MANIFEST pins and installed unchanged with its license: Liberation Sans,
# Serif and Mono (OFL 1.1), DejaVu Sans and Sans Mono (Bitstream Vera),
# Gelasio (OFL 1.1) and TeX Gyre Pagella (GUST). The names asked for lead
# to them through the table in AppKit's O2Font_FT.m; users never see the
# names asked for.
#
# More free families the user chose (2026-11-13), each at the commit its
# MANIFEST pins, installed unchanged with its license (all SIL Open Font
# License 1.1): Victor Mono and Monaspace (Argon, Krypton, Neon, Radon,
# Xenon; its static fonts, normal width), fixed-pitch; Libre Caslon Condensed; and ANRT's Gotico-Antiqua
# revivals of fifteenth-century types.
#
# Writing systems Inter and the others lack (2026-11-13, OFL 1.1, unchanged
# with their licenses): Noto Sans CJK (Chinese, Japanese and Korean, the
# regular and bold collections) and Noto Sans Arabic.

SUBDIR =

FONTS =		TTF/Inter-Thin.ttf TTF/Inter-ThinItalic.ttf \
		TTF/Inter-ExtraLight.ttf TTF/Inter-ExtraLightItalic.ttf \
		TTF/Inter-Light.ttf TTF/Inter-LightItalic.ttf TTF/Inter-Regular.ttf \
		TTF/Inter-Italic.ttf TTF/Inter-Medium.ttf TTF/Inter-MediumItalic.ttf \
		TTF/Inter-SemiBold.ttf TTF/Inter-SemiBoldItalic.ttf TTF/Inter-Bold.ttf \
		TTF/Inter-BoldItalic.ttf TTF/Inter-ExtraBold.ttf \
		TTF/Inter-ExtraBoldItalic.ttf TTF/Inter-Black.ttf \
		TTF/Inter-BlackItalic.ttf Inter-LICENSE.txt
DIR =		${SYSROOT_DIR}/System/Library/Fonts
FETCH =		sh ${ROOT_SOURCE_DIR}/BSD/lib/libdrm/fetch.sh
CACHE ?=	${ROOT_BINARY_DIR:U${.OBJDIR}}/src-cache
AGAVE =		Agave-Regular.ttf Agave-Bold.ttf
LIBERATION =	LiberationSans-Regular.ttf LiberationSans-Bold.ttf LiberationSans-Italic.ttf \
		LiberationSans-BoldItalic.ttf LiberationSerif-Regular.ttf LiberationSerif-Bold.ttf \
		LiberationSerif-Italic.ttf LiberationSerif-BoldItalic.ttf LiberationMono-Regular.ttf \
		LiberationMono-Bold.ttf LiberationMono-Italic.ttf LiberationMono-BoldItalic.ttf
DEJAVU =	DejaVuSans.ttf DejaVuSans-Bold.ttf DejaVuSans-Oblique.ttf DejaVuSans-BoldOblique.ttf \
		DejaVuSansMono.ttf DejaVuSansMono-Bold.ttf DejaVuSansMono-Oblique.ttf \
		DejaVuSansMono-BoldOblique.ttf
GELASIO =	'Gelasio[wght].ttf' 'Gelasio-Italic[wght].ttf'
PAGELLA =	texgyrepagella-regular.otf texgyrepagella-bold.otf texgyrepagella-italic.otf \
		texgyrepagella-bolditalic.otf

all: install-fonts

install-fonts: .PHONY
	mkdir -p ${DIR}
.for f in ${FONTS}
	install -C -o 0 -g 0 -m 0644 ${.CURDIR}/${f} ${DIR}/
.endfor
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.agave ${CACHE}) && \
	for f in ${AGAVE}; do \
		install -C -o 0 -g 0 -m 0644 $$src/dist/$$f ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/LICENSE ${DIR}/Agave-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.liberation ${CACHE}) && \
	for f in ${LIBERATION}; do \
		install -C -o 0 -g 0 -m 0644 $$src/$$f ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/LICENSE ${DIR}/Liberation-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.dejavu ${CACHE}) && \
	for f in ${DEJAVU}; do \
		install -C -o 0 -g 0 -m 0644 $$src/ttf/$$f ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/LICENSE ${DIR}/DejaVu-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.gelasio ${CACHE})/ofl/gelasio && \
	for f in ${GELASIO}; do \
		install -C -o 0 -g 0 -m 0644 "$$src/$$f" ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/OFL.txt ${DIR}/Gelasio-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.texgyre ${CACHE}) && \
	for f in ${PAGELLA}; do \
		install -C -o 0 -g 0 -m 0644 $$src/fonts/opentype/public/tex-gyre/$$f ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/doc/fonts/tex-gyre/GUST-FONT-LICENSE.txt ${DIR}/TeXGyre-LICENSE.txt

	src=$$(${FETCH} ${.CURDIR}/MANIFEST.victormono ${CACHE}) && \
	python3 -I -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
	    $$src/public/VictorMonoVariable.zip $$src/unpacked && \
	for f in VictorMonoVariable.ttf VictorMonoVariableItalic.ttf VictorMonoVariableOblique.ttf; do \
		install -C -o 0 -g 0 -m 0644 $$src/unpacked/$$f ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/LICENSE ${DIR}/VictorMono-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.monaspace ${CACHE}) && \
	for f in "$$src/fonts/Static Fonts"/Monaspace\ */*.otf; do \
		case "$$f" in *Wide*) continue ;; esac; \
		install -C -o 0 -g 0 -m 0644 "$$f" ${DIR}/ || exit 1; \
	done && \
	rm -f "${DIR}"/Monaspace\ *\ Var.ttf && \
	install -C -o 0 -g 0 -m 0644 $$src/LICENSE ${DIR}/Monaspace-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.caslon ${CACHE}) && \
	for f in 'LibreCaslonCondensed[wght].ttf' 'LibreCaslonCondensed-Italic[wght].ttf'; do \
		install -C -o 0 -g 0 -m 0644 "$$src/fonts/variable/$$f" ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/OFL.txt ${DIR}/LibreCaslonCondensed-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.gotico ${CACHE}) && \
	for f in "$$src"/fonts/*.otf; do \
		install -C -o 0 -g 0 -m 0644 "$$f" ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/LICENSE.txt ${DIR}/GoticoAntiqua-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.notocjk ${CACHE}) && \
	for f in NotoSansCJK-Regular.ttc NotoSansCJK-Bold.ttc; do \
		install -C -o 0 -g 0 -m 0644 $$src/Sans/OTC/$$f ${DIR}/ || exit 1; \
	done && \
	install -C -o 0 -g 0 -m 0644 $$src/Sans/LICENSE ${DIR}/NotoSansCJK-LICENSE.txt
	src=$$(${FETCH} ${.CURDIR}/MANIFEST.notoarabic ${CACHE})/ofl/notosansarabic && \
	install -C -o 0 -g 0 -m 0644 "$$src/NotoSansArabic[wdth,wght].ttf" ${DIR}/ && \
	install -C -o 0 -g 0 -m 0644 $$src/OFL.txt ${DIR}/NotoSansArabic-LICENSE.txt

.include <bsd.prog.mk>
