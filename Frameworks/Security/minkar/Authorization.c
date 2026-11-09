/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SHORTCUT(19.6.0): there is no authorization server or dialog. A right is
 * granted when its rule allows it, or when the user is an administrator
 * (root, or in the admin or wheel group) and the rule asks for one; others
 * are denied without interaction.
 */

#include "Security.h"
#include "SecInternal.h"
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

#define kRightsPath "/etc/authorization.plist"
#define kSudoPath "/usr/bin/sudo"

struct AuthorizationOpaqueRef {
    uint32_t magic;
    CFMutableSetRef granted;
};

enum { kAuthMagic = 0x4d6b4175 }; /* 'MkAu' */

static Boolean valid(AuthorizationRef a) {
    return a != NULL && a->magic == kAuthMagic;
}

/* root, or a member of admin or wheel. */
static Boolean isAdministrator(void) {
    uid_t uid = geteuid();
    struct passwd *pw;
    gid_t groups[64];
    int count = 64;

    if (uid == 0)
        return true;
    pw = getpwuid(uid);
    if (pw == NULL || getgrouplist(pw->pw_name, (int)pw->pw_gid, (int *)groups, &count) != 0)
        return false;
    for (int i = 0; i < count; i++) {
        struct group *g = getgrgid(groups[i]);
        if (g != NULL && (strcmp(g->gr_name, "admin") == 0 || strcmp(g->gr_name, "wheel") == 0))
            return true;
    }
    return false;
}

static CFMutableDictionaryRef copyRightsDatabase(void) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)kRightsPath, strlen(kRightsPath), false);
    CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, url);
    CFPropertyListRef plist = NULL;
    CFMutableDictionaryRef rights;

    if (stream != NULL && CFReadStreamOpen(stream)) {
        plist = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListMutableContainers, NULL, NULL);
        CFReadStreamClose(stream);
    }
    if (stream != NULL) CFRelease(stream);
    CFRelease(url);
    if (plist != NULL && CFGetTypeID(plist) == CFDictionaryGetTypeID())
        rights = (CFMutableDictionaryRef)plist;
    else {
        if (plist != NULL) CFRelease(plist);
        rights = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    return rights;
}

static OSStatus writeRightsDatabase(CFDictionaryRef rights) {
    CFDataRef data = CFPropertyListCreateData(NULL, rights, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    OSStatus status = errAuthorizationInternal;
    if (data != NULL) {
        int fd = open(kRightsPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            if (write(fd, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data)) == CFDataGetLength(data))
                status = errAuthorizationSuccess;
            close(fd);
        } else
            status = (errno == EACCES || errno == EPERM) ? errAuthorizationDenied : errAuthorizationInternal;
        CFRelease(data);
    }
    return status;
}

/* The rule of a right: its definition's "rule" or "class", else the
   administrator rule. */
static Boolean ruleGrants(const char *name) {
    CFMutableDictionaryRef rights = copyRightsDatabase();
    CFStringRef key = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    CFDictionaryRef definition = key != NULL ? CFDictionaryGetValue(rights, key) : NULL;
    Boolean granted;
    CFTypeRef rule = NULL;

    if (definition != NULL && CFGetTypeID(definition) == CFDictionaryGetTypeID()) {
        rule = CFDictionaryGetValue(definition, CFSTR("class"));
        if (rule == NULL)
            rule = CFDictionaryGetValue(definition, CFSTR("rule"));
        if (rule != NULL && CFGetTypeID(rule) == CFArrayGetTypeID() && CFArrayGetCount(rule) > 0)
            rule = CFArrayGetValueAtIndex(rule, 0);
    }
    if (rule != NULL && CFGetTypeID(rule) == CFStringGetTypeID() && CFEqual(rule, CFSTR(kAuthorizationRuleClassAllow)))
        granted = true;
    else if (rule != NULL && CFGetTypeID(rule) == CFStringGetTypeID() && CFEqual(rule, CFSTR(kAuthorizationRuleClassDeny)))
        granted = false;
    else
        granted = isAdministrator();
    if (key != NULL) CFRelease(key);
    CFRelease(rights);
    return granted;
}

static OSStatus copyRights(AuthorizationRef a, const AuthorizationRights *rights, AuthorizationFlags flags,
                           AuthorizationRights **authorizedRights) {
    UInt32 count = rights != NULL ? rights->count : 0, granted = 0;
    AuthorizationItem *items = count ? calloc(count, sizeof(AuthorizationItem)) : NULL;
    OSStatus status = errAuthorizationSuccess;

    for (UInt32 i = 0; i < count; i++) {
        const char *name = rights->items[i].name;
        if (name == NULL) {
            status = errAuthorizationInvalidSet;
            break;
        }
        CFStringRef key = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        Boolean ok = CFSetContainsValue(a->granted, key) || ruleGrants(name);
        if (ok) {
            if (flags & kAuthorizationFlagExtendRights)
                CFSetAddValue(a->granted, key);
            items[granted].name = strdup(name);
            granted++;
        } else if (!(flags & kAuthorizationFlagPartialRights))
            status = errAuthorizationDenied;
        CFRelease(key);
        if (status != errAuthorizationSuccess)
            break;
    }
    if (status == errAuthorizationSuccess && authorizedRights != NULL) {
        AuthorizationRights *out = calloc(1, sizeof(AuthorizationRights));
        out->count = granted;
        out->items = items;
        *authorizedRights = out;
        items = NULL;
    }
    if (items != NULL) {
        for (UInt32 i = 0; i < granted; i++)
            free((void *)items[i].name);
        free(items);
    }
    return status;
}

OSStatus AuthorizationCreate(const AuthorizationRights *rights, const AuthorizationEnvironment *environment,
                             AuthorizationFlags flags, AuthorizationRef *authorization) {
    struct AuthorizationOpaqueRef *a = calloc(1, sizeof(*a));
    OSStatus status = errAuthorizationSuccess;

    if (a == NULL)
        return errAuthorizationInternal;
    a->magic = kAuthMagic;
    a->granted = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    if (rights != NULL && rights->count > 0 && (flags & kAuthorizationFlagExtendRights))
        status = copyRights(a, rights, flags, NULL);
    if (status != errAuthorizationSuccess || authorization == NULL) {
        CFRelease(a->granted);
        free(a);
        return status;
    }
    *authorization = a;
    return errAuthorizationSuccess;
}

OSStatus AuthorizationFree(AuthorizationRef authorization, AuthorizationFlags flags) {
    struct AuthorizationOpaqueRef *a = (struct AuthorizationOpaqueRef *)authorization;
    if (!valid(a))
        return errAuthorizationInvalidRef;
    a->magic = 0;
    CFRelease(a->granted);
    free(a);
    return errAuthorizationSuccess;
}

OSStatus AuthorizationCopyRights(AuthorizationRef authorization, const AuthorizationRights *rights,
                                 const AuthorizationEnvironment *environment, AuthorizationFlags flags,
                                 AuthorizationRights **authorizedRights) {
    if (!valid(authorization))
        return errAuthorizationInvalidRef;
    if (rights == NULL)
        return errAuthorizationInvalidSet;
    return copyRights(authorization, rights, flags, authorizedRights);
}

OSStatus AuthorizationCopyInfo(AuthorizationRef authorization, AuthorizationString tag, AuthorizationItemSet **info) {
    if (!valid(authorization))
        return errAuthorizationInvalidRef;
    if (info != NULL)
        *info = calloc(1, sizeof(AuthorizationItemSet));
    return errAuthorizationSuccess;
}

OSStatus AuthorizationFreeItemSet(AuthorizationItemSet *set) {
    if (set == NULL)
        return errAuthorizationInvalidSet;
    for (UInt32 i = 0; i < set->count; i++) {
        free((void *)set->items[i].name);
        free(set->items[i].value);
    }
    free(set->items);
    free(set);
    return errAuthorizationSuccess;
}

/* The form names the user the rights were asked for; an authorization
   made from it asks the rules again. */
OSStatus AuthorizationMakeExternalForm(AuthorizationRef authorization, AuthorizationExternalForm *extForm) {
    if (!valid(authorization))
        return errAuthorizationInvalidRef;
    if (extForm == NULL)
        return errAuthorizationInvalidPointer;
    memset(extForm, 0, sizeof(*extForm));
    uint32_t magic = kAuthMagic, uid = (uint32_t)geteuid();
    memcpy(extForm->bytes, &magic, 4);
    memcpy(extForm->bytes + 4, &uid, 4);
    arc4random_buf(extForm->bytes + 8, sizeof(extForm->bytes) - 8);
    return errAuthorizationSuccess;
}

OSStatus AuthorizationCreateFromExternalForm(const AuthorizationExternalForm *extForm, AuthorizationRef *authorization) {
    uint32_t magic = 0;
    if (extForm == NULL || authorization == NULL)
        return errAuthorizationInvalidPointer;
    memcpy(&magic, extForm->bytes, 4);
    if (magic != kAuthMagic)
        return errAuthorizationInvalidRef;
    return AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, authorization);
}

/* The tool's standard input and output are one socket: the pipe. */
OSStatus AuthorizationExecuteWithPrivileges(AuthorizationRef authorization, const char *pathToTool, AuthorizationFlags options,
                                            char *const *arguments, FILE **communicationsPipe) {
    int sockets[2] = { -1, -1 };
    size_t argc = 0;
    posix_spawn_file_actions_t actions;
    pid_t pid;
    int err;

    if (!valid(authorization))
        return errAuthorizationInvalidRef;
    if (pathToTool == NULL)
        return errAuthorizationInvalidPointer;
    if (!isAdministrator())
        return errAuthorizationDenied;
    while (arguments != NULL && arguments[argc] != NULL)
        argc++;
    const char **argv = calloc(argc + 4, sizeof(char *));
    size_t n = 0;
    if (geteuid() != 0) {
        argv[n++] = kSudoPath;
        argv[n++] = "-n";
    }
    argv[n++] = pathToTool;
    for (size_t i = 0; i < argc; i++)
        argv[n++] = arguments[i];
    argv[n] = NULL;
    if (communicationsPipe != NULL && socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
        free(argv);
        return errAuthorizationToolEnvironmentError;
    }
    posix_spawn_file_actions_init(&actions);
    if (communicationsPipe != NULL) {
        posix_spawn_file_actions_adddup2(&actions, sockets[1], STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions, sockets[1], STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions, sockets[0]);
    }
    err = posix_spawn(&pid, argv[0], &actions, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    free(argv);
    if (communicationsPipe != NULL)
        close(sockets[1]);
    if (err != 0) {
        if (communicationsPipe != NULL)
            close(sockets[0]);
        return errAuthorizationToolExecuteFailure;
    }
    if (communicationsPipe != NULL)
        *communicationsPipe = fdopen(sockets[0], "r+");
    return errAuthorizationSuccess;
}

OSStatus AuthorizationRightGet(const char *rightName, CFDictionaryRef *rightDefinition) {
    CFMutableDictionaryRef rights;
    CFStringRef key;
    CFDictionaryRef definition;

    if (rightName == NULL)
        return errAuthorizationInvalidPointer;
    if (rightDefinition != NULL)
        *rightDefinition = NULL;
    rights = copyRightsDatabase();
    key = CFStringCreateWithCString(NULL, rightName, kCFStringEncodingUTF8);
    definition = CFDictionaryGetValue(rights, key);
    if (definition == NULL && strcmp(rightName, kAuthorizationRightExecute) == 0) {
        const void *k[] = { CFSTR("class"), CFSTR("group") };
        const void *v[] = { CFSTR("user"), CFSTR("admin") };
        CFDictionaryRef builtIn = CFDictionaryCreate(NULL, k, v, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (rightDefinition != NULL)
            *rightDefinition = builtIn;
        else
            CFRelease(builtIn);
        CFRelease(key);
        CFRelease(rights);
        return errAuthorizationSuccess;
    }
    if (definition != NULL && rightDefinition != NULL)
        *rightDefinition = CFRetain(definition);
    CFRelease(key);
    CFRelease(rights);
    return (definition != NULL) ? errAuthorizationSuccess : errAuthorizationDenied;
}

OSStatus AuthorizationRightSet(AuthorizationRef authRef, const char *rightName, CFTypeRef rightDefinition,
                               CFStringRef descriptionKey, CFBundleRef bundle, CFStringRef localeTableName) {
    CFMutableDictionaryRef rights, definition;
    CFStringRef key;
    OSStatus status;

    if (!valid(authRef))
        return errAuthorizationInvalidRef;
    if (rightName == NULL || rightDefinition == NULL)
        return errAuthorizationInvalidPointer;
    if (!isAdministrator())
        return errAuthorizationDenied;
    rights = copyRightsDatabase();
    key = CFStringCreateWithCString(NULL, rightName, kCFStringEncodingUTF8);
    if (CFGetTypeID(rightDefinition) == CFDictionaryGetTypeID())
        definition = CFDictionaryCreateMutableCopy(NULL, 0, rightDefinition);
    else {
        definition = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(definition, CFSTR("rule"), rightDefinition);
    }
    if (descriptionKey != NULL)
        CFDictionarySetValue(definition, CFSTR("comment"), descriptionKey);
    CFDictionarySetValue(rights, key, definition);
    status = writeRightsDatabase(rights);
    CFRelease(definition);
    CFRelease(key);
    CFRelease(rights);
    return status;
}

OSStatus AuthorizationRightRemove(AuthorizationRef authRef, const char *rightName) {
    CFMutableDictionaryRef rights;
    CFStringRef key;
    OSStatus status;

    if (!valid(authRef))
        return errAuthorizationInvalidRef;
    if (rightName == NULL)
        return errAuthorizationInvalidPointer;
    if (!isAdministrator())
        return errAuthorizationDenied;
    rights = copyRightsDatabase();
    key = CFStringCreateWithCString(NULL, rightName, kCFStringEncodingUTF8);
    CFDictionaryRemoveValue(rights, key);
    status = writeRightsDatabase(rights);
    CFRelease(key);
    CFRelease(rights);
    return status;
}
