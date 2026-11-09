/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * LaunchServices: which applications open an item (the Cocoa API's
 * LSInfo.h). Answered by launchservicesd's database.
 */

#ifndef LAUNCHSERVICES_LSINFO_H
#define LAUNCHSERVICES_LSINFO_H

#include <CoreFoundation/CoreFoundation.h>

CF_ASSUME_NONNULL_BEGIN
CF_EXTERN_C_BEGIN

/* Result codes. */
enum {
	kLSAppInTrashErr			= -10660,
	kLSUnknownErr				= -10810,
	kLSNotAnApplicationErr			= -10811,
	kLSDataUnavailableErr			= -10813,
	kLSApplicationNotFoundErr		= -10814,
	kLSDataErr				= -10817,
	kLSLaunchInProgressErr			= -10818,
	kLSServerCommunicationErr		= -10822,
	kLSCannotSetInfoErr			= -10823,
	kLSIncompatibleSystemVersionErr		= -10825,
	kLSNoLaunchPermissionErr		= -10826,
	kLSNoExecutableErr			= -10827,
	kLSMultipleSessionsNotSupportedErr	= -10829,
};

typedef CF_OPTIONS(UInt32, LSRolesMask) {
	kLSRolesNone	= 0x00000001,
	kLSRolesViewer	= 0x00000002,
	kLSRolesEditor	= 0x00000004,
	kLSRolesShell	= 0x00000008,
	kLSRolesAll	= 0xFFFFFFFF,
};

/* The application that opens an item by default, or NULL. */
CFURLRef _Nullable LSCopyDefaultApplicationURLForURL(CFURLRef inURL, LSRolesMask inRoleMask,
    CFErrorRef _Nullable *_Nullable outError);
/* All the applications that open an item, best first, or NULL. */
CFArrayRef _Nullable LSCopyApplicationURLsForURL(CFURLRef inURL, LSRolesMask inRoleMask);
/* The applications with a bundle identifier, or NULL. */
CFArrayRef _Nullable LSCopyApplicationURLsForBundleIdentifier(CFStringRef inBundleIdentifier,
    CFErrorRef _Nullable *_Nullable outError);
/* Reads an application bundle again, or adds one from another folder. */
OSStatus LSRegisterURL(CFURLRef inURL, Boolean inUpdate);

/* By type identifier: the bundle identifiers of the applications that
 * open a type in a role, best first; the best one; its URL. */
CFArrayRef _Nullable LSCopyAllRoleHandlersForContentType(CFStringRef inContentType,
    LSRolesMask inRole) CF_RETURNS_RETAINED;
CFStringRef _Nullable LSCopyDefaultRoleHandlerForContentType(CFStringRef inContentType,
    LSRolesMask inRole) CF_RETURNS_RETAINED;
CFURLRef _Nullable LSCopyDefaultApplicationURLForContentType(CFStringRef inContentType,
    LSRolesMask inRoleMask, CFErrorRef _Nullable *_Nullable outError) CF_RETURNS_RETAINED;
/* By URL scheme (CFBundleURLTypes): the bundle identifiers of the
 * applications that open it, and the preferred one. */
CFArrayRef _Nullable LSCopyAllHandlersForURLScheme(CFStringRef inURLScheme) CF_RETURNS_RETAINED;
CFStringRef _Nullable LSCopyDefaultHandlerForURLScheme(CFStringRef inURLScheme) CF_RETURNS_RETAINED;
/* The user's default application for a URL scheme or a content type (by
   bundle identifier), kept in the user's preferences. */
OSStatus LSSetDefaultHandlerForURLScheme(CFStringRef inURLScheme, CFStringRef inHandlerBundleID);
OSStatus LSSetDefaultRoleHandlerForContentType(CFStringRef inContentType, LSRolesMask inRole,
    CFStringRef inHandlerBundleID);

CF_EXTERN_C_END
CF_ASSUME_NONNULL_END

#endif
