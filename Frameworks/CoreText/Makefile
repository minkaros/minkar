WARNS = 1
MK_WERROR = no

FRAMEWORK = CoreText
FMWK_COMPAT_VERSION = 1.0.0

SRCS = CTFont.m CTFontDescriptor.m CTFontMatching.c CTFontManager.c KTFont.m CTLine.m CTParagraphStyle.m CTStringAttributes.m
HEADERS = CoreText.h CoreTextExport.h CTFont.h CTFontDescriptor.h CTFontManager.h KTFont.h CTLine.h CTParagraphStyle.h CTStringAttributes.h SFNTLayoutTypes.h
RESOURCES = Info.plist
LDFLAGS = -framework Onyx2D -framework CoreGraphics -framework Foundation \
	-framework CoreFoundation -L${.OBJDIR}/libxml2 -lxml2 -lintl -lobjc \
	-liconv -llzma -lz \
	-Wl,-force_load,${.OBJDIR}/fontconfig/src/.libs/libfontconfig.a

SUBDIR = xz libxml2 fontconfig

${FRAMEWORK}: xz libxml2 fontconfig
libxml2: xz
fontconfig: libxml2

clean:
	rm -f ${.OBJDIR}/*.o ${.OBJDIR}/*.pico

CFLAGS = -I${.CURDIR:H}  -I${.CURDIR}/fontconfig/fontconfig -I${.CURDIR}/fontconfig

fontresources:
	mkdir -p ${FRAMEWORK_DIR}/Versions/A/Resources ${SYSROOT_DIR}/etc/fonts/conf.d
	rsync -cL ${.CURDIR}/fontconfig/conf.d/*.conf ${FRAMEWORK_DIR}/Versions/A/Resources/
.for f in  \
	10-hinting-medium.conf \
	10-scale-bitmap-fonts.conf \
	10-sub-pixel-bgr.conf \
	10-yes-antialias.conf \
	11-lcdfilter-default.conf \
	20-unhint-small-vera.conf \
	25-unhint-nonlatin.conf \
	30-metric-aliases.conf \
	35-lang-normalize.conf \
	40-nonlatin.conf \
	45-generic.conf \
	45-latin.conf \
	48-spacing.conf \
	49-sansserif.conf \
	50-user.conf \
	51-local.conf \
	60-generic.conf \
	60-latin.conf \
	65-fonts-persian.conf \
	65-khmer.conf \
	65-nonlatin.conf \
	69-unifont.conf \
	70-yes-bitmaps.conf \
	80-delicious.conf \
	90-synthetic.conf
	ln -sf /System/Library/Frameworks/CoreText.framework/Resources/${f} \
		${SYSROOT_DIR}/etc/fonts/conf.d/
	mkdir -p ${FRAMEWORK_DIR}/Versions/A/Resources/conf.d
	ln -sf ../${f} ${FRAMEWORK_DIR}/Versions/A/Resources/conf.d/
.endfor
	# CoreText's own configuration (CTFontManager.c): the rules from its
	# resources, not /etc/fonts, which a sandboxed process may not read;
	# its cache is the user's (added in code).
	sed -e 's|<include ignore_missing="yes">/etc/fonts/conf.d</include>|<include ignore_missing="yes">/System/Library/Frameworks/CoreText.framework/Resources/conf.d</include>|' \
	    -e '/<cachedir/d' -e '/the following element will be removed in the future/d' \
	    ${.CURDIR}/fontconfig/fonts.conf > ${FRAMEWORK_DIR}/Versions/A/Resources/fonts.conf.new
	grep -q 'CoreText.framework/Resources/conf.d' ${FRAMEWORK_DIR}/Versions/A/Resources/fonts.conf.new
	mv ${FRAMEWORK_DIR}/Versions/A/Resources/fonts.conf.new ${FRAMEWORK_DIR}/Versions/A/Resources/fonts.conf

installresources: std_install_hook fontresources

fmwk-install-hook: fontresources
.for dir in ${MINKAR_SDKROOT}/System/Library/Frameworks/ ${SYSROOT_DIR}/System/Library/Frameworks/
	mkdir -p ${dir}
	rsync -rlc ${.OBJDIR}/${FRAMEWORK}.framework ${dir}
.endfor

.include <minkar.framework.mk>
