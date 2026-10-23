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
 * ifconfig: show and configure network interfaces.
 *
 *	ifconfig [-a | -l | -u | -d]
 *	ifconfig interface [up | down]
 *	ifconfig interface [inet] address[/prefix] [netmask mask]
 *	    [broadcast address] [alias | -alias | delete] [mtu n] [up | down]
 *
 * The output follows macOS: flags, mtu, ether, inet and inet6 addresses,
 * media and link status.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static int sock = -1;

static void __dead2
usage(void)
{
	fprintf(stderr,
	    "usage: ifconfig [-a | -l | -u | -d]\n"
	    "       ifconfig interface [inet] [address[/prefix]] [netmask mask]\n"
	    "                [broadcast address] [alias | -alias | delete]\n"
	    "                [mtu n] [up | down]\n");
	exit(EX_USAGE);
}

static int
getsock(void)
{
	if (sock < 0 && (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(EX_OSERR, "socket");
	return sock;
}

/* ---- Display ----------------------------------------------------------- */

static const char *flagnames[] = {
	"UP", "BROADCAST", "DEBUG", "LOOPBACK", "POINTOPOINT", "SMART",
	"RUNNING", "NOARP", "PROMISC", "ALLMULTI", "OACTIVE", "SIMPLEX",
	"LINK0", "LINK1", "LINK2", "MULTICAST",
};

static void
print_flags(unsigned int flags)
{
	bool first = true;

	printf("flags=%x<", flags);
	for (unsigned int i = 0; i < sizeof(flagnames) / sizeof(flagnames[0]); i++) {
		if (flags & (1U << i)) {
			printf("%s%s", first ? "" : ",", flagnames[i]);
			first = false;
		}
	}
	printf(">");
}

static int
prefixlen(const unsigned char *mask, size_t len)
{
	int bits = 0;

	for (size_t i = 0; i < len; i++) {
		unsigned char b = mask[i];

		while (b & 0x80) {
			bits++;
			b <<= 1;
		}
		if (mask[i] != 0xff)
			break;
	}
	return bits;
}

static const char *
media_subtype(int word)
{
	if (IFM_TYPE(word) != IFM_ETHER)
		return "unknown";
	switch (IFM_SUBTYPE(word)) {
	case IFM_AUTO:		return "autoselect";
	case IFM_MANUAL:	return "manual";
	case IFM_NONE:		return "none";
	case IFM_10_T:		return "10baseT/UTP";
	case IFM_100_TX:	return "100baseTX";
	case IFM_1000_T:	return "1000baseT";
	case IFM_1000_SX:	return "1000baseSX";
	case IFM_10G_SR:	return "10GbaseSR";
	default:		return "unknown";
	}
}

static void
print_media_word(int word)
{
	printf("%s", media_subtype(word));
	if (word & IFM_FDX)
		printf(" <full-duplex>");
	else if (word & IFM_HDX)
		printf(" <half-duplex>");
}

static void
print_media(const char *name)
{
	struct ifmediareq ifmr;

	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));
	if (ioctl(getsock(), SIOCGIFMEDIA, &ifmr) < 0)
		return;
	printf("\tmedia: ");
	print_media_word(ifmr.ifm_current);
	if (ifmr.ifm_active != ifmr.ifm_current && ifmr.ifm_active != 0) {
		printf(" (");
		print_media_word(ifmr.ifm_active);
		printf(")");
	}
	printf("\n");
	if (ifmr.ifm_status & IFM_AVALID)
		printf("\tstatus: %s\n",
		    (ifmr.ifm_status & IFM_ACTIVE) ? "active" : "inactive");
}

static void
print_addr(const struct ifaddrs *ifa)
{
	char buf[INET6_ADDRSTRLEN];

	switch (ifa->ifa_addr->sa_family) {
	case AF_INET: {
		const struct sockaddr_in *sin = (const struct sockaddr_in *)(const void *)ifa->ifa_addr;
		const struct sockaddr_in *mask = (const struct sockaddr_in *)(const void *)ifa->ifa_netmask;

		printf("\tinet %s", inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)));
		if (ifa->ifa_flags & IFF_POINTOPOINT && ifa->ifa_dstaddr != NULL)
			printf(" --> %s", inet_ntop(AF_INET,
			    &((const struct sockaddr_in *)(const void *)ifa->ifa_dstaddr)->sin_addr,
			    buf, sizeof(buf)));
		if (mask != NULL)
			printf(" netmask 0x%08x", ntohl(mask->sin_addr.s_addr));
		if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr != NULL)
			printf(" broadcast %s", inet_ntop(AF_INET,
			    &((const struct sockaddr_in *)(const void *)ifa->ifa_broadaddr)->sin_addr,
			    buf, sizeof(buf)));
		printf("\n");
		break;
	}
	case AF_INET6: {
		const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *)(const void *)ifa->ifa_addr;
		const struct sockaddr_in6 *mask = (const struct sockaddr_in6 *)(const void *)ifa->ifa_netmask;
		struct in6_addr addr = sin6->sin6_addr;

		/* The kernel keeps the scope in the second 16-bit word. */
		if (IN6_IS_ADDR_LINKLOCAL(&addr))
			addr.s6_addr[2] = addr.s6_addr[3] = 0;
		printf("\tinet6 %s", inet_ntop(AF_INET6, &addr, buf, sizeof(buf)));
		if (IN6_IS_ADDR_LINKLOCAL(&addr))
			printf("%%%s", ifa->ifa_name);
		if (mask != NULL)
			printf(" prefixlen %d", prefixlen(mask->sin6_addr.s6_addr, 16));
		if (sin6->sin6_scope_id != 0)
			printf(" scopeid 0x%x", sin6->sin6_scope_id);
		printf("\n");
		break;
	}
	case AF_LINK: {
		const struct sockaddr_dl *sdl = (const struct sockaddr_dl *)(const void *)ifa->ifa_addr;
		const unsigned char *ll = (const unsigned char *)sdl->sdl_data + sdl->sdl_nlen;

		if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == 6)
			printf("\tether %02x:%02x:%02x:%02x:%02x:%02x\n",
			    ll[0], ll[1], ll[2], ll[3], ll[4], ll[5]);
		break;
	}
	}
}

/* Prints one interface: its flags line, then its addresses and media. */
static void
print_interface(struct ifaddrs *list, const char *name)
{
	struct ifaddrs *ifa;
	struct ifreq ifr;
	unsigned int flags = 0;
	bool found = false;

	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, name) == 0) {
			flags = ifa->ifa_flags;
			found = true;
			break;
		}
	}
	if (!found)
		return;
	printf("%s: ", name);
	print_flags(flags);
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(getsock(), SIOCGIFMTU, &ifr) == 0)
		printf(" mtu %d", ifr.ifr_mtu);
	printf("\n");

	/* The link address first, then inet and inet6, as macOS does. */
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next)
		if (strcmp(ifa->ifa_name, name) == 0 && ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_LINK)
			print_addr(ifa);
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next)
		if (strcmp(ifa->ifa_name, name) == 0 && ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_INET)
			print_addr(ifa);
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next)
		if (strcmp(ifa->ifa_name, name) == 0 && ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == AF_INET6)
			print_addr(ifa);
	print_media(name);
}

/* Each interface once, in the order getifaddrs() returns them. */
static int
list_interfaces(bool names_only, int only_up)
{
	struct ifaddrs *list, *ifa, *prev;
	bool first = true;

	if (getifaddrs(&list) < 0)
		err(EX_OSERR, "getifaddrs");
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		bool seen = false;

		for (prev = list; prev != ifa; prev = prev->ifa_next)
			if (strcmp(prev->ifa_name, ifa->ifa_name) == 0) {
				seen = true;
				break;
			}
		if (seen)
			continue;
		if (only_up > 0 && !(ifa->ifa_flags & IFF_UP))
			continue;
		if (only_up < 0 && (ifa->ifa_flags & IFF_UP))
			continue;
		if (names_only) {
			printf("%s%s", first ? "" : " ", ifa->ifa_name);
			first = false;
		} else {
			print_interface(list, ifa->ifa_name);
		}
	}
	if (names_only)
		printf("\n");
	freeifaddrs(list);
	return 0;
}

/* ---- Configuration ----------------------------------------------------- */

static void
set_flags(const char *name, int set, int clear)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(getsock(), SIOCGIFFLAGS, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCGIFFLAGS", name);
	ifr.ifr_flags = (short)((ifr.ifr_flags | set) & ~clear);
	if (ioctl(getsock(), SIOCSIFFLAGS, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCSIFFLAGS", name);
}

static void
set_mtu(const char *name, const char *value)
{
	struct ifreq ifr;
	char *end;
	long mtu = strtol(value, &end, 10);

	if (*end != '\0' || mtu <= 0)
		errx(EX_USAGE, "%s: bad mtu", value);
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = (int)mtu;
	if (ioctl(getsock(), SIOCSIFMTU, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCSIFMTU", name);
}

static void
parse_inet(const char *s, struct sockaddr_in *sin)
{
	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	if (inet_pton(AF_INET, s, &sin->sin_addr) != 1)
		errx(EX_USAGE, "%s: bad address", s);
}

/* A netmask as a dotted quad, 0x hex word or prefix length. */
static void
parse_mask(const char *s, struct sockaddr_in *sin)
{
	char *end;
	unsigned long v;

	memset(sin, 0, sizeof(*sin));
	sin->sin_len = sizeof(*sin);
	sin->sin_family = AF_INET;
	if (strncmp(s, "0x", 2) == 0) {
		v = strtoul(s, &end, 16);
		if (*end != '\0' || v > 0xffffffffUL)
			errx(EX_USAGE, "%s: bad netmask", s);
		sin->sin_addr.s_addr = htonl((uint32_t)v);
	} else if (strchr(s, '.') == NULL) {
		v = strtoul(s, &end, 10);
		if (*end != '\0' || v > 32)
			errx(EX_USAGE, "%s: bad prefix length", s);
		sin->sin_addr.s_addr = v == 0 ? 0 : htonl(0xffffffffU << (32 - v));
	} else if (inet_pton(AF_INET, s, &sin->sin_addr) != 1) {
		errx(EX_USAGE, "%s: bad netmask", s);
	}
}

/* Removes the interface's first IPv4 address; ENXIO/EADDRNOTAVAIL if none. */
static void
delete_first_inet(const char *name)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(getsock(), SIOCGIFADDR, &ifr) < 0)
		return;
	if (ioctl(getsock(), SIOCDIFADDR, &ifr) < 0 && errno != EADDRNOTAVAIL)
		warn("%s: SIOCDIFADDR", name);
}

static int
configure(const char *name, int argc, char *argv[])
{
	struct ifaliasreq ifra;
	struct sockaddr_in addr, mask, bcast;
	bool have_addr = false, have_mask = false, have_bcast = false;
	bool alias = false, remove = false;
	int up = 0;

	memset(&ifra, 0, sizeof(ifra));
	for (int i = 0; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "inet") == 0) {
			continue;
		} else if (strcmp(a, "inet6") == 0) {
			errx(EX_UNAVAILABLE, "inet6: not supported yet");
		} else if (strcmp(a, "up") == 0) {
			up = 1;
		} else if (strcmp(a, "down") == 0) {
			up = -1;
		} else if (strcmp(a, "alias") == 0 || strcmp(a, "add") == 0) {
			alias = true;
		} else if (strcmp(a, "-alias") == 0 || strcmp(a, "delete") == 0 ||
		    strcmp(a, "remove") == 0) {
			remove = true;
		} else if (strcmp(a, "netmask") == 0) {
			if (++i == argc)
				usage();
			parse_mask(argv[i], &mask);
			have_mask = true;
		} else if (strcmp(a, "broadcast") == 0) {
			if (++i == argc)
				usage();
			parse_inet(argv[i], &bcast);
			have_bcast = true;
		} else if (strcmp(a, "mtu") == 0) {
			if (++i == argc)
				usage();
			set_mtu(name, argv[i]);
		} else if (!have_addr) {
			char buf[64], *slash;

			strlcpy(buf, a, sizeof(buf));
			if ((slash = strchr(buf, '/')) != NULL) {
				*slash = '\0';
				parse_mask(slash + 1, &mask);
				have_mask = true;
			}
			parse_inet(buf, &addr);
			have_addr = true;
		} else {
			errx(EX_USAGE, "%s: unknown parameter", a);
		}
	}

	if (have_addr && remove) {
		struct ifreq ifr;

		memset(&ifr, 0, sizeof(ifr));
		strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
		memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
		if (ioctl(getsock(), SIOCDIFADDR, &ifr) < 0)
			err(EX_OSERR, "%s: SIOCDIFADDR", name);
	} else if (have_addr) {
		if (!alias)
			delete_first_inet(name);
		strlcpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
		memcpy(&ifra.ifra_addr, &addr, sizeof(addr));
		if (have_mask)
			memcpy(&ifra.ifra_mask, &mask, sizeof(mask));
		if (have_bcast) {
			memcpy(&ifra.ifra_broadaddr, &bcast, sizeof(bcast));
		} else if (have_mask) {
			/* The broadcast address of the subnet. */
			bcast = addr;
			bcast.sin_addr.s_addr |= ~mask.sin_addr.s_addr;
			memcpy(&ifra.ifra_broadaddr, &bcast, sizeof(bcast));
		}
		if (ioctl(getsock(), SIOCAIFADDR, &ifra) < 0)
			err(EX_OSERR, "%s: SIOCAIFADDR", name);
		if (up == 0)
			up = 1;		/* an address brings the interface up */
	}
	if (up > 0)
		set_flags(name, IFF_UP, 0);
	else if (up < 0)
		set_flags(name, 0, IFF_UP);
	return 0;
}

int
main(int argc, char *argv[])
{
	struct ifaddrs *list;
	int ch, only_up = 0;
	bool all = false, names = false;

	while ((ch = getopt(argc, argv, "adlu")) != -1) {
		switch (ch) {
		case 'a':
			all = true;
			break;
		case 'd':
			only_up = -1;
			break;
		case 'l':
			names = true;
			break;
		case 'u':
			only_up = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0 || all || names) {
		if (argc != 0)
			usage();
		return list_interfaces(names, only_up);
	}
	if (if_nametoindex(argv[0]) == 0)
		errx(EX_UNAVAILABLE, "interface %s does not exist", argv[0]);
	if (argc == 1) {
		if (getifaddrs(&list) < 0)
			err(EX_OSERR, "getifaddrs");
		print_interface(list, argv[0]);
		freeifaddrs(list);
		return 0;
	}
	return configure(argv[0], argc - 1, argv + 1);
}
