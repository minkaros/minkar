/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The part of macOS's ServiceManagement the NFS daemons use (sm_compat.c):
 * whether a launchd job is loaded and whether it is enabled.
 */

#ifndef _MINKAR_SERVICEMANAGEMENT_H_
#define _MINKAR_SERVICEMANAGEMENT_H_

#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS
extern const CFStringRef kSMDomainSystemLaunchd;
extern const CFStringRef kSMDomainUserLaunchd;

/*
 * Whether the job is loaded; *persistence is set so that, as on macOS,
 * loaded XOR persistence tells it is disabled.
 */
Boolean SMJobIsEnabled(CFStringRef domain, CFStringRef label,
    Boolean *persistence);
__END_DECLS

#endif /* _MINKAR_SERVICEMANAGEMENT_H_ */
