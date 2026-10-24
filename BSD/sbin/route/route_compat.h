/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * ravynOS: XNU lacks FreeBSD's multiple FIBs: set_sofib() accepts only
 * FIB 0, and SO_SETFIB is -1 so any other use of it fails.
 * Routing messages pad sockaddrs as SA_SIZE says.
 */
#ifndef ROUTE_COMPAT_H
#define ROUTE_COMPAT_H
#include <sys/socket.h>
#ifndef SO_SETFIB
#define SO_SETFIB	(-1)
#endif

/*
 * XNU pads each sockaddr in a routing message to a multiple of
 * sizeof(uint32_t); a zero length still takes one slot.
 */
#ifndef SA_SIZE
#define SA_SIZE(sa)						\
	(((struct sockaddr *)(sa))->sa_len == 0 ? sizeof(uint32_t) :	\
	 (1 + ((((struct sockaddr *)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1))))
#endif
#endif
