/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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

/* Casper's DNS service for Minkar: no capability mode, the resolver directly. */

#ifndef _RAVYNOS_COMPAT_CAP_DNS_H
#define _RAVYNOS_COMPAT_CAP_DNS_H

#include <sys/socket.h>
#include <netdb.h>
#include <libcasper.h>

static inline struct hostent *
cap_gethostbyname(cap_channel_t *c, const char *name)
{
	(void)c;
	return (gethostbyname(name));
}

static inline struct hostent *
cap_gethostbyname2(cap_channel_t *c, const char *name, int af)
{
	(void)c;
	return (gethostbyname2(name, af));
}

static inline struct hostent *
cap_gethostbyaddr(cap_channel_t *c, const void *addr, socklen_t len, int af)
{
	(void)c;
	return (gethostbyaddr(addr, len, af));
}

static inline int
cap_getaddrinfo(cap_channel_t *c, const char *host, const char *serv,
    const struct addrinfo *hints, struct addrinfo **res)
{
	(void)c;
	return (getaddrinfo(host, serv, hints, res));
}

static inline int
cap_getnameinfo(cap_channel_t *c, const struct sockaddr *sa, socklen_t salen,
    char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
	(void)c;
	return (getnameinfo(sa, salen, host, (socklen_t)hostlen, serv,
	    (socklen_t)servlen, flags));
}

static inline int
cap_dns_type_limit(cap_channel_t *c, const char * const *types, size_t n)
{
	(void)c; (void)types; (void)n;
	return (0);
}

static inline int
cap_dns_family_limit(cap_channel_t *c, const int *families, size_t n)
{
	(void)c; (void)families; (void)n;
	return (0);
}

#endif /* !_RAVYNOS_COMPAT_CAP_DNS_H */
