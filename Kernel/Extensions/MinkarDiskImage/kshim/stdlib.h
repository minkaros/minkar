/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What bzip2's, LZFSE's and liblzma's sources take from <stdlib.h>, in the
 * kernel: size_t, NULL, and malloc, calloc and free, which each kext maps
 * (-Dmalloc=, -Dcalloc=, -Dfree=) to its own allocator.
 */

#ifndef _MINKAR_DI_KSHIM_STDLIB_H_
#define _MINKAR_DI_KSHIM_STDLIB_H_

#include <stddef.h>

void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void free(void *p);

#endif
