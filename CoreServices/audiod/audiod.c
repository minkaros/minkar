/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * audiod, the audio service (AU2): it owns the output devices (the
 * kernel's IOAudioFamily engines) and mixes its clients into them.
 *
 * A device's IO runs on a real-time thread while it has clients. Each cycle
 * (kFramesPerCycle frames) it chooses the frames of the device's timeline
 * the cycle's samples will be played at, ahead of the hardware by a cycle
 * and the engine's sample offset; it writes those times into each client's
 * shared area and wakes the clients; it waits for each one, at most until
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
#include <pthread.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "AudioServerTypes.h"
#include "AudioServerServer.h"

#define kMaxDevices     8
#define kMaxSessions    32
#define kFramesPerCycle kAudioServerFramesPerCycle
#define kChannels       kAudioServerChannels
#define kSamplesOffset  128     /* the samples, after the area's header */

typedef struct Device {
    char            uid[512];
    io_service_t    engine;
    io_connect_t    connection;
    const volatile IOAudioEngineStatus *status;
    void           *buffer;             /* the hardware's samples */
    uint32_t        bufferFrames;
    uint32_t        bytesPerFrame;
    uint32_t        channels;
    uint32_t        bitDepth;
    double          sampleRate;
    uint32_t        sampleOffset;
    pthread_t       thread;
    volatile int    running;
    volatile uint64_t completedCycles;  /* a session ends after the cycle it was in */
    uint64_t        resyncs, lateCycles;
    double          lastMeasured;
    int             clients;
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

static Device devices[kMaxDevices];
static int deviceCount;
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
        strlcpy(d->uid, path, sizeof(d->uid));
        d->engine = engine;
        syslog(LOG_INFO, "device %s", d->uid);
    }
    IOObjectRelease(engines);
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
    size = sizeof(stream);
    if (IOConnectCallMethod(d->connection, kIOAudioEngineMethodGetStream, &index, 1, NULL, 0,
                            NULL, NULL, &stream, &size) != KERN_SUCCESS)
        goto fail;
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
    if (IOConnectMapMemory64(d->connection, kIOAudioEngineMemoryStream, mach_task_self(),
                             &address, &length, kIOMapAnywhere) != KERN_SUCCESS)
        goto fail;
    d->buffer = (void *)(uintptr_t)address;
    d->channels = stream.fFormat.fNumChannels;
    d->bitDepth = stream.fFormat.fBitWidth;
    d->bytesPerFrame = d->channels * d->bitDepth / 8;
    d->bufferFrames = info.fNumSampleFramesPerBuffer;
    d->sampleRate = info.fSampleRateWhole;
    d->sampleOffset = info.fSampleOffset;
    if (d->bufferFrames < 2 * kFramesPerCycle || d->sampleRate <= 0 || d->channels == 0)
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
    uint64_t out[2] = { 0, 0 };
    uint32_t count = 2;
    double frame;

    if (IOConnectCallScalarMethod(d->connection, kIOAudioEngineMethodGetPosition, NULL, 0, out, &count) != KERN_SUCCESS)
        return d->lastMeasured;
    frame = (double)out[0] * d->bufferFrames + (double)out[1];
    while (frame < d->lastMeasured - d->bufferFrames / 2.0)
        frame += d->bufferFrames;
    d->lastMeasured = frame;
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

/* A real-time thread: a cycle's computing time out of its period. */
static void makeRealtime(Device *d) {
    struct thread_time_constraint_policy policy;
    double periodNanos = kFramesPerCycle / d->sampleRate * 1e9;

    policy.period = (uint32_t)nanosToHost(periodNanos);
    policy.computation = (uint32_t)nanosToHost(periodNanos / 4);
    policy.constraint = (uint32_t)nanosToHost(periodNanos / 2);
    policy.preemptible = 1;
    if (thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY,
                          (thread_policy_t)&policy, THREAD_TIME_CONSTRAINT_POLICY_COUNT) != KERN_SUCCESS)
        syslog(LOG_WARNING, "%s: no real-time policy", d->uid);
}

static void *deviceThread(void *arg) {
    Device *d = arg;
    static float mix[kFramesPerCycle * kChannels];
    uint64_t cycle = 0;
    double hostPerFrame = 1e9 / d->sampleRate * timebase.denom / timebase.numer;

    makeRealtime(d);
    /* Two cycles and the engine's offset ahead of the hardware: a cycle
       starts when the hardware is that far before its frames, so clients
       have almost two cycles to render (one was too little for a client's
       first cycles, which take page faults). */
    uint64_t lead = d->sampleOffset + 2 * kFramesPerCycle;
    d->lastMeasured = 0;
    double nowFrame = measureFrame(d);
    uint64_t writeFrame = (uint64_t)nowFrame + lead;
    uint64_t wake = mach_absolute_time();

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
            d->resyncs++;
        }
        uint64_t outputHost = now + (uint64_t)(((double)writeFrame - nowFrame) * hostPerFrame);
        double deadlineFrame = (double)writeFrame - d->sampleOffset - kFramesPerCycle / 4.0;
        uint64_t deadline = now + (uint64_t)((deadlineFrame > nowFrame ? deadlineFrame - nowFrame : 0) * hostPerFrame);

        cycle++;
        Session *woken[kMaxSessions];
        int count = 0;
        pthread_mutex_lock(&lock);
        for (int i = 0; i < kMaxSessions; i++) {
            Session *s = &sessions[i];
            if (s->id == 0 || s->device != d || !s->active)
                continue;
            s->area->nowSampleTime = nowFrame;
            s->area->nowHostTime = now;
            s->area->outputSampleTime = (double)writeFrame;
            s->area->outputHostTime = outputHost;
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

        memset(mix, 0, sizeof(mix));
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
            for (uint32_t k = 0; k < kFramesPerCycle * kChannels; k++)
                mix[k] += samples[k];
        }
        writeFrames(d, writeFrame, mix, kFramesPerCycle);
        writeFrame += kFramesPerCycle;
        d->completedCycles++;
        /* Next: when the hardware is lead frames before the next cycle's,
           at the nominal rate from this cycle's measure. */
        double ahead = (double)(writeFrame - lead) - nowFrame;
        wake = now + (uint64_t)((ahead > 0 ? ahead : 0) * hostPerFrame);
    }
    return NULL;
}

/* The device's IO started with its first client, stopped with its last. */
static int startDevice(Device *d) {
    if (d->clients++ > 0)
        return 1;
    if (!openDevice(d)) {
        d->clients--;
        return 0;
    }
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
    syslog(LOG_INFO, "%s: running, %u frames a cycle", d->uid, kFramesPerCycle);
    return 1;
}

static void stopDevice(Device *d) {
    if (--d->clients > 0)
        return;
    d->running = 0;
    pthread_mutex_unlock(&lock);
    pthread_join(d->thread, NULL);
    pthread_mutex_lock(&lock);
    IOConnectCallScalarMethod(d->connection, kIOAudioEngineMethodStop, NULL, 0, NULL, NULL);
    syslog(LOG_INFO, "%s: stopped after %llu cycles, %llu late client cycles, %llu resyncs", d->uid,
           (unsigned long long)d->completedCycles, (unsigned long long)d->lateCycles,
           (unsigned long long)d->resyncs);
    d->completedCycles = d->lateCycles = d->resyncs = 0;
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
    mach_port_t client, int32_t *session, mach_port_t *area, uint32_t *size,
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
    if (!startDevice(d)) {
        pthread_mutex_unlock(&lock);
        mach_port_deallocate(mach_task_self(), client);
        return KERN_SUCCESS;
    }
    s->device = d;
    s->pid = audit_token_to_pid(token);
    s->client = client;
    s->areaSize = round_page(kSamplesOffset + kFramesPerCycle * kChannels * sizeof(float));
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
    s->area->framesPerCycle = kFramesPerCycle;
    s->area->channels = kChannels;
    s->area->samplesOffset = kSamplesOffset;
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

/* A client's port died: its sessions end. */
static void clientDied(mach_port_t name) {
    pthread_mutex_lock(&lock);
    for (int i = 0; i < kMaxSessions; i++)
        if (sessions[i].id != 0 && sessions[i].client == name)
            endSession(&sessions[i]);
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
    if (bootstrap_check_in(bootstrap_port, kAudioServerName, &service) != KERN_SUCCESS) {
        syslog(LOG_ERR, "cannot check in %s", kAudioServerName);
        return 1;
    }
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort);
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &portSet);
    mach_port_move_member(mach_task_self(), service, portSet);
    mach_port_move_member(mach_task_self(), notifyPort, portSet);
    syslog(LOG_INFO, "ready, %d output devices", deviceCount);

    for (;;) {
        mach_msg_return_t mr = mach_msg(&request.header, MACH_RCV_MSG | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
                                        MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0),
                                        0, sizeof(request), portSet, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
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
