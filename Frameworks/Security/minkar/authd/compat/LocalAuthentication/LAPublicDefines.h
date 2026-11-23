/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * LocalAuthentication (biometrics, the secure element's credentials):
 * Minkar has none, so there is never a context, a credential or a result
 * and authd asks for a password.
 */

#ifndef MINKAR_AUTHD_LAPUBLICDEFINES_H
#define MINKAR_AUTHD_LAPUBLICDEFINES_H

#include <CoreFoundation/CoreFoundation.h>

#define kLAPolicyDeviceOwnerAuthentication	2
#define kLAOptionNotInteractive			1013
#define kLACredentialTypeExtractablePasscode	(-9)

#endif
