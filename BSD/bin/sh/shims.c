/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * FreeBSD functions sh uses that Darwin lacks. (test, printf and kill
 * are real built-ins now: see builtins.def and the Makefile.)
 */
#include "shims.h"
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <strings.h>

char *
strchrnul(char * s, char ch)
{
	char * result = strchr(s, ch);
	if (!result)
		return &s[strlen(s)];
	return result;
}

int
eaccess(const char *p, int mode)
{
	return access(p, mode);
}

void *
reallocarray(void * p, size_t n, size_t size)
{
	if (size != 0 && n > SIZE_MAX / size) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(p, n * size);
}

/* C11's qsort_s through Darwin's qsort_r, whose comparator takes the
   context first. */
struct qsort_s_ctx {
	fn_t	compar;
	void	*thunk;
};

static int
qsort_s_adapter(void *ctx, const void *a, const void *b)
{
	struct qsort_s_ctx *c = ctx;

	return (c->compar(a, b, c->thunk));
}

int
qsort_s(void * base, size_t nmemb, size_t size, fn_t compar, void * thunk)
{
	struct qsort_s_ctx ctx = { compar, thunk };

	if (nmemb > RSIZE_MAX || size > RSIZE_MAX)
		return EINVAL;
	if (nmemb > 0 && compar == NULL)
		return EINVAL;
	qsort_r(base, nmemb, size, &ctx, qsort_s_adapter);
	return 0;
}

/* POSIX.1-2024 signal names, from Darwin's sys_signame ("hup", ...). */
int
sig2str(int signum, char *str)
{
	size_t i;

	if (signum <= 0 || signum >= NSIG)
		return -1;
	for (i = 0; sys_signame[signum][i] != '\0' && i < SIG2STR_MAX - 1; i++)
		str[i] = toupper((unsigned char)sys_signame[signum][i]);
	str[i] = '\0';
	return 0;
}

int
str2sig(const char *str, int *pnum)
{
	long n;
	char *end;
	int i;

	if (*str >= '0' && *str <= '9') {
		errno = 0;
		n = strtol(str, &end, 10);
		if (errno != 0 || *end != '\0' || n < 0 || n >= NSIG)
			return -1;
		*pnum = (int)n;
		return 0;
	}
	for (i = 1; i < NSIG; i++) {
		if (strcasecmp(str, sys_signame[i]) == 0) {
			*pnum = i;
			return 0;
		}
	}
	return -1;
}
