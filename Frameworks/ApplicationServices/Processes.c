/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A serial number is {1, pid} for a process, {0, kCurrentProcess} for this
 * one. Activation and the activation policy are AppKit's: they are asked
 * of NSApplication and NSRunningApplication when AppKit is loaded.
 */

#include "Processes.h"
#include "UniversalAccess.h"
#include <CoreServices/MacErrors.h>
#include <CoreGraphics/CGWindow.h>
#include <errno.h>
#include <libproc.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <objc/runtime.h>
#include <objc/message.h>


static const UInt32 kPIDTag = 1;

static pid_t pidOf(const ProcessSerialNumber *psn) {
    if (psn == NULL)
        return -1;
    if (psn->highLongOfPSN == 0 && psn->lowLongOfPSN == kCurrentProcess)
        return getpid();
    if (psn->highLongOfPSN == kPIDTag)
        return (pid_t)psn->lowLongOfPSN;
    return -1;
}

static Boolean alive(pid_t pid) {
    return pid > 0 && (kill(pid, 0) == 0 || errno == EPERM);
}

OSErr GetCurrentProcess(ProcessSerialNumber *PSN) {
    if (PSN == NULL)
        return paramErr;
    PSN->highLongOfPSN = 0;
    PSN->lowLongOfPSN = kCurrentProcess;
    return noErr;
}

OSStatus GetProcessForPID(pid_t pid, ProcessSerialNumber *psn) {
    if (psn == NULL)
        return paramErr;
    if (!alive(pid))
        return procNotFound;
    psn->highLongOfPSN = kPIDTag;
    psn->lowLongOfPSN = (UInt32)pid;
    return noErr;
}

OSStatus GetProcessPID(const ProcessSerialNumber *psn, pid_t *pid) {
    pid_t p = pidOf(psn);
    if (pid == NULL)
        return paramErr;
    if (!alive(p))
        return procNotFound;
    *pid = p;
    return noErr;
}

/* The application's name (its bundle's), else the executable's. */
OSStatus CopyProcessName(const ProcessSerialNumber *psn, CFStringRef *name) {
    pid_t p = pidOf(psn);
    char path[PROC_PIDPATHINFO_MAXSIZE];

    if (name == NULL)
        return paramErr;
    *name = NULL;
    if (!alive(p))
        return procNotFound;
    if (proc_pidpath(p, path, sizeof(path)) > 0) {
        char *app = strstr(path, ".app/Contents/MacOS/");
        if (app != NULL) {
            CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, app + 4 - path, true);
            CFBundleRef bundle = url != NULL ? CFBundleCreate(NULL, url) : NULL;
            if (bundle != NULL) {
                CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleName"));
                if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID())
                    *name = CFStringCreateCopy(NULL, (CFStringRef)value);
                CFRelease(bundle);
            }
            if (url != NULL)
                CFRelease(url);
            if (*name == NULL) {
                *app = '\0';
                char *slash = strrchr(path, '/');
                *name = CFStringCreateWithCString(NULL, slash != NULL ? slash + 1 : path, kCFStringEncodingUTF8);
            }
            return noErr;
        }
        char *slash = strrchr(path, '/');
        *name = CFStringCreateWithCString(NULL, slash != NULL ? slash + 1 : path, kCFStringEncodingUTF8);
        return noErr;
    }
    if (proc_name(p, path, sizeof(path)) > 0) {
        *name = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
        return noErr;
    }
    return procNotFound;
}

OSErr SameProcess(const ProcessSerialNumber *PSN1, const ProcessSerialNumber *PSN2, Boolean *result) {
    if (result == NULL)
        return paramErr;
    pid_t a = pidOf(PSN1), b = pidOf(PSN2);
    if (a < 0 || b < 0)
        return procNotFound;
    *result = (a == b);
    return noErr;
}

/* The owner of the frontmost normal window on screen. */
OSErr GetFrontProcess(ProcessSerialNumber *PSN) {
    CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    pid_t front = -1;

    if (PSN == NULL)
        return paramErr;
    if (windows != NULL) {
        for (CFIndex i = 0, n = CFArrayGetCount(windows); i < n && front < 0; i++) {
            CFDictionaryRef w = CFArrayGetValueAtIndex(windows, i);
            CFNumberRef layer = CFDictionaryGetValue(w, kCGWindowLayer);
            CFNumberRef owner = CFDictionaryGetValue(w, kCGWindowOwnerPID);
            int value = -1;
            if (layer != NULL && CFNumberGetValue(layer, kCFNumberIntType, &value) && value == 0 && owner != NULL)
                CFNumberGetValue(owner, kCFNumberIntType, &front);
        }
        CFRelease(windows);
    }
    if (front < 0)
        return procNotFound;
    return GetProcessForPID(front, PSN);
}

/* Processes in pid order: {kNoProcess} starts, the end is procNotFound. */
OSErr GetNextProcess(ProcessSerialNumber *PSN) {
    pid_t after, pids[4096];
    int bytes, count;

    if (PSN == NULL)
        return paramErr;
    after = (PSN->highLongOfPSN == 0 && PSN->lowLongOfPSN == kNoProcess) ? 0 : pidOf(PSN);
    bytes = proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    if (bytes <= 0)
        return procNotFound;
    count = bytes / (int)sizeof(pid_t);
    pid_t next = -1;
    for (int i = 0; i < count; i++)
        if (pids[i] > after && (next < 0 || pids[i] < next))
            next = pids[i];
    if (next < 0) {
        PSN->highLongOfPSN = 0;
        PSN->lowLongOfPSN = kNoProcess;
        return procNotFound;
    }
    PSN->highLongOfPSN = kPIDTag;
    PSN->lowLongOfPSN = (UInt32)next;
    return noErr;
}

OSErr KillProcess(const ProcessSerialNumber *inProcess) {
    pid_t p = pidOf(inProcess);
    if (!alive(p))
        return procNotFound;
    return kill(p, SIGTERM) == 0 ? noErr : procNotFound;
}

static id sharedApplication(void) {
    Class cls = objc_getClass("NSApplication");
    if (cls == Nil)
        return nil;
    return ((id (*)(id, SEL))objc_msgSend)((id)cls, sel_registerName("sharedApplication"));
}

OSErr SetFrontProcess(const ProcessSerialNumber *PSN) {
    pid_t p = pidOf(PSN);

    if (!alive(p))
        return procNotFound;
    if (p == getpid()) {
        id app = sharedApplication();
        if (app == nil)
            return procNotFound;
        ((void (*)(id, SEL, BOOL))objc_msgSend)(app, sel_registerName("activateIgnoringOtherApps:"), YES);
        return noErr;
    }
    Class running = objc_getClass("NSRunningApplication");
    id other = running != Nil ? ((id (*)(id, SEL, pid_t))objc_msgSend)((id)running,
        sel_registerName("runningApplicationWithProcessIdentifier:"), p) : nil;
    if (other == nil)
        return procNotFound;
    /* NSApplicationActivateIgnoringOtherApps */
    ((BOOL (*)(id, SEL, unsigned long))objc_msgSend)(other, sel_registerName("activateWithOptions:"), 1 << 1);
    return noErr;
}

/* This process only: foreground is NSApplicationActivationPolicyRegular,
   a UI element Accessory, background Prohibited. */
OSStatus TransformProcessType(const ProcessSerialNumber *psn, ProcessApplicationTransformState transformState) {
    long policy;
    id app;

    if (pidOf(psn) != getpid())
        return paramErr;
    switch (transformState) {
    case kProcessTransformToForegroundApplication: policy = 0; break;
    case kProcessTransformToUIElementApplication: policy = 1; break;
    case kProcessTransformToBackgroundApplication: policy = 2; break;
    default: return paramErr;
    }
    app = sharedApplication();
    if (app == nil)
        return noErr;
    ((BOOL (*)(id, SEL, long))objc_msgSend)(app, sel_registerName("setActivationPolicy:"), policy);
    return noErr;
}

Boolean UAZoomEnabled(void) {
    return false;
}

OSStatus UAZoomChangeFocus(const CGRect *inRect, const CGRect *inHighlightRect, UAZoomChangeFocusType inType) {
    return (inRect == NULL) ? paramErr : noErr;
}

/* -- Process information -- */

#include <sys/sysctl.h>
#include <sys/time.h>

/* The app bundle a process runs from, or its executable when it has none. */
static Boolean bundlePath(pid_t p, char *path, size_t size, Boolean *isBundle) {
    if (proc_pidpath(p, path, (uint32_t)size) <= 0)
        return false;
    char *app = strstr(path, ".app/Contents/MacOS/");
    *isBundle = app != NULL;
    if (app != NULL)
        app[4] = '\0';
    return true;
}

static CFDictionaryRef copyInfoPlist(const char *bundle) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)bundle, strlen(bundle), true);
    CFDictionaryRef info = url != NULL ? CFBundleCopyInfoDictionaryInDirectory(url) : NULL;
    if (url != NULL)
        CFRelease(url);
    return info;
}

static Boolean plistBool(CFDictionaryRef info, CFStringRef key) {
    CFTypeRef v = info != NULL ? CFDictionaryGetValue(info, key) : NULL;
    if (v == NULL)
        return false;
    if (CFGetTypeID(v) == CFBooleanGetTypeID())
        return CFBooleanGetValue((CFBooleanRef)v);
    if (CFGetTypeID(v) == CFStringGetTypeID())
        return CFStringCompare((CFStringRef)v, CFSTR("1"), 0) == kCFCompareEqualTo ||
            CFStringCompare((CFStringRef)v, CFSTR("YES"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
    if (CFGetTypeID(v) == CFNumberGetTypeID()) {
        int n = 0;
        CFNumberGetValue((CFNumberRef)v, kCFNumberIntType, &n);
        return n != 0;
    }
    return false;
}

static OSType plistCode(CFDictionaryRef info, CFStringRef key) {
    CFTypeRef v = info != NULL ? CFDictionaryGetValue(info, key) : NULL;
    char code[8] = "????";
    if (v != NULL && CFGetTypeID(v) == CFStringGetTypeID())
        CFStringGetCString((CFStringRef)v, code, sizeof(code), kCFStringEncodingMacRoman);
    return ((OSType)(UInt8)code[0] << 24) | ((OSType)(UInt8)code[1] << 16) |
        ((OSType)(UInt8)code[2] << 8) | (OSType)(UInt8)code[3];
}

/* Ticks (60ths of a second) since the machine started. */
static UInt32 ticksSinceBoot(time_t when) {
    struct timeval boot;
    size_t size = sizeof(boot);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boot, &size, NULL, 0) != 0 || when < boot.tv_sec)
        return 0;
    return (UInt32)((when - boot.tv_sec) * 60);
}


OSErr GetProcessInformation(const ProcessSerialNumber *PSN, ProcessInfoRec *info) {
    pid_t p = pidOf(PSN);
    char path[PROC_PIDPATHINFO_MAXSIZE];
    Boolean isBundle = false;
    struct proc_bsdinfo bsd;

    if (info == NULL)
        return paramErr;
    if (!alive(p) || !bundlePath(p, path, sizeof(path), &isBundle))
        return procNotFound;
    CFDictionaryRef plist = isBundle ? copyInfoPlist(path) : NULL;

    if (info->processName != NULL) {
        CFStringRef name = NULL;
        CopyProcessName(PSN, &name);
        info->processName[0] = 0;
        if (name != NULL) {
            char buf[32];
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingMacRoman) ||
                CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                size_t n = strlen(buf);
                info->processName[0] = (UInt8)(n > 31 ? 31 : n);
                memcpy(info->processName + 1, buf, info->processName[0]);
            }
            CFRelease(name);
        }
    }
    GetProcessForPID(p, &info->processNumber);
    info->processType = isBundle ? plistCode(plist, CFSTR("CFBundlePackageType")) : 'APPL';
    info->processSignature = isBundle ? plistCode(plist, CFSTR("CFBundleSignature")) : '????';
    info->processMode = modeCanBackground | modeNeedSuspendResume | modeHighLevelEventAware |
        modeStationeryAware | modeUseTextEditServices | modeDisplayManagerAware | modeGetAppDiedMsg |
        mode32BitCompatible;
    if (!isBundle || plistBool(plist, CFSTR("LSBackgroundOnly")))
        info->processMode |= modeOnlyBackground;
    info->processLocation = NULL;
    info->processSize = 0;
    info->processFreeMem = 0;
    info->processLauncher.highLongOfPSN = 0;
    info->processLauncher.lowLongOfPSN = kNoProcess;
    info->processLaunchDate = 0;
    if (proc_pidinfo(p, PROC_PIDTBSDINFO, 0, &bsd, sizeof(bsd)) == sizeof(bsd)) {
        if (bsd.pbi_ppid > 1)
            GetProcessForPID((pid_t)bsd.pbi_ppid, &info->processLauncher);
        info->processLaunchDate = ticksSinceBoot((time_t)bsd.pbi_start_tvsec);
    }
    info->processActiveTime = 0;
    if (info->processAppRef != NULL && FSPathMakeRef((const UInt8 *)path, info->processAppRef, NULL) != noErr)
        memset(info->processAppRef, 0, sizeof(FSRef));
    if (plist != NULL)
        CFRelease(plist);
    return noErr;
}

OSStatus GetProcessBundleLocation(const ProcessSerialNumber *psn, FSRef *location) {
    pid_t p = pidOf(psn);
    char path[PROC_PIDPATHINFO_MAXSIZE];
    Boolean isBundle;
    if (location == NULL)
        return paramErr;
    if (!alive(p) || !bundlePath(p, path, sizeof(path), &isBundle))
        return procNotFound;
    return FSPathMakeRef((const UInt8 *)path, location, NULL);
}

CFDictionaryRef ProcessInformationCopyDictionary(const ProcessSerialNumber *PSN, UInt32 infoToReturn) {
    pid_t p = pidOf(PSN);
    char path[PROC_PIDPATHINFO_MAXSIZE];
    Boolean isBundle = false;
    if (!alive(p) || !bundlePath(p, path, sizeof(path), &isBundle))
        return NULL;
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionaryRef plist = isBundle ? copyInfoPlist(path) : NULL;
    const CFStringRef copied[] = { CFSTR("CFBundleIdentifier"), CFSTR("CFBundleName"), CFSTR("CFBundleExecutable"),
        CFSTR("CFBundleVersion"), CFSTR("CFBundleShortVersionString") };
    for (size_t i = 0; plist != NULL && i < sizeof(copied) / sizeof(copied[0]); i++) {
        CFTypeRef v = CFDictionaryGetValue(plist, copied[i]);
        if (v != NULL)
            CFDictionarySetValue(d, copied[i], v);
    }
    CFStringRef bundle = CFStringCreateWithFileSystemRepresentation(NULL, path);
    if (bundle != NULL) {
        CFDictionarySetValue(d, isBundle ? CFSTR("BundlePath") : CFSTR("CFBundleExecutable"), bundle);
        CFRelease(bundle);
    }
    CFNumberRef pid = CFNumberCreate(NULL, kCFNumberIntType, &p);
    CFDictionarySetValue(d, CFSTR("pid"), pid);
    CFRelease(pid);
    ProcessSerialNumber psn;
    if (GetProcessForPID(p, &psn) == noErr) {
        long long n = ((long long)psn.highLongOfPSN << 32) | psn.lowLongOfPSN;
        CFNumberRef v = CFNumberCreate(NULL, kCFNumberLongLongType, &n);
        CFDictionarySetValue(d, CFSTR("PSN"), v);
        CFRelease(v);
    }
    CFDictionarySetValue(d, CFSTR("LSBackgroundOnly"),
        (!isBundle || plistBool(plist, CFSTR("LSBackgroundOnly"))) ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, CFSTR("LSUIElement"), plistBool(plist, CFSTR("LSUIElement")) ? kCFBooleanTrue : kCFBooleanFalse);
    if (plist != NULL)
        CFRelease(plist);
    return d;
}
