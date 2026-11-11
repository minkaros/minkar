/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The user's password items, in ~/Library/Keychains/items.plist: an array
 * of dictionaries, each with its class, attributes (under the attribute
 * names, which are the four-character tags of the older API), its data
 * and an identifier. SHORTCUT(19.6.0): readable by the user only (mode
 * 0600), not encrypted.
 */

#include "Security.h"
#include "SecInternal.h"
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

const CFStringRef kSecClass = CFSTR("class");
const CFStringRef kSecClassGenericPassword = CFSTR("genp");
const CFStringRef kSecClassInternetPassword = CFSTR("inet");
const CFStringRef kSecClassCertificate = CFSTR("cert");
const CFStringRef kSecClassKey = CFSTR("keys");
const CFStringRef kSecClassIdentity = CFSTR("idnt");
const CFStringRef kSecAttrAccount = CFSTR("acct");
const CFStringRef kSecAttrService = CFSTR("svce");
const CFStringRef kSecAttrLabel = CFSTR("labl");
const CFStringRef kSecAttrDescription = CFSTR("desc");
const CFStringRef kSecAttrComment = CFSTR("icmt");
const CFStringRef kSecAttrGeneric = CFSTR("gena");
const CFStringRef kSecAttrServer = CFSTR("srvr");
const CFStringRef kSecAttrProtocol = CFSTR("ptcl");
const CFStringRef kSecAttrProtocolFTP = CFSTR("ftp ");
const CFStringRef kSecAttrProtocolFTPAccount = CFSTR("ftpa");
const CFStringRef kSecAttrProtocolHTTP = CFSTR("http");
const CFStringRef kSecAttrProtocolIRC = CFSTR("irc ");
const CFStringRef kSecAttrProtocolNNTP = CFSTR("nntp");
const CFStringRef kSecAttrProtocolPOP3 = CFSTR("pop3");
const CFStringRef kSecAttrProtocolSMTP = CFSTR("smtp");
const CFStringRef kSecAttrProtocolSOCKS = CFSTR("sox ");
const CFStringRef kSecAttrProtocolIMAP = CFSTR("imap");
const CFStringRef kSecAttrProtocolLDAP = CFSTR("ldap");
const CFStringRef kSecAttrProtocolAppleTalk = CFSTR("atlk");
const CFStringRef kSecAttrProtocolAFP = CFSTR("afp ");
const CFStringRef kSecAttrProtocolTelnet = CFSTR("teln");
const CFStringRef kSecAttrProtocolSSH = CFSTR("ssh ");
const CFStringRef kSecAttrProtocolFTPS = CFSTR("ftps");
const CFStringRef kSecAttrProtocolHTTPS = CFSTR("htps");
const CFStringRef kSecAttrProtocolHTTPProxy = CFSTR("htpx");
const CFStringRef kSecAttrProtocolHTTPSProxy = CFSTR("htsx");
const CFStringRef kSecAttrProtocolFTPProxy = CFSTR("ftpx");
const CFStringRef kSecAttrProtocolSMB = CFSTR("smb ");
const CFStringRef kSecAttrProtocolRTSP = CFSTR("rtsp");
const CFStringRef kSecAttrProtocolRTSPProxy = CFSTR("rtsx");
const CFStringRef kSecAttrProtocolDAAP = CFSTR("daap");
const CFStringRef kSecAttrProtocolEPPC = CFSTR("eppc");
const CFStringRef kSecAttrProtocolIPP = CFSTR("ipp ");
const CFStringRef kSecAttrProtocolNNTPS = CFSTR("ntps");
const CFStringRef kSecAttrProtocolLDAPS = CFSTR("ldps");
const CFStringRef kSecAttrProtocolTelnetS = CFSTR("tels");
const CFStringRef kSecAttrProtocolIMAPS = CFSTR("imps");
const CFStringRef kSecAttrProtocolIRCS = CFSTR("ircs");
const CFStringRef kSecAttrProtocolPOP3S = CFSTR("pops");
const CFStringRef kSecAttrPort = CFSTR("port");
const CFStringRef kSecAttrPath = CFSTR("path");
const CFStringRef kSecAttrSecurityDomain = CFSTR("sdmn");
const CFStringRef kSecAttrCreationDate = CFSTR("cdat");
const CFStringRef kSecAttrModificationDate = CFSTR("mdat");
const CFStringRef kSecAttrCreator = CFSTR("crtr");
const CFStringRef kSecAttrType = CFSTR("type");
const CFStringRef kSecAttrIsInvisible = CFSTR("invi");
const CFStringRef kSecAttrSynchronizable = CFSTR("sync");
const CFStringRef kSecAttrAccessible = CFSTR("pdmn");
const CFStringRef kSecAttrAccessibleWhenUnlocked = CFSTR("ak");
const CFStringRef kSecAttrAccessibleAfterFirstUnlock = CFSTR("ck");
const CFStringRef kSecAttrAccessGroup = CFSTR("agrp");
const CFStringRef kSecMatchLimit = CFSTR("m_Limit");
const CFStringRef kSecMatchLimitOne = CFSTR("m_LimitOne");
const CFStringRef kSecMatchLimitAll = CFSTR("m_LimitAll");
const CFStringRef kSecMatchSearchList = CFSTR("m_SearchList");
const CFStringRef kSecMatchCaseInsensitive = CFSTR("m_CaseInsensitive");
const CFStringRef kSecReturnData = CFSTR("r_Data");
const CFStringRef kSecReturnAttributes = CFSTR("r_Attributes");
const CFStringRef kSecReturnRef = CFSTR("r_Ref");
const CFStringRef kSecReturnPersistentRef = CFSTR("r_PersistentRef");
const CFStringRef kSecValueData = CFSTR("v_Data");
const CFStringRef kSecValueRef = CFSTR("v_Ref");
const CFStringRef kSecValuePersistentRef = CFSTR("v_PersistentRef");
const CFStringRef kSecUseKeychain = CFSTR("u_Keychain");
const CFStringRef kSecUseDataProtectionKeychain = CFSTR("nleg");

#define kItemClassKey CFSTR("class")
#define kItemDataKey CFSTR("v_Data")
#define kItemIDKey CFSTR("_id")

/* ---- The store ---- */

static void storePath(char path[PATH_MAX], Boolean makeDirectory) {
    const char *home = getenv("HOME");
    struct passwd *pw;
    if (home == NULL || home[0] == '\0') {
        pw = getpwuid(getuid());
        home = pw != NULL ? pw->pw_dir : "/tmp";
    }
    snprintf(path, PATH_MAX, "%s/Library/Keychains", home);
    if (makeDirectory) {
        char library[PATH_MAX];
        snprintf(library, sizeof(library), "%s/Library", home);
        mkdir(library, 0755);
        mkdir(path, 0700);
    }
    strlcat(path, "/items.plist", PATH_MAX);
}

/* Locked while it is read and written back. */
static int lockStore(void) {
    char path[PATH_MAX];
    storePath(path, true);
    strlcat(path, ".lock", sizeof(path));
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd >= 0)
        flock(fd, LOCK_EX);
    return fd;
}

static void unlockStore(int fd) {
    if (fd >= 0) {
        flock(fd, LOCK_UN);
        close(fd);
    }
}

static CFMutableArrayRef readItems(void) {
    char path[PATH_MAX];
    CFMutableArrayRef items = NULL;
    storePath(path, false);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), false);
    CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, url);
    if (stream != NULL && CFReadStreamOpen(stream)) {
        CFPropertyListRef plist = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListMutableContainers, NULL, NULL);
        if (plist != NULL && CFGetTypeID(plist) == CFArrayGetTypeID())
            items = (CFMutableArrayRef)plist;
        else if (plist != NULL)
            CFRelease(plist);
        CFReadStreamClose(stream);
    }
    if (stream != NULL) CFRelease(stream);
    CFRelease(url);
    return items != NULL ? items : CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
}

static OSStatus writeItems(CFArrayRef items) {
    char path[PATH_MAX], temp[PATH_MAX];
    CFDataRef data = CFPropertyListCreateData(NULL, items, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    OSStatus status = errSecIO;
    storePath(path, true);
    snprintf(temp, sizeof(temp), "%s.new", path);
    int fd = open(temp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0 && data != NULL) {
        if (write(fd, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data)) == CFDataGetLength(data) && fsync(fd) == 0)
            status = errSecSuccess;
        close(fd);
        if (status == errSecSuccess && rename(temp, path) != 0)
            status = errSecIO;
    } else if (fd >= 0)
        close(fd);
    if (data != NULL) CFRelease(data);
    return status;
}

/* ---- Items as CF objects (SecKeychainItemRef) ---- */

struct __SecKeychainItem {
    CFRuntimeBase base;
    CFDictionaryRef item;
};

static void itemFinalize(CFTypeRef cf) {
    struct __SecKeychainItem *i = (struct __SecKeychainItem *)cf;
    if (i->item != NULL) CFRelease(i->item);
}

SEC_TYPE(SecKeychainItem, "SecKeychainItem", itemFinalize)

static SecKeychainItemRef createItemRef(CFDictionaryRef item) {
    struct __SecKeychainItem *ref = _SecCreateInstance(SecKeychainItemGetTypeID(), sizeof(struct __SecKeychainItem));
    if (ref != NULL)
        ref->item = CFDictionaryCreateCopy(NULL, item);
    return ref;
}

struct __SecKeychain {
    CFRuntimeBase base;
};

SEC_TYPE(SecKeychain, "SecKeychain", NULL)

OSStatus SecKeychainCopyDefault(SecKeychainRef *keychain) {
    if (keychain == NULL)
        return errSecParam;
    *keychain = _SecCreateInstance(SecKeychainGetTypeID(), sizeof(struct __SecKeychain));
    return *keychain != NULL ? errSecSuccess : errSecAllocate;
}

/* ---- Matching ---- */

static Boolean isControlKey(CFStringRef key) {
    return CFStringHasPrefix(key, CFSTR("m_")) || CFStringHasPrefix(key, CFSTR("r_")) || CFStringHasPrefix(key, CFSTR("v_")) ||
           CFStringHasPrefix(key, CFSTR("u_")) || CFEqual(key, kSecClass) || CFEqual(key, kSecUseDataProtectionKeychain) ||
           CFEqual(key, kSecAttrSynchronizable) || CFEqual(key, kSecAttrAccessible) || CFEqual(key, kItemIDKey);
}

static Boolean valuesMatch(CFTypeRef want, CFTypeRef have, Boolean caseInsensitive) {
    if (want == NULL || have == NULL)
        return want == have;
    if (caseInsensitive && CFGetTypeID(want) == CFStringGetTypeID() && CFGetTypeID(have) == CFStringGetTypeID())
        return CFStringCompare(want, have, kCFCompareCaseInsensitive) == kCFCompareEqualTo;
    return CFEqual(want, have);
}

typedef struct { CFDictionaryRef query; Boolean caseInsensitive; Boolean matched; } MatchContext;

static void matchAttribute(const void *key, const void *value, void *context) {
    MatchContext *m = context;
    CFDictionaryRef item = (CFDictionaryRef)m->query;
    if (!m->matched || isControlKey(key))
        return;
    if (!valuesMatch(value, CFDictionaryGetValue(item, key), m->caseInsensitive))
        m->matched = false;
}

static Boolean itemMatches(CFDictionaryRef item, CFDictionaryRef query) {
    CFTypeRef cls = CFDictionaryGetValue(query, kSecClass);
    CFTypeRef ref = CFDictionaryGetValue(query, kSecValueRef);
    CFTypeRef caseInsensitive = CFDictionaryGetValue(query, kSecMatchCaseInsensitive);
    if (cls != NULL && !CFEqual(cls, CFDictionaryGetValue(item, kItemClassKey)))
        return false;
    if (ref != NULL && CFGetTypeID(ref) == SecKeychainItemGetTypeID() &&
        !CFEqual(CFDictionaryGetValue(((struct __SecKeychainItem *)ref)->item, kItemIDKey), CFDictionaryGetValue(item, kItemIDKey)))
        return false;
    MatchContext m = { item, caseInsensitive != NULL && CFEqual(caseInsensitive, kCFBooleanTrue), true };
    CFDictionaryApplyFunction(query, matchAttribute, &m);
    return m.matched;
}

static Boolean wants(CFDictionaryRef query, CFStringRef key) {
    CFTypeRef value = CFDictionaryGetValue(query, key);
    return value != NULL && CFEqual(value, kCFBooleanTrue);
}

static CFTypeRef resultFor(CFDictionaryRef item, CFDictionaryRef query) {
    Boolean data = wants(query, kSecReturnData), attributes = wants(query, kSecReturnAttributes);
    Boolean ref = wants(query, kSecReturnRef), persistent = wants(query, kSecReturnPersistentRef);
    int count = data + attributes + ref + persistent;

    if (count == 1 && data) {
        CFTypeRef bytes = CFDictionaryGetValue(item, kItemDataKey);
        return bytes != NULL ? CFRetain(bytes) : CFDataCreate(NULL, NULL, 0);
    }
    if (count == 1 && ref)
        return createItemRef(item);
    if (count == 1 && persistent)
        return CFStringCreateExternalRepresentation(NULL, CFDictionaryGetValue(item, kItemIDKey), kCFStringEncodingUTF8, 0);
    CFMutableDictionaryRef result = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (attributes || count == 0) {
        CFIndex n = CFDictionaryGetCount(item);
        const void *keys[n], *values[n];
        CFDictionaryGetKeysAndValues(item, keys, values);
        for (CFIndex i = 0; i < n; i++)
            if (!CFEqual(keys[i], kItemDataKey) && !CFEqual(keys[i], kItemIDKey))
                CFDictionarySetValue(result, keys[i], values[i]);
    }
    if (data && CFDictionaryGetValue(item, kItemDataKey) != NULL)
        CFDictionarySetValue(result, kSecValueData, CFDictionaryGetValue(item, kItemDataKey));
    if (ref) {
        SecKeychainItemRef r = createItemRef(item);
        CFDictionarySetValue(result, kSecValueRef, r);
        CFRelease(r);
    }
    return result;
}

/* ---- SecItem ---- */

static Boolean sameIdentity(CFDictionaryRef a, CFDictionaryRef b) {
    static const CFStringRef generic[] = { CFSTR("acct"), CFSTR("svce") };
    static const CFStringRef internet[] = { CFSTR("acct"), CFSTR("srvr"), CFSTR("ptcl"), CFSTR("port"), CFSTR("path"), CFSTR("sdmn") };
    CFTypeRef cls = CFDictionaryGetValue(a, kItemClassKey);
    if (!CFEqual(cls, CFDictionaryGetValue(b, kItemClassKey)))
        return false;
    Boolean inet = CFEqual(cls, kSecClassInternetPassword);
    const CFStringRef *keys = inet ? internet : generic;
    size_t n = inet ? 6 : 2;
    for (size_t i = 0; i < n; i++)
        if (!valuesMatch(CFDictionaryGetValue(a, keys[i]), CFDictionaryGetValue(b, keys[i]), false))
            return false;
    return true;
}

static Boolean supportedClass(CFTypeRef cls) {
    return cls != NULL && (CFEqual(cls, kSecClassGenericPassword) || CFEqual(cls, kSecClassInternetPassword));
}

OSStatus SecItemAdd(CFDictionaryRef attributes, CFTypeRef *result) {
    CFTypeRef cls;
    if (result != NULL)
        *result = NULL;
    if (attributes == NULL || (cls = CFDictionaryGetValue(attributes, kSecClass)) == NULL)
        return errSecParam;
    if (!supportedClass(cls))
        return errSecUnimplemented;
    CFMutableDictionaryRef item = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFIndex n = CFDictionaryGetCount(attributes);
    const void *keys[n], *values[n];
    CFDictionaryGetKeysAndValues(attributes, keys, values);
    for (CFIndex i = 0; i < n; i++)
        if (!isControlKey(keys[i]))
            CFDictionarySetValue(item, keys[i], values[i]);
    CFDictionarySetValue(item, kItemClassKey, cls);
    CFTypeRef data = CFDictionaryGetValue(attributes, kSecValueData);
    if (data != NULL && CFGetTypeID(data) == CFDataGetTypeID())
        CFDictionarySetValue(item, kItemDataKey, data);
    CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    CFDictionarySetValue(item, kSecAttrCreationDate, now);
    CFDictionarySetValue(item, kSecAttrModificationDate, now);
    CFRelease(now);
    CFUUIDRef uuid = CFUUIDCreate(NULL);
    CFStringRef identifier = CFUUIDCreateString(NULL, uuid);
    CFDictionarySetValue(item, kItemIDKey, identifier);
    CFRelease(identifier);
    CFRelease(uuid);

    int lock = lockStore();
    CFMutableArrayRef items = readItems();
    OSStatus status = errSecSuccess;
    for (CFIndex i = 0, count = CFArrayGetCount(items); i < count; i++)
        if (sameIdentity(CFArrayGetValueAtIndex(items, i), item)) {
            status = errSecDuplicateItem;
            break;
        }
    if (status == errSecSuccess) {
        CFArrayAppendValue(items, item);
        status = writeItems(items);
    }
    unlockStore(lock);
    if (status == errSecSuccess && result != NULL &&
        (wants(attributes, kSecReturnData) || wants(attributes, kSecReturnAttributes) || wants(attributes, kSecReturnRef)))
        *result = resultFor(item, attributes);
    CFRelease(items);
    CFRelease(item);
    return status;
}

OSStatus SecItemCopyMatching(CFDictionaryRef query, CFTypeRef *result) {
    CFTypeRef limit;
    Boolean all;
    if (result != NULL)
        *result = NULL;
    if (query == NULL)
        return errSecParam;
    limit = CFDictionaryGetValue(query, kSecMatchLimit);
    all = limit != NULL && CFEqual(limit, kSecMatchLimitAll);
    int lock = lockStore();
    CFMutableArrayRef items = readItems();
    unlockStore(lock);
    CFMutableArrayRef found = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (CFIndex i = 0, count = CFArrayGetCount(items); i < count; i++) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
        if (!itemMatches(item, query))
            continue;
        CFTypeRef one = resultFor(item, query);
        CFArrayAppendValue(found, one);
        CFRelease(one);
        if (!all)
            break;
    }
    CFRelease(items);
    OSStatus status = CFArrayGetCount(found) > 0 ? errSecSuccess : errSecItemNotFound;
    if (status == errSecSuccess && result != NULL)
        *result = all ? CFRetain(found) : CFRetain(CFArrayGetValueAtIndex(found, 0));
    CFRelease(found);
    return status;
}

typedef struct { CFMutableDictionaryRef item; } UpdateContext;

static void applyUpdate(const void *key, const void *value, void *context) {
    CFMutableDictionaryRef item = ((UpdateContext *)context)->item;
    if (CFEqual(key, kSecValueData))
        CFDictionarySetValue(item, kItemDataKey, value);
    else if (!isControlKey(key))
        CFDictionarySetValue(item, key, value);
}

OSStatus SecItemUpdate(CFDictionaryRef query, CFDictionaryRef attributesToUpdate) {
    if (query == NULL || attributesToUpdate == NULL)
        return errSecParam;
    int lock = lockStore();
    CFMutableArrayRef items = readItems();
    OSStatus status = errSecItemNotFound;
    for (CFIndex i = 0, count = CFArrayGetCount(items); i < count; i++) {
        CFDictionaryRef item = CFArrayGetValueAtIndex(items, i);
        if (!itemMatches(item, query))
            continue;
        CFMutableDictionaryRef changed = CFDictionaryCreateMutableCopy(NULL, 0, item);
        UpdateContext u = { changed };
        CFDictionaryApplyFunction(attributesToUpdate, applyUpdate, &u);
        CFDateRef now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
        CFDictionarySetValue(changed, kSecAttrModificationDate, now);
        CFRelease(now);
        CFArraySetValueAtIndex(items, i, changed);
        CFRelease(changed);
        status = errSecSuccess;
    }
    if (status == errSecSuccess)
        status = writeItems(items);
    unlockStore(lock);
    CFRelease(items);
    return status;
}

OSStatus SecItemDelete(CFDictionaryRef query) {
    if (query == NULL)
        return errSecParam;
    int lock = lockStore();
    CFMutableArrayRef items = readItems();
    OSStatus status = errSecItemNotFound;
    for (CFIndex i = CFArrayGetCount(items) - 1; i >= 0; i--)
        if (itemMatches(CFArrayGetValueAtIndex(items, i), query)) {
            CFArrayRemoveValueAtIndex(items, i);
            status = errSecSuccess;
        }
    if (status == errSecSuccess)
        status = writeItems(items);
    unlockStore(lock);
    CFRelease(items);
    return status;
}

/* ---- The older calls ---- */

static CFStringRef keyForTag(SecKeychainAttrType tag) {
    char name[5] = { (char)(tag >> 24), (char)(tag >> 16), (char)(tag >> 8), (char)tag, 0 };
    return CFStringCreateWithCString(NULL, name, kCFStringEncodingMacRoman);
}

static CFStringRef classForItemClass(SecItemClass itemClass) {
    if (itemClass == kSecGenericPasswordItemClass) return kSecClassGenericPassword;
    if (itemClass == kSecInternetPasswordItemClass) return kSecClassInternetPassword;
    return NULL;
}

static SecItemClass itemClassOf(CFDictionaryRef item) {
    CFTypeRef cls = CFDictionaryGetValue(item, kItemClassKey);
    return (cls != NULL && CFEqual(cls, kSecClassInternetPassword)) ? kSecInternetPasswordItemClass : kSecGenericPasswordItemClass;
}

/* An attribute's value as bytes (strings in UTF-8, numbers as 32 bits). */
static void *bytesOf(CFTypeRef value, UInt32 *length) {
    *length = 0;
    if (value == NULL)
        return NULL;
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        CFIndex max = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value), kCFStringEncodingUTF8) + 1;
        char *buffer = malloc((size_t)max);
        CFStringGetCString(value, buffer, max, kCFStringEncodingUTF8);
        *length = (UInt32)strlen(buffer);
        return buffer;
    }
    if (CFGetTypeID(value) == CFDataGetTypeID()) {
        *length = (UInt32)CFDataGetLength(value);
        void *buffer = malloc(*length ? *length : 1);
        memcpy(buffer, CFDataGetBytePtr(value), *length);
        return buffer;
    }
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        SInt32 *n = malloc(sizeof(SInt32));
        CFNumberGetValue(value, kCFNumberSInt32Type, n);
        *length = sizeof(SInt32);
        return n;
    }
    return NULL;
}

struct __SecKeychainSearch {
    CFRuntimeBase base;
    CFArrayRef results;
    CFIndex next;
};

static void searchFinalize(CFTypeRef cf) {
    struct __SecKeychainSearch *s = (struct __SecKeychainSearch *)cf;
    if (s->results != NULL) CFRelease(s->results);
}

SEC_TYPE(SecKeychainSearch, "SecKeychainSearch", searchFinalize)

static CFMutableDictionaryRef queryFromAttributes(SecItemClass itemClass, const SecKeychainAttributeList *attrList) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFStringRef cls = classForItemClass(itemClass);
    if (cls != NULL)
        CFDictionarySetValue(query, kSecClass, cls);
    for (UInt32 i = 0; attrList != NULL && i < attrList->count; i++) {
        SecKeychainAttribute *a = &attrList->attr[i];
        CFStringRef key = keyForTag(a->tag);
        CFTypeRef value;
        if (a->tag == kSecPortItemAttr && a->length == 4) {
            SInt32 port;
            memcpy(&port, a->data, 4);
            value = CFNumberCreate(NULL, kCFNumberSInt32Type, &port);
        } else
            value = CFStringCreateWithBytes(NULL, a->data, a->length, kCFStringEncodingUTF8, false);
        if (value != NULL) {
            CFDictionarySetValue(query, key, value);
            CFRelease(value);
        }
        CFRelease(key);
    }
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
    return query;
}

OSStatus SecKeychainSearchCreateFromAttributes(CFTypeRef keychainOrArray, SecItemClass itemClass,
                                               const SecKeychainAttributeList *attrList, SecKeychainSearchRef *searchRef) {
    if (searchRef == NULL)
        return errSecParam;
    CFMutableDictionaryRef query = queryFromAttributes(itemClass, attrList);
    CFTypeRef results = NULL;
    OSStatus status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status != errSecSuccess && status != errSecItemNotFound)
        return status;
    struct __SecKeychainSearch *s = _SecCreateInstance(SecKeychainSearchGetTypeID(), sizeof(struct __SecKeychainSearch));
    s->results = results != NULL ? results : CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
    *searchRef = s;
    return errSecSuccess;
}

OSStatus SecKeychainSearchCopyNext(SecKeychainSearchRef searchRef, SecKeychainItemRef *itemRef) {
    struct __SecKeychainSearch *s = (struct __SecKeychainSearch *)searchRef;
    if (s == NULL || itemRef == NULL)
        return errSecParam;
    if (s->next >= CFArrayGetCount(s->results))
        return errSecItemNotFound;
    *itemRef = (SecKeychainItemRef)CFRetain(CFArrayGetValueAtIndex(s->results, s->next++));
    return errSecSuccess;
}

/* Fills the tags the caller asked for, and the data. */
OSStatus SecKeychainItemCopyContent(SecKeychainItemRef itemRef, SecItemClass *itemClass, SecKeychainAttributeList *attrList,
                                    UInt32 *length, void **outData) {
    struct __SecKeychainItem *i = (struct __SecKeychainItem *)itemRef;
    if (i == NULL)
        return errSecParam;
    if (itemClass != NULL)
        *itemClass = itemClassOf(i->item);
    for (UInt32 n = 0; attrList != NULL && n < attrList->count; n++) {
        CFStringRef key = keyForTag(attrList->attr[n].tag);
        attrList->attr[n].data = bytesOf(CFDictionaryGetValue(i->item, key), &attrList->attr[n].length);
        CFRelease(key);
    }
    if (outData != NULL)
        *outData = bytesOf(CFDictionaryGetValue(i->item, kItemDataKey), length);
    else if (length != NULL)
        *length = (UInt32)(CFDictionaryGetValue(i->item, kItemDataKey) ? CFDataGetLength(CFDictionaryGetValue(i->item, kItemDataKey)) : 0);
    return errSecSuccess;
}

OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *attrList, void *data) {
    for (UInt32 n = 0; attrList != NULL && n < attrList->count; n++) {
        free(attrList->attr[n].data);
        attrList->attr[n].data = NULL;
    }
    free(data);
    return errSecSuccess;
}

OSStatus SecKeychainItemCopyAttributesAndData(SecKeychainItemRef itemRef, SecKeychainAttributeInfo *info, SecItemClass *itemClass,
                                              SecKeychainAttributeList **attrList, UInt32 *length, void **outData) {
    struct __SecKeychainItem *i = (struct __SecKeychainItem *)itemRef;
    if (i == NULL)
        return errSecParam;
    if (itemClass != NULL)
        *itemClass = itemClassOf(i->item);
    if (attrList != NULL) {
        SecKeychainAttributeList *list = calloc(1, sizeof(SecKeychainAttributeList));
        UInt32 count = info != NULL ? info->count : 0;
        list->count = count;
        list->attr = count ? calloc(count, sizeof(SecKeychainAttribute)) : NULL;
        for (UInt32 n = 0; n < count; n++) {
            CFStringRef key = keyForTag(info->tag[n]);
            list->attr[n].tag = info->tag[n];
            list->attr[n].data = bytesOf(CFDictionaryGetValue(i->item, key), &list->attr[n].length);
            CFRelease(key);
        }
        *attrList = list;
    }
    if (outData != NULL)
        *outData = bytesOf(CFDictionaryGetValue(i->item, kItemDataKey), length);
    return errSecSuccess;
}

OSStatus SecKeychainItemFreeAttributesAndData(SecKeychainAttributeList *attrList, void *data) {
    if (attrList != NULL) {
        SecKeychainItemFreeContent(attrList, NULL);
        free(attrList->attr);
        free(attrList);
    }
    free(data);
    return errSecSuccess;
}

static CFDictionaryRef identityQuery(CFDictionaryRef item) {
    const void *keys[] = { kSecClass, kSecValueRef };
    SecKeychainItemRef ref = createItemRef(item);
    const void *values[] = { CFDictionaryGetValue(item, kItemClassKey), ref };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(ref);
    return query;
}

OSStatus SecKeychainItemModifyAttributesAndData(SecKeychainItemRef itemRef, const SecKeychainAttributeList *attrList,
                                                UInt32 length, const void *data) {
    struct __SecKeychainItem *i = (struct __SecKeychainItem *)itemRef;
    if (i == NULL)
        return errSecParam;
    CFMutableDictionaryRef update = queryFromAttributes(0, attrList);
    CFDictionaryRemoveValue(update, kSecMatchLimit);
    CFDictionaryRemoveValue(update, kSecReturnRef);
    if (data != NULL) {
        CFDataRef bytes = CFDataCreate(NULL, data, length);
        CFDictionarySetValue(update, kSecValueData, bytes);
        CFRelease(bytes);
    }
    CFDictionaryRef query = identityQuery(i->item);
    OSStatus status = SecItemUpdate(query, update);
    CFRelease(query);
    CFRelease(update);
    return status;
}

OSStatus SecKeychainItemDelete(SecKeychainItemRef itemRef) {
    struct __SecKeychainItem *i = (struct __SecKeychainItem *)itemRef;
    if (i == NULL)
        return errSecParam;
    CFDictionaryRef query = identityQuery(i->item);
    OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return status;
}

static CFStringRef stringOf(const char *bytes, UInt32 length) {
    return CFStringCreateWithBytes(NULL, (const UInt8 *)bytes, length, kCFStringEncodingUTF8, false);
}

OSStatus SecKeychainAddGenericPassword(SecKeychainRef keychain, UInt32 serviceNameLength, const char *serviceName,
                                       UInt32 accountNameLength, const char *accountName, UInt32 passwordLength,
                                       const void *passwordData, SecKeychainItemRef *itemRef) {
    CFStringRef service = stringOf(serviceName, serviceNameLength), account = stringOf(accountName, accountNameLength);
    CFDataRef password = CFDataCreate(NULL, passwordData, passwordLength);
    const void *keys[] = { kSecClass, kSecAttrService, kSecAttrAccount, kSecValueData, kSecReturnRef };
    const void *values[] = { kSecClassGenericPassword, service, account, password, itemRef != NULL ? kCFBooleanTrue : kCFBooleanFalse };
    CFDictionaryRef attributes = CFDictionaryCreate(NULL, keys, values, 5, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef result = NULL;
    OSStatus status = SecItemAdd(attributes, itemRef != NULL ? &result : NULL);
    if (itemRef != NULL)
        *itemRef = (SecKeychainItemRef)result;
    CFRelease(attributes); CFRelease(password); CFRelease(account); CFRelease(service);
    return status;
}

OSStatus SecKeychainFindGenericPassword(CFTypeRef keychainOrArray, UInt32 serviceNameLength, const char *serviceName,
                                        UInt32 accountNameLength, const char *accountName, UInt32 *passwordLength,
                                        void **passwordData, SecKeychainItemRef *itemRef) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
    if (serviceName != NULL) {
        CFStringRef s = stringOf(serviceName, serviceNameLength);
        CFDictionarySetValue(query, kSecAttrService, s);
        CFRelease(s);
    }
    if (accountName != NULL) {
        CFStringRef a = stringOf(accountName, accountNameLength);
        CFDictionarySetValue(query, kSecAttrAccount, a);
        CFRelease(a);
    }
    CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
    CFTypeRef ref = NULL;
    OSStatus status = SecItemCopyMatching(query, &ref);
    CFRelease(query);
    if (status != errSecSuccess)
        return status;
    if (passwordData != NULL || passwordLength != NULL)
        SecKeychainItemCopyContent((SecKeychainItemRef)ref, NULL, NULL, passwordLength, passwordData);
    if (itemRef != NULL)
        *itemRef = (SecKeychainItemRef)ref;
    else
        CFRelease(ref);
    return errSecSuccess;
}
