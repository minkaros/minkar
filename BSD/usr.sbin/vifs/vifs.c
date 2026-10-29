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
 * vifs: edit /etc/fstab safely. The file is locked so that two vifs do
 * not edit it at once, copied to a temporary file the editor ($EDITOR,
 * or vi) works on, and copied back in one rename only if it changed.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	FSTAB	"/etc/fstab"

static char tmpname[PATH_MAX];

static void
cleanup(void)
{

	if (tmpname[0] != '\0')
		unlink(tmpname);
}

static void
usage(void)
{

	fprintf(stderr, "usage: vifs\n");
	exit(1);
}

/* Copy from fd in to fd out; -1 on error. */
static int
copy(int in, int out)
{
	char buf[8192];
	ssize_t n;

	while ((n = read(in, buf, sizeof(buf))) > 0)
		if (write(out, buf, (size_t)n) != n)
			return (-1);
	return (n < 0 ? -1 : 0);
}

/* Run the editor on path; its exit status, or -1. */
static int
edit(const char *path)
{
	const char *editor;
	char *cmd;
	pid_t pid;
	int status;
	struct sigaction ign, oint, oquit;

	if ((editor = getenv("EDITOR")) == NULL || *editor == '\0')
		editor = "vi";
	if (asprintf(&cmd, "%s %s", editor, path) == -1)
		err(1, "asprintf");
	memset(&ign, 0, sizeof(ign));
	ign.sa_handler = SIG_IGN;
	sigaction(SIGINT, &ign, &oint);
	sigaction(SIGQUIT, &ign, &oquit);
	switch (pid = fork()) {
	case -1:
		err(1, "fork");
	case 0:
		sigaction(SIGINT, &oint, NULL);
		sigaction(SIGQUIT, &oquit, NULL);
		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
	while (waitpid(pid, &status, 0) == -1)
		if (errno != EINTR)
			err(1, "waitpid");
	sigaction(SIGINT, &oint, NULL);
	sigaction(SIGQUIT, &oquit, NULL);
	free(cmd);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

int
main(int argc, char *argv[])
{
	struct stat before, after, st;
	int fd, tfd;

	if (argc != 1)
		usage();
	(void)argv;

	if ((fd = open(FSTAB, O_RDWR | O_CREAT, 0644)) == -1)
		err(1, "%s", FSTAB);
	if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
		if (errno == EWOULDBLOCK)
			errx(1, "%s is being edited by someone else", FSTAB);
		err(1, "%s", FSTAB);
	}
	if (fstat(fd, &st) == -1)
		err(1, "%s", FSTAB);

	strlcpy(tmpname, "/etc/vifs.XXXXXX", sizeof(tmpname));
	if ((tfd = mkstemp(tmpname)) == -1)
		err(1, "mkstemp");
	atexit(cleanup);
	if (fchmod(tfd, st.st_mode & 07777) == -1 ||
	    copy(fd, tfd) == -1 || fstat(tfd, &before) == -1)
		err(1, "%s", tmpname);
	close(tfd);

	if (edit(tmpname) != 0)
		errx(1, "the editor failed; %s is unchanged", FSTAB);
	if (stat(tmpname, &after) == -1)
		err(1, "%s", tmpname);
	if (after.st_mtimespec.tv_sec == before.st_mtimespec.tv_sec &&
	    after.st_mtimespec.tv_nsec == before.st_mtimespec.tv_nsec &&
	    after.st_size == before.st_size) {
		fprintf(stderr, "vifs: %s unchanged\n", FSTAB);
		return (0);
	}
	if (rename(tmpname, FSTAB) == -1)
		err(1, "rename %s to %s", tmpname, FSTAB);
	tmpname[0] = '\0';
	close(fd);
	return (0);
}
