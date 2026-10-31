/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * xar's config.h for Darwin, written by hand (xar's configure is not run):
 * what Darwin has, and the sizes of its types.
 */

#define HAVE_SYS_XATTR_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_GETXATTR 1
#define HAVE_SETXATTR 1
#define HAVE_GETATTRLIST 1
#define HAVE_SETATTRLIST 1
#define HAVE_CHFLAGS 1
#define HAVE_STATVFS 1
#define HAVE_STATFS 1
#define HAVE_STRUCT_STAT_ST_FLAGS 1
#define HAVE_STRUCT_STATFS_F_FSTYPENAME 1
#define HAVE_SYS_ACL_H 1
#define HAVE_LIBPTHREAD 1
#define HAVE_ASPRINTF 1
#define HAVE_LIBBZ2 1
/* No LZMA: lzmaxar.c is written for LZMA Utils, before xz 5; the
 * reference system builds xar without it too. */
#define HAVE_LCHOWN 1
#define HAVE_LCHMOD 1
#define HAVE_STRMODE 1
#define UID_STRING PRIu32
#define UID_CAST (uint32_t)
#define GID_STRING PRIu32
#define GID_CAST (uint32_t)
#define INO_STRING PRIu64
#define INO_HEXSTRING PRIx64
#define INO_CAST (uint64_t)
#define DEV_STRING PRIu32
#define DEV_HEXSTRING PRIx32
#define DEV_CAST (uint32_t)
