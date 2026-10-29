/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minkar: XNU lacks FreeBSD's multiple FIBs: set_sofib() accepts only
 * FIB 0, and SO_SETFIB is -1 so any other use of it fails.
 * SA_SIZE, the room a sockaddr takes in a routing message, is in
 * libbsdcompat's bsdcompat.h.
 */
#ifndef ROUTE_COMPAT_H
#define ROUTE_COMPAT_H
#include <sys/socket.h>
#ifndef SO_SETFIB
#define SO_SETFIB	(-1)
#endif
#endif
