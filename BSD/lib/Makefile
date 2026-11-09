SUBDIR_PARALLEL =
# libcompression is built by Frameworks: Foundation links it.
SUBDIR = bzip2 libsqlite3 libxar ncurses libedit libutil libxo libbsdcompat libmd libarchive libfetch libmagic libpw libdqfile libuv liburcu libdrm libdrm/tests vulkan-loader mesa mesa/tests pcre2 libffi glib pcre2/host glib/host nghttp2 brotli libpsl libsoup expat libpng libjpeg libwebp woff2 freetype fontconfig harfbuzz libgpg-error libgcrypt libtasn1 xkeyboard-config libxkbcommon libepoxy libxml2 libxslt javascriptcore
SUBDIR_DEPEND_libedit =	ncurses
SUBDIR_DEPEND_libdrm/tests =	libdrm
SUBDIR_DEPEND_mesa =	libdrm
SUBDIR_DEPEND_libepoxy =	mesa
SUBDIR_DEPEND_mesa/tests =	mesa vulkan-loader
SUBDIR_DEPEND_libxar =	bzip2
SUBDIR_DEPEND_libxo =	libutil
SUBDIR_DEPEND_glib =	pcre2 libffi
SUBDIR_DEPEND_glib/host =	pcre2/host
SUBDIR_DEPEND_libsoup =	glib glib/host nghttp2 brotli libpsl
SUBDIR_DEPEND_freetype =	libpng brotli
SUBDIR_DEPEND_fontconfig =	freetype expat
SUBDIR_DEPEND_harfbuzz =	freetype glib
SUBDIR_DEPEND_woff2 =	brotli
SUBDIR_DEPEND_libgcrypt =	libgpg-error
SUBDIR_DEPEND_libxslt =	libxml2

.include <bsd.subdir.mk>

