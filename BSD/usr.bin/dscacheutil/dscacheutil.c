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
 * dscacheutil: query the system's directory services, as macOS's does,
 * through the same calls programs use (getaddrinfo, getpwnam, getgrnam).
 *
 *	dscacheutil -q host -a name <name>
 *	dscacheutil -q user [-a name <name>]
 *	dscacheutil -q group [-a name <name>]
 *
 * Results print as "key: value" lines, one blank line after each entry.
 * Exits 1 when nothing is found. (The cache options of macOS's tool,
 * -cachedump, -flushcache and -statistics, are not provided.)
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static void __dead2
usage(void)
{
	fprintf(stderr,
	    "usage: dscacheutil -q host -a name <name>\n"
	    "       dscacheutil -q user [-a name <name>]\n"
	    "       dscacheutil -q group [-a name <name>]\n");
	exit(EX_USAGE);
}

/* Every IPv4 and IPv6 address of a name, as macOS prints them. */
static int
query_host(const char *name)
{
	struct addrinfo hints, *res, *ai;
	char buf[INET6_ADDRSTRLEN];
	int err, v4 = 0, v6 = 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((err = getaddrinfo(name, NULL, &hints, &res)) != 0) {
		fprintf(stderr, "dscacheutil: %s: %s\n", name, gai_strerror(err));
		return 1;
	}
	for (int pass = 0; pass < 2; pass++) {
		int family = pass == 0 ? AF_INET6 : AF_INET;
		int found = 0;

		for (ai = res; ai != NULL; ai = ai->ai_next) {
			const void *addr;

			if (ai->ai_family != family)
				continue;
			if (!found++)
				printf("name: %s\n", name);
			if (family == AF_INET)
				addr = &((const struct sockaddr_in *)(const void *)ai->ai_addr)->sin_addr;
			else
				addr = &((const struct sockaddr_in6 *)(const void *)ai->ai_addr)->sin6_addr;
			printf("%s: %s\n", family == AF_INET ? "ip_address" : "ipv6_address",
			    inet_ntop(family, addr, buf, sizeof(buf)));
		}
		if (found) {
			printf("\n");
			if (family == AF_INET)
				v4 = 1;
			else
				v6 = 1;
		}
	}
	freeaddrinfo(res);
	return (v4 || v6) ? 0 : 1;
}

static void
print_user(const struct passwd *pw)
{
	printf("name: %s\npassword: %s\nuid: %d\ngid: %d\ndir: %s\nshell: %s\ngecos: %s\n\n",
	    pw->pw_name, pw->pw_passwd, (int)pw->pw_uid, (int)pw->pw_gid,
	    pw->pw_dir, pw->pw_shell, pw->pw_gecos);
}

static int
query_user(const char *name)
{
	struct passwd *pw;

	if (name != NULL) {
		if ((pw = getpwnam(name)) == NULL)
			return 1;
		print_user(pw);
		return 0;
	}
	setpwent();
	while ((pw = getpwent()) != NULL)
		print_user(pw);
	endpwent();
	return 0;
}

static void
print_group(const struct group *gr)
{
	printf("name: %s\npassword: %s\ngid: %d\n", gr->gr_name, gr->gr_passwd,
	    (int)gr->gr_gid);
	if (gr->gr_mem != NULL && gr->gr_mem[0] != NULL) {
		printf("users:");
		for (char **m = gr->gr_mem; *m != NULL; m++)
			printf(" %s", *m);
		printf("\n");
	}
	printf("\n");
}

static int
query_group(const char *name)
{
	struct group *gr;

	if (name != NULL) {
		if ((gr = getgrnam(name)) == NULL)
			return 1;
		print_group(gr);
		return 0;
	}
	setgrent();
	while ((gr = getgrent()) != NULL)
		print_group(gr);
	endgrent();
	return 0;
}

int
main(int argc, char *argv[])
{
	const char *category = NULL, *key = NULL, *value = NULL;
	int ch;

	while ((ch = getopt(argc, argv, "q:a:")) != -1) {
		switch (ch) {
		case 'q':
			category = optarg;
			break;
		case 'a':
			key = optarg;
			if (optind >= argc)
				usage();
			value = argv[optind++];
			break;
		default:
			usage();
		}
	}
	if (category == NULL || optind != argc || (key != NULL && strcmp(key, "name") != 0))
		usage();
	if (strcmp(category, "host") == 0) {
		if (value == NULL)
			usage();
		return query_host(value);
	}
	if (strcmp(category, "user") == 0)
		return query_user(value);
	if (strcmp(category, "group") == 0)
		return query_group(value);
	usage();
}
