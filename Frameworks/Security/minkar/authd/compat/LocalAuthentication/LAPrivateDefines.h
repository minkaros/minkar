/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * LocalAuthentication's calls authd makes: none succeeds (see
 * LAPublicDefines.h).
 */

#ifndef MINKAR_AUTHD_LAPRIVATEDEFINES_H
#define MINKAR_AUTHD_LAPRIVATEDEFINES_H

#include <LocalAuthentication/LAPublicDefines.h>

static inline CFTypeRef LACreateNewContextWithACMContextInSession(uid_t uid, CFDataRef acm, CFErrorRef *error)
{
	(void)uid; (void)acm;
	if (error != NULL)
		*error = NULL;
	return NULL;
}

static inline CFDataRef LACopyCredential(CFTypeRef context, long type, CFErrorRef *error)
{
	(void)context; (void)type;
	if (error != NULL)
		*error = NULL;
	return NULL;
}

static inline CFDictionaryRef LACopyResultOfPolicyEvaluation(CFTypeRef context, long policy, CFDictionaryRef options,
                                                             CFErrorRef *error)
{
	(void)context; (void)policy; (void)options;
	if (error != NULL)
		*error = NULL;
	return NULL;
}

#endif
