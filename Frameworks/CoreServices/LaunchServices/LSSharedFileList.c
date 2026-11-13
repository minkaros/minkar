/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Shared file lists, kept in the user's preferences (the domain
 * org.minkar.sharedfilelists, a key per list): each item a dictionary
 * with its URL, display name, ID and properties. A snapshot is read
 * afresh, so every process sees the others' changes; the recent lists
 * keep their newest items only.
 */

#include "LSSharedFileList.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>

static const CFStringRef kDomain = CFSTR("org.minkar.sharedfilelists");
static const CFStringRef kURLKey = CFSTR("URL");
static const CFStringRef kNameKey = CFSTR("Name");
static const CFStringRef kIDKey = CFSTR("ID");
static const CFStringRef kPropertiesKey = CFSTR("Properties");
static const CFStringRef kSeedKey = CFSTR("Seed");

/* MacErrors.h's codes (CoreServices, above this framework). */
enum { kParamErr = -50, kFileNotFoundErr = -43 };

/* How many items a recent list keeps. */
#define RECENT_LIMIT 10

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

struct OpaqueLSSharedFileListRef {
	CFRuntimeBase base;
	CFStringRef type;
};

struct OpaqueLSSharedFileListItemRef {
	CFRuntimeBase base;
	UInt32 itemID;
	CFURLRef url;
	CFStringRef name;
	CFDictionaryRef properties;
};

static void listFinalize(CFTypeRef cf) {
	CFRelease(((LSSharedFileListRef)cf)->type);
}

static void itemFinalize(CFTypeRef cf) {
	LSSharedFileListItemRef item = (LSSharedFileListItemRef)cf;
	if (item->url != NULL)
		CFRelease(item->url);
	if (item->name != NULL)
		CFRelease(item->name);
	if (item->properties != NULL)
		CFRelease(item->properties);
}

static Boolean itemEqual(CFTypeRef a, CFTypeRef b) {
	return ((LSSharedFileListItemRef)a)->itemID == ((LSSharedFileListItemRef)b)->itemID;
}

static CFHashCode itemHash(CFTypeRef cf) {
	return ((LSSharedFileListItemRef)cf)->itemID;
}

static const CFRuntimeClass listClass = {
	0, "LSSharedFileList", NULL, NULL, listFinalize, NULL, NULL, NULL, NULL
};
static const CFRuntimeClass itemClass = {
	0, "LSSharedFileListItem", NULL, NULL, itemFinalize, itemEqual, itemHash, NULL, NULL
};

CFTypeID LSSharedFileListGetTypeID(void) {
	static CFTypeID typeID;
	static dispatch_once_t once;
	dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&listClass); });
	return typeID;
}

CFTypeID LSSharedFileListItemGetTypeID(void) {
	static CFTypeID typeID;
	static dispatch_once_t once;
	dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&itemClass); });
	return typeID;
}

static bool isRecentList(CFStringRef type) {
	return CFEqual(type, kLSSharedFileListRecentDocumentItems) ||
	    CFEqual(type, kLSSharedFileListRecentApplicationItems);
}

LSSharedFileListRef LSSharedFileListCreate(CFAllocatorRef allocator, CFStringRef listType, CFTypeRef listOptions) {
	if (listType == NULL)
		return NULL;
	LSSharedFileListRef list = (LSSharedFileListRef)_CFRuntimeCreateInstance(allocator,
	    LSSharedFileListGetTypeID(), sizeof(*list) - sizeof(CFRuntimeBase), NULL);
	if (list != NULL)
		list->type = CFStringCreateCopy(NULL, listType);
	return list;
}

/* The stored items of a list (a mutable copy), under the lock. */
static CFMutableArrayRef copyStored(CFStringRef type) {
	CFPreferencesAppSynchronize(kDomain);
	CFPropertyListRef value = CFPreferencesCopyAppValue(type, kDomain);
	CFMutableArrayRef items;
	if (value != NULL && CFGetTypeID(value) == CFArrayGetTypeID())
		items = CFArrayCreateMutableCopy(NULL, 0, value);
	else
		items = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (value != NULL)
		CFRelease(value);
	return items;
}

static UInt32 numberIn(CFDictionaryRef d, CFStringRef key) {
	SInt64 v = 0;
	CFNumberRef n = d != NULL ? CFDictionaryGetValue(d, key) : NULL;
	if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
		CFNumberGetValue(n, kCFNumberSInt64Type, &v);
	return (UInt32)v;
}

static UInt32 nextNumber(CFStringRef key) {
	CFPropertyListRef value = CFPreferencesCopyAppValue(key, kDomain);
	SInt64 n = 0;
	if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID())
		CFNumberGetValue(value, kCFNumberSInt64Type, &n);
	if (value != NULL)
		CFRelease(value);
	n++;
	CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt64Type, &n);
	CFPreferencesSetAppValue(key, number, kDomain);
	CFRelease(number);
	return (UInt32)n;
}

static void store(CFStringRef type, CFArrayRef items) {
	CFPreferencesSetAppValue(type, items, kDomain);
	CFStringRef seedKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@.%@"), type, kSeedKey);
	nextNumber(seedKey);
	CFRelease(seedKey);
	CFPreferencesAppSynchronize(kDomain);
}

static LSSharedFileListItemRef itemFromStored(CFDictionaryRef d) {
	CFStringRef urlString = CFDictionaryGetValue(d, kURLKey);
	if (urlString == NULL || CFGetTypeID(urlString) != CFStringGetTypeID())
		return NULL;
	LSSharedFileListItemRef item = (LSSharedFileListItemRef)_CFRuntimeCreateInstance(NULL,
	    LSSharedFileListItemGetTypeID(), sizeof(*item) - sizeof(CFRuntimeBase), NULL);
	if (item == NULL)
		return NULL;
	item->itemID = numberIn(d, kIDKey);
	item->url = CFURLCreateWithString(NULL, urlString, NULL);
	CFStringRef name = CFDictionaryGetValue(d, kNameKey);
	item->name = name != NULL ? CFStringCreateCopy(NULL, name) : NULL;
	CFDictionaryRef properties = CFDictionaryGetValue(d, kPropertiesKey);
	item->properties = properties != NULL ? CFDictionaryCreateCopy(NULL, properties) : NULL;
	return item;
}

CFArrayRef LSSharedFileListCopySnapshot(LSSharedFileListRef list, UInt32 *outSnapshotSeed) {
	if (list == NULL)
		return NULL;
	pthread_mutex_lock(&lock);
	CFMutableArrayRef stored = copyStored(list->type);
	if (outSnapshotSeed != NULL) {
		CFStringRef seedKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@.%@"), list->type, kSeedKey);
		CFPropertyListRef seed = CFPreferencesCopyAppValue(seedKey, kDomain);
		SInt64 n = 0;
		if (seed != NULL && CFGetTypeID(seed) == CFNumberGetTypeID())
			CFNumberGetValue(seed, kCFNumberSInt64Type, &n);
		if (seed != NULL)
			CFRelease(seed);
		CFRelease(seedKey);
		*outSnapshotSeed = (UInt32)n;
	}
	pthread_mutex_unlock(&lock);
	CFMutableArrayRef snapshot = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (CFIndex i = 0; i < CFArrayGetCount(stored); i++) {
		CFDictionaryRef d = CFArrayGetValueAtIndex(stored, i);
		LSSharedFileListItemRef item = CFGetTypeID(d) == CFDictionaryGetTypeID() ? itemFromStored(d) : NULL;
		if (item != NULL) {
			CFArrayAppendValue(snapshot, item);
			CFRelease(item);
		}
	}
	CFRelease(stored);
	return snapshot;
}

static CFIndex indexOfID(CFArrayRef stored, UInt32 itemID) {
	for (CFIndex i = 0; i < CFArrayGetCount(stored); i++)
		if (numberIn(CFArrayGetValueAtIndex(stored, i), kIDKey) == itemID)
			return i;
	return kCFNotFound;
}

static bool sameURL(CFDictionaryRef d, CFURLRef url) {
	CFStringRef s = CFDictionaryGetValue(d, kURLKey);
	return s != NULL && CFEqual(s, CFURLGetString(url));
}

LSSharedFileListItemRef LSSharedFileListInsertItemURL(LSSharedFileListRef list,
    LSSharedFileListItemRef insertAfterThisItem, CFStringRef inDisplayName, IconRef inIconRef, CFURLRef inURL,
    CFDictionaryRef inPropertiesToSet, CFArrayRef inPropertiesToClear) {
	if (list == NULL || inURL == NULL)
		return NULL;
	CFURLRef absolute = CFURLCopyAbsoluteURL(inURL);
	pthread_mutex_lock(&lock);
	CFMutableArrayRef stored = copyStored(list->type);

	/* An item already in the list moves, keeping its ID and properties. */
	CFMutableDictionaryRef d = NULL;
	for (CFIndex i = 0; i < CFArrayGetCount(stored); i++) {
		CFDictionaryRef old = CFArrayGetValueAtIndex(stored, i);
		if (sameURL(old, absolute)) {
			d = CFDictionaryCreateMutableCopy(NULL, 0, old);
			CFArrayRemoveValueAtIndex(stored, i);
			break;
		}
	}
	if (d == NULL) {
		d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(d, kURLKey, CFURLGetString(absolute));
		SInt64 itemID = nextNumber(CFSTR("NextItemID"));
		CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt64Type, &itemID);
		CFDictionarySetValue(d, kIDKey, n);
		CFRelease(n);
	}
	if (inDisplayName != NULL)
		CFDictionarySetValue(d, kNameKey, inDisplayName);
	CFDictionaryRef oldProperties = CFDictionaryGetValue(d, kPropertiesKey);
	CFMutableDictionaryRef properties = oldProperties != NULL ? CFDictionaryCreateMutableCopy(NULL, 0, oldProperties)
	    : CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	for (CFIndex i = 0; inPropertiesToClear != NULL && i < CFArrayGetCount(inPropertiesToClear); i++)
		CFDictionaryRemoveValue(properties, CFArrayGetValueAtIndex(inPropertiesToClear, i));
	if (inPropertiesToSet != NULL) {
		CFIndex count = CFDictionaryGetCount(inPropertiesToSet);
		const void *keys[count > 0 ? count : 1], *values[count > 0 ? count : 1];
		CFDictionaryGetKeysAndValues(inPropertiesToSet, keys, values);
		for (CFIndex i = 0; i < count; i++)
			CFDictionarySetValue(properties, keys[i], values[i]);
	}
	CFDictionarySetValue(d, kPropertiesKey, properties);
	CFRelease(properties);

	CFIndex at;
	if (insertAfterThisItem == kLSSharedFileListItemBeforeFirst)
		at = 0;
	else if (insertAfterThisItem == kLSSharedFileListItemLast || insertAfterThisItem == NULL)
		at = CFArrayGetCount(stored);
	else {
		CFIndex after = indexOfID(stored, insertAfterThisItem->itemID);
		at = after == kCFNotFound ? CFArrayGetCount(stored) : after + 1;
	}
	CFArrayInsertValueAtIndex(stored, at, d);
	/* A recent list keeps the newest (the first) items. */
	while (isRecentList(list->type) && CFArrayGetCount(stored) > RECENT_LIMIT)
		CFArrayRemoveValueAtIndex(stored, CFArrayGetCount(stored) - 1);
	store(list->type, stored);
	pthread_mutex_unlock(&lock);

	LSSharedFileListItemRef item = itemFromStored(d);
	CFRelease(d);
	CFRelease(stored);
	CFRelease(absolute);
	return item;
}

OSStatus LSSharedFileListItemRemove(LSSharedFileListRef list, LSSharedFileListItemRef item) {
	if (list == NULL || item == NULL)
		return kParamErr;
	pthread_mutex_lock(&lock);
	CFMutableArrayRef stored = copyStored(list->type);
	CFIndex i = indexOfID(stored, item->itemID);
	OSStatus status = noErr;
	if (i == kCFNotFound)
		status = kFileNotFoundErr;
	else {
		CFArrayRemoveValueAtIndex(stored, i);
		store(list->type, stored);
	}
	pthread_mutex_unlock(&lock);
	CFRelease(stored);
	return status;
}

OSStatus LSSharedFileListRemoveAllItems(LSSharedFileListRef list) {
	if (list == NULL)
		return kParamErr;
	pthread_mutex_lock(&lock);
	CFArrayRef empty = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	store(list->type, empty);
	CFRelease(empty);
	pthread_mutex_unlock(&lock);
	return noErr;
}

CFURLRef LSSharedFileListItemCopyResolvedURL(LSSharedFileListItemRef item, LSSharedFileListResolutionFlags flags,
    CFErrorRef *outError) {
	if (outError != NULL)
		*outError = NULL;
	if (item == NULL || item->url == NULL)
		return NULL;
	/* A file item that is gone resolves to nothing. */
	CFStringRef scheme = CFURLCopyScheme(item->url);
	bool isFile = scheme != NULL && CFStringCompare(scheme, CFSTR("file"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
	if (scheme != NULL)
		CFRelease(scheme);
	if (isFile && !CFURLResourceIsReachable(item->url, outError))
		return NULL;
	return CFRetain(item->url);
}

CFStringRef LSSharedFileListItemCopyDisplayName(LSSharedFileListItemRef item) {
	if (item == NULL)
		return NULL;
	if (item->name != NULL)
		return CFRetain(item->name);
	return item->url != NULL ? CFURLCopyLastPathComponent(item->url) : NULL;
}

UInt32 LSSharedFileListItemGetID(LSSharedFileListItemRef item) {
	return item != NULL ? item->itemID : 0;
}

CFTypeRef LSSharedFileListItemCopyProperty(LSSharedFileListItemRef item, CFStringRef propertyName) {
	CFTypeRef value = item != NULL && item->properties != NULL && propertyName != NULL ?
	    CFDictionaryGetValue(item->properties, propertyName) : NULL;
	return value != NULL ? CFRetain(value) : NULL;
}
