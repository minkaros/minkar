# The programs link BSD/lib's libraries, and only those (the rest comes
# from earlier stages): lib first, then everything else at once.
SUBDIR_PARALLEL =
SUBDIR = etc lib .WAIT bin sbin libexec usr.bin usr.sbin

.include <bsd.subdir.mk>
