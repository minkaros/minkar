/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The compression types of HFS+ compressed files, for the kernel's
 * decmpfs, written from the format's published description. A compressed
 * file keeps a header in its "com.apple.decmpfs" attribute (magic, type,
 * uncompressed size, little-endian) and its data either right after that
 * header or in its resource fork:
 *
 *   3   zlib, in the attribute
 *   4   zlib, in the resource fork: a resource-fork header (big-endian,
 *       its data at an offset, usually 0x100), at that offset the data's
 *       length (big-endian) and a block table: a count, then each block's
 *       offset (from the length field's end) and size, little-endian
 *   7   LZVN, in the attribute
 *   8   LZVN, in the resource fork: a table of little-endian offsets, the
 *       first one the table's own size; block i runs from offset i to i+1
 *   11  LZFSE, in the attribute
 *   12  LZFSE, in the resource fork, laid out as type 8
 *
 * Blocks hold 64 KB each, the last one less. A block (or the attribute's
 * data) stored as it is starts with a marker byte: 0xFF (any byte whose
 * low four bits are set) for zlib, 0x06 for LZVN and LZFSE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <sys/decmpfs.h>
#include <libkern/OSByteOrder.h>
#include <libkern/zlib.h>
#include <IOKit/IOLib.h>
#include <string.h>

#include "lzfse.h"
#include "lzvn_decode_base.h"
#include "MinkarFSCompressionTypes.h"

#define BLOCK		65536u
#define MAX_TABLE	(16u * 1024 * 1024)	/* a 1 TB file's table, about */

void *minkar_fsc_malloc(size_t size);
void minkar_fsc_free(void *p);

/* For LZFSE's sources (malloc and free map here) and our own buffers: the
 * size kept before the block, as IOFree needs it. */
void *
minkar_fsc_malloc(size_t size)
{
	size_t *p = (size_t *)IOMalloc(size + sizeof(size_t) * 2);

	if (p == NULL)
		return NULL;
	p[0] = size + sizeof(size_t) * 2;
	return p + 2;
}

void
minkar_fsc_free(void *q)
{
	size_t *p = q;

	if (p != NULL)
		IOFree(p - 2, p[-2]);
}

/* ---- Decompressors: each fills out[0, outLen) exactly, or fails ---- */

static voidpf
zalloc_fsc(voidpf opaque, uInt items, uInt size)
{
	(void)opaque;
	return minkar_fsc_malloc((size_t)items * size);
}

static void
zfree_fsc(voidpf opaque, voidpf p)
{
	(void)opaque;
	minkar_fsc_free(p);
}

static int
zlib_block(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	z_stream z;
	int r;

	if (inLen > 0 && (in[0] & 0x0f) == 0x0f) {
		if (inLen - 1 < outLen)
			return EIO;
		memcpy(out, in + 1, outLen);
		return 0;
	}
	memset(&z, 0, sizeof(z));
	z.zalloc = zalloc_fsc;
	z.zfree = zfree_fsc;
	z.next_in = (Bytef *)(uintptr_t)in;
	z.avail_in = (uInt)inLen;
	z.next_out = out;
	z.avail_out = (uInt)outLen;
	if (inflateInit(&z) != Z_OK)
		return ENOMEM;
	r = inflate(&z, Z_FINISH);
	inflateEnd(&z);
	return (r == Z_STREAM_END || r == Z_OK) && z.avail_out == 0 ? 0 : EIO;
}

static int
lzvn_block(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	lzvn_decoder_state s;

	if (inLen > 0 && in[0] == 0x06) {
		if (inLen - 1 < outLen)
			return EIO;
		memcpy(out, in + 1, outLen);
		return 0;
	}
	memset(&s, 0, sizeof(s));
	s.src = in;
	s.src_end = in + inLen;
	s.dst = s.dst_begin = out;
	s.dst_end = out + outLen;
	lzvn_decode(&s);
	return (size_t)(s.dst - out) == outLen ? 0 : EIO;
}

static int
lzfse_block(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	void *scratch;
	size_t got;

	if (inLen > 0 && in[0] == 0x06) {
		if (inLen - 1 < outLen)
			return EIO;
		memcpy(out, in + 1, outLen);
		return 0;
	}
	scratch = minkar_fsc_malloc(lzfse_decode_scratch_size());
	if (scratch == NULL)
		return ENOMEM;
	got = lzfse_decode_buffer(out, outLen, in, inLen, scratch);
	minkar_fsc_free(scratch);
	return got == outLen ? 0 : EIO;
}

typedef int (*block_func)(const uint8_t *, size_t, uint8_t *, size_t);

static block_func
decoder_for(uint32_t type)
{
	switch (type) {
	case 3: case 4: return zlib_block;
	case 7: case 8: return lzvn_block;
	case 11: case 12: return lzfse_block;
	}
	return NULL;
}

static bool
in_rsrc(uint32_t type)
{
	return type == 4 || type == 8 || type == 12;
}

/* ---- Handing data back ---- */

/* bytes (of the file, from at) into the vectors that hold the file from
 * base on: what of it falls within them. */
static void
to_vectors(const uint8_t *bytes, off_t at, size_t n, off_t base, int nvec, decmpfs_vector *vec)
{
	off_t vstart = base;

	for (int i = 0; i < nvec; i++) {
		off_t vend = vstart + vec[i].size;
		off_t lo = at > vstart ? at : vstart;
		off_t hi = (at + (off_t)n) < vend ? at + (off_t)n : vend;

		if (lo < hi)
			memcpy((uint8_t *)vec[i].buf + (lo - vstart), bytes + (lo - at), (size_t)(hi - lo));
		vstart = vend;
	}
}

/* n bytes of the resource fork at offset, all of them. */
static int
read_rsrc(vnode_t vp, vfs_context_t ctx, uint64_t offset, void *buf, size_t n)
{
	uio_t uio = uio_create(1, (off_t)offset, UIO_SYSSPACE, UIO_READ);
	size_t size = 0;
	int err;

	if (uio == NULL)
		return ENOMEM;
	uio_addiov(uio, CAST_USER_ADDR_T(buf), n);
	err = VNOP_GETXATTR(vp, XATTR_RESOURCEFORK_NAME, uio, &size, XATTR_NOSECURITY, ctx);
	if (err == 0 && uio_resid(uio) != 0)
		err = EIO;
	uio_free(uio);
	return err;
}

/* Where block i is in the resource fork. The table is read once per call;
 * a call covers a few blocks. */
struct table {
	uint32_t	 type, count;
	uint64_t	 base;		/* what the block offsets count from */
	uint32_t	*entries;	/* zlib: offset, size pairs; else offsets */
	size_t		 bytes;
};

static int
read_table(vnode_t vp, vfs_context_t ctx, uint32_t type, uint64_t size, struct table *t)
{
	uint32_t want = (uint32_t)((size + BLOCK - 1) / BLOCK);
	int err;

	memset(t, 0, sizeof(*t));
	t->type = type;
	if (type == 4) {
		uint8_t head[16];
		uint32_t dataOffset, count;

		if ((err = read_rsrc(vp, ctx, 0, head, sizeof(head))) != 0)
			return err;
		dataOffset = OSReadBigInt32(head, 0);
		t->base = (uint64_t)dataOffset + 4;
		if ((err = read_rsrc(vp, ctx, t->base, &count, sizeof(count))) != 0)
			return err;
		count = OSSwapLittleToHostInt32(count);
		if (count != want || (uint64_t)count * 8 > MAX_TABLE)
			return EIO;
		t->count = count;
		t->bytes = (size_t)count * 8;
		t->entries = minkar_fsc_malloc(t->bytes ? t->bytes : 1);
		if (t->entries == NULL)
			return ENOMEM;
		if (count > 0 && (err = read_rsrc(vp, ctx, t->base + 4, t->entries, t->bytes)) != 0)
			return err;
		for (uint32_t i = 0; i < 2 * count; i++)
			t->entries[i] = OSSwapLittleToHostInt32(t->entries[i]);
	} else {
		uint32_t first;

		if ((err = read_rsrc(vp, ctx, 0, &first, sizeof(first))) != 0)
			return err;
		first = OSSwapLittleToHostInt32(first);
		if (first % 4 != 0 || first / 4 != want + 1 || first > MAX_TABLE)
			return EIO;
		t->count = want;
		t->bytes = first;
		t->entries = minkar_fsc_malloc(first);
		if (t->entries == NULL)
			return ENOMEM;
		if ((err = read_rsrc(vp, ctx, 0, t->entries, first)) != 0)
			return err;
		for (uint32_t i = 0; i <= want; i++) {
			t->entries[i] = OSSwapLittleToHostInt32(t->entries[i]);
			if (i > 0 && t->entries[i] < t->entries[i - 1])
				return EIO;
		}
	}
	return 0;
}

static void
block_place(const struct table *t, uint32_t i, uint64_t *offset, uint32_t *length)
{
	if (t->type == 4) {
		*offset = t->base + t->entries[2 * i];
		*length = t->entries[2 * i + 1];
	} else {
		*offset = t->entries[i];
		*length = t->entries[i + 1] - t->entries[i];
	}
}

/* ---- What decmpfs calls ---- */

static int
fsc_validate(vnode_t vp, vfs_context_t ctx, decmpfs_header *hdr)
{
	struct table t;
	int err;

	if (decoder_for(hdr->compression_type) == NULL)
		return ENOTSUP;
	if (!in_rsrc(hdr->compression_type))
		return hdr->attr_size > sizeof(decmpfs_disk_header) || hdr->uncompressed_size == 0 ? 0 : EIO;
	err = read_table(vp, ctx, hdr->compression_type, hdr->uncompressed_size, &t);
	minkar_fsc_free(t.entries);
	return err;
}

/* Reads from the resource fork go by whole blocks. */
static void
fsc_adjust_fetch(vnode_t vp, vfs_context_t ctx, decmpfs_header *hdr, off_t *offset, user_ssize_t *size)
{
	off_t start, end;

	(void)vp; (void)ctx;
	if (!in_rsrc(hdr->compression_type))
		return;
	start = *offset & ~(off_t)(BLOCK - 1);
	end = (*offset + *size + BLOCK - 1) & ~(off_t)(BLOCK - 1);
	*offset = start;
	*size = end - start;
}

static int
fsc_fetch(vnode_t vp, vfs_context_t ctx, decmpfs_header *hdr, off_t offset, user_ssize_t size,
    int nvec, decmpfs_vector *vec, uint64_t *bytes_read)
{
	block_func decode = decoder_for(hdr->compression_type);
	uint64_t total = hdr->uncompressed_size;
	int err = 0;

	*bytes_read = 0;
	if (decode == NULL)
		return ENOTSUP;
	if (offset < 0 || (uint64_t)offset >= total || size <= 0)
		return 0;
	if ((uint64_t)(offset + size) > total)
		size = (user_ssize_t)(total - (uint64_t)offset);

	if (!in_rsrc(hdr->compression_type)) {
		/* The whole file, from the attribute: small by nature. */
		size_t inLen = hdr->attr_size - sizeof(decmpfs_disk_header);
		uint8_t *out;

		if (hdr->attr_size < sizeof(decmpfs_disk_header) || total > 64ull * 1024 * 1024)
			return EIO;
		out = minkar_fsc_malloc((size_t)total);
		if (out == NULL)
			return ENOMEM;
		err = decode(hdr->attr_bytes, inLen, out, (size_t)total);
		if (err == 0) {
			to_vectors(out + offset, offset, (size_t)size, offset, nvec, vec);
			*bytes_read = (uint64_t)size;
		}
		minkar_fsc_free(out);
		return err;
	}

	struct table t;
	uint8_t *in = NULL, *out = NULL;
	size_t inCap = 0;

	if ((err = read_table(vp, ctx, hdr->compression_type, total, &t)) != 0)
		goto done;
	out = minkar_fsc_malloc(BLOCK);
	if (out == NULL) {
		err = ENOMEM;
		goto done;
	}
	for (uint32_t b = (uint32_t)(offset / BLOCK); (off_t)b * BLOCK < offset + size; b++) {
		uint64_t at;
		uint32_t len;
		size_t outLen = (size_t)(total - (uint64_t)b * BLOCK < BLOCK ? total - (uint64_t)b * BLOCK : BLOCK);

		block_place(&t, b, &at, &len);
		if (len == 0 || len > 2 * BLOCK) {
			err = EIO;
			break;
		}
		if (len > inCap) {
			minkar_fsc_free(in);
			inCap = len;
			if ((in = minkar_fsc_malloc(inCap)) == NULL) {
				err = ENOMEM;
				break;
			}
		}
		if ((err = read_rsrc(vp, ctx, at, in, len)) != 0)
			break;
		if ((err = decode(in, len, out, outLen)) != 0) {
			printf("MinkarFSCompression: block %u (type %u) does not decompress\n", b,
			    hdr->compression_type);
			break;
		}
		to_vectors(out, (off_t)b * BLOCK, outLen, offset, nvec, vec);
	}
	if (err == 0)
		*bytes_read = (uint64_t)size;
done:
	minkar_fsc_free(in);
	minkar_fsc_free(out);
	minkar_fsc_free(t.entries);
	return err;
}

static const decmpfs_registration registration = {
	.decmpfs_registration = DECMPFS_REGISTRATION_VERSION_V1,
	.validate = fsc_validate,
	.adjust_fetch = fsc_adjust_fetch,
	.fetch = fsc_fetch,
	.free_data = NULL,	/* the resource fork stays when a file is decompressed in place */
	.get_flags = NULL,
};

const uint32_t MinkarFSCompressionTypes[] = { 3, 4, 7, 8, 11, 12 };
const unsigned MinkarFSCompressionTypeCount = sizeof(MinkarFSCompressionTypes) / sizeof(MinkarFSCompressionTypes[0]);

int
MinkarFSCompressionRegister(uint32_t type)
{
	return register_decmpfs_decompressor(type, &registration);
}

void
MinkarFSCompressionUnregister(uint32_t type)
{
	unregister_decmpfs_decompressor(type, (decmpfs_registration *)(uintptr_t)&registration);
}
