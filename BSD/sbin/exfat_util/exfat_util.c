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
 * exfat.util: diskarbitrationd's probe of exFAT volumes, in the exfat.fs
 * bundle, as on macOS.
 *
 *	exfat.util -p disk removable|fixed readonly|writable
 *		prints the volume's label; exits FSUR_RECOGNIZED if it is
 *		exFAT, FSUR_UNRECOGNIZED if not
 *	exfat.util -k disk
 *		prints the volume's UUID, made from its serial number and
 *		its geometry (name-based, version 3); exits FSUR_IO_SUCCESS
 */

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/loadable_fs.h>

#include <CommonCrypto/CommonDigest.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libkern/OSByteOrder.h>

#include "exfat_format.h"

#define le16(x)	OSSwapLittleToHostInt16(x)
#define le32(x)	OSSwapLittleToHostInt32(x)
#define le64(x)	OSSwapLittleToHostInt64(x)

/* Clusters of the root directory read for the label, at most. */
#define ROOT_CLUSTERS_MAX	64

/* The namespace of exFAT volume UUIDs (ours). */
static const uint8_t exfat_uuid_ns[16] = {
	0x60, 0x9e, 0x0b, 0x44, 0xa9, 0x95, 0x47, 0xf3,
	0x9e, 0xb8, 0x72, 0x18, 0xe3, 0x5b, 0x7a, 0x16
};

static int fd = -1;
static uint32_t devblock = 512;

static void
usage(void)
{
	fprintf(stderr, "usage: exfat.util -p disk removable|fixed "
	    "readonly|writable\n       exfat.util -k disk\n");
	exit(FSUR_INVAL);
}

/* Bytes from the raw device, which reads only whole blocks. */
static int
read_at(uint64_t off, void *dst, size_t len)
{
	uint64_t start = off / devblock * devblock;
	size_t span = (size_t)((off + len + devblock - 1) / devblock * devblock -
	    start);
	uint8_t *tmp;
	int ok;

	if ((tmp = malloc(span)) == NULL)
		return (-1);
	ok = pread(fd, tmp, span, (off_t)start) == (ssize_t)span;
	if (ok)
		memcpy(dst, tmp + (off - start), len);
	free(tmp);
	return (ok ? 0 : -1);
}

static int
boot_ok(const struct exfat_boot *b)
{
	return (memcmp(b->fs_name, EXFAT_NAME, 8) == 0 &&
	    le16(b->signature) == 0xaa55 &&
	    b->sector_shift >= 9 && b->sector_shift <= 12 &&
	    b->cluster_shift <= 25 - b->sector_shift &&
	    (le16(b->revision) >> 8) == 1 &&
	    le32(b->root_cluster) >= EXFAT_FIRST_CLUSTER &&
	    le32(b->root_cluster) < (uint64_t)le32(b->cluster_count) +
	    EXFAT_FIRST_CLUSTER);
}

/* The label, from the root directory's label entry; "" without one. */
static void
print_label(const struct exfat_boot *b)
{
	uint64_t ssize = 1ull << b->sector_shift;
	uint64_t csize = ssize << b->cluster_shift;
	uint64_t fat = le32(b->fat_offset) * ssize;
	uint64_t heap = le32(b->heap_offset) * ssize;
	uint32_t cluster = le32(b->root_cluster), next;
	uint32_t count = le32(b->cluster_count);
	struct exfat_label_entry *l;
	uint8_t *buf;
	unsigned n, i;
	char out[EXFAT_LABEL_MAX * 3 + 1], *p;

	if ((buf = malloc((size_t)csize)) == NULL)
		return;
	for (n = 0; n < ROOT_CLUSTERS_MAX; n++) {
		if (cluster < EXFAT_FIRST_CLUSTER ||
		    cluster >= (uint64_t)count + EXFAT_FIRST_CLUSTER ||
		    read_at(heap + (uint64_t)(cluster - 2) * csize, buf,
		    (size_t)csize) != 0)
			break;
		for (i = 0; i < csize; i += EXFAT_ENTRY_SIZE) {
			if (buf[i] == EXFAT_TYPE_END)
				goto out;
			if (buf[i] != EXFAT_TYPE_LABEL)
				continue;
			l = (struct exfat_label_entry *)(buf + i);
			p = out;
			for (unsigned k = 0; k < l->length &&
			    k < EXFAT_LABEL_MAX; k++) {
				uint16_t c = le16(l->label[k]);

				if (c < 0x80)
					*p++ = (char)c;
				else if (c < 0x800) {
					*p++ = (char)(0xc0 | (c >> 6));
					*p++ = (char)(0x80 | (c & 0x3f));
				} else {
					*p++ = (char)(0xe0 | (c >> 12));
					*p++ = (char)(0x80 | ((c >> 6) & 0x3f));
					*p++ = (char)(0x80 | (c & 0x3f));
				}
			}
			*p = '\0';
			printf("%s", out);
			goto out;
		}
		if (read_at(fat + (uint64_t)cluster * 4, &next,
		    sizeof(next)) != 0)
			break;
		cluster = le32(next);
	}
out:
	free(buf);
}

static void
print_uuid(const struct exfat_boot *b)
{
	CC_MD5_CTX ctx;
	uint8_t md[CC_MD5_DIGEST_LENGTH];

	CC_MD5_Init(&ctx);
	CC_MD5_Update(&ctx, exfat_uuid_ns, sizeof(exfat_uuid_ns));
	CC_MD5_Update(&ctx, &b->serial, sizeof(b->serial));
	CC_MD5_Update(&ctx, &b->volume_length, sizeof(b->volume_length));
	CC_MD5_Update(&ctx, &b->heap_offset, sizeof(b->heap_offset));
	CC_MD5_Update(&ctx, &b->cluster_count, sizeof(b->cluster_count));
	CC_MD5_Final(md, &ctx);
	md[6] = (md[6] & 0x0f) | 0x30;		/* version 3 */
	md[8] = (md[8] & 0x3f) | 0x80;		/* RFC 4122 */
	printf("%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-"
	    "%02X%02X%02X%02X%02X%02X", md[0], md[1], md[2], md[3], md[4],
	    md[5], md[6], md[7], md[8], md[9], md[10], md[11], md[12], md[13],
	    md[14], md[15]);
}

int
main(int argc, char *argv[])
{
	struct exfat_boot boot;
	char path[1024];
	int probe;

	if (argc < 3 || argv[1][0] != '-' || argv[1][2] != '\0')
		usage();
	if (argv[1][1] == 'p' && argc == 5)
		probe = 1;
	else if (argv[1][1] == 'k' && argc == 3)
		probe = 0;
	else
		usage();

	if (strncmp(argv[2], "/dev/", 5) == 0)
		snprintf(path, sizeof(path), "%s", argv[2]);
	else
		snprintf(path, sizeof(path), "/dev/r%s", argv[2]);
	if ((fd = open(path, O_RDONLY)) == -1) {
		perror(path);
		return (FSUR_IO_FAIL);
	}
	if (ioctl(fd, DKIOCGETBLOCKSIZE, &devblock) == -1 || devblock == 0)
		devblock = 512;
	if (read_at(0, &boot, sizeof(boot)) != 0)
		return (FSUR_IO_FAIL);
	if (!boot_ok(&boot))
		return (FSUR_UNRECOGNIZED);

	if (probe) {
		print_label(&boot);
		return (FSUR_RECOGNIZED);
	}
	print_uuid(&boot);
	return (FSUR_IO_SUCCESS);
}
