/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
/*
 * What the CTF tools take from Darwin's libSystem, for their Linux build:
 * dispatch semaphores (on pthreads) and fls().
 */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

struct dispatch_semaphore_s {
	pthread_mutex_t	lock;
	pthread_cond_t	cond;
	long		value;
};

struct dispatch_semaphore_s *
dispatch_semaphore_create(long value)
{
	struct dispatch_semaphore_s *s;

	if (value < 0 || (s = calloc(1, sizeof(*s))) == NULL)
		return NULL;
	pthread_mutex_init(&s->lock, NULL);
	pthread_cond_init(&s->cond, NULL);
	s->value = value;
	return s;
}

long
dispatch_semaphore_signal(struct dispatch_semaphore_s *s)
{
	long woke;

	pthread_mutex_lock(&s->lock);
	woke = s->value++ < 0;
	pthread_cond_signal(&s->cond);
	pthread_mutex_unlock(&s->lock);
	return woke;
}

/* The tools wait forever (DISPATCH_TIME_FOREVER). */
long
dispatch_semaphore_wait(struct dispatch_semaphore_s *s, uint64_t timeout)
{
	(void)timeout;
	pthread_mutex_lock(&s->lock);
	while (s->value <= 0)
		pthread_cond_wait(&s->cond, &s->lock);
	s->value--;
	pthread_mutex_unlock(&s->lock);
	return 0;
}

int
fls(int mask)
{
	return mask == 0 ? 0 : 32 - __builtin_clz((unsigned int)mask);
}

/* BSD's qsort_r on glibc's: the comparison takes the thunk first. */
struct bsd_qsort_thunk {
	void	*thunk;
	int	(*compar)(void *, const void *, const void *);
};

static int
bsd_qsort_compare(const void *a, const void *b, void *arg)
{
	struct bsd_qsort_thunk *t = arg;

	return t->compar(t->thunk, a, b);
}

extern void qsort_r(void *, size_t, size_t,
    int (*)(const void *, const void *, void *), void *);

void
minkar_bsd_qsort_r(void *base, size_t nel, size_t width, void *thunk,
    int (*compar)(void *, const void *, const void *))
{
	struct bsd_qsort_thunk t = { thunk, compar };

	qsort_r(base, nel, width, bsd_qsort_compare, &t);
}
