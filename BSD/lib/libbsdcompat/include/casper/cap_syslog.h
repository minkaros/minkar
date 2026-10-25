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

#ifndef _RAVYNOS_COMPAT_CAP_SYSLOG_H
#define _RAVYNOS_COMPAT_CAP_SYSLOG_H

/* Casper's syslog service, straight to syslog(3): ravynOS has no Casper. */

#include <stdarg.h>
#include <syslog.h>
#include <libcasper.h>

static inline void
cap_openlog(cap_channel_t *c, const char *ident, int logopt, int facility)
{
	(void)c;
	openlog(ident, logopt, facility);
}

static inline void
cap_closelog(cap_channel_t *c)
{
	(void)c;
	closelog();
}

static inline int
cap_setlogmask(cap_channel_t *c, int maskpri)
{
	(void)c;
	return (setlogmask(maskpri));
}

static inline void __printflike(3, 4)
cap_syslog(cap_channel_t *c, int pri, const char *fmt, ...)
{
	va_list ap;

	(void)c;
	va_start(ap, fmt);
	vsyslog(pri, fmt, ap);
	va_end(ap);
}

#endif /* !_RAVYNOS_COMPAT_CAP_SYSLOG_H */
