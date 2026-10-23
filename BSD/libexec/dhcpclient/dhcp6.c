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
 * DHCPv6 (RFC 8415), as router advertisements ask: with the M flag, an
 * address (IA_NA: Solicit, Advertise, Request, Reply, then Renew at T1,
 * Rebind at T2); with the O flag alone, only configuration
 * (Information-Request: name servers and domains). The client's DUID is
 * DUID-LL, made from the MAC address.
 */

#define __APPLE_USE_RFC_3542	/* IPV6_RECVPKTINFO */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpclient.h"

#define CLIENT_PORT6	546
#define SERVER_PORT6	547

enum {
	SOLICIT = 1, ADVERTISE, REQUEST, CONFIRM, RENEW, REBIND, REPLY,
	RELEASE, DECLINE, RECONFIGURE, INFORMATION_REQUEST,
};

enum {
	OPT6_CLIENTID = 1, OPT6_SERVERID = 2, OPT6_IA_NA = 3, OPT6_IAADDR = 5,
	OPT6_ORO = 6, OPT6_ELAPSED_TIME = 8, OPT6_STATUS_CODE = 13,
	OPT6_DNS_SERVERS = 23, OPT6_DOMAIN_LIST = 24, OPT6_INFO_REFRESH_TIME = 32,
};

/* States. */
enum {
	S6_IDLE, S6_SOLICIT, S6_REQUEST, S6_BOUND, S6_RENEW, S6_REBIND,
	S6_INFORM, S6_INFORMED,
};

/* Retransmission (RFC 8415 7.6), in seconds. */
#define SOL_MAX_RT	120
#define REQ_MAX_RC	10
#define REQ_MAX_RT	30
#define REN_MAX_RT	600
#define INF_MAX_RT	120
#define IRT_DEFAULT	86400

static time_t rt;		/* the current retransmission timeout */
static bool stateful;		/* the mode running, if any */

struct reply6 {
	int		type;
	uint8_t		server_duid[130];
	size_t		server_duid_len;
	int		status;		/* top level, or the IA's */
	bool		have_addr;
	struct in6_addr	addr;
	uint32_t	t1, t2, preferred, valid;
	struct in6_addr	dns[MAX_DNS];
	int		ndns;
	char		domain[256];
	uint32_t	refresh;
};

/* ---- The socket --------------------------------------------------------- */

static int
open_socket6(void)
{
	struct sockaddr_in6 sin6;
	int on = 1, hops = 1;

	if (ifs.sock6 >= 0)
		return 0;
	if ((ifs.sock6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		logmsg(LOG_ERR, "DHCPv6: socket: %s", strerror(errno));
		return -1;
	}
	setsockopt(ifs.sock6, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	setsockopt(ifs.sock6, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	setsockopt(ifs.sock6, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
	setsockopt(ifs.sock6, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifs.index, sizeof(ifs.index));
	setsockopt(ifs.sock6, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));
	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(CLIENT_PORT6);
	if (bind(ifs.sock6, (struct sockaddr *)&sin6, sizeof(sin6)) < 0) {
		logmsg(LOG_ERR, "DHCPv6: bind: %s", strerror(errno));
		close(ifs.sock6);
		ifs.sock6 = -1;
		return -1;
	}
	return 0;
}

/* ---- Messages ----------------------------------------------------------- */

static uint8_t *
put_opt(uint8_t *p, uint16_t code, uint16_t len, const void *data)
{
	*p++ = (uint8_t)(code >> 8);
	*p++ = (uint8_t)code;
	*p++ = (uint8_t)(len >> 8);
	*p++ = (uint8_t)len;
	if (len != 0 && data != NULL)
		memcpy(p, data, len);
	return p + len;
}

static void
put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static uint32_t
get32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint16_t
get16(const uint8_t *p)
{
	return (uint16_t)(p[0] << 8 | p[1]);
}

/* Sends type to All_DHCP_Relay_Agents_and_Servers. */
static void
send6(int type)
{
	uint8_t buf[512], *p = buf, duid[10], ia[12 + 4 + 24], elapsed[2];
	static const uint8_t oro[] = { 0, OPT6_DNS_SERVERS, 0, OPT6_DOMAIN_LIST };
	struct sockaddr_in6 to;
	time_t cs = (now() - ifs.started6) * 100;

	*p++ = (uint8_t)type;
	*p++ = (uint8_t)(ifs.xid6 >> 16);
	*p++ = (uint8_t)(ifs.xid6 >> 8);
	*p++ = (uint8_t)ifs.xid6;

	/* DUID-LL: type 3, hardware type 1 (Ethernet), the MAC. */
	duid[0] = 0; duid[1] = 3; duid[2] = 0; duid[3] = 1;
	memcpy(duid + 4, ifs.mac, ETHER_ADDR_LEN);
	p = put_opt(p, OPT6_CLIENTID, sizeof(duid), duid);
	if ((type == REQUEST || type == RENEW || type == RELEASE) && ifs.server_duid_len != 0)
		p = put_opt(p, OPT6_SERVERID, (uint16_t)ifs.server_duid_len, ifs.server_duid);
	elapsed[0] = (uint8_t)((cs > 0xffff ? 0xffff : cs) >> 8);
	elapsed[1] = (uint8_t)(cs > 0xffff ? 0xffff : cs);
	p = put_opt(p, OPT6_ELAPSED_TIME, sizeof(elapsed), elapsed);
	if (type != RELEASE)
		p = put_opt(p, OPT6_ORO, sizeof(oro), oro);
	if (type != INFORMATION_REQUEST) {
		/* IA_NA: IAID (the interface index), T1, T2, [IAADDR]. */
		uint16_t ialen = 12;

		put32(ia, ifs.index);
		put32(ia + 4, 0);
		put32(ia + 8, 0);
		if (type != SOLICIT && !IN6_IS_ADDR_UNSPECIFIED(&ifs.addr6)) {
			uint8_t *a = put_opt(ia + 12, OPT6_IAADDR, 24, NULL) - 24;

			memcpy(a, &ifs.addr6, 16);
			put32(a + 16, 0);
			put32(a + 20, 0);
			ialen += 4 + 24;
		}
		p = put_opt(p, OPT6_IA_NA, ialen, ia);
	}

	memset(&to, 0, sizeof(to));
	to.sin6_len = sizeof(to);
	to.sin6_family = AF_INET6;
	to.sin6_port = htons(SERVER_PORT6);
	to.sin6_scope_id = ifs.index;
	inet_pton(AF_INET6, "ff02::1:2", &to.sin6_addr);
	if (sendto(ifs.sock6, buf, (size_t)(p - buf), 0, (struct sockaddr *)&to, sizeof(to)) < 0)
		logmsg(LOG_ERR, "DHCPv6: send: %s", strerror(errno));
}

/* DNS wire names (RFC 1035 3.1) to "a.b c.d". */
static void
decode_domains(const uint8_t *p, size_t len, char *out, size_t outlen)
{
	size_t o = 0, i = 0;

	out[0] = '\0';
	while (i < len && o + 1 < outlen) {
		uint8_t l = p[i++];

		if (l == 0) {		/* end of a name */
			if (o > 0 && out[o - 1] == '.')
				o--;
			if (i < len && o + 1 < outlen)
				out[o++] = ' ';
			continue;
		}
		if (i + l > len || o + l + 1 >= outlen)
			break;
		memcpy(out + o, p + i, l);
		o += l;
		out[o++] = '.';
		i += l;
	}
	while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '.'))
		o--;
	out[o] = '\0';
}

static bool
parse6(const uint8_t *buf, size_t len, struct reply6 *r)
{
	const uint8_t *p = buf + 4, *end = buf + len;

	memset(r, 0, sizeof(*r));
	if (len < 4)
		return false;
	r->type = buf[0];
	if (((uint32_t)buf[1] << 16 | (uint32_t)buf[2] << 8 | buf[3]) != ifs.xid6)
		return false;
	while (p + 4 <= end) {
		uint16_t code = get16(p), olen = get16(p + 2);
		const uint8_t *v = p + 4;

		if (v + olen > end)
			break;
		switch (code) {
		case OPT6_SERVERID:
			if (olen <= sizeof(r->server_duid)) {
				memcpy(r->server_duid, v, olen);
				r->server_duid_len = olen;
			}
			break;
		case OPT6_STATUS_CODE:
			if (olen >= 2)
				r->status = get16(v);
			break;
		case OPT6_IA_NA:
			if (olen >= 12 && get32(v) == ifs.index) {
				const uint8_t *q = v + 12, *qend = v + olen;

				r->t1 = get32(v + 4);
				r->t2 = get32(v + 8);
				while (q + 4 <= qend) {
					uint16_t c = get16(q), l = get16(q + 2);

					if (q + 4 + l > qend)
						break;
					if (c == OPT6_IAADDR && l >= 24) {
						memcpy(&r->addr, q + 4, 16);
						r->preferred = get32(q + 20);
						r->valid = get32(q + 24);
						r->have_addr = r->valid != 0;
					} else if (c == OPT6_STATUS_CODE && l >= 2 && r->status == 0) {
						r->status = get16(q + 4);
					}
					q += 4 + l;
				}
			}
			break;
		case OPT6_DNS_SERVERS:
			for (uint16_t i = 0; i + 16 <= olen && r->ndns < MAX_DNS; i += 16)
				memcpy(&r->dns[r->ndns++], v + i, 16);
			break;
		case OPT6_DOMAIN_LIST:
			decode_domains(v, olen, r->domain, sizeof(r->domain));
			break;
		case OPT6_INFO_REFRESH_TIME:
			if (olen == 4)
				r->refresh = get32(v);
			break;
		}
		p = v + olen;
	}
	return true;
}

/* ---- Configuration ------------------------------------------------------ */

static void
write_dhcp6_file(void)
{
	char text[1024], buf[INET6_ADDRSTRLEN];
	size_t n = 0;

#define ADD(...) n += (size_t)snprintf(text + n, n < sizeof(text) ? sizeof(text) - n : 0, __VA_ARGS__)
	ADD("mode=%s\n", stateful ? "stateful" : "stateless");
	if (ifs.valid6 != 0) {
		ADD("ip_address=%s\n", inet_ntop(AF_INET6, &ifs.addr6, buf, sizeof(buf)));
		ADD("valid_lifetime=%u\npreferred_lifetime=%u\n", ifs.valid6, ifs.preferred6);
	}
	if (ifs.ndns6 > 0) {
		ADD("domain_name_server=");
		for (int i = 0; i < ifs.ndns6; i++)
			ADD("%s%s", i ? " " : "", inet_ntop(AF_INET6, &ifs.dns6[i], buf, sizeof(buf)));
		ADD("\n");
	}
	if (ifs.domain6[0] != '\0')
		ADD("domain_name=%s\n", ifs.domain6);
#undef ADD
	write_runtime_file("dhcp6", text);
}

static void
set_options(const struct reply6 *r)
{
	bool changed = r->ndns != ifs.ndns6 ||
	    memcmp(r->dns, ifs.dns6, sizeof(r->dns[0]) * (size_t)r->ndns) != 0 ||
	    strcmp(r->domain, ifs.domain6) != 0;

	memcpy(ifs.dns6, r->dns, sizeof(ifs.dns6));
	ifs.ndns6 = r->ndns;
	strlcpy(ifs.domain6, r->domain, sizeof(ifs.domain6));
	if (changed)
		write_resolv_conf();
}

static void
drop_address6(void)
{
	char buf[INET6_ADDRSTRLEN];

	if (ifs.valid6 == 0) {		/* offered, not configured */
		memset(&ifs.addr6, 0, sizeof(ifs.addr6));
		return;
	}
	logmsg(LOG_NOTICE, "DHCPv6: %s removed", inet_ntop(AF_INET6, &ifs.addr6, buf, sizeof(buf)));
	delete_address6(&ifs.addr6);
	memset(&ifs.addr6, 0, sizeof(ifs.addr6));
	ifs.valid6 = 0;
}

static void
bind_reply(const struct reply6 *r)
{
	char buf[INET6_ADDRSTRLEN];
	bool same = ifs.valid6 != 0 && memcmp(&r->addr, &ifs.addr6, sizeof(r->addr)) == 0;

	if (!same && ifs.valid6 != 0)
		drop_address6();
	ifs.addr6 = r->addr;
	ifs.preferred6 = r->preferred;
	ifs.valid6 = r->valid;
	/* T1 and T2 the server leaves to us: 0.5 and 0.8 of the preferred lifetime. */
	ifs.t1_6 = r->t1 != 0 ? r->t1 : r->preferred / 2;
	ifs.t2_6 = r->t2 != 0 ? r->t2 : (uint32_t)((uint64_t)r->preferred * 4 / 5);
	ifs.bound6 = now();
	set_address6(&r->addr, 128, r->valid, r->preferred);
	if (!same)
		logmsg(LOG_NOTICE, "DHCPv6: bound to %s, valid %us",
		    inet_ntop(AF_INET6, &r->addr, buf, sizeof(buf)), r->valid);
	set_options(r);
	write_dhcp6_file();
	ifs.state6 = S6_BOUND;
	ifs.next6 = ifs.t1_6 == 0xffffffff ? 0 : ifs.bound6 + ifs.t1_6;
}

/* ---- The state machine -------------------------------------------------- */

static void
start_exchange(int state, time_t delay)
{
	ifs.state6 = state;
	ifs.xid6 = arc4random() & 0xffffff;
	ifs.started6 = now();
	ifs.tries6 = 0;
	rt = 1;
	ifs.next6 = now() + delay;
}

static time_t
next_rt(time_t max)
{
	time_t r = rt;

	rt = rt * 2 > max ? max : rt * 2;
	return r;
}

void
dhcp6_update(void)
{
	bool want = ifs.method6 == M6_AUTOMATIC && (ifs.ra_managed || ifs.ra_other);
	bool wantStateful = ifs.ra_managed;

	if (!want) {
		dhcp6_stop();
		return;
	}
	if (ifs.state6 != S6_IDLE && stateful == wantStateful)
		return;
	dhcp6_stop();
	if (open_socket6() < 0)
		return;
	stateful = wantStateful;
	logmsg(LOG_INFO, "DHCPv6: %s", stateful ? "address and options (M)" : "options only (O)");
	/* A random delay of up to a second before the first message (RFC 8415 18.2.1). */
	start_exchange(stateful ? S6_SOLICIT : S6_INFORM, arc4random_uniform(2));
}

void
dhcp6_stop(void)
{
	if (ifs.state6 == S6_BOUND || ifs.state6 == S6_RENEW || ifs.state6 == S6_REBIND) {
		ifs.xid6 = arc4random() & 0xffffff;
		send6(RELEASE);
	}
	drop_address6();
	if (ifs.ndns6 != 0 || ifs.domain6[0] != '\0') {
		ifs.ndns6 = 0;
		ifs.domain6[0] = '\0';
		write_resolv_conf();
	}
	write_runtime_file("dhcp6", NULL);
	if (ifs.sock6 >= 0) {
		close(ifs.sock6);
		ifs.sock6 = -1;
	}
	ifs.server_duid_len = 0;
	ifs.state6 = S6_IDLE;
	ifs.next6 = 0;
}

void
dhcp6_timeout(void)
{
	time_t t = now();

	switch (ifs.state6) {
	case S6_SOLICIT:
		send6(SOLICIT);
		ifs.next6 = t + next_rt(SOL_MAX_RT);
		break;
	case S6_REQUEST:
		if (ifs.tries6++ >= REQ_MAX_RC) {
			start_exchange(S6_SOLICIT, 0);
			break;
		}
		send6(REQUEST);
		ifs.next6 = t + next_rt(REQ_MAX_RT);
		break;
	case S6_BOUND:
		start_exchange(S6_RENEW, 0);
		rt = 10;
		/* FALLTHROUGH */
	case S6_RENEW:
	case S6_REBIND: {
		time_t t2 = ifs.bound6 + ifs.t2_6, expiry = ifs.bound6 + ifs.valid6;

		if (t >= expiry) {
			drop_address6();
			start_exchange(S6_SOLICIT, 0);
			break;
		}
		if (t >= t2 && ifs.state6 == S6_RENEW) {
			ifs.state6 = S6_REBIND;
			ifs.server_duid_len = 0;	/* any server */
		}
		send6(ifs.state6 == S6_RENEW ? RENEW : REBIND);
		ifs.next6 = t + next_rt(REN_MAX_RT);
		if (ifs.state6 == S6_RENEW && ifs.next6 > t2)
			ifs.next6 = t2;
		if (ifs.next6 > expiry)
			ifs.next6 = expiry;
		break;
	}
	case S6_INFORM:
		send6(INFORMATION_REQUEST);
		ifs.next6 = t + next_rt(INF_MAX_RT);
		break;
	case S6_INFORMED:
		start_exchange(S6_INFORM, 0);
		break;
	default:
		ifs.next6 = 0;
		break;
	}
}

void
dhcp6_input(void)
{
	uint8_t buf[2048], cbuf[CMSG_SPACE(sizeof(struct in6_pktinfo))];
	struct iovec iov = { buf, sizeof(buf) };
	struct sockaddr_in6 from;
	struct msghdr msg;
	struct cmsghdr *cm;
	struct reply6 r;
	unsigned int index = 0;
	ssize_t n;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	if ((n = recvmsg(ifs.sock6, &msg, 0)) <= 0)
		return;
	for (cm = CMSG_FIRSTHDR(&msg); cm != NULL; cm = CMSG_NXTHDR(&msg, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 && cm->cmsg_type == IPV6_PKTINFO) {
			struct in6_pktinfo pi;

			memcpy(&pi, CMSG_DATA(cm), sizeof(pi));
			index = pi.ipi6_ifindex;
		}
	}
	if (index != ifs.index || !parse6(buf, (size_t)n, &r))
		return;

	switch (ifs.state6) {
	case S6_SOLICIT:
		if (r.type == ADVERTISE && r.status == 0 && r.have_addr && r.server_duid_len != 0) {
			char a[INET6_ADDRSTRLEN];

			logmsg(LOG_INFO, "DHCPv6: offer of %s",
			    inet_ntop(AF_INET6, &r.addr, a, sizeof(a)));
			memcpy(ifs.server_duid, r.server_duid, r.server_duid_len);
			ifs.server_duid_len = r.server_duid_len;
			ifs.addr6 = r.addr;	/* asked for in Request */
			start_exchange(S6_REQUEST, 0);
			ifs.addr6 = r.addr;
		}
		break;
	case S6_REQUEST:
	case S6_RENEW:
	case S6_REBIND:
		if (r.type != REPLY)
			break;
		if (r.status == 0 && r.have_addr) {
			if (r.server_duid_len != 0) {
				memcpy(ifs.server_duid, r.server_duid, r.server_duid_len);
				ifs.server_duid_len = r.server_duid_len;
			}
			bind_reply(&r);
		} else if (ifs.state6 == S6_REQUEST) {
			logmsg(LOG_NOTICE, "DHCPv6: request refused (status %d)", r.status);
			memset(&ifs.addr6, 0, sizeof(ifs.addr6));
			start_exchange(S6_SOLICIT, 1);
		}
		break;
	case S6_INFORM:
		if (r.type == REPLY && r.status == 0) {
			set_options(&r);
			write_dhcp6_file();
			ifs.state6 = S6_INFORMED;
			ifs.next6 = now() + (r.refresh >= 600 ? r.refresh : IRT_DEFAULT);
			logmsg(LOG_INFO, "DHCPv6: %d name server(s)", r.ndns);
		}
		break;
	}
}
