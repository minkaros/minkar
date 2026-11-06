/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Uniform type identifiers (the Cocoa API's UTType.h): the types
 * launchservicesd knows, its own and those applications declare
 * (UTExportedTypeDeclarations, UTImportedTypeDeclarations), with their
 * tags (filename extensions, MIME types), conformance and descriptions.
 * Identifiers compare without regard to case. A tag no type has gets a
 * dynamic identifier ("dyn." and the tag, encoded).
 */

#ifndef LAUNCHSERVICES_UTTYPE_H
#define LAUNCHSERVICES_UTTYPE_H

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

CF_ASSUME_NONNULL_BEGIN
CF_EXTERN_C_BEGIN

extern const CFStringRef kUTTagClassFilenameExtension;
extern const CFStringRef kUTTagClassMIMEType;
extern const CFStringRef kUTTagClassNSPboardType;
extern const CFStringRef kUTTagClassOSType;
extern const CFStringRef kUTExportedTypeDeclarationsKey;
extern const CFStringRef kUTImportedTypeDeclarationsKey;
extern const CFStringRef kUTTypeIdentifierKey;
extern const CFStringRef kUTTypeTagSpecificationKey;
extern const CFStringRef kUTTypeConformsToKey;
extern const CFStringRef kUTTypeDescriptionKey;
extern const CFStringRef kUTTypeIconFileKey;
extern const CFStringRef kUTTypeReferenceURLKey;
extern const CFStringRef kUTTypeVersionKey;

/* The type with a tag (first: the system's own), or a dynamic one. */
CFStringRef _Nullable UTTypeCreatePreferredIdentifierForTag(CFStringRef inTagClass, CFStringRef inTag,
    CFStringRef _Nullable inConformingToUTI) CF_RETURNS_RETAINED;
/* Every type with a tag, or a dynamic one. */
CFArrayRef _Nullable UTTypeCreateAllIdentifiersForTag(CFStringRef inTagClass, CFStringRef inTag,
    CFStringRef _Nullable inConformingToUTI) CF_RETURNS_RETAINED;
/* A type's first tag of a class, or all of them. */
CFStringRef _Nullable UTTypeCopyPreferredTagWithClass(CFStringRef inUTI, CFStringRef inTagClass)
    CF_RETURNS_RETAINED;
CFArrayRef _Nullable UTTypeCopyAllTagsWithClass(CFStringRef inUTI, CFStringRef inTagClass)
    CF_RETURNS_RETAINED;
Boolean UTTypeEqual(CFStringRef inUTI1, CFStringRef inUTI2);
Boolean UTTypeConformsTo(CFStringRef inUTI, CFStringRef inConformsToUTI);
CFStringRef _Nullable UTTypeCopyDescription(CFStringRef inUTI) CF_RETURNS_RETAINED;
Boolean UTTypeIsDeclared(CFStringRef inUTI);
Boolean UTTypeIsDynamic(CFStringRef inUTI);
/* The declaration as Info.plist writes it (UTTypeIdentifier,
 * UTTypeConformsTo, UTTypeTagSpecification, UTTypeDescription). */
CFDictionaryRef _Nullable UTTypeCopyDeclaration(CFStringRef inUTI) CF_RETURNS_RETAINED;
CFURLRef _Nullable UTTypeCopyDeclaringBundleURL(CFStringRef inUTI) CF_RETURNS_RETAINED;
CFStringRef UTCreateStringForOSType(OSType inOSType) CF_RETURNS_RETAINED;
OSType UTGetOSTypeFromString(CFStringRef inString);

CF_EXTERN_C_END
CF_ASSUME_NONNULL_END

#endif
