/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * LaunchServices: opening items and launching applications (the Cocoa
 * API's LSOpen.h). An application gets its documents on its session
 * port (application:openURLs: and the like in AppKit): at once when it
 * runs, else after its launch.
 */

#ifndef LAUNCHSERVICES_LSOPEN_H
#define LAUNCHSERVICES_LSOPEN_H

#include <CoreFoundation/CoreFoundation.h>
#include "LSInfo.h"

CF_ASSUME_NONNULL_BEGIN
CF_EXTERN_C_BEGIN

typedef CF_OPTIONS(UInt32, LSLaunchFlags) {
	kLSLaunchDefaults		= 0x00000001,
	kLSLaunchAndPrint		= 0x00000002,
	kLSLaunchAndDisplayErrors	= 0x00000040,
	kLSLaunchDontAddToRecents	= 0x00000100,
	kLSLaunchDontSwitch		= 0x00000200,
	kLSLaunchAsync			= 0x00010000,
	kLSLaunchNewInstance		= 0x00080000,
	kLSLaunchAndHide		= 0x00100000,
	kLSLaunchAndHideOthers		= 0x00200000,
};

struct AEDesc;

typedef struct LSLaunchURLSpec {
	CFURLRef _Nullable		appURL;		/* NULL: each item's own */
	CFArrayRef _Nullable		itemURLs;
	const struct AEDesc *_Nullable	passThruParams;	/* not used */
	LSLaunchFlags			launchFlags;
	void *_Nullable			asyncRefCon;
} LSLaunchURLSpec;

struct FSRef;

/* The same as LSLaunchURLSpec, with FSRefs. */
typedef struct LSLaunchFSRefSpec {
	const struct FSRef *_Nullable	appRef;
	ItemCount			numDocs;
	const struct FSRef *_Nullable	itemRefs;
	const struct AEDesc *_Nullable	passThruParams;
	LSLaunchFlags			launchFlags;
	void *_Nullable			asyncRefCon;
} LSLaunchFSRefSpec;

OSStatus LSOpenFromRefSpec(const LSLaunchFSRefSpec *inLaunchSpec, struct FSRef *_Nullable outLaunchedRef);

/* Opens an item with its application (an application: launches it). */
OSStatus LSOpenCFURLRef(CFURLRef inURL, CFURLRef _Nullable *_Nullable outLaunchedURL);
OSStatus LSOpenFromURLSpec(const LSLaunchURLSpec *inLaunchSpec,
    CFURLRef _Nullable *_Nullable outLaunchedURL);

/* Minkar: as LSOpenFromURLSpec, and an application it launches gets these
 * arguments (CFStrings) after its path, as `open --args` gives them. An
 * application already running is not launched again, and gets none. */
OSStatus LSOpenFromURLSpecWithArguments(const LSLaunchURLSpec *inLaunchSpec,
    CFArrayRef _Nullable inArguments, CFURLRef _Nullable *_Nullable outLaunchedURL);

/* Minkar: shows items in Files, the file manager, each selected in its
 * folder's window (what `open -R` and NSWorkspace's reveal methods do). */
OSStatus LSRevealInFiles(CFArrayRef inItemURLs);

CF_EXTERN_C_END
CF_ASSUME_NONNULL_END

#endif
