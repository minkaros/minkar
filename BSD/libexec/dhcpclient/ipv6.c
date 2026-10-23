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
 * IPv6, as macOS's IPConfiguration does it: AUTOMATIC-V6 attaches IPv6,
 * starts the link-local address and stateless autoconfiguration
 * (SIOCAUTOCONF_START), sends the router solicitations the kernel leaves
 * to user space, reads router advertisements for their RDNSS name servers
 * (RFC 8106) and M/O flags (which start DHCPv6), LINKLOCAL-V6 stops at the
 * link-local address, MANUAL-V6 adds an address, NONE-V6 detaches IPv6.
 */

#define __APPLE_USE_RFC_3542	/* IPV6_RECVPKTINFO */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpclient.h"

static int
socket6(void)
{
	int s = socket(AF_INET6, SOCK_DGRAM, 0);

	if (s < 0)
		logmsg(LOG_ERR, "IPv6: socket: %s", strerror(errno));
	return s;
}

static void
prefix_mask(struct sockaddr_in6 *sin6, int prefixlen)
{
	memset(sin6, 0, sizeof(*sin6));
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_family = AF_INET6;
	for (int i = 0; i < 16 && prefixlen > 0; i++, prefixlen -= 8)
		sin6->sin6_addr.s6_addr[i] = prefixlen >= 8 ? 0xff :
		    (uint8_t)(0xff << (8 - prefixlen));
}

/* Lifetimes in seconds; ND6_INFINITE_LIFETIME for none. */
int
set_address6(const struct in6_addr *addr, int prefixlen, uint32_t valid, uint32_t preferred)
{
	struct in6_aliasreq ifra;
	int s = socket6(), ret;

	if (s < 0)
		return -1;
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifs.name, sizeof(ifra.ifra_name));
	ifra.ifra_addr.sin6_len = sizeof(ifra.ifra_addr);
	ifra.ifra_addr.sin6_family = AF_INET6;
	ifra.ifra_addr.sin6_addr = *addr;
	prefix_mask(&ifra.ifra_prefixmask, prefixlen);
	ifra.ifra_lifetime.ia6t_vltime = valid;
	ifra.ifra_lifetime.ia6t_pltime = preferred;
	ret = ioctl(s, SIOCAIFADDR_IN6, &ifra);
	if (ret < 0)
		logmsg(LOG_ERR, "SIOCAIFADDR_IN6: %s", strerror(errno));
	close(s);
	return ret;
}

void
delete_address6(const struct in6_addr *addr)
{
	struct in6_ifreq ifr;
	int s = socket6();

	if (s < 0)
		return;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifs.name, sizeof(ifr.ifr_name));
	ifr.ifr_addr.sin6_len = sizeof(ifr.ifr_addr);
	ifr.ifr_addr.sin6_family = AF_INET6;
	ifr.ifr_addr.sin6_addr = *addr;
	(void)ioctl(s, SIOCDIFADDR_IN6, &ifr);
	close(s);
}

/*
 * Router solicitations (RFC 4861 6.3.7): a child sends three, four seconds
 * apart, so the event loop does not wait for them.
 */
static void
solicit_routers(void)
{
	struct {
		struct nd_router_solicit	rs;
		struct nd_opt_hdr		opt;	/* source link-layer address */
		uint8_t				lladdr[ETHER_ADDR_LEN];
	} __attribute__((packed)) msg;
	struct sockaddr_in6 to;
	int s, hops = 255;

	if (fork() != 0)
		return;
	if ((s = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
		_exit(1);
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifs.index, sizeof(ifs.index));
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));
	memset(&msg, 0, sizeof(msg));
	msg.rs.nd_rs_type = ND_ROUTER_SOLICIT;
	msg.opt.nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	msg.opt.nd_opt_len = 1;		/* in units of 8 bytes */
	memcpy(msg.lladdr, ifs.mac, ETHER_ADDR_LEN);
	memset(&to, 0, sizeof(to));
	to.sin6_len = sizeof(to);
	to.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "ff02::2", &to.sin6_addr);	/* all routers */
	to.sin6_scope_id = ifs.index;
	for (int i = 0; i < 3; i++) {
		sleep(i == 0 ? 1 : 4);	/* the link-local address first */
		(void)sendto(s, &msg, sizeof(msg), 0, (struct sockaddr *)&to, sizeof(to));
	}
	_exit(0);
}

static void
open_ra_socket(void)
{
	struct icmp6_filter filter;
	int on = 1;

	if (ifs.ra_sock >= 0)
		return;
	if ((ifs.ra_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		logmsg(LOG_ERR, "IPv6: ICMPv6 socket: %s", strerror(errno));
		return;
	}
	ICMP6_FILTER_SETBLOCKALL(&filter);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filter);
	setsockopt(ifs.ra_sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filter, sizeof(filter));
	setsockopt(ifs.ra_sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
}

void
ipv6_start(void)
{
	struct in6_aliasreq ifra;
	struct in6_ifreq ifr6;
	int s;

	if (ifs.method6 == M6_NONE)
		return;
	if ((s = socket6()) < 0)
		return;
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifs.name, sizeof(ifra.ifra_name));
	if (ioctl(s, SIOCPROTOATTACH_IN6, &ifra) < 0 && errno != EEXIST)
		logmsg(LOG_ERR, "IPv6: SIOCPROTOATTACH_IN6: %s", strerror(errno));
	/* No address given: the kernel makes the link-local one. */
	if (ioctl(s, SIOCLL_START, &ifra) < 0 && errno != EEXIST)
		logmsg(LOG_ERR, "IPv6: SIOCLL_START: %s", strerror(errno));
	memset(&ifr6, 0, sizeof(ifr6));
	strlcpy(ifr6.ifr_name, ifs.name, sizeof(ifr6.ifr_name));
	if (ifs.method6 == M6_AUTOMATIC) {
		if (ioctl(s, SIOCAUTOCONF_START, &ifr6) < 0)
			logmsg(LOG_ERR, "IPv6: SIOCAUTOCONF_START: %s", strerror(errno));
		open_ra_socket();
		solicit_routers();
	} else {
		(void)ioctl(s, SIOCAUTOCONF_STOP, &ifr6);
	}
	close(s);
	if (ifs.method6 == M6_MANUAL)
		set_address6(&ifs.manual6, ifs.manual6_prefix, ND6_INFINITE_LIFETIME,
		    ND6_INFINITE_LIFETIME);
}

void
ipv6_stop(void)
{
	struct in6_ifreq ifr6;
	int s;

	dhcp6_stop();
	if (ifs.ra_sock >= 0) {
		close(ifs.ra_sock);
		ifs.ra_sock = -1;
	}
	ifs.ra_managed = ifs.ra_other = false;
	if (ifs.nrdnss != 0) {
		ifs.nrdnss = 0;
		write_resolv_conf();
	}
	if (ifs.method6 == M6_MANUAL)
		delete_address6(&ifs.manual6);
	if ((s = socket6()) < 0)
		return;
	memset(&ifr6, 0, sizeof(ifr6));
	strlcpy(ifr6.ifr_name, ifs.name, sizeof(ifr6.ifr_name));
	(void)ioctl(s, SIOCAUTOCONF_STOP, &ifr6);
	(void)ioctl(s, SIOCLL_STOP, &ifr6);
	(void)ioctl(s, SIOCPROTODETACH_IN6, &ifr6);
	close(s);
}

/*
 * One router advertisement: on our interface, its RDNSS options replace
 * the IPv6 name servers (lifetime 0 removes them), and its M and O flags
 * decide DHCPv6 (stateful with M, stateless with O alone).
 */
void
ra_input(void)
{
	uint8_t buf[1500], cbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	struct iovec iov = { buf, sizeof(buf) };
	struct sockaddr_in6 from;
	struct msghdr msg;
	struct cmsghdr *cm;
	struct in6_addr found[MAX_DNS];
	struct nd_router_advert *ra = (struct nd_router_advert *)(void *)buf;
	unsigned int index = 0;
	int nfound = 0;
	bool seen = false, managed, other;
	ssize_t n;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	if ((n = recvmsg(ifs.ra_sock, &msg, 0)) < (ssize_t)sizeof(struct nd_router_advert))
		return;
	for (cm = CMSG_FIRSTHDR(&msg); cm != NULL; cm = CMSG_NXTHDR(&msg, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 && cm->cmsg_type == IPV6_PKTINFO) {
			struct in6_pktinfo pi;

			memcpy(&pi, CMSG_DATA(cm), sizeof(pi));
			index = pi.ipi6_ifindex;
		}
	}
	if (index != ifs.index || ra->nd_ra_type != ND_ROUTER_ADVERT)
		return;

	for (ssize_t off = sizeof(struct nd_router_advert); off + 8 <= n;) {
		const uint8_t *opt = buf + off;
		size_t len = (size_t)opt[1] * 8;

		if (len == 0 || off + (ssize_t)len > n)
			break;
		if (opt[0] == 25 && len >= 24) {	/* RDNSS */
			uint32_t lifetime = (uint32_t)opt[4] << 24 | (uint32_t)opt[5] << 16 |
			    (uint32_t)opt[6] << 8 | opt[7];

			seen = true;
			for (size_t a = 8; a + 16 <= len && lifetime != 0 && nfound < MAX_DNS; a += 16)
				memcpy(&found[nfound++], opt + a, 16);
		}
		off += (ssize_t)len;
	}
	if (seen && (nfound != ifs.nrdnss ||
	    memcmp(found, ifs.rdnss, sizeof(found[0]) * (size_t)nfound) != 0)) {
		memcpy(ifs.rdnss, found, sizeof(found[0]) * (size_t)nfound);
		ifs.nrdnss = nfound;
		logmsg(LOG_NOTICE, "%d IPv6 name server(s) from router advertisements", nfound);
		write_resolv_conf();
	}

	managed = (ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) != 0;
	other = (ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) != 0;
	if (managed != ifs.ra_managed || other != ifs.ra_other) {
		ifs.ra_managed = managed;
		ifs.ra_other = other;
		dhcp6_update();
	}
}
