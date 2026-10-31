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
 * rpcbind: the ONC RPC binder (program 100000), on UDP and TCP port 111.
 * It maps RPC programs and versions to where they listen: the portmapper
 * protocol (version 2: SET, UNSET, GETPORT, DUMP), which NFS clients ask
 * over IPv4, and rpcbind's (RFC 1833 versions 3 and 4: SET, UNSET,
 * GETADDR, GETVERSADDR, DUMP, GETTIME), with transports named ("udp",
 * "tcp6"...) and addresses universal ("h1.h2.h3.h4.p1.p2"). Services
 * register and unregister from this machine only. Indirect calls
 * (CALLIT) are not made.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>

#define RPCBPROG		100000
#define RPCBVERS		3
#define RPCBVERS4		4
#define RPCBPROC_SET		1
#define RPCBPROC_UNSET		2
#define RPCBPROC_GETADDR	3
#define RPCBPROC_DUMP		4
#define RPCBPROC_CALLIT		5
#define RPCBPROC_GETTIME	6
#define RPCBPROC_GETVERSADDR	9
#define MAX_STRING		1024

struct rpcb {
	unsigned int	 r_prog, r_vers;
	char		*r_netid, *r_addr, *r_owner;
};

struct mapping {
	unsigned int	 prog, vers;
	char		 netid[32];
	char		 addr[128];
	char		 owner[32];
	struct mapping	*next;
};

static struct mapping *mappings;
static bool debug;

static bool_t
xdr_rpcb(XDR *x, struct rpcb *r)
{
	return (xdr_u_int32_t(x, &r->r_prog) &&
	    xdr_u_int32_t(x, &r->r_vers) &&
	    xdr_string(x, &r->r_netid, MAX_STRING) &&
	    xdr_string(x, &r->r_addr, MAX_STRING) &&
	    xdr_string(x, &r->r_owner, MAX_STRING));
}

/* The DUMP reply: a list of mappings, each preceded by "one more". */
static bool_t
xdr_rpcblist(XDR *x, void *unused)
{
	struct mapping *m;
	bool_t more;

	(void)unused;
	for (m = mappings; m != NULL; m = m->next) {
		struct rpcb r = { m->prog, m->vers, m->netid, m->addr,
		    m->owner };

		more = TRUE;
		if (!xdr_bool(x, &more) || !xdr_rpcb(x, &r))
			return (FALSE);
	}
	more = FALSE;
	return (xdr_bool(x, &more));
}

static bool_t
xdr_pmaplist_out(XDR *x, void *unused)
{
	struct mapping *m;
	bool_t more;

	(void)unused;
	for (m = mappings; m != NULL; m = m->next) {
		struct pmap p;
		unsigned int h1, h2, h3, h4, p1, p2;

		if (strcmp(m->netid, "udp") != 0 && strcmp(m->netid, "tcp") != 0)
			continue;
		if (sscanf(m->addr, "%u.%u.%u.%u.%u.%u", &h1, &h2, &h3, &h4,
		    &p1, &p2) != 6)
			continue;
		p.pm_prog = m->prog;
		p.pm_vers = m->vers;
		p.pm_prot = strcmp(m->netid, "udp") == 0 ? IPPROTO_UDP :
		    IPPROTO_TCP;
		p.pm_port = p1 << 8 | p2;
		more = TRUE;
		if (!xdr_bool(x, &more) || !xdr_pmap(x, &p))
			return (FALSE);
	}
	more = FALSE;
	return (xdr_bool(x, &more));
}

/* ---- The table ------------------------------------------------------------- */

static struct mapping *
find(unsigned int prog, unsigned int vers, const char *netid)
{
	struct mapping *m;

	for (m = mappings; m != NULL; m = m->next)
		if (m->prog == prog && m->vers == vers &&
		    (netid == NULL || strcmp(m->netid, netid) == 0))
			return (m);
	return (NULL);
}

static bool
add(unsigned int prog, unsigned int vers, const char *netid, const char *addr,
    const char *owner)
{
	struct mapping *m;

	if (netid[0] == '\0' || addr[0] == '\0' || find(prog, vers, netid))
		return (false);
	if ((m = calloc(1, sizeof(*m))) == NULL)
		return (false);
	m->prog = prog;
	m->vers = vers;
	strlcpy(m->netid, netid, sizeof(m->netid));
	strlcpy(m->addr, addr, sizeof(m->addr));
	strlcpy(m->owner, owner, sizeof(m->owner));
	m->next = mappings;
	mappings = m;
	if (debug)
		syslog(LOG_DEBUG, "set %u.%u %s %s", prog, vers, netid, addr);
	return (true);
}

/* Every transport when netid is empty. */
static bool
del(unsigned int prog, unsigned int vers, const char *netid)
{
	struct mapping **pp = &mappings, *m;
	bool any = false;

	while ((m = *pp) != NULL) {
		if (m->prog == prog && m->vers == vers &&
		    (netid[0] == '\0' || strcmp(m->netid, netid) == 0)) {
			*pp = m->next;
			free(m);
			any = true;
		} else {
			pp = &m->next;
		}
	}
	return (any);
}

static void
uaddr_v4(char *buf, size_t len, unsigned int port)
{
	snprintf(buf, len, "0.0.0.0.%u.%u", port >> 8, port & 0xff);
}

/* Changes come from this machine only. */
static bool
local_caller(SVCXPRT *xprt)
{
	struct sockaddr_in *sin = svc_getcaller(xprt);

	return (sin != NULL && sin->sin_family == AF_INET &&
	    ntohl(sin->sin_addr.s_addr) >> 24 == IN_LOOPBACKNET);
}

/* ---- The services ------------------------------------------------------------ */

static void
pmap_service(struct svc_req *rq, SVCXPRT *xprt)
{
	struct pmap p;
	char addr[64];
	bool_t ok;
	unsigned int port;
	struct mapping *m;

	switch (rq->rq_proc) {
	case NULLPROC:
		svc_sendreply(xprt, (xdrproc_t)xdr_void, NULL);
		break;
	case PMAPPROC_SET:
	case PMAPPROC_UNSET:
		if (!svc_getargs(xprt, (xdrproc_t)xdr_pmap, (caddr_t)&p)) {
			svcerr_decode(xprt);
			break;
		}
		if (!local_caller(xprt)) {
			ok = FALSE;
		} else if (rq->rq_proc == PMAPPROC_SET) {
			uaddr_v4(addr, sizeof(addr), p.pm_port);
			ok = add(p.pm_prog, p.pm_vers, p.pm_prot == IPPROTO_UDP ?
			    "udp" : "tcp", addr, "superuser");
		} else {
			/* Both transports go. */
			ok = del(p.pm_prog, p.pm_vers, "udp");
			ok = del(p.pm_prog, p.pm_vers, "tcp") || ok;
		}
		svc_sendreply(xprt, (xdrproc_t)xdr_bool, (caddr_t)&ok);
		break;
	case PMAPPROC_GETPORT:
		if (!svc_getargs(xprt, (xdrproc_t)xdr_pmap, (caddr_t)&p)) {
			svcerr_decode(xprt);
			break;
		}
		port = 0;
		m = find(p.pm_prog, p.pm_vers, p.pm_prot == IPPROTO_UDP ? "udp" :
		    "tcp");
		if (m != NULL) {
			unsigned int h1, h2, h3, h4, p1, p2;

			if (sscanf(m->addr, "%u.%u.%u.%u.%u.%u", &h1, &h2, &h3,
			    &h4, &p1, &p2) == 6)
				port = p1 << 8 | p2;
		}
		svc_sendreply(xprt, (xdrproc_t)xdr_u_int, (caddr_t)&port);
		break;
	case PMAPPROC_DUMP:
		svc_sendreply(xprt, (xdrproc_t)xdr_pmaplist_out, NULL);
		break;
	case PMAPPROC_CALLIT:
		break;		/* not made: no reply, as for broadcasts */
	default:
		svcerr_noproc(xprt);
	}
}

static void
rpcb_service(struct svc_req *rq, SVCXPRT *xprt)
{
	struct rpcb r;
	bool_t ok;
	char *addr;
	struct mapping *m;
	unsigned int now;

	switch (rq->rq_proc) {
	case NULLPROC:
		svc_sendreply(xprt, (xdrproc_t)xdr_void, NULL);
		return;
	case RPCBPROC_DUMP:
		svc_sendreply(xprt, (xdrproc_t)xdr_rpcblist, NULL);
		return;
	case RPCBPROC_GETTIME:
		now = (unsigned int)time(NULL);
		svc_sendreply(xprt, (xdrproc_t)xdr_u_int, (caddr_t)&now);
		return;
	case RPCBPROC_CALLIT:
		return;
	case RPCBPROC_SET:
	case RPCBPROC_UNSET:
	case RPCBPROC_GETADDR:
	case RPCBPROC_GETVERSADDR:
		break;
	default:
		svcerr_noproc(xprt);
		return;
	}
	if (rq->rq_proc == RPCBPROC_GETVERSADDR && rq->rq_vers != RPCBVERS4) {
		svcerr_noproc(xprt);
		return;
	}
	memset(&r, 0, sizeof(r));
	if (!svc_getargs(xprt, (xdrproc_t)xdr_rpcb, (caddr_t)&r)) {
		svcerr_decode(xprt);
		return;
	}
	switch (rq->rq_proc) {
	case RPCBPROC_SET:
		ok = local_caller(xprt) && add(r.r_prog, r.r_vers, r.r_netid,
		    r.r_addr, r.r_owner);
		svc_sendreply(xprt, (xdrproc_t)xdr_bool, (caddr_t)&ok);
		break;
	case RPCBPROC_UNSET:
		ok = local_caller(xprt) && del(r.r_prog, r.r_vers, r.r_netid);
		svc_sendreply(xprt, (xdrproc_t)xdr_bool, (caddr_t)&ok);
		break;
	default:	/* GETADDR, GETVERSADDR */
		m = find(r.r_prog, r.r_vers, r.r_netid[0] ? r.r_netid : NULL);
		addr = m != NULL ? m->addr : "";
		svc_sendreply(xprt, (xdrproc_t)xdr_wrapstring, (caddr_t)&addr);
		break;
	}
	svc_freeargs(xprt, (xdrproc_t)xdr_rpcb, (caddr_t)&r);
}

static void
dispatch(struct svc_req *rq, SVCXPRT *xprt)
{
	if (rq->rq_vers == PMAPVERS)
		pmap_service(rq, xprt);
	else
		rpcb_service(rq, xprt);
}

static int
bound_socket(int type)
{
	struct sockaddr_in sin;
	int s = socket(AF_INET, type, 0), on = 1;

	if (s < 0)
		err(1, "socket");
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PMAPPORT);
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) != 0)
		err(1, "bind to port %d", PMAPPORT);
	return (s);
}

static void
usage(void)
{
	fprintf(stderr, "usage: rpcbind [-d]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	SVCXPRT *udp, *tcp;
	char addr[64];
	unsigned int v;
	int ch;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		if (ch == 'd')
			debug = true;
		else
			usage();
	}
	openlog("rpcbind", LOG_PID, LOG_DAEMON);
	signal(SIGPIPE, SIG_IGN);
	udp = svcudp_create(bound_socket(SOCK_DGRAM));
	tcp = svctcp_create(bound_socket(SOCK_STREAM), 0, 0);
	if (udp == NULL || tcp == NULL)
		errx(1, "can't create the RPC services");
	uaddr_v4(addr, sizeof(addr), PMAPPORT);
	for (v = PMAPVERS; v <= RPCBVERS4; v++) {
		/* Protocol 0: registered with no portmapper but ourselves. */
		if (!svc_register(udp, RPCBPROG, v, dispatch, 0) ||
		    !svc_register(tcp, RPCBPROG, v, dispatch, 0))
			errx(1, "can't register version %u", v);
		add(RPCBPROG, v, "udp", addr, "superuser");
		add(RPCBPROG, v, "tcp", addr, "superuser");
	}
	syslog(LOG_NOTICE, "listening on port %d", PMAPPORT);
	svc_run();
	errx(1, "svc_run returned");
}
