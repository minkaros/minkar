/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * MD5 (RFC 1321) with FreeBSD libmd's interface. Original implementation.
 */

#ifndef _MD5_H_
#define _MD5_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>

#define	MD5_BLOCK_LENGTH		64
#define	MD5_DIGEST_LENGTH		16
#define	MD5_DIGEST_STRING_LENGTH	(MD5_DIGEST_LENGTH * 2 + 1)

typedef struct MD5Context {
	uint32_t	state[4];	/* A, B, C, D */
	uint32_t	count[2];	/* bits, low and high */
	unsigned char	buffer[64];
} MD5_CTX;

__BEGIN_DECLS
void	MD5Init(MD5_CTX *);
void	MD5Update(MD5_CTX *, const void *, unsigned int);
void	MD5Final(unsigned char [MD5_DIGEST_LENGTH], MD5_CTX *);
char	*MD5End(MD5_CTX *, char *);
char	*MD5Fd(int, char *);
char	*MD5FdChunk(int, char *, off_t, off_t);
char	*MD5File(const char *, char *);
char	*MD5FileChunk(const char *, char *, off_t, off_t);
char	*MD5Data(const void *, unsigned int, char *);
__END_DECLS

#endif /* !_MD5_H_ */
