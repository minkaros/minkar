/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Compressed UDIF images (hdiutil attach passes their chunk table): the
 * chunk types of a "mish" block table, and the decompressors the kernel
 * has for them.
 */

#ifndef _MINKAR_DISK_IMAGE_CODECS_H_
#define _MINKAR_DISK_IMAGE_CODECS_H_

#include <stddef.h>
#include <stdint.h>

/* A chunk's type, as UDIF's block tables give it. */
enum {
	kMinkarDIChunkZero	= 0x00000000,	/* sectors of zeros */
	kMinkarDIChunkRaw	= 0x00000001,	/* stored as they are */
	kMinkarDIChunkIgnore	= 0x00000002,	/* free space: zeros */
	kMinkarDIChunkADC	= 0x80000004,
	kMinkarDIChunkZlib	= 0x80000005,	/* UDZO */
	kMinkarDIChunkBzip2	= 0x80000006,	/* UDBZ */
	kMinkarDIChunkLZFSE	= 0x80000007,	/* ULFO */
	kMinkarDIChunkLZMA	= 0x80000008,	/* ULMO: xz */
};

/* One run of the disk: sectors [sector, sector + sectors) are stored at
 * [offset, offset + length) of the image file, as type says. */
struct MinkarDiskImageChunk {
	uint64_t	sector, sectors;
	uint64_t	offset, length;
	uint32_t	type, reserved;
};

/* What hdiutil sends to attach a chunked image (kMinkarDiskImageAttachChunks):
 * this header, the path (pathLength bytes with its NUL, padded to 8), then
 * chunkCount chunks, sorted by sector, covering the disk. */
struct MinkarDiskImageChunkTable {
	uint32_t	pathLength, chunkCount;
	uint64_t	sectorCount;
};

#ifdef __cplusplus
extern "C" {
#endif
/* 0 when in decompressed fills out exactly. */
int MinkarDiskImageDecompress(uint32_t type, const void *in, size_t inLen, void *out, size_t outLen);
#ifdef __cplusplus
}
#endif

#endif
