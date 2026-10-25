/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Minkar: values nc needs that XNU's headers lack. FreeBSD-only socket
 * options are -1, so setsockopt() fails with an error when the option
 * that asks for them is used (-V, --lb, -M, -S) and nc reports it.
 * The DSCP code points are the standard ones (RFC 2474, RFC 2597,
 * RFC 3246), shifted into the TOS byte.
 */
#ifndef NC_COMPAT_H
#define NC_COMPAT_H

#include <netinet/in.h>
#include <netinet/tcp.h>

#ifndef SO_SETFIB
#define SO_SETFIB	(-1)
#endif
#ifndef IP_BINDANY
#define IP_BINDANY	(-1)
#endif
#ifndef SO_REUSEPORT_LB
#define SO_REUSEPORT_LB	(-1)
#endif
#ifndef TCP_STATS
#define TCP_STATS	(-1)
#endif
#ifndef TCP_MD5SIG
#define TCP_MD5SIG	(-1)
#endif

/* Class selectors: CSn = n << 3 (DSCP), << 2 into the TOS byte. */
#ifndef IPTOS_DSCP_CS0
#define IPTOS_DSCP_CS0	0x00
#define IPTOS_DSCP_CS1	0x20
#define IPTOS_DSCP_CS2	0x40
#define IPTOS_DSCP_CS3	0x60
#define IPTOS_DSCP_CS4	0x80
#define IPTOS_DSCP_CS5	0xa0
#define IPTOS_DSCP_CS6	0xc0
#define IPTOS_DSCP_CS7	0xe0
#endif
/* Assured forwarding AFxy: DSCP = 8x + 2y. */
#ifndef IPTOS_DSCP_AF11
#define IPTOS_DSCP_AF11	0x28
#define IPTOS_DSCP_AF12	0x30
#define IPTOS_DSCP_AF13	0x38
#define IPTOS_DSCP_AF21	0x48
#define IPTOS_DSCP_AF22	0x50
#define IPTOS_DSCP_AF23	0x58
#define IPTOS_DSCP_AF31	0x68
#define IPTOS_DSCP_AF32	0x70
#define IPTOS_DSCP_AF33	0x78
#define IPTOS_DSCP_AF41	0x88
#define IPTOS_DSCP_AF42	0x90
#define IPTOS_DSCP_AF43	0x98
#endif
/* Expedited forwarding: DSCP 46. */
#ifndef IPTOS_DSCP_EF
#define IPTOS_DSCP_EF	0xb8
#endif

#endif /* NC_COMPAT_H */
