/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Process taps and process objects (AU10).
 *
 * A process object stands for a process (its ID is the pid with
 * kFirstProcessID added): a tap's description names processes by them.
 * The system object lists the processes that play or record now (the
 * audio service knows them).
 *
 * A tap is the audio service's (create_tap): it mixes what the processes
 * play on a device into a ring it shares with this process. Here a tap is
 * an object with its UID (the description's UUID), description and
 * format; an aggregate device whose description lists the tap reads it as
 * an input (AudioAggregate.c).
 */

#import "CATapDescription.h"
#include "AudioAggregate.h"
#include "AudioDevices.h"
#include "AudioServerTypes.h"
#include "AudioServerUser.h"
#include <libproc.h>
#include <mach/mach.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <string.h>

#define kFirstProcessID 0x10000000
#define kFirstTapID 0x500
#define kMaxTaps 8

typedef struct Tap {
    AudioObjectID id;           /* 0: free */
    int32_t serviceID;
    CFStringRef uid;
    CATapDescription *description;
    AudioServerTapArea *area;
    vm_size_t areaSize;
    mach_port_t client;         /* its death (ours) ends the tap */
} Tap;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Tap taps[kMaxTaps];

static mach_port_t service(void) {
    static mach_port_t port = MACH_PORT_NULL;
    if (port == MACH_PORT_NULL && bootstrap_look_up(bootstrap_port, kAudioServerName, &port) != KERN_SUCCESS)
        port = MACH_PORT_NULL;
    return port;
}

/* ---- Process objects ---- */

Boolean _AudioProcessIs(AudioObjectID id) {
    return id > kFirstProcessID && id < kFirstProcessID + 0x1000000;
}

AudioObjectID _AudioProcessWithPID(pid_t pid) {
    return pid > 0 ? kFirstProcessID + (AudioObjectID)pid : kAudioObjectUnknown;
}

UInt32 _AudioProcessCopyIDs(AudioObjectID *ids, UInt32 capacity) {
    vm_offset_t data = 0;
    mach_msg_type_number_t count = 0;
    UInt32 n = 0;
    if (service() == MACH_PORT_NULL || as_copy_processes(service(), &data, &count) != KERN_SUCCESS || data == 0)
        return 0;
    for (UInt32 i = 0; i < count / sizeof(int32_t); i++) {
        if (n < capacity)
            ids[n] = _AudioProcessWithPID(((const int32_t *)data)[i]);
        n++;
    }
    vm_deallocate(mach_task_self(), data, count);
    return n;
}

static Boolean isAudioClient(pid_t pid) {
    AudioObjectID ids[64];
    UInt32 n = _AudioProcessCopyIDs(ids, 64);
    for (UInt32 i = 0; i < n && i < 64; i++)
        if (ids[i] == _AudioProcessWithPID(pid))
            return true;
    return false;
}

Boolean _AudioProcessHasProperty(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
    case kAudioObjectPropertyBaseClass:
    case kAudioProcessPropertyPID:
    case kAudioProcessPropertyBundleID:
    case kAudioProcessPropertyIsRunning:
    case kAudioProcessPropertyIsRunningOutput:
        return true;
    default:
        return false;
    }
}

UInt32 _AudioProcessPropertySize(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    return a->mSelector == kAudioProcessPropertyBundleID ? sizeof(CFStringRef) : sizeof(UInt32);
}

/* The bundle the process's executable is in (…/X.app/Contents/MacOS/x). */
static CFStringRef copyBundleID(pid_t pid) {
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, path, sizeof(path)) <= 0)
        return CFSTR("");
    char *macos = strstr(path, "/Contents/MacOS/");
    if (macos == NULL)
        return CFSTR("");
    *macos = '\0';
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), true);
    CFBundleRef bundle = CFBundleCreate(NULL, url);
    CFStringRef identifier = bundle != NULL ? CFBundleGetIdentifier(bundle) : NULL;
    CFStringRef result = identifier != NULL ? CFStringCreateCopy(NULL, identifier) : CFSTR("");
    if (bundle != NULL)
        CFRelease(bundle);
    CFRelease(url);
    return result;
}

OSStatus _AudioProcessGetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a, UInt32 *ioSize, void *out) {
    pid_t pid = (pid_t)(id - kFirstProcessID);
    UInt32 size = _AudioProcessPropertySize(id, a);
    if (!_AudioProcessHasProperty(id, a))
        return kAudioHardwareUnknownPropertyError;
    if (ioSize == NULL || *ioSize < size || out == NULL)
        return kAudioHardwareBadPropertySizeError;
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        *(UInt32 *)out = kAudioProcessClassID;
        break;
    case kAudioObjectPropertyBaseClass:
        *(UInt32 *)out = kAudioObjectClassID;
        break;
    case kAudioProcessPropertyPID:
        *(pid_t *)out = pid;
        break;
    case kAudioProcessPropertyBundleID:
        *(CFStringRef *)out = copyBundleID(pid);
        break;
    case kAudioProcessPropertyIsRunning:
    case kAudioProcessPropertyIsRunningOutput:
        *(UInt32 *)out = isAudioClient(pid);
        break;
    }
    *ioSize = size;
    return noErr;
}

/* ---- Taps ---- */

static Tap *find(AudioObjectID id) {
    if (id < kFirstTapID || id >= kFirstTapID + kMaxTaps)
        return NULL;
    Tap *t = &taps[id - kFirstTapID];
    return t->id == id ? t : NULL;
}

Boolean _AudioTapIs(AudioObjectID id) {
    pthread_mutex_lock(&lock);
    Boolean is = find(id) != NULL;
    pthread_mutex_unlock(&lock);
    return is;
}

UInt32 _AudioTapCopyIDs(AudioObjectID *ids, UInt32 capacity) {
    UInt32 n = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxTaps; i++)
        if (taps[i].id != 0) {
            if (n < capacity)
                ids[n] = taps[i].id;
            n++;
        }
    pthread_mutex_unlock(&lock);
    return n;
}

AudioObjectID _AudioTapWithUID(CFStringRef uid) {
    AudioObjectID id = kAudioObjectUnknown;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxTaps && uid != NULL; i++)
        if (taps[i].id != 0 && CFStringCompare(taps[i].uid, uid, kCFCompareCaseInsensitive) == kCFCompareEqualTo)
            id = taps[i].id;
    pthread_mutex_unlock(&lock);
    return id;
}

AudioServerTapArea *_AudioTapArea(AudioObjectID id) {
    pthread_mutex_lock(&lock);
    Tap *t = find(id);
    AudioServerTapArea *area = t != NULL ? t->area : NULL;
    pthread_mutex_unlock(&lock);
    return area;
}

Boolean _AudioTapHasProperty(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
    case kAudioObjectPropertyBaseClass:
    case kAudioObjectPropertyName:
    case kAudioTapPropertyUID:
    case kAudioTapPropertyDescription:
    case kAudioTapPropertyFormat:
        return true;
    default:
        return false;
    }
}

UInt32 _AudioTapPropertySize(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    switch (a->mSelector) {
    case kAudioTapPropertyFormat:
        return sizeof(AudioStreamBasicDescription);
    case kAudioObjectPropertyName:
    case kAudioTapPropertyUID:
    case kAudioTapPropertyDescription:
        return sizeof(void *);
    default:
        return sizeof(UInt32);
    }
}

OSStatus _AudioTapGetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a, UInt32 *ioSize, void *out) {
    UInt32 size = _AudioTapPropertySize(id, a);
    if (!_AudioTapHasProperty(id, a))
        return kAudioHardwareUnknownPropertyError;
    if (ioSize == NULL || *ioSize < size || out == NULL)
        return kAudioHardwareBadPropertySizeError;
    pthread_mutex_lock(&lock);
    Tap *t = find(id);
    if (t == NULL) {
        pthread_mutex_unlock(&lock);
        return kAudioHardwareBadObjectError;
    }
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        *(UInt32 *)out = kAudioTapClassID;
        break;
    case kAudioObjectPropertyBaseClass:
        *(UInt32 *)out = kAudioObjectClassID;
        break;
    case kAudioObjectPropertyName:
        *(CFStringRef *)out = (CFStringRef)[t->description.name copy];
        break;
    case kAudioTapPropertyUID:
        *(CFStringRef *)out = CFRetain(t->uid);
        break;
    case kAudioTapPropertyDescription:
        *(CATapDescription **)out = [t->description retain];
        break;
    case kAudioTapPropertyFormat: {
        AudioStreamBasicDescription *f = out;
        memset(f, 0, sizeof(*f));
        f->mSampleRate = t->area->sampleRate;
        f->mFormatID = kAudioFormatLinearPCM;
        f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
        f->mChannelsPerFrame = t->area->channels;
        f->mBitsPerChannel = 32;
        f->mBytesPerFrame = f->mBytesPerPacket = t->area->channels * sizeof(float);
        f->mFramesPerPacket = 1;
        break;
    }
    }
    pthread_mutex_unlock(&lock);
    *ioSize = size;
    return noErr;
}

static void putNumber(CFMutableDictionaryRef d, const char *key, int value) {
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &value);
    CFDictionarySetValue(d, k, n);
    CFRelease(n);
    CFRelease(k);
}

OSStatus AudioHardwareCreateProcessTap(CATapDescription *description, AudioObjectID *outTapID) {
    Tap *t = NULL;
    if (description == nil || outTapID == NULL)
        return kAudioHardwareIllegalOperationError;
    *outTapID = kAudioObjectUnknown;
    if (service() == MACH_PORT_NULL)
        return kAudioHardwareNotRunningError;

    /* The description, for the service: the processes as pids. */
    CFMutableDictionaryRef plist = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFMutableArrayRef pids = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (NSNumber *process in description.processes) {
        AudioObjectID object = (AudioObjectID)[process unsignedIntValue];
        int pid = _AudioProcessIs(object) ? (int)(object - kFirstProcessID) : 0;
        if (pid > 0) {
            CFNumberRef n = CFNumberCreate(NULL, kCFNumberIntType, &pid);
            CFArrayAppendValue(pids, n);
            CFRelease(n);
        }
    }
    CFDictionarySetValue(plist, CFSTR(kAudioServerTapPIDs), pids);
    CFRelease(pids);
    putNumber(plist, kAudioServerTapExclusive, description.isExclusive);
    putNumber(plist, kAudioServerTapMono, description.isMono);
    putNumber(plist, kAudioServerTapMixdown, description.isMixdown);
    putNumber(plist, kAudioServerTapMute, (int)description.muteBehavior);
    if (description.deviceUID != nil)
        CFDictionarySetValue(plist, CFSTR(kAudioServerTapDevice), (CFStringRef)description.deviceUID);
    CFDataRef bytes = CFPropertyListCreateData(NULL, plist, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    CFRelease(plist);
    if (bytes == NULL)
        return kAudioHardwareUnspecifiedError;

    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxTaps && t == NULL; i++)
        if (taps[i].id == 0)
            t = &taps[i];
    if (t == NULL) {
        pthread_mutex_unlock(&lock);
        CFRelease(bytes);
        return kAudioHardwareUnspecifiedError;
    }
    memset(t, 0, sizeof(*t));
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &t->client);
    int32_t serviceID = 0, result = 0;
    mach_port_t area = MACH_PORT_NULL;
    uint32_t size = 0;
    kern_return_t kr = as_create_tap(service(), (vm_offset_t)CFDataGetBytePtr(bytes),
                                     (mach_msg_type_number_t)CFDataGetLength(bytes), t->client, &serviceID, &area,
                                     &size, &result);
    CFRelease(bytes);
    vm_address_t address = 0;
    if (kr != KERN_SUCCESS || result != 0 ||
        vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_ANYWHERE, area, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE,
               VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) != KERN_SUCCESS) {
        if (area != MACH_PORT_NULL)
            mach_port_deallocate(mach_task_self(), area);
        mach_port_mod_refs(mach_task_self(), t->client, MACH_PORT_RIGHT_RECEIVE, -1);
        memset(t, 0, sizeof(*t));
        pthread_mutex_unlock(&lock);
        return result != 0 ? result : kAudioHardwareUnspecifiedError;
    }
    mach_port_deallocate(mach_task_self(), area);
    t->serviceID = serviceID;
    t->area = (AudioServerTapArea *)address;
    t->areaSize = size;
    t->description = [description copy];
    t->uid = (CFStringRef)[[description.UUID UUIDString] copy];
    t->id = kFirstTapID + (AudioObjectID)(t - taps);
    *outTapID = t->id;
    pthread_mutex_unlock(&lock);
    return noErr;
}

OSStatus AudioHardwareDestroyProcessTap(AudioObjectID id) {
    pthread_mutex_lock(&lock);
    Tap *t = find(id);
    if (t == NULL) {
        pthread_mutex_unlock(&lock);
        return kAudioHardwareBadObjectError;
    }
    int32_t result = 0;
    as_destroy_tap(service(), t->serviceID, &result);
    vm_deallocate(mach_task_self(), (vm_address_t)t->area, t->areaSize);
    mach_port_mod_refs(mach_task_self(), t->client, MACH_PORT_RIGHT_RECEIVE, -1);
    [t->description release];
    CFRelease(t->uid);
    memset(t, 0, sizeof(*t));
    pthread_mutex_unlock(&lock);
    return noErr;
}
