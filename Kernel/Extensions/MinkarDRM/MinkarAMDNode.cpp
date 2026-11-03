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

#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <miscfs/devfs/devfs.h>
#include "MinkarAMDNode.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDNode: " fmt "\n", ##__VA_ARGS__)

static int sMajor = -1;
static volatile SInt32 sNext = kMinkarDRMRenderMinor;

static int
nodeOpen(dev_t dev, int flags, int devtype, struct proc *p)
{
	(void)flags;
	(void)devtype;
	(void)p;
	int m = minor(dev);
	return m >= kMinkarDRMRenderMinor && m < sNext ? 0 : ENXIO;
}

static int
nodeClose(dev_t dev, int flags, int devtype, struct proc *p)
{
	(void)dev;
	(void)flags;
	(void)devtype;
	(void)p;
	return 0;
}

static int
nodeIoctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	(void)dev;
	(void)cmd;
	(void)data;
	(void)flag;
	(void)p;
	return ENOTTY;		/* the user client has the interface */
}

static struct cdevsw sNodeFunctions = {
	/* d_open     */ nodeOpen,
	/* d_close    */ nodeClose,
	/* d_read     */ eno_rdwrt,
	/* d_write    */ eno_rdwrt,
	/* d_ioctl    */ nodeIoctl,
	/* d_stop     */ eno_stop,
	/* d_reset    */ eno_reset,
	/* d_ttys     */ NULL,
	/* d_select   */ eno_select,
	/* d_mmap     */ eno_mmap,
	/* d_strategy */ eno_strat,
	/* d_reserved_1 */ NULL,
	/* d_reserved_2 */ NULL,
	/* d_type     */ 0,
};

bool
MinkarAMDNodeCreate(unsigned *minor, void **node)
{
	if (sMajor < 0) {
		int m = cdevsw_add(-1, &sNodeFunctions);
		if (m < 0) {
			LOG("no character device major");
			return false;
		}
		sMajor = m;
	}
	SInt32 mi = OSIncrementAtomic(&sNext);
	*node = devfs_make_node(makedev(sMajor, mi), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666,
	    "dri/renderD%d", (int)mi);
	if (*node == NULL) {
		LOG("devfs did not make dri/renderD%d", (int)mi);
		return false;
	}
	*minor = (unsigned)mi;
	LOG("/dev/dri/renderD%d", (int)mi);
	return true;
}

void
MinkarAMDNodeRemove(void *node)
{
	if (node != NULL)
		devfs_remove(node);
}
