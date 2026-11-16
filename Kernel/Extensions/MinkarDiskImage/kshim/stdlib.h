/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What bzip2's and LZFSE's sources take from <stdlib.h>, in the kernel:
 * size_t and NULL. Their malloc and free are mapped to MinkarDiskImageCodecs.c.
 */

#ifndef _MINKAR_DI_KSHIM_STDLIB_H_
#define _MINKAR_DI_KSHIM_STDLIB_H_

#include <stddef.h>

void *minkar_di_malloc(size_t size);
void minkar_di_free(void *p);

#endif
