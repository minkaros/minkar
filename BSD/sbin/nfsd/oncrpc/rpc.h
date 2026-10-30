/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * <oncrpc/rpc.h>: macOS's ONC RPC headers, over the Sun RPC libinfo carries,
 * with what Apple's NFS daemons take from oncrpc beyond it (oncrpc.c).
 */

#ifndef _MINKAR_ONCRPC_RPC_H_
#define _MINKAR_ONCRPC_RPC_H_

#include <rpc/rpc.h>
#include <sys/socket.h>

__BEGIN_DECLS
/* The caller's address, whatever its family. */
struct sockaddr *svc_getcaller_sa(SVCXPRT *);
/* Local (AF_UNIX) transports: not available; NULL and EPFNOSUPPORT. */
SVCXPRT *svcticlts_create(int);
SVCXPRT *svcticotsord_create(int, unsigned int, unsigned int);
/* TI-RPC's service controls; libinfo's service runs on one thread. */
#define RPC_SVC_MTMODE_SET	1
#define RPC_SVC_MTMODE_GET	2
#define RPC_SVC_THRMAX_SET	3
#define RPC_SVC_THRMAX_GET	4
#define RPC_SVC_MIN_THREADS_SET	5
#define RPC_SVC_MIN_THREADS_GET	6
bool_t rpc_control(int, void *);
__END_DECLS

#endif /* _MINKAR_ONCRPC_RPC_H_ */
