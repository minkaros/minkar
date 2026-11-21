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
 * The corecrypto kext's digests: the common padding and output (digest.c)
 * and each algorithm's compression function (md5.c, sha1.c, sha2.c),
 * written for Minkar from the published specifications (RFC 1321,
 * FIPS 180-4) and the interface of struct ccdigest_info in the published
 * <corecrypto/ccdigest.h>.
 */

#ifndef _MINKAR_CC_DIGEST_H_
#define _MINKAR_CC_DIGEST_H_

#include <corecrypto/ccdigest.h>

__BEGIN_DECLS
/* di->final for the MD5 family: the length and the output little-endian. */
void mcc_digest_final_le(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
    unsigned char *digest);
/* di->final for the SHA families: the length and the output big-endian. */
void mcc_digest_final_be(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
    unsigned char *digest);
__END_DECLS

#endif /* _MINKAR_CC_DIGEST_H_ */
