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
 * The few libgeom(3) calls FreeBSD's fdisk makes, over Darwin's disk
 * devices: a name without a path is under /dev; the sector size and the
 * media size come from the DKIOC ioctls. Darwin has no GEOM, so no GEOM
 * class is ever there (fdisk then writes the MBR itself).
 */

#ifndef _MINKAR_LIBGEOM_H_
#define	_MINKAR_LIBGEOM_H_

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline char *
g_device_path(const char *dev)
{
	char *path;

	if (dev[0] == '/')
		return (strdup(dev));
	if (asprintf(&path, "%s%s", _PATH_DEV, dev) == -1)
		return (NULL);
	return (path);
}

static inline int
g_open(const char *name, int dowrite)
{
	char *path = g_device_path(name);
	int fd;

	if (path == NULL)
		return (-1);
	fd = open(path, dowrite ? O_RDWR : O_RDONLY);
	free(path);
	return (fd);
}

static inline int
g_close(int fd)
{

	return (close(fd));
}

static inline ssize_t
g_sectorsize(int fd)
{
	uint32_t size;

	if (ioctl(fd, DKIOCGETBLOCKSIZE, &size) == -1)
		return (-1);
	return ((ssize_t)size);
}

static inline off_t
g_mediasize(int fd)
{
	uint64_t count;
	uint32_t size;

	if (ioctl(fd, DKIOCGETBLOCKCOUNT, &count) == -1 ||
	    ioctl(fd, DKIOCGETBLOCKSIZE, &size) == -1)
		return (-1);
	return ((off_t)(count * size));
}

/* No GEOM on Darwin: an empty mesh, no classes. */
struct gclass {
	LIST_ENTRY(gclass) lg_class;
	char	*lg_name;
};
struct gmesh {
	LIST_HEAD(, gclass) lg_class;
};

static inline int
geom_gettree(struct gmesh *mesh)
{

	LIST_INIT(&mesh->lg_class);
	return (0);
}

static inline void
geom_deletetree(struct gmesh *mesh)
{

	(void)mesh;
}

#endif /* !_MINKAR_LIBGEOM_H_ */
