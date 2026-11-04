/**
 * signpost.h
 * Author: Zoe Knox      Created: 2026-7-3
 *
 * Copyright (C) 2026 Zoe Knox. All rights reserved.
 * SPDX: BSD-2-Clause
 *
 * This is an original implementation of Apple's libsystem_trace.dylib based on
 * open-source code, including ASL, XNU, Swift, and other sources, and the API
 * specs on developer.apple.com.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Signposts (API 19, os_signpost): intervals and events for performance
 * tools. Minkar has no tool that records them yet: os_signpost_enabled
 * says no, and the emitting functions do nothing, as on the reference
 * system when nobody is recording. The IDs are real.
 *
 * Installed as <os/signpost.h>.
 */

#ifndef __OS_SIGNPOST_H__
#define __OS_SIGNPOST_H__

#include <Availability.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <os/log.h>

__BEGIN_DECLS

typedef uint64_t os_signpost_id_t;

#define OS_SIGNPOST_ID_NULL		((os_signpost_id_t)0)
#define OS_SIGNPOST_ID_INVALID		((os_signpost_id_t)~0)
#define OS_SIGNPOST_ID_EXCLUSIVE	((os_signpost_id_t)0xeeeeb0b5b2b2eeee)

typedef uint8_t os_signpost_type_t;
#define OS_SIGNPOST_EVENT		((os_signpost_type_t)0x00)
#define OS_SIGNPOST_INTERVAL_BEGIN	((os_signpost_type_t)0x01)
#define OS_SIGNPOST_INTERVAL_END	((os_signpost_type_t)0x02)

/* A new ID, unique in the process. */
__OSX_AVAILABLE(10.14)
os_signpost_id_t os_signpost_id_generate(os_log_t log);

/* An ID derived from a pointer, the same for the same pointer. */
__OSX_AVAILABLE(10.14)
os_signpost_id_t os_signpost_id_make_with_pointer(os_log_t log, const void *ptr);

/* Whether signposts on this log are recorded: no tool records them. */
__OSX_AVAILABLE(10.14)
bool os_signpost_enabled(os_log_t log);

__OSX_AVAILABLE(10.14)
void os_signpost_event_emit(os_log_t log, os_signpost_id_t spid, const char *name,
    const char *format, ...);

__OSX_AVAILABLE(10.14)
void os_signpost_interval_begin(os_log_t log, os_signpost_id_t spid, const char *name,
    const char *format, ...);

__OSX_AVAILABLE(10.14)
void os_signpost_interval_end(os_log_t log, os_signpost_id_t spid, const char *name,
    const char *format, ...);

void __os_signpost_emit_impl(os_log_t log, os_signpost_type_t type, os_signpost_id_t spid,
    const char *name, const char *format, ...);
void __os_signpost_emit_with_name_impl(os_log_t log, os_signpost_type_t type,
    os_signpost_id_t spid, const char *name, const char *format, ...);
void __os_signpost_emit_unreliably_impl(os_log_t log, os_signpost_type_t type,
    os_signpost_id_t spid, const char *name, const char *format, ...);

__END_DECLS

#endif /* __OS_SIGNPOST_H__ */
