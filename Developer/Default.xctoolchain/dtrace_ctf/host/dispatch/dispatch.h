/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
/*
 * The part of <dispatch/dispatch.h> the CTF tools use (barrier.h): the
 * semaphores host_shim.c makes on pthreads. Build hosts without
 * libdispatch's headers (Ubuntu 24.04 dropped its package) take this one.
 */
#ifndef _MINKAR_HOST_DISPATCH_H_
#define _MINKAR_HOST_DISPATCH_H_

#include <stdint.h>

typedef struct dispatch_semaphore_s *dispatch_semaphore_t;
typedef uint64_t dispatch_time_t;

#define DISPATCH_TIME_NOW	(0ull)
#define DISPATCH_TIME_FOREVER	(~0ull)

dispatch_semaphore_t dispatch_semaphore_create(long value);
long dispatch_semaphore_signal(dispatch_semaphore_t dsema);
long dispatch_semaphore_wait(dispatch_semaphore_t dsema, dispatch_time_t timeout);

#endif /* _MINKAR_HOST_DISPATCH_H_ */
