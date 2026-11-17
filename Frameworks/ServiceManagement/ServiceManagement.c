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

/* ---- SMJobBless: a privileged helper installed from the app ---- */

#include <fcntl.h>
#include <unistd.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <libkern/OSByteOrder.h>

#define kHelperTools	"/Library/PrivilegedHelperTools"
#define kLaunchDaemons	"/Library/LaunchDaemons"

/* A __TEXT section of the helper (its x86_64 slice in a universal file),
   as a property list. */
static CFPropertyListRef embeddedPlist(const char *path, const char *section) {
    int fd = open(path, O_RDONLY);
    struct stat st;
    uint8_t *file = NULL;
    CFPropertyListRef plist = NULL;

    if (fd < 0 || fstat(fd, &st) != 0 || st.st_size < (off_t)sizeof(struct mach_header_64))
        goto out;
    file = malloc((size_t)st.st_size);
    if (file == NULL || pread(fd, file, (size_t)st.st_size, 0) != st.st_size)
        goto out;
    size_t base = 0, size = (size_t)st.st_size;
    uint32_t magic = *(uint32_t *)file;
    if (magic == FAT_CIGAM || magic == FAT_MAGIC) {
        struct fat_header *fh = (struct fat_header *)file;
        uint32_t n = OSSwapBigToHostInt32(fh->nfat_arch);
        struct fat_arch *fa = (struct fat_arch *)(fh + 1);
        base = 0;
        for (uint32_t i = 0; i < n && (uint8_t *)(fa + i + 1) <= file + size; i++)
            if ((cpu_type_t)OSSwapBigToHostInt32(fa[i].cputype) == CPU_TYPE_X86_64) {
                base = OSSwapBigToHostInt32(fa[i].offset);
                size = OSSwapBigToHostInt32(fa[i].size);
                break;
            }
        if (base == 0 || base + size > (size_t)st.st_size)
            goto out;
    }
    struct mach_header_64 *mh = (struct mach_header_64 *)(file + base);
    if (size < sizeof(*mh) || mh->magic != MH_MAGIC_64)
        goto out;
    uint8_t *lc = (uint8_t *)(mh + 1), *end = file + base + size;
    for (uint32_t i = 0; i < mh->ncmds && lc + sizeof(struct load_command) <= end; i++) {
        struct load_command *cmd = (struct load_command *)lc;
        if (cmd->cmdsize == 0 || lc + cmd->cmdsize > end)
            break;
        if (cmd->cmd == LC_SEGMENT_64) {
            struct segment_command_64 *seg = (struct segment_command_64 *)lc;
            struct section_64 *sect = (struct section_64 *)(seg + 1);
            for (uint32_t j = 0; j < seg->nsects; j++)
                if (strncmp(sect[j].segname, "__TEXT", 16) == 0 &&
                    strncmp(sect[j].sectname, section, 16) == 0 &&
                    sect[j].offset + sect[j].size <= size) {
                    CFDataRef data = CFDataCreate(NULL, file + base + sect[j].offset, (CFIndex)sect[j].size);
                    plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
                    CFRelease(data);
                    goto out;
                }
        }
        lc += cmd->cmdsize;
    }
out:
    free(file);
    if (fd >= 0)
        close(fd);
    return plist;
}

/* Copies from to to (written whole, then renamed into place), owned by
   root, mode 0544. */
static Boolean installTool(const char *from, const char *to) {
    char tmp[PATH_MAX];
    char buf[65536];
    ssize_t n;
    int in, out;
    Boolean ok = false;

    snprintf(tmp, sizeof(tmp), "%s.%d", to, (int)getpid());
    if ((in = open(from, O_RDONLY)) < 0)
        return false;
    out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0544);
    if (out >= 0) {
        ok = true;
        while ((n = read(in, buf, sizeof(buf))) > 0)
            if (write(out, buf, (size_t)n) != n) {
                ok = false;
                break;
            }
        if (n < 0 || fchown(out, 0, 0) != 0 || fchmod(out, 0544) != 0 || close(out) != 0)
            ok = false;
        if (ok && rename(tmp, to) != 0)
            ok = false;
        if (!ok)
            unlink(tmp);
    }
    close(in);
    return ok;
}

/* The helper Contents/Library/LaunchServices/<label> of the main bundle,
   installed in /Library/PrivilegedHelperTools and run by the system's
   launchd as its embedded __launchd_plist says. Both sides must name each
   other: the app's SMPrivilegedExecutables has the label, the helper's
   embedded Info.plist has SMAuthorizedClients (their code requirements are
   not evaluated: Minkar does not sign code yet). Installing needs root:
   with no authorization service to act for an app yet, a process that is
   not root fails with kSMErrorAuthorizationFailure. */
Boolean SMJobBless(CFStringRef domain, CFStringRef executableLabel, AuthorizationRef auth, CFErrorRef *outError) {
    char label[256], tool[PATH_MAX], dest[PATH_MAX], plistPath[PATH_MAX];
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFURLRef bundleURL = NULL;
    CFPropertyListRef launchd = NULL, info = NULL;
    CFMutableDictionaryRef job = NULL;
    Boolean ok = false;

    (void)auth;
    if (domain == NULL || !CFEqual(domain, kSMDomainSystemLaunchd) || executableLabel == NULL ||
        !CFStringGetCString(executableLabel, label, sizeof(label), kCFStringEncodingUTF8) ||
        strchr(label, '/') != NULL || label[0] == '.') {
        setError(outError, kSMErrorDomainFramework, kSMErrorInternalFailure);
        return false;
    }
    /* The app names the helper. */
    CFDictionaryRef wanted = bundle ? CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("SMPrivilegedExecutables")) : NULL;
    if (wanted == NULL || CFGetTypeID(wanted) != CFDictionaryGetTypeID() ||
        CFDictionaryGetValue(wanted, executableLabel) == NULL) {
        setError(outError, kSMErrorDomainFramework, kSMErrorToolNotValid);
        return false;
    }
    bundleURL = CFBundleCopyBundleURL(bundle);
    if (bundleURL == NULL || !CFURLGetFileSystemRepresentation(bundleURL, true, (UInt8 *)tool, sizeof(tool))) {
        setError(outError, kSMErrorDomainFramework, kSMErrorInternalFailure);
        goto out;
    }
    strlcat(tool, "/Contents/Library/LaunchServices/", sizeof(tool));
    strlcat(tool, label, sizeof(tool));
    launchd = embeddedPlist(tool, "__launchd_plist");
    info = embeddedPlist(tool, "__info_plist");
    if (launchd == NULL || CFGetTypeID(launchd) != CFDictionaryGetTypeID()) {
        setError(outError, kSMErrorDomainFramework, kSMErrorJobPlistNotFound);
        goto out;
    }
    /* The helper names the app, and its job is the label. */
    CFTypeRef clients = info && CFGetTypeID(info) == CFDictionaryGetTypeID() ?
        CFDictionaryGetValue(info, CFSTR("SMAuthorizedClients")) : NULL;
    CFTypeRef jobLabel = CFDictionaryGetValue(launchd, CFSTR("Label"));
    if (clients == NULL || jobLabel == NULL || !CFEqual(jobLabel, executableLabel)) {
        setError(outError, kSMErrorDomainFramework, kSMErrorToolNotValid);
        goto out;
    }
    if (geteuid() != 0) {
        setError(outError, kSMErrorDomainFramework, kSMErrorAuthorizationFailure);
        goto out;
    }
    snprintf(dest, sizeof(dest), kHelperTools "/%s", label);
    snprintf(plistPath, sizeof(plistPath), kLaunchDaemons "/%s.plist", label);
    mkdir(kHelperTools, 0755);
    if (!installTool(tool, dest)) {
        setError(outError, kSMErrorDomainFramework, kSMErrorInternalFailure);
        goto out;
    }
    /* Its job runs the installed copy, and is kept for the next start. */
    job = CFDictionaryCreateMutableCopy(NULL, 0, launchd);
    CFStringRef program = CFStringCreateWithCString(NULL, dest, kCFStringEncodingUTF8);
    CFDictionarySetValue(job, CFSTR("Program"), program);
    CFRelease(program);
    CFDataRef xml = CFPropertyListCreateData(NULL, job, kCFPropertyListXMLFormat_v1_0, 0, NULL);
    int fd = xml ? open(plistPath, O_WRONLY | O_CREAT | O_TRUNC, 0644) : -1;
    Boolean written = fd >= 0 && write(fd, CFDataGetBytePtr(xml), (size_t)CFDataGetLength(xml)) == CFDataGetLength(xml);
    if (fd >= 0)
        close(fd);
    if (xml)
        CFRelease(xml);
    if (!written) {
        setError(outError, kSMErrorDomainFramework, kSMErrorInternalFailure);
        goto out;
    }
    SMJobRemove(kSMDomainSystemLaunchd, executableLabel, NULL, true, NULL);
    ok = SMJobSubmit(kSMDomainSystemLaunchd, job, NULL, outError);
out:
    if (job)
        CFRelease(job);
    if (launchd)
        CFRelease(launchd);
    if (info)
        CFRelease(info);
    if (bundleURL)
        CFRelease(bundleURL);
    return ok;
}
