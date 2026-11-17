/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What bzip2's and LZFSE's sources take from <stdlib.h>, in the kernel:
 * size_t, NULL, and malloc and free, which each kext maps (-Dmalloc=,
 * -Dfree=) to its own allocator.
 */

#ifndef _MINKAR_DI_KSHIM_STDLIB_H_
#define _MINKAR_DI_KSHIM_STDLIB_H_

#include <stddef.h>

void *malloc(size_t size);
void free(void *p);

#endif
