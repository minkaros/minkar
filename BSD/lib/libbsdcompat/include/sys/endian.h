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
 * FreeBSD's <sys/endian.h> for Minkar (Darwin has <machine/endian.h> and
 * <libkern/OSByteOrder.h>): byte order conversions, and encoding and
 * decoding of integers in a byte stream.
 */

#ifndef _MINKAR_COMPAT_SYS_ENDIAN_H
#define _MINKAR_COMPAT_SYS_ENDIAN_H

#include <sys/types.h>
#include <stdint.h>
#include <machine/endian.h>

#define	_BYTE_ORDER	BYTE_ORDER
#define	_LITTLE_ENDIAN	LITTLE_ENDIAN
#define	_BIG_ENDIAN	BIG_ENDIAN

#define	bswap16(x)	__builtin_bswap16(x)
#define	bswap32(x)	__builtin_bswap32(x)
#define	bswap64(x)	__builtin_bswap64(x)

#if BYTE_ORDER == LITTLE_ENDIAN
#define	htobe16(x)	bswap16((uint16_t)(x))
#define	htobe32(x)	bswap32((uint32_t)(x))
#define	htobe64(x)	bswap64((uint64_t)(x))
#define	htole16(x)	((uint16_t)(x))
#define	htole32(x)	((uint32_t)(x))
#define	htole64(x)	((uint64_t)(x))
#else
#define	htobe16(x)	((uint16_t)(x))
#define	htobe32(x)	((uint32_t)(x))
#define	htobe64(x)	((uint64_t)(x))
#define	htole16(x)	bswap16((uint16_t)(x))
#define	htole32(x)	bswap32((uint32_t)(x))
#define	htole64(x)	bswap64((uint64_t)(x))
#endif
#define	be16toh(x)	htobe16(x)
#define	be32toh(x)	htobe32(x)
#define	be64toh(x)	htobe64(x)
#define	le16toh(x)	htole16(x)
#define	le32toh(x)	htole32(x)
#define	le64toh(x)	htole64(x)

static inline uint16_t
be16dec(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;

	return ((uint16_t)((p[0] << 8) | p[1]));
}

static inline uint32_t
be32dec(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;

	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | p[3]);
}

static inline uint64_t
be64dec(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;

	return (((uint64_t)be32dec(p) << 32) | be32dec(p + 4));
}

static inline uint16_t
le16dec(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;

	return ((uint16_t)((p[1] << 8) | p[0]));
}

static inline uint32_t
le32dec(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;

	return (((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
	    ((uint32_t)p[1] << 8) | p[0]);
}

static inline uint64_t
le64dec(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;

	return (((uint64_t)le32dec(p + 4) << 32) | le32dec(p));
}

static inline void
be16enc(void *pp, uint16_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (uint8_t)(u >> 8);
	p[1] = (uint8_t)u;
}

static inline void
be32enc(void *pp, uint32_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (uint8_t)(u >> 24);
	p[1] = (uint8_t)(u >> 16);
	p[2] = (uint8_t)(u >> 8);
	p[3] = (uint8_t)u;
}

static inline void
be64enc(void *pp, uint64_t u)
{
	uint8_t *p = (uint8_t *)pp;

	be32enc(p, (uint32_t)(u >> 32));
	be32enc(p + 4, (uint32_t)u);
}

static inline void
le16enc(void *pp, uint16_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (uint8_t)u;
	p[1] = (uint8_t)(u >> 8);
}

static inline void
le32enc(void *pp, uint32_t u)
{
	uint8_t *p = (uint8_t *)pp;

	p[0] = (uint8_t)u;
	p[1] = (uint8_t)(u >> 8);
	p[2] = (uint8_t)(u >> 16);
	p[3] = (uint8_t)(u >> 24);
}

static inline void
le64enc(void *pp, uint64_t u)
{
	uint8_t *p = (uint8_t *)pp;

	le32enc(p, (uint32_t)u);
	le32enc(p + 4, (uint32_t)(u >> 32));
}

#endif /* !_MINKAR_COMPAT_SYS_ENDIAN_H */
