/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * The worker of one interface: its event loop, the control socket that
 * ipconfig(8) talks to, and what configuring IPv4 means (address, default
 * route, resolv.conf, the runtime lease file).
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/un.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/net_kev.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpclient.h"

#define STORE_RETRY	5	/* seconds between looks for our configd service */

struct ifstate ifs;
static int kev = -1;		/* kernel network events */

void
logmsg(int pri, const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(pri, "%s: %s", ifs.name, buf);
	if (debug || pri <= LOG_NOTICE)
		fprintf(stderr, "dhcpclient: %s: %s\n", ifs.name, buf);
}

time_t
now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec;
}

time_t
wallclock(void)
{
	return time(NULL);
}

/* ---- Addresses and routes ---------------------------------------------- */

static void
fill_sin(struct sockaddr_in *sin, struct in_addr addr)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	sin->sin_addr = addr;
}

void
delete_address(struct in_addr addr)
{
	struct ifreq ifr;
	int s = socket(AF_INET, SOCK_DGRAM, 0);

	if (s < 0)
		return;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifs.name, sizeof(ifr.ifr_name));
	fill_sin((struct sockaddr_in *)(void *)&ifr.ifr_addr, addr);
	(void)ioctl(s, SIOCDIFADDR, &ifr);
	close(s);
}

int
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
	strlcpy(ifra.ifra_name, ifs.name, sizeof(ifra.ifra_name));
	fill_sin((struct sockaddr_in *)(void *)&ifra.ifra_addr, l->addr);
	fill_sin((struct sockaddr_in *)(void *)&ifra.ifra_mask, l->mask);
	fill_sin((struct sockaddr_in *)(void *)&ifra.ifra_broadaddr, bcast);
	ret = ioctl(s, SIOCAIFADDR, &ifra);
	if (ret < 0)
		logmsg(LOG_ERR, "SIOCAIFADDR: %s", strerror(errno));
	if (ret == 0 && l->mtu >= 576) {
		struct ifreq ifr;

		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, ifs.name, sizeof(ifr.ifr_name));
		ifr.ifr_mtu = l->mtu;
		(void)ioctl(s, SIOCSIFMTU, &ifr);
	}
	close(s);
	return ret;
}

/* Adds (or with RTM_DELETE, removes) the default route through gateway. */
int
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

/* ---- resolv.conf and the runtime files --------------------------------- */

/*
 * mDNSResponder reads the name servers from /etc/resolv.conf at startup
 * and again on SIGHUP. (On macOS configd tells it.)
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
		for (size_t i = 0; i < len / sizeof(*procs); i++)
			if (strcmp(procs[i].kp_proc.p_comm, "mDNSResponder") == 0)
				kill(procs[i].kp_proc.p_pid, SIGHUP);
	}
	free(procs);
}

/*
 * The name servers of DHCPv4 (or INFORM), DHCPv6 and the routers' RDNSS
 * options, with the first domain any of them gave.
 */
static void
write_resolv_conf(void)
{
	char buf[INET6_ADDRSTRLEN];
	const char *domain = ifs.current.domain[0] ? ifs.current.domain : ifs.domain6;
	FILE *f;

	if (ifs.current.ndns == 0 && ifs.ndns6 == 0 && ifs.nrdnss == 0)
		return;
	if ((f = fopen(RESOLV_CONF ".tmp", "w")) == NULL) {
		logmsg(LOG_ERR, "%s: %s", RESOLV_CONF, strerror(errno));
		return;
	}
	fprintf(f, "#\n# Written by dhcpclient for %s.\n#\n", ifs.name);
	if (domain[0] != '\0')
		fprintf(f, "domain %s\n", domain);
	for (int i = 0; i < ifs.current.ndns; i++)
		fprintf(f, "nameserver %s\n", inet_ntoa(ifs.current.dns[i]));
	for (int i = 0; i < ifs.ndns6; i++)
		fprintf(f, "nameserver %s\n", inet_ntop(AF_INET6, &ifs.dns6[i], buf, sizeof(buf)));
	for (int i = 0; i < ifs.nrdnss; i++) {
		bool dup = false;

		for (int j = 0; j < ifs.ndns6; j++)
			dup |= memcmp(&ifs.dns6[j], &ifs.rdnss[i], sizeof(struct in6_addr)) == 0;
		if (!dup)
			fprintf(f, "nameserver %s\n",
			    inet_ntop(AF_INET6, &ifs.rdnss[i], buf, sizeof(buf)));
	}
	fclose(f);
	rename(RESOLV_CONF ".tmp", RESOLV_CONF);
	notify_resolver();
}

/*
 * What the worker configured: to configd when it has our service (store.c;
 * IPMonitor writes resolv.conf), otherwise to resolv.conf directly.
 */
void
publish_state(void)
{
	if (!store_publish())
		write_resolv_conf();
}

/* RUN_DIR/<if>.<suffix>, replaced whole; text NULL removes it. */
void
write_runtime_file(const char *suffix, const char *text)
{
	char path[128], tmp[136];
	FILE *f;

	mkdir(RUN_DIR, 0755);
	snprintf(path, sizeof(path), RUN_DIR "/%s.%s", ifs.name, suffix);
	if (text == NULL) {
		unlink(path);
		return;
	}
	snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	if ((f = fopen(tmp, "w")) == NULL)
		return;
	fputs(text, f);
	fclose(f);
	rename(tmp, path);
}

/* RUN_DIR/<if>.lease: one "key=value" per line, for ipconfig. */
static void
write_lease_file(const struct lease *l)
{
	char text[2048];
	size_t n = 0;

#define ADD(...) n += (size_t)snprintf(text + n, n < sizeof(text) ? sizeof(text) - n : 0, __VA_ARGS__)
	ADD("ip_address=%s\n", inet_ntoa(l->addr));
	ADD("subnet_mask=%s\n", inet_ntoa(l->mask));
	if (l->router.s_addr != 0)
		ADD("router=%s\n", inet_ntoa(l->router));
	if (l->server.s_addr != 0)
		ADD("server_identifier=%s\n", inet_ntoa(l->server));
	if (l->ndns > 0) {
		ADD("domain_name_server=");
		for (int i = 0; i < l->ndns; i++)
			ADD("%s%s", i ? " " : "", inet_ntoa(l->dns[i]));
		ADD("\n");
	}
	if (l->domain[0] != '\0')
		ADD("domain_name=%s\n", l->domain);
	if (l->mtu != 0)
		ADD("interface_mtu=%u\n", l->mtu);
	if (l->lease_time != 0) {
		ADD("lease_time=%u\n", l->lease_time);
		ADD("renewal_t1_time_value=%u\n", l->t1);
		ADD("rebinding_t2_time_value=%u\n", l->t2);
	}
	static const char *methods[] = { "NONE", "DHCP", "BOOTP", "MANUAL", "INFORM" };
	ADD("method=%s\n", methods[ifs.method4]);
#undef ADD
	write_runtime_file("lease", text);
}

/* Configures (or updates) IPv4 from l: address, route, resolv.conf. */
void
apply_lease(const struct lease *l)
{
	bool same = ifs.current.addr.s_addr == l->addr.s_addr &&
	    ifs.current.mask.s_addr == l->mask.s_addr;
	bool route = ifs.current.router.s_addr != l->router.s_addr;

	if (ifs.current.addr.s_addr != 0 && !same)
		delete_address(ifs.current.addr);
	if (!same && set_address(l) < 0)
		return;
	/* With configd, IPMonitor adds the default route of the primary service. */
	if (ifs.route.s_addr != 0 && (!same || route || store_active())) {
		default_route(RTM_DELETE, ifs.route);
		ifs.route.s_addr = 0;
	}
	if (l->router.s_addr != 0 && (!same || route) && !store_active()) {
		default_route(RTM_DELETE, l->router);	/* a stale one */
		if (default_route(RTM_ADD, l->router) == 0)
			ifs.route = l->router;
	}
	if (!same || route) {
		char a[INET_ADDRSTRLEN], r[INET_ADDRSTRLEN];

		inet_ntop(AF_INET, &l->addr, a, sizeof(a));
		inet_ntop(AF_INET, &l->router, r, sizeof(r));
		if (l->lease_time != 0)
			logmsg(LOG_NOTICE, "bound to %s/%d, router %s, lease %us", a,
			    __builtin_popcount(ntohl(l->mask.s_addr)), r, l->lease_time);
		else
			logmsg(LOG_NOTICE, "configured %s/%d, router %s", a,
			    __builtin_popcount(ntohl(l->mask.s_addr)), r);
	}
	ifs.current = *l;
	publish_state();
	write_lease_file(l);
}

void
drop_lease(void)
{
	if (ifs.current.addr.s_addr == 0)
		return;
	logmsg(LOG_NOTICE, "%s removed", inet_ntoa(ifs.current.addr));
	if (ifs.route.s_addr != 0) {
		default_route(RTM_DELETE, ifs.route);
		ifs.route.s_addr = 0;
	}
	delete_address(ifs.current.addr);
	write_runtime_file("lease", NULL);
	memset(&ifs.current, 0, sizeof(ifs.current));
	publish_state();
}

/* ---- The interface ----------------------------------------------------- */

static int
get_link(const char *name)
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
			memcpy(ifs.mac, sdl->sdl_data + sdl->sdl_nlen, ETHER_ADDR_LEN);
			ifs.index = sdl->sdl_index;
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
	strlcpy(ifr.ifr_name, ifs.name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0 && !(ifr.ifr_flags & IFF_UP)) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
			logmsg(LOG_ERR, "cannot bring the interface up: %s", strerror(errno));
	}
	close(s);
}

/*
 * Kernel network events: link up and down, and IPv6 address changes (the
 * kernel's SLAAC), which configd hears of from us.
 */
static void
open_kev(void)
{
	struct kev_request req = { KEV_VENDOR_APPLE, KEV_NETWORK_CLASS, KEV_ANY_SUBCLASS };

	if ((kev = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT)) < 0)
		return;
	if (ioctl(kev, SIOCSKEVFILT, &req) < 0) {
		close(kev);
		kev = -1;
	}
}

static void
kev_input(void)
{
	uint32_t buf[256];
	struct kern_event_msg *ev = (struct kern_event_msg *)buf;
	struct net_event_data *nd = (struct net_event_data *)&ev->event_data[0];
	char name[IFNAMSIZ];
	ssize_t n = recv(kev, buf, sizeof(buf), 0);

	if (n < (ssize_t)(KEV_MSG_HEADER_SIZE + sizeof(*nd)))
		return;
	snprintf(name, sizeof(name), "%s%u", nd->if_name, nd->if_unit);
	if (strcmp(name, ifs.name) != 0)
		return;
	if (ev->kev_subclass == KEV_INET6_SUBCLASS) {
		/* Every kev_in6_data starts with the net_event_data. */
		if (store_active())
			publish_state();
		return;
	}
	if (ev->kev_subclass != KEV_DL_SUBCLASS)
		return;
	switch (ev->event_code) {
	case KEV_DL_LINK_ON:
		logmsg(LOG_INFO, "link up");
		dhcp4_link_up();
		if (ifs.method6 == M6_AUTOMATIC)
			ipv6_start();	/* solicit routers again */
		break;
	case KEV_DL_IF_DETACHING:
	case KEV_DL_IF_DETACHED:
		logmsg(LOG_NOTICE, "interface gone");
		exit(0);
	}
}

/* ---- ipconfig's control socket ----------------------------------------- */

static int
open_control(void)
{
	struct sockaddr_un sun;
	int s;

	mkdir(RUN_DIR, 0755);
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path), RUN_DIR "/%s.ctl", ifs.name);
	unlink(sun.sun_path);
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	if (bind(s, (struct sockaddr *)&sun, SUN_LEN(&sun)) < 0 || listen(s, 4) < 0) {
		close(s);
		return -1;
	}
	chmod(sun.sun_path, 0600);	/* root only */
	return s;
}

static bool
parse_in(const char *s, struct in_addr *a)
{
	return s != NULL && inet_pton(AF_INET, s, a) == 1;
}

/*
 * One command, "set <METHOD> [args]", as macOS's ipconfig names them:
 * DHCP, BOOTP, MANUAL addr mask, INFORM addr mask, NONE; AUTOMATIC-V6,
 * MANUAL-V6 addr prefixlen, LINKLOCAL-V6, NONE-V6.
 */
static const char *
control_command(char *line)
{
	char *argv[5];
	int argc = 0;
	struct in_addr a, m;

	for (char *p = strtok(line, " \t\r\n"); p != NULL && argc < 5; p = strtok(NULL, " \t\r\n"))
		argv[argc++] = p;
	if (argc < 2 || strcmp(argv[0], "set") != 0)
		return "error: usage: set <method> [args]";

	if (strcasecmp(argv[1], "DHCP") == 0 || strcasecmp(argv[1], "BOOTP") == 0 ||
	    strcasecmp(argv[1], "NONE") == 0 || strcasecmp(argv[1], "MANUAL") == 0 ||
	    strcasecmp(argv[1], "INFORM") == 0) {
		int method;

		if (strcasecmp(argv[1], "DHCP") == 0)
			method = M4_DHCP;
		else if (strcasecmp(argv[1], "BOOTP") == 0)
			method = M4_BOOTP;
		else if (strcasecmp(argv[1], "NONE") == 0)
			method = M4_NONE;
		else {
			if (argc != 4 || !parse_in(argv[2], &a) || !parse_in(argv[3], &m))
				return "error: MANUAL and INFORM take an address and a mask";
			method = strcasecmp(argv[1], "MANUAL") == 0 ? M4_MANUAL : M4_INFORM;
			ifs.manual_addr = a;
			ifs.manual_mask = m;
		}
		dhcp4_stop();
		drop_lease();
		ifs.method4 = method;
		logmsg(LOG_NOTICE, "IPv4: %s", argv[1]);
		dhcp4_start();
		return "ok";
	}
	if (strcasecmp(argv[1], "AUTOMATIC-V6") == 0 || strcasecmp(argv[1], "NONE-V6") == 0 ||
	    strcasecmp(argv[1], "LINKLOCAL-V6") == 0 || strcasecmp(argv[1], "MANUAL-V6") == 0) {
		int method;

		if (strcasecmp(argv[1], "MANUAL-V6") == 0) {
			struct in6_addr a6;
			char *end;
			long plen;

			if (argc != 4 || inet_pton(AF_INET6, argv[2], &a6) != 1 ||
			    (plen = strtol(argv[3], &end, 10)) < 1 || plen > 128 || *end != '\0')
				return "error: MANUAL-V6 takes an address and a prefix length";
			ifs.manual6 = a6;
			ifs.manual6_prefix = (int)plen;
			method = M6_MANUAL;
		} else if (strcasecmp(argv[1], "AUTOMATIC-V6") == 0) {
			method = M6_AUTOMATIC;
		} else if (strcasecmp(argv[1], "LINKLOCAL-V6") == 0) {
			method = M6_LINKLOCAL;
		} else {
			method = M6_NONE;
		}
		ipv6_stop();
		ifs.method6 = method;
		logmsg(LOG_NOTICE, "IPv6: %s", argv[1]);
		ipv6_start();
		return "ok";
	}
	return "error: unknown method";
}

static void
control_input(void)
{
	char line[256];
	const char *reply;
	ssize_t n;
	int c = accept(ifs.ctl, NULL, NULL);

	if (c < 0)
		return;
	/* One short command per connection. */
	struct timeval tv = { 2, 0 };
	setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if ((n = read(c, line, sizeof(line) - 1)) > 0) {
		line[n] = '\0';
		reply = control_command(line);
		(void)write(c, reply, strlen(reply));
		(void)write(c, "\n", 1);
	}
	close(c);
}

/* ---- The event loop ---------------------------------------------------- */

static void
add_fd(struct pollfd *pfd, int *n, int fd)
{
	if (fd >= 0) {
		pfd[*n].fd = fd;
		pfd[*n].events = POLLIN;
		pfd[*n].revents = 0;
		(*n)++;
	}
}

static bool
ready(const struct pollfd *pfd, int n, int fd)
{
	for (int i = 0; i < n; i++)
		if (pfd[i].fd == fd && fd >= 0)
			return (pfd[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
	return false;
}

void
serve(const char *name)
{
	memset(&ifs, 0, sizeof(ifs));
	strlcpy(ifs.name, name, sizeof(ifs.name));
	ifs.bpf = ifs.arp = ifs.ra_sock = ifs.sock6 = ifs.ctl = -1;
	ifs.method4 = M4_DHCP;
	ifs.method6 = M6_AUTOMATIC;
	openlog("dhcpclient", LOG_PID, LOG_DAEMON);
	signal(SIGCHLD, SIG_IGN);	/* the router solicitation children */
	signal(SIGPIPE, SIG_IGN);

	if (get_link(name) < 0) {
		logmsg(LOG_ERR, "not an Ethernet interface");
		exit(1);
	}
	interface_up();
	open_kev();
	ifs.ctl = open_control();
	ipv6_start();
	dhcp4_start();

	bool published = false;		/* configd has our service */
	time_t store_check = 0;

	for (;;) {
		struct pollfd pfd[8];
		int n = 0, timeout = 60000;
		time_t t = now();

		add_fd(pfd, &n, ifs.bpf);
		add_fd(pfd, &n, ifs.arp);
		add_fd(pfd, &n, ifs.ra_sock);
		add_fd(pfd, &n, ifs.sock6);
		add_fd(pfd, &n, ifs.ctl);
		add_fd(pfd, &n, kev);
		if (ifs.next4 != 0)
			timeout = ifs.next4 <= t ? 0 : (int)(ifs.next4 - t) * 1000 < timeout ?
			    (int)(ifs.next4 - t) * 1000 : timeout;
		if (ifs.next6 != 0)
			timeout = ifs.next6 <= t ? 0 : (int)(ifs.next6 - t) * 1000 < timeout ?
			    (int)(ifs.next6 - t) * 1000 : timeout;
		if (!published && timeout > STORE_RETRY * 1000)
			timeout = STORE_RETRY * 1000;

		if (poll(pfd, (nfds_t)n, timeout) > 0) {
			if (ready(pfd, n, ifs.bpf))
				dhcp4_input();
			if (ready(pfd, n, ifs.arp))
				arp_input();
			if (ready(pfd, n, ifs.ra_sock))
				ra_input();
			if (ready(pfd, n, ifs.sock6))
				dhcp6_input();
			if (ready(pfd, n, ifs.ctl))
				control_input();
			if (ready(pfd, n, kev))
				kev_input();
		}
		t = now();
		/*
		 * configd may start (or create our service) after we have
		 * configured: hand it the state, and the default route.
		 */
		if (!published && t >= store_check) {
			store_check = t + STORE_RETRY;
			if (store_active()) {
				published = true;
				if (ifs.route.s_addr != 0) {
					default_route(RTM_DELETE, ifs.route);
					ifs.route.s_addr = 0;
				}
				publish_state();
			}
		}
		if (ifs.next4 != 0 && t >= ifs.next4)
			dhcp4_timeout();
		if (ifs.next6 != 0 && t >= ifs.next6)
			dhcp6_timeout();
	}
}
