/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "Gestalt.h"
#include "MacErrors.h"
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <unistd.h>

typedef struct Value {
    OSType selector;
    SInt32 value;
} Value;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Value *sValues;
static int sCount;

/* ProductVersion (10.15): the version programs compare against. */
static void systemVersion(int *major, int *minor, int *bugfix) {
    static int parts[3] = { -1, 0, 0 };
    if (parts[0] < 0) {
        CFURLRef url = CFURLCreateWithFileSystemPath(NULL, CFSTR("/System/Library/CoreServices/SystemVersion.plist"),
                                                     kCFURLPOSIXPathStyle, false);
        CFReadStreamRef stream = url != NULL ? CFReadStreamCreateWithFile(NULL, url) : NULL;
        CFPropertyListRef plist = NULL;
        parts[0] = 10; parts[1] = 15; parts[2] = 0;
        if (stream != NULL && CFReadStreamOpen(stream)) {
            plist = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListImmutable, NULL, NULL);
            CFReadStreamClose(stream);
        }
        if (plist != NULL && CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
            CFStringRef version = CFDictionaryGetValue(plist, CFSTR("ProductVersion"));
            char buffer[32];
            if (version != NULL && CFGetTypeID(version) == CFStringGetTypeID() &&
                CFStringGetCString(version, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                int a = 0, b = 0, c = 0;
                if (sscanf(buffer, "%d.%d.%d", &a, &b, &c) >= 2) {
                    parts[0] = a; parts[1] = b; parts[2] = c;
                }
            }
        }
        if (plist != NULL) CFRelease(plist);
        if (stream != NULL) CFRelease(stream);
        if (url != NULL) CFRelease(url);
    }
    *major = parts[0]; *minor = parts[1]; *bugfix = parts[2];
}

static SInt32 clamp(uint64_t value) {
    return value > INT32_MAX ? INT32_MAX : (SInt32)value;
}

OSErr Gestalt(OSType selector, SInt32 *response) {
    int major, minor, bugfix;
    uint64_t memory = 0;
    size_t length = sizeof(memory);

    if (response == NULL)
        return paramErr;
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < sCount; i++)
        if (sValues[i].selector == selector) {
            *response = sValues[i].value;
            pthread_mutex_unlock(&sLock);
            return noErr;
        }
    pthread_mutex_unlock(&sLock);
    switch (selector) {
    case gestaltSystemVersion:
        /* BCD, the minor and bug fix at most 9 (0x1090 from 10.9 on). */
        systemVersion(&major, &minor, &bugfix);
        if (major != 10) { *response = 0x1090; return noErr; }
        *response = 0x1000 | ((minor > 9 ? 9 : minor) << 4) | (bugfix > 9 ? 9 : bugfix);
        return noErr;
    case gestaltSystemVersionMajor:
        systemVersion(&major, &minor, &bugfix); *response = major; return noErr;
    case gestaltSystemVersionMinor:
        systemVersion(&major, &minor, &bugfix); *response = minor; return noErr;
    case gestaltSystemVersionBugFix:
        systemVersion(&major, &minor, &bugfix); *response = bugfix; return noErr;
    case gestaltPhysicalRAMSize:
    case gestaltLogicalRAMSize:
        sysctlbyname("hw.memsize", &memory, &length, NULL, 0);
        *response = clamp(memory);
        return noErr;
    case gestaltPhysicalRAMSizeInMegabytes:
        sysctlbyname("hw.memsize", &memory, &length, NULL, 0);
        *response = clamp(memory >> 20);
        return noErr;
    case gestaltSysArchitecture:
        *response = gestaltIntel;
        return noErr;
    case gestaltCountOfCPUs: {
        int cpus = 1;
        size_t n = sizeof(cpus);
        sysctlbyname("hw.ncpu", &cpus, &n, NULL, 0);
        *response = cpus;
        return noErr;
    }
    case gestaltLogicalPageSize:
        *response = (SInt32)getpagesize();
        return noErr;
    case gestaltProcClkSpeed:
    case gestaltProcClkSpeedMHz: {
        uint64_t hz = 0;
        size_t n = sizeof(hz);
        if (sysctlbyname("hw.cpufrequency", &hz, &n, NULL, 0) != 0 || hz == 0)
            return gestaltUndefSelectorErr;
        *response = clamp(selector == gestaltProcClkSpeedMHz ? hz / 1000000 : hz);
        return noErr;
    }
    default:
        return gestaltUndefSelectorErr;
    }
}

static OSErr setValue(OSType selector, SInt32 value, int mustExist) {
    OSErr err = noErr;
    pthread_mutex_lock(&sLock);
    int i;
    for (i = 0; i < sCount && sValues[i].selector != selector; i++)
        ;
    if (i < sCount) {
        if (mustExist == 0)
            err = gestaltDupSelectorErr;
        else
            sValues[i].value = value;
    } else if (mustExist == 1) {
        err = gestaltUndefSelectorErr;
    } else {
        Value *grown = realloc(sValues, (size_t)(sCount + 1) * sizeof(Value));
        if (grown == NULL)
            err = memFullErr;
        else {
            sValues = grown;
            sValues[sCount++] = (Value){ selector, value };
        }
    }
    pthread_mutex_unlock(&sLock);
    return err;
}

OSErr NewGestaltValue(OSType selector, SInt32 newValue) { return setValue(selector, newValue, 0); }
OSErr ReplaceGestaltValue(OSType selector, SInt32 replacementValue) { return setValue(selector, replacementValue, 1); }
OSErr SetGestaltValue(OSType selector, SInt32 newValue) { return setValue(selector, newValue, 2); }

OSErr DeleteGestaltValue(OSType selector) {
    OSErr err = gestaltUndefSelectorErr;
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < sCount; i++)
        if (sValues[i].selector == selector) {
            sValues[i] = sValues[--sCount];
            err = noErr;
            break;
        }
    pthread_mutex_unlock(&sLock);
    return err;
}
