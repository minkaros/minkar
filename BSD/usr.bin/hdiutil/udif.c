/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * UDIF disk images, read and written (see udif.h). Reading gives the
 * chunk table the kernel's driver serves a compressed image from;
 * writing (hdiutil convert) stores a disk in 1 MB chunks, compressed as
 * the format says, a chunk of zeros taking no room, and a chunk that does
 * not shrink stored as it is. Checksums (CRC-32) are written: of each
 * "blkx" entry's sectors, of the data fork, and of the entries' checksums.
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/stat.h>

#include <bzlib.h>
#include <compression.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <zlib.h>

#include "udif.h"

#define BE32(x)	OSSwapBigToHostInt32(x)
#define BE64(x)	OSSwapBigToHostInt64(x)
#define TOBE32(x) OSSwapHostToBigInt32(x)
#define TOBE64(x) OSSwapHostToBigInt64(x)

/* Sectors per chunk written: 1 MB. */
#define CHUNK_SECTORS	2048

struct koly {
	char		signature[4];
	uint32_t	version, header_size, flags;
	uint64_t	running_data_fork_offset, data_fork_offset;
	uint64_t	data_fork_length, rsrc_fork_offset, rsrc_fork_length;
	uint32_t	segment_number, segment_count;
	uint8_t		segment_id[16];
	uint32_t	data_checksum_type, data_checksum_size;
	uint32_t	data_checksum[32];
	uint64_t	xml_offset, xml_length;
	uint8_t		reserved1[120];
	uint32_t	checksum_type, checksum_size;
	uint32_t	checksum[32];
	uint32_t	image_variant;
	uint64_t	sector_count;
	uint32_t	reserved2, reserved3, reserved4;
} __attribute__((packed));

/* A "blkx" entry's data: this header, then its chunks. */
struct mish {
	char		signature[4];
	uint32_t	version;
	uint64_t	first_sector, sector_count;
	uint64_t	data_offset;
	uint32_t	buffers_needed, block_descriptors;
	uint32_t	reserved[6];
	uint32_t	checksum_type, checksum_size;
	uint32_t	checksum[32];
	uint32_t	chunk_count;
} __attribute__((packed));

struct mish_chunk {
	uint32_t	type, comment;
	uint64_t	sector, sectors;
	uint64_t	offset, length;
} __attribute__((packed));

_Static_assert(sizeof(struct koly) == 512, "koly");
_Static_assert(sizeof(struct mish) == 204, "mish");
_Static_assert(sizeof(struct mish_chunk) == 40, "mish chunk");

/* ---- Reading ---- */

static int
chunk_order(const void *a, const void *b)
{
	const struct udif_chunk *x = a, *y = b;

	return x->sector < y->sector ? -1 : x->sector > y->sector;
}

static bool
add_chunk(struct image *img, uint32_t *cap, struct udif_chunk c)
{
	if (img->nchunks == *cap) {
		uint32_t n = *cap ? *cap * 2 : 256;
		struct udif_chunk *p = realloc(img->chunks, n * sizeof(*p));

		if (p == NULL)
			return false;
		img->chunks = p;
		*cap = n;
	}
	img->chunks[img->nchunks++] = c;
	return true;
}

/* The chunks of the "blkx" entries, sorted, the gaps between them zeros,
 * runs of zeros merged. */
static bool
read_blkx(int fd, const struct koly *k, struct image *img, const char **why)
{
	uint64_t xoff = BE64(k->xml_offset), xlen = BE64(k->xml_length);
	uint64_t base = BE64(k->data_fork_offset);
	uint64_t dlen = BE64(k->data_fork_length);
	CFPropertyListRef plist = NULL;
	CFDataRef data;
	CFArrayRef blkx = NULL;
	uint32_t cap = 0;
	uint8_t *xml;
	bool ok = false;
	bool zlib = false, bz = false, lz = false, adc = false, lzma = false, raw = false;

	*why = "the image's block table can't be read";
	if (xlen == 0 || xlen > 64 * 1024 * 1024 || (xml = malloc(xlen)) == NULL)
		return false;
	if (pread(fd, xml, xlen, (off_t)xoff) != (ssize_t)xlen) {
		free(xml);
		return false;
	}
	data = CFDataCreateWithBytesNoCopy(NULL, xml, (CFIndex)xlen, kCFAllocatorNull);
	if (data != NULL)
		plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
	if (plist != NULL && CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
		CFDictionaryRef rf = CFDictionaryGetValue(plist, CFSTR("resource-fork"));
		if (rf != NULL && CFGetTypeID(rf) == CFDictionaryGetTypeID())
			blkx = CFDictionaryGetValue(rf, CFSTR("blkx"));
	}
	if (blkx == NULL || CFGetTypeID(blkx) != CFArrayGetTypeID())
		goto out;
	for (CFIndex i = 0; i < CFArrayGetCount(blkx); i++) {
		CFDictionaryRef e = CFArrayGetValueAtIndex(blkx, i);
		CFDataRef d = CFGetTypeID(e) == CFDictionaryGetTypeID() ?
		    CFDictionaryGetValue(e, CFSTR("Data")) : NULL;
		const uint8_t *p;
		struct mish m;
		uint32_t n;

		if (d == NULL || CFGetTypeID(d) != CFDataGetTypeID() ||
		    (size_t)CFDataGetLength(d) < sizeof(m))
			goto out;
		p = CFDataGetBytePtr(d);
		memcpy(&m, p, sizeof(m));
		n = BE32(m.chunk_count);
		if (memcmp(m.signature, "mish", 4) != 0 ||
		    (size_t)CFDataGetLength(d) < sizeof(m) + (size_t)n * sizeof(struct mish_chunk))
			goto out;
		for (uint32_t j = 0; j < n; j++) {
			struct mish_chunk mc;
			struct udif_chunk c;

			memcpy(&mc, p + sizeof(m) + j * sizeof(mc), sizeof(mc));
			c.type = BE32(mc.type);
			if (c.type == UDIF_COMMENT || c.type == UDIF_END || BE64(mc.sectors) == 0)
				continue;
			c.sector = BE64(m.first_sector) + BE64(mc.sector);
			c.sectors = BE64(mc.sectors);
			c.offset = base + BE64(m.data_offset) + BE64(mc.offset);
			c.length = BE64(mc.length);
			c.reserved = 0;
			switch (c.type) {
			case UDIF_ZERO: case UDIF_IGNORE:
				c.type = UDIF_ZERO; c.offset = c.length = 0;
				break;
			case UDIF_RAW: raw = true; break;
			case UDIF_ZLIB: zlib = true; break;
			case UDIF_BZIP2: bz = true; break;
			case UDIF_LZFSE: lz = true; break;
			case UDIF_ADC: adc = true; break;
			case UDIF_LZMA: lzma = true; break;
			default:
				*why = "the image stores sectors in a way not known";
				goto out;
			}
			if (c.type != UDIF_ZERO && (c.offset + c.length > base + dlen ||
			    (c.type == UDIF_RAW && c.length < c.sectors * SECTOR))) {
				*why = "the image's block table points past its data";
				goto out;
			}
			if (!add_chunk(img, &cap, c)) {
				*why = strerror(ENOMEM);
				goto out;
			}
		}
	}
	if (img->nchunks == 0)
		goto out;
	qsort(img->chunks, img->nchunks, sizeof(*img->chunks), chunk_order);
	{
		/* Gaps become zeros; overlaps are refused; zeros merge. */
		struct udif_chunk *in = img->chunks;
		uint32_t nin = img->nchunks;
		uint64_t next = 0;

		img->chunks = NULL;
		img->nchunks = cap = 0;
		for (uint32_t i = 0; i <= nin; i++) {
			uint64_t at = i < nin ? in[i].sector : img->sectors;
			if (at < next || (i < nin && in[i].sector + in[i].sectors > img->sectors)) {
				free(in);
				*why = "the image's block table overlaps itself";
				goto out;
			}
			if (at > next) {
				struct udif_chunk z = { next, at - next, 0, 0, UDIF_ZERO, 0 };
				if (!add_chunk(img, &cap, z)) {
					free(in);
					goto out;
				}
			}
			if (i == nin)
				break;
			struct udif_chunk *last = img->nchunks ? &img->chunks[img->nchunks - 1] : NULL;
			if (last != NULL && last->type == UDIF_ZERO && in[i].type == UDIF_ZERO)
				last->sectors += in[i].sectors;
			else if (!add_chunk(img, &cap, in[i])) {
				free(in);
				goto out;
			}
			next = in[i].sector + in[i].sectors;
		}
		free(in);
	}
	img->format = lzma ? "ULMO" : lz ? "ULFO" : bz ? "UDBZ" : zlib ? "UDZO" : adc ? "UDCO" :
	    raw ? "UDRO" : "UDRO";
	ok = true;
out:
	if (plist != NULL)
		CFRelease(plist);
	if (data != NULL)
		CFRelease(data);
	free(xml);
	if (!ok) {
		free(img->chunks);
		img->chunks = NULL;
		img->nchunks = 0;
	}
	return ok;
}

bool
image_layout(const char *path, struct image *img, const char **why)
{
	struct koly k;
	struct stat st;
	int fd;

	memset(img, 0, sizeof(*img));
	if ((fd = open(path, O_RDONLY)) < 0 || fstat(fd, &st) != 0) {
		*why = strerror(errno);
		if (fd >= 0)
			close(fd);
		return false;
	}
	img->format = "RAW*";
	img->flat = true;
	img->offset = 0;
	img->length = (uint64_t)st.st_size;
	img->sectors = img->length / SECTOR;
	if (st.st_size >= (off_t)sizeof(k) &&
	    pread(fd, &k, sizeof(k), st.st_size - (off_t)sizeof(k)) == (ssize_t)sizeof(k) &&
	    memcmp(k.signature, "koly", 4) == 0) {
		uint64_t sectors = BE64(k.sector_count);
		uint64_t length = BE64(k.data_fork_length);

		if (BE32(k.segment_count) > 1) {
			*why = "a segmented image (not supported)";
			close(fd);
			return false;
		}
		img->sectors = sectors;
		img->offset = BE64(k.data_fork_offset);
		if (length == sectors * SECTOR) {
			/* Read/write UDIF: the data fork is the disk. */
			img->format = "UDRW";
			img->length = length;
		} else {
			img->flat = false;
			img->length = sectors * SECTOR;
			if (!read_blkx(fd, &k, img, why)) {
				close(fd);
				return false;
			}
		}
	}
	close(fd);
	return true;
}

void
image_free(struct image *img)
{
	free(img->chunks);
	img->chunks = NULL;
	img->nchunks = 0;
}

const char *
image_format_description(const char *f)
{
	static const struct { const char *f, *d; } names[] = {
		{ "RAW*", "raw read/write image" },
		{ "UDRW", "UDIF read/write image" },
		{ "UDRO", "UDIF read-only image" },
		{ "UDCO", "UDIF ADC-compressed image" },
		{ "UDZO", "UDIF zlib-compressed image" },
		{ "UDBZ", "UDIF bzip2-compressed image" },
		{ "ULFO", "UDIF lzfse-compressed image" },
		{ "ULMO", "UDIF lzma-compressed image" },
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (strcmp(names[i].f, f) == 0)
			return names[i].d;
	return f;
}

/* ---- Decompressing (convert reads any image) ---- */

/* ADC: see the driver's MinkarDiskImageCodecs.c for the stream's form. */
static bool
adc_decode(const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	size_t i = 0, o = 0;

	while (i < inLen && o < outLen) {
		uint8_t b = in[i];
		size_t n, back;

		if (b & 0x80) {
			n = (size_t)(b & 0x7f) + 1;
			if (i + 1 + n > inLen || o + n > outLen)
				return false;
			memcpy(out + o, in + i + 1, n);
			i += 1 + n;
			o += n;
			continue;
		}
		if (b & 0x40) {
			if (i + 3 > inLen)
				return false;
			n = (size_t)(b & 0x3f) + 4;
			back = ((size_t)in[i + 1] << 8 | in[i + 2]) + 1;
			i += 3;
		} else {
			if (i + 2 > inLen)
				return false;
			n = (size_t)((b >> 2) & 0x0f) + 3;
			back = ((size_t)(b & 0x03) << 8 | in[i + 1]) + 1;
			i += 2;
		}
		if (back > o || o + n > outLen)
			return false;
		for (size_t k = 0; k < n; k++, o++)
			out[o] = out[o - back];
	}
	return o == outLen;
}

static bool
decode(uint32_t type, const uint8_t *in, size_t inLen, uint8_t *out, size_t outLen)
{
	switch (type) {
	case UDIF_ZLIB: {
		uLongf got = outLen;
		return uncompress(out, &got, in, inLen) == Z_OK && got == outLen;
	}
	case UDIF_BZIP2: {
		unsigned int got = (unsigned int)outLen;
		return BZ2_bzBuffToBuffDecompress((char *)out, &got, (char *)(uintptr_t)in,
		    (unsigned int)inLen, 0, 0) == BZ_OK && got == outLen;
	}
	case UDIF_LZFSE:
		return compression_decode_buffer(out, outLen, in, inLen, NULL, COMPRESSION_LZFSE) == outLen;
	case UDIF_LZMA:
		return compression_decode_buffer(out, outLen, in, inLen, NULL, COMPRESSION_LZMA) == outLen;
	case UDIF_ADC:
		return adc_decode(in, inLen, out, outLen);
	}
	return false;
}

/* n sectors of the disk from sector, into buf. */
static bool
read_sectors(int fd, const struct image *img, uint64_t sector, uint64_t n, uint8_t *buf)
{
	if (img->flat) {
		ssize_t got = pread(fd, buf, n * SECTOR, (off_t)(img->offset + sector * SECTOR));

		if (got < 0)
			return false;
		/* Past the file's end (a sparse tail) reads as zeros. */
		memset(buf + got, 0, n * SECTOR - (size_t)got);
		return true;
	}
	while (n > 0) {
		uint32_t lo = 0, hi = img->nchunks;
		while (hi - lo > 1) {
			uint32_t mid = (lo + hi) / 2;
			if (img->chunks[mid].sector <= sector)
				lo = mid;
			else
				hi = mid;
		}
		const struct udif_chunk *c = &img->chunks[lo];
		uint64_t skip = sector - c->sector;
		uint64_t take = c->sectors - skip < n ? c->sectors - skip : n;

		switch (c->type) {
		case UDIF_ZERO:
			memset(buf, 0, take * SECTOR);
			break;
		case UDIF_RAW:
			if (pread(fd, buf, take * SECTOR, (off_t)(c->offset + skip * SECTOR)) !=
			    (ssize_t)(take * SECTOR))
				return false;
			break;
		default: {
			uint8_t *in = malloc(c->length), *out = malloc(c->sectors * SECTOR);
			bool ok = in != NULL && out != NULL &&
			    pread(fd, in, c->length, (off_t)c->offset) == (ssize_t)c->length &&
			    decode(c->type, in, c->length, out, c->sectors * SECTOR);
			if (ok)
				memcpy(buf, out + skip * SECTOR, take * SECTOR);
			free(in);
			free(out);
			if (!ok)
				return false;
		}
		}
		buf += take * SECTOR;
		sector += take;
		n -= take;
	}
	return true;
}

/* ---- Writing ---- */

static bool
all_zero(const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (p[i] != 0)
			return false;
	return true;
}

static bool
write_all(int fd, const void *p, size_t n)
{
	const uint8_t *b = p;

	while (n > 0) {
		ssize_t w = write(fd, b, n);
		if (w <= 0)
			return false;
		b += w;
		n -= (size_t)w;
	}
	return true;
}

/* Compressed into out (room for outCap): its length, or 0 when it does
 * not shrink. */
static size_t
encode(uint32_t type, int level, const uint8_t *in, size_t inLen, uint8_t *out, size_t outCap)
{
	switch (type) {
	case UDIF_ZLIB: {
		uLongf got = outCap;
		if (compress2(out, &got, in, inLen, level) != Z_OK)
			return 0;
		return got < inLen ? got : 0;
	}
	case UDIF_BZIP2: {
		unsigned int got = (unsigned int)outCap;
		if (BZ2_bzBuffToBuffCompress((char *)out, &got, (char *)(uintptr_t)in,
		    (unsigned int)inLen, 9, 0, 0) != BZ_OK)
			return 0;
		return got < inLen ? got : 0;
	}
	case UDIF_LZFSE: {
		size_t got = compression_encode_buffer(out, outCap, in, inLen, NULL, COMPRESSION_LZFSE);
		return got > 0 && got < inLen ? got : 0;
	}
	}
	return 0;
}

static CFDataRef
blkx_plist(const struct mish *m, const struct mish_chunk *chunks, uint32_t n)
{
	size_t size = sizeof(*m) + (size_t)n * sizeof(*chunks);
	uint8_t *bytes = malloc(size);
	CFMutableDictionaryRef root, rf, entry;
	CFMutableArrayRef blkx;
	CFDataRef data, out;

	if (bytes == NULL)
		return NULL;
	memcpy(bytes, m, sizeof(*m));
	memcpy(bytes + sizeof(*m), chunks, (size_t)n * sizeof(*chunks));
	data = CFDataCreate(NULL, bytes, (CFIndex)size);
	free(bytes);

	entry = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(entry, CFSTR("Attributes"), CFSTR("0x0050"));
	CFDictionarySetValue(entry, CFSTR("CFName"), CFSTR("whole disk"));
	CFDictionarySetValue(entry, CFSTR("Name"), CFSTR("whole disk"));
	CFDictionarySetValue(entry, CFSTR("ID"), CFSTR("0"));
	CFDictionarySetValue(entry, CFSTR("Data"), data);
	CFRelease(data);
	blkx = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(blkx, entry);
	CFRelease(entry);
	rf = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(rf, CFSTR("blkx"), blkx);
	CFRelease(blkx);
	root = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(root, CFSTR("resource-fork"), rf);
	CFRelease(rf);
	out = CFPropertyListCreateData(NULL, root, kCFPropertyListXMLFormat_v1_0, 0, NULL);
	CFRelease(root);
	return out;
}

int
image_convert(const char *src, const char *dst, const char *format, int level)
{
	struct image img;
	const char *why;
	uint32_t type;
	bool udtw = false, udrw = false;
	int in, out = -1, rc = 1;
	uint8_t *buf = NULL, *cbuf = NULL;
	struct mish_chunk *chunks = NULL;
	uint32_t nchunks = 0, cap = 0;
	uLong dataCRC = crc32(0, NULL, 0), diskCRC = crc32(0, NULL, 0);
	uint64_t pos = 0, cbufCap = (uint64_t)CHUNK_SECTORS * SECTOR * 2 + 4096;
	char tmp[1024];

	if (strcmp(format, "UDZO") == 0)
		type = UDIF_ZLIB;
	else if (strcmp(format, "UDBZ") == 0)
		type = UDIF_BZIP2;
	else if (strcmp(format, "ULFO") == 0)
		type = UDIF_LZFSE;
	else if (strcmp(format, "UDRO") == 0)
		type = UDIF_RAW;
	else if (strcmp(format, "UDRW") == 0) {
		type = UDIF_RAW;
		udrw = true;
	} else if (strcmp(format, "UDTO") == 0) {
		type = UDIF_RAW;
		udtw = true;
	} else {
		fprintf(stderr, "hdiutil: convert failed - %s: format not supported "
		    "(UDZO, UDBZ, ULFO, UDRO, UDRW, UDTO)\n", format);
		return 1;
	}
	if (level <= 0 || level > 9)
		level = 1;
	if (!image_layout(src, &img, &why)) {
		fprintf(stderr, "hdiutil: convert failed - %s\n", why);
		return 1;
	}
	if ((in = open(src, O_RDONLY)) < 0) {
		warn("%s", src);
		goto out;
	}
	snprintf(tmp, sizeof(tmp), "%s.convert-%d", dst, (int)getpid());
	if ((out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		warn("%s", tmp);
		goto out;
	}
	buf = malloc((size_t)CHUNK_SECTORS * SECTOR);
	cbuf = malloc(cbufCap);
	if (buf == NULL || cbuf == NULL)
		goto out;

	for (uint64_t s = 0; s < img.sectors; s += CHUNK_SECTORS) {
		uint64_t n = img.sectors - s < CHUNK_SECTORS ? img.sectors - s : CHUNK_SECTORS;
		size_t bytes = (size_t)(n * SECTOR), len = 0;
		struct mish_chunk c;

		if (!read_sectors(in, &img, s, n, buf)) {
			fprintf(stderr, "hdiutil: convert failed - sectors %llu to %llu of %s "
			    "can't be read\n", (unsigned long long)s, (unsigned long long)(s + n), src);
			goto out;
		}
		diskCRC = crc32(diskCRC, buf, (uInt)bytes);
		c.comment = 0;
		c.sector = TOBE64(s);
		c.sectors = TOBE64(n);
		c.offset = TOBE64(pos);
		if (!udtw && !udrw && all_zero(buf, bytes)) {
			c.type = TOBE32(UDIF_ZERO);
			c.length = 0;
		} else {
			const uint8_t *data = buf;
			uint32_t t = UDIF_RAW;

			len = bytes;
			if (type != UDIF_RAW) {
				size_t got = encode(type, level, buf, bytes, cbuf, cbufCap);
				if (got > 0) {
					data = cbuf;
					len = got;
					t = type;
				}
			}
			if (!write_all(out, data, len)) {
				warn("%s", tmp);
				goto out;
			}
			dataCRC = crc32(dataCRC, data, (uInt)len);
			c.type = TOBE32(t);
			c.length = TOBE64(len);
			pos += len;
		}
		if (nchunks == cap) {
			cap = cap ? cap * 2 : 1024;
			struct mish_chunk *p = realloc(chunks, (cap + 1) * sizeof(*p));
			if (p == NULL)
				goto out;
			chunks = p;
		}
		chunks[nchunks++] = c;
	}

	if (!udtw) {
		struct mish m;
		struct koly k;
		CFDataRef xml;
		uLong master;
		uint32_t crcBE;

		/* The table's end. */
		memset(&chunks[nchunks], 0, sizeof(chunks[0]));
		chunks[nchunks].type = TOBE32(UDIF_END);
		chunks[nchunks].sector = TOBE64(img.sectors);
		chunks[nchunks].offset = TOBE64(pos);
		nchunks++;

		memset(&m, 0, sizeof(m));
		memcpy(m.signature, "mish", 4);
		m.version = TOBE32(1);
		m.first_sector = 0;
		m.sector_count = TOBE64(img.sectors);
		m.data_offset = 0;
		m.buffers_needed = TOBE32(CHUNK_SECTORS + 8);
		m.block_descriptors = TOBE32(0);
		m.checksum_type = TOBE32(2);	/* CRC-32 */
		m.checksum_size = TOBE32(32);
		m.checksum[0] = TOBE32((uint32_t)diskCRC);
		m.chunk_count = TOBE32(nchunks);
		xml = blkx_plist(&m, chunks, nchunks);
		if (xml == NULL || !write_all(out, CFDataGetBytePtr(xml), (size_t)CFDataGetLength(xml))) {
			if (xml != NULL)
				CFRelease(xml);
			warn("%s", tmp);
			goto out;
		}

		crcBE = TOBE32((uint32_t)diskCRC);
		master = crc32(crc32(0, NULL, 0), (const Bytef *)&crcBE, sizeof(crcBE));
		memset(&k, 0, sizeof(k));
		memcpy(k.signature, "koly", 4);
		k.version = TOBE32(4);
		k.header_size = TOBE32(sizeof(k));
		k.flags = TOBE32(1);		/* flattened */
		k.data_fork_offset = 0;
		k.data_fork_length = TOBE64(pos);
		k.segment_number = TOBE32(1);
		k.segment_count = TOBE32(1);
		uuid_generate(k.segment_id);
		k.data_checksum_type = TOBE32(2);
		k.data_checksum_size = TOBE32(32);
		k.data_checksum[0] = TOBE32((uint32_t)dataCRC);
		k.xml_offset = TOBE64(pos);
		k.xml_length = TOBE64((uint64_t)CFDataGetLength(xml));
		k.checksum_type = TOBE32(2);
		k.checksum_size = TOBE32(32);
		k.checksum[0] = TOBE32((uint32_t)master);
		k.image_variant = TOBE32(1);
		k.sector_count = TOBE64(img.sectors);
		CFRelease(xml);
		if (!write_all(out, &k, sizeof(k))) {
			warn("%s", tmp);
			goto out;
		}
	}
	if (fsync(out) != 0 || close(out) != 0) {
		out = -1;
		warn("%s", tmp);
		goto out;
	}
	out = -1;
	if (rename(tmp, dst) != 0) {
		warn("%s", dst);
		goto out;
	}
	rc = 0;
out:
	if (out >= 0)
		close(out);
	if (rc != 0)
		unlink(tmp);
	if (in >= 0)
		close(in);
	free(buf);
	free(cbuf);
	free(chunks);
	image_free(&img);
	return rc;
}
