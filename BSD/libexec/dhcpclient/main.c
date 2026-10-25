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
 * dhcpclient: configures the IP of the Ethernet interfaces, started by
 * launchd (org.minkar.dhcpclient).
 *
 *	dhcpclient [-d] [interface ...]
 *
 * Without operands it serves every Ethernet interface: a worker process
 * each (iface.c), started when the interface appears (the kernel's
 * KEV_DL_IF_ATTACHED event, or the periodic scan) and again if it exits
 * while the interface is still there. -d stays in the foreground and logs
 * to stderr as well. ipconfig(8) talks to the workers.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/kern_event.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/net_kev.h>

#include <errno.h>
#include <ifaddrs.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpclient.h"

bool debug;

#define MAX_IFS		16
#define RESTART_SECS	5	/* before restarting a worker that exited */

static struct {
	char	name[IFNAMSIZ];
	pid_t	pid;		/* 0: not running */
	time_t	restart;	/* when it may start again */
} workers[MAX_IFS];
static int nworkers;
static char *only[MAX_IFS];	/* the operands, if any */
static int nonly;

static bool
wanted(const char *name)
{
	if (nonly == 0)
		return true;
	for (int i = 0; i < nonly; i++)
		if (strcmp(only[i], name) == 0)
			return true;
	return false;
}

/* Starts a worker for every Ethernet interface that has none. */
static void
scan(void)
{
	struct ifaddrs *list, *ifa;
	time_t t = now();

	if (getifaddrs(&list) < 0)
		return;
	for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
		const struct sockaddr_dl *sdl;
		int i;

		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		sdl = (const struct sockaddr_dl *)(const void *)ifa->ifa_addr;
		if (sdl->sdl_type != IFT_ETHER || sdl->sdl_alen != ETHER_ADDR_LEN ||
		    !wanted(ifa->ifa_name))
			continue;
		for (i = 0; i < nworkers; i++)
			if (strcmp(workers[i].name, ifa->ifa_name) == 0)
				break;
		if (i == nworkers) {
			if (nworkers == MAX_IFS)
				continue;
			strlcpy(workers[nworkers].name, ifa->ifa_name, IFNAMSIZ);
			workers[nworkers].pid = 0;
			workers[nworkers].restart = 0;
			nworkers++;
		}
		if (workers[i].pid != 0 || t < workers[i].restart)
			continue;
		switch (workers[i].pid = fork()) {
		case -1:
			workers[i].pid = 0;
			break;
		case 0:
			freeifaddrs(list);
			serve(workers[i].name);
		default:
			fprintf(stderr, "dhcpclient: serving %s\n", workers[i].name);
		}
	}
	freeifaddrs(list);
}

static void
reap(void)
{
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		for (int i = 0; i < nworkers; i++) {
			if (workers[i].pid == pid) {
				workers[i].pid = 0;
				workers[i].restart = now() + RESTART_SECS;
			}
		}
	}
}

static int sigpipe[2];

static void
onchild(int sig)
{
	(void)sig;
	(void)write(sigpipe[1], "c", 1);
}

static void
onterm(int sig)
{
	(void)sig;
	for (int i = 0; i < nworkers; i++)
		if (workers[i].pid != 0)
			kill(workers[i].pid, SIGTERM);
	_exit(0);
}

int
main(int argc, char *argv[])
{
	struct kev_request req = { KEV_VENDOR_APPLE, KEV_NETWORK_CLASS, KEV_DL_SUBCLASS };
	int ch, kev;

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
	for (int i = 0; i < argc && i < MAX_IFS; i++)
		only[nonly++] = argv[i];

	if (pipe(sigpipe) < 0)
		return 1;
	signal(SIGCHLD, onchild);
	signal(SIGTERM, onterm);

	/* Interfaces attaching. */
	kev = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
	if (kev >= 0 && ioctl(kev, SIOCSKEVFILT, &req) < 0) {
		close(kev);
		kev = -1;
	}

	for (;;) {
		struct pollfd pfd[2] = { { sigpipe[0], POLLIN, 0 }, { kev, POLLIN, 0 } };
		char buf[1024];

		reap();
		scan();
		if (poll(pfd, kev >= 0 ? 2 : 1, RESTART_SECS * 1000) <= 0)
			continue;
		if (pfd[0].revents & POLLIN)
			(void)read(sigpipe[0], buf, sizeof(buf));
		if (kev >= 0 && (pfd[1].revents & POLLIN))
			(void)recv(kev, buf, sizeof(buf), 0);	/* any of them: scan */
	}
}
