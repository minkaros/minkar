/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
/*
 * Included before each file of the CTF tools' Linux build: what Darwin's
 * headers declare along the way, and BSD's qsort_r (glibc's takes its
 * arguments in another order).
 */
#ifndef _HOST_PRELUDE_H
#define _HOST_PRELUDE_H
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
int	fls(int);
void	(*sigset(int, void (*)(int)))(int);
int	sighold(int);
void	minkar_bsd_qsort_r(void *, size_t, size_t, void *,
	    int (*)(void *, const void *, const void *));
#ifdef __cplusplus
}
#endif
#define qsort_r	minkar_bsd_qsort_r
#endif
