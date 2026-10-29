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
 * IPv4: DHCP (RFC 2131: INIT, INIT-REBOOT with the saved lease, renewing
 * at T1, rebinding at T2), BOOTP (RFC 951), INFORM (a manual address with
 * DHCP's options) and MANUAL. Before an address is used, an ARP probe
 * (RFC 5227) checks that no other host has it; DHCP declines an address in
 * use. Packets go through BPF while the interface has no address.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpclient.h"

#define CLIENT_PORT	68
#define SERVER_PORT	67

enum {
	DHCPDISCOVER = 1, DHCPOFFER, DHCPREQUEST, DHCPDECLINE,
	DHCPACK, DHCPNAK, DHCPRELEASE, DHCPINFORM,
};

enum {
	OPT_PAD = 0, OPT_SUBNET_MASK = 1, OPT_ROUTER = 3, OPT_DNS = 6,
	OPT_DOMAIN = 15, OPT_MTU = 26, OPT_BROADCAST = 28,
	OPT_REQUESTED_IP = 50, OPT_LEASE_TIME = 51, OPT_MSG_TYPE = 53,
	OPT_SERVER_ID = 54, OPT_PARAM_LIST = 55, OPT_MAX_SIZE = 57,
	OPT_T1 = 58, OPT_T2 = 59, OPT_CLIENT_ID = 61, OPT_END = 255,
};

/* States. */
enum {
	S4_IDLE, S4_INIT, S4_REQUESTING, S4_INIT_REBOOT, S4_PROBING,
	S4_DECLINED, S4_BOUND, S4_RENEWING, S4_REBINDING, S4_INFORMING,
	S4_BOOTP,
};

/* What a successful ARP probe leads to. */
enum { PROBE_DHCP, PROBE_MANUAL, PROBE_BOOTP };

struct dhcp_packet {
	uint8_t		op, htype, hlen, hops;
	uint32_t	xid;
	uint16_t	secs, flags;
	struct in_addr	ciaddr, yiaddr, siaddr, giaddr;
	uint8_t		chaddr[16];
	char		sname[64];
	char		file[128];
	uint8_t		cookie[4];
	uint8_t		options[308];
} __attribute__((packed));

static const uint8_t magic_cookie[4] = { 99, 130, 83, 99 };

static uint8_t *bpfbuf;
static size_t bpflen;
static int probe_purpose;
static struct lease probe_lease;	/* configured if the probe succeeds */

/* ---- BPF ---------------------------------------------------------------- */

/* IPv4, UDP, not a fragment, destination port 68. */
static struct bpf_insn dhcp_filter[] = {
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_IP, 0, 8),
	BPF_STMT(BPF_LD + BPF_B + BPF_ABS, 23),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, IPPROTO_UDP, 0, 6),
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 20),
	BPF_JUMP(BPF_JMP + BPF_JSET + BPF_K, 0x1fff, 4, 0),
	BPF_STMT(BPF_LDX + BPF_B + BPF_MSH, 14),
	BPF_STMT(BPF_LD + BPF_H + BPF_IND, 16),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, CLIENT_PORT, 0, 1),
	BPF_STMT(BPF_RET + BPF_K, (u_int)-1),
	BPF_STMT(BPF_RET + BPF_K, 0),
};

/* ARP. */
static struct bpf_insn arp_filter[] = {
	BPF_STMT(BPF_LD + BPF_H + BPF_ABS, 12),
	BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, ETHERTYPE_ARP, 0, 1),
	BPF_STMT(BPF_RET + BPF_K, (u_int)-1),
	BPF_STMT(BPF_RET + BPF_K, 0),
};

int
open_bpf(int *fd, bool arp)
{
	struct bpf_program prog;
	struct ifreq ifr;
	u_int on = 1, off = 0, len;
	char dev[16];
	int b = -1;

	if (arp) {
		prog.bf_len = sizeof(arp_filter) / sizeof(arp_filter[0]);
		prog.bf_insns = arp_filter;
	} else {
		prog.bf_len = sizeof(dhcp_filter) / sizeof(dhcp_filter[0]);
		prog.bf_insns = dhcp_filter;
	}
	for (int i = 0; i < 256; i++) {
		snprintf(dev, sizeof(dev), "/dev/bpf%d", i);
		if ((b = open(dev, O_RDWR)) >= 0 || errno != EBUSY)
			break;
	}
	if (b < 0) {
		logmsg(LOG_ERR, "no BPF device: %s", strerror(errno));
		return -1;
	}
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifs.name, sizeof(ifr.ifr_name));
	if (ioctl(b, BIOCGBLEN, &len) < 0 || ioctl(b, BIOCSETIF, &ifr) < 0 ||
	    ioctl(b, BIOCIMMEDIATE, &on) < 0 || ioctl(b, BIOCSHDRCMPLT, &on) < 0 ||
	    ioctl(b, BIOCSSEESENT, &off) < 0 || ioctl(b, BIOCSETF, &prog) < 0) {
		logmsg(LOG_ERR, "BPF setup: %s", strerror(errno));
		close(b);
		return -1;
	}
	if (bpfbuf == NULL || len > bpflen) {
		free(bpfbuf);
		if ((bpfbuf = malloc(len)) == NULL) {
			close(b);
			return -1;
		}
		bpflen = len;
	}
	*fd = b;
	return 0;
}

static uint16_t
ip_checksum(const void *data, size_t len)
{
	const uint8_t *p = data;
	uint32_t sum = 0;

	for (size_t i = 0; i + 1 < len; i += 2)
		sum += (uint32_t)(p[i] << 8 | p[i + 1]);
	if (len & 1)
		sum += (uint32_t)(p[len - 1] << 8);
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return htons((uint16_t)~sum);
}

/* A DHCP message from src (0.0.0.0 if none) to the broadcast address. */
static int
send_broadcast(const struct dhcp_packet *pkt, size_t len, struct in_addr src)
{
	uint8_t frame[ETHER_HDR_LEN + sizeof(struct ip) + sizeof(struct udphdr) +
	    sizeof(struct dhcp_packet)];
	struct ether_header *eh = (struct ether_header *)(void *)frame;
	struct ip *ip = (struct ip *)(void *)(frame + ETHER_HDR_LEN);
	struct udphdr *udp = (struct udphdr *)(void *)(frame + ETHER_HDR_LEN + sizeof(*ip));
	size_t total = ETHER_HDR_LEN + sizeof(*ip) + sizeof(*udp) + len;

	memset(frame, 0, sizeof(frame));
	memset(eh->ether_dhost, 0xff, ETHER_ADDR_LEN);
	memcpy(eh->ether_shost, ifs.mac, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_IP);
	ip->ip_v = 4;
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_len = htons((uint16_t)(sizeof(*ip) + sizeof(*udp) + len));
	ip->ip_ttl = 64;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_src = src;
	ip->ip_dst.s_addr = INADDR_BROADCAST;
	ip->ip_sum = ip_checksum(ip, sizeof(*ip));
	udp->uh_sport = htons(CLIENT_PORT);
	udp->uh_dport = htons(SERVER_PORT);
	udp->uh_ulen = htons((uint16_t)(sizeof(*udp) + len));
	udp->uh_sum = 0;	/* optional for UDP over IPv4 */
	memcpy(frame + ETHER_HDR_LEN + sizeof(*ip) + sizeof(*udp), pkt, len);
	if (write(ifs.bpf, frame, total) != (ssize_t)total) {
		logmsg(LOG_ERR, "BPF write: %s", strerror(errno));
		return -1;
	}
	return 0;
}

/* To the server, from our address (renewing, releasing). */
static int
send_unicast(const struct dhcp_packet *pkt, size_t len, struct in_addr src,
    struct in_addr server)
{
	struct sockaddr_in from, to;
	int s, on = 1, ret = -1;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
	memset(&from, 0, sizeof(from));
	from.sin_len = sizeof(from);
	from.sin_family = AF_INET;
	from.sin_addr = src;
	from.sin_port = htons(CLIENT_PORT);
	memset(&to, 0, sizeof(to));
	to.sin_len = sizeof(to);
	to.sin_family = AF_INET;
	to.sin_addr = server;
	to.sin_port = htons(SERVER_PORT);
	if (bind(s, (struct sockaddr *)&from, sizeof(from)) == 0 &&
	    sendto(s, pkt, len, 0, (struct sockaddr *)&to, sizeof(to)) == (ssize_t)len)
		ret = 0;
	else
		logmsg(LOG_ERR, "send to %s: %s", inet_ntoa(server), strerror(errno));
	close(s);
	return ret;
}

/* ---- Messages ----------------------------------------------------------- */

static uint8_t *
add_option(uint8_t *p, uint8_t code, uint8_t len, const void *data)
{
	*p++ = code;
	*p++ = len;
	memcpy(p, data, len);
	return p + len;
}

/* A request of type (0: BOOTP); returns its length. */
static size_t
build(struct dhcp_packet *pkt, int type, struct in_addr ciaddr,
    struct in_addr requested, struct in_addr server)
{
	static const uint8_t params[] = {
		OPT_SUBNET_MASK, OPT_ROUTER, OPT_DNS, OPT_DOMAIN, OPT_MTU,
		OPT_BROADCAST, OPT_LEASE_TIME, OPT_SERVER_ID, OPT_T1, OPT_T2,
	};
	uint8_t client_id[1 + ETHER_ADDR_LEN];
	uint8_t t = (uint8_t)type;
	uint16_t max = htons(1500);
	uint8_t *p;
	time_t secs = now() - ifs.started4;
	size_t len;

	memset(pkt, 0, sizeof(*pkt));
	pkt->op = 1;			/* BOOTREQUEST */
	pkt->htype = 1;			/* Ethernet */
	pkt->hlen = ETHER_ADDR_LEN;
	pkt->xid = ifs.xid;
	pkt->secs = htons((uint16_t)(secs > 0xffff ? 0xffff : secs));
	pkt->ciaddr = ciaddr;
	if (ciaddr.s_addr == 0)
		pkt->flags = htons(0x8000);	/* broadcast the reply */
	memcpy(pkt->chaddr, ifs.mac, ETHER_ADDR_LEN);
	memcpy(pkt->cookie, magic_cookie, sizeof(magic_cookie));

	p = pkt->options;
	if (type != 0) {
		p = add_option(p, OPT_MSG_TYPE, 1, &t);
		client_id[0] = 1;
		memcpy(client_id + 1, ifs.mac, ETHER_ADDR_LEN);
		p = add_option(p, OPT_CLIENT_ID, sizeof(client_id), client_id);
		if (requested.s_addr != 0)
			p = add_option(p, OPT_REQUESTED_IP, 4, &requested);
		if (server.s_addr != 0)
			p = add_option(p, OPT_SERVER_ID, 4, &server);
		if (type != DHCPDECLINE && type != DHCPRELEASE) {
			p = add_option(p, OPT_MAX_SIZE, 2, &max);
			p = add_option(p, OPT_PARAM_LIST, sizeof(params), params);
		}
	}
	*p++ = OPT_END;
	len = (size_t)(p - (uint8_t *)pkt);
	return len < 300 ? 300 : len;
}

static uint32_t
get32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/*
 * A reply for us: its DHCP type (0 for a BOOTP reply) in *type, its lease
 * in *l. Returns false if it is not for us.
 */
static bool
parse(const uint8_t *data, size_t len, int *type, struct lease *l)
{
	const struct dhcp_packet *pkt = (const struct dhcp_packet *)(const void *)data;
	const uint8_t *p, *end;

	memset(l, 0, sizeof(*l));
	*type = 0;
	if (len < offsetof(struct dhcp_packet, options) || pkt->op != 2 ||
	    pkt->xid != ifs.xid || memcmp(pkt->chaddr, ifs.mac, ETHER_ADDR_LEN) != 0)
		return false;
	l->addr = pkt->yiaddr;
	if (memcmp(pkt->cookie, magic_cookie, sizeof(magic_cookie)) == 0) {
		p = pkt->options;
		end = data + len;
		while (p < end && *p != OPT_END) {
			uint8_t code = *p++, olen;

			if (code == OPT_PAD)
				continue;
			if (p >= end || p + 1 + *p > end)
				break;
			olen = *p++;
			switch (code) {
			case OPT_MSG_TYPE:
				if (olen == 1)
					*type = p[0];
				break;
			case OPT_SUBNET_MASK:
				if (olen == 4)
					memcpy(&l->mask, p, 4);
				break;
			case OPT_ROUTER:
				if (olen >= 4)
					memcpy(&l->router, p, 4);
				break;
			case OPT_BROADCAST:
				if (olen == 4)
					memcpy(&l->broadcast, p, 4);
				break;
			case OPT_DNS:
				for (int i = 0; i + 4 <= olen && l->ndns < MAX_DNS; i += 4)
					memcpy(&l->dns[l->ndns++], p + i, 4);
				break;
			case OPT_DOMAIN:
				memcpy(l->domain, p, olen);
				l->domain[olen] = '\0';
				break;
			case OPT_MTU:
				if (olen == 2)
					l->mtu = (uint16_t)(p[0] << 8 | p[1]);
				break;
			case OPT_SERVER_ID:
				if (olen == 4)
					memcpy(&l->server, p, 4);
				break;
			case OPT_LEASE_TIME:
				if (olen == 4)
					l->lease_time = get32(p);
				break;
			case OPT_T1:
				if (olen == 4)
					l->t1 = get32(p);
				break;
			case OPT_T2:
				if (olen == 4)
					l->t2 = get32(p);
				break;
			}
			p += olen;
		}
	}
	if (l->mask.s_addr == 0 && l->addr.s_addr != 0) {
		/* The class mask, as old servers expect. */
		uint32_t a = ntohl(l->addr.s_addr);

		l->mask.s_addr = htonl(IN_CLASSA(a) ? IN_CLASSA_NET :
		    IN_CLASSB(a) ? IN_CLASSB_NET : IN_CLASSC_NET);
	}
	if (*type != 0 && *type != DHCPINFORM) {
		if (l->lease_time == 0)
			l->lease_time = 3600;
		if (l->lease_time == 0xffffffff)
			l->lease_time = 0;	/* infinite */
		if (l->lease_time != 0) {
			if (l->t1 == 0 || l->t1 >= l->lease_time)
				l->t1 = l->lease_time / 2;
			if (l->t2 == 0 || l->t2 >= l->lease_time || l->t2 <= l->t1)
				l->t2 = (uint32_t)((uint64_t)l->lease_time * 7 / 8);
		}
	}
	return true;
}

/* 4, 8, 16, 32, 64, 64... seconds, +-1 s (RFC 2131 4.1). */
static time_t
backoff(int attempt)
{
	time_t d = 4 << (attempt < 4 ? attempt : 4);

	return d - 1 + (time_t)arc4random_uniform(3);
}

/* ---- The saved lease (INIT-REBOOT) --------------------------------------- */

static void
saved_path(char *path, size_t size)
{
	snprintf(path, size, SAVED_DIR "/%s", ifs.name);
}

static void
save_lease(const struct lease *l)
{
	char path[128], tmp[136];
	FILE *f;

	mkdir("/var/db/dhcpclient", 0755);
	mkdir(SAVED_DIR, 0755);
	saved_path(path, sizeof(path));
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	if ((f = fopen(tmp, "w")) == NULL)
		return;
	fprintf(f, "addr=%s\n", inet_ntoa(l->addr));
	fprintf(f, "server=%s\n", inet_ntoa(l->server));
	fprintf(f, "expires=%lld\n", l->lease_time == 0 ? 0LL :
	    (long long)(wallclock() + l->lease_time - (now() - l->start)));
	fclose(f);
	rename(tmp, path);
}

/* The saved address, if its lease has not expired. */
static bool
load_lease(struct in_addr *addr)
{
	char path[128], line[128];
	long long expires = -1;
	bool have = false;
	FILE *f;

	saved_path(path, sizeof(path));
	if ((f = fopen(path, "r")) == NULL)
		return false;
	while (fgets(line, sizeof(line), f) != NULL) {
		line[strcspn(line, "\n")] = '\0';
		if (strncmp(line, "addr=", 5) == 0)
			have = inet_pton(AF_INET, line + 5, addr) == 1;
		else if (strncmp(line, "expires=", 8) == 0)
			expires = strtoll(line + 8, NULL, 10);
	}
	fclose(f);
	return have && (expires == 0 || expires > wallclock() + 10);
}

static void
forget_lease(void)
{
	char path[128];

	saved_path(path, sizeof(path));
	unlink(path);
}

/* ---- ARP probe (RFC 5227) ----------------------------------------------- */

#define PROBE_NUM	3	/* probes, one second apart */
#define ANNOUNCE_WAIT	2	/* seconds after the last one */

static void
send_arp_probe(struct in_addr target)
{
	uint8_t frame[ETHER_HDR_LEN + 28];
	struct ether_header *eh = (struct ether_header *)(void *)frame;
	uint8_t *a = frame + ETHER_HDR_LEN;

	memset(frame, 0, sizeof(frame));
	memset(eh->ether_dhost, 0xff, ETHER_ADDR_LEN);
	memcpy(eh->ether_shost, ifs.mac, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_ARP);
	a[0] = 0; a[1] = 1;		/* Ethernet */
	a[2] = 0x08; a[3] = 0x00;	/* IPv4 */
	a[4] = ETHER_ADDR_LEN;
	a[5] = 4;
	a[6] = 0; a[7] = 1;		/* request */
	memcpy(a + 8, ifs.mac, ETHER_ADDR_LEN);
	/* sender IP 0.0.0.0, target hardware 0 */
	memcpy(a + 24, &target, 4);
	if (write(ifs.arp, frame, sizeof(frame)) != (ssize_t)sizeof(frame))
		logmsg(LOG_ERR, "ARP probe: %s", strerror(errno));
}

static void probe_done(bool clear);

/* Checks addr before it is used; probe_done() says the outcome. */
static void
start_probe(struct in_addr addr, int purpose, const struct lease *l)
{
	probe_purpose = purpose;
	probe_lease = *l;
	ifs.probe = addr;
	ifs.probes = 0;
	if (ifs.arp < 0 && open_bpf(&ifs.arp, true) < 0) {
		probe_done(true);	/* cannot check: go ahead */
		return;
	}
	ifs.state4 = S4_PROBING;
	ifs.next4 = now();
}

/* Another host using, or probing for, the address. */
void
arp_input(void)
{
	ssize_t n = read(ifs.arp, bpfbuf, bpflen);

	for (uint8_t *p = bpfbuf; n > 0 && p < bpfbuf + n;) {
		const struct bpf_hdr *bh = (const struct bpf_hdr *)(void *)p;
		const uint8_t *a = p + bh->bh_hdrlen + ETHER_HDR_LEN;
		size_t caplen = bh->bh_caplen;

		p += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
		if (ifs.state4 != S4_PROBING || caplen < ETHER_HDR_LEN + 28)
			continue;
		if (memcmp(a + 8, ifs.mac, ETHER_ADDR_LEN) == 0)
			continue;	/* ours */
		if (memcmp(a + 14, &ifs.probe, 4) == 0 ||
		    (memcmp(a + 14, "\0\0\0\0", 4) == 0 && memcmp(a + 24, &ifs.probe, 4) == 0)) {
			logmsg(LOG_NOTICE, "%s is in use by %02x:%02x:%02x:%02x:%02x:%02x",
			    inet_ntoa(ifs.probe), a[8], a[9], a[10], a[11], a[12], a[13]);
			probe_done(false);
			return;
		}
	}
}

/* ---- The state machine -------------------------------------------------- */

static void
send_dhcp(int type, struct in_addr ciaddr, struct in_addr requested, struct in_addr server,
    bool unicast)
{
	struct dhcp_packet pkt;
	size_t len = build(&pkt, type, ciaddr, requested, server);

	if (unicast)
		send_unicast(&pkt, len, ciaddr, server);
	else
		send_broadcast(&pkt, len, ciaddr);
}

static void
enter_init(time_t delay)
{
	ifs.state4 = S4_INIT;
	ifs.tries4 = 0;
	ifs.next4 = now() + delay;
}

/* The manual address (MANUAL, INFORM), as a lease without expiry. */
static struct lease
manual_lease(void)
{
	struct lease l;

	memset(&l, 0, sizeof(l));
	l.addr = ifs.manual_addr;
	l.mask = ifs.manual_mask;
	l.start = now();
	return l;
}

static void
probe_done(bool clear)
{
	struct in_addr none = { 0 };

	if (ifs.arp >= 0) {
		close(ifs.arp);
		ifs.arp = -1;
	}
	switch (probe_purpose) {
	case PROBE_DHCP:
		if (clear) {
			apply_lease(&probe_lease);
			save_lease(&probe_lease);
			ifs.state4 = S4_BOUND;
			ifs.next4 = probe_lease.lease_time == 0 ? 0 :
			    probe_lease.start + probe_lease.t1;
		} else {
			/* Decline it, wait, and start over (RFC 2131 3.1.5). */
			send_dhcp(DHCPDECLINE, none, probe_lease.addr, probe_lease.server, false);
			forget_lease();
			ifs.state4 = S4_DECLINED;
			ifs.next4 = now() + 10;
		}
		break;
	case PROBE_MANUAL:
		if (!clear) {
			ifs.state4 = S4_IDLE;
			ifs.next4 = now() + 60;	/* try again later */
			break;
		}
		apply_lease(&probe_lease);
		if (ifs.method4 == M4_INFORM) {
			/* Ask DHCP for the rest: router, name servers. */
			ifs.xid = arc4random();
			ifs.started4 = now();
			ifs.tries4 = 0;
			ifs.state4 = S4_INFORMING;
			ifs.next4 = now();
		} else {
			ifs.state4 = S4_IDLE;
			ifs.next4 = 0;
		}
		break;
	case PROBE_BOOTP:
		if (clear) {
			apply_lease(&probe_lease);
			ifs.state4 = S4_IDLE;
			ifs.next4 = 0;
		} else {
			enter_init(10);
			ifs.state4 = S4_BOOTP;
		}
		break;
	}
}

void
dhcp4_start(void)
{
	struct in_addr saved;

	ifs.next4 = 0;
	ifs.state4 = S4_IDLE;
	if (ifs.method4 == M4_NONE)
		return;
	if (ifs.method4 == M4_MANUAL || ifs.method4 == M4_INFORM) {
		struct lease l = manual_lease();

		if (ifs.bpf < 0 && ifs.method4 == M4_INFORM && open_bpf(&ifs.bpf, false) < 0)
			return;
		start_probe(l.addr, PROBE_MANUAL, &l);
		return;
	}
	if (ifs.bpf < 0 && open_bpf(&ifs.bpf, false) < 0) {
		ifs.next4 = now() + 30;	/* try again */
		return;
	}
	ifs.xid = arc4random();
	ifs.started4 = now();
	ifs.tries4 = 0;
	if (ifs.method4 == M4_BOOTP) {
		ifs.state4 = S4_BOOTP;
		ifs.next4 = now();
	} else if (load_lease(&saved)) {
		/* INIT-REBOOT: ask for the address we had. */
		logmsg(LOG_INFO, "asking for %s again", inet_ntoa(saved));
		ifs.offer.addr = saved;
		ifs.state4 = S4_INIT_REBOOT;
		ifs.next4 = now();
	} else {
		enter_init(0);
	}
}

void
dhcp4_stop(void)
{
	struct in_addr none = { 0 };

	/* Give a DHCP lease back. */
	if (ifs.method4 == M4_DHCP && ifs.current.addr.s_addr != 0 &&
	    ifs.current.server.s_addr != 0 &&
	    (ifs.state4 == S4_BOUND || ifs.state4 == S4_RENEWING || ifs.state4 == S4_REBINDING)) {
		ifs.xid = arc4random();
		send_dhcp(DHCPRELEASE, ifs.current.addr, none, ifs.current.server, true);
		forget_lease();
	}
	if (ifs.arp >= 0) {
		close(ifs.arp);
		ifs.arp = -1;
	}
	ifs.state4 = S4_IDLE;
	ifs.next4 = 0;
}

/* The address given up when the link went down, to ask for again. */
static struct in_addr link_down_addr;

/*
 * The link went down: as on macOS, the interface gives up its address and
 * default route, so that traffic goes out through an interface that is
 * still connected. The address is asked for again when the link returns.
 */
void
dhcp4_link_down(void)
{
	if (ifs.method4 != M4_DHCP)
		return;
	if (ifs.current.addr.s_addr != 0) {
		link_down_addr = ifs.current.addr;
		drop_lease();
	}
	if (ifs.arp >= 0) {
		close(ifs.arp);
		ifs.arp = -1;
	}
	ifs.state4 = S4_IDLE;
	ifs.next4 = 0;
}

/* The link came back: check that the lease still holds (INIT-REBOOT). */
void
dhcp4_link_up(void)
{
	if (ifs.method4 != M4_DHCP)
		return;
	if (ifs.current.addr.s_addr == 0 && link_down_addr.s_addr != 0) {
		ifs.offer.addr = link_down_addr;
		link_down_addr.s_addr = 0;
		ifs.xid = arc4random();
		ifs.started4 = now();
		ifs.tries4 = 0;
		ifs.state4 = S4_INIT_REBOOT;
		ifs.next4 = now();
	} else if (ifs.current.addr.s_addr != 0) {
		ifs.offer.addr = ifs.current.addr;
		ifs.xid = arc4random();
		ifs.started4 = now();
		ifs.tries4 = 0;
		ifs.state4 = S4_INIT_REBOOT;
		ifs.next4 = now();
	} else if (ifs.state4 == S4_INIT || ifs.state4 == S4_IDLE) {
		enter_init(0);
	}
}

void
dhcp4_timeout(void)
{
	struct in_addr none = { 0 };
	time_t t = now();

	switch (ifs.state4) {
	case S4_INIT:
		if (ifs.tries4 == 0) {
			ifs.xid = arc4random();
			ifs.started4 = t;
		}
		send_dhcp(DHCPDISCOVER, none, none, none, false);
		ifs.next4 = t + backoff(ifs.tries4++);
		break;
	case S4_REQUESTING:
		if (ifs.tries4 >= 4) {
			enter_init(0);
			break;
		}
		send_dhcp(DHCPREQUEST, none, ifs.offer.addr, ifs.offer.server, false);
		ifs.next4 = t + backoff(ifs.tries4++);
		break;
	case S4_INIT_REBOOT:
		if (ifs.tries4 >= 2) {
			/* No answer: start over (RFC 2131 3.2). */
			if (ifs.current.addr.s_addr != 0)
				drop_lease();
			enter_init(0);
			break;
		}
		send_dhcp(DHCPREQUEST, none, ifs.offer.addr, none, false);
		ifs.next4 = t + backoff(ifs.tries4++);
		break;
	case S4_PROBING:
		if (ifs.probes < PROBE_NUM) {
			send_arp_probe(ifs.probe);
			ifs.probes++;
			ifs.next4 = t + (ifs.probes < PROBE_NUM ? 1 : ANNOUNCE_WAIT);
		} else {
			probe_done(true);
		}
		break;
	case S4_DECLINED:
		enter_init(0);
		break;
	case S4_BOUND:
		ifs.xid = arc4random();
		ifs.started4 = t;
		ifs.state4 = S4_RENEWING;
		/* FALLTHROUGH */
	case S4_RENEWING:
	case S4_REBINDING: {
		const struct lease *l = &ifs.current;
		time_t t2 = l->start + l->t2, expiry = l->start + l->lease_time, delay;

		if (t >= expiry) {
			logmsg(LOG_NOTICE, "lease on %s expired", inet_ntoa(l->addr));
			drop_lease();
			forget_lease();
			enter_init(0);
			break;
		}
		if (t >= t2)
			ifs.state4 = S4_REBINDING;
		if (ifs.state4 == S4_REBINDING)
			send_dhcp(DHCPREQUEST, l->addr, none, none, false);
		else
			send_dhcp(DHCPREQUEST, l->addr, none, l->server, true);
		/* Half the time left, at least a minute (RFC 2131 4.4.5). */
		delay = ((ifs.state4 == S4_REBINDING ? expiry : t2) - t) / 2;
		if (delay < 60)
			delay = 60;
		if (t + delay > expiry)
			delay = expiry - t;
		ifs.next4 = t + delay;
		break;
	}
	case S4_INFORMING:
		if (ifs.tries4 >= 4) {
			/* No DHCP server: keep the manual address alone. */
			logmsg(LOG_INFO, "no answer to DHCPINFORM");
			ifs.state4 = S4_IDLE;
			ifs.next4 = t + 300;
			break;
		}
		send_dhcp(DHCPINFORM, ifs.manual_addr, none, none, false);
		ifs.next4 = t + backoff(ifs.tries4++);
		break;
	case S4_BOOTP:
		if (ifs.tries4 == 0) {
			ifs.xid = arc4random();
			ifs.started4 = t;
		}
		send_dhcp(0, none, none, none, false);
		ifs.next4 = t + backoff(ifs.tries4++);
		break;
	case S4_IDLE:
		/* A manual address to check again, or INFORM to refresh. */
		ifs.next4 = 0;
		dhcp4_start();
		break;
	}
}

static void
reply_input(int type, struct lease *l)
{
	time_t t = now();

	switch (ifs.state4) {
	case S4_INIT:
		if (type == DHCPOFFER && l->addr.s_addr != 0) {
			logmsg(LOG_INFO, "offer of %s from %s", inet_ntoa(l->addr),
			    inet_ntoa(l->server));
			ifs.offer = *l;
			ifs.state4 = S4_REQUESTING;
			ifs.tries4 = 0;
			ifs.next4 = t;
		}
		break;
	case S4_REQUESTING:
	case S4_INIT_REBOOT:
		if (type == DHCPACK) {
			l->start = t;
			if (l->server.s_addr == 0)
				l->server = ifs.offer.server;
			if (ifs.current.addr.s_addr == l->addr.s_addr) {
				/* The address we have: no need to check it again. */
				apply_lease(l);
				save_lease(l);
				ifs.state4 = S4_BOUND;
				ifs.next4 = l->lease_time == 0 ? 0 : l->start + l->t1;
			} else {
				start_probe(l->addr, PROBE_DHCP, l);
			}
		} else if (type == DHCPNAK) {
			logmsg(LOG_NOTICE, "server refused %s", inet_ntoa(ifs.offer.addr));
			if (ifs.current.addr.s_addr != 0)
				drop_lease();
			forget_lease();
			enter_init(ifs.state4 == S4_REQUESTING ? 2 : 0);
		}
		break;
	case S4_RENEWING:
	case S4_REBINDING:
		if (type == DHCPACK && l->addr.s_addr == ifs.current.addr.s_addr) {
			l->start = t;
			if (l->server.s_addr == 0)
				l->server = ifs.current.server;
			apply_lease(l);
			save_lease(l);
			ifs.state4 = S4_BOUND;
			ifs.next4 = l->lease_time == 0 ? 0 : l->start + l->t1;
		} else if (type == DHCPNAK) {
			logmsg(LOG_NOTICE, "server refused the renewal");
			drop_lease();
			forget_lease();
			enter_init(0);
		}
		break;
	case S4_INFORMING:
		if (type == DHCPACK) {
			struct lease m = manual_lease();

			m.router = l->router;
			m.server = l->server;
			memcpy(m.dns, l->dns, sizeof(m.dns));
			m.ndns = l->ndns;
			memcpy(m.domain, l->domain, sizeof(m.domain));
			apply_lease(&m);
			ifs.state4 = S4_IDLE;
			ifs.next4 = t + 3600;	/* refresh the options hourly */
		}
		break;
	case S4_BOOTP:
		if (type == 0 && l->addr.s_addr != 0) {
			l->start = t;
			l->lease_time = 0;	/* BOOTP addresses do not expire */
			logmsg(LOG_INFO, "BOOTP reply: %s", inet_ntoa(l->addr));
			start_probe(l->addr, PROBE_BOOTP, l);
		}
		break;
	}
}

void
dhcp4_input(void)
{
	ssize_t n = read(ifs.bpf, bpfbuf, bpflen);

	for (uint8_t *p = bpfbuf; n > 0 && p < bpfbuf + n;) {
		const struct bpf_hdr *bh = (const struct bpf_hdr *)(void *)p;
		const uint8_t *frame = p + bh->bh_hdrlen;
		size_t caplen = bh->bh_caplen, off;
		struct lease l;
		int type;

		p += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
		if (caplen < ETHER_HDR_LEN + sizeof(struct ip) + sizeof(struct udphdr))
			continue;
		off = ETHER_HDR_LEN + (size_t)(frame[ETHER_HDR_LEN] & 0x0f) * 4 +
		    sizeof(struct udphdr);
		if (off >= caplen || !parse(frame + off, caplen - off, &type, &l))
			continue;
		reply_input(type, &l);
	}
}
