/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SHORTCUT(19.6.0): code signatures are not read or checked yet. Code
 * objects know their path (and process); validity checks report the code
 * as not signed, and signing information has the main executable and the
 * bundle's identifier and Info.plist only.
 */

#include "Security.h"
#include "SecInternal.h"
#include <libproc.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

const CFStringRef kSecCodeInfoIdentifier = CFSTR("identifier");
const CFStringRef kSecCodeInfoTeamIdentifier = CFSTR("teamid");
const CFStringRef kSecCodeInfoMainExecutable = CFSTR("main-executable");
const CFStringRef kSecCodeInfoCertificates = CFSTR("certificates");
const CFStringRef kSecCodeInfoFlags = CFSTR("flags");
const CFStringRef kSecCodeInfoStatus = CFSTR("status");
const CFStringRef kSecCodeInfoFormat = CFSTR("format");
const CFStringRef kSecCodeInfoPList = CFSTR("info-plist");
const CFStringRef kSecCodeInfoUnique = CFSTR("unique");
const CFStringRef kSecCodeInfoDesignatedRequirement = CFSTR("designated-requirement");
const CFStringRef kSecCodeInfoEntitlementsDict = CFSTR("entitlements-dict");
const CFStringRef kSecGuestAttributePid = CFSTR("pid");
const CFStringRef kSecGuestAttributeAudit = CFSTR("audit");

struct __SecCode {
    CFRuntimeBase base;
    CFURLRef url;
    pid_t pid;
};

static void codeFinalize(CFTypeRef cf) {
    struct __SecCode *c = (struct __SecCode *)cf;
    if (c->url != NULL) CFRelease(c->url);
}

SEC_TYPE(SecCode, "SecCode", codeFinalize)

CFTypeID SecStaticCodeGetTypeID(void) {
    return SecCodeGetTypeID();
}

static SecCodeRef createCode(CFURLRef url, pid_t pid) {
    struct __SecCode *c = _SecCreateInstance(SecCodeGetTypeID(), sizeof(struct __SecCode));
    c->url = CFRetain(url);
    c->pid = pid;
    return c;
}

/* An executable inside a bundle stands for the bundle. */
static CFURLRef copyCodeURL(const char *path) {
    char buffer[PATH_MAX];
    strlcpy(buffer, path, sizeof(buffer));
    char *contents = strstr(buffer, "/Contents/MacOS/");
    if (contents != NULL)
        *contents = '\0';
    struct stat st;
    Boolean directory = stat(buffer, &st) == 0 && S_ISDIR(st.st_mode);
    return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)buffer, (CFIndex)strlen(buffer), directory);
}

OSStatus SecCodeCopySelf(SecCSFlags flags, SecCodeRef *self) {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (self == NULL)
        return errSecParam;
    if (proc_pidpath(getpid(), path, sizeof(path)) <= 0)
        return errSecCSStaticCodeNotFound;
    CFURLRef url = copyCodeURL(path);
    *self = createCode(url, getpid());
    CFRelease(url);
    return errSecSuccess;
}

OSStatus SecCodeCopyGuestWithAttributes(SecCodeRef host, CFDictionaryRef attributes, SecCSFlags flags, SecCodeRef *guest) {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    CFNumberRef pidNumber = attributes != NULL ? CFDictionaryGetValue(attributes, kSecGuestAttributePid) : NULL;
    int pid = 0;
    if (guest == NULL || pidNumber == NULL || !CFNumberGetValue(pidNumber, kCFNumberIntType, &pid))
        return errSecParam;
    if (proc_pidpath(pid, path, sizeof(path)) <= 0)
        return errSecCSStaticCodeNotFound;
    CFURLRef url = copyCodeURL(path);
    *guest = createCode(url, pid);
    CFRelease(url);
    return errSecSuccess;
}

OSStatus SecCodeCopyStaticCode(SecCodeRef code, SecCSFlags flags, SecStaticCodeRef *staticCode) {
    if (code == NULL || staticCode == NULL)
        return errSecParam;
    *staticCode = createCode(((struct __SecCode *)code)->url, 0);
    return errSecSuccess;
}

OSStatus SecStaticCodeCreateWithPath(CFURLRef path, SecCSFlags flags, SecStaticCodeRef *staticCode) {
    char buffer[PATH_MAX];
    struct stat st;
    if (path == NULL || staticCode == NULL)
        return errSecParam;
    if (!CFURLGetFileSystemRepresentation(path, true, (UInt8 *)buffer, sizeof(buffer)) || stat(buffer, &st) != 0)
        return errSecCSStaticCodeNotFound;
    *staticCode = createCode(path, 0);
    return errSecSuccess;
}

OSStatus SecCodeCopyPath(SecStaticCodeRef staticCode, SecCSFlags flags, CFURLRef *path) {
    if (staticCode == NULL || path == NULL)
        return errSecParam;
    *path = CFRetain(((struct __SecCode *)staticCode)->url);
    return errSecSuccess;
}

/* What is known without a signature: the main executable, and a bundle's
   identifier and Info.plist. */
OSStatus SecCodeCopySigningInformation(SecStaticCodeRef code, SecCSFlags flags, CFDictionaryRef *information) {
    struct __SecCode *c = (struct __SecCode *)code;
    CFMutableDictionaryRef info;
    CFBundleRef bundle;
    if (c == NULL || information == NULL)
        return errSecParam;
    info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    bundle = CFBundleCreate(NULL, c->url);
    if (bundle != NULL) {
        CFURLRef executable = CFBundleCopyExecutableURL(bundle);
        CFDictionaryRef plist = CFBundleGetInfoDictionary(bundle);
        CFStringRef identifier = CFBundleGetIdentifier(bundle);
        if (executable != NULL) {
            CFDictionarySetValue(info, kSecCodeInfoMainExecutable, executable);
            CFRelease(executable);
        }
        if (plist != NULL)
            CFDictionarySetValue(info, kSecCodeInfoPList, plist);
        if (identifier != NULL && (flags & kSecCSSigningInformation) == 0)
            CFDictionarySetValue(info, CFSTR("bundle-identifier"), identifier);
        CFRelease(bundle);
    } else
        CFDictionarySetValue(info, kSecCodeInfoMainExecutable, c->url);
    *information = info;
    return errSecSuccess;
}

OSStatus SecCodeCopyDesignatedRequirement(SecStaticCodeRef code, SecCSFlags flags, SecRequirementRef *requirement) {
    if (code == NULL || requirement == NULL)
        return errSecParam;
    *requirement = NULL;
    return errSecCSUnsigned;
}

OSStatus SecCodeCheckValidity(SecCodeRef code, SecCSFlags flags, SecRequirementRef requirement) {
    return code != NULL ? errSecCSUnsigned : errSecCSInvalidObjectRef;
}

static void unsignedError(CFErrorRef *errors) {
    if (errors != NULL)
        *errors = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecCSUnsigned, NULL);
}

OSStatus SecCodeCheckValidityWithErrors(SecCodeRef code, SecCSFlags flags, SecRequirementRef requirement, CFErrorRef *errors) {
    OSStatus status = SecCodeCheckValidity(code, flags, requirement);
    if (status != errSecSuccess)
        unsignedError(errors);
    return status;
}

OSStatus SecStaticCodeCheckValidity(SecStaticCodeRef staticCode, SecCSFlags flags, SecRequirementRef requirement) {
    return staticCode != NULL ? errSecCSUnsigned : errSecCSInvalidObjectRef;
}

OSStatus SecStaticCodeCheckValidityWithErrors(SecStaticCodeRef staticCode, SecCSFlags flags, SecRequirementRef requirement,
                                              CFErrorRef *errors) {
    OSStatus status = SecStaticCodeCheckValidity(staticCode, flags, requirement);
    if (status != errSecSuccess)
        unsignedError(errors);
    return status;
}

/* ---- Requirements: their text ---- */

struct __SecRequirement {
    CFRuntimeBase base;
    CFStringRef text;
};

static void requirementFinalize(CFTypeRef cf) {
    struct __SecRequirement *r = (struct __SecRequirement *)cf;
    if (r->text != NULL) CFRelease(r->text);
}

SEC_TYPE(SecRequirement, "SecRequirement", requirementFinalize)

OSStatus SecRequirementCreateWithStringAndErrors(CFStringRef text, SecCSFlags flags, CFErrorRef *errors, SecRequirementRef *requirement) {
    if (text == NULL || requirement == NULL)
        return errSecParam;
    CFStringRef trimmed = CFStringCreateMutableCopy(NULL, 0, text);
    CFStringTrimWhitespace((CFMutableStringRef)trimmed);
    if (CFStringGetLength(trimmed) == 0) {
        CFRelease(trimmed);
        if (errors != NULL)
            *errors = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecCSReqInvalid, NULL);
        return errSecCSReqInvalid;
    }
    struct __SecRequirement *r = _SecCreateInstance(SecRequirementGetTypeID(), sizeof(struct __SecRequirement));
    r->text = trimmed;
    *requirement = r;
    return errSecSuccess;
}

OSStatus SecRequirementCreateWithString(CFStringRef text, SecCSFlags flags, SecRequirementRef *requirement) {
    return SecRequirementCreateWithStringAndErrors(text, flags, NULL, requirement);
}

OSStatus SecRequirementCopyString(SecRequirementRef requirement, SecCSFlags flags, CFStringRef *text) {
    if (requirement == NULL || text == NULL)
        return errSecParam;
    *text = CFRetain(((struct __SecRequirement *)requirement)->text);
    return errSecSuccess;
}
