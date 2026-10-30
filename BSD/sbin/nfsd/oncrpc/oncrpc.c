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
 * What Apple's NFS daemons take from macOS's oncrpc beyond the Sun RPC in
 * libinfo: registering with rpcbind by transport name and address (RFC
 * 1833 version 3 SET and UNSET, to the local rpcbind, started through
 * launchd when it is not running yet), the caller's address of any family,
 * and the local transports (not available here).
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <oncrpc/rpc.h>
#include <oncrpc/rpcb.h>

extern char **environ;

#define RPCBIND_LABEL	"org.minkar.rpcbind"
#define MAX_STRING	1024

bool_t
xdr_rpcb(XDR *x, RPCB *r)
{
	return (xdr_u_int32_t(x, &r->r_prog) &&
	    xdr_u_int32_t(x, &r->r_vers) &&
	    xdr_string(x, &r->r_netid, MAX_STRING) &&
	    xdr_string(x, &r->r_addr, MAX_STRING) &&
	    xdr_string(x, &r->r_owner, MAX_STRING));
}

char *
rpcb_sa2uaddr(const struct sockaddr *sa)
{
	char host[INET6_ADDRSTRLEN], *s = NULL;
	unsigned int port;

	if (sa == NULL)
		return (strdup(""));
	switch (sa->sa_family) {
	case AF_INET: {
		const struct sockaddr_in *in = (const struct sockaddr_in *)
		    (const void *)sa;

		inet_ntop(AF_INET, &in->sin_addr, host, sizeof(host));
		port = ntohs(in->sin_port);
		break;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *in6 = (const struct sockaddr_in6 *)
		    (const void *)sa;

		inet_ntop(AF_INET6, &in6->sin6_addr, host, sizeof(host));
		port = ntohs(in6->sin6_port);
		break;
	}
	case AF_UNIX:
		return (strdup(((const struct sockaddr_un *)(const void *)
		    sa)->sun_path));
	default:
		return (NULL);
	}
	if (asprintf(&s, "%s.%u.%u", host, port >> 8, port & 0xff) < 0)
		return (NULL);
	return (s);
}

/* rpcbind is started by launchd on demand: once, when not answering. */
static void
start_rpcbind(void)
{
	const char *argv[] = { "/bin/launchctl", "start", RPCBIND_LABEL, NULL };
	pid_t pid;
	int status;

	if (posix_spawn(&pid, argv[0], NULL, NULL, (char *const *)argv,
	    environ) == 0)
		waitpid(pid, &status, 0);
	usleep(500000);
}

static CLIENT *
local_rpcbind(void)
{
	struct sockaddr_in sin;
	struct timeval wait = { 3, 0 };
	CLIENT *c;
	int sock = RPC_ANYSOCK, tries;

	for (tries = 0; tries < 2; tries++) {
		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		sin.sin_port = htons(111);
		sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		sock = RPC_ANYSOCK;
		c = clntudp_create(&sin, RPCBPROG, RPCBVERS, wait, &sock);
		if (c != NULL) {
			enum clnt_stat st;
			struct timeval t = { 2, 0 };

			st = clnt_call(c, RPCBPROC_NULL, (xdrproc_t)xdr_void,
			    NULL, (xdrproc_t)xdr_void, NULL, t);
			if (st == RPC_SUCCESS)
				return (c);
			clnt_destroy(c);
		}
		if (tries == 0)
			start_rpcbind();
	}
	return (NULL);
}

static bool_t
rpcb_call(int proc, const char *netid, unsigned int prog, unsigned int vers,
    const struct sockaddr *sa)
{
	CLIENT *c = local_rpcbind();
	struct timeval t = { 5, 0 };
	char owner[32];
	bool_t ok = FALSE;
	RPCB r;

	if (c == NULL)
		return (FALSE);
	snprintf(owner, sizeof(owner), "%u", (unsigned)geteuid());
	r.r_prog = prog;
	r.r_vers = vers;
	r.r_netid = (char *)(netid ? netid : "");
	r.r_addr = rpcb_sa2uaddr(sa);
	r.r_owner = owner;
	if (r.r_addr == NULL) {
		clnt_destroy(c);
		return (FALSE);
	}
	if (clnt_call(c, proc, (xdrproc_t)xdr_rpcb, (caddr_t)&r,
	    (xdrproc_t)xdr_bool, (caddr_t)&ok, t) != RPC_SUCCESS)
		ok = FALSE;
	free(r.r_addr);
	clnt_destroy(c);
	return (ok);
}

bool_t
rpcb_set(const char *netid, unsigned int prog, unsigned int vers,
    const struct sockaddr *sa)
{
	if (netid == NULL || sa == NULL)
		return (FALSE);
	return (rpcb_call(RPCBPROC_SET, netid, prog, vers, sa));
}

bool_t
rpcb_unset(const char *netid, unsigned int prog, unsigned int vers)
{
	return (rpcb_call(RPCBPROC_UNSET, netid, prog, vers, NULL));
}

struct sockaddr *
svc_getcaller_sa(SVCXPRT *x)
{
	return ((struct sockaddr *)(void *)svc_getcaller(x));
}

SVCXPRT *
svcticlts_create(int fd)
{
	(void)fd;
	errno = EPFNOSUPPORT;
	return (NULL);
}

SVCXPRT *
svcticotsord_create(int fd, unsigned int sendsz, unsigned int recvsz)
{
	(void)fd;
	(void)sendsz;
	(void)recvsz;
	errno = EPFNOSUPPORT;
	return (NULL);
}

bool_t
rpc_control(int op, void *arg)
{
	(void)op;
	(void)arg;
	return (TRUE);
}
