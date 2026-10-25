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

/* Window list filtering, following the CGWindowListOption semantics. */

#include "CGWindowListFilter.h"

#include <CoreGraphics/CGWindowLevel.h>

static int is_desktop_element(const struct wsWindowListEntry *e)
{
    return e->levelKey == kCGDesktopWindowLevelKey ||
           e->levelKey == kCGDesktopIconWindowLevelKey;
}

static int is_on_screen(const struct wsWindowListEntry *e)
{
    return (e->flags & kWSWindowListOnScreen) != 0;
}

size_t _CGWindowListFilter(const struct wsWindowListEntry *entries,
                           size_t count, uint32_t option,
                           uint32_t relativeToWindow, size_t *indexes)
{
    const uint32_t relative = _CGWLOptionOnScreenAboveWindow |
                              _CGWLOptionOnScreenBelowWindow;
    size_t i, n = 0, pivot = count;

    /* Locate the relative window, if one is needed. */
    if ((option & (relative | _CGWLOptionIncludingWindow)) != 0 &&
        relativeToWindow != 0) {
        for (i = 0; i < count; i++) {
            if (entries[i].windowID == relativeToWindow) {
                pivot = i;
                break;
            }
        }
        if (pivot == count)
            return 0;               /* unknown window: empty list */
    }

    for (i = 0; i < count; i++) {
        const struct wsWindowListEntry *e = &entries[i];
        int keep;

        if ((option & _CGWLExcludeDesktopElements) && is_desktop_element(e))
            continue;

        if ((option & relative) != 0) {
            if (pivot == count) {
                /* No relative window given: every on-screen window. */
                keep = is_on_screen(e);
            } else if (i == pivot) {
                keep = (option & _CGWLOptionIncludingWindow) != 0;
            } else if (i < pivot) {
                keep = (option & _CGWLOptionOnScreenAboveWindow) &&
                       is_on_screen(e);
            } else {
                keep = (option & _CGWLOptionOnScreenBelowWindow) &&
                       is_on_screen(e);
            }
        } else if (option & _CGWLOptionIncludingWindow) {
            /* Only the relative window itself. */
            keep = (pivot != count && i == pivot);
        } else if (option & _CGWLOptionOnScreenOnly) {
            keep = is_on_screen(e);
        } else {
            keep = 1;               /* kCGWindowListOptionAll */
        }

        if (keep)
            indexes[n++] = i;
    }
    return n;
}
