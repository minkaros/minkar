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
 *	ipconfig ifcount
 *
 * DHCP information comes from the lease dhcpclient(8) keeps in
 * /var/run/dhcpclient/<interface>.lease.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ifaddrs.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

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
	    "\tifcount\n");
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
open_lease(const char *name)
{
	char path[64];

	if (strchr(name, '/') != NULL || strlen(name) >= IFNAMSIZ)
		return NULL;
	snprintf(path, sizeof(path), LEASE_DIR "/%s.lease", name);
	return fopen(path, "r");
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
	if (strcmp(argv[1], "set") == 0) {
		fprintf(stderr, "ipconfig: set: not supported yet; dhcpclient configures every Ethernet interface\n");
		return EX_UNAVAILABLE;
	}
	usage();
}
