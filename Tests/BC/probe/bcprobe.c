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
 * bcprobe.dylib: gate BC's probe (Docs/ROADMAP.md, lane BC). Inserted
 * into a published app with DYLD_INSERT_LIBRARIES, it appends to the file
 * $BCPROBE_OUT:
 *
 *   init        when its initializer runs: dyld loaded the app and every
 *               library it links, and bound their symbols;
 *   main-queue  when the main queue first runs a block: the app's main
 *               function is running its run loop.
 *
 * bc-run.sh reads them to tell where an app that does not show a window
 * stopped. The variable is removed so that the app's own children are
 * not probed.
 */

#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char out[1024];

static void
mark(const char *what)
{
	int fd = open(out, O_WRONLY | O_CREAT | O_APPEND, 0644);

	if (fd < 0)
		return;
	write(fd, what, strlen(what));
	write(fd, "\n", 1);
	close(fd);
}

static void
main_queue(void *context)
{
	(void)context;
	mark("main-queue");
}

__attribute__((constructor)) static void
bcprobe_init(void)
{
	const char *path = getenv("BCPROBE_OUT");

	if (path == NULL || strlen(path) >= sizeof(out))
		return;
	strcpy(out, path);
	unsetenv("BCPROBE_OUT");
	unsetenv("DYLD_INSERT_LIBRARIES");
	mark("init");
	dispatch_async_f(dispatch_get_main_queue(), NULL, main_queue);
}
