/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * HFS+ compression of a file just copied (ditto --hfsCompression; the
 * installer copies the system so, decided 23 Nov 2026), in the format the
 * kernel's decmpfs reads (Kernel/Extensions/MinkarFSCompression): LZFSE,
 * in 64 KB blocks. A file whose compressed form fits keeps it in its
 * "com.apple.decmpfs" attribute, after the header (type 11); a bigger one
 * in its resource fork, a table of little-endian offsets first, the first
 * the table's own size, block i from offset i to i+1 (type 12). A block
 * LZFSE does not make smaller is kept as it is after a 0x06 byte. Then the
 * data fork is emptied and the file flagged UF_COMPRESSED; its times are
 * kept. A file that would not shrink, is empty, has a resource fork or
 * other links, or is not on HFS+, is left alone.
 */

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>

#include <compression.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hfscompress.h"

#define	BLOCK		65536
#define	MAGIC		0x636d7066	/* 'fpmc' */
#define	TYPE_ATTR	11
#define	TYPE_RSRC	12
#define	INLINE_MAX	3802		/* an attribute's data kept in its record */
#define	FILE_MAX	(1ULL << 31)
#ifndef DECMPFS_XATTR_NAME
#define	DECMPFS_XATTR_NAME	"com.apple.decmpfs"
#endif

struct header {
	uint32_t	magic;
	uint32_t	type;
	uint64_t	size;
} __attribute__((packed));

/* One block, compressed, or stored after its marker; its length. */
static size_t
encode(const uint8_t *in, size_t len, uint8_t *out, size_t room, void *scratch)
{
	size_t n = compression_encode_buffer(out, room, in, len, scratch, COMPRESSION_LZFSE);

	if (n == 0 || n >= len + 1) {
		out[0] = 0x06;
		memcpy(out + 1, in, len);
		return (len + 1);
	}
	return (n);
}

int
hfs_compress_file(const char *path)
{
	struct stat sb;
	struct statfs sf;
	struct timeval times[2];
	uint8_t *data = NULL, *packed = NULL, *attr = NULL;
	uint32_t *offsets = NULL;
	void *scratch = NULL;
	size_t blocks, at, room, total;
	ssize_t got;
	struct header h;
	int fd = -1, rc = 0;

	if (lstat(path, &sb) != 0 || !S_ISREG(sb.st_mode) || sb.st_nlink != 1 ||
	    sb.st_size == 0 || (uint64_t)sb.st_size > FILE_MAX || (sb.st_flags & UF_COMPRESSED))
		return (0);
	if (statfs(path, &sf) != 0 || strcmp(sf.f_fstypename, "hfs") != 0)
		return (0);
	if (getxattr(path, XATTR_RESOURCEFORK_NAME, NULL, 0, 0, XATTR_NOFOLLOW) > 0)
		return (0);

	if ((fd = open(path, O_RDONLY | O_NOFOLLOW)) < 0)
		return (-1);
	data = malloc((size_t)sb.st_size);
	blocks = ((size_t)sb.st_size + BLOCK - 1) / BLOCK;
	room = (size_t)sb.st_size + blocks + 64;
	packed = malloc(room);
	offsets = calloc(blocks + 1, sizeof(uint32_t));
	scratch = malloc(compression_encode_scratch_buffer_size(COMPRESSION_LZFSE));
	if (data == NULL || packed == NULL || offsets == NULL || scratch == NULL) {
		rc = -1;
		goto out;
	}
	for (at = 0; at < (size_t)sb.st_size; at += (size_t)got)
		if ((got = read(fd, data + at, (size_t)sb.st_size - at)) <= 0) {
			rc = -1;
			goto out;
		}
	close(fd);
	fd = -1;

	h.magic = MAGIC;
	h.size = (uint64_t)sb.st_size;

	/* Small: the whole file one block, in the attribute. */
	if (blocks == 1) {
		size_t n = encode(data, (size_t)sb.st_size, packed, room, scratch);

		if (n + sizeof(h) <= INLINE_MAX) {
			if (n >= (size_t)sb.st_size)
				goto out;	/* it would not shrink */
			h.type = TYPE_ATTR;
			if ((attr = malloc(sizeof(h) + n)) == NULL) {
				rc = -1;
				goto out;
			}
			memcpy(attr, &h, sizeof(h));
			memcpy(attr + sizeof(h), packed, n);
			if (setxattr(path, DECMPFS_XATTR_NAME, attr, sizeof(h) + n, 0, XATTR_NOFOLLOW) != 0) {
				rc = -1;
				goto out;
			}
			goto flag;
		}
	}

	/* Bigger: the blocks in the resource fork, after their table. */
	total = (blocks + 1) * sizeof(uint32_t);
	{
		uint8_t *body = malloc(room);
		size_t used = 0;

		if (body == NULL) {
			rc = -1;
			goto out;
		}
		for (size_t i = 0; i < blocks; i++) {
			size_t len = (size_t)sb.st_size - i * BLOCK < BLOCK ? (size_t)sb.st_size - i * BLOCK : BLOCK;

			offsets[i] = (uint32_t)(total + used);
			used += encode(data + i * BLOCK, len, body + used, room - used, scratch);
		}
		offsets[blocks] = (uint32_t)(total + used);
		if (total + used >= (size_t)sb.st_size) {
			free(body);
			goto out;	/* it would not shrink */
		}
		memcpy(packed, offsets, total);
		memcpy(packed + total, body, used);
		free(body);
		total += used;
	}
	if (setxattr(path, XATTR_RESOURCEFORK_NAME, packed, total, 0, XATTR_NOFOLLOW) != 0) {
		rc = -1;
		goto out;
	}
	h.type = TYPE_RSRC;
	if (setxattr(path, DECMPFS_XATTR_NAME, &h, sizeof(h), 0, XATTR_NOFOLLOW) != 0) {
		(void)removexattr(path, XATTR_RESOURCEFORK_NAME, XATTR_NOFOLLOW);
		rc = -1;
		goto out;
	}

flag:
	/* The data fork emptied, the file flagged: decmpfs gives its data.
	   Not flagged after all: the file as it was. */
	if (truncate(path, 0) != 0 || chflags(path, sb.st_flags | UF_COMPRESSED) != 0) {
		int back = open(path, O_WRONLY | O_TRUNC | O_NOFOLLOW);

		if (back >= 0) {
			for (at = 0; at < (size_t)sb.st_size; at += (size_t)got)
				if ((got = write(back, data + at, (size_t)sb.st_size - at)) <= 0)
					break;
			close(back);
		}
		(void)removexattr(path, DECMPFS_XATTR_NAME, XATTR_NOFOLLOW);
		(void)removexattr(path, XATTR_RESOURCEFORK_NAME, XATTR_NOFOLLOW);
		rc = -1;
		goto out;
	}
	times[0].tv_sec = sb.st_atimespec.tv_sec;
	times[0].tv_usec = (int)(sb.st_atimespec.tv_nsec / 1000);
	times[1].tv_sec = sb.st_mtimespec.tv_sec;
	times[1].tv_usec = (int)(sb.st_mtimespec.tv_nsec / 1000);
	(void)lutimes(path, times);
	rc = 1;
out:
	if (fd >= 0)
		close(fd);
	free(data);
	free(packed);
	free(offsets);
	free(attr);
	free(scratch);
	return (rc);
}
