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
 * traceroute: print the routers an IPv4 packet passes on its way to a host.
 *
 * Probes leave with TTL 1, 2, ...; each router that drops one answers
 * with ICMP "time exceeded", and the destination answers with "port
 * unreachable" (UDP probes) or "echo reply" (-I). Replies arrive on a
 * raw ICMP socket, which needs root: the program is setuid root and
 * gives that up as soon as its sockets are open.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	DEF_PORT	33434	/* first UDP destination port */
#define	DEF_MAXTTL	64
#define	DEF_NPROBES	3
#define	DEF_WAIT	5	/* seconds */
#define	MAX_PACKET	32768

static bool	nflag;		/* numeric output */
static bool	Iflag;		/* ICMP echo probes */
static int	first_ttl = 1;
static int	max_ttl = DEF_MAXTTL;
static int	nprobes = DEF_NPROBES;
static int	waitsecs = DEF_WAIT;
static int	base_port = DEF_PORT;
static size_t	datalen = 12;	/* payload bytes after the UDP/ICMP header */

static int	sndsock = -1;	/* UDP, or the raw ICMP socket when -I */
static int	rcvsock = -1;	/* raw ICMP */
static struct sockaddr_in dst;
static uint16_t	ident;		/* our UDP source port, or ICMP echo id */
static unsigned	seq;		/* probe counter */

static void	usage(void) __dead2;

static double
elapsed_ms(const struct timeval *a, const struct timeval *b)
{

	return ((b->tv_sec - a->tv_sec) * 1000.0 +
	    (b->tv_usec - a->tv_usec) / 1000.0);
}

static uint16_t
in_cksum(const void *buf, size_t len)
{
	const uint8_t *p = buf;
	uint32_t sum = 0;

	while (len > 1) {
		sum += (uint32_t)p[0] << 8 | p[1];
		p += 2;
		len -= 2;
	}
	if (len == 1)
		sum += (uint32_t)p[0] << 8;
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return (htons((uint16_t)~sum));
}

static void
set_ttl(int ttl)
{

	if (setsockopt(sndsock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == -1)
		err(EX_OSERR, "setsockopt IP_TTL");
}

/* Send probe number seq; returns false when sending failed. */
static bool
send_probe(void)
{
	static uint8_t pkt[MAX_PACKET];
	struct sockaddr_in to = dst;
	size_t len;

	memset(pkt, 0, sizeof(pkt));
	if (Iflag) {
		struct icmp *icp = (struct icmp *)pkt;

		len = ICMP_MINLEN + datalen;
		icp->icmp_type = ICMP_ECHO;
		icp->icmp_code = 0;
		icp->icmp_id = htons(ident);
		icp->icmp_seq = htons((uint16_t)seq);
		icp->icmp_cksum = 0;
		icp->icmp_cksum = in_cksum(pkt, len);
	} else {
		len = datalen;
		to.sin_port = htons((uint16_t)(base_port + seq));
	}
	if (sendto(sndsock, pkt, len, 0, (struct sockaddr *)&to,
	    sizeof(to)) != (ssize_t)len) {
		warn("sendto");
		return (false);
	}
	return (true);
}

/*
 * Look at one ICMP message. Returns -1 if it is not about our probe,
 * otherwise the ICMP type, with *code set to its code.
 */
static int
match_reply(const uint8_t *buf, size_t len, int *code)
{
	const struct ip *ip = (const struct ip *)buf;
	const struct icmp *icp;
	const struct ip *inner;
	size_t hlen, ihlen;

	if (len < sizeof(struct ip))
		return (-1);
	hlen = (size_t)ip->ip_hl << 2;
	if (len < hlen + ICMP_MINLEN)
		return (-1);
	icp = (const struct icmp *)(buf + hlen);
	*code = icp->icmp_code;

	if (Iflag && icp->icmp_type == ICMP_ECHOREPLY) {
		if (ntohs(icp->icmp_id) == ident &&
		    ntohs(icp->icmp_seq) == (uint16_t)seq)
			return (ICMP_ECHOREPLY);
		return (-1);
	}
	if (icp->icmp_type != ICMP_TIMXCEED && icp->icmp_type != ICMP_UNREACH)
		return (-1);

	/* The original IP header and 8 bytes of its payload follow. */
	if (len < hlen + ICMP_MINLEN + sizeof(struct ip))
		return (-1);
	inner = (const struct ip *)(buf + hlen + ICMP_MINLEN);
	ihlen = (size_t)inner->ip_hl << 2;
	if (len < hlen + ICMP_MINLEN + ihlen + 8)
		return (-1);
	if (inner->ip_dst.s_addr != dst.sin_addr.s_addr)
		return (-1);
	if (Iflag) {
		const struct icmp *iicp = (const struct icmp *)
		    ((const uint8_t *)inner + ihlen);

		if (inner->ip_p == IPPROTO_ICMP &&
		    iicp->icmp_type == ICMP_ECHO &&
		    ntohs(iicp->icmp_id) == ident &&
		    ntohs(iicp->icmp_seq) == (uint16_t)seq)
			return (icp->icmp_type);
	} else {
		const struct udphdr *uh = (const struct udphdr *)
		    ((const uint8_t *)inner + ihlen);

		if (inner->ip_p == IPPROTO_UDP &&
		    ntohs(uh->uh_sport) == ident &&
		    ntohs(uh->uh_dport) == (uint16_t)(base_port + seq))
			return (icp->icmp_type);
	}
	return (-1);
}

/*
 * Wait for the reply to the probe just sent. Returns the ICMP type, or
 * -1 on timeout; fills in the sender and the arrival time.
 */
static int
wait_reply(const struct timeval *sent, struct sockaddr_in *from, int *code,
    struct timeval *got)
{
	static uint8_t buf[MAX_PACKET];
	struct pollfd pfd = { .fd = rcvsock, .events = POLLIN };
	socklen_t fromlen;
	ssize_t n;
	double left;
	int type;

	for (;;) {
		gettimeofday(got, NULL);
		left = waitsecs * 1000.0 - elapsed_ms(sent, got);
		if (left <= 0)
			return (-1);
		if (poll(&pfd, 1, (int)left + 1) <= 0) {
			if (errno == EINTR)
				continue;
			return (-1);
		}
		fromlen = sizeof(*from);
		n = recvfrom(rcvsock, buf, sizeof(buf), 0,
		    (struct sockaddr *)from, &fromlen);
		gettimeofday(got, NULL);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			warn("recvfrom");
			return (-1);
		}
		type = match_reply(buf, (size_t)n, code);
		if (type != -1)
			return (type);
	}
}

static void
print_host(const struct sockaddr_in *sin)
{
	char name[NI_MAXHOST];
	const char *addr = inet_ntoa(sin->sin_addr);

	if (nflag || getnameinfo((const struct sockaddr *)sin, sizeof(*sin),
	    name, sizeof(name), NULL, 0, NI_NAMEREQD) != 0)
		printf(" %s", addr);
	else
		printf(" %s (%s)", name, addr);
}

/* The !X marker for an unreachable code, or NULL when none applies. */
static const char *
unreach_mark(int code, bool *reached)
{
	static char other[16];

	switch (code) {
	case ICMP_UNREACH_PORT:
		*reached = true;
		return (Iflag ? "!P" : NULL);
	case ICMP_UNREACH_NET:
	case ICMP_UNREACH_NET_UNKNOWN:
		return ("!N");
	case ICMP_UNREACH_HOST:
	case ICMP_UNREACH_HOST_UNKNOWN:
		return ("!H");
	case ICMP_UNREACH_PROTOCOL:
		*reached = true;
		return ("!P");
	case ICMP_UNREACH_NEEDFRAG:
		return ("!F");
	case ICMP_UNREACH_SRCFAIL:
		return ("!S");
	case ICMP_UNREACH_NET_PROHIB:
	case ICMP_UNREACH_HOST_PROHIB:
	case ICMP_UNREACH_FILTER_PROHIB:
		return ("!X");
	default:
		snprintf(other, sizeof(other), "!<%d>", code);
		return (other);
	}
}

static int
number(const char *s, int min, int max, const char *what)
{
	const char *errstr;
	int v;

	v = (int)strtonum(s, min, max, &errstr);
	if (errstr != NULL)
		errx(EX_USAGE, "%s is %s: %s", what, errstr, s);
	return (v);
}

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	struct sockaddr_in src, from, last;
	struct timeval sent, got;
	const char *srcaddr = NULL;
	char *target;
	int ch, ttl, probe, type, code, error, unreachable, udpsock;
	bool reached, have_last;

	/* Open the privileged socket first, then drop root for good. */
	rcvsock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	error = errno;
	udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (setuid(getuid()) == -1)
		err(EX_OSERR, "setuid");
	if (rcvsock == -1) {
		errno = error;
		err(EX_OSERR, "raw ICMP socket (traceroute must be setuid root)");
	}
	if (udpsock == -1)
		err(EX_OSERR, "UDP socket");

	while ((ch = getopt(argc, argv, "f:Im:np:q:s:w:")) != -1) {
		switch (ch) {
		case 'f':
			first_ttl = number(optarg, 1, 255, "first ttl");
			break;
		case 'I':
			Iflag = true;
			break;
		case 'm':
			max_ttl = number(optarg, 1, 255, "max ttl");
			break;
		case 'n':
			nflag = true;
			break;
		case 'p':
			base_port = number(optarg, 1, 65535, "port");
			break;
		case 'q':
			nprobes = number(optarg, 1, 10, "nprobes");
			break;
		case 's':
			srcaddr = optarg;
			break;
		case 'w':
			waitsecs = number(optarg, 1, 86400, "wait time");
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1 || argc > 2)
		usage();
	if (first_ttl > max_ttl)
		errx(EX_USAGE, "first ttl (%d) is larger than max ttl (%d)",
		    first_ttl, max_ttl);
	target = argv[0];
	if (argc == 2) {
		size_t hdr = sizeof(struct ip) +
		    (Iflag ? ICMP_MINLEN : sizeof(struct udphdr));
		int total = number(argv[1], (int)hdr, MAX_PACKET,
		    "packet length");

		datalen = (size_t)total - hdr;
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	error = getaddrinfo(target, NULL, &hints, &res);
	if (error != 0)
		errx(EX_NOHOST, "%s: %s", target, gai_strerror(error));
	memcpy(&dst, res->ai_addr, sizeof(dst));
	freeaddrinfo(res);

	memset(&src, 0, sizeof(src));
	src.sin_family = AF_INET;
	src.sin_len = sizeof(src);
	if (srcaddr != NULL && inet_pton(AF_INET, srcaddr, &src.sin_addr) != 1)
		errx(EX_USAGE, "bad source address: %s", srcaddr);
	/* ICMP probes go out on the raw socket itself. */
	if (Iflag) {
		close(udpsock);
		sndsock = rcvsock;
	} else
		sndsock = udpsock;
	if (Iflag) {
		ident = (uint16_t)(getpid() & 0xffff);
		if (srcaddr != NULL && bind(sndsock, (struct sockaddr *)&src,
		    sizeof(src)) == -1)
			err(EX_OSERR, "bind %s", srcaddr);
	} else {
		socklen_t len = sizeof(src);

		/* Let the kernel pick our source port; replies quote it. */
		if (bind(sndsock, (struct sockaddr *)&src, sizeof(src)) == -1)
			err(EX_OSERR, "bind");
		if (getsockname(sndsock, (struct sockaddr *)&src, &len) == -1)
			err(EX_OSERR, "getsockname");
		ident = ntohs(src.sin_port);
	}

	fprintf(stderr, "traceroute to %s (%s), %d hops max, %zu byte packets\n",
	    target, inet_ntoa(dst.sin_addr), max_ttl, sizeof(struct ip) +
	    (Iflag ? ICMP_MINLEN : sizeof(struct udphdr)) + datalen);
	fflush(stderr);

	reached = false;
	for (ttl = first_ttl; ttl <= max_ttl && !reached; ttl++) {
		have_last = false;
		unreachable = 0;
		printf("%2d ", ttl);
		set_ttl(ttl);
		for (probe = 0; probe < nprobes; probe++, seq++) {
			gettimeofday(&sent, NULL);
			if (!send_probe()) {
				printf(" *");
				fflush(stdout);
				continue;
			}
			type = wait_reply(&sent, &from, &code, &got);
			if (type == -1) {
				printf(" *");
				fflush(stdout);
				continue;
			}
			if (!have_last ||
			    from.sin_addr.s_addr != last.sin_addr.s_addr) {
				print_host(&from);
				last = from;
				have_last = true;
			}
			printf("  %.3f ms", elapsed_ms(&sent, &got));
			if (type == ICMP_ECHOREPLY) {
				reached = true;
			} else if (type == ICMP_UNREACH) {
				const char *mark = unreach_mark(code, &reached);

				if (mark != NULL) {
					printf(" %s", mark);
					if (!reached)
						unreachable++;
				}
			}
			fflush(stdout);
		}
		printf("\n");
		/* Every probe came back unreachable: no point going on. */
		if (unreachable > 0 && unreachable >= nprobes - 1)
			break;
	}
	return (0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: traceroute [-In] [-f first_ttl] [-m max_ttl] "
	    "[-p port] [-q nqueries]\n"
	    "                  [-s src_addr] [-w waittime] host "
	    "[packetlen]\n");
	exit(EX_USAGE);
}
