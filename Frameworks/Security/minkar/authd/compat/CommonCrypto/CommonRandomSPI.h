/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CommonCrypto's random bytes, as the public CCRandomGenerateBytes.
 */

#ifndef MINKAR_AUTHD_COMMONRANDOMSPI_H
#define MINKAR_AUTHD_COMMONRANDOMSPI_H

#include <CommonCrypto/CommonRandom.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptoError.h>

typedef const void *CCRandomRef;
#define kCCRandomDefault ((CCRandomRef)0)

static inline int CCRandomCopyBytes(CCRandomRef rnd, void *bytes, size_t count)
{
	(void)rnd;
	return CCRandomGenerateBytes(bytes, count) == kCCSuccess ? 0 : -1;
}

#endif
