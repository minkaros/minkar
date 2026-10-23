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
 * dhcpclient: the DHCP client daemon (RFC 2131), started by launchd.
 *
 *	dhcpclient [-d] [interface ...]
 *
 * Without operands it serves every Ethernet interface, one process each.
 * It brings the interface up and starts IPv6 on it (link-local address and
 * stateless autoconfiguration from router advertisements, as macOS's
 * IPConfiguration does), then gets a lease through BPF (the interface has
 * no address yet), then sets the address, the default route and
 * /var/run/resolv.conf, renews the lease at T1, rebinds at T2 and starts
 * over when the lease expires or the server refuses it. The lease is
 * written to /var/run/dhcpclient/<interface>.lease for ipconfig(8).
 *
 * -d stays in the foreground and logs to stderr as well.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define CLIENT_PORT	68
#define SERVER_PORT	67
#define LEASE_DIR	"/var/run/dhcpclient"
#define RESOLV_CONF	"/var/run/resolv.conf"

/* DHCP message types (option 53). */
enum {
	DHCPDISCOVER = 1, DHCPOFFER, DHCPREQUEST, DHCPDECLINE,
	DHCPACK, DHCPNAK, DHCPRELEASE, DHCPINFORM,
};

/* Options. */
enum {
	OPT_PAD = 0, OPT_SUBNET_MASK = 1, OPT_ROUTER = 3, OPT_DNS = 6,
	OPT_HOSTNAME = 12, OPT_DOMAIN = 15, OPT_MTU = 26, OPT_BROADCAST = 28,
	OPT_REQUESTED_IP = 50, OPT_LEASE_TIME = 51, OPT_MSG_TYPE = 53,
	OPT_SERVER_ID = 54, OPT_PARAM_LIST = 55, OPT_MAX_SIZE = 57,
	OPT_T1 = 58, OPT_T2 = 59, OPT_CLIENT_ID = 61, OPT_END = 255,
};

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

#define MAX_DNS 4

struct lease {
	struct in_addr	addr, mask, router, server, broadcast;
	struct in_addr	dns[MAX_DNS];
	int		ndns;
	char		domain[256];
	uint32_t	lease_time, t1, t2;
	uint16_t	mtu;
	time_t		start;		/* when the lease was granted */
};

struct reply {
	int		type;
	struct lease	lease;
};

static char ifname[IFNAMSIZ];
static uint8_t mac[ETHER_ADDR_LEN];
static int bpf = -1;
static uint8_t *bpfbuf;
static size_t bpflen;
static uint32_t xid;
static time_t started;		/* when this exchange began, for secs */
static bool debug;
static struct lease current;	/* the lease in use; addr 0 if none */

/* ---- Logging ----------------------------------------------------------- */

static void
logmsg(int pri, const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(pri, "%s: %s", ifname, buf);
	if (debug || pri <= LOG_NOTICE)
		fprintf(stderr, "dhcpclient: %s: %s\n", ifname, buf);
}

static time_t
now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}

/* ---- Interface --------------------------------------------------------- */

static int
get_mac(const char *name, uint8_t out[ETHER_ADDR_LEN])
{
	struct ifaddrs *list, *ifa;
	int ret = -1;

	if (getifaddrs(&list) < 0)
		return -1;
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		const struct sockaddr_dl *sdl;

		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_LINK ||
		    strcmp(ifa->ifa_name, name) != 0)
			continue;
		sdl = (const struct sockaddr_dl *)(const void *)ifa->ifa_addr;
		if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == ETHER_ADDR_LEN) {
			memcpy(out, sdl->sdl_data + sdl->sdl_nlen, ETHER_ADDR_LEN);
			ret = 0;
		}
		break;
	}
	freeifaddrs(list);
	return ret;
}

static void
interface_up(void)
{
	struct ifreq ifr;
	int s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s < 0)
		return;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0 && !(ifr.ifr_flags & IFF_UP)) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
			logmsg(LOG_ERR, "cannot bring the interface up: %s", strerror(errno));
	}
	close(s);
}

/*
 * Router solicitations (RFC 4861 6.3.7): the kernel processes the
 * advertisements, but on macOS the configuration agent sends the
 * solicitations. A child sends three, four seconds apart, so DHCP does not
 * wait for them.
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
	unsigned int index = if_nametoindex(ifname);
	int s, hops = 255;
	pid_t pid;

	if (index == 0 || (pid = fork()) != 0)
		return;		/* parent, or no interface */
	if ((s = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		logmsg(LOG_ERR, "IPv6: ICMPv6 socket: %s", strerror(errno));
		_exit(1);
	}
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_IF, &index, sizeof(index));
	setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &hops, sizeof(hops));
	memset(&msg, 0, sizeof(msg));
	msg.rs.nd_rs_type = ND_ROUTER_SOLICIT;
	msg.opt.nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	msg.opt.nd_opt_len = 1;		/* in units of 8 bytes */
	memcpy(msg.lladdr, mac, ETHER_ADDR_LEN);
	memset(&to, 0, sizeof(to));
	to.sin6_len = sizeof(to);
	to.sin6_family = AF_INET6;
	inet_pton(AF_INET6, "ff02::2", &to.sin6_addr);	/* all routers */
	to.sin6_scope_id = index;
	for (int i = 0; i < 3; i++) {
		sleep(i == 0 ? 1 : 4);	/* the link-local address first */
		if (sendto(s, &msg, sizeof(msg), 0, (struct sockaddr *)&to, sizeof(to)) < 0)
			logmsg(LOG_ERR, "IPv6: router solicitation: %s", strerror(errno));
	}
	_exit(0);
}

/*
 * IPv6: attach the protocol, configure the link-local address and accept
 * router advertisements (SLAAC). Failures are logged, IPv4 goes on.
 */
static void
ipv6_start(void)
{
	struct in6_aliasreq ifra;
	struct in6_ifreq ifr6;
	int s = socket(AF_INET6, SOCK_DGRAM, 0);

	if (s < 0) {
		logmsg(LOG_ERR, "IPv6: socket: %s", strerror(errno));
		return;
	}
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	if (ioctl(s, SIOCPROTOATTACH_IN6, &ifra) < 0 && errno != EEXIST)
		logmsg(LOG_ERR, "IPv6: SIOCPROTOATTACH_IN6: %s", strerror(errno));
	/* No address given: the kernel makes the link-local one. */
	if (ioctl(s, SIOCLL_START, &ifra) < 0 && errno != EEXIST)
		logmsg(LOG_ERR, "IPv6: SIOCLL_START: %s", strerror(errno));
	memset(&ifr6, 0, sizeof(ifr6));
	strlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
	if (ioctl(s, SIOCAUTOCONF_START, &ifr6) < 0)
		logmsg(LOG_ERR, "IPv6: SIOCAUTOCONF_START: %s", strerror(errno));
	close(s);
	solicit_routers();
}

static void
fill_sin(struct sockaddr_in *sin, struct in_addr addr)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = addr;
}

static void
delete_address(struct in_addr addr)
{
	struct ifreq ifr;
	int s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s < 0)
		return;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	fill_sin((struct sockaddr_in *)(void *)&ifr.ifr_addr, addr);
	(void)ioctl(s, SIOCDIFADDR, &ifr);
	close(s);
}

static int
set_address(const struct lease *l)
{
	struct ifaliasreq ifra;
	struct in_addr bcast = l->broadcast;
	int s, ret;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return -1;
	if (bcast.s_addr == 0)
		bcast.s_addr = l->addr.s_addr | ~l->mask.s_addr;
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
	fill_sin((struct sockaddr_in *)(void *)&ifra.ifra_addr, l->addr);
	fill_sin((struct sockaddr_in *)(void *)&ifra.ifra_mask, l->mask);
	fill_sin((struct sockaddr_in *)(void *)&ifra.ifra_broadaddr, bcast);
	ret = ioctl(s, SIOCAIFADDR, &ifra);
	if (ret < 0)
		logmsg(LOG_ERR, "SIOCAIFADDR: %s", strerror(errno));
	if (ret == 0 && l->mtu >= 576) {
		struct ifreq ifr;

		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
		ifr.ifr_mtu = l->mtu;
		(void)ioctl(s, SIOCSIFMTU, &ifr);
	}
	close(s);
	return ret;
}

/* Adds (or with delete, removes) the default route through gateway. */
static int
default_route(int cmd, struct in_addr gateway)
{
	struct {
		struct rt_msghdr	hdr;
		struct sockaddr_in	dst, gw, mask;
	} msg;
	struct in_addr any = { 0 };
	int s, ret;

	if ((s = socket(PF_ROUTE, SOCK_RAW, AF_INET)) < 0)
		return -1;
	memset(&msg, 0, sizeof(msg));
	msg.hdr.rtm_msglen = sizeof(msg);
	msg.hdr.rtm_version = RTM_VERSION;
	msg.hdr.rtm_type = (u_char)cmd;
	msg.hdr.rtm_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	msg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	msg.hdr.rtm_pid = getpid();
	msg.hdr.rtm_seq = 1;
	fill_sin(&msg.dst, any);
	fill_sin(&msg.gw, gateway);
	fill_sin(&msg.mask, any);
	ret = (int)write(s, &msg, sizeof(msg));
	if (ret < 0 && !(cmd == RTM_DELETE && errno == ESRCH))
		logmsg(LOG_ERR, "default route %s %s: %s",
		    cmd == RTM_ADD ? "add" : "delete", inet_ntoa(gateway), strerror(errno));
	close(s);
	return ret < 0 ? -1 : 0;
}

/*
 * mDNSResponder reads the name servers from /etc/resolv.conf at startup
 * and again on SIGHUP. (On macOS configd tells it; ravynOS has no configd
 * DNS service yet.)
 */
static void
notify_resolver(void)
{
	int mib[3] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL };
	struct kinfo_proc *procs;
	size_t len = 0;

	if (sysctl(mib, 3, NULL, &len, NULL, 0) < 0 || len == 0)
		return;
	len += len / 8;		/* processes may start meanwhile */
	if ((procs = malloc(len)) == NULL)
		return;
	if (sysctl(mib, 3, procs, &len, NULL, 0) == 0) {
		for (size_t i = 0; i < len / sizeof(*procs); i++) {
			if (strcmp(procs[i].kp_proc.p_comm, "mDNSResponder") == 0)
				kill(procs[i].kp_proc.p_pid, SIGHUP);
		}
	}
	free(procs);
}

static void
write_resolv_conf(const struct lease *l)
{
	FILE *f;

	if (l->ndns == 0)
		return;
	if ((f = fopen(RESOLV_CONF ".tmp", "w")) == NULL) {
		logmsg(LOG_ERR, "%s: %s", RESOLV_CONF, strerror(errno));
		return;
	}
	fprintf(f, "#\n# Written by dhcpclient for %s.\n#\n", ifname);
	if (l->domain[0] != '\0')
		fprintf(f, "domain %s\n", l->domain);
	for (int i = 0; i < l->ndns; i++)
		fprintf(f, "nameserver %s\n", inet_ntoa(l->dns[i]));
	fclose(f);
	rename(RESOLV_CONF ".tmp", RESOLV_CONF);
	notify_resolver();
}

/* /var/run/dhcpclient/<if>.lease: one "key=value" per line, for ipconfig. */
static void
write_lease_file(const struct lease *l)
{
	char path[64], tmp[72];
	FILE *f;

	mkdir(LEASE_DIR, 0755);
	snprintf(path, sizeof(path), LEASE_DIR "/%s.lease", ifname);
	if (l == NULL) {
		unlink(path);
		return;
	}
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	if ((f = fopen(tmp, "w")) == NULL)
		return;
	fprintf(f, "ip_address=%s\n", inet_ntoa(l->addr));
	fprintf(f, "subnet_mask=%s\n", inet_ntoa(l->mask));
	if (l->router.s_addr != 0)
		fprintf(f, "router=%s\n", inet_ntoa(l->router));
	fprintf(f, "server_identifier=%s\n", inet_ntoa(l->server));
	if (l->ndns > 0) {
		fprintf(f, "domain_name_server=");
		for (int i = 0; i < l->ndns; i++)
			fprintf(f, "%s%s", i ? " " : "", inet_ntoa(l->dns[i]));
		fprintf(f, "\n");
	}
	if (l->domain[0] != '\0')
		fprintf(f, "domain_name=%s\n", l->domain);
	if (l->mtu != 0)
		fprintf(f, "interface_mtu=%u\n", l->mtu);
	fprintf(f, "lease_time=%u\n", l->lease_time);
	fprintf(f, "renewal_t1_time_value=%u\n", l->t1);
	fprintf(f, "rebinding_t2_time_value=%u\n", l->t2);
	fclose(f);
	rename(tmp, path);
}

static void
apply_lease(const struct lease *l)
{
	bool same = current.addr.s_addr == l->addr.s_addr &&
	    current.mask.s_addr == l->mask.s_addr;

	if (current.addr.s_addr != 0 && !same)
		delete_address(current.addr);
	if (!same && set_address(l) < 0)
		return;
	if (current.router.s_addr != 0 &&
	    (!same || current.router.s_addr != l->router.s_addr))
		default_route(RTM_DELETE, current.router);
	if (l->router.s_addr != 0 &&
	    (!same || current.router.s_addr != l->router.s_addr)) {
		default_route(RTM_DELETE, l->router);	/* a stale one */
		default_route(RTM_ADD, l->router);
	}
	write_resolv_conf(l);
	write_lease_file(l);
	if (!same || current.router.s_addr != l->router.s_addr) {
		char a[INET_ADDRSTRLEN], r[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &l->addr, a, sizeof(a));
		inet_ntop(AF_INET, &l->router, r, sizeof(r));
		logmsg(LOG_NOTICE, "bound to %s/%d, router %s, lease %us", a,
		    __builtin_popcount(ntohl(l->mask.s_addr)), r, l->lease_time);
	}
	current = *l;
}

static void
drop_lease(void)
{
	if (current.addr.s_addr == 0)
		return;
	logmsg(LOG_NOTICE, "lease on %s lost", inet_ntoa(current.addr));
	if (current.router.s_addr != 0)
		default_route(RTM_DELETE, current.router);
	delete_address(current.addr);
	write_lease_file(NULL);
	memset(&current, 0, sizeof(current));
}

/* ---- BPF --------------------------------------------------------------- */

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

static int
open_bpf(void)
{
	struct bpf_program prog = {
		sizeof(dhcp_filter) / sizeof(dhcp_filter[0]), dhcp_filter,
	};
	struct ifreq ifr;
	u_int on = 1, len;
	char dev[16];

	for (int i = 0; i < 256; i++) {
		snprintf(dev, sizeof(dev), "/dev/bpf%d", i);
		if ((bpf = open(dev, O_RDWR)) >= 0 || errno != EBUSY)
			break;
	}
	if (bpf < 0) {
		logmsg(LOG_ERR, "no BPF device: %s", strerror(errno));
		return -1;
	}
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(bpf, BIOCGBLEN, &len) < 0 || ioctl(bpf, BIOCSETIF, &ifr) < 0 ||
	    ioctl(bpf, BIOCIMMEDIATE, &on) < 0 || ioctl(bpf, BIOCSHDRCMPLT, &on) < 0 ||
	    ioctl(bpf, BIOCSETF, &prog) < 0) {
		logmsg(LOG_ERR, "BPF setup: %s", strerror(errno));
		close(bpf);
		bpf = -1;
		return -1;
	}
	bpflen = len;
	if ((bpfbuf = malloc(bpflen)) == NULL)
		return -1;
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

/* Sends a DHCP message from 0.0.0.0 (or src) to the broadcast address. */
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
	memcpy(eh->ether_shost, mac, ETHER_ADDR_LEN);
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
	if (write(bpf, frame, total) != (ssize_t)total) {
		logmsg(LOG_ERR, "BPF write: %s", strerror(errno));
		return -1;
	}
	return 0;
}

/* Sends to the server from our address (RENEWING). */
static int
send_unicast(const struct dhcp_packet *pkt, size_t len, struct in_addr src,
    struct in_addr server)
{
	struct sockaddr_in from, to;
	int s, on = 1, ret;

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
	ret = -1;
	if (bind(s, (struct sockaddr *)&from, sizeof(from)) == 0 &&
	    sendto(s, pkt, len, 0, (struct sockaddr *)&to, sizeof(to)) == (ssize_t)len)
		ret = 0;
	else
		logmsg(LOG_ERR, "send to %s: %s", inet_ntoa(server), strerror(errno));
	close(s);
	return ret;
}

/* ---- Messages ---------------------------------------------------------- */

static uint8_t *
add_option(uint8_t *p, uint8_t code, uint8_t len, const void *data)
{
	*p++ = code;
	*p++ = len;
	memcpy(p, data, len);
	return p + len;
}

/* Builds a message; returns its length. */
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
	time_t secs = now() - started;

	memset(pkt, 0, sizeof(*pkt));
	pkt->op = 1;			/* BOOTREQUEST */
	pkt->htype = 1;			/* Ethernet */
	pkt->hlen = ETHER_ADDR_LEN;
	pkt->xid = xid;
	pkt->secs = htons((uint16_t)(secs > 0xffff ? 0xffff : secs));
	pkt->ciaddr = ciaddr;
	if (ciaddr.s_addr == 0)
		pkt->flags = htons(0x8000);	/* broadcast the reply */
	memcpy(pkt->chaddr, mac, ETHER_ADDR_LEN);
	memcpy(pkt->cookie, magic_cookie, sizeof(magic_cookie));

	p = pkt->options;
	p = add_option(p, OPT_MSG_TYPE, 1, &t);
	client_id[0] = 1;
	memcpy(client_id + 1, mac, ETHER_ADDR_LEN);
	p = add_option(p, OPT_CLIENT_ID, sizeof(client_id), client_id);
	if (requested.s_addr != 0)
		p = add_option(p, OPT_REQUESTED_IP, 4, &requested);
	if (server.s_addr != 0)
		p = add_option(p, OPT_SERVER_ID, 4, &server);
	p = add_option(p, OPT_MAX_SIZE, 2, &max);
	p = add_option(p, OPT_PARAM_LIST, sizeof(params), params);
	*p++ = OPT_END;
	return (size_t)(p - (uint8_t *)pkt) < 300 ? 300 : (size_t)(p - (uint8_t *)pkt);
}

static uint32_t
get32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* Parses a reply for us; returns its message type, or 0 if it is not one. */
static int
parse(const uint8_t *data, size_t len, struct reply *r)
{
	const struct dhcp_packet *pkt = (const struct dhcp_packet *)(const void *)data;
	const uint8_t *p, *end;
	struct lease *l = &r->lease;

	memset(r, 0, sizeof(*r));
	if (len < offsetof(struct dhcp_packet, options) || pkt->op != 2 ||
	    pkt->xid != xid || memcmp(pkt->chaddr, mac, ETHER_ADDR_LEN) != 0 ||
	    memcmp(pkt->cookie, magic_cookie, sizeof(magic_cookie)) != 0)
		return 0;
	l->addr = pkt->yiaddr;
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
				r->type = p[0];
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
	if (r->type == 0)
		return 0;
	if (l->mask.s_addr == 0) {
		/* The mask of the address's class, as old clients assume. */
		uint32_t a = ntohl(l->addr.s_addr);

		l->mask.s_addr = htonl(IN_CLASSA(a) ? IN_CLASSA_NET :
		    IN_CLASSB(a) ? IN_CLASSB_NET : IN_CLASSC_NET);
	}
	if (l->lease_time == 0)
		l->lease_time = 3600;
	if (l->t1 == 0 || l->t1 >= l->lease_time)
		l->t1 = l->lease_time / 2;
	if (l->t2 == 0 || l->t2 >= l->lease_time || l->t2 <= l->t1)
		l->t2 = (uint32_t)((uint64_t)l->lease_time * 7 / 8);
	return r->type;
}

/*
 * Waits until deadline (monotonic seconds) for a reply of one of the
 * wanted types. Returns its type, or 0 on timeout.
 */
static int
wait_reply(time_t deadline, int want1, int want2, struct reply *r)
{
	for (;;) {
		struct pollfd pfd = { bpf, POLLIN, 0 };
		time_t left = deadline - now();
		ssize_t n;

		if (left <= 0)
			return 0;
		if (poll(&pfd, 1, (int)(left > 60 ? 60000 : left * 1000)) <= 0)
			continue;
		if ((n = read(bpf, bpfbuf, bpflen)) <= 0)
			continue;
		for (uint8_t *p = bpfbuf; p < bpfbuf + n;) {
			const struct bpf_hdr *bh = (const struct bpf_hdr *)(void *)p;
			const uint8_t *frame = p + bh->bh_hdrlen;
			size_t caplen = bh->bh_caplen;
			size_t off;

			p += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
			if (caplen < ETHER_HDR_LEN + sizeof(struct ip) + sizeof(struct udphdr))
				continue;
			off = ETHER_HDR_LEN + (size_t)(frame[ETHER_HDR_LEN] & 0x0f) * 4 +
			    sizeof(struct udphdr);
			if (off >= caplen)
				continue;
			int type = parse(frame + off, caplen - off, r);
			if (type != 0 && (type == want1 || type == want2))
				return type;
		}
	}
}

/* Retransmission delay: 4, 8, 16, 32, 64, 64... seconds, +-1 s. */
static time_t
backoff(int attempt)
{
	time_t d = 4 << (attempt < 4 ? attempt : 4);

	return d - 1 + (time_t)arc4random_uniform(3);
}

/* ---- The client -------------------------------------------------------- */

/* INIT and SELECTING, then REQUESTING: returns 0 with the lease in *r. */
static int
obtain(struct reply *r)
{
	struct dhcp_packet pkt;
	struct in_addr none = { 0 };
	struct reply offer;
	size_t len;

	for (int attempt = 0;; attempt++) {
		xid = arc4random();
		if (attempt == 0)
			started = now();
		len = build(&pkt, DHCPDISCOVER, none, none, none);
		send_broadcast(&pkt, len, none);
		if (wait_reply(now() + backoff(attempt), DHCPOFFER, 0, &offer) == 0)
			continue;
		logmsg(LOG_INFO, "offer of %s from %s", inet_ntoa(offer.lease.addr),
		    inet_ntoa(offer.lease.server));

		for (int req = 0; req < 4; req++) {
			len = build(&pkt, DHCPREQUEST, none, offer.lease.addr,
			    offer.lease.server);
			send_broadcast(&pkt, len, none);
			switch (wait_reply(now() + backoff(req), DHCPACK, DHCPNAK, r)) {
			case DHCPACK:
				r->lease.start = now();
				if (r->lease.server.s_addr == 0)
					r->lease.server = offer.lease.server;
				return 0;
			case DHCPNAK:
				logmsg(LOG_INFO, "server refused %s", inet_ntoa(offer.lease.addr));
				req = 4;
				break;
			}
		}
	}
}

/*
 * BOUND, RENEWING and REBINDING: keeps the lease alive. Returns when the
 * lease is gone (expired or refused).
 */
static void
keep(void)
{
	struct dhcp_packet pkt;
	struct in_addr none = { 0 };
	struct reply r;
	size_t len;

	for (;;) {
		time_t t1 = current.start + current.t1;
		time_t t2 = current.start + current.t2;
		time_t expiry = current.start + current.lease_time;
		time_t delay;
		int type;

		/* BOUND: sleep until T1, dropping whatever BPF sees. */
		while (now() < t1)
			(void)wait_reply(t1, -1, -1, &r);

		xid = arc4random();
		started = now();
		type = 0;
		while (type == 0 && now() < expiry) {
			bool rebinding = now() >= t2;

			len = build(&pkt, DHCPREQUEST, current.addr, none, none);
			if (rebinding)
				send_broadcast(&pkt, len, current.addr);
			else
				send_unicast(&pkt, len, current.addr, current.server);
			/* Half the time left, at least a minute (RFC 2131 4.4.5). */
			delay = ((rebinding ? expiry : t2) - now()) / 2;
			if (delay < 60)
				delay = 60;
			if (now() + delay > expiry)
				delay = expiry - now();
			type = wait_reply(now() + delay, DHCPACK, DHCPNAK, &r);
		}
		if (type == DHCPACK && r.lease.addr.s_addr == current.addr.s_addr) {
			r.lease.start = now();
			if (r.lease.server.s_addr == 0)
				r.lease.server = current.server;
			apply_lease(&r.lease);
			continue;
		}
		if (type == DHCPNAK)
			logmsg(LOG_NOTICE, "server refused the renewal");
		drop_lease();
		return;
	}
}

static void __dead2
serve(const char *name)
{
	struct reply r;

	strlcpy(ifname, name, sizeof(ifname));
	openlog("dhcpclient", LOG_PID, LOG_DAEMON);
	signal(SIGCHLD, SIG_IGN);	/* the router solicitation child */
	if (get_mac(ifname, mac) < 0) {
		logmsg(LOG_ERR, "not an Ethernet interface");
		exit(1);
	}
	interface_up();
	ipv6_start();
	if (open_bpf() < 0)
		exit(1);
	for (;;) {
		if (obtain(&r) == 0) {
			apply_lease(&r.lease);
			keep();
		}
	}
}

/* Every Ethernet interface. */
static int
ethernet_interfaces(char names[][IFNAMSIZ], int max)
{
	struct ifaddrs *list, *ifa;
	int n = 0;

	if (getifaddrs(&list) < 0)
		return 0;
	for (ifa = list; ifa != NULL && n < max; ifa = ifa->ifa_next) {
		const struct sockaddr_dl *sdl;

		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		sdl = (const struct sockaddr_dl *)(const void *)ifa->ifa_addr;
		if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == ETHER_ADDR_LEN)
			strlcpy(names[n++], ifa->ifa_name, IFNAMSIZ);
	}
	freeifaddrs(list);
	return n;
}

int
main(int argc, char *argv[])
{
	char names[16][IFNAMSIZ];
	int ch, n;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug = true;
			break;
		default:
			fprintf(stderr, "usage: dhcpclient [-d] [interface ...]\n");
			return 64;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		n = 0;
		for (int i = 0; i < argc && n < 16; i++)
			strlcpy(names[n++], argv[i], IFNAMSIZ);
	} else {
		n = ethernet_interfaces(names, 16);
	}
	if (n == 0) {
		fprintf(stderr, "dhcpclient: no Ethernet interfaces\n");
		/* launchd would restart us at once: wait for a while instead. */
		sleep(60);
		return 1;
	}
	if (n == 1)
		serve(names[0]);

	/* One process per interface; exit (and be restarted) if any ends. */
	for (int i = 0; i < n; i++) {
		pid_t pid = fork();

		if (pid == 0)
			serve(names[i]);
		if (pid < 0)
			err(1, "fork");
	}
	(void)wait(NULL);
	signal(SIGTERM, SIG_IGN);
	kill(0, SIGTERM);
	return 1;
}
