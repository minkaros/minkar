/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * authd (APSL) builds against these: the private headers it includes,
 * as what Minkar has.
 */

#ifndef MINKAR_AUTHD_AUDIT_SESSION_H
#define MINKAR_AUTHD_AUDIT_SESSION_H

#include <bsm/audit.h>
#include <bsm/audit_kevents.h>
#include <errno.h>

/* A session's attributes in its audit info (ai_flags). */
#ifndef AU_SESSION_FLAG_IS_INITIAL
#define AU_SESSION_FLAG_IS_INITIAL		0x0001
#define AU_SESSION_FLAG_HAS_GRAPHIC_ACCESS	0x0010
#define AU_SESSION_FLAG_HAS_TTY			0x0020
#define AU_SESSION_FLAG_IS_REMOTE		0x1000
#define AU_SESSION_FLAG_HAS_CONSOLE_ACCESS	0x2000
#define AU_SESSION_FLAG_HAS_AUTHENTICATED	0x4000
#endif

/* The audit sessions device, which tells of sessions ending: not in
   Minkar's libbsm, so it cannot be opened and authd keeps its sessions
   until it exits (it logs that it could not). */
#define AUDIT_SDEV_PATH		"/dev/auditsessions"
#define AU_SDEVF_ALLSESSIONS	0x00000001
#ifndef AUE_SESSION_END
#define AUE_SESSION_END		44903
#endif
typedef struct au_sdev_handle au_sdev_handle_t;

static inline au_sdev_handle_t *au_sdev_open(int flags)
{
	(void)flags;
	errno = ENOTSUP;
	return (au_sdev_handle_t *)0;
}

static inline int au_sdev_read_aia(au_sdev_handle_t *dev, int *event, auditinfo_addr_t *aia)
{
	(void)dev; (void)event; (void)aia;
	errno = ENOTSUP;
	return -1;
}

#endif
