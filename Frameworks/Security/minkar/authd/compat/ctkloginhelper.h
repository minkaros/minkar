/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CryptoTokenKit's login helper: Minkar has no smart cards, so none is
 * ever enforced.
 */

#ifndef MINKAR_AUTHD_CTKLOGINHELPER_H
#define MINKAR_AUTHD_CTKLOGINHELPER_H

#define kTKEnforceSmartcard 1

static inline int TKGetSmartcardSetting(int setting)
{
	(void)setting;
	return 0;
}

#endif
