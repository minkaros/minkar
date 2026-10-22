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
 * Window list filtering for CGWindowListCreate/CGWindowListCopyWindowInfo
 * (private). Plain C so it can be tested on the build host.
 */

#ifndef _CGWINDOWLISTFILTER_H_
#define _CGWINDOWLISTFILTER_H_

#include <stddef.h>
#include <stdint.h>
#include <WindowServer/rpc.h>

/* Same values as the public CGWindowListOption bits (CGWindow.h). */
#define _CGWLOptionOnScreenOnly         (1u << 0)
#define _CGWLOptionOnScreenAboveWindow  (1u << 1)
#define _CGWLOptionOnScreenBelowWindow  (1u << 2)
#define _CGWLOptionIncludingWindow      (1u << 3)
#define _CGWLExcludeDesktopElements     (1u << 4)

/*
 * Select entries (ordered front-most first) according to a
 * CGWindowListOption and a relative window ID (0 = kCGNullWindowID).
 * Writes the indexes of the selected entries, in order, to 'indexes'
 * (room for 'count') and returns how many were selected.
 */
size_t _CGWindowListFilter(const struct wsWindowListEntry *entries,
                           size_t count, uint32_t option,
                           uint32_t relativeToWindow, size_t *indexes);

#endif /* _CGWINDOWLISTFILTER_H_ */
