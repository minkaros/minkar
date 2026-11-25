/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * audiod, the audio service (AU2): it owns the audio devices (the
 * kernel's IOAudioFamily engines), mixes its clients into their outputs and
 * gives them what their inputs record.
 *
 * A device's IO runs on a real-time thread while it has clients. Each cycle
 * (its frames set by the device's first client, 512 unless it asks for
 * fewer) it chooses the frames of the device's timeline the cycle's samples
 * will be played at, ahead of the hardware by two cycles and the engine's
 * sample offset; it copies the frames the input recorded last, behind the
 * hardware, into each client's shared area with those times, and wakes
 * the clients; it waits for each one, at most until
 * the cycle's deadline, adds their samples, and writes the sum, converted
 * to the device's format, into the device's own buffer. Silence is written
 * too, so the hardware never plays a buffer's old samples again.
 *
 * Where the hardware is, it is asked each cycle: the engine's loop count
 * and current frame (its DMA's position, ahead of what is heard). The
 * cycle is placed and the next wake-up timed from that measure: before
 * the buffer first wraps, the start's time stamp says nothing of how far
 * the DMA has already read.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <bsm/libbsm.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/thread_policy.h>
#include <math.h>
#include <notify.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "AudioServerTypes.h"
#include "AudioServerServer.h"
#include "plugins.h"

#define kMaxDevices     8
#define kMaxSessions    32
#define kMaxFrames      kAudioServerMaxFramesPerCycle
#define kChannels       kAudioServerChannels
#define kSamplesOffset  128     /* the samples, after the area's header */

typedef struct Device {
    char            uid[512];
    io_service_t    engine;
    io_service_t    volume, mute;       /* its output level and toggle, or 0 */
    int64_t         savedVolume, savedMute;
    io_connect_t    connection;
    const volatile IOAudioEngineStatus *status;
    void           *buffer;             /* the hardware's samples */
    uint32_t        bufferFrames;
    uint32_t        bytesPerFrame;
    uint32_t        channels;
    uint32_t        bitDepth;
    const void     *inputBuffer;        /* what the hardware records, or NULL */
    uint32_t        inputChannels, inputBitDepth;
    uint32_t        framesPerCycle;     /* set by the device's first client */
    double          sampleRate;
    uint32_t        sampleOffset;
    pthread_t       thread;
    volatile int    running;
    volatile uint64_t completedCycles;  /* a session ends after the cycle it was in */
    uint64_t        resyncs, lateCycles;
    uint64_t        inputResyncs;       /* the input's, apart: it may come in bursts */
    double          lastMeasured;
    double          inputMeasured;      /* the input's frame, on its own timeline */
    double          inputSlack;         /* frames the input came late by, kept as margin */
    int             clients;
    PlugInDevice   *plugin;             /* an audio server plug-in's device, or NULL */
    int             hasInput;
    float          *tapMix;             /* the taps' samples of a cycle (kMaxTaps of them) */
} Device;

typedef struct Session {
    int32_t         id;                 /* 0: free */
    Device         *device;
    mach_port_t     client;             /* watched for its death */
    AudioServerArea *area;
    vm_size_t       areaSize;
    semaphore_t     wake, done;
    volatile int    active;
    pid_t           pid;
} Session;

/* A process tap (AU10): what some processes play on a device, mixed and
   written to a ring the client reads (AudioServerTapArea). */
#define kMaxTaps        8
#define kMaxTapPIDs     64

typedef struct Tap {
    int32_t         id;                 /* 0: free */
    pid_t           owner;
    mach_port_t     client;             /* watched for its death */
    pid_t           pids[kMaxTapPIDs];
    int             pidCount;
    int             exclusive, mono, mute;
    struct Device  *device;
    AudioServerTapArea *area;
    vm_size_t       areaSize;
    volatile int    active;
} Tap;

static Tap taps[kMaxTaps];
static int32_t nextTap = 1;

static Device devices[kMaxDevices];
static int deviceCount, engineCount;
static Session sessions[kMaxSessions];
static int32_t nextSession = 1;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static mach_port_t notifyPort;
static mach_timebase_info_data_t timebase;

static uint64_t nanosToHost(double nanos) {
    return (uint64_t)(nanos * timebase.denom / timebase.numer);
}

static double hostToNanos(uint64_t host) {
    return (double)host * timebase.numer / timebase.denom;
}

/* ---- Devices ----------------------------------------------------------- */

static int64_t numberProperty(io_registry_entry_t entry, CFStringRef key, int64_t fallback) {
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, key, NULL, 0);
    int64_t n = fallback;
    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberSInt64Type, &n);
    if (value != NULL)
        CFRelease(value);
    return n;
}

static int hasOutput(io_service_t engine) {
    io_iterator_t children;
    io_registry_entry_t child;
    int output = 0;

    if (IORegistryEntryGetChildIterator(engine, kIOServicePlane, &children) != KERN_SUCCESS)
        return 0;
    while (!output && (child = IOIteratorNext(children)) != 0) {
        if (IOObjectConformsTo(child, "IOAudioStream") &&
            numberProperty(child, CFSTR("IOAudioStreamDirection"), -1) == 0)
            output = 1;
        IOObjectRelease(child);
    }
    IOObjectRelease(children);
    return output;
}

/* The engine's output control of a type and subtype, or 0. */
static io_service_t findControl(io_service_t engine, uint32_t type, uint32_t subType) {
    io_iterator_t children;
    io_registry_entry_t child;
    io_service_t found = 0;

    if (IORegistryEntryGetChildIterator(engine, kIOServicePlane, &children) != KERN_SUCCESS)
        return 0;
    while (found == 0 && (child = IOIteratorNext(children)) != 0) {
        if ((uint32_t)numberProperty(child, CFSTR("IOAudioControlType"), 0) == type &&
            (uint32_t)numberProperty(child, CFSTR("IOAudioControlSubType"), 0) == subType &&
            (uint32_t)numberProperty(child, CFSTR("IOAudioControlUsage"), 'outp') == 'outp')
            found = child;
        else
            IOObjectRelease(child);
    }
    IOObjectRelease(children);
    return found;
}

/* ---- Settings: each device's volume and mute, kept across starts ------- */

#define kSettingsDomain CFSTR("org.minkar.audio")
#define kVolumeChangedNotification "org.minkar.audio.volume"

static void setControl(io_service_t control, int64_t value) {
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt64Type, &value);
    CFStringRef key = CFSTR("IOAudioControlValue");
    CFDictionaryRef properties = CFDictionaryCreate(NULL, (const void **)&key, (const void **)&number, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IORegistryEntrySetCFProperties(control, properties);
    CFRelease(properties);
    CFRelease(number);
}

/* What was saved for the device, put back into its controls. */
static void restoreSettings(Device *d) {
    CFStringRef uid = CFStringCreateWithCString(NULL, d->uid, kCFStringEncodingUTF8);
    CFPropertyListRef saved = CFPreferencesCopyValue(uid, kSettingsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
    int64_t n;

    if (saved != NULL && CFGetTypeID(saved) == CFDictionaryGetTypeID()) {
        CFNumberRef volume = CFDictionaryGetValue(saved, CFSTR("volume"));
        CFNumberRef mute = CFDictionaryGetValue(saved, CFSTR("mute"));
        if (d->volume != 0 && volume != NULL && CFGetTypeID(volume) == CFNumberGetTypeID() &&
            CFNumberGetValue(volume, kCFNumberSInt64Type, &n))
            setControl(d->volume, n);
        if (d->mute != 0 && mute != NULL && CFGetTypeID(mute) == CFNumberGetTypeID() &&
            CFNumberGetValue(mute, kCFNumberSInt64Type, &n))
            setControl(d->mute, n);
        syslog(LOG_INFO, "%s: volume and mute restored", d->uid);
    }
    if (saved != NULL)
        CFRelease(saved);
    CFRelease(uid);
    d->savedVolume = d->volume != 0 ? numberProperty(d->volume, CFSTR("IOAudioControlValue"), -1) : -1;
    d->savedMute = d->mute != 0 ? numberProperty(d->mute, CFSTR("IOAudioControlValue"), -1) : -1;
}

/* A device's volume or mute changed (by any client): saved, and told to
   CoreAudio's listeners. */
static void checkSettings(void) {
    int changed = 0;

    for (int i = 0; i < deviceCount; i++) {
        Device *d = &devices[i];
        int64_t volume = d->volume != 0 ? numberProperty(d->volume, CFSTR("IOAudioControlValue"), -1) : -1;
        int64_t mute = d->mute != 0 ? numberProperty(d->mute, CFSTR("IOAudioControlValue"), -1) : -1;

        if (volume == d->savedVolume && mute == d->savedMute)
            continue;
        d->savedVolume = volume;
        d->savedMute = mute;
        CFMutableDictionaryRef values = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                                                  &kCFTypeDictionaryValueCallBacks);
        CFNumberRef v = CFNumberCreate(NULL, kCFNumberSInt64Type, &volume);
        CFNumberRef m = CFNumberCreate(NULL, kCFNumberSInt64Type, &mute);
        if (volume >= 0)
            CFDictionarySetValue(values, CFSTR("volume"), v);
        if (mute >= 0)
            CFDictionarySetValue(values, CFSTR("mute"), m);
        CFStringRef uid = CFStringCreateWithCString(NULL, d->uid, kCFStringEncodingUTF8);
        CFPreferencesSetValue(uid, values, kSettingsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        CFRelease(uid);
        CFRelease(values);
        CFRelease(v);
        CFRelease(m);
        changed = 1;
    }
    if (changed) {
        CFPreferencesSynchronize(kSettingsDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        notify_post(kVolumeChangedNotification);
    }
}

/* The output engines, by their path in the registry (CoreAudio's UIDs). */
static void findDevices(void) {
    io_iterator_t engines;
    io_service_t engine;

    if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOAudioEngine"),
                                     &engines) != KERN_SUCCESS)
        return;
    while ((engine = IOIteratorNext(engines)) != 0) {
        io_string_t path;

        if (deviceCount == kMaxDevices || !hasOutput(engine) ||
            IORegistryEntryGetPath(engine, kIOServicePlane, path) != KERN_SUCCESS) {
            IOObjectRelease(engine);
            continue;
        }
        Device *d = &devices[deviceCount++];
        engineCount++;
        strlcpy(d->uid, path, sizeof(d->uid));
        d->engine = engine;
        d->volume = findControl(engine, 'levl', 'vlme');
        d->mute = findControl(engine, 'togl', 'mute');
        restoreSettings(d);
        syslog(LOG_INFO, "device %s", d->uid);
    }
    IOObjectRelease(engines);
}

/* The plug-ins' devices, after the engines (AU10). */
static void findPlugInDevices(void) {
    PlugInsLoad();
    for (int i = 0; i < PlugInDeviceCount() && deviceCount < kMaxDevices; i++) {
        PlugInDevice *p = PlugInDeviceAt(i);
        Device *d = &devices[deviceCount++];
        strlcpy(d->uid, PlugInDeviceUID(p), sizeof(d->uid));
        d->plugin = p;
        d->sampleRate = PlugInDeviceSampleRate(p);
        d->sampleOffset = PlugInDeviceSafetyOffset(p, 0);
        d->hasInput = PlugInDeviceHasInput(p);
        d->channels = kChannels;
    }
}

static Device *deviceWithUID(const char *uid) {
    for (int i = 0; i < deviceCount; i++)
        if (strcmp(devices[i].uid, uid) == 0)
            return &devices[i];
    return NULL;
}

/* The engine opened: its status and first output stream mapped, its
   format read (16- or 32-bit integers, interleaved). */
static int openDevice(Device *d) {
    IOAudioEngineInfo info;
    IOAudioEngineStreamInfo stream;
    size_t size = sizeof(info);
    uint64_t index = 0;
    mach_vm_address_t address = 0;
    mach_vm_size_t length = 0;

    if (d->connection != 0)
        return 1;
    if (IOServiceOpen(d->engine, mach_task_self(), 0, &d->connection) != KERN_SUCCESS)
        return 0;
    if (IOConnectCallStructMethod(d->connection, kIOAudioEngineMethodGetInfo, NULL, 0, &info, &size) != KERN_SUCCESS)
        goto fail;
    /* The first output stream, and the first input one if there is. */
    int64_t outIndex = -1, inIndex = -1;
    IOAudioEngineStreamInfo inStream;
    for (index = 0; index < info.fNumStreams; index++) {
        IOAudioEngineStreamInfo si;
        size = sizeof(si);
        if (IOConnectCallMethod(d->connection, kIOAudioEngineMethodGetStream, &index, 1, NULL, 0,
                                NULL, NULL, &si, &size) != KERN_SUCCESS)
            continue;
        if (si.fDirection == kIOAudioStreamDirectionOutput && outIndex < 0) {
            outIndex = (int64_t)index;
            stream = si;
        } else if (si.fDirection == kIOAudioStreamDirectionInput && inIndex < 0) {
            inIndex = (int64_t)index;
            inStream = si;
        }
    }
    if (outIndex < 0)
        goto fail;
    index = (uint64_t)outIndex;
    if (stream.fFormat.fSampleFormat != kIOAudioStreamSampleFormatLinearPCM ||
        stream.fFormat.fNumericRepresentation != kIOAudioStreamNumericRepresentationSignedInt ||
        (stream.fFormat.fBitWidth != 16 && stream.fFormat.fBitWidth != 32)) {
        syslog(LOG_ERR, "%s: a format this service does not write", d->uid);
        goto fail;
    }
    if (IOConnectMapMemory64(d->connection, kIOAudioEngineMemoryStatus, mach_task_self(),
                             &address, &length, kIOMapAnywhere) != KERN_SUCCESS)
        goto fail;
    d->status = (const volatile IOAudioEngineStatus *)(uintptr_t)address;
    address = 0;
    if (IOConnectMapMemory64(d->connection, kIOAudioEngineMemoryStream + (uint32_t)outIndex, mach_task_self(),
                             &address, &length, kIOMapAnywhere) != KERN_SUCCESS)
        goto fail;
    d->buffer = (void *)(uintptr_t)address;
    d->inputBuffer = NULL;
    d->inputChannels = 0;
    if (inIndex >= 0 && inStream.fFormat.fNumericRepresentation == kIOAudioStreamNumericRepresentationSignedInt &&
        (inStream.fFormat.fBitWidth == 16 || inStream.fFormat.fBitWidth == 32)) {
        address = 0;
        if (IOConnectMapMemory64(d->connection, kIOAudioEngineMemoryStream + (uint32_t)inIndex, mach_task_self(),
                                 &address, &length, kIOMapAnywhere | kIOMapReadOnly) == KERN_SUCCESS) {
            d->inputBuffer = (const void *)(uintptr_t)address;
            d->inputChannels = inStream.fFormat.fNumChannels;
            d->inputBitDepth = inStream.fFormat.fBitWidth;
        }
    }
    d->hasInput = d->inputBuffer != NULL;
    d->channels = stream.fFormat.fNumChannels;
    d->bitDepth = stream.fFormat.fBitWidth;
    d->bytesPerFrame = d->channels * d->bitDepth / 8;
    d->bufferFrames = info.fNumSampleFramesPerBuffer;
    d->sampleRate = info.fSampleRateWhole;
    d->sampleOffset = info.fSampleOffset;
    if (d->bufferFrames < 4 * kAudioServerMinFramesPerCycle || d->sampleRate <= 0 || d->channels == 0)
        goto fail;
    return 1;
fail:
    IOServiceClose(d->connection);
    d->connection = 0;
    return 0;
}

/* The hardware's frame on the device's timeline (loops of the buffer and
   the frame in it). A wrap the driver has not counted yet shows as a frame
   lower than the last: the buffer is added. */
static double measureFrame(Device *d) {
    uint64_t out[3] = { 0, 0, 0 };
    uint32_t count = 3;
    double frame, moved;

    if (IOConnectCallScalarMethod(d->connection, kIOAudioEngineMethodGetPosition, NULL, 0, out, &count) != KERN_SUCCESS)
        return d->lastMeasured;
    frame = (double)out[0] * d->bufferFrames + (double)out[1];
    while (frame < d->lastMeasured - d->bufferFrames / 2.0)
        frame += d->bufferFrames;
    d->lastMeasured = frame;
    /* The input's DMA, on its own timeline from the same start: on
       hardware it runs with the output's, an emulated controller's starts
       late and moves as the host gives it frames. It is measured every
       cycle, so it moves less than half the buffer between two. */
    if (count >= 3 && d->inputMeasured < 0)
        d->inputMeasured = (double)out[2];
    else if (count >= 3) {
        moved = fmod((double)out[2] - fmod(d->inputMeasured, d->bufferFrames) + 2.0 * d->bufferFrames, d->bufferFrames);
        if (moved < d->bufferFrames / 2.0)
            d->inputMeasured += moved;
    } else
        d->inputMeasured = frame;
    return frame;
}

/* The frames of the device's buffer from frame on, the mix converted. */
static void writeFrames(Device *d, uint64_t frame, const float *mix, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t at = (uint32_t)((frame + i) % d->bufferFrames);
        for (uint32_t c = 0; c < d->channels; c++) {
            float v = c < kChannels ? mix[i * kChannels + c] : 0;
            if (v > 1)
                v = 1;
            if (v < -1)
                v = -1;
            if (d->bitDepth == 16)
                ((int16_t *)d->buffer)[at * d->channels + c] = (int16_t)(v * 32767.0f);
            else
                ((int32_t *)d->buffer)[at * d->channels + c] = (int32_t)(v * 2147483647.0);
        }
    }
}

/* The recorded frames from frame on, as float stereo (mono to both);
   frames before the device started are silence. */
static void readFrames(Device *d, int64_t frame, float *out, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        uint32_t at = (uint32_t)((uint64_t)(frame + i) % d->bufferFrames);
        for (uint32_t c = 0; c < kChannels; c++) {
            uint32_t from = c < d->inputChannels ? c : 0;
            float v;
            if (frame + (int64_t)i < 0)
                v = 0;
            else if (d->inputBitDepth == 16)
                v = ((const int16_t *)d->inputBuffer)[at * d->inputChannels + from] / 32768.0f;
            else
                v = ((const int32_t *)d->inputBuffer)[at * d->inputChannels + from] / 2147483648.0f;
            out[i * kChannels + c] = v;
        }
    }
}

/* A real-time thread: a cycle's computing time out of its period. */
static void makeRealtime(Device *d) {
    struct thread_time_constraint_policy policy;
    double periodNanos = d->framesPerCycle / d->sampleRate * 1e9;

    policy.period = (uint32_t)nanosToHost(periodNanos);
    policy.computation = (uint32_t)nanosToHost(periodNanos / 4);
    policy.constraint = (uint32_t)nanosToHost(periodNanos / 2);
    policy.preemptible = 1;
    if (thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY,
                          (thread_policy_t)&policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT) != KERN_SUCCESS)
        syslog(LOG_WARNING, "%s: no real-time policy", d->uid);
}

/* Whether a tap takes a process's samples. */
static int tapTakes(const Tap *t, pid_t pid) {
    int listed = 0;
    for (int i = 0; i < t->pidCount && !listed; i++)
        listed = t->pids[i] == pid;
    return t->exclusive ? !listed : listed;
}

/* A cycle's tapped samples (stereo) into the tap's ring, in its channels. */
static void tapWrite(Tap *t, const float *in, uint32_t frames) {
    AudioServerTapArea *a = t->area;
    float *ring = (float *)((char *)a + a->ringOffset);
    uint64_t w = a->written;
    for (uint32_t i = 0; i < frames; i++) {
        float *at = ring + ((w + i) % a->ringFrames) * a->channels;
        if (a->channels == 1)
            at[0] = (in[i * kChannels] + in[i * kChannels + 1]) / 2;
        else
            for (uint32_t c = 0; c < a->channels; c++)
                at[c] = c < kChannels ? in[i * kChannels + c] : 0;
    }
    __atomic_store_n(&a->written, w + frames, __ATOMIC_RELEASE);
}

/* One cycle's clients: each given the cycle's times and input and woken,
   then waited for until the deadline; their samples summed into mix. */
static void serveClients(Device *d, uint64_t cycle, uint32_t kFramesPerCycle, uint64_t now, double nowFrame,
                         double writeFrame, uint64_t outputHost, double readFrame, uint64_t inputHost,
                         const float *recorded, uint64_t deadline, float *mix) {
    Session *woken[kMaxSessions];
    int count = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxSessions; i++) {
        Session *s = &sessions[i];
        if (s->id == 0 || s->device != d || !s->active)
            continue;
        s->area->nowSampleTime = nowFrame;
        s->area->nowHostTime = now;
        s->area->outputSampleTime = writeFrame;
        s->area->outputHostTime = outputHost;
        s->area->framesPerCycle = kFramesPerCycle;
        if (s->area->inputChannels > 0) {
            memcpy((char *)s->area + s->area->inputSamplesOffset, recorded,
                   kFramesPerCycle * kChannels * sizeof(float));
            s->area->inputSampleTime = readFrame;
            s->area->inputHostTime = inputHost;
        }
        __sync_synchronize();
        s->area->cycle = cycle;
        /* A done signalled after an earlier cycle's deadline is not
           this cycle's. */
        mach_timespec_t none = { 0, 0 };
        while (semaphore_timedwait(s->done, none) == KERN_SUCCESS)
            ;
        semaphore_signal(s->wake);
        woken[count++] = s;
    }
    pthread_mutex_unlock(&lock);

    /* The device's taps, as this cycle starts. */
    Tap *tapped[kMaxTaps];
    int tapCount = 0;
    pthread_mutex_lock(&lock);
    for (int t = 0; t < kMaxTaps; t++)
        if (taps[t].id != 0 && taps[t].active && taps[t].device == d)
            tapped[tapCount++] = &taps[t];
    pthread_mutex_unlock(&lock);
    if (tapCount > 0 && d->tapMix == NULL)
        d->tapMix = calloc((size_t)kMaxTaps * kMaxFrames * kChannels, sizeof(float));
    if (d->tapMix == NULL)
        tapCount = 0;
    if (tapCount > 0)
        memset(d->tapMix, 0, (size_t)kMaxTaps * kMaxFrames * kChannels * sizeof(float));

    memset(mix, 0, (size_t)kFramesPerCycle * kChannels * sizeof(float));
    for (int i = 0; i < count; i++) {
        Session *s = woken[i];
        uint64_t left = mach_absolute_time();
        mach_timespec_t wait = { 0, 0 };
        if (deadline > left) {
            double nanos = hostToNanos(deadline - left);
            wait.tv_sec = (unsigned)(nanos / 1e9);
            wait.tv_nsec = (clock_res_t)(nanos - wait.tv_sec * 1e9);
        }
        if (semaphore_timedwait(s->done, wait) != KERN_SUCCESS) {
            s->area->late++;
            d->lateCycles++;
            continue;
        }
        const float *samples = (const float *)((const char *)s->area + s->area->samplesOffset);
        /* Into the taps that take this process, and into the mix unless
           one of them mutes it. */
        int muted = 0;
        for (int t = 0; t < tapCount; t++) {
            if (!tapTakes(tapped[t], s->pid))
                continue;
            float *into = d->tapMix + (size_t)t * kMaxFrames * kChannels;
            for (uint32_t k = 0; k < kFramesPerCycle * kChannels; k++)
                into[k] += samples[k];
            muted |= tapped[t]->mute == 1 || (tapped[t]->mute == 2 && tapped[t]->area->readers > 0);
        }
        if (!muted)
            for (uint32_t k = 0; k < kFramesPerCycle * kChannels; k++)
                mix[k] += samples[k];
    }
    for (int t = 0; t < tapCount; t++)
        tapWrite(tapped[t], d->tapMix + (size_t)t * kMaxFrames * kChannels, kFramesPerCycle);
}

static void *deviceThread(void *arg) {
    Device *d = arg;
    static float mix[kMaxFrames * kChannels], recorded[kMaxFrames * kChannels];
    const uint32_t kFramesPerCycle = d->framesPerCycle;
    uint64_t cycle = 0;
    double hostPerFrame = 1e9 / d->sampleRate * timebase.denom / timebase.numer;

    makeRealtime(d);
    /* Two cycles and the engine's offset ahead of the hardware: a cycle
       starts when the hardware is that far before its frames, so clients
       have almost two cycles to render (one was too little for a client's
       first cycles, which take page faults). */
    uint64_t lead = d->sampleOffset + 2 * kFramesPerCycle;
    d->lastMeasured = 0;
    d->inputMeasured = -1;              /* the first measure sets it */
    double nowFrame = measureFrame(d);
    uint64_t writeFrame = (uint64_t)nowFrame + lead;
    uint64_t wake = mach_absolute_time();
    /* The input is read behind its own DMA: frames it has recorded, with
       a margin for the controller's FIFO. One cycle's worth, in step with
       the output's cycles. An input that comes in bursts (an emulated
       controller's, as the host gives it frames) makes the margin as large
       as the most it came late by, up to a quarter of the buffer. */
    d->inputSlack = 0;
    double inputMargin = d->sampleOffset + 32;
    int64_t readFrame = (int64_t)d->inputMeasured - (int64_t)inputMargin - kFramesPerCycle;

    while (d->running) {
        mach_wait_until(wake);
        if (!d->running)
            break;
        uint64_t now = mach_absolute_time();
        nowFrame = measureFrame(d);
        /* Fallen behind (a long stall): start again ahead of the hardware. */
        if ((double)writeFrame < nowFrame + d->sampleOffset) {
            syslog(LOG_WARNING, "%s: behind at cycle %llu (%.0f frames late)", d->uid,
                   (unsigned long long)cycle, nowFrame + d->sampleOffset - (double)writeFrame);
            writeFrame = (uint64_t)nowFrame + lead;
            readFrame = (int64_t)d->inputMeasured - (int64_t)inputMargin - kFramesPerCycle;
            d->resyncs++;
        }
        if (d->inputBuffer != NULL) {
            /* Not past what is recorded: a cycle that came early waits for
               the next (the frames stay in step). */
            double early = (double)(readFrame + kFramesPerCycle) - (d->inputMeasured - inputMargin);
            double late = d->inputMeasured - (double)readFrame - (d->bufferFrames - kFramesPerCycle);
            if (early > 0 && readFrame + (int64_t)kFramesPerCycle > 0) {
                double slack = early + 32 > d->inputSlack ? early + 32 : d->inputSlack;
                if (slack > d->bufferFrames / 4.0)
                    slack = d->bufferFrames / 4.0;
                if (d->inputResyncs < 8)
                    syslog(LOG_WARNING, "%s: input not recorded yet at cycle %llu (%.0f frames early), margin %.0f",
                           d->uid, (unsigned long long)cycle, early, d->sampleOffset + 32 + slack);
                d->inputSlack = slack;
                inputMargin = d->sampleOffset + 32 + slack;
                /* Back by the margin and as much again: an input that
                   drifts from the output (an emulated controller's, whose
                   streams keep their own rates) comes back here seldom. */
                readFrame = (int64_t)(d->inputMeasured - 2 * inputMargin) - kFramesPerCycle;
                d->inputResyncs++;
            } else if (late > 0) {
                /* So far behind that the hardware wrote over the frames. */
                if (d->inputResyncs < 8)
                    syslog(LOG_WARNING, "%s: input overwritten at cycle %llu (%.0f frames late)", d->uid,
                           (unsigned long long)cycle, late);
                readFrame = (int64_t)(d->inputMeasured - inputMargin) - kFramesPerCycle;
                d->inputResyncs++;
            }
            readFrames(d, readFrame, recorded, kFramesPerCycle);
        }
        double inputAge = d->inputMeasured - (double)readFrame;
        uint64_t inputHost = now - (uint64_t)((inputAge > 0 ? inputAge : 0) * hostPerFrame);
        uint64_t outputHost = now + (uint64_t)(((double)writeFrame - nowFrame) * hostPerFrame);
        double deadlineFrame = (double)writeFrame - d->sampleOffset - kFramesPerCycle / 4.0;
        uint64_t deadline = now + (uint64_t)((deadlineFrame > nowFrame ? deadlineFrame - nowFrame : 0) * hostPerFrame);

        cycle++;
        serveClients(d, cycle, kFramesPerCycle, now, nowFrame, (double)writeFrame, outputHost, (double)readFrame,
                     inputHost, recorded, deadline, mix);
        writeFrames(d, writeFrame, mix, kFramesPerCycle);
        writeFrame += kFramesPerCycle;
        readFrame += kFramesPerCycle;
        d->completedCycles++;
        /* Next: when the hardware is lead frames before the next cycle's,
           at the nominal rate from this cycle's measure. */
        double ahead = (double)(writeFrame - lead) - nowFrame;
        wake = now + (uint64_t)((ahead > 0 ? ahead : 0) * hostPerFrame);
    }
    return NULL;
}

/* A plug-in's device: its clock is the driver's zero time stamps, read
   each cycle; the same cycles as an engine's, two ahead of the device and
   its safety offset, the input read behind it by its safety offset. */
static void *plugInThread(void *arg) {
    Device *d = arg;
    float *mix = calloc(kMaxFrames * kChannels, sizeof(float));
    float *recorded = calloc(kMaxFrames * kChannels, sizeof(float));
    const uint32_t kFramesPerCycle = d->framesPerCycle;
    double hostPerFrame = 1e9 / d->sampleRate * timebase.denom / timebase.numer;
    uint64_t cycle = 0;
    uint64_t lead = d->sampleOffset + 2 * kFramesPerCycle;
    uint32_t inputSafety = PlugInDeviceSafetyOffset(d->plugin, 1);
    uint64_t wake = mach_absolute_time();
    double nowFrame = PlugInDeviceFrameAt(d->plugin, wake);
    uint64_t writeFrame = (uint64_t)(nowFrame > 0 ? nowFrame : 0) + lead;

    if (mix == NULL || recorded == NULL) {
        free(mix);
        free(recorded);
        return NULL;
    }
    makeRealtime(d);
    while (d->running) {
        mach_wait_until(wake);
        if (!d->running)
            break;
        uint64_t now = mach_absolute_time();
        nowFrame = PlugInDeviceFrameAt(d->plugin, now);
        if ((double)writeFrame < nowFrame + d->sampleOffset) {
            syslog(LOG_WARNING, "%s: behind at cycle %llu (%.0f frames late)", d->uid,
                   (unsigned long long)cycle, nowFrame + d->sampleOffset - (double)writeFrame);
            writeFrame = (uint64_t)nowFrame + lead;
            d->resyncs++;
        }
        /* The input: the cycle before what the device has recorded. */
        double readFrame = floor(nowFrame) - inputSafety - kFramesPerCycle;
        uint64_t inputHost = now - (uint64_t)((nowFrame - readFrame) * hostPerFrame);
        uint64_t outputHost = now + (uint64_t)(((double)writeFrame - nowFrame) * hostPerFrame);
        double deadlineFrame = (double)writeFrame - d->sampleOffset - kFramesPerCycle / 4.0;
        uint64_t deadline = now + (uint64_t)((deadlineFrame > nowFrame ? deadlineFrame - nowFrame : 0) * hostPerFrame);

        cycle++;
        if (d->hasInput)
            PlugInDeviceCycle(d->plugin, cycle, kFramesPerCycle, now, nowFrame, readFrame, inputHost, recorded,
                              0, 0, NULL);
        serveClients(d, cycle, kFramesPerCycle, now, nowFrame, (double)writeFrame, outputHost, readFrame,
                     inputHost, recorded, deadline, mix);
        PlugInDeviceCycle(d->plugin, cycle, kFramesPerCycle, now, nowFrame, readFrame, inputHost, NULL,
                          (double)writeFrame, outputHost, mix);
        writeFrame += kFramesPerCycle;
        d->completedCycles++;
        double ahead = (double)(writeFrame - lead) - nowFrame;
        wake = now + (uint64_t)((ahead > 0 ? ahead : 0) * hostPerFrame);
    }
    free(mix);
    free(recorded);
    return NULL;
}

static int startPlugInDevice(Device *d, uint32_t frames) {
    d->framesPerCycle = frames;
    if (!PlugInDeviceStartIO(d->plugin)) {
        d->clients--;
        return 0;
    }
    d->running = 1;
    if (pthread_create(&d->thread, NULL, plugInThread, d) != 0) {
        d->running = 0;
        PlugInDeviceStopIO(d->plugin);
        d->clients--;
        return 0;
    }
    syslog(LOG_INFO, "%s: running, %u frames a cycle (a plug-in's device)", d->uid, d->framesPerCycle);
    return 1;
}

/* The device's IO started with its first client, stopped with its last. */
static int startDevice(Device *d, uint32_t frames) {
    if (d->clients++ > 0)
        return 1;
    if (frames < kAudioServerMinFramesPerCycle || frames > kMaxFrames)
        frames = kAudioServerFramesPerCycle;
    if (d->plugin != NULL)
        return startPlugInDevice(d, frames);
    if (!openDevice(d)) {
        d->clients--;
        return 0;
    }
    /* The cycle being written, and the two ahead with the engine's offset,
       fit in the hardware's buffer. */
    while (frames > kAudioServerMinFramesPerCycle && 3 * frames + d->sampleOffset > d->bufferFrames)
        frames /= 2;
    d->framesPerCycle = frames;
    memset(d->buffer, 0, (size_t)d->bufferFrames * d->bytesPerFrame);
    if (IOConnectCallScalarMethod(d->connection, kIOAudioEngineMethodStart, NULL, 0, NULL, NULL) != KERN_SUCCESS) {
        d->clients--;
        return 0;
    }
    d->running = 1;
    if (pthread_create(&d->thread, NULL, deviceThread, d) != 0) {
        d->running = 0;
        IOConnectCallScalarMethod(d->connection, kIOAudioEngineMethodStop, NULL, 0, NULL, NULL);
        d->clients--;
        return 0;
    }
    syslog(LOG_INFO, "%s: running, %u frames a cycle%s", d->uid, d->framesPerCycle,
           d->inputBuffer != NULL ? ", with its input" : "");
    return 1;
}

static void stopDevice(Device *d) {
    if (--d->clients > 0)
        return;
    d->running = 0;
    pthread_mutex_unlock(&lock);
    pthread_join(d->thread, NULL);
    pthread_mutex_lock(&lock);
    if (d->plugin != NULL)
        PlugInDeviceStopIO(d->plugin);
    else
        IOConnectCallScalarMethod(d->connection, kIOAudioEngineMethodStop, NULL, 0, NULL, NULL);
    syslog(LOG_INFO, "%s: stopped after %llu cycles (%llu input resyncs), %llu late client cycles, %llu resyncs",
           d->uid, (unsigned long long)d->completedCycles, (unsigned long long)d->inputResyncs,
           (unsigned long long)d->lateCycles, (unsigned long long)d->resyncs);
    d->completedCycles = d->lateCycles = d->resyncs = d->inputResyncs = 0;
}

/* ---- Sessions ---------------------------------------------------------- */

/* Under the lock. A cycle that took the session in finishes before its
   area and semaphores go: the device's thread uses them outside the lock. */
static void endSession(Session *s) {
    Device *d = s->device;

    s->active = 0;
    /* A client waiting for a cycle is let go. */
    semaphore_signal(s->wake);
    if (d->running && d->clients > 1) {
        uint64_t seen = d->completedCycles;
        pthread_mutex_unlock(&lock);
        while (d->running && d->completedCycles < seen + 1)
            usleep(1000);
        pthread_mutex_lock(&lock);
    }
    stopDevice(d);
    semaphore_destroy(mach_task_self(), s->wake);
    semaphore_destroy(mach_task_self(), s->done);
    vm_deallocate(mach_task_self(), (vm_address_t)s->area, s->areaSize);
    mach_port_deallocate(mach_task_self(), s->client);
    syslog(LOG_INFO, "session %d of pid %d ended", s->id, s->pid);
    memset(s, 0, sizeof(*s));
}

kern_return_t ass_hello(mach_port_t server, int32_t *version) {
    *version = kAudioServerInterfaceVersion;
    return KERN_SUCCESS;
}

kern_return_t ass_start_io(mach_port_t server, audit_token_t token, as_name_t uid,
    mach_port_t client, uint32_t frames, int32_t *session, mach_port_t *area, uint32_t *size,
    mach_port_t *wake, mach_port_t *done, int32_t *result) {
    Session *s = NULL;
    Device *d;
    memory_object_size_t entrySize;
    mach_port_t previous;

    *session = 0;
    *area = *wake = *done = MACH_PORT_NULL;
    *size = 0;
    *result = '!dev';
    pthread_mutex_lock(&lock);
    d = deviceWithUID(uid);
    for (int i = 0; d != NULL && s == NULL && i < kMaxSessions; i++)
        if (sessions[i].id == 0)
            s = &sessions[i];
    if (d == NULL || s == NULL) {
        if (d != NULL)
            *result = 'nope';
        pthread_mutex_unlock(&lock);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    if (!startDevice(d, frames)) {
        pthread_mutex_unlock(&lock);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    s->device = d;
    s->pid = audit_token_to_pid(token);
    s->client = client;
    /* The output's samples, then the input's. */
    s->areaSize = round_page(kSamplesOffset + 2 * kMaxFrames * kChannels * sizeof(float));
    vm_address_t address = 0;
    if (vm_allocate(mach_task_self(), &address, s->areaSize, VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
        stopDevice(d);
        memset(s, 0, sizeof(*s));
        pthread_mutex_unlock(&lock);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    s->area = (AudioServerArea *)address;
    s->area->version = kAudioServerInterfaceVersion;
    s->area->framesPerCycle = d->framesPerCycle;
    s->area->channels = kChannels;
    s->area->samplesOffset = kSamplesOffset;
    s->area->inputChannels = d->hasInput ? kChannels : 0;
    s->area->inputSamplesOffset = kSamplesOffset + kMaxFrames * kChannels * sizeof(float);
    s->area->sampleRate = d->sampleRate;
    entrySize = s->areaSize;
    mach_make_memory_entry_64(mach_task_self(), &entrySize, address,
                              VM_PROT_READ | VM_PROT_WRITE, area, MACH_PORT_NULL);
    semaphore_create(mach_task_self(), &s->wake, SYNC_POLICY_FIFO, 0);
    semaphore_create(mach_task_self(), &s->done, SYNC_POLICY_FIFO, 0);
    /* The client gets send rights to the semaphores (it keeps ours). */
    mach_port_mod_refs(mach_task_self(), s->wake, MACH_PORT_RIGHT_SEND, 1);
    mach_port_mod_refs(mach_task_self(), s->done, MACH_PORT_RIGHT_SEND, 1);
    *wake = s->wake;
    *done = s->done;
    /* Its death ends the session. */
    mach_port_request_notification(mach_task_self(), client, MACH_NOTIFY_DEAD_NAME, 0,
                                   notifyPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    s->id = nextSession++;
    s->active = 1;
    *session = s->id;
    *size = (uint32_t)s->areaSize;
    *result = 0;
    syslog(LOG_INFO, "session %d of pid %d on %s", s->id, s->pid, d->uid);
    pthread_mutex_unlock(&lock);
    return KERN_SUCCESS;
}

kern_return_t ass_stop_io(mach_port_t server, audit_token_t token, int32_t session, int32_t *result) {
    pid_t pid = audit_token_to_pid(token);

    *result = 'nope';
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxSessions; i++)
        if (sessions[i].id == session && session != 0 && sessions[i].pid == pid) {
            endSession(&sessions[i]);
            *result = 0;
            break;
        }
    pthread_mutex_unlock(&lock);
    return KERN_SUCCESS;
}

/* Under the lock. A cycle that took the tap in finishes before its area
   goes. */
static void endTap(Tap *t) {
    Device *d = t->device;
    t->active = 0;
    if (d != NULL && d->running) {
        uint64_t seen = d->completedCycles;
        pthread_mutex_unlock(&lock);
        while (d->running && d->completedCycles < seen + 1)
            usleep(1000);
        pthread_mutex_lock(&lock);
    }
    vm_deallocate(mach_task_self(), (vm_address_t)t->area, t->areaSize);
    mach_port_deallocate(mach_task_self(), t->client);
    syslog(LOG_INFO, "tap %d of pid %d ended", t->id, t->owner);
    memset(t, 0, sizeof(*t));
}

static int dictionaryInt(CFDictionaryRef d, const char *key, int fallback) {
    CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFTypeRef v = CFDictionaryGetValue(d, k);
    int n = fallback;
    CFRelease(k);
    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberIntType, &n);
    else if (v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID())
        n = CFBooleanGetValue(v);
    return n;
}

kern_return_t ass_create_tap(mach_port_t server, audit_token_t token, vm_offset_t description,
    mach_msg_type_number_t descriptionCount, mach_port_t client, int32_t *tap, mach_port_t *area, uint32_t *size,
    int32_t *result) {
    CFDataRef bytes = CFDataCreate(NULL, (const UInt8 *)description, (CFIndex)descriptionCount);
    CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, bytes, 0, NULL, NULL);
    Tap *t = NULL;
    Device *d = NULL;

    CFRelease(bytes);
    vm_deallocate(mach_task_self(), description, descriptionCount);
    *tap = 0;
    *area = MACH_PORT_NULL;
    *size = 0;
    *result = 'nope';
    if (plist == NULL || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        if (plist != NULL)
            CFRelease(plist);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxTaps && t == NULL; i++)
        if (taps[i].id == 0)
            t = &taps[i];
    /* Its device: the one named, else the default output (the first
       engine's, as CoreAudio chooses it). */
    CFStringRef uid = CFDictionaryGetValue(plist, CFSTR(kAudioServerTapDevice));
    char uidBuffer[512];
    if (uid != NULL && CFGetTypeID(uid) == CFStringGetTypeID() &&
        CFStringGetCString(uid, uidBuffer, sizeof(uidBuffer), kCFStringEncodingUTF8))
        d = deviceWithUID(uidBuffer);
    for (int i = 0; d == NULL && i < deviceCount; i++)
        if (devices[i].plugin == NULL)
            d = &devices[i];
    if (d == NULL && deviceCount > 0)
        d = &devices[0];
    if (t == NULL || d == NULL) {
        *result = d == NULL ? '!dev' : 'nope';
        pthread_mutex_unlock(&lock);
        CFRelease(plist);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    memset(t, 0, sizeof(*t));
    CFArrayRef pids = CFDictionaryGetValue(plist, CFSTR(kAudioServerTapPIDs));
    for (CFIndex i = 0; pids != NULL && CFGetTypeID(pids) == CFArrayGetTypeID() && i < CFArrayGetCount(pids) &&
                        t->pidCount < kMaxTapPIDs; i++) {
        CFNumberRef n = CFArrayGetValueAtIndex(pids, i);
        int pid;
        if (CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, kCFNumberIntType, &pid))
            t->pids[t->pidCount++] = pid;
    }
    t->exclusive = dictionaryInt(plist, kAudioServerTapExclusive, 0);
    t->mono = dictionaryInt(plist, kAudioServerTapMono, 0);
    t->mute = dictionaryInt(plist, kAudioServerTapMute, 0);
    CFRelease(plist);
    t->device = d;
    t->owner = audit_token_to_pid(token);
    t->client = client;
    uint32_t channels = t->mono ? 1 : kChannels;
    size_t header = 64 * ((sizeof(AudioServerTapArea) + 63) / 64);
    t->areaSize = round_page(header + (size_t)kAudioServerTapRingFrames * channels * sizeof(float));
    vm_address_t address = 0;
    if (vm_allocate(mach_task_self(), &address, t->areaSize, VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
        memset(t, 0, sizeof(*t));
        pthread_mutex_unlock(&lock);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    t->area = (AudioServerTapArea *)address;
    t->area->version = kAudioServerInterfaceVersion;
    t->area->channels = channels;
    t->area->sampleRate = d->sampleRate > 0 ? d->sampleRate :
        (double)numberProperty(d->engine, CFSTR("IOAudioEngineSampleRate"), 48000);
    t->area->ringFrames = kAudioServerTapRingFrames;
    t->area->ringOffset = (uint32_t)header;
    memory_object_size_t entrySize = t->areaSize;
    mach_make_memory_entry_64(mach_task_self(), &entrySize, address, VM_PROT_READ | VM_PROT_WRITE, area,
                              MACH_PORT_NULL);
    mach_port_t previous;
    mach_port_request_notification(mach_task_self(), client, MACH_NOTIFY_DEAD_NAME, 0, notifyPort,
                                   MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
    t->id = nextTap++;
    t->active = 1;
    *tap = t->id;
    *size = (uint32_t)t->areaSize;
    *result = 0;
    syslog(LOG_INFO, "tap %d of pid %d on %s: %d pids%s, %u channels, mute %d", t->id, t->owner, d->uid,
           t->pidCount, t->exclusive ? " excluded" : "", channels, t->mute);
    pthread_mutex_unlock(&lock);
    return KERN_SUCCESS;
}

kern_return_t ass_destroy_tap(mach_port_t server, audit_token_t token, int32_t tap, int32_t *result) {
    pid_t pid = audit_token_to_pid(token);
    *result = 'nope';
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxTaps; i++)
        if (taps[i].id == tap && tap != 0 && taps[i].owner == pid) {
            endTap(&taps[i]);
            *result = 0;
            break;
        }
    pthread_mutex_unlock(&lock);
    return KERN_SUCCESS;
}

kern_return_t ass_copy_processes(mach_port_t server, vm_offset_t *data, mach_msg_type_number_t *count) {
    int32_t pids[kMaxSessions];
    int n = 0;
    vm_address_t address = 0;

    *data = 0;
    *count = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxSessions; i++) {
        int seen = 0;
        if (sessions[i].id == 0)
            continue;
        for (int k = 0; k < n; k++)
            seen |= pids[k] == sessions[i].pid;
        if (!seen)
            pids[n++] = sessions[i].pid;
    }
    pthread_mutex_unlock(&lock);
    if (n > 0 && vm_allocate(mach_task_self(), &address, n * sizeof(int32_t), VM_FLAGS_ANYWHERE) == KERN_SUCCESS) {
        memcpy((void *)address, pids, n * sizeof(int32_t));
        *data = address;
        *count = (mach_msg_type_number_t)(n * sizeof(int32_t));
    }
    return KERN_SUCCESS;
}

kern_return_t ass_copy_devices(mach_port_t server, vm_offset_t *data, mach_msg_type_number_t *count) {
    CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFDataRef bytes;
    vm_address_t address = 0;

    *data = 0;
    *count = 0;
    pthread_mutex_lock(&lock);
    for (int i = 0; i < deviceCount; i++)
        if (devices[i].plugin != NULL) {
            CFDictionaryRef description = PlugInDeviceCopyDescription(devices[i].plugin);
            CFArrayAppendValue(list, description);
            CFRelease(description);
        }
    pthread_mutex_unlock(&lock);
    bytes = CFPropertyListCreateData(NULL, list, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    CFRelease(list);
    if (bytes == NULL)
        return KERN_SUCCESS;
    if (vm_allocate(mach_task_self(), &address, (vm_size_t)CFDataGetLength(bytes), VM_FLAGS_ANYWHERE) == KERN_SUCCESS) {
        memcpy((void *)address, CFDataGetBytePtr(bytes), (size_t)CFDataGetLength(bytes));
        *data = address;
        *count = (mach_msg_type_number_t)CFDataGetLength(bytes);
    }
    CFRelease(bytes);
    return KERN_SUCCESS;
}

/* A client's port died: its sessions end. */
static void clientDied(mach_port_t name) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxSessions; i++)
        if (sessions[i].id != 0 && sessions[i].client == name)
            endSession(&sessions[i]);
    for (int i = 0; i < kMaxTaps; i++)
        if (taps[i].id != 0 && taps[i].client == name)
            endTap(&taps[i]);
    pthread_mutex_unlock(&lock);
    mach_port_deallocate(mach_task_self(), name);
}

/* ---- Main -------------------------------------------------------------- */

int main(int argc, char **argv) {
    mach_port_t service, portSet;
    union {
        mach_msg_header_t header;
        char bytes[4096];
    } request, reply;

    /* The log goes to syslog and to the service's own file (stderr). */
    openlog("audiod", LOG_PID | LOG_PERROR, LOG_DAEMON);
    mach_timebase_info(&timebase);
    findDevices();
    findPlugInDevices();
    if (bootstrap_check_in(bootstrap_port, kAudioServerName, &service) != KERN_SUCCESS) {
        syslog(LOG_ERR, "cannot check in %s", kAudioServerName);
        return 1;
    }
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort);
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &portSet);
    mach_port_move_member(mach_task_self(), service, portSet);
    mach_port_move_member(mach_task_self(), notifyPort, portSet);
    syslog(LOG_INFO, "ready, %d devices", deviceCount);

    /* Every two seconds: the devices' volume and mute saved when they
       changed, and the engines looked for while there are none (the
       service starts with the system, maybe before the drivers). */
    uint64_t nextCheck = 0;
    for (;;) {
        uint64_t now = mach_absolute_time();
        if (now >= nextCheck) {
            pthread_mutex_lock(&lock);
            if (engineCount == 0 && deviceCount < kMaxDevices) {
                /* Engines go before the plug-ins' devices: found late,
                   they are added after them. */
                findDevices();
            }
            checkSettings();
            pthread_mutex_unlock(&lock);
            nextCheck = now + nanosToHost(2e9);
        }
        mach_msg_return_t mr = mach_msg(&request.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT |
                                        MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
                                        MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0),
                                        0, sizeof(request), portSet, 2000, MACH_PORT_NULL);
        if (mr != MACH_MSG_SUCCESS)
            continue;
        if (request.header.msgh_local_port == notifyPort) {
            if (request.header.msgh_id == MACH_NOTIFY_DEAD_NAME)
                clientDied(((mach_dead_name_notification_t *)&request.header)->not_port);
            continue;
        }
        if (!AudioServer_server(&request.header, &reply.header)) {
            mach_msg_destroy(&request.header);
            continue;
        }
        if (reply.header.msgh_remote_port != MACH_PORT_NULL)
            mach_msg(&reply.header, MACH_SEND_MSG, reply.header.msgh_size, 0, MACH_PORT_NULL,
                     MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    }
}
