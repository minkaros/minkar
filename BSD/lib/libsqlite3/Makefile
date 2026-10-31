# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
# SQLite 3.54.1 (public domain; dist/ is the unchanged amalgamation from
# sqlite.org, SHA3-256 92281c86...e30): /usr/lib/libsqlite3.dylib, its
# headers and the sqlite3 shell, as the reference system ships them.

MK_WERROR = no
LIB = sqlite3
SUBDIR =
MAN =
.PATH: ${.CURDIR}/dist
SRCS = sqlite3.c

# The features the reference system's library has.
SQLITE_OPTS = -DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_FTS3 \
	-DSQLITE_ENABLE_FTS3_PARENTHESIS -DSQLITE_ENABLE_FTS4 \
	-DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_RTREE \
	-DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_ENABLE_LOCKING_STYLE=1 \
	-DSQLITE_ENABLE_DBSTAT_VTAB -DSQLITE_ENABLE_PREUPDATE_HOOK \
	-DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_SNAPSHOT \
	-DSQLITE_ENABLE_STAT4 -DSQLITE_ENABLE_UNLOCK_NOTIFY \
	-DSQLITE_ENABLE_API_ARMOR -DSQLITE_OMIT_LOAD_EXTENSION=0 \
	-DHAVE_USLEEP=1

CFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/dist -Wno-nullability-completeness ${SQLITE_OPTS}
LDFLAGS += --sysroot=${MINKAR_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-fuse-ld=${LD}

# The library's own versions: compatibility 9.0.0, what binaries linked
# against the reference system's libsqlite3 record; current 354.1.0
# (SQLite 3.54.1).
SQLITE_COMPAT = 9.0.0
SQLITE_CURRENT = 354.1.0

all: lib${LIB}.dylib sqlite3 install-headers man-page

lib${LIB}.dylib: lib${LIB}.a
	${CC} ${LDFLAGS} -o ${.TARGET} -shared -Wl,-force_load,lib${LIB}.a \
	  -Wl,-compatibility_version,${SQLITE_COMPAT} \
	  -Wl,-current_version,${SQLITE_CURRENT} \
	  -Wl,-install_name,/usr/lib/libsqlite3.dylib
	mkdir -p ${SYSROOT_DIR}/usr/lib ${MINKAR_SDKROOT}/usr/lib
	cp -f ${.TARGET} ${SYSROOT_DIR}/usr/lib/libsqlite3.dylib
	cp -f ${.TARGET} ${MINKAR_SDKROOT}/usr/lib/libsqlite3.dylib
	ln -fs libsqlite3.dylib ${SYSROOT_DIR}/usr/lib/libsqlite3.0.dylib

install-headers: .PHONY
	mkdir -p ${MINKAR_SDKROOT}/usr/include
	install -m 0444 ${.CURDIR}/dist/sqlite3.h ${.CURDIR}/dist/sqlite3ext.h \
	  ${MINKAR_SDKROOT}/usr/include/

sqlite3: ${.CURDIR}/dist/shell.c lib${LIB}.dylib
	mkdir -p ${SYSROOT_DIR}/usr/bin
	${CC} ${CFLAGS} -o ${.TARGET}.o -c ${.CURDIR}/dist/shell.c
	${CC} ${LDFLAGS} -o ${SYSROOT_DIR}/usr/bin/sqlite3 ${.TARGET}.o \
	  -L${.OBJDIR} -lsqlite3

man-page: .PHONY
	mkdir -p ${SYSROOT_DIR}/usr/share/man/man1
	install -m 0444 ${.CURDIR}/sqlite3.1 ${SYSROOT_DIR}/usr/share/man/man1/

.include <bsd.lib.mk>
