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

/*
 * dhcpclient: IP configuration of the Ethernet interfaces, started by
 * launchd. main.c supervises one worker process per interface (and starts
 * them as interfaces appear); the worker (iface.c) runs an event loop for
 * its interface: DHCPv4/BOOTP/INFORM or a manual address (dhcp4.c, with an
 * ARP check of the address), IPv6 (ipv6.c: link-local, SLAAC, router
 * advertisements and their RDNSS servers; dhcp6.c: DHCPv6), the state it
 * publishes to configd (store.c) or, without configd, resolv.conf and the
 * default route, and the control socket ipconfig(8) uses ("ipconfig set").
 */

#ifndef _DHCPCLIENT_H
#define _DHCPCLIENT_H

#include <sys/types.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <syslog.h>
#include <time.h>

#define RUN_DIR		"/var/run/dhcpclient"		/* <if>.lease, <if>.dhcp6, <if>.ctl */
#define SAVED_DIR	"/var/db/dhcpclient/leases"	/* <if>: the last lease, for INIT-REBOOT */
#define RESOLV_CONF	"/var/run/resolv.conf"

#define MAX_DNS		4

/* How IPv4 and IPv6 are configured ("ipconfig set"). */
enum method4 { M4_NONE, M4_DHCP, M4_BOOTP, M4_MANUAL, M4_INFORM };
enum method6 { M6_NONE, M6_AUTOMATIC, M6_LINKLOCAL, M6_MANUAL };

/* An IPv4 configuration: a DHCP lease, or a manual address and DHCP's options. */
struct lease {
	struct in_addr	addr, mask, router, server, broadcast;
	struct in_addr	dns[MAX_DNS];
	int		ndns;
	char		domain[256];
	uint32_t	lease_time, t1, t2;	/* seconds; lease_time 0: infinite */
	uint16_t	mtu;
	time_t		start;			/* monotonic time it was granted */
};

/* The interface a worker serves, and everything it knows. */
struct ifstate {
	char		name[IFNAMSIZ];
	unsigned int	index;
	uint8_t		mac[ETHER_ADDR_LEN];

	int		method4;
	struct in_addr	manual_addr, manual_mask;
	int		method6;
	struct in6_addr	manual6;
	int		manual6_prefix;

	/* IPv4: what is configured, and the DHCP exchange in progress. */
	struct lease	current;		/* addr 0: nothing configured */
	struct in_addr	route;			/* the default route we added, 0 if none */
	int		bpf;			/* DHCP replies */
	int		state4;
	time_t		next4;			/* monotonic deadline, 0: none */
	int		tries4;
	uint32_t	xid;
	time_t		started4;
	struct lease	offer;
	int		arp;			/* BPF for the ARP check, -1 if none */
	struct in_addr	probe;
	int		probes;

	/* IPv6: router advertisements and DHCPv6. */
	int		ra_sock;
	bool		ra_managed, ra_other;	/* the M and O flags last seen */
	struct in6_addr	router6;		/* the last router that advertised, :: if none */
	struct in6_addr	rdnss[MAX_DNS];
	int		nrdnss;
	int		sock6;
	int		state6;
	time_t		next6;
	int		tries6;
	uint32_t	xid6;
	time_t		started6;
	uint8_t		server_duid[130];
	size_t		server_duid_len;
	struct in6_addr	addr6;			/* DHCPv6 address, :: if none */
	uint32_t	t1_6, t2_6, preferred6, valid6;
	time_t		bound6;
	struct in6_addr	dns6[MAX_DNS];
	int		ndns6;
	char		domain6[256];

	int		ctl;			/* control socket */
};

extern struct ifstate ifs;
extern bool debug;

/* iface.c */
void logmsg(int pri, const char *fmt, ...) __printflike(2, 3);
time_t now(void);
time_t wallclock(void);
void serve(const char *name) __dead2;
int set_address(const struct lease *l);
void delete_address(struct in_addr addr);
int default_route(int cmd, struct in_addr gateway);
void publish_state(void);	/* to configd, or resolv.conf */
void write_runtime_file(const char *suffix, const char *text);
void apply_lease(const struct lease *l);
void drop_lease(void);

/* dhcp4.c */
int open_bpf(int *fd, bool arp);
void dhcp4_start(void);
void dhcp4_stop(void);
void dhcp4_input(void);
void dhcp4_timeout(void);
void dhcp4_link_up(void);
void arp_input(void);

/* ipv6.c */
void ipv6_start(void);
void ipv6_stop(void);
void ra_input(void);
int set_address6(const struct in6_addr *addr, int prefixlen, uint32_t valid, uint32_t preferred);
void delete_address6(const struct in6_addr *addr);

/* store.c */
bool store_active(void);
bool store_publish(void);

/* dhcp6.c */
void dhcp6_update(void);	/* after an RA: start, change or stop DHCPv6 */
void dhcp6_stop(void);
void dhcp6_input(void);
void dhcp6_timeout(void);

#endif /* !_DHCPCLIENT_H */
