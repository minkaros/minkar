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
 * The exfat kext's own structures: the mounted volume, its files and
 * directories, and the routines the vfs and vnode operations share.
 */

#ifndef _EXFAT_FS_H_
#define _EXFAT_FS_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <kern/locks.h>
#include <libkern/OSMalloc.h>

#include "exfat_format.h"
#include "exfat_mount.h"

#define EXFAT_ROOT_ID		2	/* the root directory's file number */
#define EXFAT_ROOT_PARENT_ID	1	/* and its parent's, as on HFS */
#define EXFAT_HASH		256	/* buckets of the node table */
#define EXFAT_SET_MAX		19	/* a file and up to 18 secondaries */
#define EXFAT_UPCASE_CHARS	65536

/* Where a file's data lies: a FAT chain, or contiguous clusters. */
struct exfat_stream {
	uint32_t	first;		/* its first cluster; 0 if empty */
	uint8_t		contig;		/* NoFatChain: no FAT to follow */
	uint64_t	length;		/* allocated length, in bytes */
	uint32_t	hint_index;	/* the last cluster mapped, by index */
	uint32_t	hint_cluster;
};

/* A file or directory, as its entry set describes it. */
struct exfat_info {
	uint64_t	id;		/* its file number */
	uint64_t	parent_id;
	uint16_t	attr;
	uint32_t	ctime, mtime, atime;
	uint8_t		c10ms, m10ms;
	uint8_t		cutc, mutc, autc;
	uint32_t	first;
	uint8_t		contig;
	uint64_t	valid;		/* bytes written; zeros after them */
	uint64_t	length;		/* the file's size */
	uint32_t	count;		/* entries in its set; 0: the root */
	uint64_t	pos[EXFAT_SET_MAX];	/* each one's byte on the device */
};

/* A parsed entry set of a directory. */
struct exfat_entry {
	struct exfat_info info;
	uint64_t	index;		/* the file entry's index in the directory */
	uint32_t	count;		/* the set's entries */
	uint16_t	hash;
	uint16_t	namelen;	/* in UTF-16 characters */
	uint16_t	name[EXFAT_NAME_MAX];
};

struct exfat_node;
struct exfat_dirrec;

struct exfat_mount {
	mount_t		mp;
	vnode_t		devvp;
	uint32_t	sector_size;
	uint32_t	sector_shift;
	uint32_t	csector_shift;	/* sectors per cluster, as a shift */
	uint32_t	cluster_shift;	/* bytes per cluster, as a shift */
	uint64_t	cluster_size;
	uint32_t	fat_offset;	/* in sectors */
	uint32_t	heap_offset;	/* in sectors */
	uint32_t	cluster_count;
	uint64_t	volume_sectors;
	uint32_t	serial;
	uint64_t	free_clusters;
	uint32_t	alloc_hint;	/* where to look for a free cluster */
	struct exfat_stream bitmap;	/* the allocation bitmap */
	int		rw;		/* mounted read-write */
	lck_rw_t	*meta;		/* directories, FAT and bitmap changes */
	uint16_t	*upcase;	/* EXFAT_UPCASE_CHARS entries */
	char		label[EXFAT_LABEL_MAX * 3 + 1];
	uid_t		uid;
	gid_t		gid;
	mode_t		perms;
	int32_t		seconds_west;
	uint32_t	old_blocksize;	/* the device's, restored at unmount */
	struct exfat_info root;
	lck_mtx_t	*lock;		/* the tables below */
	LIST_HEAD(, exfat_node) nodes[EXFAT_HASH];
	LIST_HEAD(, exfat_dirrec) dirs[EXFAT_HASH];
};

/* A directory's description, kept while mounted to find ".." again. */
struct exfat_dirrec {
	LIST_ENTRY(exfat_dirrec) link;
	struct exfat_info info;
	char		*name;		/* its name on the disk, in UTF-8 */
	uint32_t	namesize;
};

#define EXFAT_NODE_ATTACHING	0x01	/* its vnode is being made */
#define EXFAT_NODE_WANTED	0x02	/* someone waits for that */
#define EXFAT_NODE_DELETED	0x04	/* its entries are gone */
#define EXFAT_NODE_FREED	0x08	/* and so are its clusters */
#define EXFAT_NODE_HASHED	0x10	/* in the node table */

struct exfat_node {
	LIST_ENTRY(exfat_node) link;
	vnode_t		vp;
	struct exfat_mount *xmp;
	uint32_t	state;
	struct exfat_info info;
	lck_mtx_t	*lock;		/* info and stream */
	lck_rw_t	*iolock;	/* writes and size changes */
	struct exfat_stream stream;
};

/* Reads of a directory's entries, one sector buffered. */
struct exfat_dcursor {
	struct exfat_mount *xmp;
	struct exfat_stream stream;
	buf_t		bp;
	uint64_t	sector;		/* the buffered one */
	uint64_t	last_byte;	/* the device byte of the last entry read */
};

#define VFSTOEXFAT(mp)	((struct exfat_mount *)vfs_fsprivate(mp))
#define VTOEXFAT(vp)	((struct exfat_node *)vnode_fsnode(vp))

extern OSMallocTag exfat_tag;
extern lck_grp_t *exfat_lck_grp;
extern int (**exfat_vnodeop_p)(void *);
extern struct vnodeopv_desc exfat_vnodeop_opv_desc;

/* exfat_subr.c */
int	exfat_fat_next(struct exfat_mount *, uint32_t, uint32_t *);
int	exfat_bread(struct exfat_mount *, uint64_t, buf_t *, uint8_t **);
int	exfat_map(struct exfat_mount *, struct exfat_stream *, uint32_t,
	    uint32_t *, uint32_t *);
int	exfat_read_stream(struct exfat_mount *, struct exfat_stream *,
	    uint64_t, void *, size_t);
uint64_t exfat_cluster_sector(struct exfat_mount *, uint32_t);
int	exfat_chain_length(struct exfat_mount *, uint32_t, uint32_t *);
void	exfat_stream_init(struct exfat_stream *, const struct exfat_info *);
void	exfat_dcursor_init(struct exfat_dcursor *, struct exfat_mount *,
	    const struct exfat_info *);
void	exfat_dcursor_done(struct exfat_dcursor *);
int	exfat_dentry(struct exfat_dcursor *, uint64_t, const uint8_t **);
int	exfat_dir_next(struct exfat_dcursor *, uint64_t *, uint64_t,
	    struct exfat_entry *);
int	exfat_dir_find(struct exfat_mount *, const struct exfat_info *,
	    const uint16_t *, uint16_t, struct exfat_entry *);
uint16_t exfat_upcase(struct exfat_mount *, uint16_t);
int	exfat_load_upcase(struct exfat_mount *, uint32_t, uint64_t, uint32_t);
void	exfat_time(struct exfat_mount *, uint32_t, uint8_t, uint8_t,
	    struct timespec *);
int	exfat_name_to_utf8(const uint16_t *, uint16_t, char *, size_t,
	    size_t *);

/* exfat_write.c; with the mount's meta lock held exclusive */
void	exfat_encode_time(struct exfat_mount *, const struct timespec *,
	    uint32_t *, uint8_t *, uint8_t *);
void	exfat_now(struct exfat_mount *, uint32_t *, uint8_t *, uint8_t *);
int	exfat_grow(struct exfat_mount *, struct exfat_stream *, uint64_t);
int	exfat_shrink(struct exfat_mount *, struct exfat_stream *, uint64_t,
	    int);
int	exfat_zero_cluster(struct exfat_mount *, uint32_t);
int	exfat_name_from_utf8(const char *, size_t, uint16_t *, uint16_t *);
unsigned exfat_build_set(struct exfat_mount *, const struct exfat_info *,
	    const uint16_t *, uint16_t, uint8_t *);
int	exfat_write_set(struct exfat_mount *, const uint64_t *, unsigned,
	    const uint8_t *);
int	exfat_read_set(struct exfat_mount *, const uint64_t *, unsigned,
	    uint8_t *);
int	exfat_update_set(struct exfat_mount *, const struct exfat_info *);
int	exfat_delete_set(struct exfat_mount *, const uint64_t *, unsigned);
int	exfat_dir_space(struct exfat_mount *, struct exfat_node *, unsigned,
	    uint64_t *);
int	exfat_dir_empty(struct exfat_mount *, const struct exfat_info *);
int	exfat_set_dirty(struct exfat_mount *, int);

/* exfat_vnops.c */
int	exfat_getnode(struct exfat_mount *, const struct exfat_info *,
	    vnode_t, struct componentname *, const char *, vnode_t *);
struct exfat_dirrec *exfat_dirrec_find(struct exfat_mount *, uint64_t);
void	exfat_node_changed(struct exfat_mount *, struct exfat_node *);

#endif /* !_EXFAT_FS_H_ */
