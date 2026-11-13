/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The older LaunchServices calls, on FSRefs, MIME types and extensions:
 * each turned into its URL and type identifier and answered by the
 * calls on those (LSOpen.c, UTType.c).
 */

#include "LSInfo.h"
#include "LSOpen.h"
#include "UTType.h"
#include "UTCoreTypes.h"
#include "../Files.h"	/* FSRef: CoreServices, above this framework */

/* The application for a MIME type, by its type identifier. */
OSStatus LSCopyApplicationForMIMEType(CFStringRef inMIMEType, LSRolesMask inRoleMask, CFURLRef *outAppURL) {
	if (inMIMEType == NULL || outAppURL == NULL)
		return -50;	/* paramErr */
	*outAppURL = NULL;
	CFStringRef type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, inMIMEType, NULL);
	if (type == NULL)
		return kLSApplicationNotFoundErr;
	*outAppURL = LSCopyDefaultApplicationURLForContentType(type, inRoleMask, NULL);
	CFRelease(type);
	return *outAppURL != NULL ? noErr : kLSApplicationNotFoundErr;
}

/* The application for an extension (types and creators are not used: no
   file has them here). */
OSStatus LSGetApplicationForInfo(OSType inType, OSType inCreator, CFStringRef inExtension, LSRolesMask inRoleMask,
    struct FSRef *outAppRef, CFURLRef *outAppURL) {
	if (outAppURL != NULL)
		*outAppURL = NULL;
	if (inExtension == NULL)
		return kLSApplicationNotFoundErr;
	CFStringRef type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, inExtension, NULL);
	if (type == NULL)
		return kLSApplicationNotFoundErr;
	CFURLRef app = LSCopyDefaultApplicationURLForContentType(type, inRoleMask, NULL);
	CFRelease(type);
	if (app == NULL)
		return kLSApplicationNotFoundErr;
	if (outAppRef != NULL && !CFURLGetFSRef(app, outAppRef)) {
		CFRelease(app);
		return kLSApplicationNotFoundErr;
	}
	if (outAppURL != NULL)
		*outAppURL = app;
	else
		CFRelease(app);
	return noErr;
}

static CFTypeRef copyResource(CFURLRef url, CFStringRef key) {
	CFTypeRef value = NULL;
	if (!CFURLCopyResourcePropertyForKey(url, key, &value, NULL))
		return NULL;
	return value;
}

/* An item's attribute (LSInfo.h's kLSItem keys), from its URL's resource
   properties and its type. */
OSStatus LSCopyItemAttribute(const struct FSRef *inItem, LSRolesMask inRoles, CFStringRef inAttributeName,
    CFTypeRef *outValue) {
	if (inItem == NULL || inAttributeName == NULL || outValue == NULL)
		return -50;	/* paramErr */
	*outValue = NULL;
	CFURLRef url = CFURLCreateFromFSRef(NULL, inItem);
	if (url == NULL)
		return -43;	/* fnfErr */

	if (CFEqual(inAttributeName, kLSItemContentType)) {
		*outValue = copyResource(url, kCFURLTypeIdentifierKey);
	} else if (CFEqual(inAttributeName, kLSItemExtension)) {
		CFStringRef extension = CFURLCopyPathExtension(url);
		*outValue = extension != NULL ? extension : CFSTR("");
		if (extension == NULL)
			CFRetain(*outValue);
	} else if (CFEqual(inAttributeName, kLSItemDisplayName)) {
		*outValue = copyResource(url, kCFURLLocalizedNameKey);
	} else if (CFEqual(inAttributeName, kLSItemDisplayKind)) {
		*outValue = copyResource(url, kCFURLLocalizedTypeDescriptionKey);
	} else if (CFEqual(inAttributeName, kLSItemIsInvisible)) {
		*outValue = copyResource(url, kCFURLIsHiddenKey);
	} else if (CFEqual(inAttributeName, kLSItemExtensionIsHidden)) {
		*outValue = copyResource(url, kCFURLHasHiddenExtensionKey);
	} else if (CFEqual(inAttributeName, kLSItemQuarantineProperties)) {
		*outValue = copyResource(url, kCFURLQuarantinePropertiesKey);
	} else if (CFEqual(inAttributeName, kLSItemFileType) || CFEqual(inAttributeName, kLSItemFileCreator)) {
		/* Files have no type or creator codes: '\0\0\0\0', as a string. */
		*outValue = CFStringCreateWithBytes(NULL, (const UInt8 *)"\0\0\0\0", 4, kCFStringEncodingMacRoman, false);
	} else if (CFEqual(inAttributeName, kLSItemRoleHandlerDisplayName)) {
		CFURLRef app = LSCopyDefaultApplicationURLForURL(url, inRoles, NULL);
		if (app != NULL) {
			*outValue = copyResource(app, kCFURLLocalizedNameKey);
			CFRelease(app);
		}
	} else {
		CFRelease(url);
		return kLSUnknownErr;
	}
	CFRelease(url);
	return *outValue != NULL ? noErr : kLSDataUnavailableErr;
}

/* Opening by FSRefs: the same as by URLs. */
OSStatus LSOpenFromRefSpec(const LSLaunchFSRefSpec *inLaunchSpec, struct FSRef *outLaunchedRef) {
	if (inLaunchSpec == NULL)
		return -50;	/* paramErr */
	CFMutableArrayRef items = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (ItemCount i = 0; i < inLaunchSpec->numDocs && inLaunchSpec->itemRefs != NULL; i++) {
		CFURLRef url = CFURLCreateFromFSRef(NULL, &inLaunchSpec->itemRefs[i]);
		if (url == NULL) {
			CFRelease(items);
			return -43;	/* fnfErr */
		}
		CFArrayAppendValue(items, url);
		CFRelease(url);
	}
	CFURLRef app = inLaunchSpec->appRef != NULL ? CFURLCreateFromFSRef(NULL, inLaunchSpec->appRef) : NULL;
	LSLaunchURLSpec spec = {
		.appURL = app,
		.itemURLs = CFArrayGetCount(items) > 0 ? items : NULL,
		.passThruParams = inLaunchSpec->passThruParams,
		.launchFlags = inLaunchSpec->launchFlags,
		.asyncRefCon = inLaunchSpec->asyncRefCon,
	};
	CFURLRef launched = NULL;
	OSStatus status = LSOpenFromURLSpec(&spec, outLaunchedRef != NULL ? &launched : NULL);
	if (status == noErr && outLaunchedRef != NULL && launched != NULL && !CFURLGetFSRef(launched, outLaunchedRef))
		status = kLSApplicationNotFoundErr;
	if (launched != NULL)
		CFRelease(launched);
	if (app != NULL)
		CFRelease(app);
	CFRelease(items);
	return status;
}
