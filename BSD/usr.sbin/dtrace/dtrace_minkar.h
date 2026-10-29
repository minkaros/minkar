/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * dtrace_minkar.h: included before each of dtrace's files. What macOS's
 * headers bring along indirectly (string functions, dispatch), and the
 * prototypes of the private functions dtrace calls, which Minkar's
 * libsystem_kernel and libsystem_pthread export.
 */

#ifndef _DTRACE_MINKAR_H
#define _DTRACE_MINKAR_H

#include <string.h>
#include <strings.h>
#ifndef __cplusplus
#include <dispatch/dispatch.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
int	pid_resume(int pid);
int	pthread_set_fixedpriority_self(void);
#ifdef __cplusplus
}
#endif

#endif /* !_DTRACE_MINKAR_H */
