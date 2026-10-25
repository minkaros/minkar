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
 * NSSystemDirectories: the C search-path API that predates <sysdir.h>.
 * Deprecated since macOS 10.12 and still shipped by macOS 10.15; the
 * values are those of <sysdir.h>, and the functions forward to it.
 */

#ifndef __NS_SYSTEM_DIRECTORIES_H__
#define __NS_SYSTEM_DIRECTORIES_H__

#include <sys/cdefs.h>
#include <Availability.h>

typedef enum {
	NSApplicationDirectory = 1,		/* Applications */
	NSDemoApplicationDirectory = 2,		/* Applications/Demos */
	NSDeveloperApplicationDirectory = 3,	/* Developer/Applications */
	NSAdminApplicationDirectory = 4,	/* Applications/Utilities */
	NSLibraryDirectory = 5,			/* Library */
	NSDeveloperDirectory = 6,		/* Developer */
	NSUserDirectory = 7,			/* Users */
	NSDocumentationDirectory = 8,		/* Library/Documentation */
	NSDocumentDirectory = 9,		/* Documents */
	NSCoreServiceDirectory = 10,		/* Library/CoreServices */
	NSAutosavedInformationDirectory = 11,	/* Library/Autosave Information */
	NSDesktopDirectory = 12,		/* Desktop */
	NSCachesDirectory = 13,			/* Library/Caches */
	NSApplicationSupportDirectory = 14,	/* Library/Application Support */
	NSDownloadsDirectory = 15,		/* Downloads */
	NSInputMethodsDirectory = 16,		/* Library/Input Methods */
	NSMoviesDirectory = 17,			/* Movies */
	NSMusicDirectory = 18,			/* Music */
	NSPicturesDirectory = 19,		/* Pictures */
	NSPrinterDescriptionDirectory = 20,	/* System/Library/Printers/PPDs */
	NSSharedPublicDirectory = 21,		/* Public */
	NSPreferencePanesDirectory = 22,	/* Library/PreferencePanes */
	NSAllApplicationsDirectory = 100,	/* every Applications directory */
	NSAllLibrariesDirectory = 101		/* every Library directory */
} NSSearchPathDirectory;

typedef enum {
	NSUserDomainMask = 1,		/* ~ */
	NSLocalDomainMask = 2,		/* / */
	NSNetworkDomainMask = 4,	/* /Network */
	NSSystemDomainMask = 8,		/* /System */
	NSAllDomainsMask = 0x0ffff	/* all of the above and future domains */
} NSSearchPathDomainMask;

typedef unsigned int NSSearchPathEnumerationState;

__BEGIN_DECLS

/*
 * Start enumerating the paths of dir in the domains of domainMask; pass
 * the state to NSGetNextSearchPathEnumeration() until it returns 0. path
 * must hold PATH_MAX bytes.
 */
NSSearchPathEnumerationState NSStartSearchPathEnumeration(NSSearchPathDirectory dir,
    NSSearchPathDomainMask domainMask)
    __API_DEPRECATED_WITH_REPLACEMENT("sysdir_start_search_path_enumeration", macos(10.0, 10.12));

NSSearchPathEnumerationState NSGetNextSearchPathEnumeration(NSSearchPathEnumerationState state,
    char *path)
    __API_DEPRECATED_WITH_REPLACEMENT("sysdir_get_next_search_path_enumeration", macos(10.0, 10.12));

__END_DECLS

#endif /* __NS_SYSTEM_DIRECTORIES_H__ */
