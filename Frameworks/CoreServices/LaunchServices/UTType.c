/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Uniform type identifiers (UTType.h), answered by launchservicesd's
 * database (LaunchServices interface version 5).
 *
 * A dynamic identifier names a tag no type has: "dyn." then the tag
 * class's letter (e: filename extension, m: MIME type, p: pasteboard
 * type, o: OSType) and the tag in hexadecimal. It conforms to
 * public.data and public.item, and gives its tag back.
 */

#include <ctype.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <string.h>
#include "UTType.h"
#include "UTCoreTypes.h"
#include "LSSession.h"

#define MAX_STRING	1024

static Boolean
copyCString(CFStringRef string, char buffer[MAX_STRING])
{
	return string != NULL && CFGetTypeID(string) == CFStringGetTypeID() &&
	    CFStringGetCString(string, buffer, MAX_STRING, kCFStringEncodingUTF8);
}

static CFStringRef
createString(const char *bytes, size_t length)
{
	return CFStringCreateWithBytes(NULL, (const UInt8 *)bytes, (CFIndex)length,
	    kCFStringEncodingUTF8, false);
}

/* The daemon's tag classes: 0, filename extensions; 1, MIME types. */
static int
daemonClass(CFStringRef tagClass)
{
	if (CFEqual(tagClass, kUTTagClassFilenameExtension))
		return 0;
	if (CFEqual(tagClass, kUTTagClassMIMEType))
		return 1;
	return -1;
}

static char
dynamicLetter(CFStringRef tagClass)
{
	if (CFEqual(tagClass, kUTTagClassFilenameExtension))
		return 'e';
	if (CFEqual(tagClass, kUTTagClassMIMEType))
		return 'm';
	if (CFEqual(tagClass, kUTTagClassNSPboardType))
		return 'p';
	if (CFEqual(tagClass, kUTTagClassOSType))
		return 'o';
	return 0;
}

static CFStringRef
createDynamic(CFStringRef tagClass, const char *tag)
{
	char letter = dynamicLetter(tagClass), id[2 * MAX_STRING + 8];
	static const char hex[] = "0123456789abcdef";
	size_t n = 0;

	if (letter == 0)
		return NULL;
	n += (size_t)snprintf(id, sizeof(id), "dyn.%c", letter);
	for (const unsigned char *p = (const unsigned char *)tag; *p && n + 2 < sizeof(id); p++) {
		id[n++] = hex[*p >> 4];
		id[n++] = hex[*p & 15];
	}
	id[n] = '\0';
	return createString(id, n);
}

/* A dynamic identifier's tag, if it is of that class. */
static CFStringRef
createDynamicTag(const char *uti, CFStringRef tagClass)
{
	char tag[MAX_STRING];
	size_t n = 0, length = strlen(uti);

	if (length < 5 || strncasecmp(uti, "dyn.", 4) != 0 || tolower(uti[4]) != dynamicLetter(tagClass))
		return NULL;
	for (size_t i = 5; i + 1 < length && n < sizeof(tag) - 1; i += 2) {
		char pair[3] = { uti[i], uti[i + 1], 0 };
		tag[n++] = (char)strtol(pair, NULL, 16);
	}
	return createString(tag, n);
}

Boolean
UTTypeIsDynamic(CFStringRef inUTI)
{
	char uti[MAX_STRING];

	return copyCString(inUTI, uti) && strncasecmp(uti, "dyn.", 4) == 0;
}

/* The types with a tag, one per line, or NULL. */
static char *
copyTypesForTag(CFStringRef tagClass, CFStringRef tag, CFStringRef conforming)
{
	char t[MAX_STRING], c[MAX_STRING], *text = NULL;
	int cls = tagClass ? daemonClass(tagClass) : -1;

	if (cls < 0 || !copyCString(tag, t))
		return NULL;
	if (conforming != NULL && !copyCString(conforming, c))
		return NULL;
	if (LSSessionCopyTypesForTag((uint32_t)cls, t, conforming ? c : NULL, &text) != 0) {
		free(text);
		return NULL;
	}
	return text;
}

CFStringRef
UTTypeCreatePreferredIdentifierForTag(CFStringRef inTagClass, CFStringRef inTag,
    CFStringRef inConformingToUTI)
{
	char *text = copyTypesForTag(inTagClass, inTag, inConformingToUTI), tag[MAX_STRING];

	if (text != NULL) {
		char *end = strchr(text, '\n');
		CFStringRef uti = createString(text, end ? (size_t)(end - text) : strlen(text));
		free(text);
		return uti;
	}
	return inTagClass && copyCString(inTag, tag) ? createDynamic(inTagClass, tag) : NULL;
}

CFArrayRef
UTTypeCreateAllIdentifiersForTag(CFStringRef inTagClass, CFStringRef inTag,
    CFStringRef inConformingToUTI)
{
	char *text = copyTypesForTag(inTagClass, inTag, inConformingToUTI), tag[MAX_STRING];
	CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (text != NULL) {
		for (char *line = text; *line != '\0';) {
			char *end = strchr(line, '\n');
			size_t n = end ? (size_t)(end - line) : strlen(line);
			if (n > 0) {
				CFStringRef uti = createString(line, n);
				CFArrayAppendValue(list, uti);
				CFRelease(uti);
			}
			if (end == NULL)
				break;
			line = end + 1;
		}
		free(text);
	} else if (inTagClass && copyCString(inTag, tag)) {
		CFStringRef uti = createDynamic(inTagClass, tag);
		if (uti != NULL) {
			CFArrayAppendValue(list, uti);
			CFRelease(uti);
		}
	}
	return list;
}

/* A known type's declaration lines: conforms to, extensions, MIME types,
 * description. NULL for one the daemon does not know. */
static char *
copyDeclarationText(CFStringRef inUTI, char *lines[4])
{
	char uti[MAX_STRING], *text = NULL;

	if (!copyCString(inUTI, uti) || LSSessionCopyDeclaration(uti, &text) != 0) {
		free(text);
		return NULL;
	}
	char *p = text;
	for (int i = 0; i < 4; i++) {
		lines[i] = p;
		char *end = strchr(p, '\n');
		if (end == NULL) {
			p += strlen(p);
			continue;
		}
		*end = '\0';
		p = end + 1;
	}
	return text;
}

/* Space-separated words into an array of strings. */
static CFArrayRef
createWords(const char *words)
{
	CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	while (*words) {
		const char *end = strchr(words, ' ');
		size_t n = end ? (size_t)(end - words) : strlen(words);
		if (n > 0) {
			CFStringRef word = createString(words, n);
			CFArrayAppendValue(list, word);
			CFRelease(word);
		}
		words += n + (end != NULL);
	}
	return list;
}

CFArrayRef
UTTypeCopyAllTagsWithClass(CFStringRef inUTI, CFStringRef inTagClass)
{
	char *lines[4], uti[MAX_STRING];
	int cls = inTagClass ? daemonClass(inTagClass) : -1;
	char *text;

	if (inTagClass != NULL && copyCString(inUTI, uti)) {
		CFStringRef tag = createDynamicTag(uti, inTagClass);
		if (tag != NULL) {
			CFArrayRef list = CFArrayCreate(NULL, (const void **)&tag, 1, &kCFTypeArrayCallBacks);
			CFRelease(tag);
			return list;
		}
	}
	if (cls < 0 || (text = copyDeclarationText(inUTI, lines)) == NULL)
		return NULL;
	CFArrayRef list = createWords(lines[1 + cls]);
	free(text);
	if (CFArrayGetCount(list) == 0) {
		CFRelease(list);
		return NULL;
	}
	return list;
}

CFStringRef
UTTypeCopyPreferredTagWithClass(CFStringRef inUTI, CFStringRef inTagClass)
{
	CFArrayRef tags = UTTypeCopyAllTagsWithClass(inUTI, inTagClass);

	if (tags == NULL)
		return NULL;
	CFStringRef tag = CFRetain(CFArrayGetValueAtIndex(tags, 0));
	CFRelease(tags);
	return tag;
}

Boolean
UTTypeEqual(CFStringRef inUTI1, CFStringRef inUTI2)
{
	if (inUTI1 == NULL || inUTI2 == NULL)
		return false;
	return CFStringCompare(inUTI1, inUTI2, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
}

Boolean
UTTypeConformsTo(CFStringRef inUTI, CFStringRef inConformsToUTI)
{
	char uti[MAX_STRING], other[MAX_STRING];

	if (UTTypeEqual(inUTI, inConformsToUTI))
		return true;
	if (!copyCString(inUTI, uti) || !copyCString(inConformsToUTI, other))
		return false;
	if (strncasecmp(uti, "dyn.", 4) == 0)
		return strcasecmp(other, "public.data") == 0 || strcasecmp(other, "public.item") == 0;
	return LSSessionConformsTo(uti, other) == 1;
}

CFStringRef
UTTypeCopyDescription(CFStringRef inUTI)
{
	char *lines[4], *text = copyDeclarationText(inUTI, lines);

	if (text == NULL)
		return NULL;
	CFStringRef description = lines[3][0] ? createString(lines[3], strlen(lines[3])) : NULL;
	free(text);
	return description;
}

Boolean
UTTypeIsDeclared(CFStringRef inUTI)
{
	char *lines[4], *text = copyDeclarationText(inUTI, lines);

	free(text);
	return text != NULL;
}

CFDictionaryRef
UTTypeCopyDeclaration(CFStringRef inUTI)
{
	char *lines[4], *text = copyDeclarationText(inUTI, lines);

	if (text == NULL)
		return NULL;
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFMutableDictionaryRef tags = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFArrayRef conforms = createWords(lines[0]), extensions = createWords(lines[1]);
	CFArrayRef mime = createWords(lines[2]);

	CFDictionarySetValue(d, kUTTypeIdentifierKey, inUTI);
	if (CFArrayGetCount(conforms) > 0)
		CFDictionarySetValue(d, kUTTypeConformsToKey, conforms);
	if (CFArrayGetCount(extensions) > 0)
		CFDictionarySetValue(tags, kUTTagClassFilenameExtension, extensions);
	if (CFArrayGetCount(mime) > 0)
		CFDictionarySetValue(tags, kUTTagClassMIMEType, mime);
	if (CFDictionaryGetCount(tags) > 0)
		CFDictionarySetValue(d, kUTTypeTagSpecificationKey, tags);
	if (lines[3][0]) {
		CFStringRef description = createString(lines[3], strlen(lines[3]));
		CFDictionarySetValue(d, kUTTypeDescriptionKey, description);
		CFRelease(description);
	}
	CFRelease(conforms);
	CFRelease(extensions);
	CFRelease(mime);
	CFRelease(tags);
	free(text);
	return d;
}

/* SHORTCUT(W0): the daemon does not say which bundle declared a type yet;
 * the system's own types have none. */
CFURLRef
UTTypeCopyDeclaringBundleURL(CFStringRef inUTI)
{
	(void)inUTI;
	return NULL;
}

CFStringRef
UTCreateStringForOSType(OSType inOSType)
{
	char s[4] = { (char)(inOSType >> 24), (char)(inOSType >> 16), (char)(inOSType >> 8),
	    (char)inOSType };

	return CFStringCreateWithBytes(NULL, (const UInt8 *)s, 4, kCFStringEncodingMacRoman, false);
}

OSType
UTGetOSTypeFromString(CFStringRef inString)
{
	UInt8 s[4] = { 0, 0, 0, 0 };
	CFIndex used = 0;

	if (inString == NULL || CFStringGetLength(inString) != 4)
		return 0;
	CFStringGetBytes(inString, CFRangeMake(0, 4), kCFStringEncodingMacRoman, 0, false, s, 4, &used);
	if (used != 4)
		return 0;
	return ((OSType)s[0] << 24) | ((OSType)s[1] << 16) | ((OSType)s[2] << 8) | s[3];
}
