/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio server plug-ins (AU10, AudioServerPlugIn.h): the drivers in
 * /Library/Audio/Plug-Ins/HAL and /System/Library/Audio/Plug-Ins/HAL,
 * CFPlugIn bundles (.driver) with a factory for the plug-in type. Each is
 * initialized with the service as its host; its devices, read once, are
 * the service's devices too, with their first output and input stream.
 *
 * A device's IO: the service is its one client. Its clock is the driver's
 * zero time stamps (a sample time and the host time it was at, once a
 * period); each cycle the service reads the cycle's input (ReadInput) and
 * writes its clients' mix (WriteMix) through the driver's IO operations,
 * the samples in the stream's virtual format.
 *
 * What a driver stores (CopyFromStorage, WriteToStorage) is kept in
 * /Library/Preferences/Audio/org.minkar.audiod.plugins.plist, by the
 * driver's bundle identifier.
 */

#include "plugins.h"
#include <CoreAudio/AudioServerPlugIn.h>
#include <dirent.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#define kMaxPlugInDevices 8
#define kMaxDeviceChannels 64
#define kStoragePath "/Library/Preferences/Audio/org.minkar.audiod.plugins.plist"
#define kClientID 1

typedef struct Host {
    AudioServerPlugInHostInterface iface;   /* first: the driver's host ref points here */
    CFStringRef bundleID;
} Host;

typedef struct Stream {
    AudioObjectID id;
    AudioStreamBasicDescription format;
} Stream;

struct PlugInDevice {
    AudioServerPlugInDriverRef driver;
    Host *host;
    AudioObjectID id;
    char uid[512];
    CFStringRef name, manufacturer;
    UInt32 transport;
    Float64 rate;
    Stream output, input;           /* id 0: none */
    UInt32 latency[2], safety[2];   /* output, input */
    UInt32 canBeDefault;
    double hostPerFrame;
    Boolean willRead, willWrite, willCycle;
    void *buffer;                   /* a cycle in the device's format */
};

static PlugInDevice devices[kMaxPlugInDevices];
static int deviceCount;
static CFMutableDictionaryRef storage;

/* ---- The host's side ---- */

static OSStatus hostPropertiesChanged(AudioServerPlugInHostRef host, AudioObjectID object, UInt32 count,
                                      const AudioObjectPropertyAddress *addresses) {
    /* Devices come and go only at the service's start for now. */
    for (UInt32 i = 0; i < count; i++)
        if (addresses[i].mSelector == kAudioPlugInPropertyDeviceList)
            syslog(LOG_INFO, "a plug-in's devices changed; seen at the service's next start");
    return noErr;
}

static void loadStorage(void) {
    if (storage != NULL)
        return;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)kStoragePath,
                                                           (CFIndex)strlen(kStoragePath), false);
    CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, url);
    CFPropertyListRef plist = NULL;
    if (stream != NULL && CFReadStreamOpen(stream)) {
        plist = CFPropertyListCreateWithStream(NULL, stream, 0, kCFPropertyListMutableContainers, NULL, NULL);
        CFReadStreamClose(stream);
    }
    if (stream != NULL)
        CFRelease(stream);
    CFRelease(url);
    if (plist != NULL && CFGetTypeID(plist) == CFDictionaryGetTypeID())
        storage = (CFMutableDictionaryRef)plist;
    else {
        if (plist != NULL)
            CFRelease(plist);
        storage = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
}

static void saveStorage(void) {
    CFDataRef data = CFPropertyListCreateData(NULL, storage, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    if (data == NULL)
        return;
    mkdir("/Library/Preferences/Audio", 0755);
    FILE *f = fopen(kStoragePath ".new", "w");
    if (f != NULL) {
        fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), f);
        if (fclose(f) == 0)
            rename(kStoragePath ".new", kStoragePath);
    }
    CFRelease(data);
}

static CFMutableDictionaryRef driverStorage(AudioServerPlugInHostRef host, Boolean create) {
    const Host *h = (const Host *)host;
    loadStorage();
    CFMutableDictionaryRef d = (CFMutableDictionaryRef)CFDictionaryGetValue(storage, h->bundleID);
    if (d == NULL && create) {
        d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(storage, h->bundleID, d);
        CFRelease(d);
    }
    return d;
}

static OSStatus hostCopyFromStorage(AudioServerPlugInHostRef host, CFStringRef key, CFPropertyListRef *out) {
    CFDictionaryRef d = driverStorage(host, false);
    CFPropertyListRef value = d != NULL && key != NULL ? CFDictionaryGetValue(d, key) : NULL;
    if (out == NULL)
        return kAudioHardwareIllegalOperationError;
    *out = value != NULL ? CFPropertyListCreateDeepCopy(NULL, value, kCFPropertyListImmutable) : NULL;
    return value != NULL ? noErr : kAudioHardwareUnknownPropertyError;
}

static OSStatus hostWriteToStorage(AudioServerPlugInHostRef host, CFStringRef key, CFPropertyListRef data) {
    if (key == NULL || data == NULL)
        return kAudioHardwareIllegalOperationError;
    CFDictionarySetValue(driverStorage(host, true), key, data);
    saveStorage();
    return noErr;
}

static OSStatus hostDeleteFromStorage(AudioServerPlugInHostRef host, CFStringRef key) {
    CFMutableDictionaryRef d = driverStorage(host, false);
    if (d != NULL && key != NULL) {
        CFDictionaryRemoveValue(d, key);
        saveStorage();
    }
    return noErr;
}

/* Done at once: the service stops nothing to do it (its devices are
   read once). */
static OSStatus hostRequestConfigurationChange(AudioServerPlugInHostRef host, AudioObjectID device, UInt64 action,
                                               void *info) {
    for (int i = 0; i < deviceCount; i++)
        if (devices[i].host == (const Host *)host && devices[i].id == device)
            return (*devices[i].driver)->PerformDeviceConfigurationChange(devices[i].driver, device, action, info);
    return kAudioHardwareBadDeviceError;
}

/* ---- Asking a driver ---- */

static OSStatus get(PlugInDevice *d, AudioObjectID object, AudioObjectPropertySelector selector,
                    AudioObjectPropertyScope scope, UInt32 size, void *out) {
    AudioObjectPropertyAddress a = { selector, scope, kAudioObjectPropertyElementMain };
    UInt32 got = 0;
    if (!(*d->driver)->HasProperty(d->driver, object, getpid(), &a))
        return kAudioHardwareUnknownPropertyError;
    return (*d->driver)->GetPropertyData(d->driver, object, getpid(), &a, 0, NULL, size, &got, out);
}

static UInt32 getUInt32(PlugInDevice *d, AudioObjectID object, AudioObjectPropertySelector selector,
                        AudioObjectPropertyScope scope, UInt32 fallback) {
    UInt32 v = fallback;
    if (get(d, object, selector, scope, sizeof(v), &v) != noErr)
        v = fallback;
    return v;
}

static CFStringRef getString(PlugInDevice *d, AudioObjectPropertySelector selector) {
    CFStringRef s = NULL;
    if (get(d, d->id, selector, kAudioObjectPropertyScopeGlobal, sizeof(s), &s) != noErr || s == NULL ||
        CFGetTypeID(s) != CFStringGetTypeID())
        return NULL;
    return s;
}

/* The device's first stream of a scope, and its virtual format. */
static void findStream(PlugInDevice *d, AudioObjectPropertyScope scope, Stream *out) {
    AudioObjectPropertyAddress a = { kAudioDevicePropertyStreams, scope, kAudioObjectPropertyElementMain };
    AudioObjectID ids[16];
    UInt32 size = 0;
    memset(out, 0, sizeof(*out));
    if (!(*d->driver)->HasProperty(d->driver, d->id, getpid(), &a) ||
        (*d->driver)->GetPropertyData(d->driver, d->id, getpid(), &a, 0, NULL, sizeof(ids), &size, ids) != noErr ||
        size < sizeof(AudioObjectID))
        return;
    if (get(d, ids[0], kAudioStreamPropertyVirtualFormat, kAudioObjectPropertyScopeGlobal, sizeof(out->format),
            &out->format) != noErr)
        return;
    /* Linear PCM, interleaved: float or signed integers of 16 or 32 bits. */
    const AudioStreamBasicDescription *f = &out->format;
    Boolean isFloat = (f->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
    if (f->mFormatID != kAudioFormatLinearPCM || (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) ||
        f->mChannelsPerFrame == 0 || f->mChannelsPerFrame > kMaxDeviceChannels ||
        !(isFloat ? f->mBitsPerChannel == 32 : (f->mBitsPerChannel == 16 || f->mBitsPerChannel == 32))) {
        syslog(LOG_ERR, "%s: a stream format the service does not use", d->uid);
        return;
    }
    out->id = ids[0];
}

static void addDevice(AudioServerPlugInDriverRef driver, Host *host, AudioObjectID id) {
    PlugInDevice *d;
    CFStringRef uid;
    if (deviceCount == kMaxPlugInDevices)
        return;
    d = &devices[deviceCount];
    memset(d, 0, sizeof(*d));
    d->driver = driver;
    d->host = host;
    d->id = id;
    uid = getString(d, kAudioDevicePropertyDeviceUID);
    if (uid == NULL || !CFStringGetCString(uid, d->uid, sizeof(d->uid), kCFStringEncodingUTF8)) {
        if (uid != NULL)
            CFRelease(uid);
        return;
    }
    CFRelease(uid);
    d->name = getString(d, kAudioObjectPropertyName);
    d->manufacturer = getString(d, kAudioObjectPropertyManufacturer);
    d->transport = getUInt32(d, id, kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeGlobal,
                             kAudioDeviceTransportTypeVirtual);
    if (get(d, id, kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, sizeof(d->rate),
            &d->rate) != noErr || d->rate <= 0)
        d->rate = 48000;
    d->latency[0] = getUInt32(d, id, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput, 0);
    d->latency[1] = getUInt32(d, id, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeInput, 0);
    d->safety[0] = getUInt32(d, id, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeOutput, 0);
    d->safety[1] = getUInt32(d, id, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeInput, 0);
    d->canBeDefault = getUInt32(d, id, kAudioDevicePropertyDeviceCanBeDefaultDevice,
                                kAudioObjectPropertyScopeOutput, 1);
    findStream(d, kAudioObjectPropertyScopeOutput, &d->output);
    findStream(d, kAudioObjectPropertyScopeInput, &d->input);
    if (d->output.id == 0) {
        /* The service mixes into outputs: a device it cannot play to is
           left out (an input only device comes with the input's own
           clients). */
        syslog(LOG_INFO, "%s: no output stream it can use; left out", d->uid);
        return;
    }
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    d->hostPerFrame = 1e9 / d->rate * tb.denom / tb.numer;
    deviceCount++;
    syslog(LOG_INFO, "plug-in device %s, %.0f Hz, %u out, %u in", d->uid, d->rate,
           (unsigned)d->output.format.mChannelsPerFrame, (unsigned)d->input.format.mChannelsPerFrame);
}

static void loadDriver(const char *path) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), true);
    CFPlugInRef plugIn = CFPlugInCreate(NULL, url);
    CFArrayRef factories;
    CFRelease(url);
    if (plugIn == NULL) {
        syslog(LOG_ERR, "%s: not a plug-in", path);
        return;
    }
    factories = CFPlugInFindFactoriesForPlugInTypeInPlugIn(kAudioServerPlugInTypeUUID, plugIn);
    for (CFIndex i = 0; factories != NULL && i < CFArrayGetCount(factories); i++) {
        CFUUIDRef factory = CFArrayGetValueAtIndex(factories, i);
        IUnknownVTbl **unknown = CFPlugInInstanceCreate(NULL, factory, kAudioServerPlugInTypeUUID);
        AudioServerPlugInDriverRef driver = NULL;
        if (unknown == NULL) {
            syslog(LOG_ERR, "%s: its factory made nothing", path);
            continue;
        }
        (*unknown)->QueryInterface(unknown, CFUUIDGetUUIDBytes(kAudioServerPlugInDriverInterfaceUUID),
                                   (LPVOID *)&driver);
        (*unknown)->Release(unknown);
        if (driver == NULL) {
            syslog(LOG_ERR, "%s: no driver interface", path);
            continue;
        }
        Host *host = calloc(1, sizeof(Host));
        CFBundleRef bundle = CFPlugInGetBundle(plugIn);
        CFStringRef bundleID = bundle != NULL ? CFBundleGetIdentifier(bundle) : NULL;
        host->iface.PropertiesChanged = hostPropertiesChanged;
        host->iface.CopyFromStorage = hostCopyFromStorage;
        host->iface.WriteToStorage = hostWriteToStorage;
        host->iface.DeleteFromStorage = hostDeleteFromStorage;
        host->iface.RequestDeviceConfigurationChange = hostRequestConfigurationChange;
        host->bundleID = bundleID != NULL ? CFRetain(bundleID) : CFSTR("unknown");
        if ((*driver)->Initialize(driver, &host->iface) != noErr) {
            syslog(LOG_ERR, "%s: it did not initialize", path);
            continue;
        }
        /* Its devices, from its plug-in object. */
        AudioObjectPropertyAddress a = { kAudioPlugInPropertyDeviceList, kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMain };
        AudioObjectID ids[kMaxPlugInDevices];
        UInt32 size = 0;
        if ((*driver)->GetPropertyData(driver, kAudioObjectPlugInObject, getpid(), &a, 0, NULL, sizeof(ids), &size,
                                       ids) == noErr)
            for (UInt32 k = 0; k < size / sizeof(AudioObjectID); k++)
                addDevice(driver, host, ids[k]);
        syslog(LOG_INFO, "driver %s loaded", path);
    }
    if (factories != NULL)
        CFRelease(factories);
    /* The plug-in stays: its code runs for as long as the service. */
}

static void loadFolder(const char *folder) {
    DIR *dir = opendir(folder);
    struct dirent *e;
    if (dir == NULL)
        return;
    while ((e = readdir(dir)) != NULL) {
        size_t n = strlen(e->d_name);
        char path[1024];
        if (n > 7 && strcmp(e->d_name + n - 7, ".driver") == 0 &&
            snprintf(path, sizeof(path), "%s/%s", folder, e->d_name) < (int)sizeof(path))
            loadDriver(path);
    }
    closedir(dir);
}

int PlugInsLoad(void) {
    loadFolder("/System/Library/Audio/Plug-Ins/HAL");
    loadFolder("/Library/Audio/Plug-Ins/HAL");
    return deviceCount;
}

int PlugInDeviceCount(void) {
    return deviceCount;
}

PlugInDevice *PlugInDeviceAt(int index) {
    return index >= 0 && index < deviceCount ? &devices[index] : NULL;
}

const char *PlugInDeviceUID(const PlugInDevice *d) {
    return d->uid;
}

double PlugInDeviceSampleRate(const PlugInDevice *d) {
    return d->rate;
}

int PlugInDeviceHasInput(const PlugInDevice *d) {
    return d->input.id != 0;
}

uint32_t PlugInDeviceSafetyOffset(const PlugInDevice *d, int input) {
    return d->safety[input ? 1 : 0];
}

static void putNumber(CFMutableDictionaryRef dict, const char *key, CFNumberType type, const void *value) {
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFNumberRef n = CFNumberCreate(NULL, type, value);
    CFDictionarySetValue(dict, k, n);
    CFRelease(n);
    CFRelease(k);
}

static void putString(CFMutableDictionaryRef dict, const char *key, CFStringRef value) {
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFDictionarySetValue(dict, k, value);
    CFRelease(k);
}

CFDictionaryRef PlugInDeviceCopyDescription(const PlugInDevice *d) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);
    CFStringRef uid = CFStringCreateWithCString(NULL, d->uid, kCFStringEncodingUTF8);
    UInt32 out = d->output.format.mChannelsPerFrame, in = d->input.id != 0 ? d->input.format.mChannelsPerFrame : 0;
    putString(dict, "uid", uid);
    CFRelease(uid);
    putString(dict, "name", d->name != NULL ? d->name : CFSTR("Audio Device"));
    putString(dict, "manufacturer", d->manufacturer != NULL ? d->manufacturer : CFSTR("Unknown"));
    putNumber(dict, "transport", kCFNumberSInt32Type, &d->transport);
    putNumber(dict, "rate", kCFNumberFloat64Type, &d->rate);
    putNumber(dict, "out", kCFNumberSInt32Type, &out);
    putNumber(dict, "in", kCFNumberSInt32Type, &in);
    putNumber(dict, "latency-out", kCFNumberSInt32Type, &d->latency[0]);
    putNumber(dict, "latency-in", kCFNumberSInt32Type, &d->latency[1]);
    putNumber(dict, "safety-out", kCFNumberSInt32Type, &d->safety[0]);
    putNumber(dict, "safety-in", kCFNumberSInt32Type, &d->safety[1]);
    putNumber(dict, "default", kCFNumberSInt32Type, &d->canBeDefault);
    return dict;
}

/* ---- IO ---- */

static Boolean willDo(PlugInDevice *d, UInt32 operation) {
    Boolean will = false, inPlace = true;
    if ((*d->driver)->WillDoIOOperation(d->driver, d->id, kClientID, operation, &will, &inPlace) != noErr)
        return false;
    return will;
}

int PlugInDeviceStartIO(PlugInDevice *d) {
    AudioServerPlugInClientInfo client = { kClientID, getpid(), true, CFSTR("org.minkar.audiod") };
    UInt32 bytes = d->output.format.mBytesPerFrame;
    if (d->input.id != 0 && d->input.format.mBytesPerFrame > bytes)
        bytes = d->input.format.mBytesPerFrame;
    free(d->buffer);
    d->buffer = calloc(4096, bytes);
    if (d->buffer == NULL)
        return 0;
    (*d->driver)->AddDeviceClient(d->driver, d->id, &client);
    if ((*d->driver)->StartIO(d->driver, d->id, kClientID) != noErr) {
        (*d->driver)->RemoveDeviceClient(d->driver, d->id, &client);
        return 0;
    }
    d->willRead = d->input.id != 0 && willDo(d, kAudioServerPlugInIOOperationReadInput);
    d->willWrite = willDo(d, kAudioServerPlugInIOOperationWriteMix);
    d->willCycle = willDo(d, kAudioServerPlugInIOOperationCycle);
    return 1;
}

void PlugInDeviceStopIO(PlugInDevice *d) {
    AudioServerPlugInClientInfo client = { kClientID, getpid(), true, CFSTR("org.minkar.audiod") };
    (*d->driver)->StopIO(d->driver, d->id, kClientID);
    (*d->driver)->RemoveDeviceClient(d->driver, d->id, &client);
}

double PlugInDeviceFrameAt(PlugInDevice *d, uint64_t host) {
    Float64 sample = 0;
    UInt64 zeroHost = 0, seed = 0;
    if ((*d->driver)->GetZeroTimeStamp(d->driver, d->id, kClientID, &sample, &zeroHost, &seed) != noErr)
        return 0;
    return sample + ((double)host - (double)zeroHost) / d->hostPerFrame;
}

static float readSample(const AudioStreamBasicDescription *f, const void *buffer, uint32_t index) {
    if (f->mFormatFlags & kAudioFormatFlagIsFloat)
        return ((const float *)buffer)[index];
    if (f->mBitsPerChannel == 16)
        return ((const int16_t *)buffer)[index] / 32768.0f;
    return ((const int32_t *)buffer)[index] / 2147483648.0f;
}

static void writeSample(const AudioStreamBasicDescription *f, void *buffer, uint32_t index, float v) {
    if (v > 1)
        v = 1;
    if (v < -1)
        v = -1;
    if (f->mFormatFlags & kAudioFormatFlagIsFloat)
        ((float *)buffer)[index] = v;
    else if (f->mBitsPerChannel == 16)
        ((int16_t *)buffer)[index] = (int16_t)(v * 32767.0f);
    else
        ((int32_t *)buffer)[index] = (int32_t)(v * 2147483647.0);
}

static AudioTimeStamp stamp(double sample, uint64_t host) {
    AudioTimeStamp t;
    memset(&t, 0, sizeof(t));
    t.mSampleTime = sample;
    t.mHostTime = host;
    t.mRateScalar = 1;
    t.mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
    return t;
}

void PlugInDeviceCycle(PlugInDevice *d, uint64_t cycle, uint32_t frames, uint64_t now, double nowFrame,
                       double readFrame, uint64_t inputHost, float *recorded,
                       double writeFrame, uint64_t outputHost, const float *mix) {
    AudioServerPlugInIOCycleInfo info;
    memset(&info, 0, sizeof(info));
    info.mIOCycleCounter = cycle;
    info.mNominalIOBufferFrameSize = frames;
    info.mCurrentTime = stamp(nowFrame, now);
    info.mInputTime = stamp(readFrame, inputHost);
    info.mOutputTime = stamp(writeFrame, outputHost);
    info.mMainTime = info.mOutputTime;
    info.mDeviceHostTicksPerFrame = d->hostPerFrame;

    if (d->willCycle) {
        (*d->driver)->BeginIOOperation(d->driver, d->id, kClientID, kAudioServerPlugInIOOperationCycle, frames, &info);
    }
    if (d->willRead && recorded != NULL) {
        const AudioStreamBasicDescription *f = &d->input.format;
        UInt32 ch = f->mChannelsPerFrame;
        memset(d->buffer, 0, (size_t)frames * f->mBytesPerFrame);
        (*d->driver)->BeginIOOperation(d->driver, d->id, kClientID, kAudioServerPlugInIOOperationReadInput, frames,
                                       &info);
        (*d->driver)->DoIOOperation(d->driver, d->id, d->input.id, kClientID, kAudioServerPlugInIOOperationReadInput,
                                    frames, &info, d->buffer, NULL);
        (*d->driver)->EndIOOperation(d->driver, d->id, kClientID, kAudioServerPlugInIOOperationReadInput, frames,
                                     &info);
        /* As stereo: its first two channels, a mono one to both. */
        for (uint32_t i = 0; i < frames; i++) {
            recorded[i * 2] = readSample(f, d->buffer, i * ch);
            recorded[i * 2 + 1] = readSample(f, d->buffer, i * ch + (ch > 1 ? 1 : 0));
        }
    }
    if (d->willWrite && mix != NULL) {
        const AudioStreamBasicDescription *f = &d->output.format;
        UInt32 ch = f->mChannelsPerFrame;
        /* Stereo to its channels: the first two, a mono one the average,
           the others silent. */
        for (uint32_t i = 0; i < frames; i++)
            for (uint32_t c = 0; c < ch; c++) {
                float v = ch == 1 ? (mix[i * 2] + mix[i * 2 + 1]) / 2 : c < 2 ? mix[i * 2 + c] : 0;
                writeSample(f, d->buffer, i * ch + c, v);
            }
        (*d->driver)->BeginIOOperation(d->driver, d->id, kClientID, kAudioServerPlugInIOOperationWriteMix, frames,
                                       &info);
        (*d->driver)->DoIOOperation(d->driver, d->id, d->output.id, kClientID, kAudioServerPlugInIOOperationWriteMix,
                                    frames, &info, d->buffer, NULL);
        (*d->driver)->EndIOOperation(d->driver, d->id, kClientID, kAudioServerPlugInIOOperationWriteMix, frames,
                                     &info);
    }
    if (d->willCycle) {
        (*d->driver)->EndIOOperation(d->driver, d->id, kClientID, kAudioServerPlugInIOOperationCycle, frames, &info);
    }
}
