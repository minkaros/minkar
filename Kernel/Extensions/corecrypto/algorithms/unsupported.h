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
 * The corecrypto kext's function table entries Minkar does not implement
 * (HMAC, AES-XTS and AES-GCM, DES and triple DES, RC4, Blowfish, CAST):
 * the kernel paths that use them are not used by Minkar yet. Each logs
 * its first use. Written for Minkar from the table's published interface
 * (Kernel/xnu/libkern/libkern/crypto/register_crypto.h).
 */

#ifndef _MINKAR_CC_UNSUPPORTED_H_
#define _MINKAR_CC_UNSUPPORTED_H_

#include <corecrypto/ccdigest.h>
#include <corecrypto/cchmac.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccrc4.h>

__BEGIN_DECLS
void pdchmac_init_fn_dummy(const struct ccdigest_info *di, cchmac_ctx_t ctx,
    unsigned long key_len, const void *key);
void pdchmac_update_fn_dummy(const struct ccdigest_info *di, cchmac_ctx_t ctx,
    unsigned long data_len, const void *data);
void pdchmac_final_fn_dummy(const struct ccdigest_info *di, cchmac_ctx_t ctx,
    unsigned char *mac);
void pdchmac_fn_dummy(const struct ccdigest_info *di, unsigned long key_len,
    const void *key, unsigned long data_len, const void *data, unsigned char *mac);

extern const struct ccmode_gcm pdcaes_gcm_encrypt_dummy;
extern const struct ccmode_gcm pdcaes_gcm_decrypt_dummy;
extern const struct ccmode_ecb pdcdes_ecb_encrypt_dummy;
extern const struct ccmode_ecb pdcdes_ecb_decrypt_dummy;
extern const struct ccmode_cbc pdcdes_cbc_encrypt_dummy;
extern const struct ccmode_cbc pdcdes_cbc_decrypt_dummy;
extern const struct ccmode_ecb pdctdes_ecb_encrypt_dummy;
extern const struct ccmode_ecb pdctdes_ecb_decrypt_dummy;
extern const struct ccmode_cbc pdctdes_cbc_encrypt_dummy;
extern const struct ccmode_cbc pdctdes_cbc_decrypt_dummy;
extern const struct ccrc4_info pdcrc4_info_dummy;
extern const struct ccmode_ecb pdcblowfish_ecb_encrypt_dummy;
extern const struct ccmode_ecb pdcblowfish_ecb_decrypt_dummy;
extern const struct ccmode_ecb pdccast_ecb_encrypt_dummy;
extern const struct ccmode_ecb pdccast_ecb_decrypt_dummy;

int pdcdes_key_is_weak_fn_dummy(void *key, unsigned long length);
void pdcdes_key_set_odd_parity_fn_dummy(void *key, unsigned long length);
void pdcpad_xts_decrypt_fn_dummy(const struct ccmode_xts *xts, ccxts_ctx *ctx,
    unsigned long nbytes, const void *in, void *out);
void pdcpad_xts_encrypt_fn_dummy(const struct ccmode_xts *xts, ccxts_ctx *ctx,
    unsigned long nbytes, const void *in, void *out);
__END_DECLS

#endif /* _MINKAR_CC_UNSUPPORTED_H_ */
