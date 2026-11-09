/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Jobs go to launchd as launch_data dictionaries made from the property
 * list. Login items are started as jobs labeled with the helper's
 * identifier; SHORTCUT(Atlas): they are not started again at the next
 * login (loginwindow starts only the session's applications).
 */

#include "ServiceManagement.h"
#include <launch.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

const CFStringRef kSMDomainSystemLaunchd = CFSTR("kSMDomainSystemLaunchd");
const CFStringRef kSMDomainUserLaunchd = CFSTR("kSMDomainUserLaunchd");
const CFStringRef kSMErrorDomainIPC = CFSTR("kSMErrorDomainIPC");
const CFStringRef kSMErrorDomainFramework = CFSTR("kSMErrorDomainFramework");
const CFStringRef kSMErrorDomainLaunchd = CFSTR("kSMErrorDomainLaunchd");

static void setError(CFErrorRef *outError, CFStringRef domain, CFIndex code) {
    if (outError != NULL)
        *outError = CFErrorCreate(NULL, domain, code, NULL);
}

static launch_data_t dataOf(CFTypeRef value) {
    CFTypeID type = CFGetTypeID(value);
    if (type == CFStringGetTypeID()) {
        char buffer[PATH_MAX * 2];
        if (!CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8))
            return NULL;
        return launch_data_new_string(buffer);
    }
    if (type == CFBooleanGetTypeID())
        return launch_data_new_bool(CFBooleanGetValue(value));
    if (type == CFNumberGetTypeID()) {
        if (CFNumberIsFloatType(value)) {
            double d = 0;
            CFNumberGetValue(value, kCFNumberDoubleType, &d);
            return launch_data_new_real(d);
        }
        long long n = 0;
        CFNumberGetValue(value, kCFNumberLongLongType, &n);
        return launch_data_new_integer(n);
    }
    if (type == CFArrayGetTypeID()) {
        launch_data_t array = launch_data_alloc(LAUNCH_DATA_ARRAY);
        for (CFIndex i = 0, n = CFArrayGetCount(value); i < n; i++) {
            launch_data_t item = dataOf(CFArrayGetValueAtIndex(value, i));
            if (item != NULL)
                launch_data_array_set_index(array, item, (size_t)i);
        }
        return array;
    }
    if (type == CFDictionaryGetTypeID()) {
        launch_data_t dict = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
        CFIndex n = CFDictionaryGetCount(value);
        const void *keys[n > 0 ? n : 1], *values[n > 0 ? n : 1];
        CFDictionaryGetKeysAndValues(value, keys, values);
        for (CFIndex i = 0; i < n; i++) {
            char key[256];
            launch_data_t item;
            if (CFGetTypeID(keys[i]) != CFStringGetTypeID() ||
                !CFStringGetCString(keys[i], key, sizeof(key), kCFStringEncodingUTF8))
                continue;
            item = dataOf(values[i]);
            if (item != NULL)
                launch_data_dict_insert(dict, item, key);
        }
        return dict;
    }
    return NULL;
}

/* A request's answer: an errno (0 is success), or a value. */
static int answerErrno(launch_data_t answer) {
    int result = EIO;
    if (answer == NULL)
        return errno ? errno : EIO;
    if (launch_data_get_type(answer) == LAUNCH_DATA_ERRNO)
        result = launch_data_get_errno(answer);
    else
        result = 0;
    launch_data_free(answer);
    return result;
}

Boolean SMJobSubmit(CFStringRef domain, CFDictionaryRef job, AuthorizationRef auth, CFErrorRef *outError) {
    launch_data_t request, data;
    if (job == NULL || CFGetTypeID(job) != CFDictionaryGetTypeID() || CFDictionaryGetValue(job, CFSTR("Label")) == NULL) {
        setError(outError, kSMErrorDomainFramework, kSMErrorInvalidPlist);
        return false;
    }
    data = dataOf(job);
    if (data == NULL) {
        setError(outError, kSMErrorDomainFramework, kSMErrorInvalidPlist);
        return false;
    }
    request = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
    launch_data_dict_insert(request, data, LAUNCH_KEY_SUBMITJOB);
    int err = answerErrno(launch_msg(request));
    launch_data_free(request);
    if (err != 0) {
        setError(outError, kSMErrorDomainLaunchd, err);
        return false;
    }
    return true;
}

static launch_data_t labelRequest(const char *key, CFStringRef label) {
    char buffer[256];
    launch_data_t request;
    if (label == NULL || !CFStringGetCString(label, buffer, sizeof(buffer), kCFStringEncodingUTF8))
        return NULL;
    request = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
    launch_data_dict_insert(request, launch_data_new_string(buffer), key);
    return request;
}

Boolean SMJobRemove(CFStringRef domain, CFStringRef jobLabel, AuthorizationRef auth, Boolean wait, CFErrorRef *outError) {
    launch_data_t request = labelRequest(LAUNCH_KEY_REMOVEJOB, jobLabel);
    if (request == NULL) {
        setError(outError, kSMErrorDomainFramework, kSMErrorInvalidPlist);
        return false;
    }
    int err = answerErrno(launch_msg(request));
    launch_data_free(request);
    if (err != 0) {
        setError(outError, kSMErrorDomainLaunchd, err == ESRCH ? kSMErrorJobNotFound : err);
        return false;
    }
    return true;
}

static CFTypeRef valueOf(launch_data_t data);

static void addEntry(launch_data_t value, const char *key, void *context) {
    CFTypeRef v = valueOf(value);
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    if (v != NULL && k != NULL)
        CFDictionarySetValue((CFMutableDictionaryRef)context, k, v);
    if (v != NULL) CFRelease(v);
    if (k != NULL) CFRelease(k);
}

static CFTypeRef valueOf(launch_data_t data) {
    switch (launch_data_get_type(data)) {
    case LAUNCH_DATA_STRING:
        return CFStringCreateWithCString(NULL, launch_data_get_string(data), kCFStringEncodingUTF8);
    case LAUNCH_DATA_BOOL:
        return CFRetain(launch_data_get_bool(data) ? kCFBooleanTrue : kCFBooleanFalse);
    case LAUNCH_DATA_INTEGER: {
        long long n = launch_data_get_integer(data);
        return CFNumberCreate(NULL, kCFNumberLongLongType, &n);
    }
    case LAUNCH_DATA_REAL: {
        double d = launch_data_get_real(data);
        return CFNumberCreate(NULL, kCFNumberDoubleType, &d);
    }
    case LAUNCH_DATA_ARRAY: {
        CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        for (size_t i = 0, n = launch_data_array_get_count(data); i < n; i++) {
            CFTypeRef v = valueOf(launch_data_array_get_index(data, i));
            if (v != NULL) {
                CFArrayAppendValue(array, v);
                CFRelease(v);
            }
        }
        return array;
    }
    case LAUNCH_DATA_DICTIONARY: {
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                &kCFTypeDictionaryValueCallBacks);
        launch_data_dict_iterate(data, addEntry, dict);
        return dict;
    }
    default:
        return NULL;
    }
}

CFDictionaryRef SMJobCopyDictionary(CFStringRef domain, CFStringRef jobLabel) {
    launch_data_t request = labelRequest(LAUNCH_KEY_GETJOB, jobLabel), answer;
    CFTypeRef result = NULL;
    if (request == NULL)
        return NULL;
    answer = launch_msg(request);
    launch_data_free(request);
    if (answer == NULL)
        return NULL;
    if (launch_data_get_type(answer) == LAUNCH_DATA_DICTIONARY)
        result = valueOf(answer);
    launch_data_free(answer);
    return result;
}

/* The executable of the helper with this identifier in the main bundle's
   Contents/Library/LoginItems. */
static CFStringRef copyHelperExecutable(CFStringRef identifier) {
    CFBundleRef main = CFBundleGetMainBundle();
    CFURLRef bundleURL = main != NULL ? CFBundleCopyBundleURL(main) : NULL;
    CFURLRef items = bundleURL != NULL ?
        CFURLCreateCopyAppendingPathComponent(NULL, bundleURL, CFSTR("Contents/Library/LoginItems"), true) : NULL;
    CFArrayRef helpers = items != NULL ? CFBundleCreateBundlesFromDirectory(NULL, items, CFSTR("app")) : NULL;
    CFStringRef path = NULL;

    for (CFIndex i = 0, n = helpers != NULL ? CFArrayGetCount(helpers) : 0; i < n && path == NULL; i++) {
        CFBundleRef helper = (CFBundleRef)CFArrayGetValueAtIndex(helpers, i);
        CFStringRef id = CFBundleGetIdentifier(helper);
        if (id != NULL && CFEqual(id, identifier)) {
            CFURLRef executable = CFBundleCopyExecutableURL(helper);
            if (executable != NULL) {
                path = CFURLCopyFileSystemPath(executable, kCFURLPOSIXPathStyle);
                CFRelease(executable);
            }
        }
    }
    if (helpers != NULL) CFRelease(helpers);
    if (items != NULL) CFRelease(items);
    if (bundleURL != NULL) CFRelease(bundleURL);
    return path;
}

Boolean SMLoginItemSetEnabled(CFStringRef identifier, Boolean enabled) {
    if (identifier == NULL)
        return false;
    if (!enabled) {
        SMJobRemove(kSMDomainUserLaunchd, identifier, NULL, false, NULL);
        return true;
    }
    CFStringRef executable = copyHelperExecutable(identifier);
    if (executable == NULL)
        return false;
    CFArrayRef arguments = CFArrayCreate(NULL, (const void **)&executable, 1, &kCFTypeArrayCallBacks);
    const void *keys[] = { CFSTR("Label"), CFSTR("ProgramArguments"), CFSTR("RunAtLoad") };
    const void *values[] = { identifier, arguments, kCFBooleanTrue };
    CFDictionaryRef job = CFDictionaryCreate(NULL, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
    SMJobRemove(kSMDomainUserLaunchd, identifier, NULL, false, NULL);
    Boolean ok = SMJobSubmit(kSMDomainUserLaunchd, job, NULL, NULL);
    CFRelease(job);
    CFRelease(arguments);
    CFRelease(executable);
    return ok;
}
