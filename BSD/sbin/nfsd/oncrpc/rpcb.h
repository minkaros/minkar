/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * <oncrpc/rpcb.h>: registering services with rpcbind (RFC 1833, version 3),
 * as Apple's oncrpc does: by transport name ("udp", "tcp6"...) and address.
 */

#ifndef _MINKAR_ONCRPC_RPCB_H_
#define _MINKAR_ONCRPC_RPCB_H_

#include <oncrpc/rpc.h>

#define RPCBPROG		100000
#define RPCBVERS		3
#define RPCBVERS4		4
#define RPCBPROC_NULL		0
#define RPCBPROC_SET		1
#define RPCBPROC_UNSET		2
#define RPCBPROC_GETADDR	3
#define RPCBPROC_DUMP		4
#define RPCBPROC_GETTIME	6
#define RPCBPROC_GETVERSADDR	9

/* A mapping: program, version, transport, universal address, owner. */
struct rpcb {
	unsigned int	 r_prog;
	unsigned int	 r_vers;
	char		*r_netid;
	char		*r_addr;
	char		*r_owner;
};
typedef struct rpcb RPCB;

__BEGIN_DECLS
bool_t xdr_rpcb(XDR *, RPCB *);
/* "a.b.c.d.p1.p2" (IPv4), "x:...:y.p1.p2" (IPv6), a path (local). */
char *rpcb_sa2uaddr(const struct sockaddr *);
bool_t rpcb_set(const char *, unsigned int, unsigned int,
    const struct sockaddr *);
/* A NULL transport unsets every one. */
bool_t rpcb_unset(const char *, unsigned int, unsigned int);
__END_DECLS

#endif /* _MINKAR_ONCRPC_RPCB_H_ */
