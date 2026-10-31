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
 * exfat: the vnode operations of a read-only exFAT volume. A file's data
 * goes through the unified buffer cache (cluster_read, cluster_pagein),
 * mapped to the disk by exfat_vnop_blockmap; bytes past a file's valid
 * length read as zeros (holes). Names are looked up without regard to
 * case, with the volume's up-case table.
 */

#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ubc.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/utfconv.h>
#include <sys/vnode_if.h>

#include "exfat_fs.h"

#define VOPFUNC			int (*)(void *)
#define EXFAT_IOSIZE		(128 * 1024)
#define EXFAT_HASHOF(id)	((id) % EXFAT_HASH)
#define EXT_DIRENT_LEN(n)	\
	((offsetof(struct direntry, d_name) + (n) + 1 + 7) & ~(size_t)7)
#define STD_DIRENT_LEN(n)	\
	((offsetof(struct dirent, d_name) + (n) + 1 + 3) & ~(size_t)3)

int (**exfat_vnodeop_p)(void *);

/* Called with the mount's lock held. */
struct exfat_dirrec *
exfat_dirrec_find(struct exfat_mount *xmp, uint64_t id)
{
	struct exfat_dirrec *rec;

	LIST_FOREACH(rec, &xmp->dirs[EXFAT_HASHOF(id)], link)
		if (rec->info.id == id)
			return (rec);
	return (NULL);
}

static void
exfat_dirrec_add(struct exfat_mount *xmp, const struct exfat_info *info,
    const char *name)
{
	struct exfat_dirrec *rec;

	rec = exfat_dirrec_find(xmp, info->id);
	if (rec == NULL) {
		rec = OSMalloc(sizeof(*rec), exfat_tag);
		if (rec == NULL)
			return;
		rec->info = *info;
		rec->name = NULL;
		rec->namesize = 0;
		LIST_INSERT_HEAD(&xmp->dirs[EXFAT_HASHOF(info->id)], rec, link);
	}
	if (rec->name == NULL && name != NULL) {
		rec->namesize = (uint32_t)strlen(name) + 1;
		rec->name = OSMalloc(rec->namesize, exfat_tag);
		if (rec->name != NULL)
			strlcpy(rec->name, name, rec->namesize);
		else
			rec->namesize = 0;
	}
}

/*
 * A directory's vnode made without a lookup (from "..", or by its file
 * number) gets its name from its record, so the kernel can build its
 * path (getcwd); its parent comes by number (VOL_CAP_FMT_PATH_FROM_ID).
 */
static void
exfat_name_vnode(struct exfat_mount *xmp, vnode_t vp, uint64_t id)
{
	struct exfat_dirrec *rec;
	const char *had;
	char *name = NULL;
	uint32_t size = 0;

	if ((had = vnode_getname(vp)) != NULL) {
		vnode_putname(had);
		return;
	}
	lck_mtx_lock(xmp->lock);
	rec = exfat_dirrec_find(xmp, id);
	if (rec != NULL && rec->name != NULL) {
		size = rec->namesize;
		name = OSMalloc(size, exfat_tag);
		if (name != NULL)
			memcpy(name, rec->name, size);
	}
	lck_mtx_unlock(xmp->lock);
	if (name == NULL)
		return;
	vnode_update_identity(vp, NULLVP, name, (int)strlen(name), 0,
	    VNODE_UPDATE_NAME);
	OSFree(name, size, exfat_tag);
}

/*
 * The vnode of a file or directory: the one in use, or a new one. While
 * one is being made its node is in the table, marked, and others wait.
 */
int
exfat_getnode(struct exfat_mount *xmp, const struct exfat_info *info,
    vnode_t dvp, struct componentname *cnp, const char *name, vnode_t *vpp)
{
	struct vnode_fsparam vfsp;
	struct exfat_node *np;
	struct timespec pause = { 0, 1000000 };
	int dir = (info->attr & EXFAT_ATTR_DIR) != 0;
	uint32_t vid;
	vnode_t vp;
	int error;

	*vpp = NULL;
	lck_mtx_lock(xmp->lock);
again:
	LIST_FOREACH(np, &xmp->nodes[EXFAT_HASHOF(info->id)], link)
		if (np->info.id == info->id)
			break;
	if (np != NULL) {
		if (np->state & EXFAT_NODE_ATTACHING) {
			np->state |= EXFAT_NODE_WANTED;
			msleep(np, xmp->lock, PINOD, "exfat", NULL);
			goto again;
		}
		vp = np->vp;
		vid = vnode_vid(vp);
		lck_mtx_unlock(xmp->lock);
		if (vnode_getwithvid(vp, vid) == 0) {
			if (dir && cnp == NULL && info->id != EXFAT_ROOT_ID)
				exfat_name_vnode(xmp, vp, info->id);
			*vpp = vp;
			return (0);
		}
		/* Being reclaimed: its node leaves the table soon. */
		lck_mtx_lock(xmp->lock);
		msleep(&xmp->nodes, xmp->lock, PINOD, "exfatrc", &pause);
		goto again;
	}

	np = OSMalloc(sizeof(*np), exfat_tag);
	if (np == NULL) {
		lck_mtx_unlock(xmp->lock);
		return (ENOMEM);
	}
	bzero(np, sizeof(*np));
	np->xmp = xmp;
	np->info = *info;
	np->state = EXFAT_NODE_ATTACHING;
	np->lock = lck_mtx_alloc_init(exfat_lck_grp, LCK_ATTR_NULL);
	exfat_stream_init(&np->stream, info);
	LIST_INSERT_HEAD(&xmp->nodes[EXFAT_HASHOF(info->id)], np, link);
	if (dir)
		exfat_dirrec_add(xmp, info, name);
	lck_mtx_unlock(xmp->lock);

	bzero(&vfsp, sizeof(vfsp));
	vfsp.vnfs_mp = xmp->mp;
	vfsp.vnfs_vtype = dir ? VDIR : VREG;
	vfsp.vnfs_str = "exfat";
	vfsp.vnfs_dvp = dvp;
	vfsp.vnfs_fsnode = np;
	vfsp.vnfs_vops = exfat_vnodeop_p;
	vfsp.vnfs_markroot = info->id == EXFAT_ROOT_ID;
	vfsp.vnfs_filesize = (off_t)info->length;
	vfsp.vnfs_cnp = cnp;
	vfsp.vnfs_flags = VNFS_ADDFSREF;
	if (cnp == NULL || dvp == NULL || !(cnp->cn_flags & MAKEENTRY))
		vfsp.vnfs_flags |= VNFS_NOCACHE;
	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &vp);

	lck_mtx_lock(xmp->lock);
	if (error == 0) {
		np->vp = vp;
		np->state &= ~EXFAT_NODE_ATTACHING;
	} else
		LIST_REMOVE(np, link);
	if (np->state & EXFAT_NODE_WANTED) {
		np->state &= ~EXFAT_NODE_WANTED;
		wakeup(np);
	}
	lck_mtx_unlock(xmp->lock);
	if (error) {
		lck_mtx_free(np->lock, exfat_lck_grp);
		OSFree(np, sizeof(*np), exfat_tag);
		return (error);
	}
	if (dir && cnp == NULL && info->id != EXFAT_ROOT_ID)
		exfat_name_vnode(xmp, vp, info->id);
	*vpp = vp;
	return (0);
}

static int
exfat_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;

	lck_mtx_lock(xmp->lock);
	LIST_REMOVE(np, link);
	lck_mtx_unlock(xmp->lock);
	vnode_removefsref(vp);
	vnode_clearfsnode(vp);
	lck_mtx_free(np->lock, exfat_lck_grp);
	OSFree(np, sizeof(*np), exfat_tag);
	return (0);
}

static int
exfat_vnop_inactive(struct vnop_inactive_args *ap)
{
#pragma unused(ap)
	return (0);
}

/* A directory's parent: its own record says which one. */
static int
exfat_parent(struct exfat_mount *xmp, vnode_t dvp, vnode_t *vpp)
{
	struct exfat_node *dnp = VTOEXFAT(dvp);
	struct exfat_dirrec *rec;
	struct exfat_info info;

	if (dnp->info.id == EXFAT_ROOT_ID) {
		if (vnode_get(dvp))
			return (ENOENT);
		*vpp = dvp;
		return (0);
	}
	if (dnp->info.parent_id == EXFAT_ROOT_ID)
		info = xmp->root;
	else {
		lck_mtx_lock(xmp->lock);
		rec = exfat_dirrec_find(xmp, dnp->info.parent_id);
		if (rec != NULL)
			info = rec->info;
		lck_mtx_unlock(xmp->lock);
		if (rec == NULL)
			return (ENOENT);
	}
	return (exfat_getnode(xmp, &info, NULL, NULL, NULL, vpp));
}

static int
exfat_vnop_lookup(struct vnop_lookup_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct exfat_node *dnp = VTOEXFAT(dvp);
	struct exfat_mount *xmp = dnp->xmp;
	int last = (cnp->cn_flags & ISLASTCN) != 0;
	int op = cnp->cn_nameiop;
	uint16_t name[EXFAT_NAME_MAX];
	struct exfat_entry *e;
	size_t bytes;
	unsigned i;
	int error;

	*ap->a_vpp = NULL;
	if (!vnode_isdir(dvp))
		return (ENOTDIR);
	if (last && (op == DELETE || op == RENAME))
		return (EROFS);

	error = cache_lookup(dvp, ap->a_vpp, cnp);
	if (error == -1)
		return (0);
	if (error == ENOENT)
		return (last && op == CREATE ? EROFS : ENOENT);

	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		if (vnode_get(dvp))
			return (ENOENT);
		*ap->a_vpp = dvp;
		return (0);
	}
	if (cnp->cn_flags & ISDOTDOT)
		return (exfat_parent(xmp, dvp, ap->a_vpp));

	error = utf8_decodestr((const u_int8_t *)cnp->cn_nameptr,
	    (size_t)cnp->cn_namelen, name, &bytes, sizeof(name), 0,
	    UTF_PRECOMPOSED);
	if (error)
		return (error == ENAMETOOLONG ? ENAMETOOLONG : ENOENT);
	for (i = 0; i < bytes / 2; i++)
		name[i] = exfat_upcase(xmp, name[i]);

	e = OSMalloc(sizeof(*e), exfat_tag);
	if (e == NULL)
		return (ENOMEM);
	error = exfat_dir_find(xmp, &dnp->info, name, (uint16_t)(bytes / 2), e);
	if (error == 0 && (e->info.attr & EXFAT_ATTR_DIR)) {
		/* A directory's record keeps its name as the disk has it. */
		char *utf8 = OSMalloc(EXFAT_NAME_MAX * 3 + 1, exfat_tag);
		size_t len;

		if (utf8 != NULL && exfat_name_to_utf8(e->name, e->namelen,
		    utf8, EXFAT_NAME_MAX * 3 + 1, &len) != 0) {
			OSFree(utf8, EXFAT_NAME_MAX * 3 + 1, exfat_tag);
			utf8 = NULL;
		}
		error = exfat_getnode(xmp, &e->info, dvp, cnp, utf8, ap->a_vpp);
		if (utf8 != NULL)
			OSFree(utf8, EXFAT_NAME_MAX * 3 + 1, exfat_tag);
	} else if (error == 0)
		error = exfat_getnode(xmp, &e->info, dvp, cnp, NULL, ap->a_vpp);
	else if (error == ENOENT) {
		if ((cnp->cn_flags & MAKEENTRY) && op != CREATE)
			cache_enter(dvp, NULL, cnp);
		if (last && op == CREATE)
			error = EROFS;
	}
	OSFree(e, sizeof(*e), exfat_tag);
	return (error);
}

static int
exfat_vnop_open(struct vnop_open_args *ap)
{
	if ((ap->a_mode & FWRITE) != 0)
		return (EROFS);
	return (0);
}

static int
exfat_vnop_close(struct vnop_close_args *ap)
{
#pragma unused(ap)
	return (0);
}

static int
exfat_vnop_getattr(struct vnop_getattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;
	const struct exfat_info *info = &np->info;
	int dir = vnode_isdir(vp);
	uint64_t alloc;
	uint32_t flags = 0;

	alloc = (info->length + xmp->cluster_size - 1) &
	    ~(xmp->cluster_size - 1);
	VATTR_RETURN(vap, va_rdev, 0);
	VATTR_RETURN(vap, va_nlink, dir ? 2 : 1);
	VATTR_RETURN(vap, va_data_size, (off_t)info->length);
	VATTR_RETURN(vap, va_total_size, (off_t)info->length);
	VATTR_RETURN(vap, va_data_alloc, (off_t)alloc);
	VATTR_RETURN(vap, va_total_alloc, (off_t)alloc);
	VATTR_RETURN(vap, va_iosize, EXFAT_IOSIZE);
	VATTR_RETURN(vap, va_uid, xmp->uid);
	VATTR_RETURN(vap, va_gid, xmp->gid);
	VATTR_RETURN(vap, va_mode,
	    (dir ? S_IFDIR : S_IFREG) | xmp->perms);
	if (info->attr & EXFAT_ATTR_HIDDEN)
		flags |= UF_HIDDEN;
	if (!dir && (info->attr & EXFAT_ATTR_RDONLY))
		flags |= UF_IMMUTABLE;
	VATTR_RETURN(vap, va_flags, flags);
	if (VATTR_IS_ACTIVE(vap, va_create_time)) {
		exfat_time(xmp, info->ctime, info->c10ms, info->cutc,
		    &vap->va_create_time);
		VATTR_SET_SUPPORTED(vap, va_create_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
		exfat_time(xmp, info->mtime, info->m10ms, info->mutc,
		    &vap->va_modify_time);
		VATTR_SET_SUPPORTED(vap, va_modify_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_change_time)) {
		exfat_time(xmp, info->mtime, info->m10ms, info->mutc,
		    &vap->va_change_time);
		VATTR_SET_SUPPORTED(vap, va_change_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_access_time)) {
		exfat_time(xmp, info->atime, 0, info->autc, &vap->va_access_time);
		VATTR_SET_SUPPORTED(vap, va_access_time);
	}
	VATTR_RETURN(vap, va_fileid, info->id);
	VATTR_RETURN(vap, va_linkid, info->id);
	VATTR_RETURN(vap, va_parentid, info->parent_id);
	VATTR_RETURN(vap, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
	VATTR_RETURN(vap, va_filerev, 0);
	VATTR_RETURN(vap, va_gen, 0);
	return (0);
}

static int
exfat_vnop_setattr(struct vnop_setattr_args *ap)
{
#pragma unused(ap)
	return (EROFS);
}

static int
exfat_vnop_read(struct vnop_read_args *ap)
{
	vnode_t vp = ap->a_vp;

	if (vnode_isdir(vp))
		return (EISDIR);
	if (!vnode_isreg(vp))
		return (EPERM);
	if (uio_offset(ap->a_uio) < 0)
		return (EINVAL);
	return (cluster_read(vp, ap->a_uio, (off_t)VTOEXFAT(vp)->info.length,
	    ap->a_ioflag));
}

static int
exfat_vnop_write(struct vnop_write_args *ap)
{
#pragma unused(ap)
	return (EROFS);
}

static int
exfat_vnop_pagein(struct vnop_pagein_args *ap)
{
	vnode_t vp = ap->a_vp;

	return (cluster_pagein(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
	    (int)ap->a_size, (off_t)VTOEXFAT(vp)->info.length, ap->a_flags));
}

static int
exfat_vnop_pageout(struct vnop_pageout_args *ap)
{
	if (!(ap->a_flags & UPL_NOCOMMIT) && ap->a_pl != NULL)
		ubc_upl_abort_range(ap->a_pl, ap->a_pl_offset,
		    (upl_size_t)ap->a_size, UPL_ABORT_FREE_ON_EMPTY);
	return (EROFS);
}

static int
exfat_vnop_mmap(struct vnop_mmap_args *ap)
{
	if (ap->a_fflags & PROT_WRITE)
		return (EROFS);
	return (0);
}

/*
 * A file's offset on the disk, in the device's blocks (the volume's
 * sectors), and how many bytes from it are contiguous. Past the valid
 * length a file reads as zeros: a hole.
 */
static int
exfat_vnop_blockmap(struct vnop_blockmap_args *ap)
{
	struct exfat_node *np = VTOEXFAT(ap->a_vp);
	struct exfat_mount *xmp = np->xmp;
	uint64_t off = (uint64_t)ap->a_foffset, in, bytes, valid_end;
	uint32_t cluster, run;
	int error;

	if (ap->a_poff != NULL)
		*(int *)ap->a_poff = 0;
	if (ap->a_bpn == NULL)
		return (0);
	if (ap->a_foffset < 0)
		return (EINVAL);
	if (off >= np->info.valid) {
		*ap->a_bpn = -1;
		if (ap->a_run != NULL)
			*ap->a_run = ap->a_size;
		return (0);
	}

	lck_mtx_lock(np->lock);
	error = exfat_map(xmp, &np->stream,
	    (uint32_t)(off >> xmp->cluster_shift), &cluster, &run);
	lck_mtx_unlock(np->lock);
	if (error)
		return (error);

	in = off & (xmp->cluster_size - 1);
	*ap->a_bpn = (daddr64_t)(exfat_cluster_sector(xmp, cluster) +
	    (in >> xmp->sector_shift));
	bytes = ((uint64_t)run << xmp->cluster_shift) - in;
	valid_end = (np->info.valid + xmp->sector_size - 1) &
	    ~(uint64_t)(xmp->sector_size - 1);
	bytes = MIN(bytes, valid_end - off);
	if (ap->a_run != NULL)
		*ap->a_run = (size_t)MIN(bytes, ap->a_size);
	return (0);
}

static int
exfat_vnop_strategy(struct vnop_strategy_args *ap)
{
	vnode_t vp = buf_vnode(ap->a_bp);

	return (buf_strategy(VTOEXFAT(vp)->xmp->devvp, ap));
}

static int
exfat_vnop_blktooff(struct vnop_blktooff_args *ap)
{
	*ap->a_offset = (off_t)ap->a_lblkno * PAGE_SIZE_64;
	return (0);
}

static int
exfat_vnop_offtoblk(struct vnop_offtoblk_args *ap)
{
	*ap->a_lblkno = (daddr64_t)(ap->a_offset / PAGE_SIZE_64);
	return (0);
}

/*
 * One directory entry into the caller's buffer, in the extended form
 * (64-bit numbers, long names) or the old one. -1: no room for it.
 */
static int
exfat_emit(uio_t uio, int extended, void *buf, uint64_t id, uint8_t type,
    const char *name, size_t len, uint64_t next)
{
	size_t reclen;

	if (extended) {
		struct direntry *d = buf;

		reclen = EXT_DIRENT_LEN(len);
		d->d_ino = id;
		d->d_seekoff = next;
		d->d_reclen = (uint16_t)reclen;
		d->d_namlen = (uint16_t)len;
		d->d_type = type;
		if (name != d->d_name)
			memcpy(d->d_name, name, len);
		d->d_name[len] = '\0';
	} else {
		struct dirent *d = buf;

		reclen = STD_DIRENT_LEN(len);
		d->d_ino = (ino_t)id;
		d->d_reclen = (uint16_t)reclen;
		d->d_namlen = (uint8_t)len;
		d->d_type = type;
		if (name != d->d_name)
			memcpy(d->d_name, name, len);
		d->d_name[len] = '\0';
	}
	if (reclen > (size_t)uio_resid(uio))
		return (-1);
	return (uiomove(buf, (int)reclen, uio));
}

/*
 * A directory's offsets: 0 is ".", 1 is "..", and 2 on the index of an
 * entry in it, plus 2.
 */
static int
exfat_vnop_readdir(struct vnop_readdir_args *ap)
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;
	int extended = (ap->a_flags & VNODE_READDIR_EXTENDED) != 0;
	struct exfat_dcursor c;
	struct exfat_entry *e;
	off_t cookie = uio_offset(uio);
	size_t bufsize, namesize, len;
	void *buf;
	char *name;
	uint64_t index, next;
	int count = 0, eof = 0, error = 0, r;

	if (!vnode_isdir(vp))
		return (ENOTDIR);
	if (cookie < 0)
		return (EINVAL);
	bufsize = extended ? sizeof(struct direntry) : sizeof(struct dirent);
	buf = OSMalloc((uint32_t)bufsize, exfat_tag);
	e = OSMalloc(sizeof(*e), exfat_tag);
	if (buf == NULL || e == NULL) {
		error = ENOMEM;
		goto out;
	}
	if (extended) {
		name = ((struct direntry *)buf)->d_name;
		namesize = sizeof(((struct direntry *)buf)->d_name);
	} else {
		name = ((struct dirent *)buf)->d_name;
		namesize = sizeof(((struct dirent *)buf)->d_name);
	}

	while (cookie < 2) {
		r = exfat_emit(uio, extended, buf,
		    cookie == 0 ? np->info.id : np->info.parent_id, DT_DIR,
		    "..", (size_t)cookie + 1, (uint64_t)cookie + 1);
		if (r == -1)
			goto out;
		if (r) {
			error = r;
			goto out;
		}
		cookie++;
		count++;
	}

	exfat_dcursor_init(&c, xmp, &np->info);
	for (index = (uint64_t)cookie - 2;; ) {
		next = index;
		error = exfat_dir_next(&c, &next, np->info.id, e);
		if (error == ENOENT) {
			error = 0;
			eof = 1;
			break;
		}
		if (error)
			break;
		if (exfat_name_to_utf8(e->name, e->namelen, name, namesize,
		    &len) == 0) {
			r = exfat_emit(uio, extended, buf, e->info.id,
			    (e->info.attr & EXFAT_ATTR_DIR) ? DT_DIR : DT_REG,
			    name, len, next + 2);
			if (r == -1)
				break;
			if (r) {
				error = r;
				break;
			}
			count++;
		}
		index = next;
		cookie = (off_t)next + 2;
	}
	exfat_dcursor_done(&c);
out:
	uio_setoffset(uio, cookie);
	if (error == 0 && count == 0 && !eof && buf != NULL && e != NULL)
		error = EINVAL;		/* not even one entry fits */
	if (ap->a_eofflag != NULL)
		*ap->a_eofflag = eof;
	if (ap->a_numdirent != NULL)
		*ap->a_numdirent = count;
	if (buf != NULL)
		OSFree(buf, (uint32_t)bufsize, exfat_tag);
	if (e != NULL)
		OSFree(e, sizeof(*e), exfat_tag);
	return (error);
}

static int
exfat_vnop_pathconf(struct vnop_pathconf_args *ap)
{
	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 1;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = EXFAT_NAME_MAX;
		break;
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 200112;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 200112;
		break;
	case _PC_CASE_SENSITIVE:
		*ap->a_retval = 0;
		break;
	case _PC_CASE_PRESERVING:
		*ap->a_retval = 1;
		break;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static int
exfat_vnop_fsync(struct vnop_fsync_args *ap)
{
#pragma unused(ap)
	return (0);
}

static int
exfat_vnop_ioctl(struct vnop_ioctl_args *ap)
{
#pragma unused(ap)
	return (ENOTTY);
}

static int
exfat_vnop_select(struct vnop_select_args *ap)
{
#pragma unused(ap)
	return (1);
}

static int
exfat_vnop_rofs(void *ap)
{
#pragma unused(ap)
	return (EROFS);
}

static const struct vnodeopv_entry_desc exfat_vnodeop_entries[] = {
	{ &vnop_default_desc,	(VOPFUNC)vn_default_error },
	{ &vnop_lookup_desc,	(VOPFUNC)exfat_vnop_lookup },
	{ &vnop_open_desc,	(VOPFUNC)exfat_vnop_open },
	{ &vnop_close_desc,	(VOPFUNC)exfat_vnop_close },
	{ &vnop_getattr_desc,	(VOPFUNC)exfat_vnop_getattr },
	{ &vnop_setattr_desc,	(VOPFUNC)exfat_vnop_setattr },
	{ &vnop_read_desc,	(VOPFUNC)exfat_vnop_read },
	{ &vnop_write_desc,	(VOPFUNC)exfat_vnop_write },
	{ &vnop_ioctl_desc,	(VOPFUNC)exfat_vnop_ioctl },
	{ &vnop_select_desc,	(VOPFUNC)exfat_vnop_select },
	{ &vnop_mmap_desc,	(VOPFUNC)exfat_vnop_mmap },
	{ &vnop_fsync_desc,	(VOPFUNC)exfat_vnop_fsync },
	{ &vnop_create_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_mknod_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_remove_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_link_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_rename_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_mkdir_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_rmdir_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_symlink_desc,	(VOPFUNC)exfat_vnop_rofs },
	{ &vnop_readdir_desc,	(VOPFUNC)exfat_vnop_readdir },
	{ &vnop_inactive_desc,	(VOPFUNC)exfat_vnop_inactive },
	{ &vnop_reclaim_desc,	(VOPFUNC)exfat_vnop_reclaim },
	{ &vnop_pathconf_desc,	(VOPFUNC)exfat_vnop_pathconf },
	{ &vnop_pagein_desc,	(VOPFUNC)exfat_vnop_pagein },
	{ &vnop_pageout_desc,	(VOPFUNC)exfat_vnop_pageout },
	{ &vnop_blktooff_desc,	(VOPFUNC)exfat_vnop_blktooff },
	{ &vnop_offtoblk_desc,	(VOPFUNC)exfat_vnop_offtoblk },
	{ &vnop_blockmap_desc,	(VOPFUNC)exfat_vnop_blockmap },
	{ &vnop_strategy_desc,	(VOPFUNC)exfat_vnop_strategy },
	{ NULL,			(VOPFUNC)NULL }
};

struct vnodeopv_desc exfat_vnodeop_opv_desc = {
	&exfat_vnodeop_p, exfat_vnodeop_entries
};
