/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * OpenBSM's configure results for Minkar (Darwin), in place of the
 * autoconf config.h (which is FreeBSD's).
 */

#ifndef _OPENBSM_CONFIG_MINKAR_H
#define _OPENBSM_CONFIG_MINKAR_H

#include <stdint.h>	/* compat/endian.h uses the fixed-width types */

#define HAVE_AUDIT_SYSCALLS	/**/
#define HAVE_FULL_QUEUE_H	/**/
#define HAVE_IPC_PERM__KEY	/**/
#define HAVE_IPC_PERM__SEQ	/**/
#define HAVE_PTHREAD_MUTEX_LOCK	1
#define HAVE_STRLCAT		1
#define HAVE_STRLCPY		1
#define HAVE_VIS		1
#define USE_COMPAT_ENDIAN_H	1
#define USE_COMPAT_ENDIAN_ENC_H	1
/* auditd and audit(8) talk over Mach: auditd_control.defs. */
#define USE_MACH_IPC		1

#endif /* !_OPENBSM_CONFIG_MINKAR_H */
