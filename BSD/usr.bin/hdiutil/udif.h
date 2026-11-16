/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * UDIF disk images: a data fork, a property list whose "blkx" entries
 * say where each run of the disk's sectors is in the data fork and how it
 * is stored, and a 512-byte "koly" trailer pointing at both. Read and
 * written from the format's published description.
 */

#ifndef _HDIUTIL_UDIF_H_
#define _HDIUTIL_UDIF_H_

#include <stdbool.h>
#include <stdint.h>

#define SECTOR		512

/* A run's storage, as the "mish" block tables give it. */
#define UDIF_ZERO	0x00000000u
#define UDIF_RAW	0x00000001u
#define UDIF_IGNORE	0x00000002u
#define UDIF_ADC	0x80000004u
#define UDIF_ZLIB	0x80000005u
#define UDIF_BZIP2	0x80000006u
#define UDIF_LZFSE	0x80000007u
#define UDIF_LZMA	0x80000008u
#define UDIF_COMMENT	0x7ffffffeu
#define UDIF_END	0xffffffffu

/* The same layout as the driver's MinkarDiskImageChunk: sectors [sector,
 * sector + sectors) are at [offset, offset + length) of the file. */
struct udif_chunk {
	uint64_t	sector, sectors;
	uint64_t	offset, length;
	uint32_t	type, reserved;
};

struct image {
	const char		*format;	/* "RAW*", "UDRW", "UDZO"... */
	uint64_t		 offset, length; /* the disk in the file (flat) */
	uint64_t		 sectors;
	bool			 flat;		/* the disk is in the file as is */
	struct udif_chunk	*chunks;	/* sorted, covering the disk */
	uint32_t		 nchunks;
};

/* What the image is and where its disk is; false (why says) when it
 * can't be read. */
bool image_layout(const char *path, struct image *img, const char **why);
void image_free(struct image *img);

/* What format names, for imageinfo. */
const char *image_format_description(const char *format);

/* src (any image this reads) written as a new image in format (UDZO,
 * UDBZ, ULFO, UDRO, UDRW, UDTO); level is zlib's (1-9), 0 for its
 * default. 0 or an error printed. */
int image_convert(const char *src, const char *dst, const char *format, int level);

#endif
