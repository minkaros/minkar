/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Aggregate devices (AU10): several devices seen as one. Made with
 * AudioHardwareCreateAggregateDevice from a description (its UID and name,
 * its sub-devices by UID, the main one whose clock it runs on, private or
 * not, stacked or not); known to the process that made it.
 *
 * Its channels are its sub-devices' in order: one buffer of each one's
 * channels for the output, and one for each one with an input. A stacked
 * aggregate (a multi-output device) has the main sub-device's output
 * buffer only, which every sub-device plays.
 *
 * Its IO runs on the main sub-device's cycle: the app's procedure gets the
 * main's input and output buffers as they are and the others' through
 * rings. A sub-device's own clock drifts from the main's: each ring is
 * read at a rate corrected by how full it is (a linear interpolation, at
 * most half a percent off), so it neither empties nor fills.
 */

#include "AudioAggregate.h"
#include "AudioDevices.h"
#include "AudioServerTypes.h"
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define kMaxAggregates   8
#define kMaxSubDevices   8
#define kFirstAggregateID 0x400
#define kChannels        kAudioServerChannels
#define kRingFrames      16384

typedef struct Aggregate {
    AudioObjectID id;           /* 0: free */
    CFStringRef uid, name;
    CFDictionaryRef composition;
    CFStringRef subUIDs[kMaxSubDevices];
    UInt32 subCount;
    UInt32 main;                /* index of the main sub-device */
    Boolean stacked, isPrivate;
} Aggregate;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Aggregate aggregates[kMaxAggregates];

/* ---- Rings with drift correction ---- */

typedef struct Ring {
    float data[kRingFrames * kChannels];
    volatile uint64_t written;  /* frames, by the producer */
    double read;                /* frame position, by the consumer */
    double rate;                /* the consumer's step, about 1 */
    UInt32 target;              /* frames kept between them */
} Ring;

static void ringInit(Ring *r, UInt32 target) {
    memset(r->data, 0, sizeof(r->data));
    r->target = target;
    r->written = target;        /* a cushion of silence */
    r->read = 0;
    r->rate = 1;
}

static void ringWrite(Ring *r, const float *in, UInt32 frames) {
    uint64_t w = r->written;
    for (UInt32 i = 0; i < frames; i++)
        for (UInt32 c = 0; c < kChannels; c++)
            r->data[((w + i) % kRingFrames) * kChannels + c] = in != NULL ? in[i * kChannels + c] : 0;
    __atomic_store_n(&r->written, w + frames, __ATOMIC_RELEASE);
}

/* frames out, at a rate corrected toward the target fill. */
static void ringRead(Ring *r, float *out, UInt32 frames) {
    uint64_t w = __atomic_load_n(&r->written, __ATOMIC_ACQUIRE);
    double fill = (double)w - r->read;
    /* Too far behind or ahead (a stall): back to the target at once. */
    if (fill < frames || fill > kRingFrames - 2.0 * frames) {
        r->read = (double)w - r->target - frames;
        if (r->read < 0)
            r->read = 0;
        fill = (double)w - r->read;
    }
    double error = (fill - frames - r->target) / (double)(r->target > 0 ? r->target : 1);
    double wanted = 1 + 0.002 * error;
    if (wanted > 1.005)
        wanted = 1.005;
    if (wanted < 0.995)
        wanted = 0.995;
    r->rate += (wanted - r->rate) * 0.05;
    for (UInt32 i = 0; i < frames; i++) {
        double at = r->read + i * r->rate;
        uint64_t k = (uint64_t)at;
        float t = (float)(at - (double)k);
        for (UInt32 c = 0; c < kChannels; c++) {
            float a = r->data[(k % kRingFrames) * kChannels + c];
            float b = r->data[((k + 1) % kRingFrames) * kChannels + c];
            out[i * kChannels + c] = a + (b - a) * t;
        }
    }
    r->read += frames * r->rate;
}

/* ---- Lookup ---- */

static Aggregate *find(AudioObjectID id) {
    if (id < kFirstAggregateID || id >= kFirstAggregateID + kMaxAggregates)
        return NULL;
    Aggregate *a = &aggregates[id - kFirstAggregateID];
    return a->id == id ? a : NULL;
}

Boolean _AudioAggregateIs(AudioObjectID id) {
    pthread_mutex_lock(&lock);
    Boolean is = find(id) != NULL;
    pthread_mutex_unlock(&lock);
    return is;
}

UInt32 _AudioAggregateCopyIDs(AudioObjectID *ids, UInt32 capacity) {
    UInt32 n = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxAggregates; i++)
        if (aggregates[i].id != 0) {
            if (n < capacity)
                ids[n] = aggregates[i].id;
            n++;
        }
    pthread_mutex_unlock(&lock);
    return n;
}

AudioObjectID _AudioAggregateWithUID(CFStringRef uid) {
    AudioObjectID id = kAudioObjectUnknown;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxAggregates && uid != NULL; i++)
        if (aggregates[i].id != 0 && CFEqual(aggregates[i].uid, uid))
            id = aggregates[i].id;
    pthread_mutex_unlock(&lock);
    return id;
}

static AudioDeviceID subDevice(const Aggregate *a, UInt32 i) {
    return _AudioDeviceWithUID(a->subUIDs[i]);
}

static Boolean subHasInput(const Aggregate *a, UInt32 i) {
    AudioObjectPropertyAddress p = { kAudioDevicePropertyStreams, kAudioObjectPropertyScopeInput,
                                     kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    AudioDeviceID d = subDevice(a, i);
    return d != kAudioObjectUnknown && _AudioDeviceHasProperty(d, &p) &&
           (size = _AudioDevicePropertySize(d, &p)) > 0;
}

static UInt32 outputBuffers(const Aggregate *a) {
    return a->stacked ? 1 : a->subCount;
}

static UInt32 inputBuffers(const Aggregate *a) {
    UInt32 n = 0;
    for (UInt32 i = 0; i < a->subCount; i++)
        n += subHasInput(a, i);
    return n;
}

/* ---- Making and destroying ---- */

static CFTypeRef value(CFDictionaryRef d, const char *key) {
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFTypeRef v = d != NULL ? CFDictionaryGetValue(d, k) : NULL;
    CFRelease(k);
    return v;
}

static Boolean flag(CFDictionaryRef d, const char *key) {
    CFTypeRef v = value(d, key);
    SInt32 n = 0;
    if (v == NULL)
        return false;
    if (CFGetTypeID(v) == CFBooleanGetTypeID())
        return CFBooleanGetValue(v);
    if (CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberSInt32Type, &n);
    return n != 0;
}

static CFStringRef string(CFTypeRef v) {
    return v != NULL && CFGetTypeID(v) == CFStringGetTypeID() ? v : NULL;
}

OSStatus AudioHardwareCreateAggregateDevice(CFDictionaryRef description, AudioObjectID *outDeviceID) {
    Aggregate *a = NULL;
    CFStringRef uid, name, mainUID;
    CFArrayRef subs;

    if (description == NULL || outDeviceID == NULL || CFGetTypeID(description) != CFDictionaryGetTypeID())
        return kAudioHardwareIllegalOperationError;
    *outDeviceID = kAudioObjectUnknown;
    uid = string(value(description, kAudioAggregateDeviceUIDKey));
    name = string(value(description, kAudioAggregateDeviceNameKey));
    if (uid == NULL)
        return kAudioHardwareIllegalOperationError;
    if (_AudioDeviceWithUID(uid) != kAudioObjectUnknown || _AudioAggregateWithUID(uid) != kAudioObjectUnknown)
        return kAudioHardwareIllegalOperationError;
    /* Its sub-devices: devices that exist, each once (looked up before the
       lock is taken: the lookup takes it too). */
    CFStringRef found[kMaxSubDevices];
    UInt32 foundCount = 0;
    subs = value(description, kAudioAggregateDeviceSubDeviceListKey);
    for (CFIndex i = 0; subs != NULL && CFGetTypeID(subs) == CFArrayGetTypeID() && i < CFArrayGetCount(subs) &&
                        foundCount < kMaxSubDevices; i++) {
        CFTypeRef entry = CFArrayGetValueAtIndex(subs, i);
        CFStringRef subUID = CFGetTypeID(entry) == CFDictionaryGetTypeID() ?
            string(value(entry, kAudioSubDeviceUIDKey)) : string(entry);
        Boolean again = false;
        for (UInt32 k = 0; k < foundCount; k++)
            again |= CFEqual(found[k], subUID);
        if (subUID != NULL && !again && _AudioDeviceWithUID(subUID) != kAudioObjectUnknown &&
            _AudioAggregateWithUID(subUID) == kAudioObjectUnknown)
            found[foundCount++] = subUID;
    }
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxAggregates && a == NULL; i++)
        if (aggregates[i].id == 0)
            a = &aggregates[i];
    if (a == NULL) {
        pthread_mutex_unlock(&lock);
        return kAudioHardwareUnspecifiedError;
    }
    memset(a, 0, sizeof(*a));
    for (UInt32 k = 0; k < foundCount; k++)
        a->subUIDs[a->subCount++] = CFRetain(found[k]);
    mainUID = string(value(description, kAudioAggregateDeviceMainSubDeviceKey));
    if (mainUID == NULL)
        mainUID = string(value(description, kAudioAggregateDeviceClockDeviceKey));
    for (UInt32 k = 0; mainUID != NULL && k < a->subCount; k++)
        if (CFEqual(a->subUIDs[k], mainUID))
            a->main = k;
    a->uid = CFRetain(uid);
    a->name = name != NULL ? CFRetain(name) : CFRetain(uid);
    a->composition = CFDictionaryCreateCopy(NULL, description);
    a->stacked = flag(description, kAudioAggregateDeviceIsStackedKey);
    a->isPrivate = flag(description, kAudioAggregateDeviceIsPrivateKey);
    a->id = kFirstAggregateID + (AudioObjectID)(a - aggregates);
    *outDeviceID = a->id;
    pthread_mutex_unlock(&lock);
    _AudioHardwareDevicesChanged();
    return noErr;
}

OSStatus AudioHardwareDestroyAggregateDevice(AudioObjectID id) {
    pthread_mutex_lock(&lock);
    Aggregate *a = find(id);
    if (a == NULL) {
        pthread_mutex_unlock(&lock);
        return kAudioHardwareBadDeviceError;
    }
    for (UInt32 i = 0; i < a->subCount; i++)
        CFRelease(a->subUIDs[i]);
    CFRelease(a->uid);
    CFRelease(a->name);
    CFRelease(a->composition);
    memset(a, 0, sizeof(*a));
    pthread_mutex_unlock(&lock);
    _AudioHardwareDevicesChanged();
    return noErr;
}

/* ---- Properties ---- */

static Boolean isInputScope(const AudioObjectPropertyAddress *p) {
    return p->mScope == kAudioObjectPropertyScopeInput;
}

Boolean _AudioAggregateHasProperty(AudioObjectID id, const AudioObjectPropertyAddress *p) {
    switch (p->mSelector) {
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
    case kAudioObjectPropertyClass:
    case kAudioObjectPropertyBaseClass:
    case kAudioObjectPropertyOwnedObjects:
    case kAudioDevicePropertyDeviceUID:
    case kAudioDevicePropertyTransportType:
    case kAudioDevicePropertyNominalSampleRate:
    case kAudioDevicePropertyDeviceIsAlive:
    case kAudioDevicePropertyDeviceIsRunning:
    case kAudioDevicePropertyBufferFrameSize:
    case kAudioDevicePropertyBufferFrameSizeRange:
    case kAudioDevicePropertyStreamConfiguration:
    case kAudioDevicePropertyStreams:
    case kAudioDevicePropertyLatency:
    case kAudioDevicePropertySafetyOffset:
    case kAudioAggregateDevicePropertyFullSubDeviceList:
    case kAudioAggregateDevicePropertyActiveSubDeviceList:
    case kAudioAggregateDevicePropertyComposition:
    case kAudioAggregateDevicePropertyMainSubDevice:
    case kAudioAggregateDevicePropertyClockDevice:
        return true;
    default:
        return false;
    }
}

Boolean _AudioAggregateIsPropertySettable(AudioObjectID id, const AudioObjectPropertyAddress *p) {
    return p->mSelector == kAudioDevicePropertyBufferFrameSize;
}

UInt32 _AudioAggregatePropertySize(AudioObjectID id, const AudioObjectPropertyAddress *p) {
    Aggregate copy;
    pthread_mutex_lock(&lock);
    Aggregate *a = find(id);
    if (a != NULL)
        copy = *a;
    pthread_mutex_unlock(&lock);
    if (a == NULL)
        return 0;
    switch (p->mSelector) {
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
    case kAudioDevicePropertyDeviceUID:
    case kAudioAggregateDevicePropertyFullSubDeviceList:
    case kAudioAggregateDevicePropertyComposition:
    case kAudioAggregateDevicePropertyMainSubDevice:
    case kAudioAggregateDevicePropertyClockDevice:
        return sizeof(CFTypeRef);
    case kAudioDevicePropertyNominalSampleRate:
        return sizeof(Float64);
    case kAudioDevicePropertyBufferFrameSizeRange:
        return sizeof(AudioValueRange);
    case kAudioDevicePropertyStreamConfiguration: {
        UInt32 n = isInputScope(p) ? inputBuffers(&copy) : outputBuffers(&copy);
        return (UInt32)(offsetof(AudioBufferList, mBuffers) + (n > 0 ? n : 1) * sizeof(AudioBuffer));
    }
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
        return 0;   /* its streams are its sub-devices' */
    case kAudioAggregateDevicePropertyActiveSubDeviceList:
        return copy.subCount * sizeof(AudioObjectID);
    default:
        return sizeof(UInt32);
    }
}

static UInt32 framesPerCycle[kMaxAggregates];

OSStatus _AudioAggregateGetProperty(AudioObjectID id, const AudioObjectPropertyAddress *p, UInt32 *ioSize,
                                    void *out) {
    UInt32 size = _AudioAggregatePropertySize(id, p);
    Aggregate copy;

    if (!_AudioAggregateHasProperty(id, p))
        return kAudioHardwareUnknownPropertyError;
    if (ioSize == NULL || (*ioSize < size) || (out == NULL && size > 0))
        return kAudioHardwareBadPropertySizeError;
    pthread_mutex_lock(&lock);
    Aggregate *a = find(id);
    if (a != NULL)
        copy = *a;
    pthread_mutex_unlock(&lock);
    if (a == NULL)
        return kAudioHardwareBadObjectError;
    a = &copy;
    AudioDeviceID mainDevice = a->subCount > 0 ? subDevice(a, a->main) : kAudioObjectUnknown;
    UInt32 cycle = framesPerCycle[id - kFirstAggregateID];
    if (cycle == 0)
        cycle = kAudioServerFramesPerCycle;
    switch (p->mSelector) {
    case kAudioObjectPropertyName:
        *(CFStringRef *)out = CFRetain(a->name);
        break;
    case kAudioObjectPropertyManufacturer:
        *(CFStringRef *)out = CFRetain(CFSTR("Minkar"));
        break;
    case kAudioDevicePropertyDeviceUID:
        *(CFStringRef *)out = CFRetain(a->uid);
        break;
    case kAudioObjectPropertyClass:
        *(UInt32 *)out = kAudioAggregateDeviceClassID;
        break;
    case kAudioObjectPropertyBaseClass:
        *(UInt32 *)out = kAudioDeviceClassID;
        break;
    case kAudioDevicePropertyTransportType:
        *(UInt32 *)out = kAudioDeviceTransportTypeAggregate;
        break;
    case kAudioDevicePropertyDeviceIsAlive:
        *(UInt32 *)out = 1;
        break;
    case kAudioDevicePropertyDeviceIsRunning:
        *(UInt32 *)out = 0;
        break;
    case kAudioDevicePropertyBufferFrameSize:
        *(UInt32 *)out = cycle;
        break;
    case kAudioDevicePropertyBufferFrameSizeRange:
        ((AudioValueRange *)out)->mMinimum = kAudioServerMinFramesPerCycle;
        ((AudioValueRange *)out)->mMaximum = 1024;
        break;
    case kAudioDevicePropertyNominalSampleRate:
    case kAudioDevicePropertySafetyOffset: {
        UInt32 s = size;
        if (mainDevice == kAudioObjectUnknown || _AudioDeviceGetProperty(mainDevice, p, &s, out) != noErr)
            memset(out, 0, size);
        break;
    }
    case kAudioDevicePropertyLatency: {
        /* The largest of the sub-devices'. */
        UInt32 most = 0;
        for (UInt32 i = 0; i < a->subCount; i++) {
            UInt32 l = 0, s = sizeof(l);
            AudioDeviceID d = subDevice(a, i);
            if (d != kAudioObjectUnknown && _AudioDeviceGetProperty(d, p, &s, &l) == noErr && l > most)
                most = l;
        }
        *(UInt32 *)out = most;
        break;
    }
    case kAudioDevicePropertyStreamConfiguration: {
        AudioBufferList *list = out;
        UInt32 n = isInputScope(p) ? inputBuffers(a) : outputBuffers(a);
        list->mNumberBuffers = n;
        for (UInt32 i = 0; i < n; i++) {
            list->mBuffers[i].mNumberChannels = kChannels;
            list->mBuffers[i].mDataByteSize = cycle * kChannels * sizeof(Float32);
            list->mBuffers[i].mData = NULL;
        }
        break;
    }
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
        break;
    case kAudioAggregateDevicePropertyFullSubDeviceList:
        *(CFArrayRef *)out = CFArrayCreate(NULL, (const void **)a->subUIDs, a->subCount, &kCFTypeArrayCallBacks);
        break;
    case kAudioAggregateDevicePropertyActiveSubDeviceList:
        for (UInt32 i = 0; i < a->subCount; i++)
            ((AudioObjectID *)out)[i] = subDevice(a, i);
        break;
    case kAudioAggregateDevicePropertyComposition:
        *(CFDictionaryRef *)out = CFRetain(a->composition);
        break;
    case kAudioAggregateDevicePropertyMainSubDevice:
    case kAudioAggregateDevicePropertyClockDevice:
        *(CFStringRef *)out = a->subCount > 0 ? CFRetain(a->subUIDs[a->main]) : NULL;
        break;
    default:
        return kAudioHardwareUnknownPropertyError;
    }
    *ioSize = size;
    return noErr;
}

OSStatus _AudioAggregateSetProperty(AudioObjectID id, const AudioObjectPropertyAddress *p, UInt32 size,
                                    const void *data) {
    if (!_AudioAggregateIs(id))
        return kAudioHardwareBadObjectError;
    if (p->mSelector != kAudioDevicePropertyBufferFrameSize)
        return kAudioHardwareUnknownPropertyError;
    if (data == NULL || size < sizeof(UInt32))
        return kAudioHardwareBadPropertySizeError;
    UInt32 frames = *(const UInt32 *)data;
    if (frames < kAudioServerMinFramesPerCycle || frames > 1024)
        return kAudioHardwareIllegalOperationError;
    framesPerCycle[id - kFirstAggregateID] = frames;
    return noErr;
}

/* ---- IO ---- */

typedef struct Run Run;

typedef struct Sub {
    Run *run;
    UInt32 index;
    AudioDeviceID device;
    AudioDeviceIOProcID proc;
    Boolean hasInput;
    UInt32 inputBuffer;         /* its buffer in the aggregate's input list */
    Ring *out, *in;             /* not the main's */
} Sub;

struct Run {
    AudioObjectID id;
    AudioDeviceIOProc proc;
    void *clientData;
    UInt32 subCount, main, inputs;
    Boolean stacked;
    Sub subs[kMaxSubDevices];
    float *outMemory, *inMemory;
    AudioBufferList *outList, *inList;
};

#define kMaxCycle 4096

static AudioBufferList *newList(UInt32 buffers, float **memory) {
    AudioBufferList *l = calloc(1, offsetof(AudioBufferList, mBuffers) + (buffers > 0 ? buffers : 1) * sizeof(AudioBuffer));
    *memory = calloc((size_t)(buffers > 0 ? buffers : 1) * kMaxCycle * kChannels, sizeof(float));
    if (l != NULL)
        l->mNumberBuffers = buffers;
    return l;
}

static void setFrames(AudioBufferList *l, float *memory, UInt32 frames) {
    for (UInt32 i = 0; i < l->mNumberBuffers; i++) {
        l->mBuffers[i].mNumberChannels = kChannels;
        l->mBuffers[i].mDataByteSize = frames * kChannels * sizeof(float);
        l->mBuffers[i].mData = memory + (size_t)i * kMaxCycle * kChannels;
    }
}

/* A sub-device's cycle. The main's runs the app's procedure; the others
   play what the main left them and record for it. */
static OSStatus subIO(AudioObjectID device, const AudioTimeStamp *now, const AudioBufferList *input,
                      const AudioTimeStamp *inputTime, AudioBufferList *output, const AudioTimeStamp *outputTime,
                      void *data) {
    Sub *s = data;
    Run *r = s->run;
    UInt32 frames = output->mBuffers[0].mDataByteSize / (kChannels * sizeof(float));
    if (frames > kMaxCycle)
        frames = kMaxCycle;
    if (s->index != r->main) {
        ringRead(s->out, output->mBuffers[0].mData, frames);
        if (s->hasInput && input != NULL)
            ringWrite(s->in, input->mBuffers[0].mData, frames);
        return noErr;
    }
    setFrames(r->outList, r->outMemory, frames);
    setFrames(r->inList, r->inMemory, frames);
    for (UInt32 i = 0; i < r->outList->mNumberBuffers; i++)
        memset(r->outList->mBuffers[i].mData, 0, r->outList->mBuffers[i].mDataByteSize);
    /* The input: the main's as it is, the others' from their rings. */
    for (UInt32 i = 0; i < r->subCount; i++) {
        Sub *t = &r->subs[i];
        if (!t->hasInput)
            continue;
        float *to = r->inList->mBuffers[t->inputBuffer].mData;
        if (i == r->main) {
            if (input != NULL)
                memcpy(to, input->mBuffers[0].mData, frames * kChannels * sizeof(float));
            else
                memset(to, 0, frames * kChannels * sizeof(float));
        } else
            ringRead(t->in, to, frames);
    }
    r->proc(r->id, now, r->inputs > 0 ? r->inList : NULL, inputTime, r->outList, outputTime, r->clientData);
    /* The output: the main's buffer to its device, the others' to their
       rings (stacked: the one buffer to all). */
    for (UInt32 i = 0; i < r->subCount; i++) {
        const float *from = r->outList->mBuffers[r->stacked ? 0 : i].mData;
        if (i == r->main)
            memcpy(output->mBuffers[0].mData, from, frames * kChannels * sizeof(float));
        else
            ringWrite(r->subs[i].out, from, frames);
    }
    return noErr;
}

static void freeRun(Run *r) {
    for (UInt32 i = 0; i < r->subCount; i++) {
        free(r->subs[i].out);
        free(r->subs[i].in);
    }
    free(r->outList);
    free(r->inList);
    free(r->outMemory);
    free(r->inMemory);
    free(r);
}

void *_AudioAggregateStart(AudioObjectID id, AudioDeviceIOProc proc, void *clientData, OSStatus *status) {
    Aggregate copy;
    Run *r;
    UInt32 cycle;

    *status = noErr;
    pthread_mutex_lock(&lock);
    Aggregate *a = find(id);
    if (a != NULL)
        copy = *a;
    pthread_mutex_unlock(&lock);
    if (a == NULL || copy.subCount == 0) {
        *status = kAudioHardwareBadDeviceError;
        return NULL;
    }
    cycle = framesPerCycle[id - kFirstAggregateID];
    if (cycle == 0)
        cycle = kAudioServerFramesPerCycle;
    r = calloc(1, sizeof(Run));
    if (r == NULL) {
        *status = kAudioHardwareUnspecifiedError;
        return NULL;
    }
    r->id = id;
    r->proc = proc;
    r->clientData = clientData;
    r->subCount = copy.subCount;
    r->main = copy.main;
    r->stacked = copy.stacked;
    for (UInt32 i = 0; i < copy.subCount; i++) {
        Sub *s = &r->subs[i];
        s->run = r;
        s->index = i;
        s->device = subDevice(&copy, i);
        s->hasInput = subHasInput(&copy, i);
        if (s->hasInput)
            s->inputBuffer = r->inputs++;
        if (i != copy.main) {
            s->out = malloc(sizeof(Ring));
            s->in = s->hasInput ? malloc(sizeof(Ring)) : NULL;
            if (s->out == NULL || (s->hasInput && s->in == NULL)) {
                freeRun(r);
                *status = kAudioHardwareUnspecifiedError;
                return NULL;
            }
            /* Two cycles between the main and each sub-device. */
            ringInit(s->out, 2 * cycle);
            if (s->in != NULL)
                ringInit(s->in, 2 * cycle);
        }
    }
    r->outList = newList(copy.stacked ? 1 : copy.subCount, &r->outMemory);
    r->inList = newList(r->inputs, &r->inMemory);
    if (r->outList == NULL || r->inList == NULL || r->outMemory == NULL || r->inMemory == NULL) {
        freeRun(r);
        *status = kAudioHardwareUnspecifiedError;
        return NULL;
    }
    for (UInt32 i = 0; i < r->subCount && *status == noErr; i++) {
        Sub *s = &r->subs[i];
        AudioObjectPropertyAddress size = { kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal, 0 };
        _AudioDeviceSetProperty(s->device, &size, sizeof(cycle), &cycle);
        *status = AudioDeviceCreateIOProcID(s->device, subIO, s, &s->proc);
    }
    /* The others first, so their rings run when the main's first cycle
       comes. */
    for (UInt32 i = 0; i < r->subCount && *status == noErr; i++)
        if (i != r->main)
            *status = AudioDeviceStart(r->subs[i].device, r->subs[i].proc);
    if (*status == noErr)
        *status = AudioDeviceStart(r->subs[r->main].device, r->subs[r->main].proc);
    if (*status != noErr) {
        _AudioAggregateStop(r);
        return NULL;
    }
    return r;
}

void _AudioAggregateStop(void *run) {
    Run *r = run;
    if (r == NULL)
        return;
    for (UInt32 i = 0; i < r->subCount; i++)
        if (r->subs[i].proc != NULL) {
            AudioDeviceStop(r->subs[i].device, r->subs[i].proc);
            AudioDeviceDestroyIOProcID(r->subs[i].device, r->subs[i].proc);
        }
    freeRun(r);
}
