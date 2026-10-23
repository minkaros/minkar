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
 *	ifconfig [-a | -l | -u | -d] [-m]
 *	ifconfig [-m] interface
 *	ifconfig interface [inet] address[/prefix] [netmask mask]
 *	    [broadcast address] [alias | -alias | delete] [mtu n] [up | down]
 *	ifconfig interface inet6 address[/prefixlen] [prefixlen n]
 *	    [alias | -alias | delete]
 *	ifconfig interface media type [mediaopt opts] [-mediaopt opts]
 *	ifconfig vlanN create [vlan tag vlandev parent]
 *	ifconfig vlanN vlan tag vlandev parent | -vlandev
 *	ifconfig interface destroy
 *
 * The output follows macOS: flags, mtu, ether, inet and inet6 addresses,
 * VLAN, media and link status; -m adds the supported media.
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
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
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

static int sock = -1, sock6 = -1;
static bool show_media_list;

/* SIOCSIFVLAN and SIOCGIFVLAN take this through ifr_data. */
struct vlanreq {
	char		vlr_parent[IFNAMSIZ];
	u_short		vlr_tag;
};

static void __dead2
usage(void)
{
	fprintf(stderr,
	    "usage: ifconfig [-a | -l | -u | -d] [-m]\n"
	    "       ifconfig [-m] interface\n"
	    "       ifconfig interface [inet] [address[/prefix]] [netmask mask]\n"
	    "                [broadcast address] [alias | -alias | delete]\n"
	    "                [mtu n] [up | down]\n"
	    "       ifconfig interface inet6 address[/prefixlen] [prefixlen n]\n"
	    "                [alias | -alias | delete]\n"
	    "       ifconfig interface media type [mediaopt opts] [-mediaopt opts]\n"
	    "       ifconfig interface create [vlan tag vlandev parent]\n"
	    "       ifconfig interface vlan tag vlandev parent | -vlandev\n"
	    "       ifconfig interface destroy\n");
	exit(EX_USAGE);
}

static int
getsock(void)
{
	if (sock < 0 && (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(EX_OSERR, "socket");
	return sock;
}

static int
getsock6(void)
{
	if (sock6 < 0 && (sock6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		err(EX_OSERR, "socket");
	return sock6;
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

/* Ethernet media subtypes and options, by the names macOS uses. */
static const struct {
	int		word;
	const char	*name;
} subtypes[] = {
	{ IFM_AUTO,	"autoselect" },
	{ IFM_MANUAL,	"manual" },
	{ IFM_NONE,	"none" },
	{ IFM_10_T,	"10baseT/UTP" },
	{ IFM_100_TX,	"100baseTX" },
	{ IFM_1000_T,	"1000baseT" },
	{ IFM_1000_SX,	"1000baseSX" },
	{ IFM_10G_SR,	"10GbaseSR" },
}, options[] = {
	{ IFM_FDX,	"full-duplex" },
	{ IFM_HDX,	"half-duplex" },
	{ IFM_FLAG0,	"flag0" },
	{ IFM_FLAG1,	"flag1" },
	{ IFM_FLAG2,	"flag2" },
	{ IFM_LOOP,	"hw-loopback" },
};
#define NELEM(a)	(sizeof(a) / sizeof((a)[0]))

static const char *
media_subtype(int word)
{
	if (IFM_TYPE(word) != IFM_ETHER)
		return "unknown";
	for (size_t i = 0; i < NELEM(subtypes); i++)
		if (IFM_SUBTYPE(word) == subtypes[i].word)
			return subtypes[i].name;
	return "unknown";
}

/* "1000baseT <full-duplex>", or with list, "1000baseT mediaopt full-duplex". */
static void
print_media_word(int word, bool list)
{
	bool first = true;

	printf("%s", media_subtype(word));
	for (size_t i = 0; i < NELEM(options); i++) {
		if (!(word & options[i].word))
			continue;
		if (list)
			printf("%s%s", first ? " mediaopt " : ",", options[i].name);
		else
			printf("%s%s", first ? " <" : ",", options[i].name);
		first = false;
	}
	if (!list && !first)
		printf(">");
}

/* A subtype name; -1 if unknown. */
static int
parse_subtype(const char *s)
{
	for (size_t i = 0; i < NELEM(subtypes); i++)
		if (strcasecmp(s, subtypes[i].name) == 0 ||
		    (subtypes[i].word == IFM_AUTO && strcasecmp(s, "auto") == 0))
			return subtypes[i].word;
	return -1;
}

/* Comma-separated option names. */
static int
parse_options(const char *s)
{
	char buf[128], *p, *opt;
	int word = 0;

	strlcpy(buf, s, sizeof(buf));
	for (p = buf; (opt = strsep(&p, ",")) != NULL;) {
		size_t i;

		for (i = 0; i < NELEM(options); i++)
			if (strcasecmp(opt, options[i].name) == 0)
				break;
		if (i == NELEM(options))
			errx(EX_USAGE, "%s: unknown media option", opt);
		word |= options[i].word;
	}
	return word;
}

static void
print_media(const char *name)
{
	struct ifmediareq ifmr;
	int *list = NULL;

	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));
	if (ioctl(getsock(), SIOCGIFMEDIA, &ifmr) < 0)
		return;
	if (show_media_list && ifmr.ifm_count > 0 &&
	    (list = calloc((size_t)ifmr.ifm_count, sizeof(int))) != NULL) {
		ifmr.ifm_ulist = list;
		if (ioctl(getsock(), SIOCGIFMEDIA, &ifmr) < 0)
			ifmr.ifm_count = 0;
	}
	printf("\tmedia: ");
	print_media_word(ifmr.ifm_current, false);
	if (ifmr.ifm_active != ifmr.ifm_current && ifmr.ifm_active != 0) {
		printf(" (");
		print_media_word(ifmr.ifm_active, false);
		printf(")");
	}
	printf("\n");
	if (ifmr.ifm_status & IFM_AVALID)
		printf("\tstatus: %s\n",
		    (ifmr.ifm_status & IFM_ACTIVE) ? "active" : "inactive");
	if (list != NULL) {
		printf("\tsupported media:\n");
		for (int i = 0; i < ifmr.ifm_count; i++) {
			printf("\t\tmedia ");
			print_media_word(list[i], true);
			printf("\n");
		}
		free(list);
	}
}

static void
print_vlan(const char *name)
{
	struct ifreq ifr;
	struct vlanreq vlr;

	memset(&ifr, 0, sizeof(ifr));
	memset(&vlr, 0, sizeof(vlr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&vlr;
	if (ioctl(getsock(), SIOCGIFVLAN, &ifr) < 0)
		return;
	if (vlr.vlr_parent[0] != '\0')
		printf("\tvlan: %u parent interface: %s\n", vlr.vlr_tag, vlr.vlr_parent);
	else
		printf("\tvlan: <none> parent interface: <none>\n");
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
	print_vlan(name);
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

/* addr[/prefixlen] */
static void
parse_inet6(const char *s, struct sockaddr_in6 *sin6, int *plen)
{
	char buf[INET6_ADDRSTRLEN + 8], *slash, *end;

	memset(sin6, 0, sizeof(*sin6));
	sin6->sin6_len = sizeof(*sin6);
	sin6->sin6_family = AF_INET6;
	strlcpy(buf, s, sizeof(buf));
	if ((slash = strchr(buf, '/')) != NULL) {
		long v = strtol(slash + 1, &end, 10);

		if (*end != '\0' || v < 0 || v > 128)
			errx(EX_USAGE, "%s: bad prefix length", slash + 1);
		*plen = (int)v;
		*slash = '\0';
	}
	if (inet_pton(AF_INET6, buf, &sin6->sin6_addr) != 1)
		errx(EX_USAGE, "%s: bad address", buf);
}

static void
prefix_mask6(int plen, struct sockaddr_in6 *mask)
{
	memset(mask, 0, sizeof(*mask));
	mask->sin6_len = sizeof(*mask);
	mask->sin6_family = AF_INET6;
	for (int i = 0; i < 16; i++, plen -= 8)
		mask->sin6_addr.s6_addr[i] = plen >= 8 ? 0xff :
		    (plen > 0 ? (uint8_t)(0xff << (8 - plen)) : 0);
}

/* A link-local address names its link in the second 16-bit word. */
static void
embed_scope(const char *name, struct sockaddr_in6 *sin6)
{
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
		unsigned int index = if_nametoindex(name);

		sin6->sin6_addr.s6_addr[2] = (uint8_t)(index >> 8);
		sin6->sin6_addr.s6_addr[3] = (uint8_t)index;
	}
}

static void
configure_inet6(const char *name, const struct sockaddr_in6 *addr, int plen, bool remove)
{
	if (remove) {
		struct in6_ifreq ifr6;

		memset(&ifr6, 0, sizeof(ifr6));
		strlcpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
		ifr6.ifr_addr = *addr;
		embed_scope(name, &ifr6.ifr_addr);
		if (ioctl(getsock6(), SIOCDIFADDR_IN6, &ifr6) < 0)
			err(EX_OSERR, "%s: SIOCDIFADDR_IN6", name);
	} else {
		struct in6_aliasreq ifra6;

		memset(&ifra6, 0, sizeof(ifra6));
		strlcpy(ifra6.ifra_name, name, sizeof(ifra6.ifra_name));
		ifra6.ifra_addr = *addr;
		embed_scope(name, &ifra6.ifra_addr);
		prefix_mask6(plen, &ifra6.ifra_prefixmask);
		ifra6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;
		ifra6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
		if (ioctl(getsock6(), SIOCAIFADDR_IN6, &ifra6) < 0)
			err(EX_OSERR, "%s: SIOCAIFADDR_IN6", name);
	}
}

/*
 * media and mediaopt change the current media word: the subtype, then
 * options added and removed.
 */
static void
configure_media(const char *name, int subtype, int addopts, int delopts)
{
	struct ifmediareq ifmr;
	struct ifreq ifr;
	int word;

	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));
	if (ioctl(getsock(), SIOCGIFMEDIA, &ifmr) < 0)
		err(EX_OSERR, "%s: SIOCGIFMEDIA", name);
	word = ifmr.ifm_current;
	if (subtype >= 0) {
		/* A new subtype starts without the old one's options. */
		word = IFM_TYPE(word) | subtype;
		if (word == subtype)
			word |= IFM_ETHER;
	}
	word = (word | addopts) & ~delopts;
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_media = word;
	if (ioctl(getsock(), SIOCSIFMEDIA, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCSIFMEDIA", name);
}

/* tag and parent; an empty parent detaches the VLAN. */
static void
configure_vlan(const char *name, int tag, const char *parent)
{
	struct ifreq ifr;
	struct vlanreq vlr;

	memset(&ifr, 0, sizeof(ifr));
	memset(&vlr, 0, sizeof(vlr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	strlcpy(vlr.vlr_parent, parent, sizeof(vlr.vlr_parent));
	vlr.vlr_tag = (u_short)tag;
	ifr.ifr_data = (caddr_t)&vlr;
	if (ioctl(getsock(), SIOCSIFVLAN, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCSIFVLAN", name);
}

/* Creates a cloned interface (vlan0); a name without a unit gets the next one. */
static void
create_interface(char *name, size_t size)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(getsock(), SIOCIFCREATE, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCIFCREATE", name);
	if (strcmp(name, ifr.ifr_name) != 0) {
		strlcpy(name, ifr.ifr_name, size);
		printf("%s\n", name);
	}
}

static void
destroy_interface(const char *name)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(getsock(), SIOCIFDESTROY, &ifr) < 0)
		err(EX_OSERR, "%s: SIOCIFDESTROY", name);
}

static int
configure(const char *name, int argc, char *argv[])
{
	struct ifaliasreq ifra;
	struct sockaddr_in addr, mask, bcast;
	struct sockaddr_in6 addr6;
	bool have_addr = false, have_mask = false, have_bcast = false;
	bool alias = false, remove = false, inet6 = false;
	bool have_vlan = false, have_vlandev = false, unset_vlandev = false;
	int up = 0, plen = 64, vlan_tag = 0;
	int subtype = -1, addopts = 0, delopts = 0;
	const char *vlandev = "";

	memset(&ifra, 0, sizeof(ifra));
	for (int i = 0; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "inet") == 0) {
			inet6 = false;
		} else if (strcmp(a, "inet6") == 0) {
			inet6 = true;
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
		} else if (strcmp(a, "prefixlen") == 0) {
			char *end;
			long v;

			if (++i == argc)
				usage();
			v = strtol(argv[i], &end, 10);
			if (*end != '\0' || v < 0 || v > 128)
				errx(EX_USAGE, "%s: bad prefix length", argv[i]);
			plen = (int)v;
		} else if (strcmp(a, "broadcast") == 0) {
			if (++i == argc)
				usage();
			parse_inet(argv[i], &bcast);
			have_bcast = true;
		} else if (strcmp(a, "mtu") == 0) {
			if (++i == argc)
				usage();
			set_mtu(name, argv[i]);
		} else if (strcmp(a, "media") == 0) {
			if (++i == argc)
				usage();
			if ((subtype = parse_subtype(argv[i])) < 0)
				errx(EX_USAGE, "%s: unknown media type", argv[i]);
		} else if (strcmp(a, "mediaopt") == 0) {
			if (++i == argc)
				usage();
			addopts |= parse_options(argv[i]);
		} else if (strcmp(a, "-mediaopt") == 0) {
			if (++i == argc)
				usage();
			delopts |= parse_options(argv[i]);
		} else if (strcmp(a, "vlan") == 0) {
			char *end;
			long v;

			if (++i == argc)
				usage();
			v = strtol(argv[i], &end, 10);
			if (*end != '\0' || v < 1 || v > 4094)
				errx(EX_USAGE, "%s: bad VLAN tag", argv[i]);
			vlan_tag = (int)v;
			have_vlan = true;
		} else if (strcmp(a, "vlandev") == 0) {
			if (++i == argc)
				usage();
			vlandev = argv[i];
			have_vlandev = true;
		} else if (strcmp(a, "-vlandev") == 0) {
			unset_vlandev = true;
		} else if (strcmp(a, "destroy") == 0) {
			destroy_interface(name);
			return 0;
		} else if (!have_addr && inet6) {
			parse_inet6(a, &addr6, &plen);
			have_addr = true;
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

	if (unset_vlandev) {
		configure_vlan(name, 0, "");
	} else if (have_vlan || have_vlandev) {
		if (!have_vlan || !have_vlandev)
			errx(EX_USAGE, "vlan and vlandev go together");
		configure_vlan(name, vlan_tag, vlandev);
	}
	if (subtype >= 0 || addopts != 0 || delopts != 0)
		configure_media(name, subtype, addopts, delopts);

	if (have_addr && inet6) {
		configure_inet6(name, &addr6, plen, remove);
		if (!remove && up == 0)
			up = 1;
	} else if (have_addr && remove) {
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

	while ((ch = getopt(argc, argv, "adlmu")) != -1) {
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
		case 'm':
			show_media_list = true;
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
	if (argc > 1 && strcmp(argv[1], "create") == 0) {
		char name[IFNAMSIZ];

		strlcpy(name, argv[0], sizeof(name));
		create_interface(name, sizeof(name));
		if (argc == 2)
			return 0;
		return configure(name, argc - 2, argv + 2);
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
