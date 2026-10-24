/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * libmd's helpers for every digest: the digest as a hex string (End),
 * and the digest of a descriptor, a file, part of either, or a buffer.
 * Original implementation of FreeBSD libmd's interface.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "md5.h"
#include "sha.h"
#include "sha224.h"
#include "sha256.h"
#include "sha384.h"
#include "sha512.h"
#include "sha512t.h"

static char *
hex(const unsigned char *d, size_t n, char *buf)
{
	static const char digits[] = "0123456789abcdef";

	if (buf == NULL && (buf = malloc(2 * n + 1)) == NULL)
		return (NULL);
	for (size_t i = 0; i < n; i++) {
		buf[2 * i] = digits[d[i] >> 4];
		buf[2 * i + 1] = digits[d[i] & 15];
	}
	buf[2 * n] = '\0';
	return (buf);
}

/*
 * len 0: to the end. Reads in chunks; a read error gives NULL with errno.
 */
#define	HELPERS(CTX, INIT, UPDATE, FINAL, LEN, END, FD, FDCHUNK, FILE_, FILECHUNK, DATA) \
char *									\
END(CTX *ctx, char *buf)						\
{									\
	unsigned char d[LEN];						\
									\
	FINAL(d, ctx);							\
	return (hex(d, LEN, buf));					\
}									\
									\
char *									\
FDCHUNK(int fd, char *buf, off_t ofs, off_t len)			\
{									\
	unsigned char block[16 * 1024];					\
	CTX ctx;							\
	ssize_t n;							\
									\
	if (ofs != 0 && lseek(fd, ofs, SEEK_SET) == -1)			\
		return (NULL);						\
	INIT(&ctx);							\
	for (;;) {							\
		size_t want = sizeof(block);				\
									\
		if (len > 0 && (off_t)want > len)			\
			want = (size_t)len;				\
		if (want == 0)						\
			break;						\
		if ((n = read(fd, block, want)) < 0)			\
			return (NULL);					\
		if (n == 0)						\
			break;						\
		UPDATE(&ctx, block, (size_t)n);				\
		if (len > 0 && (len -= n) == 0)				\
			break;						\
	}								\
	return (END(&ctx, buf));					\
}									\
									\
char *									\
FD(int fd, char *buf)							\
{									\
	return (FDCHUNK(fd, buf, 0, 0));				\
}									\
									\
char *									\
FILECHUNK(const char *name, char *buf, off_t ofs, off_t len)		\
{									\
	char *ret;							\
	int fd, e;							\
									\
	if ((fd = open(name, O_RDONLY)) < 0)				\
		return (NULL);						\
	ret = FDCHUNK(fd, buf, ofs, len);				\
	e = errno;							\
	close(fd);							\
	errno = e;							\
	return (ret);							\
}									\
									\
char *									\
FILE_(const char *name, char *buf)					\
{									\
	return (FILECHUNK(name, buf, 0, 0));				\
}									\
									\
char *									\
DATA(const void *data, unsigned int len, char *buf)			\
{									\
	CTX ctx;							\
									\
	INIT(&ctx);							\
	UPDATE(&ctx, data, len);					\
	return (END(&ctx, buf));					\
}

static void md5_update(MD5_CTX *c, const void *p, size_t n) { MD5Update(c, p, (unsigned int)n); }

HELPERS(MD5_CTX, MD5Init, md5_update, MD5Final, MD5_DIGEST_LENGTH,
    MD5End, MD5Fd, MD5FdChunk, MD5File, MD5FileChunk, MD5Data)
HELPERS(SHA_CTX, SHA1_Init, SHA1_Update, SHA1_Final, 20,
    SHA1_End, SHA1_Fd, SHA1_FdChunk, SHA1_File, SHA1_FileChunk, SHA1_Data)
HELPERS(SHA224_CTX, SHA224_Init, SHA224_Update, SHA224_Final, SHA224_DIGEST_LENGTH,
    SHA224_End, SHA224_Fd, SHA224_FdChunk, SHA224_File, SHA224_FileChunk, SHA224_Data)
HELPERS(SHA256_CTX, SHA256_Init, SHA256_Update, SHA256_Final, SHA256_DIGEST_LENGTH,
    SHA256_End, SHA256_Fd, SHA256_FdChunk, SHA256_File, SHA256_FileChunk, SHA256_Data)
HELPERS(SHA384_CTX, SHA384_Init, SHA384_Update, SHA384_Final, SHA384_DIGEST_LENGTH,
    SHA384_End, SHA384_Fd, SHA384_FdChunk, SHA384_File, SHA384_FileChunk, SHA384_Data)
HELPERS(SHA512_CTX, SHA512_Init, SHA512_Update, SHA512_Final, SHA512_DIGEST_LENGTH,
    SHA512_End, SHA512_Fd, SHA512_FdChunk, SHA512_File, SHA512_FileChunk, SHA512_Data)
HELPERS(SHA512_CTX, SHA512_224_Init, SHA512_224_Update, SHA512_224_Final, SHA512_224_DIGEST_LENGTH,
    SHA512_224_End, SHA512_224_Fd, SHA512_224_FdChunk, SHA512_224_File, SHA512_224_FileChunk, SHA512_224_Data)
HELPERS(SHA512_CTX, SHA512_256_Init, SHA512_256_Update, SHA512_256_Final, SHA512_256_DIGEST_LENGTH,
    SHA512_256_End, SHA512_256_Fd, SHA512_256_FdChunk, SHA512_256_File, SHA512_256_FileChunk, SHA512_256_Data)
