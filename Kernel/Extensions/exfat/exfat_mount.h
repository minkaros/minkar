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
 * The arguments of an exfat mount, shared by the kext and mount_exfat.
 * The kernel reads the device's path from the front of them (a local
 * volume's arguments) and gives the kext what follows it.
 */

#ifndef _EXFAT_MOUNT_H_
#define _EXFAT_MOUNT_H_

#include <stdint.h>

#define EXFAT_ARGS_VERSION	1

#define EXFAT_MNT_UID		0x0001	/* uid is given */
#define EXFAT_MNT_GID		0x0002	/* gid is given */
#define EXFAT_MNT_PERMS		0x0004	/* perms is given */
#define EXFAT_MNT_SECONDSWEST	0x0008	/* seconds_west is given */

/* What follows the device's path. */
struct exfat_args_fs {
	uint32_t	version;	/* EXFAT_ARGS_VERSION */
	uint32_t	flags;
	uint32_t	uid;		/* the owner of every file */
	uint32_t	gid;
	uint32_t	perms;		/* the permissions of every file */
	int32_t		seconds_west;	/* local time, west of UTC: for the
					   times saved without their offset */
};

#ifndef KERNEL
struct exfat_args {
	const char	*fspec;		/* the device */
	struct exfat_args_fs fs;
};
#endif

#endif /* !_EXFAT_MOUNT_H_ */
