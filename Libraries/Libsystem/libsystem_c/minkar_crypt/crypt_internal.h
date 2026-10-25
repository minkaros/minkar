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
 * Included first (-include) in every file of crypt(3)'s SHA-crypt support:
 * the SHA-2 and SHA-crypt functions are libc internals, so they get libc
 * private names (and the files are built with -fvisibility=hidden).
 * Programs that bring their own SHA512_Init() (OpenSSL, libmd) never see
 * these.
 */

#ifndef _MINKAR_CRYPT_INTERNAL_H_
#define _MINKAR_CRYPT_INTERNAL_H_

#include <stdint.h>		/* misc.c expects it from <sys/types.h>, as on FreeBSD */

#define SHA256_Init	__libc_SHA256_Init
#define SHA256_Update	__libc_SHA256_Update
#define SHA256_Final	__libc_SHA256_Final
#define SHA512_Init	__libc_SHA512_Init
#define SHA512_Update	__libc_SHA512_Update
#define SHA512_Final	__libc_SHA512_Final
#define crypt_sha256	__libc_crypt_sha256
#define crypt_sha512	__libc_crypt_sha512
#define _crypt_to64	__libc_crypt_to64
#define b64_from_24bit	__libc_b64_from_24bit

#ifdef __APPLE__
/* FreeBSD's explicit_bzero(): on Darwin, C11's memset_s() never elided. */
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>
#define explicit_bzero(p, n)	((void)memset_s((p), (n), 0, (n)))
#endif

#endif /* _MINKAR_CRYPT_INTERNAL_H_ */
