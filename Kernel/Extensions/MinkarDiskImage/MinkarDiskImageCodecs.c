/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The decompressors of compressed UDIF images' chunks, in the kernel:
 * zlib (UDZO) through libkern's zlib, bzip2 (UDBZ) through bzip2's own
 * sources (built with BZ_NO_STDIO, its allocations on IOMalloc), LZFSE
 * (ULFO) through the LZFSE reference sources, and ADC, the old chunk
 * compression of disk images, written here from its published
 * description. Each fills the whole of out or fails.
 */

#include <IOKit/IOLib.h>
#include <libkern/zlib.h>
#include <string.h>
#include <stdint.h>
#include "MinkarDiskImageCodecs.h"
#include "bzlib.h"
#include "lzfse.h"
#include "lzma.h"

/* ---- Allocations for bzip2 (malloc and free are mapped here) ---- */

void *minkar_di_malloc(size_t size);
void minkar_di_free(void *p);
void bz_internal_error(int errcode);

/* The size is kept before the block, as IOFree needs it. */
void *
minkar_di_malloc(size_t size)
{
	size_t *p = (size_t *)IOMalloc(size + sizeof(size_t) * 2);

	if (p == NULL)
		return NULL;
	p[0] = size + sizeof(size_t) * 2;
	return p + 2;
}

/* liblzma's: zeroed, as calloc's. */
void *minkar_di_calloc(size_t count, size_t size);

void *
minkar_di_calloc(size_t count, size_t size)
{
	void *p;

	if (size != 0 && count > SIZE_MAX / size)
		return NULL;
	p = minkar_di_malloc(count * size);
	if (p != NULL)
		memset(p, 0, count * size);
	return p;
}

void
minkar_di_free(void *q)
{
	size_t *p = q;

	if (p != NULL)
		IOFree(p - 2, p[-2]);
}

/* bzip2's own consistency checks: a broken stream is an I/O error, not a
 * panic. */
void
bz_internal_error(int errcode)
{
	IOLog("MinkarDiskImage: bzip2 internal error %d\n", errcode);
}

/* bzip2 here computes its CRC with a vector routine when the CPU says it
 * can; the kernel's copy keeps to the plain one: no capabilities, and a
 * crc32_vec that walks the same table. */
uint64_t _get_cpu_capabilities(void);
int crc32_vec(int crc, char *p, int len);
extern const unsigned int BZ2_crc32Table[256];

uint64_t
_get_cpu_capabilities(void)
{
	return 0;
}

int
crc32_vec(int crc, char *p, int len)
{
	unsigned int c = (unsigned int)crc;

	while (len-- > 0)
		c = (c << 8) ^ BZ2_crc32Table[(c >> 24) ^ (unsigned char)*p++];
	return (int)c;
}

/* ---- zlib ---- */

/* libkern's zlib has no allocator of its own: the stream brings one. */
static voidpf
zalloc_chunk(voidpf opaque, uInt items, uInt size)
{
	(void)opaque;
	return minkar_di_malloc((size_t)items * size);
}

static void
zfree_chunk(voidpf opaque, voidpf p)
{
	(void)opaque;
	minkar_di_free(p);
}

static int
inflate_chunk(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	z_stream z;
	int r;

	memset(&z, 0, sizeof(z));
	z.zalloc = zalloc_chunk;
	z.zfree = zfree_chunk;
	z.next_in = (Bytef *)(uintptr_t)in;
	z.avail_in = (uInt)inLen;
	z.next_out = out;
	z.avail_out = (uInt)outLen;
	if (inflateInit(&z) != Z_OK)
		return -1;
	r = inflate(&z, Z_FINISH);
	inflateEnd(&z);
	return (r == Z_STREAM_END || r == Z_OK) && z.avail_out == 0 ? 0 : -1;
}

/* ---- ADC: a byte stream of literals and back-references. A byte with
 * the top bit set starts (low 7 bits + 1) literal bytes; else bit 6 set: a
 * three-byte copy of (low 6 bits + 4) bytes at offset (next two bytes,
 * big-endian, + 1) back; else a two-byte copy of ((bits 2-5) + 3) bytes at
 * offset (((low 2 bits) << 8 | next byte) + 1) back. ---- */

static int
adc_chunk(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	size_t i = 0, o = 0;

	while (i < inLen && o < outLen) {
		uint8_t b = in[i];
		size_t n, back;

		if (b & 0x80) {
			n = (size_t)(b & 0x7f) + 1;
			if (i + 1 + n > inLen || o + n > outLen)
				return -1;
			memcpy(out + o, in + i + 1, n);
			i += 1 + n;
			o += n;
			continue;
		}
		if (b & 0x40) {
			if (i + 3 > inLen)
				return -1;
			n = (size_t)(b & 0x3f) + 4;
			back = ((size_t)in[i + 1] << 8 | in[i + 2]) + 1;
			i += 3;
		} else {
			if (i + 2 > inLen)
				return -1;
			n = (size_t)((b >> 2) & 0x0f) + 3;
			back = ((size_t)(b & 0x03) << 8 | in[i + 1]) + 1;
			i += 2;
		}
		if (back > o || o + n > outLen)
			return -1;
		/* Byte by byte: a copy may overlap what it writes. */
		for (size_t k = 0; k < n; k++, o++)
			out[o] = out[o - back];
	}
	return o == outLen ? 0 : -1;
}

/* ---- The dispatcher ---- */

int
MinkarDiskImageDecompress(uint32_t type, const void *in, size_t inLen, void *out, size_t outLen)
{
	switch (type) {
	case kMinkarDIChunkZlib:
		return inflate_chunk(in, inLen, out, outLen);
	case kMinkarDIChunkBzip2: {
		unsigned int got = (unsigned int)outLen;
		int r = BZ2_bzBuffToBuffDecompress(out, &got, (char *)(uintptr_t)in, (unsigned int)inLen, 0, 0);
		return r == BZ_OK && got == outLen ? 0 : -1;
	}
	case kMinkarDIChunkLZFSE: {
		size_t scratchSize = lzfse_decode_scratch_size();
		void *scratch = minkar_di_malloc(scratchSize);
		size_t got;
		if (scratch == NULL)
			return -1;
		got = lzfse_decode_buffer(out, outLen, in, inLen, scratch);
		minkar_di_free(scratch);
		return got == outLen ? 0 : -1;
	}
	case kMinkarDIChunkADC:
		return adc_chunk(in, inLen, out, outLen);
	case kMinkarDIChunkLZMA: {
		/* An xz stream (liblzma's own allocations go through malloc). */
		uint64_t memlimit = 256 << 20;
		size_t inPos = 0, outPos = 0;
		lzma_ret r = lzma_stream_buffer_decode(&memlimit, 0, NULL, in, &inPos, inLen,
		    out, &outPos, outLen);
		return r == LZMA_OK && outPos == outLen ? 0 : -1;
	}
	}
	return -1;
}
