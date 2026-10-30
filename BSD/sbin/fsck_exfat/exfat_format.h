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
 * exFAT's on-disk format, from Microsoft's published exFAT specification:
 * the boot region, the FAT, the cluster heap and the directory entries,
 * and the checksums and hashes over them. Shared by newfs_exfat,
 * fsck_exfat and the exfat kext. All on-disk values are little-endian.
 */

#ifndef _EXFAT_FORMAT_H_
#define _EXFAT_FORMAT_H_

#include <stdint.h>

/* The boot region: 12 sectors, then its backup at sector 12. */
#define EXFAT_BOOT_SECTORS	12
#define EXFAT_BACKUP_BOOT	12
#define EXFAT_CHECKSUM_SECTOR	11
#define EXFAT_NAME		"EXFAT   "

struct exfat_boot {
	uint8_t		jump[3];		/* EB 76 90 */
	char		fs_name[8];		/* "EXFAT   " */
	uint8_t		zero[53];
	uint64_t	partition_offset;
	uint64_t	volume_length;		/* in sectors */
	uint32_t	fat_offset;		/* in sectors */
	uint32_t	fat_length;		/* in sectors */
	uint32_t	heap_offset;		/* the cluster heap, in sectors */
	uint32_t	cluster_count;
	uint32_t	root_cluster;
	uint32_t	serial;
	uint16_t	revision;		/* 0x0100 */
	uint16_t	flags;
	uint8_t		sector_shift;		/* 9 to 12 */
	uint8_t		cluster_shift;		/* sectors per cluster */
	uint8_t		nfats;			/* 1 (2 with TexFAT) */
	uint8_t		drive;			/* 0x80 */
	uint8_t		percent_used;		/* 0xff: unknown */
	uint8_t		reserved[7];
	uint8_t		boot_code[390];
	uint16_t	signature;		/* 0xaa55 */
} __attribute__((packed));

/* Volume flags. */
#define EXFAT_ACTIVE_FAT	0x0001
#define EXFAT_DIRTY		0x0002
#define EXFAT_MEDIA_FAILURE	0x0004
#define EXFAT_CLEAR_TO_ZERO	0x0008

/* FAT entries. */
#define EXFAT_FIRST_CLUSTER	2
#define EXFAT_BAD		0xfffffff7u
#define EXFAT_EOC		0xffffffffu
#define EXFAT_MEDIA		0xfffffff8u

/* Directory entries: 32 bytes; the type's top bit says it is in use. */
#define EXFAT_ENTRY_SIZE	32
#define EXFAT_INUSE		0x80
#define EXFAT_SECONDARY		0x40	/* category */
#define EXFAT_BENIGN		0x20	/* importance */
#define EXFAT_TYPE_END		0x00	/* no entry from here on */
#define EXFAT_TYPE_BITMAP	0x81
#define EXFAT_TYPE_UPCASE	0x82
#define EXFAT_TYPE_LABEL	0x83
#define EXFAT_TYPE_FILE		0x85
#define EXFAT_TYPE_GUID		0xa0
#define EXFAT_TYPE_STREAM	0xc0
#define EXFAT_TYPE_NAME		0xc1

struct exfat_bitmap_entry {
	uint8_t		type;			/* 0x81 */
	uint8_t		flags;			/* bit 0: which bitmap */
	uint8_t		reserved[18];
	uint32_t	first_cluster;
	uint64_t	length;
} __attribute__((packed));

struct exfat_upcase_entry {
	uint8_t		type;			/* 0x82 */
	uint8_t		reserved1[3];
	uint32_t	checksum;
	uint8_t		reserved2[12];
	uint32_t	first_cluster;
	uint64_t	length;
} __attribute__((packed));

#define EXFAT_LABEL_MAX		11
struct exfat_label_entry {
	uint8_t		type;			/* 0x83 */
	uint8_t		length;			/* in characters */
	uint16_t	label[EXFAT_LABEL_MAX];	/* UTF-16 */
	uint8_t		reserved[8];
} __attribute__((packed));

struct exfat_guid_entry {
	uint8_t		type;			/* 0xa0 */
	uint8_t		secondary_count;	/* 0 */
	uint16_t	checksum;
	uint16_t	flags;
	uint8_t		guid[16];
	uint8_t		reserved[10];
} __attribute__((packed));

struct exfat_file_entry {
	uint8_t		type;			/* 0x85 */
	uint8_t		secondary_count;
	uint16_t	checksum;		/* of the whole set */
	uint16_t	attributes;
	uint16_t	reserved1;
	uint32_t	create_time;
	uint32_t	modify_time;
	uint32_t	access_time;
	uint8_t		create_10ms;
	uint8_t		modify_10ms;
	uint8_t		create_utc;
	uint8_t		modify_utc;
	uint8_t		access_utc;
	uint8_t		reserved2[7];
} __attribute__((packed));

/* File attributes. */
#define EXFAT_ATTR_RDONLY	0x0001
#define EXFAT_ATTR_HIDDEN	0x0002
#define EXFAT_ATTR_SYSTEM	0x0004
#define EXFAT_ATTR_DIR		0x0010
#define EXFAT_ATTR_ARCHIVE	0x0020

struct exfat_stream_entry {
	uint8_t		type;			/* 0xc0 */
	uint8_t		flags;
	uint8_t		reserved1;
	uint8_t		name_length;		/* in characters */
	uint16_t	name_hash;
	uint16_t	reserved2;
	uint64_t	valid_length;
	uint32_t	reserved3;
	uint32_t	first_cluster;
	uint64_t	length;
} __attribute__((packed));

/* General secondary flags. */
#define EXFAT_ALLOC_POSSIBLE	0x01
#define EXFAT_NO_FAT_CHAIN	0x02	/* contiguous: the FAT is not used */

#define EXFAT_NAME_CHARS	15	/* in a name entry */
#define EXFAT_NAME_MAX		255
struct exfat_name_entry {
	uint8_t		type;			/* 0xc1 */
	uint8_t		flags;
	uint16_t	name[EXFAT_NAME_CHARS];	/* UTF-16 */
} __attribute__((packed));

/* The boot region's checksum: sectors 0-10, less the flags and the use. */
static inline uint32_t
exfat_boot_checksum(const uint8_t *region, unsigned sector_size)
{
	uint32_t sum = 0;
	unsigned i;

	for (i = 0; i < sector_size * EXFAT_CHECKSUM_SECTOR; i++) {
		if (i == 106 || i == 107 || i == 112)
			continue;
		sum = ((sum & 1) ? 0x80000000u : 0) + (sum >> 1) + region[i];
	}
	return (sum);
}

/* An entry set's checksum: all its entries, less the checksum itself. */
static inline uint16_t
exfat_set_checksum(const uint8_t *set, unsigned count)
{
	uint16_t sum = 0;
	unsigned i;

	for (i = 0; i < count * EXFAT_ENTRY_SIZE; i++) {
		if (i == 2 || i == 3)
			continue;
		sum = (uint16_t)(((sum & 1) ? 0x8000 : 0) + (sum >> 1) + set[i]);
	}
	return (sum);
}

/* The up-case table's checksum: all its bytes. */
static inline uint32_t
exfat_table_checksum(const uint8_t *table, uint64_t length)
{
	uint32_t sum = 0;
	uint64_t i;

	for (i = 0; i < length; i++)
		sum = ((sum & 1) ? 0x80000000u : 0) + (sum >> 1) + table[i];
	return (sum);
}

/* A name's hash: of its up-cased characters, each as two bytes. */
static inline uint16_t
exfat_name_hash(const uint16_t *upcased, unsigned length)
{
	uint16_t hash = 0;
	unsigned i;

	for (i = 0; i < length; i++) {
		hash = (uint16_t)(((hash & 1) ? 0x8000 : 0) + (hash >> 1) +
		    (upcased[i] & 0xff));
		hash = (uint16_t)(((hash & 1) ? 0x8000 : 0) + (hash >> 1) +
		    (upcased[i] >> 8));
	}
	return (hash);
}

#endif /* !_EXFAT_FORMAT_H_ */
