/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * FreeBSD library functions that the FreeBSD commands built on Minkar use
 * and Darwin lacks (libbsdcompat.a). Included first by those commands
 * (-include bsdcompat.h).
 */

#ifndef _MINKAR_BSDCOMPAT_H
#define _MINKAR_BSDCOMPAT_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Names FreeBSD has and Darwin calls otherwise, or lacks:
 * - struct stat's timespecs are st_atimespec etc. on Darwin;
 * - O_RESOLVE_BENEATH, AT_RESOLVE_BENEATH: no such lookup restriction on
 *   Darwin, so cp -R and friends open without it (a symbolic link in the
 *   destination can lead outside it, as with older FreeBSD);
 * - UF_ARCHIVE: no such flag (Darwin's is the superuser's SF_ARCHIVED).
 */
#define	st_atim		st_atimespec
#define	st_mtim		st_mtimespec
#define	st_ctim		st_ctimespec
#define	st_birthtim	st_birthtimespec
#ifndef O_RESOLVE_BENEATH
#define	O_RESOLVE_BENEATH	0
#endif
#ifndef AT_RESOLVE_BENEATH
#define	AT_RESOLVE_BENEATH	0
#endif
#ifndef UF_ARCHIVE
#define	UF_ARCHIVE	0
#endif
#ifndef O_SEARCH
#define	O_SEARCH	O_RDONLY	/* a directory opened to look things up in */
#endif
/*
 * ACLs: Darwin's are NFSv4-style "extended" ACLs; it has no POSIX.1e
 * access ACLs (so _PC_ACL_EXTENDED is a name pathconf rejects).
 */
#define	_PC_ACL_NFS4		_PC_EXTENDED_SECURITY_NP
#define	_PC_ACL_EXTENDED	(-1)
#define	ACL_TYPE_NFS4		ACL_TYPE_EXTENDED
/* <sys/mman.h>: no core dump, no sync hints on Darwin. */
#ifndef MAP_NOCORE
#define	MAP_NOCORE	0
#endif
#ifndef MAP_NOSYNC
#define	MAP_NOSYNC	0
#endif
/* <sys/cdefs.h> and <sys/param.h> macros Darwin lacks. */
#ifndef __min_size
#define	__min_size(x)	static (x)	/* an array parameter of at least x elements */
#endif
#ifndef __packed
#define	__packed	__attribute__((__packed__))
#endif
#ifndef __aligned
#define	__aligned(x)	__attribute__((__aligned__(x)))
#endif
#ifndef __predict_true
#define	__predict_true(e)	__builtin_expect(!!(e), 1)
#define	__predict_false(e)	__builtin_expect(!!(e), 0)
#endif
#ifndef __DECONST
#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif
#ifndef rounddown
/* <sys/param.h>: x rounded down to a multiple of y. */
#define	rounddown(x, y)	(((x) / (y)) * (y))
#endif
#ifndef __DEQUALIFY
#define	__DEQUALIFY(type, var)	((type)(uintptr_t)(const volatile void *)(var))
#endif
/* <paths.h>: where local software lives. */
#ifndef _PATH_LOCALBASE
#define	_PATH_LOCALBASE	"/usr/local"
#endif

/* <sys/cdefs.h>: a char array that need not end in NUL (a warning hint). */
#ifndef __nonstring
#define	__nonstring
#endif
#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif
#ifndef COMMLEN
#define	COMMLEN		MAXCOMLEN	/* FreeBSD's command name length */
#endif
/* FreeBSD's <sys/queue.h>: moves the elements after elm to rest. */
#ifndef STAILQ_SPLIT_AFTER
#define	STAILQ_SPLIT_AFTER(head, elm, rest, field) do {			\
	if (STAILQ_NEXT((elm), field) == NULL)				\
		STAILQ_INIT((rest));					\
	else {								\
		STAILQ_FIRST((rest)) = STAILQ_NEXT((elm), field);	\
		(rest)->stqh_last = (head)->stqh_last;			\
		STAILQ_NEXT((elm), field) = NULL;			\
		(head)->stqh_last = &STAILQ_NEXT((elm), field);		\
	}								\
} while (0)
#endif
/* FreeBSD's <sys/sbuf.h>: a buffer that grows (libbsdcompat's sbuf.c). */
#ifndef sbuf_new_auto
#define	sbuf_new_auto()	sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND)
#endif
#ifndef __containerof
#define	__containerof(x, s, m)	((s *)(void *)((char *)(x) - offsetof(s, m)))
#endif
/* <sys/time.h> timespec arithmetic. */
#ifndef timespecclear
#define	timespecclear(tvp)	((tvp)->tv_sec = (tvp)->tv_nsec = 0)
#define	timespecisset(tvp)	((tvp)->tv_sec || (tvp)->tv_nsec)
#define	timespeccmp(tvp, uvp, cmp)					\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_nsec cmp (uvp)->tv_nsec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#define	timespecadd(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec + (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec + (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec >= 1000000000L) {			\
			(vsp)->tv_sec++;				\
			(vsp)->tv_nsec -= 1000000000L;			\
		}							\
	} while (0)
#define	timespecsub(tsp, usp, vsp)					\
	do {								\
		(vsp)->tv_sec = (tsp)->tv_sec - (usp)->tv_sec;		\
		(vsp)->tv_nsec = (tsp)->tv_nsec - (usp)->tv_nsec;	\
		if ((vsp)->tv_nsec < 0) {				\
			(vsp)->tv_sec--;				\
			(vsp)->tv_nsec += 1000000000L;			\
		}							\
	} while (0)
#endif
/* <netinet/in.h>, <poll.h>, <sys/param.h> */
#ifndef IPPORT_MAX
#define	IPPORT_MAX	65535
#endif
#ifndef INFTIM
#define	INFTIM		(-1)	/* poll() waits for ever */
#endif
#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	256	/* Darwin's value */
#endif
/* <arpa/nameser.h> */
#ifndef MAXDNAME
#define	MAXDNAME	1025
#endif
/* <paths.h> */
#define	_PATH_CP	"/bin/cp"
#define	_PATH_RM	"/bin/rm"

/* FreeBSD's <sys/_types.h> name (<sys/tree.h> uses it). */
typedef uintptr_t	__uintptr_t;

__BEGIN_DECLS

/* FreeBSD declares getentropy in <unistd.h>, Darwin in <sys/random.h>. */
int		getentropy(void *buf, size_t buflen);

/* <strings.h>: zeroes memory the compiler may not optimize away. */
void		explicit_bzero(void *buf, size_t len);

/* <unistd.h>: closes every open descriptor from lowfd up. */
void		closefrom(int lowfd);

/* <unistd.h>: whether the system has a feature ("inet", "inet6"). */
int		feature_present(const char *feature);

/* <stdlib.h>: a number in [minval, maxval], or 0 and *errstr set. */
long long	strtonum(const char *numstr, long long minval, long long maxval,
		    const char **errstr);

/* <libutil.h>: "10k", "4M", ... (powers of 1024) to a number. */
int		expand_number(const char *buf, int64_t *num);

/* <unistd.h>: copies len bytes; with an offset pointer, at that offset. */
#include <sys/types.h>
ssize_t		copy_file_range(int infd, off_t *inoffp, int outfd, off_t *outoffp,
		    size_t len, unsigned int flags);

/* <pwd.h>, <grp.h>: pwcache(3) name lookups, and switching their source. */
struct passwd;
struct group;
int		uid_from_user(const char *name, uid_t *uid);
int		gid_from_group(const char *name, gid_t *gid);
int		pwcache_userdb(int (*)(int), void (*)(void),
		    struct passwd *(*)(const char *), struct passwd *(*)(uid_t));
int		pwcache_groupdb(int (*)(int), void (*)(void),
		    struct group *(*)(const char *), struct group *(*)(gid_t));

/* <sys/queue.h>: the TAILQ walk from an entry on, safe against removal. */
#include <sys/queue.h>
#ifndef TAILQ_FOREACH_FROM_SAFE
#define	TAILQ_FOREACH_FROM_SAFE(var, head, field, tvar)			\
	for ((var) = ((var) ? (var) : TAILQ_FIRST((head)));		\
	    (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

/* <unistd.h>: access(2) with the effective ids. */
int		eaccess(const char *path, int mode);

/* <string.h>: compare as versions (a9 < a10). */
int		strverscmp(const char *s1, const char *s2);

/* <unistd.h>: pathconf of a symbolic link itself. */
long		lpathconf(const char *path, int name);

/* <unistd.h>: FreeBSD's per-utility compat switch; Darwin has none. */
#define	check_utility_compat(utility)	0

/* <sys/acl.h>: an ACL without entries is trivial (the mode says it all). */
struct _acl;
int		acl_is_trivial_np(struct _acl *acl, int *trivial);

/* <unistd.h>: file flags, relative to a directory descriptor. */
int		chflagsat(int fd, const char *path, unsigned long flags, int atflag);

/* <dirent.h>: scandir of a directory descriptor. */
struct dirent;
int		fdscandir(int dirfd, struct dirent ***namelist,
		    int (*select)(const struct dirent *),
		    int (*compar)(const struct dirent **, const struct dirent **));

/* <string.h>: like strchr, but the end of s rather than NULL. */
char		*strchrnul(const char *s, int c);

/* <stdlib.h>: realloc of nmemb * size, failing on overflow. */
void		*reallocarray(void *ptr, size_t nmemb, size_t size);

/* <unistd.h>: the title ps shows; Minkar keeps argv as it is. */
void		setproctitle(const char *fmt, ...) __printflike(1, 2);

/* <sys/stat.h>: relative to a directory descriptor. */
int		mkfifoat(int fd, const char *path, mode_t mode);
int		mknodat(int fd, const char *path, mode_t mode, dev_t dev);

/* <libutil.h>: where local software lives (LOCALBASE, else /usr/local). */
const char	*getlocalbase(void);

/* <signal.h>: a signal from its name ("TERM", "SIGTERM") or number. */
int		str2sig(const char *str, int *signum);

/* <unistd.h>: a pipe with O_CLOEXEC and O_NONBLOCK. */
int		pipe2(int fds[2], int flags);

__END_DECLS

/*
 * <fcntl.h>: Darwin has no O_PATH; a FreeBSD program that opens a directory
 * with it to keep a descriptor for openat gets a read-only one instead.
 */
#ifndef O_PATH
#define	O_PATH		0
#endif

/* <signal.h>: how many entries sys_signame and sys_siglist have. */
#ifndef sys_nsig
#define	sys_nsig	NSIG
#endif

/* <sys/cdefs.h>: a place the program cannot reach. */
#ifndef __assert_unreachable
#define	__assert_unreachable()	__builtin_unreachable()
#endif

/*
 * <net/route.h>: the room a sockaddr takes in a routing message. XNU pads
 * each to a multiple of sizeof(uint32_t) (FreeBSD: of sizeof(long)); a
 * zero length still takes one slot.
 */
#ifndef SA_SIZE
#define	SA_SIZE(sa)							\
	(((struct sockaddr *)(sa))->sa_len == 0 ? sizeof(uint32_t) :	\
	 (1 + ((((struct sockaddr *)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1))))
#endif

#endif /* !_MINKAR_BSDCOMPAT_H */
