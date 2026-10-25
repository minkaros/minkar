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
 * netstat: show network status.
 *
 *   netstat [-anW] [-f family] [-p proto]   active TCP and UDP sockets
 *   netstat -r [-nW] [-f family]            routing tables
 *   netstat -i [-nW] [-f family]            interfaces and their counters
 *
 * Everything comes from sysctl(3) and getifaddrs(3): the routing table
 * from PF_ROUTE NET_RT_DUMP, sockets from net.inet.{tcp,udp}.pcblist64.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

static bool	aflag;		/* also servers (listening/unbound sockets) */
static bool	nflag;		/* numbers, not names */
static bool	Wflag;		/* don't truncate addresses */
static int	family = AF_UNSPEC;
static const char *protoname;	/* "tcp", "udp" or NULL for both */

static void	usage(void) __dead2;

/* Fetch a whole sysctl; the caller frees the buffer. */
static void *
fetch(int *mib, u_int miblen, const char *name, size_t *lenp)
{
	void *buf = NULL;
	size_t len;
	int tries;

	for (tries = 0; tries < 8; tries++) {
		if ((name != NULL ? sysctlbyname(name, NULL, &len, NULL, 0) :
		    sysctl(mib, miblen, NULL, &len, NULL, 0)) == -1)
			return (NULL);
		len += len / 8 + 512;	/* room for growth in between */
		free(buf);
		if ((buf = malloc(len)) == NULL)
			err(EX_OSERR, "malloc");
		if ((name != NULL ? sysctlbyname(name, buf, &len, NULL, 0) :
		    sysctl(mib, miblen, buf, &len, NULL, 0)) == 0) {
			*lenp = len;
			return (buf);
		}
		if (errno != ENOMEM)
			break;
	}
	free(buf);
	return (NULL);
}

/* ---------------------------------------------------------------------
 * Addresses
 */

/* Routing messages pad each sockaddr to a multiple of 4 bytes. */
static size_t
sa_size(const struct sockaddr *sa)
{

	if (sa->sa_len == 0)
		return (sizeof(uint32_t));
	return (1 + ((sa->sa_len - 1) | (sizeof(uint32_t) - 1)));
}

static int
mask_bits(const uint8_t *p, size_t len)
{
	int bits = 0;

	for (size_t i = 0; i < len; i++) {
		uint8_t b = p[i];

		if (b == 0xff) {
			bits += 8;
			continue;
		}
		while (b & 0x80) {
			bits++;
			b <<= 1;
		}
		break;
	}
	return (bits);
}

static void
host_name(const struct sockaddr *sa, char *buf, size_t len)
{
	int flags = nflag ? NI_NUMERICHOST : 0;

	if (getnameinfo(sa, sa->sa_len, buf, (socklen_t)len, NULL, 0,
	    flags) != 0)
		snprintf(buf, len, "?");
}

/*
 * A routing destination: "default", a host, or a network with its
 * prefix length. A netmask in a routing message may be cut short and
 * carry no family; only its bytes after the header matter.
 */
static void
dest_name(const struct sockaddr *sa, const struct sockaddr *mask, bool host,
    char *buf, size_t len)
{
	char addr[NI_MAXHOST];
	int bits = -1;

	if (sa->sa_family == AF_INET) {
		struct sockaddr_in sin;
		uint8_t m[4] = { 0, 0, 0, 0 };

		memset(&sin, 0, sizeof(sin));
		memcpy(&sin, sa, MIN(sa->sa_len, sizeof(sin)));
		sin.sin_len = sizeof(sin);
		if (mask != NULL && mask->sa_len > 4)
			memcpy(m, (const uint8_t *)mask + 4,
			    MIN((size_t)mask->sa_len - 4, sizeof(m)));
		if (!host)
			bits = mask != NULL ? mask_bits(m, sizeof(m)) : 32;
		if (!host && bits == 0 && sin.sin_addr.s_addr == 0) {
			snprintf(buf, len, "default");
			return;
		}
		if (host)
			host_name((struct sockaddr *)&sin, addr, sizeof(addr));
		else
			inet_ntop(AF_INET, &sin.sin_addr, addr, sizeof(addr));
	} else if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 sin6;
		uint8_t m[16];

		memset(&sin6, 0, sizeof(sin6));
		memcpy(&sin6, sa, MIN(sa->sa_len, sizeof(sin6)));
		sin6.sin6_len = sizeof(sin6);
		memset(m, 0, sizeof(m));
		if (mask != NULL && mask->sa_len > 8)
			memcpy(m, (const uint8_t *)mask + 8,
			    MIN((size_t)mask->sa_len - 8, sizeof(m)));
		if (!host)
			bits = mask != NULL ? mask_bits(m, sizeof(m)) : 128;
		if (!host && bits == 0 &&
		    IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr)) {
			snprintf(buf, len, "default");
			return;
		}
		/* The kernel keeps a link-local's scope in the address. */
		if (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr)) {
			uint16_t scope = (uint16_t)(sin6.sin6_addr.s6_addr[2] << 8 |
			    sin6.sin6_addr.s6_addr[3]);

			if (scope != 0 && sin6.sin6_scope_id == 0)
				sin6.sin6_scope_id = scope;
			sin6.sin6_addr.s6_addr[2] = sin6.sin6_addr.s6_addr[3] = 0;
		}
		if (host)
			host_name((struct sockaddr *)&sin6, addr, sizeof(addr));
		else
			getnameinfo((struct sockaddr *)&sin6, sizeof(sin6),
			    addr, sizeof(addr), NULL, 0, NI_NUMERICHOST);
	} else {
		snprintf(buf, len, "(family %d)", sa->sa_family);
		return;
	}
	if (bits >= 0)
		snprintf(buf, len, "%s/%d", addr, bits);
	else
		snprintf(buf, len, "%s", addr);
}

static void
link_name(const struct sockaddr_dl *sdl, char *buf, size_t len)
{
	const uint8_t *a = (const uint8_t *)sdl->sdl_data + sdl->sdl_nlen;
	size_t n = 0;

	if (sdl->sdl_alen == 0) {
		snprintf(buf, len, "link#%d", sdl->sdl_index);
		return;
	}
	buf[0] = '\0';
	for (int i = 0; i < sdl->sdl_alen && n + 4 < len; i++)
		n += (size_t)snprintf(buf + n, len - n, "%s%x",
		    i ? ":" : "", a[i]);
}

static void
gateway_name(const struct sockaddr *sa, char *buf, size_t len)
{

	switch (sa->sa_family) {
	case AF_INET:
	case AF_INET6:
		dest_name(sa, NULL, true, buf, len);
		break;
	case AF_LINK:
		link_name((const struct sockaddr_dl *)sa, buf, len);
		break;
	default:
		snprintf(buf, len, "(family %d)", sa->sa_family);
		break;
	}
}

static const char *
family_name(int af)
{

	switch (af) {
	case AF_INET:
		return ("Internet");
	case AF_INET6:
		return ("Internet6");
	default:
		return ("Other");
	}
}

/* ---------------------------------------------------------------------
 * Routing tables
 */

static const struct {
	int	flag;
	char	letter;
} rtflags[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_REJECT,	'R' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_CLONING,	'C' },
	{ RTF_LLINFO,	'L' },
	{ RTF_STATIC,	'S' },
	{ RTF_BLACKHOLE, 'B' },
	{ RTF_WASCLONED, 'W' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ RTF_PROTO3,	'3' },
	{ RTF_BROADCAST, 'b' },
	{ RTF_MULTICAST, 'm' },
	{ RTF_IFSCOPE,	'I' },
	{ RTF_IFREF,	'i' },
};

static void
flag_string(int flags, char *buf)
{
	size_t n = 0;

	for (size_t i = 0; i < nitems(rtflags); i++)
		if (flags & rtflags[i].flag)
			buf[n++] = rtflags[i].letter;
	buf[n] = '\0';
}

static void
routes_for(int af, bool header)
{
	int mib[6] = { CTL_NET, PF_ROUTE, 0, af, NET_RT_DUMP, 0 };
	char *buf, *p, *end;
	size_t len;
	time_t now = time(NULL);
	int dw = Wflag ? 39 : (af == AF_INET6 ? 31 : 18);
	int gw = Wflag ? 39 : (af == AF_INET6 ? 25 : 18);

	if ((buf = fetch(mib, 6, NULL, &len)) == NULL) {
		warn("route table");
		return;
	}
	if (header)
		printf("\n");
	printf("%s:\n", family_name(af));
	printf("%-*s %-*s %-10s %8s %6s\n", dw, "Destination", gw, "Gateway",
	    "Flags", "Netif", "Expire");

	end = buf + len;
	for (p = buf; p + sizeof(struct rt_msghdr) <= end;) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)(void *)p;
		const struct sockaddr *sa[RTAX_MAX];
		char *q;
		char dst[NI_MAXHOST + 8], gate[NI_MAXHOST], fl[32];
		char ifname[IF_NAMESIZE] = "";

		if (rtm->rtm_msglen == 0)
			break;
		memset(sa, 0, sizeof(sa));
		q = (char *)(rtm + 1);
		for (int i = 0; i < RTAX_MAX; i++) {
			if ((rtm->rtm_addrs & (1 << i)) == 0)
				continue;
			if (q >= p + rtm->rtm_msglen)
				break;
			sa[i] = (const struct sockaddr *)(void *)q;
			q += sa_size(sa[i]);
		}
		p += rtm->rtm_msglen;

		if (sa[RTAX_DST] == NULL || sa[RTAX_DST]->sa_family != af)
			continue;
		dest_name(sa[RTAX_DST], sa[RTAX_NETMASK],
		    (rtm->rtm_flags & RTF_HOST) != 0, dst, sizeof(dst));
		if (sa[RTAX_GATEWAY] != NULL)
			gateway_name(sa[RTAX_GATEWAY], gate, sizeof(gate));
		else
			snprintf(gate, sizeof(gate), "-");
		flag_string(rtm->rtm_flags, fl);
		if (if_indextoname(rtm->rtm_index, ifname) == NULL)
			snprintf(ifname, sizeof(ifname), "-");

		printf("%-*.*s %-*.*s %-10s %8.8s", dw, Wflag ? 255 : dw, dst,
		    gw, Wflag ? 255 : gw, gate, fl, ifname);
		/* rmx_expire is calendar time; show what is left, if any. */
		if (rtm->rtm_rmx.rmx_expire > 0) {
			long left = (long)rtm->rtm_rmx.rmx_expire - (long)now;

			if (left > 0)
				printf(" %6ld", left);
		}
		printf("\n");
	}
	free(buf);
}

static void
show_routes(void)
{

	printf("Routing tables\n");
	if (family == AF_UNSPEC || family == AF_INET)
		routes_for(AF_INET, true);
	if (family == AF_UNSPEC || family == AF_INET6)
		routes_for(AF_INET6, true);
}

/* ---------------------------------------------------------------------
 * Interfaces
 */

static void
show_interfaces(void)
{
	struct ifaddrs *ifap, *ifa, *link;
	int nw = Wflag ? 39 : 17, aw = Wflag ? 39 : 17;

	if (getifaddrs(&ifap) == -1)
		err(EX_OSERR, "getifaddrs");
	printf("%-8s %5s %-*s %-*s %10s %5s %10s %5s %5s\n", "Name", "Mtu",
	    nw, "Network", aw, "Address", "Ipkts", "Ierrs", "Opkts", "Oerrs",
	    "Coll");

	for (link = ifap; link != NULL; link = link->ifa_next) {
		const struct if_data *ifd = link->ifa_data;
		char name[IF_NAMESIZE + 2], addr[NI_MAXHOST];

		if (link->ifa_addr == NULL ||
		    link->ifa_addr->sa_family != AF_LINK || ifd == NULL)
			continue;
		snprintf(name, sizeof(name), "%s%s", link->ifa_name,
		    (link->ifa_flags & IFF_UP) ? "" : "*");

		if (family == AF_UNSPEC) {
			const struct sockaddr_dl *sdl =
			    (const struct sockaddr_dl *)(void *)link->ifa_addr;
			char net[24];

			snprintf(net, sizeof(net), "<Link#%d>", sdl->sdl_index);
			if (sdl->sdl_alen > 0)
				link_name(sdl, addr, sizeof(addr));
			else
				addr[0] = '\0';
			printf("%-8.8s %5u %-*s %-*s %10u %5u %10u %5u %5u\n",
			    name, ifd->ifi_mtu, nw, net, aw, addr,
			    ifd->ifi_ipackets, ifd->ifi_ierrors,
			    ifd->ifi_opackets, ifd->ifi_oerrors,
			    ifd->ifi_collisions);
		}

		for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
			char net[NI_MAXHOST + 8];
			int af;

			if (ifa->ifa_addr == NULL ||
			    strcmp(ifa->ifa_name, link->ifa_name) != 0)
				continue;
			af = ifa->ifa_addr->sa_family;
			if (af != AF_INET && af != AF_INET6)
				continue;
			if (family != AF_UNSPEC && af != family)
				continue;
			/* Show the network: the address under its mask. */
			if (af == AF_INET && ifa->ifa_netmask != NULL) {
				struct sockaddr_in n4;

				memcpy(&n4, ifa->ifa_addr, sizeof(n4));
				n4.sin_addr.s_addr &= ((const struct sockaddr_in *)
				    (void *)ifa->ifa_netmask)->sin_addr.s_addr;
				dest_name((struct sockaddr *)&n4,
				    ifa->ifa_netmask, false, net, sizeof(net));
			} else if (af == AF_INET6 && ifa->ifa_netmask != NULL) {
				struct sockaddr_in6 n6;
				const struct sockaddr_in6 *m6 =
				    (const struct sockaddr_in6 *)(void *)
				    ifa->ifa_netmask;

				memcpy(&n6, ifa->ifa_addr, sizeof(n6));
				for (int i = 0; i < 16; i++)
					n6.sin6_addr.s6_addr[i] &=
					    m6->sin6_addr.s6_addr[i];
				dest_name((struct sockaddr *)&n6,
				    ifa->ifa_netmask, false, net, sizeof(net));
			} else
				dest_name(ifa->ifa_addr, NULL, false, net,
				    sizeof(net));
			dest_name(ifa->ifa_addr, NULL, true, addr,
			    sizeof(addr));
			printf("%-8.8s %5u %-*.*s %-*.*s %10u %5s %10u %5s %5s\n",
			    name, ifd->ifi_mtu, nw, Wflag ? 255 : nw, net, aw,
			    Wflag ? 255 : aw, addr, ifd->ifi_ipackets, "-",
			    ifd->ifi_opackets, "-", "-");
		}
	}
	freeifaddrs(ifap);
}

/* ---------------------------------------------------------------------
 * Sockets
 */

static const char *const tcpstates[] = {
	"CLOSED", "LISTEN", "SYN_SENT", "SYN_RCVD", "ESTABLISHED",
	"CLOSE_WAIT", "FIN_WAIT_1", "CLOSING", "LAST_ACK", "FIN_WAIT_2",
	"TIME_WAIT",
};

/* "addr.port", with "*" for a wildcard address or port. */
static void
endpoint(int af, const void *addr, u_short port, const char *proto,
    char *buf, size_t len)
{
	char host[NI_MAXHOST], serv[NI_MAXSERV];
	bool any;

	if (af == AF_INET) {
		struct sockaddr_in sin;

		memset(&sin, 0, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		memcpy(&sin.sin_addr, addr, sizeof(sin.sin_addr));
		any = sin.sin_addr.s_addr == INADDR_ANY;
		if (!any)
			host_name((struct sockaddr *)&sin, host, sizeof(host));
	} else {
		struct sockaddr_in6 sin6;

		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_len = sizeof(sin6);
		sin6.sin6_family = AF_INET6;
		memcpy(&sin6.sin6_addr, addr, sizeof(sin6.sin6_addr));
		any = IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr);
		if (!any)
			host_name((struct sockaddr *)&sin6, host, sizeof(host));
	}
	if (any)
		snprintf(host, sizeof(host), "*");

	if (port == 0)
		snprintf(serv, sizeof(serv), "*");
	else {
		struct servent *sp = nflag ? NULL :
		    getservbyport((int)port, proto);

		if (sp != NULL)
			snprintf(serv, sizeof(serv), "%s", sp->s_name);
		else
			snprintf(serv, sizeof(serv), "%u", ntohs(port));
	}
	snprintf(buf, len, "%s.%s", host, serv);
}

static bool
show_pcb(const char *proto, const struct xinpcb64 *inp, int state)
{
	const struct xsocket64 *so = &inp->xi_socket;
	char local[NI_MAXHOST + NI_MAXSERV + 2];
	char foreign[NI_MAXHOST + NI_MAXSERV + 2];
	char pname[8];
	const void *la, *fa;
	int af;
	bool v4 = (inp->inp_vflag & INP_IPV4) != 0;
	bool v6 = (inp->inp_vflag & INP_IPV6) != 0;
	int w = Wflag ? 45 : 22;

	if (family == AF_INET && !v4)
		return (false);
	if (family == AF_INET6 && !v6)
		return (false);
	if (v6) {
		af = AF_INET6;
		la = &inp->inp_dependladdr.inp6_local;
		fa = &inp->inp_dependfaddr.inp6_foreign;
	} else {
		af = AF_INET;
		la = &inp->inp_dependladdr.inp46_local.ia46_addr4;
		fa = &inp->inp_dependfaddr.inp46_foreign.ia46_addr4;
	}

	/* Without -a, leave out servers: listeners and unbound sockets. */
	if (!aflag) {
		if (state == TCPS_LISTEN)
			return (false);
		if (state == -1 && inp->inp_fport == 0)
			return (false);
	}

	snprintf(pname, sizeof(pname), "%s%s", proto,
	    v4 && v6 ? "46" : v6 ? "6" : "4");
	endpoint(af, la, inp->inp_lport, proto, local, sizeof(local));
	endpoint(af, fa, inp->inp_fport, proto, foreign, sizeof(foreign));
	printf("%-6s %6u %6u  %-*.*s %-*.*s", pname, so->so_rcv.sb_cc,
	    so->so_snd.sb_cc, w, Wflag ? 255 : w, local, w, Wflag ? 255 : w,
	    foreign);
	if (state >= 0 && state < (int)nitems(tcpstates))
		printf(" %s", tcpstates[state]);
	else if (state >= 0)
		printf(" %d", state);
	printf("\n");
	return (true);
}

static void
show_sockets(const char *proto, bool *header)
{
	char name[64];
	char *buf, *p, *end;
	size_t len;
	bool tcp = strcmp(proto, "tcp") == 0;

	snprintf(name, sizeof(name), "net.inet.%s.pcblist64", proto);
	if ((buf = fetch(NULL, 0, name, &len)) == NULL) {
		warn("%s", name);
		return;
	}
	end = buf + len;
	p = buf + ((struct xinpgen *)(void *)buf)->xig_len;
	while (p + sizeof(struct xinpgen) < end) {
		uint32_t reclen = *(uint32_t *)(void *)p;

		/* The list ends with another xinpgen. */
		if (reclen <= sizeof(struct xinpgen) || p + reclen > end)
			break;
		if (!*header) {
			int w = Wflag ? 45 : 22;

			printf("Active Internet connections%s\n",
			    aflag ? " (including servers)" : "");
			printf("%-6s %6s %6s  %-*s %-*s %s\n", "Proto",
			    "Recv-Q", "Send-Q", w, "Local Address", w,
			    "Foreign Address", "(state)");
			*header = true;
		}
		if (tcp) {
			const struct xtcpcb64 *xt =
			    (const struct xtcpcb64 *)(void *)p;

			show_pcb(proto, &xt->xt_inpcb, xt->t_state);
		} else
			show_pcb(proto, (const struct xinpcb64 *)(void *)p, -1);
		p += reclen;
	}
	free(buf);
}

int
main(int argc, char *argv[])
{
	bool iflag = false, rflag = false, header = false;
	int ch;

	while ((ch = getopt(argc, argv, "af:inp:rW")) != -1) {
		switch (ch) {
		case 'a':
			aflag = true;
			break;
		case 'f':
			if (strcmp(optarg, "inet") == 0)
				family = AF_INET;
			else if (strcmp(optarg, "inet6") == 0)
				family = AF_INET6;
			else
				errx(EX_USAGE, "%s: unknown address family",
				    optarg);
			break;
		case 'i':
			iflag = true;
			break;
		case 'n':
			nflag = true;
			break;
		case 'p':
			if (strcmp(optarg, "tcp") != 0 &&
			    strcmp(optarg, "udp") != 0)
				errx(EX_USAGE, "%s: unknown protocol", optarg);
			protoname = optarg;
			break;
		case 'r':
			rflag = true;
			break;
		case 'W':
			Wflag = true;
			break;
		default:
			usage();
		}
	}
	if (optind != argc || (iflag && rflag))
		usage();

	if (rflag)
		show_routes();
	else if (iflag)
		show_interfaces();
	else {
		if (protoname == NULL || strcmp(protoname, "tcp") == 0)
			show_sockets("tcp", &header);
		if (protoname == NULL || strcmp(protoname, "udp") == 0)
			show_sockets("udp", &header);
	}
	return (0);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: netstat [-anW] [-f inet|inet6] [-p tcp|udp]\n"
	    "       netstat -r [-nW] [-f inet|inet6]\n"
	    "       netstat -i [-nW] [-f inet|inet6]\n");
	exit(EX_USAGE);
}
