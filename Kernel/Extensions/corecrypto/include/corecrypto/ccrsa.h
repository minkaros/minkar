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
 * The RSA types the corecrypto kext's function table names, written for
 * Minkar from their call sites only (clean room):
 *
 *   Kernel/xnu/libkern/libkern/crypto/register_crypto.h, which declares
 *     ccrsa_make_pub_fn_t and ccrsa_verify_pkcs1v15_fn_t with a
 *     ccrsa_pub_ctx_t key, and
 *   Kernel/xnu/libkern/crypto/corecrypto_rsa.c, which passes the key
 *     through the table.
 *
 * The kext does not implement RSA (those table entries stay NULL), so
 * only the key's type is declared here, as an opaque pointer, which is
 * all the table's layout needs. It replaces a header that came from
 * another project under the GPL-3.0.
 */

#ifndef _CORECRYPTO_CCRSA_H_
#define _CORECRYPTO_CCRSA_H_

struct ccrsa_pub_ctx;
typedef struct ccrsa_pub_ctx *ccrsa_pub_ctx_t;

#endif /* _CORECRYPTO_CCRSA_H_ */
