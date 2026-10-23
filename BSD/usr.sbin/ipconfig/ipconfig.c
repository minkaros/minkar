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
 * ipconfig: ask about the IP configuration, as on macOS.
 *
 *	ipconfig getifaddr interface
 *	ipconfig getoption interface option
 *	ipconfig getpacket interface
 *	ipconfig getv6packet interface
 *	ipconfig getsummary interface
 *	ipconfig ifcount
 *	ipconfig set interface DHCP | BOOTP | NONE | MANUAL addr mask |
 *	    INFORM addr mask | AUTOMATIC-V6 | LINKLOCAL-V6 | NONE-V6 |
 *	    MANUAL-V6 addr prefixlen
 *
 * DHCP information comes from what dhcpclient(8) keeps in
 * /var/run/dhcpclient/<interface>.lease and .dhcp6; "set" goes to its
 * control socket, /var/run/dhcpclient/<interface>.ctl (root only). As on
 * macOS, "set" lasts until dhcpclient restarts.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ifaddrs.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define LEASE_DIR	"/var/run/dhcpclient"

static void __dead2
usage(void)
{
	fprintf(stderr,
	    "usage: ipconfig <command> <args>\n"
	    "where <command> is one of:\n"
	    "\tgetifaddr <interface>\n"
	    "\tgetoption <interface> <option>\n"
	    "\tgetpacket <interface>\n"
	    "\tgetv6packet <interface>\n"
	    "\tgetsummary <interface>\n"
	    "\tifcount\n"
	    "\tset <interface> <method> [args]\n"
	    "where <method> is DHCP, BOOTP, NONE, MANUAL addr mask, INFORM addr mask,\n"
	    "AUTOMATIC-V6, LINKLOCAL-V6, NONE-V6 or MANUAL-V6 addr prefixlen\n");
	exit(EX_USAGE);
}

/* The interface's first IPv4 address. */
static int
getifaddr(const char *name)
{
	struct ifaddrs *list, *ifa;
	char buf[INET_ADDRSTRLEN];
	int ret = 1;

	if (getifaddrs(&list) < 0)
		return 1;
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET &&
		    strcmp(ifa->ifa_name, name) == 0) {
			const struct sockaddr_in *sin =
			    (const struct sockaddr_in *)(const void *)ifa->ifa_addr;

			printf("%s\n", inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)));
			ret = 0;
			break;
		}
	}
	freeifaddrs(list);
	return ret;
}

/* Interfaces, other than loopback, that have an IPv4 address. */
static int
ifcount(void)
{
	struct ifaddrs *list, *ifa, *prev;
	int n = 0;

	if (getifaddrs(&list) < 0)
		return 1;
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		bool seen = false;

		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET ||
		    (ifa->ifa_flags & IFF_LOOPBACK))
			continue;
		for (prev = list; prev != ifa; prev = prev->ifa_next)
			if (prev->ifa_addr != NULL && prev->ifa_addr->sa_family == AF_INET &&
			    strcmp(prev->ifa_name, ifa->ifa_name) == 0)
				seen = true;
		if (!seen)
			n++;
	}
	freeifaddrs(list);
	printf("%d\n", n);
	return 0;
}

static FILE *
open_file(const char *name, const char *suffix)
{
	char path[64];

	if (strchr(name, '/') != NULL || strlen(name) >= IFNAMSIZ)
		return NULL;
	snprintf(path, sizeof(path), LEASE_DIR "/%s.%s", name, suffix);
	return fopen(path, "r");
}

static FILE *
open_lease(const char *name)
{
	return open_file(name, "lease");
}

/* "key=value" lines as "key = value". */
static int
print_file(FILE *f, const char *indent)
{
	char line[512];

	if (f == NULL)
		return 1;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *eq = strchr(line, '=');

		if (eq == NULL)
			continue;
		*eq = '\0';
		eq[1 + strcspn(eq + 1, "\n")] = '\0';
		printf("%s%s = %s\n", indent, line, eq + 1);
	}
	fclose(f);
	return 0;
}

/* Sends "set <method> [args]" to the interface's dhcpclient. */
static int
set_method(const char *name, int argc, char *argv[])
{
	struct sockaddr_un sun;
	char cmd[256], reply[256];
	size_t n = 0;
	ssize_t r;
	int s;

	if (strchr(name, '/') != NULL || strlen(name) >= IFNAMSIZ)
		usage();
	n = (size_t)snprintf(cmd, sizeof(cmd), "set");
	for (int i = 0; i < argc && n < sizeof(cmd); i++)
		n += (size_t)snprintf(cmd + n, sizeof(cmd) - n, " %s", argv[i]);
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	snprintf(sun.sun_path, sizeof(sun.sun_path), LEASE_DIR "/%s.ctl", name);
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 ||
	    connect(s, (struct sockaddr *)&sun, SUN_LEN(&sun)) < 0) {
		fprintf(stderr, "ipconfig: %s: dhcpclient is not serving it (%s)\n", name,
		    strerror(errno));
		return EX_UNAVAILABLE;
	}
	if (write(s, cmd, strlen(cmd)) < 0 || (r = read(s, reply, sizeof(reply) - 1)) <= 0) {
		fprintf(stderr, "ipconfig: %s: no answer from dhcpclient\n", name);
		close(s);
		return EX_IOERR;
	}
	close(s);
	reply[r] = '\0';
	reply[strcspn(reply, "\n")] = '\0';
	if (strcmp(reply, "ok") != 0) {
		fprintf(stderr, "ipconfig: %s: %s\n", name, reply);
		return EX_USAGE;
	}
	return 0;
}

/*
 * One option of the lease, by its macOS name (router, subnet_mask,
 * domain_name_server, ...). A list prints its first element.
 */
static int
getoption(const char *name, const char *option)
{
	char line[512];
	size_t olen = strlen(option);
	FILE *f;
	int ret = 1;

	if ((f = open_lease(name)) == NULL)
		return 1;
	while (fgets(line, sizeof(line), f) != NULL) {
		if (strncmp(line, option, olen) == 0 && line[olen] == '=') {
			char *v = line + olen + 1;

			v[strcspn(v, " \n")] = '\0';
			printf("%s\n", v);
			ret = 0;
			break;
		}
	}
	fclose(f);
	return ret;
}

static int
getpacket(const char *name)
{
	char line[512];
	FILE *f;

	if ((f = open_lease(name)) == NULL)
		return 1;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *eq = strchr(line, '=');

		if (eq == NULL)
			continue;
		*eq = '\0';
		line[strcspn(line, "\n")] = '\0';
		eq[1 + strcspn(eq + 1, "\n")] = '\0';
		printf("%s = %s\n", line, eq + 1);
	}
	fclose(f);
	return 0;
}

int
main(int argc, char *argv[])
{
	if (argc < 2)
		usage();
	if (strcmp(argv[1], "getifaddr") == 0 && argc == 3)
		return getifaddr(argv[2]);
	if (strcmp(argv[1], "ifcount") == 0 && argc == 2)
		return ifcount();
	if (strcmp(argv[1], "getoption") == 0 && argc == 4)
		return getoption(argv[2], argv[3]);
	if (strcmp(argv[1], "getpacket") == 0 && argc == 3)
		return getpacket(argv[2]);
	if (strcmp(argv[1], "getv6packet") == 0 && argc == 3)
		return print_file(open_file(argv[2], "dhcp6"), "");
	if (strcmp(argv[1], "getsummary") == 0 && argc == 3) {
		printf("<dictionary> {\n  InterfaceName = %s\n", argv[2]);
		printf("  IPv4 : <dictionary> {\n");
		print_file(open_lease(argv[2]), "    ");
		printf("  }\n  DHCPv6 : <dictionary> {\n");
		print_file(open_file(argv[2], "dhcp6"), "    ");
		printf("  }\n}\n");
		return 0;
	}
	if (strcmp(argv[1], "set") == 0 && argc >= 4)
		return set_method(argv[2], argc - 3, argv + 3);
	usage();
}
