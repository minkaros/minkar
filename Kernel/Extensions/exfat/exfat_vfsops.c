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
 * exfat: the volume operations, and the kext's start and stop, which
 * register and remove the "exfat" file system. A volume mounted
 * read-write is marked dirty on the disk until it is unmounted cleanly.
 */

#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include <sys/vnode_if.h>
#include <sys/ubc.h>
#include <mach/kmod.h>
#include <libkern/OSByteOrder.h>

#include "exfat_fs.h"

#define le16(x)	OSSwapLittleToHostInt16(x)
#define le32(x)	OSSwapLittleToHostInt32(x)
#define le64(x)	OSSwapLittleToHostInt64(x)

#define EXFAT_IOSIZE	(128 * 1024)
#define UNKNOWNID	99

OSMallocTag exfat_tag;
lck_grp_t *exfat_lck_grp;
static vfstable_t exfat_vfsconf;

kern_return_t exfat_module_start(kmod_info_t *, void *);
kern_return_t exfat_module_stop(kmod_info_t *, void *);

/* The boot sector says it is exFAT, and its numbers make sense. */
static int
exfat_boot_ok(const struct exfat_boot *b)
{
	uint32_t fat_offset = le32(b->fat_offset), fat_length =
	    le32(b->fat_length), heap = le32(b->heap_offset),
	    count = le32(b->cluster_count), root = le32(b->root_cluster);

	if (memcmp(b->fs_name, EXFAT_NAME, 8) != 0 ||
	    le16(b->signature) != 0xaa55)
		return (0);
	if (b->sector_shift < 9 || b->sector_shift > 12 ||
	    b->cluster_shift > 25 - b->sector_shift)
		return (0);
	if (b->nfats < 1 || b->nfats > 2 || (le16(b->revision) >> 8) != 1)
		return (0);
	if (fat_offset < 24 || fat_length == 0 ||
	    heap < fat_offset + (uint64_t)fat_length * b->nfats)
		return (0);
	if (count == 0 || (uint64_t)fat_length << b->sector_shift <
	    ((uint64_t)count + EXFAT_FIRST_CLUSTER) * 4)
		return (0);
	if (root < EXFAT_FIRST_CLUSTER ||
	    root >= (uint64_t)count + EXFAT_FIRST_CLUSTER)
		return (0);
	if (heap + ((uint64_t)count << b->cluster_shift) >
	    le64(b->volume_length))
		return (0);
	return (1);
}

/* The boot region's checksum, in sector 11, over sectors 0 to 10. */
static int
exfat_boot_checksum_ok(struct exfat_mount *xmp)
{
	uint32_t size = xmp->sector_size * EXFAT_CHECKSUM_SECTOR, sum, want;
	uint8_t *region, *data;
	unsigned i;
	buf_t bp;
	int error;

	region = OSMalloc(size, exfat_tag);
	if (region == NULL)
		return (ENOMEM);
	for (i = 0; i < EXFAT_CHECKSUM_SECTOR; i++) {
		if ((error = exfat_bread(xmp, i, &bp, &data)))
			goto out;
		memcpy(region + i * xmp->sector_size, data, xmp->sector_size);
		buf_brelse(bp);
	}
	sum = exfat_boot_checksum(region, xmp->sector_size);
	if ((error = exfat_bread(xmp, EXFAT_CHECKSUM_SECTOR, &bp, &data)))
		goto out;
	memcpy(&want, data, sizeof(want));
	buf_brelse(bp);
	error = le32(want) == sum ? 0 : EINVAL;
out:
	OSFree(region, size, exfat_tag);
	return (error);
}

/* Free clusters: the bitmap's clear bits, one per cluster of the heap. */
static int
exfat_count_free(struct exfat_mount *xmp, uint32_t first, uint64_t length)
{
	struct exfat_stream st;
	uint8_t chunk[512];
	uint64_t bits = xmp->cluster_count, off = 0, used = 0;
	size_t n, i;
	int error;

	if (first == 0 || length * 8 < bits)
		return (EINVAL);
	st.first = first;
	st.contig = 0;
	st.length = length;
	st.hint_index = st.hint_cluster = 0;
	while (off * 8 < bits) {
		n = (size_t)MIN(sizeof(chunk), (bits + 7) / 8 - off);
		if ((error = exfat_read_stream(xmp, &st, off, chunk, n)))
			return (error);
		for (i = 0; i < n; i++) {
			uint8_t v = chunk[i];

			if ((off + i + 1) * 8 > bits)
				v &= (uint8_t)((1u << (bits % 8)) - 1);
			used += (uint64_t)__builtin_popcount(v);
		}
		off += n;
	}
	xmp->free_clusters = bits - used;
	return (0);
}

/* The root directory's system entries: bitmap, up-case table, label. */
static int
exfat_read_root(struct exfat_mount *xmp)
{
	struct exfat_dcursor c;
	const uint8_t *ent;
	uint32_t bm_first = 0, up_first = 0, up_sum = 0;
	uint64_t bm_length = 0, up_length = 0, index;
	uint16_t label[EXFAT_LABEL_MAX];
	unsigned label_len = 0, i;
	size_t len;
	int error = 0;

	exfat_dcursor_init(&c, xmp, &xmp->root);
	for (index = 0;; index++) {
		if ((error = exfat_dentry(&c, index, &ent))) {
			if (error == ENOENT)
				error = 0;
			break;
		}
		if (ent[0] == EXFAT_TYPE_END)
			break;
		if (ent[0] == EXFAT_TYPE_BITMAP && bm_first == 0) {
			const struct exfat_bitmap_entry *b = (const void *)ent;

			if ((b->flags & 1) == 0) {
				bm_first = le32(b->first_cluster);
				bm_length = le64(b->length);
			}
		} else if (ent[0] == EXFAT_TYPE_UPCASE && up_first == 0) {
			const struct exfat_upcase_entry *u = (const void *)ent;

			up_first = le32(u->first_cluster);
			up_length = le64(u->length);
			up_sum = le32(u->checksum);
		} else if (ent[0] == EXFAT_TYPE_LABEL && label_len == 0) {
			const struct exfat_label_entry *l = (const void *)ent;

			label_len = MIN(l->length, EXFAT_LABEL_MAX);
			for (i = 0; i < label_len; i++)
				label[i] = le16(l->label[i]);
		}
	}
	exfat_dcursor_done(&c);
	if (error)
		return (error);

	if (exfat_load_upcase(xmp, up_first, up_length, up_sum) != 0)
		printf("exfat: no usable up-case table; only ASCII letters "
		    "fold\n");
	xmp->bitmap.first = bm_first;
	xmp->bitmap.contig = 0;
	xmp->bitmap.length = bm_length;
	if (exfat_count_free(xmp, bm_first, bm_length) != 0) {
		printf("exfat: no usable allocation bitmap\n");
		xmp->free_clusters = 0;
	}
	xmp->label[0] = '\0';
	if (label_len > 0 && exfat_name_to_utf8(label, (uint16_t)label_len,
	    xmp->label, sizeof(xmp->label), &len) != 0)
		xmp->label[0] = '\0';
	return (0);
}

static void
exfat_free_mount(struct exfat_mount *xmp)
{
	struct exfat_dirrec *rec;
	unsigned i;

	for (i = 0; i < EXFAT_HASH; i++) {
		while ((rec = LIST_FIRST(&xmp->dirs[i])) != NULL) {
			LIST_REMOVE(rec, link);
			if (rec->name != NULL)
				OSFree(rec->name, rec->namesize, exfat_tag);
			OSFree(rec, sizeof(*rec), exfat_tag);
		}
	}
	if (xmp->upcase != NULL)
		OSFree(xmp->upcase, EXFAT_UPCASE_CHARS * 2, exfat_tag);
	if (xmp->lock != NULL)
		lck_mtx_free(xmp->lock, exfat_lck_grp);
	if (xmp->meta != NULL)
		lck_rw_free(xmp->meta, exfat_lck_grp);
	OSFree(xmp, sizeof(*xmp), exfat_tag);
}

static int
exfat_mountfs(mount_t mp, vnode_t devvp, const struct exfat_args_fs *args,
    vfs_context_t ctx)
{
	struct exfat_mount *xmp = NULL;
	struct exfat_boot boot;
	uint32_t devbsize, ssize, root_clusters;
	buf_t bp = NULL;
	unsigned i;
	int error;

	if ((error = VNOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, (caddr_t)&devbsize,
	    0, ctx)))
		return (error);
	if (devbsize < sizeof(boot) || devbsize > 4096)
		return (EINVAL);
	if ((error = buf_meta_bread(devvp, 0, (int)devbsize, NOCRED, &bp))) {
		if (bp != NULL)
			buf_brelse(bp);
		return (error);
	}
	memcpy(&boot, (const void *)buf_dataptr(bp), sizeof(boot));
	buf_markinvalid(bp);
	buf_brelse(bp);
	if (!exfat_boot_ok(&boot))
		return (EINVAL);

	/* The device's blocks become the volume's sectors. */
	ssize = 1u << boot.sector_shift;
	if (ssize < devbsize)
		return (EINVAL);
	if (ssize != devbsize) {
		if ((error = VNOP_IOCTL(devvp, DKIOCSETBLOCKSIZE,
		    (caddr_t)&ssize, FWRITE, ctx)))
			return (EINVAL);
		buf_invalidateblks(devvp, 0, 0, 0);
	}

	xmp = OSMalloc(sizeof(*xmp), exfat_tag);
	if (xmp == NULL) {
		error = ENOMEM;
		goto fail;
	}
	bzero(xmp, sizeof(*xmp));
	xmp->mp = mp;
	xmp->devvp = devvp;
	xmp->old_blocksize = ssize != devbsize ? devbsize : 0;
	xmp->sector_size = ssize;
	xmp->sector_shift = boot.sector_shift;
	xmp->csector_shift = boot.cluster_shift;
	xmp->cluster_shift = boot.sector_shift + boot.cluster_shift;
	xmp->cluster_size = 1ull << xmp->cluster_shift;
	xmp->fat_offset = le32(boot.fat_offset);
	if (boot.nfats == 2 && (le16(boot.flags) & EXFAT_ACTIVE_FAT))
		xmp->fat_offset += le32(boot.fat_length);
	xmp->heap_offset = le32(boot.heap_offset);
	xmp->cluster_count = le32(boot.cluster_count);
	xmp->volume_sectors = le64(boot.volume_length);
	xmp->serial = le32(boot.serial);
	xmp->uid = (args->flags & EXFAT_MNT_UID) ? args->uid : UNKNOWNID;
	xmp->gid = (args->flags & EXFAT_MNT_GID) ? args->gid : UNKNOWNID;
	xmp->perms = (args->flags & EXFAT_MNT_PERMS) ?
	    (args->perms & ACCESSPERMS) : 0755;
	xmp->seconds_west = (args->flags & EXFAT_MNT_SECONDSWEST) ?
	    args->seconds_west : 0;
	xmp->lock = lck_mtx_alloc_init(exfat_lck_grp, LCK_ATTR_NULL);
	xmp->meta = lck_rw_alloc_init(exfat_lck_grp, LCK_ATTR_NULL);
	for (i = 0; i < EXFAT_HASH; i++) {
		LIST_INIT(&xmp->nodes[i]);
		LIST_INIT(&xmp->dirs[i]);
	}
	xmp->upcase = OSMalloc(EXFAT_UPCASE_CHARS * 2, exfat_tag);
	if (xmp->lock == NULL || xmp->meta == NULL || xmp->upcase == NULL) {
		error = ENOMEM;
		goto fail;
	}

	if ((error = exfat_boot_checksum_ok(xmp))) {
		printf("exfat: the boot region's checksum does not match\n");
		goto fail;
	}
	if ((error = exfat_chain_length(xmp, le32(boot.root_cluster),
	    &root_clusters)))
		goto fail;
	xmp->root.id = EXFAT_ROOT_ID;
	xmp->root.parent_id = EXFAT_ROOT_PARENT_ID;
	xmp->root.attr = EXFAT_ATTR_DIR;
	xmp->root.first = le32(boot.root_cluster);
	xmp->root.length = xmp->root.valid =
	    (uint64_t)root_clusters << xmp->cluster_shift;
	if ((error = exfat_read_root(xmp)))
		goto fail;

	if (!vfs_isrdonly(mp)) {
		/* Two FATs (TexFAT) or no bitmap: read only. */
		if (boot.nfats != 1 || xmp->bitmap.first == 0) {
			error = EROFS;
			goto fail;
		}
		if ((error = exfat_set_dirty(xmp, 1)))
			goto fail;
		xmp->rw = 1;
	}
	vfs_setfsprivate(mp, xmp);
	vfs_getnewfsid(mp);
	vfs_setlocklocal(mp);
	return (0);

fail:
	if (xmp != NULL)
		exfat_free_mount(xmp);
	if (ssize != devbsize) {
		buf_invalidateblks(devvp, 0, 0, 0);
		(void)VNOP_IOCTL(devvp, DKIOCSETBLOCKSIZE, (caddr_t)&devbsize,
		    FWRITE, ctx);
	}
	return (error ? error : EINVAL);
}

static int
exfat_vfs_mount(mount_t mp, vnode_t devvp, user_addr_t data,
    vfs_context_t ctx)
{
	struct exfat_args_fs args;
	int error;

	if (vfs_isupdate(mp)) {
		struct exfat_mount *xmp = VFSTOEXFAT(mp);

		if (vfs_iswriteupgrade(mp) && !xmp->rw) {
			if (xmp->bitmap.first == 0)
				return (EROFS);
			if ((error = exfat_set_dirty(xmp, 1)))
				return (error);
			xmp->rw = 1;
		} else if (vfs_isrdonly(mp) && xmp->rw) {
			error = vflush(mp, NULLVP,
			    (vfs_flags(mp) & MNT_FORCE) ? FORCECLOSE : WRITECLOSE);
			if (error)
				return (error);
			buf_flushdirtyblks(xmp->devvp, 1, 0, "exfat_ro");
			xmp->rw = 0;
			(void)exfat_set_dirty(xmp, 0);
		}
		return (0);
	}
	if ((error = copyin(data, &args, sizeof(args))))
		return (error);
	if (args.version != EXFAT_ARGS_VERSION)
		return (EINVAL);
	return (exfat_mountfs(mp, devvp, &args, ctx));
}

static int
exfat_vfs_start(mount_t mp, int flags, vfs_context_t ctx)
{
#pragma unused(mp, flags, ctx)
	return (0);
}

static int
exfat_vfs_unmount(mount_t mp, int mntflags, vfs_context_t ctx)
{
	struct exfat_mount *xmp = VFSTOEXFAT(mp);
	vnode_t devvp = xmp->devvp;
	uint32_t old = xmp->old_blocksize;
	int error;

	error = vflush(mp, NULLVP, (mntflags & MNT_FORCE) ? FORCECLOSE : 0);
	if (error)
		return (error);
	if (xmp->rw) {
		/* Everything on the disk, then the volume is clean. */
		buf_flushdirtyblks(devvp, 1, 0, "exfat_unmount");
		(void)exfat_set_dirty(xmp, 0);
		(void)VNOP_FSYNC(devvp, MNT_WAIT, ctx);
	}
	buf_invalidateblks(devvp, 0, 0, 0);
	if (old != 0)
		(void)VNOP_IOCTL(devvp, DKIOCSETBLOCKSIZE, (caddr_t)&old, FWRITE,
		    ctx);
	vfs_setfsprivate(mp, NULL);
	exfat_free_mount(xmp);
	return (0);
}

static int
exfat_vfs_root(mount_t mp, vnode_t *vpp, vfs_context_t ctx)
{
#pragma unused(ctx)
	struct exfat_mount *xmp = VFSTOEXFAT(mp);

	return (exfat_getnode(xmp, &xmp->root, NULL, NULL, NULL, vpp));
}

static int
exfat_vfs_getattr(mount_t mp, struct vfs_attr *fsap, vfs_context_t ctx)
{
#pragma unused(ctx)
	struct exfat_mount *xmp = VFSTOEXFAT(mp);

	VFSATTR_RETURN(fsap, f_bsize, (uint32_t)xmp->cluster_size);
	VFSATTR_RETURN(fsap, f_iosize, EXFAT_IOSIZE);
	VFSATTR_RETURN(fsap, f_blocks, xmp->cluster_count);
	VFSATTR_RETURN(fsap, f_bfree, xmp->free_clusters);
	VFSATTR_RETURN(fsap, f_bavail, xmp->free_clusters);
	VFSATTR_RETURN(fsap, f_bused, xmp->cluster_count - xmp->free_clusters);
	VFSATTR_RETURN(fsap, f_fssubtype, 0);
	if (VFSATTR_IS_ACTIVE(fsap, f_vol_name)) {
		strlcpy(fsap->f_vol_name, xmp->label, MAXPATHLEN);
		VFSATTR_SET_SUPPORTED(fsap, f_vol_name);
	}
	if (VFSATTR_IS_ACTIVE(fsap, f_capabilities)) {
		vol_capabilities_attr_t *cap = &fsap->f_capabilities;

		cap->capabilities[VOL_CAPABILITIES_FORMAT] =
		    VOL_CAP_FMT_NO_ROOT_TIMES | VOL_CAP_FMT_CASE_PRESERVING |
		    VOL_CAP_FMT_FAST_STATFS | VOL_CAP_FMT_2TB_FILESIZE |
		    VOL_CAP_FMT_HIDDEN_FILES | VOL_CAP_FMT_PATH_FROM_ID |
		    VOL_CAP_FMT_64BIT_OBJECT_IDS;
		cap->capabilities[VOL_CAPABILITIES_INTERFACES] = 0;
		cap->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
		cap->valid[VOL_CAPABILITIES_FORMAT] =
		    VOL_CAP_FMT_PERSISTENTOBJECTIDS | VOL_CAP_FMT_SYMBOLICLINKS |
		    VOL_CAP_FMT_HARDLINKS | VOL_CAP_FMT_JOURNAL |
		    VOL_CAP_FMT_JOURNAL_ACTIVE | VOL_CAP_FMT_NO_ROOT_TIMES |
		    VOL_CAP_FMT_SPARSE_FILES | VOL_CAP_FMT_ZERO_RUNS |
		    VOL_CAP_FMT_CASE_SENSITIVE | VOL_CAP_FMT_CASE_PRESERVING |
		    VOL_CAP_FMT_FAST_STATFS | VOL_CAP_FMT_2TB_FILESIZE |
		    VOL_CAP_FMT_OPENDENYMODES | VOL_CAP_FMT_HIDDEN_FILES |
		    VOL_CAP_FMT_PATH_FROM_ID | VOL_CAP_FMT_64BIT_OBJECT_IDS;
		cap->valid[VOL_CAPABILITIES_INTERFACES] =
		    VOL_CAP_INT_SEARCHFS | VOL_CAP_INT_ATTRLIST |
		    VOL_CAP_INT_NFSEXPORT | VOL_CAP_INT_READDIRATTR |
		    VOL_CAP_INT_EXCHANGEDATA | VOL_CAP_INT_COPYFILE |
		    VOL_CAP_INT_ALLOCATE | VOL_CAP_INT_VOL_RENAME |
		    VOL_CAP_INT_ADVLOCK | VOL_CAP_INT_FLOCK |
		    VOL_CAP_INT_EXTENDED_SECURITY | VOL_CAP_INT_USERACCESS |
		    VOL_CAP_INT_MANLOCK | VOL_CAP_INT_EXTENDED_ATTR |
		    VOL_CAP_INT_NAMEDSTREAMS;
		cap->valid[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(fsap, f_capabilities);
	}
	return (0);
}

static int
exfat_sync_vnode(vnode_t vp, void *arg)
{
	int wait = *(int *)arg;

	if (vnode_isreg(vp))
		(void)cluster_push(vp, wait ? IO_SYNC : 0);
	return (VNODE_RETURNED);
}

/* Files' data, then the FAT, bitmap and directories. */
static int
exfat_vfs_sync(mount_t mp, int waitfor, vfs_context_t ctx)
{
#pragma unused(ctx)
	struct exfat_mount *xmp = VFSTOEXFAT(mp);
	int wait = waitfor == MNT_WAIT;

	if (xmp == NULL || !xmp->rw)
		return (0);
	(void)vnode_iterate(mp, 0, exfat_sync_vnode, &wait);
	buf_flushdirtyblks(xmp->devvp, wait, 0, "exfat_sync");
	return (0);
}

/* A directory by its file number (the kernel builds paths with it). */
static int
exfat_vfs_vget(mount_t mp, ino64_t ino, vnode_t *vpp, vfs_context_t ctx)
{
#pragma unused(ctx)
	struct exfat_mount *xmp = VFSTOEXFAT(mp);
	struct exfat_dirrec *rec;
	struct exfat_info info;

	if (ino == EXFAT_ROOT_ID)
		return (exfat_getnode(xmp, &xmp->root, NULL, NULL, NULL, vpp));
	lck_mtx_lock(xmp->lock);
	rec = exfat_dirrec_find(xmp, (uint64_t)ino);
	if (rec != NULL)
		info = rec->info;
	lck_mtx_unlock(xmp->lock);
	if (rec == NULL)
		return (ENOENT);
	return (exfat_getnode(xmp, &info, NULL, NULL, NULL, vpp));
}

static struct vfsops exfat_vfsops = {
	.vfs_mount	= exfat_vfs_mount,
	.vfs_start	= exfat_vfs_start,
	.vfs_unmount	= exfat_vfs_unmount,
	.vfs_root	= exfat_vfs_root,
	.vfs_getattr	= exfat_vfs_getattr,
	.vfs_sync	= exfat_vfs_sync,
	.vfs_vget	= exfat_vfs_vget,
};

static struct vnodeopv_desc *exfat_opv_descs[] = {
	&exfat_vnodeop_opv_desc,
	NULL
};

kern_return_t
exfat_module_start(kmod_info_t *ki, void *data)
{
#pragma unused(ki, data)
	struct vfs_fsentry vfe;

	exfat_tag = OSMalloc_Tagalloc("exfat", OSMT_DEFAULT);
	exfat_lck_grp = lck_grp_alloc_init("exfat", LCK_GRP_ATTR_NULL);
	bzero(&vfe, sizeof(vfe));
	vfe.vfe_vfsops = &exfat_vfsops;
	vfe.vfe_vopcnt = 1;
	vfe.vfe_opvdescs = exfat_opv_descs;
	strlcpy(vfe.vfe_fsname, "exfat", sizeof(vfe.vfe_fsname));
	vfe.vfe_flags = VFS_TBLTHREADSAFE | VFS_TBLNOTYPENUM | VFS_TBLLOCALVOL |
	    VFS_TBL64BITREADY | VFS_TBLREADDIR_EXTENDED;
	if (vfs_fsadd(&vfe, &exfat_vfsconf) != 0) {
		lck_grp_free(exfat_lck_grp);
		OSMalloc_Tagfree(exfat_tag);
		return (KERN_FAILURE);
	}
	return (KERN_SUCCESS);
}

kern_return_t
exfat_module_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki, data)
	if (vfs_fsremove(exfat_vfsconf) != 0)
		return (KERN_FAILURE);
	lck_grp_free(exfat_lck_grp);
	OSMalloc_Tagfree(exfat_tag);
	return (KERN_SUCCESS);
}
