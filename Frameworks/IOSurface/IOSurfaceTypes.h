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

#ifndef IOSURFACE_IOSURFACETYPES_H
#define IOSURFACE_IOSURFACETYPES_H

#include <stdint.h>
#include <mach/kern_return.h>

typedef uint32_t IOSurfaceID;

typedef uint32_t IOSurfaceLockOptions;
enum {
	kIOSurfaceLockReadOnly	= 0x00000001,
	kIOSurfaceLockAvoidSync	= 0x00000002,
};

typedef uint32_t IOSurfacePurgeabilityState;
enum {
	kIOSurfacePurgeableNonVolatile	= 0,
	kIOSurfacePurgeableVolatile	= 1,
	kIOSurfacePurgeableEmpty	= 2,
	kIOSurfacePurgeableKeepCurrent	= 3,
};

/* Cache modes, for kIOSurfaceCacheMode. (IOKit's kIOMap*Cache are its
 * own: IOKit/IOTypes.h, with the mode shifted by 8.) */
enum {
	kIOSurfaceDefaultCache		= 0,
	kIOSurfaceInhibitCache		= 1,
	kIOSurfaceWriteThruCache	= 2,
	kIOSurfaceCopybackCache		= 3,
	kIOSurfaceWriteCombineCache	= 4,
	kIOSurfaceCopybackInnerCache	= 5,
};

/* The same modes as IOKit's mapping options carry them. */
enum {
	kIOSurfaceMapCacheShift			= 8,
	kIOSurfaceMapDefaultCache		= kIOSurfaceDefaultCache << kIOSurfaceMapCacheShift,
	kIOSurfaceMapInhibitCache		= kIOSurfaceInhibitCache << kIOSurfaceMapCacheShift,
	kIOSurfaceMapWriteThruCache		= kIOSurfaceWriteThruCache << kIOSurfaceMapCacheShift,
	kIOSurfaceMapCopybackCache		= kIOSurfaceCopybackCache << kIOSurfaceMapCacheShift,
	kIOSurfaceMapWriteCombineCache		= kIOSurfaceWriteCombineCache << kIOSurfaceMapCacheShift,
	kIOSurfaceMapCopybackInnerCache		= kIOSurfaceCopybackInnerCache << kIOSurfaceMapCacheShift,
};

/* Components of a plane, for IOSurfaceGetSubsampling and the like. */
typedef int32_t IOSurfaceSubsampling;
enum {
	kIOSurfaceSubsamplingUnknown	= 0,
	kIOSurfaceSubsamplingNone	= 1,
	kIOSurfaceSubsampling422	= 2,
	kIOSurfaceSubsampling420	= 3,
	kIOSurfaceSubsampling411	= 4,
};

#endif
