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
 * The Authorization types DiskArbitration's interface carries. Minkar has
 * no Security framework yet: a session sends no authorization, and
 * diskarbitrationd decides a user's rights itself (see DASupport.c).
 */

#ifndef _MINKAR_COMPAT_AUTHORIZATION_H
#define _MINKAR_COMPAT_AUTHORIZATION_H

#include <CoreFoundation/CoreFoundation.h>

typedef const struct AuthorizationOpaqueRef *AuthorizationRef;
typedef UInt32 AuthorizationFlags;

enum {
	errAuthorizationSuccess		= 0,
	errAuthorizationDenied		= -60005,
	kAuthorizationFlagDefaults	= 0,
	kAuthorizationFlagInteractionAllowed = (1 << 0),
	kAuthorizationFlagExtendRights	= (1 << 1),
	kAuthorizationFlagPreAuthorize	= (1 << 4),
};

#define kAuthorizationExternalFormLength 32
typedef struct {
	char	bytes[kAuthorizationExternalFormLength];
} AuthorizationExternalForm;

#endif /* !_MINKAR_COMPAT_AUTHORIZATION_H */
