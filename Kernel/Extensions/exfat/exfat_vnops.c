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
 * exfat: the vnode operations of an exFAT volume. A file's data
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
	np->state = EXFAT_NODE_ATTACHING | EXFAT_NODE_HASHED;
	np->lock = lck_mtx_alloc_init(exfat_lck_grp, LCK_ATTR_NULL);
	np->iolock = lck_rw_alloc_init(exfat_lck_grp, LCK_ATTR_NULL);
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
	} else {
		LIST_REMOVE(np, link);
		np->state &= ~EXFAT_NODE_HASHED;
	}
	if (np->state & EXFAT_NODE_WANTED) {
		np->state &= ~EXFAT_NODE_WANTED;
		wakeup(np);
	}
	lck_mtx_unlock(xmp->lock);
	if (error) {
		lck_mtx_free(np->lock, exfat_lck_grp);
		lck_rw_free(np->iolock, exfat_lck_grp);
		OSFree(np, sizeof(*np), exfat_tag);
		return (error);
	}
	if (dir && cnp == NULL && info->id != EXFAT_ROOT_ID)
		exfat_name_vnode(xmp, vp, info->id);
	*vpp = vp;
	return (0);
}

/* Out of the node table: it is deleted, or it got another number. */
static void
exfat_unhash(struct exfat_mount *xmp, struct exfat_node *np)
{
	lck_mtx_lock(xmp->lock);
	if (np->state & EXFAT_NODE_HASHED) {
		LIST_REMOVE(np, link);
		np->state &= ~EXFAT_NODE_HASHED;
	}
	lck_mtx_unlock(xmp->lock);
}

/* A deleted file's clusters, once nothing has it open. */
static void
exfat_free_deleted(struct exfat_mount *xmp, vnode_t vp, struct exfat_node *np)
{
	if (!(np->state & EXFAT_NODE_DELETED) || (np->state & EXFAT_NODE_FREED))
		return;
	np->state |= EXFAT_NODE_FREED;
	if (vnode_isreg(vp))
		ubc_setsize(vp, 0);
	lck_rw_lock_exclusive(xmp->meta);
	lck_mtx_lock(np->lock);
	(void)exfat_shrink(xmp, &np->stream, 0, vnode_isdir(vp));
	lck_mtx_unlock(np->lock);
	lck_rw_unlock_exclusive(xmp->meta);
}

static int
exfat_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;

	exfat_free_deleted(xmp, vp, np);
	exfat_unhash(xmp, np);
	vnode_removefsref(vp);
	vnode_clearfsnode(vp);
	lck_mtx_free(np->lock, exfat_lck_grp);
	lck_rw_free(np->iolock, exfat_lck_grp);
	OSFree(np, sizeof(*np), exfat_tag);
	return (0);
}

static int
exfat_vnop_inactive(struct vnop_inactive_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp);

	if (np->state & EXFAT_NODE_DELETED) {
		exfat_free_deleted(np->xmp, vp, np);
		vnode_recycle(vp);
	}
	return (0);
}

/*
 * A file's or directory's set takes its node's info; a directory's
 * record too (or the root's), so ".." finds it as it is now.
 */
void
exfat_node_changed(struct exfat_mount *xmp, struct exfat_node *np)
{
	struct exfat_dirrec *rec;
	struct exfat_info info;

	lck_mtx_lock(np->lock);
	info = np->info;
	lck_mtx_unlock(np->lock);
	if (np->state & EXFAT_NODE_DELETED)
		return;
	(void)exfat_update_set(xmp, &info);
	if (!(info.attr & EXFAT_ATTR_DIR))
		return;
	lck_mtx_lock(xmp->lock);
	if (info.id == EXFAT_ROOT_ID)
		xmp->root = info;
	else if ((rec = exfat_dirrec_find(xmp, info.id)) != NULL)
		rec->info = info;
	lck_mtx_unlock(xmp->lock);
}

/* A directory was changed in: its modification time. */
static void
exfat_touch_dir(struct exfat_mount *xmp, struct exfat_node *dnp)
{
	lck_mtx_lock(dnp->lock);
	exfat_now(xmp, &dnp->info.mtime, &dnp->info.m10ms, &dnp->info.mutc);
	dnp->info.atime = dnp->info.mtime;
	dnp->info.autc = dnp->info.mutc;
	lck_mtx_unlock(dnp->lock);
	exfat_node_changed(xmp, dnp);
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
	if (VTOEXFAT(dvp)->state & EXFAT_NODE_DELETED)
		return (ENOENT);
	if (last && !xmp->rw && (op == DELETE || op == RENAME))
		return (EROFS);

	error = cache_lookup(dvp, ap->a_vpp, cnp);
	if (error == -1)
		return (0);
	if (error == ENOENT) {
		if (last && (op == CREATE || op == RENAME))
			return (xmp->rw ? EJUSTRETURN : EROFS);
		return (ENOENT);
	}

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
	lck_rw_lock_shared(xmp->meta);
	error = exfat_dir_find(xmp, &dnp->info, name, (uint16_t)(bytes / 2), e);
	lck_rw_unlock_shared(xmp->meta);
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
		if ((cnp->cn_flags & MAKEENTRY) && op != CREATE && op != RENAME)
			cache_enter(dvp, NULL, cnp);
		if (last && (op == CREATE || op == RENAME))
			error = xmp->rw ? EJUSTRETURN : EROFS;
	}
	OSFree(e, sizeof(*e), exfat_tag);
	return (error);
}

static int
exfat_vnop_open(struct vnop_open_args *ap)
{
	if ((ap->a_mode & FWRITE) != 0 && !VTOEXFAT(ap->a_vp)->xmp->rw)
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

/* A file's clusters: as many as size takes. */
static int
exfat_alloc_to(struct exfat_mount *xmp, struct exfat_node *np, uint64_t size)
{
	uint64_t want = (size + xmp->cluster_size - 1) >> xmp->cluster_shift;
	uint64_t have;
	int error = 0;

	lck_rw_lock_exclusive(xmp->meta);
	lck_mtx_lock(np->lock);
	have = np->stream.first ? (np->stream.length + xmp->cluster_size - 1) >>
	    xmp->cluster_shift : 0;
	if (want > have)
		error = exfat_grow(xmp, &np->stream, want);
	else if (want < have)
		error = exfat_shrink(xmp, &np->stream, want, 0);
	np->info.first = np->stream.first;
	np->info.contig = np->stream.contig;
	lck_mtx_unlock(np->lock);
	lck_rw_unlock_exclusive(xmp->meta);
	return (error);
}

static void
exfat_save(struct exfat_mount *xmp, struct exfat_node *np)
{
	lck_rw_lock_exclusive(xmp->meta);
	exfat_node_changed(xmp, np);
	lck_rw_unlock_exclusive(xmp->meta);
}

/* Modified now: its time, and the archive bit, as Windows sets it. */
static void
exfat_modified(struct exfat_mount *xmp, struct exfat_node *np)
{
	lck_mtx_lock(np->lock);
	exfat_now(xmp, &np->info.mtime, &np->info.m10ms, &np->info.mutc);
	np->info.atime = np->info.mtime;
	np->info.autc = np->info.mutc;
	np->info.attr |= EXFAT_ATTR_ARCHIVE;
	lck_mtx_unlock(np->lock);
}

/*
 * A file from elsewhere may have bytes past its valid length (zeros to
 * a reader): before it changes they are written as zeros, so all of it
 * is valid from then on.
 */
static int
exfat_fill_valid(vnode_t vp, struct exfat_node *np)
{
	uint64_t valid = np->info.valid, length = np->info.length;
	int error;

	if (valid >= length)
		return (0);
	error = cluster_write(vp, NULL, (off_t)length, (off_t)length,
	    (off_t)valid, 0, IO_HEADZEROFILL | IO_NOZERODIRTY);
	if (error)
		return (error);
	lck_mtx_lock(np->lock);
	np->info.valid = np->info.length;
	lck_mtx_unlock(np->lock);
	return (0);
}

/* A file's size changes: cut, or grown with zeros. */
static int
exfat_resize(vnode_t vp, struct exfat_node *np, uint64_t size)
{
	struct exfat_mount *xmp = np->xmp;
	uint64_t old = np->info.length;
	int error;

	if (size == old)
		return (0);
	if (size < old) {
		ubc_setsize(vp, (off_t)size);
		lck_mtx_lock(np->lock);
		np->info.length = size;
		np->info.valid = MIN(np->info.valid, size);
		lck_mtx_unlock(np->lock);
		error = exfat_alloc_to(xmp, np, size);
	} else {
		if ((error = exfat_fill_valid(vp, np)))
			return (error);
		if ((error = exfat_alloc_to(xmp, np, size)))
			return (error);
		error = cluster_write(vp, NULL, (off_t)old, (off_t)size,
		    (off_t)old, 0, IO_HEADZEROFILL | IO_NOZERODIRTY);
		if (error) {
			(void)exfat_alloc_to(xmp, np, old);
			return (error);
		}
		lck_mtx_lock(np->lock);
		np->info.length = np->info.valid = size;
		lck_mtx_unlock(np->lock);
		ubc_setsize(vp, (off_t)size);
	}
	exfat_modified(xmp, np);
	exfat_save(xmp, np);
	return (error);
}

/*
 * Size, times and the flags exFAT has (hidden, read only). Owners and
 * modes are the mount's: setting them is taken, and changes nothing.
 */
static int
exfat_vnop_setattr(struct vnop_setattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;
	int changed = 0, error;

	if (!xmp->rw)
		return (EROFS);
	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		if (vnode_isdir(vp))
			return (EISDIR);
		if (vap->va_data_size < 0)
			return (EINVAL);
		lck_rw_lock_exclusive(np->iolock);
		error = exfat_resize(vp, np, (uint64_t)vap->va_data_size);
		lck_rw_unlock_exclusive(np->iolock);
		if (error)
			return (error);
		VATTR_SET_SUPPORTED(vap, va_data_size);
	}

	lck_mtx_lock(np->lock);
	if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
		exfat_encode_time(xmp, &vap->va_modify_time, &np->info.mtime,
		    &np->info.m10ms, &np->info.mutc);
		VATTR_SET_SUPPORTED(vap, va_modify_time);
		changed = 1;
	}
	if (VATTR_IS_ACTIVE(vap, va_access_time)) {
		uint8_t ms10;

		exfat_encode_time(xmp, &vap->va_access_time, &np->info.atime,
		    &ms10, &np->info.autc);
		VATTR_SET_SUPPORTED(vap, va_access_time);
		changed = 1;
	}
	if (VATTR_IS_ACTIVE(vap, va_create_time)) {
		exfat_encode_time(xmp, &vap->va_create_time, &np->info.ctime,
		    &np->info.c10ms, &np->info.cutc);
		VATTR_SET_SUPPORTED(vap, va_create_time);
		changed = 1;
	}
	if (VATTR_IS_ACTIVE(vap, va_flags)) {
		np->info.attr &= (uint16_t)~(EXFAT_ATTR_HIDDEN | EXFAT_ATTR_RDONLY);
		if (vap->va_flags & UF_HIDDEN)
			np->info.attr |= EXFAT_ATTR_HIDDEN;
		if (!vnode_isdir(vp) && (vap->va_flags & UF_IMMUTABLE))
			np->info.attr |= EXFAT_ATTR_RDONLY;
		VATTR_SET_SUPPORTED(vap, va_flags);
		changed = 1;
	}
	lck_mtx_unlock(np->lock);
	if (VATTR_IS_ACTIVE(vap, va_mode))
		VATTR_SET_SUPPORTED(vap, va_mode);
	if (VATTR_IS_ACTIVE(vap, va_uid))
		VATTR_SET_SUPPORTED(vap, va_uid);
	if (VATTR_IS_ACTIVE(vap, va_gid))
		VATTR_SET_SUPPORTED(vap, va_gid);
	if (changed)
		exfat_save(xmp, np);
	return (0);
}

static const uint8_t exfat_zeros[PAGE_SIZE];

/*
 * Past a file's valid length it reads as zeros, though the disk may hold
 * anything there: the part before it is read as if the file ended there
 * (the kernel zeros the rest of that page), the rest is given as zeros.
 */
static int
exfat_vnop_read(struct vnop_read_args *ap)
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	struct exfat_node *np = VTOEXFAT(vp);
	uint64_t valid, length, off;
	user_ssize_t resid, part, n;
	int error = 0;

	if (vnode_isdir(vp))
		return (EISDIR);
	if (!vnode_isreg(vp))
		return (EPERM);
	if (uio_offset(uio) < 0)
		return (EINVAL);
	lck_mtx_lock(np->lock);
	valid = np->info.valid;
	length = np->info.length;
	lck_mtx_unlock(np->lock);
	if (valid >= length)
		return (cluster_read(vp, uio, (off_t)length, ap->a_ioflag));

	off = (uint64_t)uio_offset(uio);
	resid = uio_resid(uio);
	if (off < valid && resid > 0) {
		part = (user_ssize_t)MIN((uint64_t)resid, valid - off);
		uio_setresid(uio, part);
		error = cluster_read(vp, uio, (off_t)valid, ap->a_ioflag);
		resid -= part - uio_resid(uio);
		uio_setresid(uio, resid);
		if (error)
			return (error);
	}
	while (error == 0 && uio_resid(uio) > 0 &&
	    (uint64_t)uio_offset(uio) < length) {
		n = (user_ssize_t)MIN((uint64_t)uio_resid(uio),
		    length - (uint64_t)uio_offset(uio));
		n = MIN(n, (user_ssize_t)sizeof(exfat_zeros));
		error = uiomove((const char *)exfat_zeros, (int)n, uio);
	}
	return (error);
}

/*
 * Clusters first, then the data through the unified buffer cache (a gap
 * after the old end is zeros), then the size and time in its set.
 */
static int
exfat_vnop_write(struct vnop_write_args *ap)
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;
	uint64_t off, end, oldsize, newsize;
	int flags = ap->a_ioflag, error;
	user_ssize_t resid;

	if (vnode_isdir(vp))
		return (EISDIR);
	if (!vnode_isreg(vp))
		return (EPERM);
	if (!xmp->rw)
		return (EROFS);
	if (uio_offset(uio) < 0)
		return (EINVAL);

	lck_rw_lock_exclusive(np->iolock);
	if (ap->a_ioflag & IO_APPEND)
		uio_setoffset(uio, (off_t)np->info.length);
	resid = uio_resid(uio);
	if (resid <= 0) {
		error = 0;
		goto out;
	}
	off = (uint64_t)uio_offset(uio);
	end = off + (uint64_t)resid;
	oldsize = np->info.length;
	if ((error = exfat_fill_valid(vp, np)))
		goto out;
	if ((error = exfat_alloc_to(xmp, np, MAX(oldsize, end))))
		goto out;
	if (off > oldsize)
		flags |= IO_HEADZEROFILL;
	error = cluster_write(vp, uio, (off_t)oldsize, (off_t)MAX(oldsize, end),
	    (off_t)oldsize, 0, flags);
	newsize = MAX(oldsize, (uint64_t)uio_offset(uio));
	if (newsize > oldsize) {
		lck_mtx_lock(np->lock);
		np->info.length = np->info.valid = newsize;
		lck_mtx_unlock(np->lock);
		ubc_setsize(vp, (off_t)newsize);
	}
	if (error)
		(void)exfat_alloc_to(xmp, np, newsize);
	if (newsize != oldsize || error == 0)
		exfat_modified(xmp, np);
	exfat_save(xmp, np);
	if (error == 0 && (ap->a_ioflag & IO_SYNC)) {
		error = cluster_push(vp, IO_SYNC);
		buf_flushdirtyblks(xmp->devvp, 1, 0, "exfat_write");
	}
out:
	lck_rw_unlock_exclusive(np->iolock);
	return (error);
}

/* Pages of a page list made zeros, and given to the file. */
static int
exfat_zero_pages(upl_t pl, upl_offset_t pl_offset, size_t size, int flags)
{
	vm_offset_t addr;

	if (ubc_upl_map(pl, &addr) != KERN_SUCCESS) {
		if (!(flags & UPL_NOCOMMIT))
			ubc_upl_abort_range(pl, pl_offset, (upl_size_t)size,
			    UPL_ABORT_FREE_ON_EMPTY | UPL_ABORT_ERROR);
		return (EIO);
	}
	bzero((void *)(addr + pl_offset), size);
	(void)ubc_upl_unmap(pl);
	if (!(flags & UPL_NOCOMMIT))
		ubc_upl_commit_range(pl, pl_offset, (upl_size_t)size,
		    UPL_COMMIT_FREE_ON_EMPTY);
	return (0);
}

/* As read: past the valid length, zeros (see exfat_vnop_read). */
static int
exfat_vnop_pagein(struct vnop_pagein_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp);
	uint64_t valid, length, f = (uint64_t)ap->a_f_offset, edge;
	size_t size = ap->a_size, part;
	int error;

	lck_mtx_lock(np->lock);
	valid = np->info.valid;
	length = np->info.length;
	lck_mtx_unlock(np->lock);
	if (valid >= length || ap->a_f_offset < 0 || f >= length)
		return (cluster_pagein(vp, ap->a_pl, ap->a_pl_offset,
		    ap->a_f_offset, (int)ap->a_size, (off_t)length, ap->a_flags));

	edge = (valid + PAGE_SIZE - 1) & ~(uint64_t)PAGE_MASK;
	if (f < edge) {
		part = (size_t)MIN((uint64_t)size, edge - f);
		error = cluster_pagein(vp, ap->a_pl, ap->a_pl_offset,
		    ap->a_f_offset, (int)part, (off_t)valid, ap->a_flags);
		if (error || part == size)
			return (error);
		return (exfat_zero_pages(ap->a_pl,
		    ap->a_pl_offset + (upl_offset_t)part, size - part,
		    ap->a_flags));
	}
	return (exfat_zero_pages(ap->a_pl, ap->a_pl_offset, size, ap->a_flags));
}

static int
exfat_vnop_pageout(struct vnop_pageout_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp);

	if (!np->xmp->rw) {
		if (!(ap->a_flags & UPL_NOCOMMIT) && ap->a_pl != NULL)
			ubc_upl_abort_range(ap->a_pl, ap->a_pl_offset,
			    (upl_size_t)ap->a_size, UPL_ABORT_FREE_ON_EMPTY);
		return (EROFS);
	}
	return (cluster_pageout(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
	    (int)ap->a_size, (off_t)np->info.length, ap->a_flags));
}

static int
exfat_vnop_mmap(struct vnop_mmap_args *ap)
{
	if ((ap->a_fflags & PROT_WRITE) && !VTOEXFAT(ap->a_vp)->xmp->rw)
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
	if (!(ap->a_flags & VNODE_WRITE) && off >= np->info.valid) {
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
	if (!(ap->a_flags & VNODE_WRITE))
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

	if (np->state & EXFAT_NODE_DELETED) {
		eof = 1;
		goto out;
	}
	lck_rw_lock_shared(xmp->meta);
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
	lck_rw_unlock_shared(xmp->meta);
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
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp);
	struct exfat_mount *xmp = np->xmp;
	int wait = ap->a_waitfor == MNT_WAIT;

	if (!xmp->rw)
		return (0);
	if (vnode_isreg(vp))
		(void)cluster_push(vp, wait ? IO_SYNC : 0);
	exfat_save(xmp, np);
	buf_flushdirtyblks(xmp->devvp, wait, 0, "exfat_fsync");
	return (0);
}

/* A new file or directory, in dvp. */
static int
exfat_make(vnode_t dvp, struct componentname *cnp, int dir, vnode_t *vpp)
{
	struct exfat_node *dnp = VTOEXFAT(dvp);
	struct exfat_mount *xmp = dnp->xmp;
	uint16_t name[EXFAT_NAME_MAX], up[EXFAT_NAME_MAX], namelen;
	uint8_t set[EXFAT_SET_MAX * EXFAT_ENTRY_SIZE];
	struct exfat_stream st;
	struct exfat_info info;
	struct exfat_entry *e;
	char *utf8 = NULL;
	unsigned count, i;
	size_t len;
	int error;

	*vpp = NULL;
	if (!xmp->rw)
		return (EROFS);
	error = exfat_name_from_utf8(cnp->cn_nameptr, (size_t)cnp->cn_namelen,
	    name, &namelen);
	if (error)
		return (error);
	for (i = 0; i < namelen; i++)
		up[i] = exfat_upcase(xmp, name[i]);
	if ((e = OSMalloc(sizeof(*e), exfat_tag)) == NULL)
		return (ENOMEM);
	bzero(&info, sizeof(info));
	bzero(&st, sizeof(st));
	info.attr = dir ? EXFAT_ATTR_DIR : EXFAT_ATTR_ARCHIVE;
	exfat_now(xmp, &info.ctime, &info.c10ms, &info.cutc);
	info.mtime = info.atime = info.ctime;
	info.m10ms = info.c10ms;
	info.mutc = info.autc = info.cutc;

	lck_rw_lock_exclusive(xmp->meta);
	if (dnp->state & EXFAT_NODE_DELETED) {
		error = ENOENT;
		goto out;
	}
	error = exfat_dir_find(xmp, &dnp->info, up, namelen, e);
	if (error != ENOENT) {
		if (error == 0)
			error = EEXIST;
		goto out;
	}
	if (dir) {
		if ((error = exfat_grow(xmp, &st, 1)))
			goto out;
		if ((error = exfat_zero_cluster(xmp, st.first))) {
			(void)exfat_shrink(xmp, &st, 0, 1);
			goto out;
		}
		info.first = st.first;
		info.contig = st.contig;
		info.length = info.valid = st.length;
	}
	count = 2 + (namelen + EXFAT_NAME_CHARS - 1) / EXFAT_NAME_CHARS;
	error = exfat_dir_space(xmp, dnp, count, info.pos);
	if (error == 0) {
		info.count = count;
		info.id = info.pos[0] / EXFAT_ENTRY_SIZE;
		info.parent_id = dnp->info.id;
		(void)exfat_build_set(xmp, &info, name, namelen, set);
		error = exfat_write_set(xmp, info.pos, count, set);
	}
	if (error) {
		if (dir)
			(void)exfat_shrink(xmp, &st, 0, 1);
		goto out;
	}
	exfat_touch_dir(xmp, dnp);
out:
	lck_rw_unlock_exclusive(xmp->meta);
	if (error == 0) {
		cache_purge_negatives(dvp);
		if (dir && (utf8 = OSMalloc(EXFAT_NAME_MAX * 3 + 1,
		    exfat_tag)) != NULL && exfat_name_to_utf8(name, namelen, utf8,
		    EXFAT_NAME_MAX * 3 + 1, &len) != 0) {
			OSFree(utf8, EXFAT_NAME_MAX * 3 + 1, exfat_tag);
			utf8 = NULL;
		}
		error = exfat_getnode(xmp, &info, dvp, cnp, utf8, vpp);
		if (utf8 != NULL)
			OSFree(utf8, EXFAT_NAME_MAX * 3 + 1, exfat_tag);
	}
	OSFree(e, sizeof(*e), exfat_tag);
	return (error);
}

static int
exfat_vnop_create(struct vnop_create_args *ap)
{
	if (ap->a_vap->va_type != VREG)
		return (ENOTSUP);
	return (exfat_make(ap->a_dvp, ap->a_cnp, 0, ap->a_vpp));
}

static int
exfat_vnop_mkdir(struct vnop_mkdir_args *ap)
{
	return (exfat_make(ap->a_dvp, ap->a_cnp, 1, ap->a_vpp));
}

/* A directory's record goes, with the directory. */
static void
exfat_dirrec_remove(struct exfat_mount *xmp, uint64_t id)
{
	struct exfat_dirrec *rec;

	lck_mtx_lock(xmp->lock);
	if ((rec = exfat_dirrec_find(xmp, id)) != NULL)
		LIST_REMOVE(rec, link);
	lck_mtx_unlock(xmp->lock);
	if (rec != NULL) {
		if (rec->name != NULL)
			OSFree(rec->name, rec->namesize, exfat_tag);
		OSFree(rec, sizeof(*rec), exfat_tag);
	}
}

/*
 * Its entries go now; a file's clusters when nothing has it open any
 * more (inactive), a directory's now (it is empty).
 */
static int
exfat_unlink(struct exfat_mount *xmp, vnode_t vp, struct exfat_node *np)
{
	int error;

	if ((error = exfat_delete_set(xmp, np->info.pos, np->info.count)))
		return (error);
	np->state |= EXFAT_NODE_DELETED;
	if (vnode_isdir(vp)) {
		lck_mtx_lock(np->lock);
		(void)exfat_shrink(xmp, &np->stream, 0, 1);
		lck_mtx_unlock(np->lock);
		np->state |= EXFAT_NODE_FREED;
		exfat_dirrec_remove(xmp, np->info.id);
	}
	return (0);
}

static int
exfat_vnop_remove(struct vnop_remove_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp), *dnp = VTOEXFAT(ap->a_dvp);
	struct exfat_mount *xmp = np->xmp;
	int error;

	if (!xmp->rw)
		return (EROFS);
	if (vnode_isdir(vp))
		return (EPERM);
	if ((ap->a_flags & VNODE_REMOVE_NODELETEBUSY) && vnode_isinuse(vp, 0))
		return (EBUSY);
	lck_rw_lock_exclusive(xmp->meta);
	if (np->state & EXFAT_NODE_DELETED)
		error = ENOENT;
	else if ((error = exfat_unlink(xmp, vp, np)) == 0)
		exfat_touch_dir(xmp, dnp);
	lck_rw_unlock_exclusive(xmp->meta);
	if (error == 0) {
		exfat_unhash(xmp, np);
		cache_purge(vp);
	}
	return (error);
}

static int
exfat_vnop_rmdir(struct vnop_rmdir_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct exfat_node *np = VTOEXFAT(vp), *dnp = VTOEXFAT(ap->a_dvp);
	struct exfat_mount *xmp = np->xmp;
	int error;

	if (!xmp->rw)
		return (EROFS);
	if (vnode_isvroot(vp))
		return (EBUSY);
	lck_rw_lock_exclusive(xmp->meta);
	if (np->state & EXFAT_NODE_DELETED)
		error = ENOENT;
	else if ((error = exfat_dir_empty(xmp, &np->info)) == 0 &&
	    (error = exfat_unlink(xmp, vp, np)) == 0)
		exfat_touch_dir(xmp, dnp);
	lck_rw_unlock_exclusive(xmp->meta);
	if (error == 0) {
		exfat_unhash(xmp, np);
		cache_purge(vp);
	}
	return (error);
}

/*
 * A renamed file's entries move, so its number (where its entries are)
 * changes: its node, a directory's record and its children follow.
 */
static void
exfat_renumber(struct exfat_mount *xmp, struct exfat_node *np, uint64_t old,
    const char *name)
{
	struct exfat_dirrec *rec;
	struct exfat_node *n;
	uint64_t id = np->info.id;
	unsigned i;

	lck_mtx_lock(xmp->lock);
	if (np->state & EXFAT_NODE_HASHED) {
		LIST_REMOVE(np, link);
		LIST_INSERT_HEAD(&xmp->nodes[EXFAT_HASHOF(id)], np, link);
	}
	if (np->info.attr & EXFAT_ATTR_DIR) {
		if ((rec = exfat_dirrec_find(xmp, old)) != NULL) {
			LIST_REMOVE(rec, link);
			rec->info = np->info;
			if (name != NULL) {
				char *copy;
				uint32_t size = (uint32_t)strlen(name) + 1;

				if ((copy = OSMalloc(size, exfat_tag)) != NULL) {
					strlcpy(copy, name, size);
					if (rec->name != NULL)
						OSFree(rec->name, rec->namesize,
						    exfat_tag);
					rec->name = copy;
					rec->namesize = size;
				}
			}
			LIST_INSERT_HEAD(&xmp->dirs[EXFAT_HASHOF(id)], rec, link);
		}
		for (i = 0; i < EXFAT_HASH; i++) {
			LIST_FOREACH(n, &xmp->nodes[i], link)
				if (n->info.parent_id == old)
					n->info.parent_id = id;
			LIST_FOREACH(rec, &xmp->dirs[i], link)
				if (rec->info.parent_id == old)
					rec->info.parent_id = id;
		}
	}
	lck_mtx_unlock(xmp->lock);
}

/* Whether a directory is dir or one of its subdirectories. */
static int
exfat_inside(struct exfat_mount *xmp, uint64_t dir, uint64_t id)
{
	struct exfat_dirrec *rec;
	unsigned depth = 0;
	int inside = 0;

	lck_mtx_lock(xmp->lock);
	while (id != EXFAT_ROOT_ID && depth++ < 4096) {
		if (id == dir) {
			inside = 1;
			break;
		}
		if ((rec = exfat_dirrec_find(xmp, id)) == NULL)
			break;
		id = rec->info.parent_id;
	}
	lck_mtx_unlock(xmp->lock);
	return (inside);
}

static int
exfat_vnop_rename(struct vnop_rename_args *ap)
{
	vnode_t fvp = ap->a_fvp, tvp = ap->a_tvp, tdvp = ap->a_tdvp;
	struct exfat_node *fnp = VTOEXFAT(fvp), *tdnp = VTOEXFAT(tdvp);
	struct exfat_node *fdnp = VTOEXFAT(ap->a_fdvp);
	struct exfat_node *tnp = tvp != NULL && tvp != fvp ? VTOEXFAT(tvp) : NULL;
	struct exfat_mount *xmp = fnp->xmp;
	uint8_t set[EXFAT_SET_MAX * EXFAT_ENTRY_SIZE];
	uint16_t name[EXFAT_NAME_MAX], namelen;
	struct exfat_info info, old;
	int dir = vnode_isdir(fvp), error;
	char *utf8 = NULL;
	unsigned count;
	size_t len;

	if (!xmp->rw)
		return (EROFS);
	error = exfat_name_from_utf8(ap->a_tcnp->cn_nameptr,
	    (size_t)ap->a_tcnp->cn_namelen, name, &namelen);
	if (error)
		return (error);
	if (dir && (utf8 = OSMalloc(EXFAT_NAME_MAX * 3 + 1, exfat_tag)) != NULL &&
	    exfat_name_to_utf8(name, namelen, utf8, EXFAT_NAME_MAX * 3 + 1,
	    &len) != 0) {
		OSFree(utf8, EXFAT_NAME_MAX * 3 + 1, exfat_tag);
		utf8 = NULL;
	}

	lck_rw_lock_exclusive(xmp->meta);
	if ((fnp->state | tdnp->state) & EXFAT_NODE_DELETED) {
		error = ENOENT;
		goto out;
	}
	if (dir && exfat_inside(xmp, fnp->info.id, tdnp->info.id)) {
		error = EINVAL;
		goto out;
	}
	if (tnp != NULL) {
		if (dir && !vnode_isdir(tvp))
			error = ENOTDIR;
		else if (!dir && vnode_isdir(tvp))
			error = EISDIR;
		else if (vnode_isdir(tvp))
			error = exfat_dir_empty(xmp, &tnp->info);
		if (error == 0 && (tnp->state & EXFAT_NODE_DELETED) == 0)
			error = exfat_unlink(xmp, tvp, tnp);
		if (error)
			goto out;
	}

	lck_mtx_lock(fnp->lock);
	old = info = fnp->info;
	lck_mtx_unlock(fnp->lock);
	count = 2 + (namelen + EXFAT_NAME_CHARS - 1) / EXFAT_NAME_CHARS;
	if ((error = exfat_dir_space(xmp, tdnp, count, info.pos)))
		goto out;
	info.count = count;
	info.id = info.pos[0] / EXFAT_ENTRY_SIZE;
	info.parent_id = tdnp->info.id;
	(void)exfat_build_set(xmp, &info, name, namelen, set);
	if ((error = exfat_write_set(xmp, info.pos, count, set)))
		goto out;
	(void)exfat_delete_set(xmp, old.pos, old.count);

	lck_mtx_lock(fnp->lock);
	fnp->info.count = info.count;
	memcpy(fnp->info.pos, info.pos, sizeof(info.pos));
	fnp->info.id = info.id;
	fnp->info.parent_id = info.parent_id;
	lck_mtx_unlock(fnp->lock);
	exfat_renumber(xmp, fnp, old.id, utf8);
	exfat_touch_dir(xmp, fdnp);
	if (tdnp != fdnp)
		exfat_touch_dir(xmp, tdnp);
out:
	lck_rw_unlock_exclusive(xmp->meta);
	if (utf8 != NULL)
		OSFree(utf8, EXFAT_NAME_MAX * 3 + 1, exfat_tag);
	if (error == 0) {
		if (tnp != NULL) {
			exfat_unhash(xmp, tnp);
			cache_purge(tvp);
		}
		cache_purge(fvp);
		cache_purge_negatives(tdvp);
	}
	return (error);
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

/* exFAT has no links, symbolic links or device files. */
static int
exfat_vnop_nosupport(void *ap)
{
#pragma unused(ap)
	return (ENOTSUP);
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
	{ &vnop_create_desc,	(VOPFUNC)exfat_vnop_create },
	{ &vnop_mknod_desc,	(VOPFUNC)exfat_vnop_nosupport },
	{ &vnop_remove_desc,	(VOPFUNC)exfat_vnop_remove },
	{ &vnop_link_desc,	(VOPFUNC)exfat_vnop_nosupport },
	{ &vnop_rename_desc,	(VOPFUNC)exfat_vnop_rename },
	{ &vnop_mkdir_desc,	(VOPFUNC)exfat_vnop_mkdir },
	{ &vnop_rmdir_desc,	(VOPFUNC)exfat_vnop_rmdir },
	{ &vnop_symlink_desc,	(VOPFUNC)exfat_vnop_nosupport },
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
