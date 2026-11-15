# The programs link BSD/lib's libraries, and only those (the rest comes
# from earlier stages): lib first, then everything else at once; then the
# manual pages the programs did not install themselves (share/man).
# glib-networking comes after usr.bin, which builds the OpenSSL it uses.
# The media decoder (CoreServices/mediadecoder) links lib's FFmpeg.
SUBDIR_PARALLEL =
SUBDIR = etc lib share/locale share/zoneinfo .WAIT bin sbin libexec usr.bin usr.sbin \
	../CoreServices/mediadecoder \
	.WAIT lib/glib-networking share/man

.include <bsd.subdir.mk>
