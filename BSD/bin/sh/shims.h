#ifndef _SHIMS_H
#define _SHIMS_H

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define O_VERIFY 0x10000000 /* dummy value */

typedef int (*fn_t)(const void *, const void *, void *);

char * strchrnul(char * s, char ch);
int eaccess(const char * p, int mode);
void * reallocarray(void * p, size_t n, size_t size);
int qsort_s(void * base, size_t nmemb, size_t size, fn_t compar, void * thunk);

#define SIG2STR_MAX 32
#define sys_nsig NSIG	/* FreeBSD name for the number of signals */
int sig2str(int signum, char *str);
int str2sig(const char *str, int *pnum);

#endif

