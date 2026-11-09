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
